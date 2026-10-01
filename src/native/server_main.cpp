#include "network_session.hpp"
#include "server_host.hpp"
#include "public_directory.hpp"
#include "assets.hpp"
#include "smoke.hpp"
#include "thread_affinity.hpp"
#include "legacy.hpp"
#include "../Igc/igc.h"
#include <atomic>
#include <csignal>
#include <chrono>
#include <thread>
#include <cstdio>
#include <fstream>
#include <algorithm>
#include <time.h>
#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/thread_info.h>
#endif
#ifndef _WIN32
#include <sys/resource.h>
#endif
namespace {
std::atomic_bool running{true};
void stop(int) { running = false; }
struct ThreadUsage {double user{},system{};long faults{};};
#ifdef _WIN32
ThreadUsage threadUsage() {
    FILETIME created{},ended{},kernel{},user{};
    GetThreadTimes(GetCurrentThread(),&created,&ended,&kernel,&user);
    auto milliseconds=[](FILETIME value) { return double(uint64_t(value.dwLowDateTime)|(uint64_t(value.dwHighDateTime)<<32))*.0001; };
    return {milliseconds(user),milliseconds(kernel),0};
}
double threadMilliseconds() {auto usage=threadUsage();return usage.user+usage.system;}
#elif defined(__APPLE__)
ThreadUsage threadUsage() {
    // Darwin's getrusage lacks RUSAGE_THREAD. Use its maintained per-thread
    // statistics rather than incorrectly attributing worker CPU to simulation.
    thread_basic_info_data_t info{};mach_msg_type_number_t count=THREAD_BASIC_INFO_COUNT;
    auto thread=mach_thread_self();auto result=thread_info(thread,THREAD_BASIC_INFO,reinterpret_cast<thread_info_t>(&info),&count);mach_port_deallocate(mach_task_self(),thread);
    if(result!=KERN_SUCCESS)return {};
    auto milliseconds=[](time_value_t value){return double(value.seconds)*1000+double(value.microseconds)*.001;};
    return {milliseconds(info.user_time),milliseconds(info.system_time),0};
}
double threadMilliseconds(){auto usage=threadUsage();return usage.user+usage.system;}
#else
ThreadUsage threadUsage() {
    rusage usage{};getrusage(RUSAGE_THREAD,&usage);
    auto milliseconds=[](timeval value){return value.tv_sec*1000.+value.tv_usec*.001;};
    return {milliseconds(usage.ru_utime),milliseconds(usage.ru_stime),usage.ru_minflt};
}
double threadMilliseconds() {
    timespec time{};
    if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&time)!=0)return 0;
    return double(time.tv_sec)*1000+double(time.tv_nsec)*.000001;
}
#endif
} // namespace
int main(int argc, char **argv) {
    try {
        Native::Affinity::initialize();
        auto art = Native::Assets::steamArtwork();
        Native::SessionOptions options;
        options.scenario = Native::Scenario::Conquest;
        options.startInHangar = true;
        options.pilot = "Server seat";
        uint16_t port = 27500;
        std::string serverName="Native";
        std::vector<std::string> directoryAddresses;
        int ticks = 0, scaleShips = 0;
        bool autoStart = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            auto value = [&] {
                if (++i >= argc)
                    throw std::runtime_error("Missing " + arg);
                return std::string(argv[i]);
            };
            if (arg == "--artwork")
                art = value();
            else if(arg=="--story"){
                std::ifstream file(value(),std::ios::binary|std::ios::ate);if(!file)throw std::runtime_error("Cannot open mission story");
                auto size=file.tellg();if(size<0 || size>Native::Net::maxBriefingBytes)throw std::runtime_error("Mission story exceeds the reliable message limit");
                options.story.resize(size);file.seekg(0);if(!file.read(options.story.data(),options.story.size()) || options.story.find('\0')!=std::string::npos)throw std::runtime_error("Invalid mission story text");
            }
            else if(arg=="--name")options.name=value();
            else if(arg=="--server-name")serverName=value();
            else if(arg=="--directory"){
                if(directoryAddresses.size()>=8)throw std::runtime_error("Too many public directories");
                directoryAddresses.push_back(value());
            }
            else if (arg == "--port") {
                int p = std::stoi(value());
                if (p < 1 || p > 65535)
                    throw std::runtime_error("Invalid port");
                port = p;
            } else if (arg == "--scenario") {
                int n = std::stoi(value());
                if (n < 0 || n >= int(Native::scenarios.size()))
                    throw std::runtime_error("Invalid scenario");
                options.scenario = Native::Scenario(n);
                auto &s = Native::scenarios[n];
                options.map = s.map;
                options.enemies = s.enemies;
                options.allies = s.allies;
                options.killLimit = s.killLimit;
            } else if (arg == "--auto-start")
                autoStart = true;
            else if (arg == "--countdown")
                options.rules.startCountdown = std::stoi(value());
            else if (arg == "--seed")
                options.seed = std::stoul(value());
            else if (arg == "--teams")
                options.rules.teams = std::stoi(value());
            else if (arg == "--enemies")
                options.enemies = std::stoi(value());
            else if (arg == "--allies")
                options.allies = std::stoi(value());
            else if (arg == "--ticks")
                ticks = std::stoi(value());
            else if (arg == "--scale-test") {
                scaleShips = std::stoi(value());
                if (scaleShips <= 0 || 2ll * scaleShips + 1 > Native::shipIdentityCapacity)
                    throw std::runtime_error("Invalid scale team size");
            }
            else if (arg == "--help") {
                puts("allegiance-server [--port 27500] [--scenario 5] [--teams 2] [--seed 1] [--artwork PATH] [--ticks "
                     "N] [--countdown 15] [--auto-start] [--scale-test AI_PER_TEAM] [--story FILE] [--directory HOST:PORT]");
                return 0;
            } else
                throw std::runtime_error("Unknown server option " + arg);
        }
        if (scaleShips) {
            options.scenario = Native::Scenario::Skirmish;
            options.map = 0;
            options.rules.teams = 2;
            options.enemies = options.allies = scaleShips;
            options.killLimit = 0;
            options.variedWingmen = true;
            options.observerBattleDepth = 1400;
            options.startInHangar = false;
            autoStart = true;
        }
        Native::Assets assets(art);
        std::vector<std::shared_ptr<Native::ModelData>> owners;
        std::unordered_map<std::string, Native::ModelHandle> models;
        auto loader = [&](const std::string &name, const std::string &) {
            if (auto i = models.find(name); i != models.end())
                return i->second;
            auto m = assets.model(name);
            owners.push_back(m);
            return models.emplace(name, Native::ModelHandle{1, m->radius, m->frames, m->lights}).first->second;
        };
        auto content = Native::Net::contentDigest(art);
        Native::Game game(art, loader, 0, 0, options, {}, Native::GameRole::Authority);
        if (scaleShips)
            Native::arrangeScaleBattle(game, false, false, 750, true);
        Native::QuickChatCatalog quickChat(assets);
        game.quickChats(quickChat);
        Native::Net::Server server(game, options, content, port, !autoStart,{},serverName);
        Native::Net::ServerHost host(server,content,port,[&](const Native::SessionOptions& session){
            auto created=std::make_unique<Native::Game>(art,loader,0,0,session,Native::EffectLoader{},Native::GameRole::Authority);
            created->quickChats(quickChat);return created;
        },assets.coreCatalog());
        std::vector<Native::Net::DirectoryRegistration> registrations;
        registrations.reserve(directoryAddresses.size());
        for(auto& address:directoryAddresses)registrations.emplace_back(std::move(address),port,content);
        std::vector<bool> registered(registrations.size());std::vector<std::string> registrationErrors(registrations.size());
        for(auto& registration:registrations)printf("Public directory %s: registration pending\n",registration.address().c_str());
        std::signal(SIGINT, stop);
        std::signal(SIGTERM, stop);
        printf("Native Allegiance authority listening; %u Hz simulation, %u Hz publication\n",
               Native::Net::tickRate, Native::Net::snapshotRate);
        fflush(stdout);
        if (scaleShips) {
            printf("Mixed-hull battle: %d AI per team; waiting for the first client before combat\n", scaleShips);
            fflush(stdout);
        }
        using Clock = std::chrono::steady_clock;
        Native::Affinity::pinCurrentThread(Native::Affinity::Lane::Simulation);
        auto deadline = Clock::now();
        std::vector<double> samples;
        std::vector<double> simulationSamples,publicationSamples;
        samples.reserve(600);simulationSamples.reserve(600);publicationSamples.reserve(100);
        uint64_t misses = 0, lastReport = 0,overBudget=0;
        bool battleStarted = !scaleShips;
        while (running && (!ticks || server.tick() < uint64_t(ticks))) {
            auto begin=Clock::now();
            double cpuBegin=scaleShips?threadMilliseconds():0;
            ThreadUsage usageBegin=scaleShips?threadUsage():ThreadUsage{};
            server.poll();host.poll();
            for(size_t i=0;i<registrations.size();++i){auto& registration=registrations[i];registration.poll();
                if(registration.registered()!=registered[i]){registered[i]=registration.registered();fprintf(stderr,"Public directory %s: %s\n",registration.address().c_str(),registered[i]?"registered":"registration lost");}
                if(registration.error()!=registrationErrors[i]){registrationErrors[i]=registration.error();if(!registrationErrors[i].empty())fprintf(stderr,"Public directory %s: %s\n",registration.address().c_str(),registrationErrors[i].c_str());}
            }
            auto now = Clock::now();
            if (!battleStarted) {
                deadline = now;
                if (!server.clients()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                battleStarted = true;
                puts("First client joined; combat started");
                fflush(stdout);
            }
            if (now < deadline) {
                std::this_thread::sleep_for(std::min(deadline - now, Clock::duration(std::chrono::milliseconds(1))));
                continue;
            }
            server.step();host.step();
            double stepMs = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
            samples.push_back(stepMs);
            simulationSamples.push_back(server.metrics().simulationMs);
            if(server.tick()%Native::Net::snapshotInterval==0)publicationSamples.push_back(server.metrics().publicationMs);
            if(stepMs>1000./Native::Net::tickRate)++overBudget;
            if (stepMs > 1000./Native::Net::tickRate) {
                auto usage=scaleShips?threadUsage():ThreadUsage{};
                printf("Over-budget server tick %llu: %.3f ms (CPU %.3f, user %.3f, kernel %.3f, sim %.3f, capture %.3f)\n", (unsigned long long)server.tick(), stepMs,
                    scaleShips?threadMilliseconds()-cpuBegin:0,
                    usage.user-usageBegin.user,usage.system-usageBegin.system,
                    server.metrics().simulationMs,server.metrics().publicationMs);
                size_t models=0;for(auto* l=game.core()->GetClusters()->first();l;l=l->next())models+=l->data()->GetModels()->n();
                printf("  World: %d ships, %zu models; %ld minor faults\n",game.core()->GetShips()->n(),models,usage.faults-usageBegin.faults);
                auto phases=game.simulationPhases();
                printf("  IGC phases: prep %.3f plot %.3f execute %.3f updates %.3f bounds/tree %.3f rays %.3f collisions %.3f move %.3f visibility %.3f explosions %.3f\n",phases[0],phases[1],phases[2],phases[3],phases[4],phases[5],phases[6],phases[7],phases[8],phases[9]);
                fflush(stdout);
            }
            deadline += std::chrono::nanoseconds(1000000000 / Native::Net::tickRate);
            if (Clock::now() > deadline)
                ++misses;
            if (Clock::now() - deadline > std::chrono::seconds(1)) {
                if (!scaleShips)
                    throw std::runtime_error("Simulation cannot sustain its declared cadence at tick " + std::to_string(server.tick()) +
                        " (last step " + std::to_string(stepMs) + " ms)");
                // This diagnostic must remain joinable through the busiest
                // encounters. Preserve every fixed simulation step rather than
                // terminating the match or jumping its gameplay clock forward.
                double lagMs=std::chrono::duration<double,std::milli>(Clock::now()-deadline).count();
                printf("Battle overload at tick %llu: %.1f ms wall-time delay; continuing all simulation steps\n",
                       (unsigned long long)server.tick(),lagMs);
                fflush(stdout);
                deadline=Clock::now();
            }
            if (server.tick() >= lastReport + 600) {
                auto s = samples;
                std::sort(s.begin(), s.end());
                auto p99=[](std::vector<double>& values){if(values.empty())return 0.;std::sort(values.begin(),values.end());return values[size_t(.99*(values.size()-1))];};
                printf("tick %llu clients %zu p99 %.3f ms max %.3f ms sim p99 %.3f ms capture p99 %.3f ms over budget %llu deadline misses %llu bytes sent %llu\n",
                       (unsigned long long)server.tick(), server.clients(), s[size_t(.99 * (s.size() - 1))],
                       s.back(),p99(simulationSamples),p99(publicationSamples),(unsigned long long)overBudget,
                       (unsigned long long)misses, (unsigned long long)server.metrics().bytesSent);
                samples.clear();
                simulationSamples.clear();publicationSamples.clear();
                lastReport = server.tick();
                fflush(stdout);
            }
        }
        return 0;
    } catch (const std::exception &e) {
        fprintf(stderr, "Server: %s\n", e.what());
        return 1;
    }
}
