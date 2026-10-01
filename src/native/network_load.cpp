// Encrypted UDP load agents. They exercise the actual wire codec, receipts,
// visibility subscriptions and compact input bundles, without rendering or
// duplicating hundreds of client IGC simulations on the server test machine.
#include "network_session.hpp"
#include "network_codec.hpp"
#include "assets.hpp"
#include "thread_affinity.hpp"
#include <chrono>
#include <cstdio>
#include <thread>
#include <cmath>
#include <atomic>
#include <csignal>
#include <fstream>
#include <steam/steamnetworkingsockets.h>
using namespace Native;
namespace {
using Clock=std::chrono::steady_clock;
std::atomic_bool running{true};
void stop(int){running=false;}
using Bytes=std::shared_ptr<const std::vector<std::byte>>;
// Load agents share decoding of identical, fully received blocks. A peer must
// still receive every fragment and possess its own delta basis before using
// this cache, so it cannot conceal loss or repair failures on that connection.
std::unordered_map<uint32_t,std::map<uint64_t,Bytes>> decodedArenas;
struct Peer {
    int side{},index{},pilot{-1};uint64_t match{},receipt{},number{},tick{},reports{},misses{};
    Clock::time_point received{};
    int64_t lastProbe{},clockServer{};uint64_t clockTick{};
    double offset{},bestRtt{1e12};
    std::deque<Net::Input> inputs;
    std::map<uint64_t,std::vector<std::byte>> bootstrap;
    std::unordered_map<uint32_t,std::map<uint64_t,Bytes>> arenas;
    std::unordered_map<uint32_t,uint64_t> checkpoints;
    struct Assembly {uint32_t count{};std::vector<std::byte> bytes;std::vector<bool> pieces;};
    std::map<uint64_t,Assembly> assemblies;
    struct Report {Net::Header header;std::vector<Net::ArenaRef> references;};
    std::map<uint64_t,Report> pending;
    std::vector<Native::Net::Message> early;
    std::vector<double> ages;
    std::vector<double> authorityAges;
};
void apply(Peer& peer) {
    for(auto i=peer.pending.begin();i!=peer.pending.end();) {
        if(i->first<=peer.receipt){i=peer.pending.erase(i);continue;}
        bool complete=true;
        for(const auto& reference:i->second.references){if(reference.reserved&Net::presentationArena)continue;auto arena=peer.arenas.find(reference.domain);
            if(arena==peer.arenas.end() || !arena->second.contains(reference.revision)){complete=false;break;}}
        if(!complete){++i;continue;}
        peer.bootstrap.clear();peer.receipt=i->first;peer.tick=i->second.header.tick;peer.received=Clock::now();++peer.reports;
        i=peer.pending.erase(i);
    }
    while(peer.pending.size()>8)peer.pending.erase(peer.pending.begin());
}
void receive(Peer& peer,const Net::Message& message) {
    Net::Reader reader(message.data);auto h=reader.take<Net::Header>();std::vector<std::byte> unpacked;
    Net::unpackMessage(h,reader,unpacked);
    if(h.kind==Net::Kind::Welcome) {
        auto welcome=reader.take<Net::Welcome>();peer.match=welcome.match;peer.pilot=welcome.pilot;peer.number=welcome.inputAck;
        auto early=std::move(peer.early);for(const auto& m:early)receive(peer,m);return;
    }
    if(!peer.match){peer.early.push_back(message);return;}
    if(h.kind==Net::Kind::Fragment || h.kind==Net::Kind::ArenaFragment) {
        auto fragment=reader.take<Net::Fragment>();
        if(fragment.size>Net::maxFrame*2 || fragment.offset>=fragment.size || fragment.offset%Net::fragmentBytes)
            throw std::runtime_error("Load fragment bounds");
        auto& assembly=peer.assemblies[h.sequence];
        if(assembly.bytes.empty()){assembly.bytes.resize(fragment.size);assembly.pieces.resize((fragment.size+Net::fragmentBytes-1)/Net::fragmentBytes);}
        size_t slot=fragment.offset/Net::fragmentBytes;
        if(assembly.bytes.size()!=fragment.size || reader.rest().size()!=std::min(Net::fragmentBytes,size_t(fragment.size-fragment.offset)))
            throw std::runtime_error("Load fragment length");
        if(!assembly.pieces[slot]){std::memcpy(assembly.bytes.data()+fragment.offset,reader.rest().data(),reader.rest().size());assembly.pieces[slot]=true;++assembly.count;}
        if(assembly.count==assembly.pieces.size()){Net::Message complete{message.peer,std::move(assembly.bytes)};peer.assemblies.erase(h.sequence);receive(peer,complete);}
        while(peer.assemblies.size()>Net::maxReportArenas*4)peer.assemblies.erase(peer.assemblies.begin());return;
    }
    if(h.kind==Net::Kind::Delta) {
        if(h.sequence<=peer.receipt)return;
        auto full=Net::expanded(message.data,peer.bootstrap);if(full.empty())throw std::runtime_error("Bootstrap basis missing");
        auto f=Net::frame(full);if(f.status.pilot!=peer.pilot || f.status.match!=peer.match)throw std::runtime_error("Bootstrap pilot mismatch");
        peer.receipt=h.sequence;peer.tick=h.tick;peer.received=Clock::now();peer.bootstrap[h.sequence]=std::move(full);return;
    }
    if(h.kind==Net::Kind::Arena) {
        auto arena=reader.take<Net::ArenaRecord>();if(arena.match!=peer.match)return;
        // Public projectile records are visible in the viewed sector; every
        // faction-specific category must match this authenticated pilot.
        if(!(arena.domain&0x800000) && (arena.domain&255)!=unsigned(peer.side))
            throw std::runtime_error("Another faction's visibility arena leaked");
        auto& history=peer.arenas[arena.domain];
        if(!history.empty() && h.tick<=history.rbegin()->first){return;}
        Net::Reader delta(reader.rest());delta.take<Net::Header>();auto basis=delta.take<Net::Delta>();
        auto previous=history.find(basis.baseline);
        if(basis.baseline && previous==history.end()){++peer.misses;return;}
        auto& cache=decodedArenas[arena.domain];Bytes full;
        if(auto found=cache.find(h.tick);found!=cache.end())full=found->second;
        else {
            full=std::make_shared<const std::vector<std::byte>>(Net::expandedAgainst(reader.rest(),basis.baseline?std::span(*previous->second):std::span<const std::byte>{}));
            cache[h.tick]=full;while(cache.size()>4)cache.erase(cache.begin());
        }
        Net::Reader decoded(*full);auto revision=decoded.take<Net::Header>();auto status=decoded.take<Net::Snapshot>();
        if(status.pilot!=-1 || status.match!=peer.match || revision.sequence!=h.tick)
            throw std::runtime_error("World identity mismatch");
        history[revision.sequence]=std::move(full);
        if(!basis.baseline)peer.checkpoints[arena.domain]=revision.sequence;
        while(history.size()>4){auto oldest=history.begin();if(oldest->first==peer.checkpoints[arena.domain])++oldest;history.erase(oldest);}
        apply(peer);return;
    }
    if(h.kind==Net::Kind::Report) {
        auto manifest=reader.take<Net::ArenaManifest>();if(manifest.count==0 || manifest.count>Net::maxReportArenas)throw std::runtime_error("Manifest count");
        auto refs=reader.array<Net::ArenaRef>(manifest.count);auto full=Net::expanded(reader.rest(),{});auto personal=Net::frame(full);
        if(personal.status.pilot!=peer.pilot || personal.status.match!=peer.match || !personal.entities.empty())
            throw std::runtime_error("Private pilot mismatch");
        peer.pending[h.sequence]={h,std::move(refs)};apply(peer);return;
    }
    if(h.kind==Net::Kind::Pong) {
        auto probe=reader.take<Net::ClockProbe>();auto current=Net::now();
        double rtt=current-probe.client-(probe.serverSend-probe.serverReceive);
        if(h.tick && rtt>=0 && rtt<peer.bestRtt*1.1) {
            peer.bestRtt=std::min(peer.bestRtt,rtt);
            double offset=((probe.serverReceive-probe.client)+(probe.serverSend-current))*.5;
            peer.offset=peer.clockServer?peer.offset*.9+offset*.1:offset;
            peer.clockServer=probe.serverSend;peer.clockTick=h.tick;
        }
        return;
    }
    if(h.kind==Net::Kind::Receipt)return;
    throw std::runtime_error("Unexpected load agent record");
}
}
int main(int argc,char** argv){try{
    // Synthetic clients share this host with the authority. Use its normal
    // worker placement, leaving authority/publication cores out of this load.
    Affinity::initialize();const Affinity::ScopedUnpin agentPlacement(true);
    int count=128,start=0,perTeam=128,seconds=30;bool fire=true,withReplica=false;std::string address="127.0.0.1:27660",csv;
    for(int i=1;i<argc;++i){std::string_view a=argv[i];auto value=[&]{if(++i>=argc)throw std::runtime_error("Missing argument");return argv[i];};
        if(a=="--count")count=std::stoi(value());else if(a=="--start")start=std::stoi(value());else if(a=="--pilots")perTeam=std::stoi(value());
        else if(a=="--seconds")seconds=std::stoi(value());else if(a=="--address")address=value();else if(a=="--csv")csv=value();else if(a=="--replica")withReplica=true;else if(a=="--coast")fire=false;else throw std::runtime_error("Unknown argument");}
    if(count<1 || perTeam<1 || start<0 || start+count>6*perTeam)throw std::runtime_error("Invalid load roster");
    auto [transport,first]=Net::connect(address);Net::impairment(0,0,0,32*1024*1024);
    std::unordered_map<Net::Connection,Peer> peers;
    for(int i=0;i<count-(withReplica?1:0);++i){auto c=i?transport->open(address):first;Peer peer;peer.side=(start+i)/perTeam;peer.index=start+i;peers.emplace(c,std::move(peer));}
    auto content=Net::contentDigest(Assets::steamArtwork());uint64_t sent=0,received=0;
    std::unique_ptr<Assets> assets;std::unique_ptr<Game> game;std::unique_ptr<Net::Client> replica;
    std::vector<std::shared_ptr<ModelData>> modelOwners;std::unordered_map<std::string,ModelHandle> models;
    if(withReplica){
        if(count<2)throw std::runtime_error("Replica fixture needs at least one codec peer");
        assets=std::make_unique<Assets>(Assets::steamArtwork());
        replica=std::make_unique<Net::Client>(address,content,"IGC replica",(start+count-1)/perTeam,[&](const SessionOptions& session)->Game&{
            game=std::make_unique<Game>(assets->root(),[&](const std::string& name,const std::string&){
                if(auto found=models.find(name);found!=models.end())return found->second;
                auto model=assets->model(name);modelOwners.push_back(model);
                return models.emplace(name,ModelHandle{1,model->radius,model->frames,model->lights}).first->second;
            },0,0,session,EffectLoader{},GameRole::Replica);
            return *game;
        });
    }
    std::signal(SIGINT,stop);std::signal(SIGTERM,stop);
    auto end=Clock::now()+std::chrono::seconds(seconds),deadline=Clock::now();bool disconnected=false;
    auto started=Clock::now();std::ofstream output;
    auto lastReplica=started;Clock::time_point flying{};int replicaStage=0;double commandStarted{};std::vector<double> replicaWork;
    if(!csv.empty()){output.open(csv);output<<"elapsed_s,min_tick,max_tick,reports,gap_max_ms,authority_max_ms\n";}
    while(running && Clock::now()<end && !disconnected) {
        auto incoming=transport->receive();
        for(const auto& e:transport->events()) {
            if(!peers.contains(e.peer))continue;
            if(!e.connected){disconnected=true;break;}
            Net::Hello hello;hello.build=Net::buildDigest();hello.content=content;hello.side=peers.at(e.peer).side;
            Net::text(hello.name,"Load pilot "+std::to_string(peers.at(e.peer).index));
            std::vector<std::byte> bytes;Net::Header header;header.kind=Net::Kind::Hello;
            Net::append(bytes,header);Net::append(bytes,hello);sent+=bytes.size();transport->send(e.peer,bytes,Net::Lane::Control);
        }
        for(const auto& message:incoming){received+=message.data.size();receive(peers.at(message.peer),message);}
        bool sample=Clock::now()>=deadline;
        if(replica && sample){
            auto before=Clock::now();FlightInput input;input.active=true;input.throttle=.7f;input.fire=fire;
            replica->step(input,std::chrono::duration<double>(before-lastReplica).count(),{monotonicNs(),0});lastReplica=before;
            replicaWork.push_back(std::chrono::duration<double,std::milli>(Clock::now()-before).count());
            if(replica->closed())throw std::runtime_error("IGC replica disconnected: "+replica->error());
            auto rejected=replica->takeRejection();if(!rejected.empty())throw std::runtime_error("IGC replica action rejected: "+rejected);
            if(replica->ready() && game && !game->state().docked){
                if(flying==Clock::time_point{})flying=before;
                double elapsed=std::chrono::duration<double>(before-flying).count();
                if(!replicaStage && elapsed>2 && !game->state().cinematic){
                    auto sector=std::find_if(game->state().sectors.begin(),game->state().sectors.end(),[&](const auto& s){return s.id!=game->state().sectorId;});
                    if(sector==game->state().sectors.end())throw std::runtime_error("IGC replica has no known alternate sector");
                    int target=sector->id;game->commandView(true);
                    if(!game->viewSector(target))throw std::runtime_error("IGC replica command-sector switch failed");replicaStage=1;commandStarted=elapsed;
                }
                if(replicaStage==1 && elapsed>commandStarted+2){
                    if(!game->state().commandView)throw std::runtime_error("IGC replica command view did not activate");
                    game->commandView(false);replicaStage=2;
                }
                const auto& position=game->state().position;
                if(!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))throw std::runtime_error("IGC replica position is not finite");
            }
        }
        std::vector<Net::OutgoingMessage> outgoing;
        uint64_t minTick=~uint64_t(0),maxTick=0,reports=0;double maxGap=0,maxAuthorityAge=0;
        for(auto& [connection,peer]:peers)if(peer.match) {
            if(sample) {
                auto current=Net::now();
                if(current-peer.lastProbe>500000){
                    peer.lastProbe=current;std::vector<std::byte> probe;
                    Net::append(probe,Net::Header{Net::magic,Net::Kind::Ping,0,0,peer.receipt});
                    Net::append(probe,Net::ClockProbe{current,0,0});
                    outgoing.push_back({connection,std::make_shared<const std::vector<std::byte>>(std::move(probe)),Net::Lane::State,true});
                }
                std::vector<std::byte> bytes;Net::append(bytes,Net::InputHeader{Net::magic,Net::Kind::Input,uint32_t(peer.match),uint32_t(peer.receipt)});
                if(peer.tick) {
                    FlightInput f;f.active=true;f.throttle=.7;f.fire=fire;
                    f.yaw=.035f*std::sin(float(peer.index));f.roll=.012f*std::cos(float(peer.index));
                    double age=std::chrono::duration<double>(Clock::now()-peer.received).count();peer.ages.push_back(age*1000);
                    maxGap=std::max(maxGap,age*1000);minTick=std::min(minTick,peer.tick);maxTick=std::max(maxTick,peer.tick);reports+=peer.reports;
                    if(peer.clockServer){
                        double authorityAge=(current+peer.offset-peer.clockServer)*.001+
                            (int64_t(peer.clockTick)-int64_t(peer.tick))*1000./Net::tickRate;
                        peer.authorityAges.push_back(std::max(0.,authorityAge));maxAuthorityAge=std::max(maxAuthorityAge,authorityAge);
                    }
                    uint64_t tick=peer.tick+uint64_t(age*Net::tickRate);
                    peer.inputs.push_back(Net::input(f,++peer.number,tick));while(peer.inputs.size()>3)peer.inputs.pop_front();
                    for(const auto& input:peer.inputs)Net::append(bytes,input);
                }
                outgoing.push_back({connection,std::make_shared<const std::vector<std::byte>>(std::move(bytes)),Net::Lane::State,true});
            }
        }
        for(const auto& message:outgoing)sent+=message.data->size();transport->sendBatch(outgoing);
        if(sample)deadline=Clock::now()+std::chrono::nanoseconds(1000000000/Net::tickRate);
        if(sample && maxTick && output)output<<std::chrono::duration<double>(Clock::now()-started).count()<<','<<minTick<<','<<maxTick<<','<<reports<<','<<maxGap<<','<<maxAuthorityAge<<'\n';
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    uint64_t reports=0,misses=0,minReports=~uint64_t(0);std::vector<double> ages,authorityAges;
    for(const auto& [_,peer]:peers){reports+=peer.reports;misses+=peer.misses;minReports=std::min(minReports,peer.reports);ages.insert(ages.end(),peer.ages.begin(),peer.ages.end());authorityAges.insert(authorityAges.end(),peer.authorityAges.begin(),peer.authorityAges.end());}
    std::sort(ages.begin(),ages.end());double p99=ages.empty()?0:ages[size_t(.99*(ages.size()-1))];
    auto worst=std::min_element(peers.begin(),peers.end(),[](const auto& a,const auto& b){return a.second.reports<b.second.reports;});
    if(worst!=peers.end()) {
        auto& peer=worst->second;
        printf("Worst pilot %d: report tick %llu, pending %zu, assemblies %zu\n",peer.pilot,(unsigned long long)peer.tick,peer.pending.size(),peer.assemblies.size());
        if(!peer.pending.empty())for(const auto& ref:peer.pending.rbegin()->second.references)if(!(ref.reserved&Net::presentationArena)) {
            const auto& arena=peer.arenas[ref.domain];
            printf("  required domain %x revision %llu, available %llu..%llu\n",ref.domain,(unsigned long long)ref.revision,(unsigned long long)(arena.empty()?0:arena.begin()->first),(unsigned long long)(arena.empty()?0:arena.rbegin()->first));
        }
        char status[8192];if(SteamNetworkingSockets()->GetDetailedConnectionStatus(worst->first,status,sizeof(status))==0)printf("%s\n",status);
    }
    printf("Encrypted UDP agents: %zu peers, %llu reports (%llu minimum per pilot), %llu missing bases, p99 report age %.1f ms; %.2f MiB received, %.2f MiB sent\n",peers.size(),(unsigned long long)reports,(unsigned long long)minReports,(unsigned long long)misses,p99,received/1048576.,sent/1048576.);
    std::sort(authorityAges.begin(),authorityAges.end());
    printf("Clock-probed authority age: p99 %.1f ms (%zu samples; up to one simulation tick of phase uncertainty)\n",authorityAges.empty()?0:authorityAges[size_t(.99*(authorityAges.size()-1))],authorityAges.size());
    if(replica){
        const auto& metrics=replica->metrics();std::sort(replicaWork.begin(),replicaWork.end());
        printf("IGC replica: %llu reports, %llu missing bases, %d command-view stages, step p99 %.3f ms, %.2f MiB received\n",
            (unsigned long long)metrics.snapshots,(unsigned long long)metrics.missingBaselines,replicaStage,
            replicaWork.empty()?0:replicaWork[size_t(.99*(replicaWork.size()-1))],metrics.bytesReceived/1048576.);
        if(!replica->ready() || metrics.missingBaselines || replicaStage!=2)throw std::runtime_error("IGC replica did not complete the scale fixture");
    }
    return minReports?0:2;
}catch(const std::exception& e){fprintf(stderr,"Network load: %s\n",e.what());return 1;}}
