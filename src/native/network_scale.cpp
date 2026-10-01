// Headless human-roster diagnostic. The default path isolates authority,
// visibility capture and inline encoding; --server adds encrypted transport.
#include "game.hpp"
#include "assets.hpp"
#include "network_arena.hpp"
#include "network_session.hpp"
#include "test_fixture.hpp"
#include "thread_affinity.hpp"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <unordered_set>
#include <numbers>
#include <thread>

using namespace Native;
namespace {
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point a,Clock::time_point b){return std::chrono::duration<double,std::milli>(b-a).count();}
double quantile(std::vector<double> a,double p){std::sort(a.begin(),a.end());return a[size_t(p*(a.size()-1))];}
}
int main(int argc,char** argv){try {
    Affinity::initialize();Affinity::pinCurrentThread(Affinity::Lane::Simulation);
    int teams=2,perTeam=128,ticks=360,warmup=60,seconds=20,map=0;uint16_t port=27660;bool legacy=false,fire=true,serverMode=false,largeMap=false;std::string csv,dump;
    for(int i=1;i<argc;++i){std::string_view a=argv[i];auto value=[&]{if(++i>=argc)throw std::runtime_error("Missing argument");return argv[i];};
        if(a=="--teams")teams=std::stoi(value());else if(a=="--pilots")perTeam=std::stoi(value());
        else if(a=="--ticks")ticks=std::stoi(value());else if(a=="--warmup")warmup=std::stoi(value());
        else if(a=="--legacy")legacy=true;else if(a=="--coast")fire=false;
        else if(a=="--server")serverMode=true;else if(a=="--seconds")seconds=std::stoi(value());else if(a=="--port")port=std::stoi(value());
        else if(a=="--map")map=std::stoi(value());else if(a=="--large-map")largeMap=true;
        else if(a=="--csv")csv=value();else if(a=="--dump-domains")dump=value();else throw std::runtime_error("Unknown argument");
    }
    NativeTest::require(teams>=2 && teams<=6 && perTeam>0 && teams*perTeam<shipIdentityCapacity && ticks>warmup && warmup>=0,"Invalid roster");
    Assets assets(Assets::steamArtwork());
    std::vector<std::shared_ptr<ModelData>> owners;std::unordered_map<std::string,ModelHandle> models;
    auto loader=[&](const std::string& name,const std::string&){if(auto i=models.find(name);i!=models.end())return i->second;
        auto m=assets.model(name);owners.push_back(m);return models.emplace(name,ModelHandle{1,m->radius,m->frames,m->lights}).first->second;};
    SessionOptions options;options.scenario=Scenario::Conquest;options.map=map;options.rules.mapSize=largeMap;options.rules.teams=teams;options.rules.miners=0;
    options.rules.showMap=true;
    options.rules.homeTreasures=options.rules.neutralTreasures=0;options.rules.homeTreasureRate=options.rules.neutralTreasureRate=0;
    options.rules.startingMoney=9;options.seed=47;
    if(!serverMode)for(int side=0;side<teams;++side)for(int n=side==0?1:0;n<perTeam;++n)
        options.localPilots.push_back({"Pilot "+std::to_string(side)+"/"+std::to_string(n),side,true});
    Game game(assets.root(),loader,0,0,options,{},GameRole::Authority);
    std::unique_ptr<Net::Server> server;
    if(serverMode) {
        server=std::make_unique<Net::Server>(game,options,Net::contentDigest(assets.root()),port);
        // This diagnostic measures a known-capacity local link, not GNS's
        // unfinished open-source bandwidth estimator. Production connections
        // keep their transport policy separate from this load-test fixture.
        Net::impairment(0,0,0,32*1024*1024);
        printf("Waiting for %d encrypted human connections on port %u\n",teams*perTeam,port);fflush(stdout);
        auto timeout=Clock::now()+std::chrono::seconds(90);
        while(server->clients()!=size_t(teams*perTeam) || server->readyClients()!=size_t(teams*perTeam)) {
            server->poll();if(Clock::now()>timeout)throw std::runtime_error("Roster connection timeout");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    std::vector<IclusterIGC*> sectors;for(auto* c=game.core()->GetClusters()->first();c;c=c->next())sectors.push_back(c->data());
    std::vector<int> pilots;std::vector<std::pair<int,FlightInput>> controls;std::vector<int> ordinal(teams);
    for(auto* l=game.core()->GetShips()->first();l;l=l->next()){
        auto* ship=l->data();if(ship->GetPilotType()<c_ptPlayer)continue;
        int side=ship->GetSide()->GetObjectID(),n=ordinal[side]++;size_t sector=size_t(n)%sectors.size();
        ship->SetStation(nullptr);ship->SetCluster(sectors[sector]);
        float angle=2*std::numbers::pi_v<float>*side/teams;
        Vector radial(std::cos(angle),std::sin(angle),0),tangent(-std::sin(angle),std::cos(angle),0);
        ship->SetPosition(radial*1200+tangent*(float(n/int(sectors.size()))*80-600)+Vector(0,0,700));
        Orientation o;o.Set(-radial,Vector(0,0,1));ship->SetOrientation(o);ship->SetVelocity(Vector::GetZero());
        pilots.push_back(ship->GetObjectID());FlightInput f;f.active=true;f.throttle=.7f;f.fire=fire;
        f.yaw=.035f*std::sin(float(n));f.roll=.012f*std::cos(float(n));
        controls.emplace_back(ship->GetObjectID(),Net::flight(Net::input(f,1,0)));
    }
    NativeTest::require(pilots.size()==size_t(teams*perTeam),"Roster silently capped");
    if(serverMode) {
        auto initialMetrics=server->metrics();
        auto deadline=Clock::now();std::vector<double> work,sim,capture,preparation,reports,enqueue;
        int misses=0;ticks=seconds*Net::tickRate;
        std::ofstream serverCsv;if(!csv.empty()){serverCsv.open(csv);serverCsv<<"tick,work_ms,sim_ms,publication_ms,capture_ms,enqueue_ms,reports,bytes,ships,models,pods,map_bytes,physical_bytes,trajectory_bytes,private_bytes\n";}
        for(int t=0;t<ticks;++t) {
            auto start=Clock::now();server->poll();server->step();
            work.push_back(ms(start,Clock::now()));sim.push_back(server->metrics().simulationMs);
            capture.push_back(server->metrics().publicationMs);
            preparation.push_back(server->metrics().preparationMs);reports.push_back(server->metrics().captureMs);enqueue.push_back(server->metrics().enqueueMs);
            deadline+=std::chrono::nanoseconds(1000000000/Net::tickRate);
            if(Clock::now()>deadline){++misses;fprintf(stderr,"Deadline miss tick %d: work %.3f ms, sim %.3f ms, publication %.3f ms\n",t+1,work.back(),sim.back(),capture.back());
                auto phases=game.simulationPhases();size_t pods=0;
                for(auto* l=game.core()->GetShips()->first();l;l=l->next())pods+=l->data()->GetBaseHullType()->HasCapability(c_habmLifepod);
                fprintf(stderr,"  %zu pods; cluster phases ms:",pods);for(auto phase:phases)fprintf(stderr," %.3f",phase);fprintf(stderr,"\n");}
            size_t modelCount=0,pods=0;if(serverCsv){for(auto* c:sectors)modelCount+=c->GetModels()->n();
                for(auto* l=game.core()->GetShips()->first();l;l=l->next())pods+=l->data()->GetBaseHullType()->HasCapability(c_habmLifepod);}
            if(serverCsv){const auto& metrics=server->metrics();serverCsv<<t+1<<','<<work.back()<<','<<sim.back()<<','<<capture.back()<<','<<metrics.captureMs<<','<<metrics.enqueueMs<<','<<metrics.snapshots-initialMetrics.snapshots<<','<<metrics.bytesSent-initialMetrics.bytesSent<<','<<game.core()->GetShips()->n()<<','<<modelCount<<','<<pods;
                for(auto bytes:metrics.arenaTraffic)serverCsv<<','<<bytes;serverCsv<<'\n';}
            std::this_thread::sleep_until(deadline);
        }
        auto& metrics=server->metrics();
        printf("%s",server->publicationDiagnostic().c_str());
        printf("UDP authority: %zu pilots, %zu sectors, %d seconds; work p99 %.3f ms, max %.3f ms, sim p99 %.3f ms, capture p99 %.3f ms; %d deadline misses\n",server->clients(),sectors.size(),seconds,quantile(work,.99),*std::max_element(work.begin(),work.end()),quantile(sim,.99),quantile(capture,.99),misses);
        printf("Publication components p99: preparation %.3f ms, captures %.3f ms, enqueue %.3f ms\n",quantile(preparation,.99),quantile(reports,.99),quantile(enqueue,.99));
        printf("UDP traffic %.2f MiB/s down, %.2f MiB/s up; %llu reports, %llu arena encodes, %.2f MiB shared history; rejected %llu, input drops %llu\n",(metrics.bytesSent-initialMetrics.bytesSent)/double(seconds)/1048576.,(metrics.bytesReceived-initialMetrics.bytesReceived)/double(seconds)/1048576.,(unsigned long long)(metrics.snapshots-initialMetrics.snapshots),(unsigned long long)(metrics.arenaEncodes-initialMetrics.arenaEncodes),metrics.arenaBytes/1048576.,(unsigned long long)(metrics.rejected-initialMetrics.rejected),(unsigned long long)(metrics.inputDrops-initialMetrics.inputDrops));
        printf("Arena traffic MiB/s: map %.2f, physical %.2f, trajectories %.2f, private %.2f; bootstrap %.2f MiB\n",metrics.arenaTraffic[0]/double(seconds)/1048576.,metrics.arenaTraffic[1]/double(seconds)/1048576.,metrics.arenaTraffic[2]/double(seconds)/1048576.,metrics.arenaTraffic[3]/double(seconds)/1048576.,initialMetrics.bytesSent/1048576.);
        fflush(stdout);return misses?2:0;
    }
    printf("Human network scale: %d factions x %d pilots, %zu sectors, %s, %s\n",teams,perTeam,sectors.size(),legacy?"per-peer encoding":"shared arenas",fire?"firing":"coasting");fflush(stdout);
    Net::ArenaEncoder encoder;uint64_t sequence=0,wire=0,rawPeak=0,publications=0;
    if(!dump.empty())std::filesystem::create_directories(dump);
    std::vector<Net::Frame::Effects> effects(pilots.size());
    std::vector<std::vector<std::byte>> baselines(legacy?pilots.size():0);
    std::vector<std::unordered_map<uint32_t,uint64_t>> checkpoints(pilots.size());
    std::vector<double> work,simulation,capture,encode;std::ofstream output;
    if(!csv.empty()){output.open(csv);output<<"tick,sim_ms,capture_ms,encode_ms,work_ms,bytes,arena_encodes,retained_bytes,models\n";}
    for(int t=1;t<=ticks;++t){auto begin=Clock::now();game.authorityStep(controls,1./Net::tickRate);auto simulated=Clock::now();
        double captured=0,encoded=0;uint64_t bytes=0;
        if(t%Net::snapshotInterval==0){game.beginNetworkPublication();
            for(size_t i=0;i<pilots.size();++i){auto a=Clock::now();
                auto report=game.networkReport(pilots[i],t,t,effects[i]);auto b=Clock::now();captured+=ms(a,b);
                auto& f=report.personal;f.header.sequence=++sequence;f.status.match=1;
                if(!f.notices.empty())effects[i].notices=f.notices.back().number;
                if(!f.events.empty())effects[i].events=f.events.back().number;
                for(const auto& world:report.worlds)if(!world.frame->events.empty())effects[i].events=std::max(effects[i].events,world.frame->events.back().number);
                if(legacy){std::vector<const Net::Frame*> worlds;for(const auto& world:report.worlds)worlds.push_back(world.frame.get());
                    auto frame=Net::composeReport(Net::combineWorlds(worlds),std::move(f));
                    auto full=Net::packet(frame);auto delta=Net::compressed(full,t-Net::snapshotInterval,baselines[i]);
                    bytes+=delta.size();rawPeak=std::max<uint64_t>(rawPeak,full.size());baselines[i]=std::move(full);
                }else{std::vector<Net::ArenaRef> references;
                    for(const auto& world:report.worlds) {
                      for(const auto& arena:encoder.publishWorld(world.domain,t,1,*world.frame)) {
                        if(!dump.empty() && (t<=2*Net::snapshotInterval || t==600 || t==606)){
                            auto path=std::filesystem::path(dump)/("domain-"+std::to_string(arena->domain)+"-tick-"+std::to_string(t)+".bin");
                            if(!std::filesystem::exists(path)){std::ofstream out(path,std::ios::binary);
                                out.write(reinterpret_cast<const char*>(arena->raw->data()),arena->raw->size());}
                        }
                        auto& ack=checkpoints[i][arena->domain];
                        if(!(arena->domain&0x800000)){if(auto update=encoder.stateUpdate(*arena,ack))bytes+=update->size();}
                        else {
                            if(ack<arena->chainBase){bytes+=arena->checkpointBytes->size();ack=arena->checkpoint;}
                            for(const auto& block:arena->chain)if(block.revision>ack)bytes+=block.bytes->size();
                        }
                        ack=arena->revision;references.push_back({arena->domain,(arena->domain&0x800000)?Net::presentationArena:world.flags,arena->revision});
                        rawPeak=std::max<uint64_t>(rawPeak,arena->raw->size());
                      }
                    }
                    bytes+=Net::reportPacket(std::move(f),references).size();
                }
                encoded+=ms(b,Clock::now());++publications;
            }
        }
        wire+=bytes;double elapsed=ms(begin,Clock::now());
        if(t>warmup){simulation.push_back(ms(begin,simulated));work.push_back(elapsed);if(bytes){capture.push_back(captured);encode.push_back(encoded);}}
        if(output)output<<t<<','<<ms(begin,simulated)<<','<<captured<<','<<encoded<<','<<elapsed<<','<<bytes<<','<<encoder.encodes()<<','<<encoder.retainedBytes()<<','<<game.core()->GetShips()->n()<<'\n';
    }
    printf("sim p99 %.3f ms; total work p99 %.3f ms, max %.3f ms; publication capture p99 %.3f ms, encode p99 %.3f ms\n",quantile(simulation,.99),quantile(work,.99),*std::max_element(work.begin(),work.end()),quantile(capture,.99),quantile(encode,.99));
    printf("%llu recipient reports; %llu arena encodes; raw peak %.1f KiB; shared history %.2f MiB; downstream %.2f MiB/s aggregate, %.2f KiB/s per pilot\n",(unsigned long long)publications,(unsigned long long)encoder.encodes(),rawPeak/1024.,encoder.retainedBytes()/1048576.,wire/double(ticks)*Net::tickRate/1048576.,wire/double(ticks)*Net::tickRate/pilots.size()/1024.);
    return 0;
}catch(const std::exception& e){fprintf(stderr,"Network scale: %s\n",e.what());return 1;}}
