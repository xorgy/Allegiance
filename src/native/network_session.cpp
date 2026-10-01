#include "network_session.hpp"
#include "thread_affinity.hpp"
#include "network_codec.hpp"
#include <glm/gtc/quaternion.hpp>
#include "igc.h"
#include <lz4.h>
#include "crypto.hpp"
#include "../ClientCore/LobbyReady.h"
#include "../ClientCore/LobbyActions.h"
#include "../ClientCore/MissionLifecycle.h"
#include "../ClientCore/MissionChatCommands.h"
#include "../ClientCore/PlayerPanePresentation.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <atomic>
namespace Native::Net {
namespace {
// Display/input sampling can run faster than the fixed simulation. Retain
// digital presses until an input tick consumes them; analog values stay latest.
constexpr uint32_t sampledButtons = Forward | Backward | Left | Right | Up | Down | Boost | Fire | Missile |
                                    Chaff | Deploy | ThrottleChanged | MatchSpeed | ManualOverride;
constexpr bool transferOperation(const Action& action){return action.op==Operation::Board || action.op==Operation::Leave ||
    action.op==Operation::Teleport || (action.op==Operation::Team && action.args[0]==noTeam) ||
    (action.op==Operation::Lobby && action.args[0]==int(LobbyAction::CancelRequest));}
template <Record T>
std::vector<std::byte> message(Kind kind, const T &record, Tick tick = 0, uint64_t sequence = 0, uint64_t ack = 0) {
    std::vector<std::byte> out;
    append(out, Header{magic, kind, tick, sequence, ack});
    append(out, record);
    return compactMessage(std::move(out));
}
glm::quat orientation(ImodelIGC *model) {
    if (!model || !model->GetThingSite())
        return {1, 0, 0, 0};
    const auto &o = model->GetOrientation();
    auto vector = [](const Vector &v) { return glm::vec3(v.x, v.y, v.z); };
    return glm::normalize(glm::quat_cast(glm::mat3(vector(o.GetRight()), vector(o.GetUp()), vector(o.GetBackward()))));
}
bool valid(const Input &i) {
    return i.yaw != -128 && i.pitch != -128 && i.roll != -128 && !(i.buttons & ~32767u);
}
template <class Map> void bound(Map &m, size_t count) {
    while (m.size() > count)
        m.erase(m.begin());
}
template <class Map> void boundSnapshots(Map &history,uint64_t pinned=0) {
    auto size=[](const auto& data){if constexpr(requires{data->size();})return data->size();else return data.size();};
    size_t bytes = 0;
    for (auto &[_, data] : history)
        bytes += size(data);
    // Keep the delta basis and several in-flight frames even when a crowded
    // sector makes one frame exceed the ordinary byte budget. A one-frame
    // history cannot acknowledge a publication after the next one is sent.
    while (history.size() > 4 && (history.size() > 32 || bytes > 8 * 1024 * 1024)) {
        auto oldest=history.begin();
        if(oldest->first==pinned)++oldest;
        bytes -= size(oldest->second);
        history.erase(oldest);
    }
}
} // namespace
// Only immutable, pointer-free captured records cross this boundary. Gameplay
// and peer receipts stay on the authority thread. Encoding, connection admission
// and batched dispatch run here; a slow transport cannot hold the simulation.
// Each peer has at most one job in flight, bounding work and captured memory.
struct Server::Publisher {
    using SnapshotHistory=std::map<uint64_t,SharedBytes>;
    struct Result {
        Connection connection{};uint64_t match{},notice{},event{};Header header;
        uint64_t encodes{},retainedBytes{},wireBytes{};bool published{},backpressure{};
        std::array<uint64_t,4> traffic{};
        std::vector<ArenaRef> revisions;std::string error;
    };
    struct Job {
        Connection connection{};uint64_t match{},notice{},event{};Header header;
        uint64_t encodes{},retainedBytes{},wireBytes{};
        std::array<uint64_t,4> traffic{};
        bool published{},backpressure{},prepared{};
        int64_t admissionDeadline{};
        Frame captured;std::vector<ReportCapture::World> worlds;
        std::vector<ArenaRef> bases,revisions;
        struct Block {SharedBytes bytes;bool arena;};
        std::vector<Block> blocks;
        std::string error;
    };
    Transport& transport;
    std::atomic_uint64_t match{1};
    std::mutex mutex;std::condition_variable wake;
    std::deque<Job> queued;std::vector<Result> completed;std::vector<Frame> retiredFrames;
    std::vector<SnapshotHistory> retiredSnapshots;
    ArenaEncoder encoder;
    bool stopping{};std::thread worker;
    explicit Publisher(Transport& t):transport(t),worker(Affinity::createThread([this]{Affinity::pinCurrentThread(Affinity::Lane::Publication);run();})){}
    ~Publisher(){
        {std::lock_guard lock(mutex);stopping=true;queued.clear();}
        wake.notify_one();worker.join();
    }
    void submit(Job job){
        // Admission can fail just before a preceding burst finishes draining.
        // Retain this immutable capture for one simulation tick instead of
        // turning a brief queue tail into a whole missed publication interval.
        job.admissionDeadline=now()+1000000/tickRate;
        {std::lock_guard lock(mutex);queued.push_back(std::move(job));}
        wake.notify_one();
    }
    std::vector<Result> take(){std::lock_guard lock(mutex);return std::exchange(completed,{});}
    void retire(SnapshotHistory history){
        {std::lock_guard lock(mutex);retiredSnapshots.push_back(std::move(history));}
        wake.notify_one();
    }
    Frame acquireFrame(){
        std::lock_guard lock(mutex);if(retiredFrames.empty())return {};
        auto frame=std::move(retiredFrames.back());retiredFrames.pop_back();return frame;
    }
    void recycle(Frame frame){
        frame.clear();std::lock_guard lock(mutex);
        if(retiredFrames.size()<8)retiredFrames.push_back(std::move(frame));
    }
    void dispatch(std::vector<Job>& jobs){
        struct Owner {size_t job;unsigned category;};
        std::vector<OutgoingMessage> outgoing;std::vector<Owner> owners;
        std::unordered_map<SharedBytes,std::vector<SharedBytes>> fragments;
        auto queue=[&](Connection connection,SharedBytes bytes,size_t owner,bool arena) {
            Reader envelope(*bytes);auto identity=envelope.take<Header>();
            unsigned category=!arena?3:(identity.sequence&0x800000)?2:(identity.sequence&0x400000)?1:0;
            // A manifest follows required revisions on the same ordered lane.
            Lane lane=arena && (identity.sequence&0x800000)?Lane::Projectiles:Lane::World;
            auto& pieces=fragments[bytes];
            if(pieces.empty()) {
                if(bytes->size()<=maxPacket)pieces.push_back(bytes);
                else for(size_t offset=0;offset<bytes->size();offset+=fragmentBytes) {
                    std::vector<std::byte> piece;piece.reserve(sizeof(Header)+sizeof(Fragment)+fragmentBytes);
                    auto h=identity;h.kind=arena?Kind::ArenaFragment:Kind::Fragment;
                    append(piece,h);append(piece,Fragment{uint32_t(bytes->size()),uint32_t(offset)});
                    append(piece,std::span(*bytes).subspan(offset,std::min(fragmentBytes,bytes->size()-offset)));
                    pieces.push_back(std::make_shared<const std::vector<std::byte>>(std::move(piece)));
                }
            }
            for(const auto& piece:pieces){outgoing.push_back({connection,piece,lane});owners.push_back({owner,category});}
        };
        for(size_t i=0;i<jobs.size();++i){auto& job=jobs[i];
            if(job.match!=match.load())continue;
            try {
              if(!job.prepared){
                // Encode in capture order, before admission. Deferred sends
                // keep these immutable blocks; they must not ask the encoder
                // to revisit an older revision after other pilots advance it.
                auto before=encoder.encodes();std::vector<ArenaRef> references;
                auto retain=[&](SharedBytes bytes,bool arena){job.blocks.push_back({std::move(bytes),arena});};
                for(const auto& world:job.worlds)
                    for(const auto& arena:encoder.publishWorld(world.domain,world.frame->header.tick,job.match,*world.frame)) {
                        references.push_back({arena->domain,(arena->domain&0x800000)?presentationArena:world.flags,arena->revision});
                        auto basis=std::find_if(job.bases.begin(),job.bases.end(),[&](const auto& b){return b.domain==arena->domain;});
                        uint64_t sent=basis==job.bases.end()?0:basis->revision;
                        if(!(arena->domain&0x800000)){
                            if(auto update=encoder.stateUpdate(*arena,sent))retain(update,true);
                        }else {
                            if(sent<arena->chainBase){retain(arena->checkpointBytes,true);sent=arena->checkpoint;}
                            for(const auto& block:arena->chain)if(block.revision>sent)retain(block.bytes,true);
                        }
                    }
                job.encodes=encoder.encodes()-before;
                retain(std::make_shared<const std::vector<std::byte>>(reportPacket(std::move(job.captured),references)),false);
                job.revisions=std::move(references);job.prepared=true;
                job.worlds.clear();
              }
              if(!transport.writable(job.connection)){job.backpressure=true;continue;}
              job.backpressure=false;
              for(const auto& block:job.blocks)queue(job.connection,block.bytes,i,block.arena);
              job.published=true;
            }catch(const std::exception& e){job.error=e.what();}
            job.worlds.clear();
        }
        auto results=transport.sendBatch(outgoing);
        if(results.size()!=outgoing.size())throw std::logic_error("Transport batch receipts");
        for(size_t i=0;i<outgoing.size();++i){auto& job=jobs[owners[i].job];
            job.published=job.published && results[i];
            if(results[i]){job.wireBytes+=outgoing[i].data->size();job.traffic[owners[i].category]+=outgoing[i].data->size();}
        }
        // Retained history is a property of the shared encoder, not each pilot.
        if(!jobs.empty())jobs.back().retainedBytes=encoder.retainedBytes();
        for(auto& job:jobs)if(!job.backpressure)job.worlds.clear();
    }
    void run(){
        std::deque<Job> deferred;
        for(;;){
            std::vector<Job> jobs;std::vector<SnapshotHistory> retired;
            {std::unique_lock lock(mutex);
                auto ready=[&]{return stopping || !queued.empty() || !retiredSnapshots.empty();};
                if(deferred.empty())wake.wait(lock,ready);
                else wake.wait_for(lock,std::chrono::milliseconds(1),ready);
                if(stopping)return;
                retired=std::move(retiredSnapshots);retiredSnapshots.clear();
                jobs.reserve(128);
                size_t retryCount=std::min<size_t>(deferred.size(),queued.empty()?128:64);
                while(retryCount--){jobs.push_back(std::move(deferred.front()));deferred.pop_front();}
                while(!queued.empty() && jobs.size()<128){jobs.push_back(std::move(queued.front()));queued.pop_front();}}
            try{dispatch(jobs);}catch(const std::exception& e){for(auto& job:jobs){job.published=false;job.error=e.what();}}
            {std::lock_guard lock(mutex);for(auto& job:jobs){
                if(job.backpressure && job.error.empty() && job.match==match.load() && now()<job.admissionDeadline){
                    deferred.push_back(std::move(job));continue;
                }
                completed.push_back({job.connection,job.match,job.notice,job.event,job.header,
                    job.encodes,job.retainedBytes,job.wireBytes,job.published,job.backpressure,job.traffic,std::move(job.revisions),std::move(job.error)});
            }}
            // Raw history, captures and join baselines release on this worker.
            // Results crossing back to the authority contain no large owners.
        }
    }
};

FlightInput flight(const Input &i) {
    FlightInput f{};
    f.yaw = expandAxis(i.yaw);
    f.pitch = expandAxis(i.pitch);
    f.roll = expandAxis(i.roll);
    f.throttle = expandThrottle(i.throttle);
    f.canonical = true;
    f.fieldOfView = -1;
    f.manualOverride = i.buttons & ManualOverride;
    f.forward = i.buttons & Forward;
    f.backward = i.buttons & Backward;
    f.left = i.buttons & Left;
    f.right = i.buttons & Right;
    f.up = i.buttons & Up;
    f.down = i.buttons & Down;
    f.boost = i.buttons & Boost;
    f.fire = i.buttons & Fire;
    f.missile = i.buttons & Missile;
    f.chaff = i.buttons & Chaff;
    f.deploy = i.buttons & Deploy;
    f.active = i.buttons & Active;
    f.throttleChanged = i.buttons & ThrottleChanged;
    f.matchSpeed = i.buttons & MatchSpeed;
    return f;
}
uint32_t inputButtons(const FlightInput &f) {
    return (f.forward ? Forward : 0) | (f.backward ? Backward : 0) | (f.left ? Left : 0) | (f.right ? Right : 0) |
                (f.up ? Up : 0) | (f.down ? Down : 0) | (f.boost ? Boost : 0) | (f.fire ? Fire : 0) |
                (f.missile ? Missile : 0) | (f.chaff ? Chaff : 0) | (f.deploy ? Deploy : 0) | (f.active ? Active : 0) |
                (f.throttleChanged ? ThrottleChanged : 0) | (f.matchSpeed ? MatchSpeed : 0) |
                (f.manualOverride ? ManualOverride : 0);
}
Input input(const FlightInput &f, uint64_t number, Tick tick) {
    Input i{};
    i.number = uint32_t(number);
    i.tick = uint32_t(tick);
    i.yaw = compactAxis(f.yaw);
    i.pitch = compactAxis(f.pitch);
    i.roll = compactAxis(f.roll);
    i.throttle = compactThrottle(f.throttle);
    i.buttons = inputButtons(f);
    return i;
}
Server::Server(Game &game, SessionOptions options, const Digest &content, uint16_t port, bool lobby,std::string listingName,std::string serverName,bool advertise)
    : game_(game), session_(session(game.sessionOptions())), content_(content),
      listingName_(listingName.empty()?sessionName(options):std::move(listingName)),serverName_(std::move(serverName)),
      transport_(listen(port)), epoch_(now()),publisher_(std::make_unique<Publisher>(*transport_)) {
    if (game.role() != GameRole::Authority)
        throw std::logic_error("Server requires authority");
    do{Crypto::random({reinterpret_cast<uint8_t*>(instance_.data()),sizeof(instance_)});}while(!validInstance(instance_));
    if(options.story.size()>maxBriefingBytes || options.story.find('\0')!=std::string::npos)throw std::runtime_error("Invalid mission briefing text");
    if(advertise)discovery_=std::make_unique<LanDiscovery>(LanDiscovery::Role::Server,port);
    lobby_ = lobby;
    if (lobby_)
        game_.core()->SetMissionStage(STAGE_NOTSTARTED);
    if(lobby_ && game_.core()->GetMissionParams()->bAutoRestart)
        restartCountdown_=int64_t(game_.core()->GetMissionParams()->fStartCountdown*tickRate);
}
Server::~Server()=default;
size_t Server::occupants()const {return clients()+dormant_.size();}
std::array<uint64_t,2> Server::reserveCreator(std::string name){
    if(initialClaimed_ || occupants() || name.empty())throw std::logic_error("Mission already claimed");
    creatorName_=std::move(name);Crypto::random({reinterpret_cast<uint8_t*>(creatorToken_.data()),sizeof(creatorToken_)});return creatorToken_;
}
ServerInfo Server::listing(std::string_view player)const{
    ServerInfo info{};info.session=session_;info.instance=instance_;bool found=false;
    for(const auto& [_,p]:peers_)if(p.pilot>=0){++info.players;if(p.side==noTeam)++info.noTeam;
        if(!player.empty() && !Allegiance::PlayerPaneNameCompare(p.name,player))found=true;}
    for(const auto& [_,p]:dormant_){++info.players;if(p.side==noTeam)++info.noTeam;}
    info.flags=lobby_?(countdown_>=0?ServerStarting:ServerWaiting):ServerRunning;
    if(found && !paused_)info.flags|=ServerPlayerFound;
    if(lockedLobby_)info.flags|=ServerLocked;
    if(!paused_ && !creatorToken_[0] && !creatorToken_[1] && game_.core()->GetMissionStage()!=STAGE_OVER && !lockedLobby_ &&
        (lobby_ || session_.rules.allowJoiners) && game_.core()->GetShips()->n()<shipIdentityCapacity)info.flags|=ServerJoinable;
    info.seconds=lobby_?(countdown_>=0?-countdown_:0):std::max(0,int(game_.state().time));
    text(info.name,listingName_);text(info.server,serverName_);text(info.core,game_.core()->GetMissionParams()->szIGCStaticFile);return info;
}

void Server::pause(bool paused){
    if(paused_==paused)return;
    paused_=paused;rosterDirty_=true;
    // Like RestartPausedMissions, unpausing makes a completed restartable
    // mission eligible again. The normal end-of-round path prepares its lobby.
}
Server::Peer* Server::pilotPeer(int id) {
    for(auto& [_,p]:peers_)if(p.pilot==id)return &p;
    return nullptr;
}
Allegiance::LobbyJoinFailure Server::positionFailure(const Peer& p,int side)const {
    using Failure=Allegiance::LobbyJoinFailure;
    if(side!=noTeam && (side<0 || side>=session_.rules.teams))return Failure::SideGone;
    // Leaving a side is CS_QUIT_SIDE, not C_POSITIONREQ: self-departure is
    // allowed with defections off and locked sides. Preserve lastSide so a
    // subsequent position request can only rejoin the original faction.
    if(side==noTeam)return Failure::None;
    auto stage=game_.core()->GetMissionStage();
    if(stage==STAGE_STARTED && !session_.rules.allowDefections &&
       (p.side!=noTeam || (p.lastSide!=noTeam && p.lastSide!=side)))return Failure::NoDefections;
    if(lockedSides_)return Failure::SidesLocked;
    auto* target=game_.core()->GetSide(SideID(side));
    if(!target || !target->GetActiveF())return Failure::SideGone;
    if(paused_ || stage==STAGE_OVER || game_.state().complete)return Failure::ServerPaused;
    if(p.bannedSides&(1u<<side))return Failure::Banned;
    bool restart=false;for(auto* l=target->GetStations()->first();l;l=l->next())if(l->data()->GetStationType()->HasCapability(c_sabmRestart)){restart=true;break;}
    if(!restart)return Failure::NoBase;
    if(stage!=STAGE_NOTSTARTED && session_.rules.maxImbalance!=32767){
        std::array<int,6> counts{};
        // FedSrv counts non-ghost pilots and clamps each unranked rank to one.
        // The two admission modes therefore share this event-local count.
        auto count=[&](const Peer& peer){if(peer.pilot>=0 && peer.side>=0 && !peer.expelled)if(auto* ship=game_.core()->GetShip(ShipID(peer.pilot));ship && !ship->IsGhost())++counts[peer.side];};
        for(const auto& [_,peer]:peers_)count(peer);
        for(const auto& [_,peer]:dormant_)count(peer);
        int minimum=shipIdentityCapacity;
        for(int i=0;i<session_.rules.teams;++i)if(game_.core()->GetSide(SideID(i))->GetActiveF())minimum=std::min(minimum,counts[i]);
        // FedSrv admits two extra new pilots beyond the count imbalance.
        // Native pilots have no authenticated Club rank and are unranked.
        if(!Allegiance::UnrankedLobbyPositionAvailable(false,session_.rules.maxImbalance,counts[side],minimum))return Failure::TeamBalance;
    }
    return Failure::None;
}
bool Server::canRequest(const Peer& peer,int side)const{return positionFailure(peer,side)==Allegiance::LobbyJoinFailure::None;}
bool Server::moveTeam(Peer& p,int side) {
    int previous=p.side;
    if(!game_.moveNetworkPilotToTeam(p.pilot,side))return false;
    p.side=side;p.requested=noTeamRequest;
    if(side>=0)p.lastSide=side;
    p.inputs.clear();p.held={};p.held.active=false;cancelCountdown();rosterDirty_=true;
    if(p.pilot==host_ && previous!=side){
        host_=game_.teamLeader(previous);
        if(host_<0)for(int team=0;team<session_.rules.teams;++team)if((host_=game_.teamLeader(team))>=0)break;
    }
    for(auto& [connection,peer]:peers_)if(&peer==&p){sendBriefing(connection,p);break;}
    return true;
}
bool Server::requestTeam(Peer& p,int side,uint32_t* details) {
    auto failure=positionFailure(p,side);
    if(failure!=Allegiance::LobbyJoinFailure::None){if(details)*details=uint32_t(failure);return false;}
    if(p.side==side){p.requested=noTeamRequest;rosterDirty_=true;return true;}
    if(p.side>=0 && !moveTeam(p,noTeam))return false;
    if(side==noTeam){p.requested=noTeamRequest;rosterDirty_=true;return true;}
    if(game_.teamLeader(side)<0 || (autoAccept_&(1u<<side)))return moveTeam(p,side);
    p.requested=side;rosterDirty_=true;return true;
}
void Server::cancelCountdown() {
    countdown_=-1;
    if(lobby_ && game_.core()->GetMissionStage()!=STAGE_OVER)game_.core()->SetMissionStage(STAGE_NOTSTARTED);
}
void Server::automaticCountdown(){
    auto* mission=game_.core();const auto* params=mission->GetMissionParams();
    auto stage=mission->GetMissionStage();if(stage==STAGE_OVER)return;
    if(params->bAutoRestart && restartCountdown_>=0){
        auto previous=restartCountdown_;if(restartCountdown_>0)--restartCountdown_;
        if(stage!=STAGE_NOTSTARTED)return;
        if(!params->bAllowEmptyTeams && restartCountdown_<=int64_t(c_fMissionBriefingCountdown*tickRate))
            forceReady_=(1u<<session_.rules.teams)-1;
        bool needsReady=!params->bAllowEmptyTeams && (restartCountdown_<=int64_t(c_fMissionBriefingCountdown*tickRate) ||
            (restartCountdown_<=int64_t(2*c_fMissionBriefingCountdown*tickRate) && previous>int64_t(2*c_fMissionBriefingCountdown*tickRate)));
        switch(Allegiance::MissionAutomaticRestart(double(restartCountdown_)/tickRate,double(previous)/tickRate,
                c_fMissionBriefingCountdown,params->bAllowEmptyTeams,needsReady && allReady())){
        case Allegiance::AutomaticRestart::Briefing:
            countdown_=int(restartCountdown_)+1;mission->SetMissionStage(STAGE_STARTING);rosterDirty_=true;break;
        case Allegiance::AutomaticRestart::Cancel:
            countdown_=-1;restartCountdown_=-1;mission->SetMissionStage(STAGE_OVER);rosterDirty_=true;
            mission->GetIgcSite()->SendChat(nullptr,CHAT_EVERYONE,NA,NA,Allegiance::MissionRestartCanceledMessage);break;
        case Allegiance::AutomaticRestart::Warn:{
            const auto text="Warning: Game will be canceled if all teams are not ready in "+std::to_string(int(2*c_fMissionBriefingCountdown))+" seconds.";
            mission->GetIgcSite()->SendChat(nullptr,CHAT_EVERYONE,NA,NA,text.c_str());break;}
        default:break;
        }
    }else if(params->bAutoStart && stage==STAGE_NOTSTARTED && allReady()){
        countdown_=int(params->fStartCountdown*tickRate);mission->SetMissionStage(STAGE_STARTING);rosterDirty_=true;
    }
}
bool Server::randomizeTeams() {
    // FM_C_RANDOMIZE_TEAMS means Flush unless Enforce Balance is selected.
    // Preserve commanders, side locks and AFK state; clear the Boot bans.
    const bool balance=session_.rules.maxImbalance==32766;
    autoAccept_=0;
    struct Member {Peer* peer;bool connected;};
    std::unordered_map<int,Member> members;members.reserve(peers_.size()+dormant_.size());
    for(auto& [_,peer]:peers_)if(peer.pilot>=0)members.emplace(peer.pilot,Member{&peer,true});
    for(auto& [_,peer]:dormant_)members.emplace(peer.pilot,Member{&peer,false});
    // Flush uses mission ship-list order. Players already in No Team precede
    // newly flushed players in its append-only list; preserve that order for
    // equal-rank assignment, rather than sorting by pilot ID.
    for(auto* link=game_.core()->GetShips()->first();link;link=link->next()){
        auto member=members.find(link->data()->GetObjectID());if(member==members.end())continue;
        auto* pilot=member->second.peer;
        pilot->bannedSides=0;
        if(pilot->side>=0 && game_.teamLeader(pilot->side)!=pilot->pilot){
            if(!moveTeam(*pilot,noTeam))return false;
            pilot->lastSide=noTeam;
            game_.lobbyTeamResetNotice(pilot->pilot,balance);
        }
        if(balance)pilot->requested=noTeamRequest;
    }
    if(balance){
        // All current native players are unranked. Stable rank-one ties pick
        // the first lowest-count eligible side, as PickNewSide does. Cache
        // counts instead of repeatedly traversing the entire player roster.
        std::array<int,6> counts{};
        for(const auto& [_,member]:members)if(member.peer->side>=0)++counts[member.peer->side];
        for(auto* link=game_.core()->GetSide(SideID(noTeam))->GetShips()->first();link;){
            auto* next=link->next();auto member=members.find(link->data()->GetObjectID());
            if(member==members.end() || !member->second.connected || !member->second.peer->ready){link=next;continue;}
            auto* pilot=member->second.peer;
            int best=-1;
            for(int side=0;side<session_.rules.teams;++side)
                if(canRequest(*pilot,side) && (best<0 || counts[side]<counts[best]))best=side;
            if(best<0)break;
            if(!moveTeam(*pilot,best))return false;
            ++counts[best];link=next;
        }
    }
    rosterDirty_=true;return true;
}
void Server::expel(Peer& p,bool owner) {
    // Drain after this receive batch, outside any action's peer references.
    // Later queued messages from this connection cannot undo the removal.
    if(p.expelled)return;
    for(auto& [connection,peer]:peers_)if(&peer==&p){p.expelled=true;p.ownerBoot=owner;pendingExpulsions_.push_back(connection);return;}
}
void Server::setAutoAccept(int side,bool enabled) {
    if(enabled)autoAccept_|=1u<<side;else autoAccept_&=~(1u<<side);
    // FedSrv clears every pending request to this side, including requests
    // which no longer pass admission, when auto-accept is enabled.
    if(enabled)for(auto& [_,request]:peers_)if(request.pilot>=0 && !request.expelled && request.requested==side){
        request.requested=noTeamRequest;
        auto failure=positionFailure(request,side);
        if(failure==Allegiance::LobbyJoinFailure::None){if(!moveTeam(request,side))failure=Allegiance::LobbyJoinFailure::Other;}
        if(failure!=Allegiance::LobbyJoinFailure::None)game_.lobbyRequestRejected(request.pilot,failure);
    }
    rosterDirty_=true;
}
bool Server::chatAction(Peer& p,const Action& action) {
    if(action.args[0]<0 || action.args[0]>=int(ChatChannel::Count))return false;
    bool leader=p.side>=0 && game_.teamLeader(p.side)==p.pilot;
    auto command=Allegiance::ParseMissionChat(action.text,{p.pilot==host_,leader,p.side>=0,
        !lobby_ && game_.core()->GetMissionStage()==STAGE_STARTED && !game_.state().complete});
    if(command.kind==Allegiance::MissionChatKind::Ban){
        for(auto* l=game_.core()->GetShips()->first();l;l=l->next())if(auto* ship=l->data();ship->GetPilotType()>=c_ptPlayer && ship->GetSide()->GetObjectID()<0 && Allegiance::ChatNameEqual(command.target,ship->GetName())){
            if(auto* target=pilotPeer(ship->GetObjectID()))expel(*target,true);break;
        }
        return true;
    }
    auto forwarded=action;
    if(command.kind==Allegiance::MissionChatKind::AutoBalance){forwarded.args[0]=int(ChatChannel::Everyone);forwarded.args[1]=-1;}
    if(!game_.authorityAction(p.pilot,forwarded,nullptr,false))return false;
    if(command.kind==Allegiance::MissionChatKind::AutoAccept)setAutoAccept(p.side,!(autoAccept_&(1u<<p.side)));
    if(command.kind==Allegiance::MissionChatKind::AutoBalance && command.imbalance>=0 && command.imbalance!=session_.rules.maxImbalance){
        game_.teamImbalance(command.imbalance);session_.rules.maxImbalance=command.imbalance;
        publishMissionPolicy();
    }
    return true;
}
void Server::publishMissionPolicy() {
    auto policy=game_.missionPolicy(match_);
    session_.alliances=policy.alliances;session_.rules.maxImbalance=policy.imbalance;
    session_.rules.allowDefections=policy.flags&1;session_.rules.alliedSight=policy.flags&2;session_.rules.alliedRipcord=policy.flags&4;
    auto bytes=::Native::Net::message(Kind::MissionPolicy,policy,tick_);
    for(auto& [connection,recipient]:peers_)if(recipient.pilot>=0 && !recipient.expelled)send(connection,bytes,Lane::Control);
}
bool Server::lobbyAction(Peer& p,const Action& a) {
    auto kind=LobbyAction(a.args[0]);int value=a.args[1],target=a.args[2];
    bool leader=p.side>=0 && game_.teamLeader(p.side)==p.pilot;
    switch(kind){
    case LobbyAction::TeamActive:{
        if(p.pilot!=host_ || !lobby_ || (value!=0 && value!=1))return false;
        auto previous=game_.missionPolicy();
        if(!game_.setTeamActive(target,value!=0))return false;
        session_.teamPolicy=::Native::Net::session(game_.sessionOptions()).teamPolicy;
        if(previous!=game_.missionPolicy()){
            for(auto& [_,member]:peers_)if(member.pilot!=host_ && member.ready && member.side>=0 && game_.teamLeader(member.side)==member.pilot){forceReady_&=~(1u<<member.side);member.ready=false;}
            for(auto& [_,member]:dormant_)if(member.pilot!=host_ && member.ready && member.side>=0 && game_.teamLeader(member.side)==member.pilot){forceReady_&=~(1u<<member.side);member.ready=false;}
            publishMissionPolicy();
        }
        break;}
    case LobbyAction::Alliance:{
        if(p.pilot!=host_ || !lobby_ || (a.args[3]!=0 && a.args[3]!=1))return false;
        auto previous=game_.missionPolicy();
        if(!game_.changeAlliance(value,target,a.args[3]!=0))return false;
        if(previous==game_.missionPolicy())return true;
        // CHANGE_ALLIANCES notifies TeamScreen, which withdraws ready
        // non-owner commanders' force-ready and personal ready consent.
        auto withdraw=[&](Peer& member){if(member.pilot!=host_ && member.ready && member.side>=0 && game_.teamLeader(member.side)==member.pilot){
            forceReady_&=~(1u<<member.side);member.ready=false;
        }};
        for(auto& [_,member]:peers_)withdraw(member);
        for(auto& [_,member]:dormant_)withdraw(member);
        cancelCountdown();publishMissionPolicy();break;}
    case LobbyAction::Randomize:
        if(p.pilot!=host_ || !lobby_ || game_.core()->GetMissionStage()!=STAGE_NOTSTARTED)return false;
        return randomizeTeams();
    case LobbyAction::TeamSettings:{
        if(!leader || !lobby_ || session_.rules.lockTeamSettings)return false;
        bool changed=game_.core()->GetSide(SideID(p.side))->GetCivilization()->GetObjectID()!=value;
        if(!game_.setTeamSettings(p.side,value,a.text))return false;
        auto current=Net::session(game_.sessionOptions());
        session_.civilizations[p.side]=current.civilizations[p.side];Net::text(session_.teamNames[p.side],current.teamNames[p.side]);
        if(changed){
            // WinTrek withdraws a ready opposing commander's consent when a
            // faction changes. Personal AFK and team force-ready are distinct.
            for(auto& [_,other]:peers_)if(other.side>=0 && other.side!=p.side && game_.teamLeader(other.side)==other.pilot &&
                (forceReady_&(1u<<other.side))){forceReady_&=~(1u<<other.side);other.ready=false;}
            cancelCountdown();
        }
        break;}
    case LobbyAction::LockLobby:case LobbyAction::LockSides:
        if(p.pilot!=host_ || (kind==LobbyAction::LockLobby && game_.core()->GetMissionParams()->bLockGameOpen))return false;
        (kind==LobbyAction::LockLobby?lockedLobby_:lockedSides_)=value!=0;break;
    case LobbyAction::ForceReady:case LobbyAction::AutoAccept:{
        if(!leader || (kind==LobbyAction::AutoAccept && game_.core()->GetMissionParams()->bLockGameOpen))return false;
        if(kind==LobbyAction::AutoAccept)setAutoAccept(p.side,value!=0);
        else if(value)forceReady_|=1u<<p.side;else forceReady_&=~(1u<<p.side);
        break;}
    case LobbyAction::Accept:case LobbyAction::Reject:case LobbyAction::Boot:case LobbyAction::FlightBoot:{
        auto* request=pilotPeer(target);
        if(!leader || !request || request==&p || request->expelled ||
           (kind==LobbyAction::FlightBoot && value!=p.side))return false;
        if(kind==LobbyAction::Accept){
            if(request->requested!=p.side || !canRequest(*request,p.side))return false;
            return moveTeam(*request,p.side);
        }
        auto effect=kind==LobbyAction::FlightBoot?Allegiance::ConfirmedPlayerBoot(request->requested==p.side,session_.rules.allowDefections):Allegiance::PlayerBootEffect::LeaveTeam;
        if(effect==Allegiance::PlayerBootEffect::RejectRequest){request->requested=noTeamRequest;break;}
        if(request->side==p.side){
            if(session_.rules.lockTeamSettings)return false;
            if(effect==Allegiance::PlayerBootEffect::LeaveMission){game_.lobbyBootNotice(target,p.side,true);expel(*request);}
            else {
                if(!moveTeam(*request,noTeam))return false;
                request->bannedSides|=1u<<p.side;
                uint32_t legal=(1u<<session_.rules.teams)-1;
                bool allBanned=(request->bannedSides&legal)==legal;
                game_.lobbyBootNotice(target,p.side,allBanned);
                if(allBanned)expel(*request);
            }
        }else if(kind==LobbyAction::FlightBoot){return false;
        }else if(kind==LobbyAction::Boot || request->requested!=p.side)return false;
        else {request->requested=noTeamRequest;game_.lobbyRequestRejected(request->pilot,Allegiance::LobbyJoinFailure::Rejected);}
        break;}
    case LobbyAction::CancelRequest:p.requested=noTeamRequest;break;
    default:return false;
    }
    rosterDirty_=true;if(!allReady())cancelCountdown();return true;
}
uint32_t Server::dormantSides()const {
    uint32_t sides=0;
    for(const auto& [_,peer]:dormant_)if(peer.pilot>=0 && peer.side>=0 && !peer.expelled)sides|=1u<<peer.side;
    return sides;
}
bool Server::allReady()const {
    if(dormantSides())return false;
    std::array<Allegiance::LobbyTeamState,6> teams{};
    for(int side=0;side<session_.rules.teams;++side)teams[side].active=game_.core()->GetSide(SideID(side))->GetActiveF();
    for(const auto& [_,p]:peers_)if(p.pilot>=0 && p.side>=0){
        ++teams[p.side].players;
        if(!p.ready && !(forceReady_&(1u<<p.side)))teams[p.side].ready=false;
    }
    // Native empty-team matches support single-player versus AI. There is no
    // additional pilot cap beyond the original signed-short identity space.
    const auto* params=game_.core()->GetMissionParams();
    return Allegiance::LobbyStart(session_.rules.teams,params->bAllowEmptyTeams?0:params->nMinPlayersPerTeam,shipIdentityCapacity,session_.rules.maxImbalance,params->bAllowEmptyTeams,
        [&](int side){return teams[side];}).CanStart();
}
void Server::captureRoster() {
    if(!rosterDirty_ && roster_)return;
    Frame f;f.header.kind=Kind::Snapshot;readySides_=((1u<<session_.rules.teams)-1)&~dormantSides();
    std::array<int,6> players{};
    for(const auto& [_,p]:peers_)if(p.pilot>=0){
        if(p.side>=0)++players[p.side];
        if(p.side>=0 && !p.ready && !(forceReady_&(1u<<p.side)))readySides_&=~(1u<<p.side);
        Member m;m.id=p.pilot;m.side=p.side;m.requested=p.requested;
        m.flags=(p.ready?1:0)|2|((p.side>=0 && game_.teamLeader(p.side)==p.pilot)?4:0);
        auto* ship=game_.core()->GetShip(ShipID(p.pilot));Net::text(m.name,ship?ship->GetName():p.name);
        if(ship){m.kills=ship->GetKills();m.deaths=ship->GetDeaths();m.ejections=ship->GetEjections();}
        f.members.push_back(m);
    }
    const auto* params=game_.core()->GetMissionParams();
    if(!params->bAllowEmptyTeams)for(int side=0;side<session_.rules.teams;++side)
        if(game_.core()->GetSide(SideID(side))->GetActiveF() && players[side]<params->nMinPlayersPerTeam)readySides_&=~(1u<<side);
    std::sort(f.members.begin(),f.members.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    if(!roster_ || f.members!=roster_->members){f.header.tick=++rosterRevision_;roster_=std::make_shared<const Frame>(std::move(f));}
    rosterDirty_=false;
}
void Server::lobbyStatus(Frame& f)const {
    f.status.match=match_;f.status.stage=game_.core()->GetMissionStage();f.status.host=host_;
    f.status.countdown=countdown_<0?-1:(countdown_+tickRate-1)/tickRate;
    f.status.lobbyFlags=(lockedLobby_?1u:0u)|(lockedSides_?2u:0u)|(!lobby_?4u:0u);
    const auto* params=game_.core()->GetMissionParams();
    if(params->bAutoRestart)f.status.lobbyFlags|=Allegiance::LobbyAutoRestart;
    if(paused_)f.status.lobbyFlags|=Allegiance::LobbyServerPaused;
    if(!params->bAllowRestart)f.status.lobbyFlags|=Allegiance::LobbyNoRestart;
    if(params->bObjectModelCreated)f.status.lobbyFlags|=Allegiance::LobbyObjectModelCreated;
    if(params->bLockGameOpen)f.status.lobbyFlags|=Allegiance::LobbyGameOpenLocked;
    if(params->bAutoStart)f.status.lobbyFlags|=Allegiance::LobbyAutoStart;
    if(!params->bAllowEmptyTeams)f.status.lobbyFlags|=Allegiance::LobbyRequirePlayers|(uint32_t(params->nMinPlayersPerTeam)<<Allegiance::LobbyMinimumShift);
    if(lobby_ && f.status.stage==STAGE_NOTSTARTED && restartCountdown_>=0)
        f.status.countdown=int((restartCountdown_+tickRate-1)/tickRate);
    f.status.autoAccept=autoAccept_;f.status.forceReady=forceReady_;f.status.readySides=readySides_;
    auto reconnecting=dormantSides();
    if(reconnecting)f.status.lobbyFlags|=Allegiance::LobbyReconnecting|(reconnecting<<Allegiance::LobbyReconnectingShift);
}

size_t Server::readyClients() const {
    return std::count_if(peers_.begin(),peers_.end(),[](const auto& p){return p.second.pilot>=0 && p.second.snapshotAck;});
}
size_t Server::clients() const {
    size_t n = 0;
    for (auto &[_, p] : peers_)
        n += p.pilot >= 0;
    return n;
}
std::string Server::publicationDiagnostic() {
    auto worst=std::min_element(peers_.begin(),peers_.end(),[](const auto& a,const auto& b){return a.second.reportsSent<b.second.reportsSent;});
    if(worst==peers_.end())return {};
    const auto& peer=worst->second;
    return "Least-published pilot "+std::to_string(peer.pilot)+": "+std::to_string(peer.reportsSent)+
        " reports, latest tick "+std::to_string(peer.reportTick)+", backpressure "+std::to_string(peer.backpressure)+"\n"+
        transport_->diagnostic(worst->first);
}
void Server::detach(Connection connection, bool resume) {
    auto i = peers_.find(connection);
    if (i == peers_.end())
        return;
    if (i->second.pilot >= 0) {
        rosterDirty_=true;
        if (lobby_ && i->second.side>=0)
            cancelCountdown();
        if (resume) {
            game_.networkPilotConnected(i->second.pilot,false);
            auto peer = std::move(i->second);
            peer.connected = now();
            peer.inputs.clear();
            peer.held = {};
            peer.held.active = false;
            dormant_[peer.pilot] = std::move(peer);
        } else
            game_.retireNetworkPilot(i->second.pilot);
    }
    peers_.erase(i);connectionReport_.clear();
}
void Server::poll() {
    if(discovery_)discovery_->poll();
    flushPublications();
    auto incoming = transport_->receive();
    for (auto &e : transport_->events()) {
        if (e.connected) {
            Peer p;
            p.connected = now();
            p.held.active = false;
            peers_.try_emplace(e.peer, std::move(p));
        } else {
            detach(e.peer, e.retryable);
        }
    }
    for (auto &m : incoming)
        try {
            message(m);
        } catch (const std::exception &e) {
            ++metrics_.rejected;
            fprintf(stderr, "Network peer %u rejected: %s\n", m.peer, e.what());
            transport_->close(m.peer, e.what());
            detach(m.peer, false);
        }
    for(auto connection:pendingExpulsions_){
        // Preserve the original reason without claiming the unavailable ASGS
        // alternate-callsign login facility in OnQuitMission's second sentence.
        auto found=peers_.find(connection);bool owner=found!=peers_.end() && found->second.ownerBoot;
        transport_->close(connection,owner?"You have been lobby booted by the mission owner!":"You have been booted by your commander!");
        detach(connection,false);
    }
    pendingExpulsions_.clear();
    for (auto i = peers_.begin(); i != peers_.end();)
        if (i->second.pilot < 0 && now() - i->second.connected > 10000000) {
            transport_->close(i->first, "Compatibility handshake timed out");
            i = peers_.erase(i);
        } else
            ++i;
    for (auto i = dormant_.begin(); i != dormant_.end();)
        if (now() - i->second.connected > 30000000) {
            rosterDirty_=true;game_.retireNetworkPilot(i->first);
            i = dormant_.erase(i);
        } else
            ++i;
}
void Server::welcome(Connection connection, Peer &peer) {
    connectionReport_.clear();
    Welcome w{};
    w.instance=instance_;
    w.session = session_;
    text(w.server,serverName_);
    w.pilot = peer.pilot;
    w.side = peer.side;
    w.match = match_;
    w.epoch = epoch_;
    w.token = peer.token;
    w.inputAck = peer.inputAck;
    w.actionAck = peer.actionAck;
    send(connection, ::Native::Net::message(Kind::Welcome, w, tick_), Lane::Control);
    sendBriefing(connection,peer);
    publish(connection, peer, true);
}
void Server::sendBriefing(Connection connection,const Peer& peer){
    auto briefing=game_.missionBriefing(peer.side);if(!briefing)return;
    if(briefing->text.size()>maxBriefingBytes || briefing->text.find('\0')!=std::string::npos)throw std::runtime_error("Invalid mission briefing text");
    auto& message=briefingMessages_[briefing->generateCivilization?peer.side:6];
    if(message.source!=briefing || message.match!=match_){
        message.source=briefing;message.match=match_;message.bytes.clear();
        append(message.bytes,Header{magic,Kind::Briefing,match_});
        append(message.bytes,Briefing{briefing->generateCivilization?1u<<peer.side:(1u<<session_.rules.teams)-1,unsigned(briefing->generateCivilization),uint32_t(briefing->text.size()),0});
        auto bytes=reinterpret_cast<const std::byte*>(briefing->text.data());message.bytes.insert(message.bytes.end(),bytes,bytes+briefing->text.size());++metrics_.briefingEncodes;
    }
    send(connection,message.bytes,Lane::Control);
}
bool Server::configure(Session s,bool fromClient) {
    if(fromClient && (game_.core()->GetMissionStage()!=STAGE_NOTSTARTED || game_.core()->GetMissionParams()->bObjectModelCreated))return false;
    if(!terminated(s.coreFile) || !Allegiance::CoreFileEqual(s.coreFile,session_.coreFile))return false;
    if (paused_ || dormantSides() || s.scenario != int(Scenario::Conquest))
        return false;
    auto options = Net::options(s, 0, "Server seat");
    // Active sides are controlled by the original checkbox operation, never
    // by unvalidated client configuration. A new round restores all sides.
    const auto& current=game_.sessionOptions();
    options.inactiveTeams=fromClient?current.inactiveTeams&((1u<<std::clamp(options.rules.teams,0,6))-1):0;
    options.requireTeamPlayers=current.requireTeamPlayers || (!fromClient && current.inactiveTeams!=0);
    options.story=game_.sessionOptions().story;
    if(Allegiance::MissionTeamReductionError(session_.rules.teams,options.rules.teams,[&](int side){return game_.core()->GetSide(SideID(side))->GetActiveF();}))
        return false;
    std::vector<Connection> participants;
    Connection owner{};
    for (auto &[c, p] : peers_)
        if (p.pilot >= 0) {
            if (p.side >= s.rules.teams)
                return false;
            participants.push_back(c);
            if (p.pilot == host_)
                owner = c;
        }
    if (!owner)
        return false;
    std::sort(participants.begin(), participants.end(), [&](auto a, auto b) {
        if (a == owner || b == owner)
            return a == owner && b != owner;
        return peers_.at(a).pilot < peers_.at(b).pilot;
    });
    options.team = std::max(0,peers_.at(owner).side);
    if(peers_.at(owner).side==noTeam)while(options.team<options.rules.teams && (options.inactiveTeams&(1u<<options.team)))++options.team;
    if (!game_.configurationError(options,fromClient).empty())return false;
    Allegiance::MissionLifecyclePolicy lifecycle(*game_.core()->GetMissionParams());
    if(!fromClient && (current.requireTeamPlayers || current.inactiveTeams))lifecycle.allowEmpty=false;
    game_.resetAuthority(options);
    auto params=*game_.core()->GetMissionParams();lifecycle.apply(params);game_.core()->SetMissionParams(&params);
    session_ = session(game_.sessionOptions());rosterDirty_=true;forceReady_=0;autoAccept_=0x3f;lockedLobby_=lockedSides_=false;
    tick_ = 0;
    epoch_ = now();
    ++match_;
    publisher_->match.store(match_);
    lobby_ = true;
    roundEndingPublished_=false;
    countdown_ = -1;
    restartCountdown_=params.bAutoRestart?int64_t((fromClient?params.fStartCountdown:params.fRestartCountdown)*tickRate):-1;
    initialClaimed_ = true;
    auto resetPeer=[](Peer& p){
        p.inputs.clear();
        p.ready=false;p.requested=noTeamRequest;
        p.held = {};
        p.held.active = false;
        p.snapshots.clear();
        p.investments.clear();
        p.sentArenaCheckpoints.clear();
        p.effects.clear();
        p.noticeAck = p.eventAck = 0;
        p.lastPublication = 0;
        p.snapshotAck = 0;
    };
    bool first = true;
    for (auto c : participants) {
        auto &p = peers_.at(c);
        p.pilot = game_.joinNetworkPilot(p.name, p.side, first);
        if (first) {
            host_ = p.pilot;
            first = false;
        }
        resetPeer(p);
    }
    // No Team visitors have no say in readiness or settings. Preserve their
    // authenticated lease when a world is replaced, just like connected No
    // Team peers; stale world baselines/requests cannot survive the new match.
    auto visitors=std::move(dormant_);dormant_.clear();
    for(auto& [_,p]:visitors){
        p.pilot=game_.joinNetworkPilot(p.name,noTeam,false);
        resetPeer(p);game_.networkPilotConnected(p.pilot,false);
        dormant_.emplace(p.pilot,std::move(p));
    }
    game_.core()->SetMissionStage(STAGE_NOTSTARTED);
    for (auto c : participants)
        welcome(c, peers_.at(c));
    return true;
}
void Server::message(const Message &m) {
    metrics_.bytesReceived += m.data.size();
    if (m.data.size() > maxPacket)
        throw std::runtime_error("Message limit");
    auto found = peers_.find(m.peer);
    if (found == peers_.end())
        return;
    auto &peer = found->second;
    if(peer.expelled)return;
    Reader r(m.data);
    auto prefix = r.take<MessagePrefix>();
    r = Reader(m.data);
    Header h;
    uint64_t inputMatch = 0;
    if (prefix.kind == Kind::Input) {
        auto bundle = r.take<InputHeader>();
        h.signature = bundle.signature;
        h.kind = bundle.kind;
        h.ack = expandCounter(bundle.ack, publication_);
        inputMatch = bundle.match;
    } else
        h = r.take<Header>();
    if (h.signature != magic)
        throw std::runtime_error("Foreign game record");
    std::vector<std::byte> unpacked;
    unpackMessage(h, r, unpacked);
    if(h.kind==Kind::Browse || h.kind==Kind::FindPlayer){
        if(peer.pilot>=0 || peer.browsing)throw std::runtime_error("Browsing requires a separate connection");
        auto query=h.kind==Kind::FindPlayer?r.take<PlayerQuery>():PlayerQuery{r.take<BrowseQuery>()};
        if(!r.rest().empty() || !terminated(query.player))throw std::runtime_error("Invalid browse request");
        if(query.identity.build!=buildDigest())throw std::runtime_error("Client and server builds differ");
        if(query.identity.content!=content_)throw std::runtime_error("Client and server game data differ");
        peer.browsing=true;
        if(directory_){auto bytes=directory_(query.player);send(m.peer,bytes,Lane::Control);}
        else send(m.peer,::Native::Net::message(Kind::ServerInfo,listing(query.player),tick_),Lane::Control);
        return;
    }
    if(h.kind==Kind::CreateGame){
        if(peer.pilot>=0 || peer.browsing)throw std::runtime_error("Creation requires a separate connection");
        auto query=r.take<CreateGameRequest>();
        if(!r.rest().empty() || !terminated(query.name) || !terminated(query.pilot) || !terminated(query.core) || !query.pilot[0] ||
            (!query.request[0] && !query.request[1]) || std::any_of(query.reserved.begin(),query.reserved.end(),[](auto n){return n!=0;}))
            throw std::runtime_error("Invalid create-game request");
        if(query.identity.build!=buildDigest())throw std::runtime_error("Client and server builds differ");
        if(query.identity.content!=content_)throw std::runtime_error("Client and server game data differ");
        peer.browsing=true;GameCreated result{};
        if(paused_)text(result.error,Allegiance::MissionServerPausedMessage);
        else if(createGame_)result=createGame_(query);
        else text(result.error,"This server does not accept game creation requests.");
        send(m.peer,::Native::Net::message(Kind::GameCreated,result,tick_),Lane::Control);return;
    }
    if (h.kind == Kind::Hello) {
        if(peer.browsing)throw std::runtime_error("Joining requires a separate connection");
        if (peer.pilot >= 0)
            return;
        auto hello = r.take<Hello>();
        if (!r.rest().empty() || !terminated(hello.name) || hello.reserved)
            throw std::runtime_error("Invalid hello");
        if (hello.build != buildDigest())
            throw std::runtime_error("Client and server builds differ");
        if (hello.content != content_)
            throw std::runtime_error("Client and server game data differ");
        if(validInstance(hello.instance) && hello.instance!=instance_)
            throw std::runtime_error("The selected game is no longer available.");
        const bool creator=(creatorToken_[0] || creatorToken_[1]) && hello.token==creatorToken_ && creatorName_==hello.name;
        if((creatorToken_[0] || creatorToken_[1]) && !creator)throw std::runtime_error("Waiting for the mission creator");
        bool resume = !creator && (hello.token[0] || hello.token[1]);
        // No Team is a valid resumed membership, not an initial requested
        // faction. Resume still requires the retained authenticated token;
        // the authority restores membership/history from that peer record.
        if (hello.side < (resume?noTeam:0) || hello.side >= game_.core()->GetSides()->n() || !hello.name[0])
            throw std::runtime_error("Invalid pilot or team");
        if (resume) {
            auto match = std::find_if(dormant_.begin(), dormant_.end(),
                                      [&](auto &item) { return item.second.token == hello.token; });
            if (match == dormant_.end()) {
                auto active = std::find_if(peers_.begin(), peers_.end(), [&](auto &item) {
                    return item.first != m.peer && item.second.pilot >= 0 && item.second.token == hello.token;
                });
                if (active != peers_.end()) {
                    auto old = active->first;
                    transport_->close(old, "Session resumed on another connection");
                    detach(old, true);
                    match = std::find_if(dormant_.begin(), dormant_.end(),
                                         [&](auto &item) { return item.second.token == hello.token; });
                }
            }
            if (match == dormant_.end())
                throw std::runtime_error("Resume token expired");
            peer = std::move(match->second);
            dormant_.erase(match);
            peer.snapshots.clear();
            peer.sentArenaCheckpoints.clear();
            peer.effects.clear();
            peer.snapshotAck = 0;
            peer.connected = now();
            peer.lastInput = 0;
            peer.held.active = false;
            peer.inputs.clear();
            game_.networkPilotConnected(peer.pilot,true);
        } else {
            if(paused_)throw std::runtime_error(Allegiance::MissionServerPausedMessage);
            if(lockedLobby_ || (!lobby_ && !session_.rules.allowJoiners))throw std::runtime_error(lockedLobby_?"Lobby is locked":"This game does not allow late joiners");
            peer.name = hello.name;peer.side=noTeam;
            bool direct=canRequest(peer,hello.side) && (game_.teamLeader(hello.side)<0 || (autoAccept_&(1u<<hello.side)));
            peer.pilot=game_.joinNetworkPilot(hello.name,direct?hello.side:noTeam,!initialClaimed_);
            if(direct){peer.side=hello.side;peer.lastSide=hello.side;}
            else if(peer.pilot>=0 && canRequest(peer,hello.side))requestTeam(peer,hello.side);
            if (peer.pilot < 0)
                throw std::runtime_error("Team cannot accept a pilot");
            initialClaimed_ = true;
            if (lobby_ && peer.side>=0)
                cancelCountdown();
            if (host_ < 0)
                host_ = peer.pilot;
            Crypto::random({reinterpret_cast<uint8_t*>(peer.token.data()),sizeof(peer.token)});
            if(creator){creatorToken_={};creatorName_.clear();}
        }
        rosterDirty_=true;welcome(m.peer, peer);
        return;
    }
    if (peer.pilot < 0)
        throw std::runtime_error("Handshake required");
    if (h.kind == Kind::Input && inputMatch != uint32_t(match_))
        return;
    if (h.ack && peer.effects.contains(h.ack)) {
        peer.snapshotAck = std::max(peer.snapshotAck, h.ack);
        if (auto i = peer.effects.find(h.ack); i != peer.effects.end()) {
            peer.noticeAck = std::max(peer.noticeAck, i->second.first);
            peer.eventAck = std::max(peer.eventAck, i->second.second);
        }
    }
    // An acknowledged arena report supersedes the one-off join baseline.
    // Retaining a full bootstrap per pilot otherwise costs hundreds of MiB.
    if(!lobby_ && !peer.snapshots.empty() && peer.snapshotAck>peer.snapshots.rbegin()->first)publisher_->retire(std::exchange(peer.snapshots,{}));
    if (h.kind == Kind::Input) {
        if (r.rest().size() % sizeof(Input) || r.rest().size() > 3 * sizeof(Input))
            throw std::runtime_error("Invalid input bundle");
        bool hasInputs = !r.rest().empty();
        while (!r.rest().empty()) {
            auto i = r.take<Input>();
            auto number = expandCounter(i.number, peer.inputAck);
            auto tick = expandCounter(i.tick, tick_);
            if (!valid(i) || tick > tick_ + tickRate / 2 || number > peer.inputAck + inputHistory)
                throw std::runtime_error("Invalid input bounds");
            if (number <= peer.inputAck)
                continue;
            auto slot = std::lower_bound(peer.inputs.begin(), peer.inputs.end(), number,
                [&](const auto &queued, uint64_t n) { return expandCounter(queued.number, peer.inputAck) < n; });
            if (slot == peer.inputs.end() || slot->number != i.number)
                peer.inputs.insert(slot, i);
        }
        if (hasInputs)
            peer.lastInput = now(); // Receipts alone must not defeat the lost-input watchdog.
        return;
    }
    if (h.kind == Kind::Configure) {
        auto c = r.take<Configuration>();
        if (!r.rest().empty())
            throw std::runtime_error("Invalid configuration record");
        if (auto i = peer.receipts.find(c.number); i != peer.receipts.end()) {
            send(m.peer, ::Native::Net::message(Kind::Receipt, i->second, tick_), Lane::Control);
            return;
        }
        if (c.number != peer.actionAck + 1)
            throw std::runtime_error("Configuration sequence discontinuity");
        bool allowed = c.match == match_ && peer.pilot == host_ && game_.core()->GetMissionStage()!=STAGE_OVER && (lobby_ || game_.state().complete);
        peer.actionAck = c.number;
        bool accepted = allowed && configure(c.session);
        Receipt receipt{c.number, unsigned(accepted), 0};
        peer.receipts[c.number] = receipt;
        bound(peer.receipts, 256);
        send(m.peer, ::Native::Net::message(Kind::Receipt, receipt, tick_), Lane::Control);
        return;
    }
    if (h.kind == Kind::Action) {
        auto a = r.take<Action>();
        if (!r.rest().empty() || !terminated(a.text) || a.count > a.parts.size())
            throw std::runtime_error("Invalid action record");
        if (auto i = peer.receipts.find(a.number); i != peer.receipts.end()) {
            send(m.peer, ::Native::Net::message(Kind::Receipt, i->second, tick_), Lane::Control);
            return;
        }
        if (a.number != peer.actionAck + 1)
            throw std::runtime_error("Action sequence discontinuity");
        if (!std::isfinite(a.position.x) || !std::isfinite(a.position.y) || !std::isfinite(a.position.z))
            throw std::runtime_error("Invalid command position");
        bool accepted = false;
        uint32_t details=0;
        if (a.match != match_)
            accepted = false;
        else if(a.op==Operation::Chat){
            accepted=chatAction(peer,a);
        }
        else if (a.op == Operation::Team) {
            accepted=requestTeam(peer,a.args[0],&details);
        } else if(a.op==Operation::Lobby){
            accepted=lobbyAction(peer,a);
        } else if (a.op == Operation::NewRound && peer.pilot == host_ && game_.state().complete && Allegiance::MissionCanRestart(paused_,game_.core()->GetMissionParams()->bAllowRestart)) {
            auto next = session_;
            ++next.seed;
            peer.actionAck = a.number;
            accepted = configure(next,false);
        } else if (a.op == Operation::Ready) {
            // FedSrv PLAYER_READY is personal state at every mission stage,
            // including JOIN's AFK wake-up before a running-side departure.
            peer.ready = a.args[0] != 0;
            if (!allReady())cancelCountdown();
            rosterDirty_=true;
            accepted = true;
        } else if (lobby_ && a.op == Operation::Start && peer.pilot == host_ && game_.core()->GetMissionStage()==STAGE_NOTSTARTED && !game_.core()->GetMissionParams()->bAutoRestart) {
            if (countdown_<0 && allReady()) {
                countdown_ = session_.rules.startCountdown * tickRate;
                game_.core()->SetMissionStage(STAGE_STARTING);
                accepted = true;
            }
        } else if (!lobby_ || a.op == Operation::Chat || a.op == Operation::QuickChat || a.op == Operation::Commander || a.op == Operation::Leader || a.op==Operation::ChatPreferences || a.op==Operation::Mute || a.op==Operation::Wing)
            accepted = game_.authorityAction(peer.pilot, a, &details);
        // WinTrek's mission owner is the leader of the owning side. Moving
        // that side's command must move its host controls as well.
        if(accepted && (a.op==Operation::Leader || a.op==Operation::Commander))rosterDirty_=true;
        if(accepted && peer.pilot==host_ && (a.op==Operation::Leader || a.op==Operation::Commander))host_=a.args[0];
        Receipt receipt{a.number, unsigned(accepted), details};
        peer.actionAck = a.number;
        if(accepted && a.op==Operation::Invest)peer.investments.emplace_back(a.number,tick_);
        peer.receipts[a.number] = receipt;
        bound(peer.receipts, 256);
        send(m.peer, ::Native::Net::message(Kind::Receipt, receipt, tick_), Lane::Control);
        return;
    }
    if(h.kind==Kind::ConnectionQuery){
        if(!r.rest().empty() || !h.sequence)throw std::runtime_error("Invalid connection query");
        if(h.tick!=match_)return;
        if(connectionReport_.empty() || now()-connectionReportTime_>=5'000'000){
            std::vector<PlayerConnectionInfo> rows;rows.reserve(peers_.size());
            for(const auto& [connection,p]:peers_)if(p.pilot>=0 && !p.expelled){
                auto quality=transport_->quality(connection);++metrics_.connectionSamples;
                rows.push_back({p.pilot,uint16_t(std::clamp(quality.ping,0,65535)),uint8_t(std::clamp(int((1-quality.outgoingDelivery)*100),0,100)),0});
            }
            std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.pilot<b.pilot;});
            connectionReport_.clear();connectionReport_.reserve(sizeof(ConnectionReport)+rows.size()*sizeof(PlayerConnectionInfo));append(connectionReport_,ConnectionReport{uint32_t(rows.size())});
            for(const auto& row:rows)append(connectionReport_,row);connectionReportTime_=now();
        }
        std::vector<std::byte> bytes;bytes.reserve(sizeof(Header)+connectionReport_.size());append(bytes,Header{magic,Kind::ConnectionReport,match_,h.sequence});
        bytes.insert(bytes.end(),connectionReport_.begin(),connectionReport_.end());send(m.peer,bytes,Lane::Bulk);++metrics_.connectionReports;return;
    }
    if (h.kind == Kind::Ping) {
        auto clock = r.take<ClockProbe>();
        clock.serverReceive = now();
        clock.serverSend = now();
        send(m.peer, ::Native::Net::message(Kind::Pong, clock, tick_), Lane::State);
        return;
    }
    throw std::runtime_error("Unexpected client record");
}
uint64_t Server::reportActionAck(Peer& peer,Tick captured,bool fresh){
    // Actions are applied between ticks. The next simulation/publication tick
    // includes them; an older faction arena must retain the prediction.
    while(!peer.investments.empty() && (fresh || peer.investments.front().second<captured))
        peer.investments.pop_front();
    return peer.investments.empty()?peer.actionAck:peer.investments.front().first-1;
}
void Server::publish(Connection connection, Peer &peer, bool reliable) {
    if (!reliable) {
        if(peer.pendingPublication || (!peer.snapshotAck && !peer.snapshots.empty()))return;
    }
    if(reliable)peer.pendingPublication=0; // A newer bootstrap supersedes an in-flight unreliable capture.
    captureRoster();
    if(!reliable && !lobby_) {
        auto captureStart=now();
        auto report=game_.networkReport(peer.pilot,tick_,peer.inputAck,{peer.noticeAck,peer.eventAck});
        metrics_.captureMs+=(now()-captureStart)*.001;
        auto enqueueStart=now();
        auto& f=report.personal;
        f.header.ack=reportActionAck(peer,report.worlds.front().frame->header.tick);
        metrics_.noticesOmitted+=f.omitted.notices;metrics_.eventsOmitted+=f.omitted.events;
        lobbyStatus(f);
        report.worlds.push_back({0x100000,roster_});
        f.header.sequence=++publication_;
        peer.pendingPublication=f.header.sequence;
        Publisher::Job job;job.connection=connection;job.match=match_;job.header=f.header;
        job.notice=f.notices.empty()?peer.noticeAck:f.notices.back().number;
        job.event=f.events.empty()?peer.eventAck:f.events.back().number;
        for(const auto& world:report.worlds){
            if(!world.frame->events.empty())job.event=std::max(job.event,world.frame->events.back().number);
            if(!world.frame->notices.empty())job.notice=std::max(job.notice,world.frame->notices.back().number);
        }
        job.worlds=std::move(report.worlds);job.captured=std::move(f);
        job.bases.reserve(peer.sentArenaCheckpoints.size());
        for(const auto& [domain,revision]:peer.sentArenaCheckpoints)job.bases.push_back({domain,0,revision});
        publisher_->submit(std::move(job));metrics_.enqueueMs+=(now()-enqueueStart)*.001;return;
    }
    auto f = game_.networkFrame(peer.pilot, tick_, peer.inputAck, !reliable && peers_.size()>1,publisher_->acquireFrame(),!reliable && !lobby_,{peer.noticeAck,peer.eventAck});
    metrics_.noticesOmitted+=f.omitted.notices;metrics_.eventsOmitted+=f.omitted.events;
    lobbyStatus(f);f.members=roster_->members;
    f.header.ack=reportActionAck(peer,f.header.tick,reliable || lobby_);
    f.header.sequence = ++publication_;
    std::vector<std::byte> full;
    if(reliable || lobby_)full=packet(f);
    if (!reliable && lobby_ && !peer.snapshots.empty() && now() - peer.lastPublication < 500000) {
        const auto &previous = *peer.snapshots.rbegin()->second;
        if (full.size() == previous.size() &&
            std::equal(full.begin() + sizeof(Header), full.end(), previous.begin() + sizeof(Header)))
            return; // Lobby time is paused; changed state still publishes immediately.
    }
    std::span<const std::byte> baseline;
    std::shared_ptr<const std::vector<std::byte>> previous;
    uint64_t base = 0;
    if (auto i = peer.snapshots.find(peer.snapshotAck); i != peer.snapshots.end()) {
        base = i->first;
        previous=i->second;
        baseline = *previous;
    }
    uint64_t notice=f.notices.empty()?peer.noticeAck:f.notices.back().number;
    uint64_t event=f.events.empty()?peer.eventAck:f.events.back().number;
    auto captured=std::make_shared<const std::vector<std::byte>>(std::move(full));
    auto bytes = compressed(*captured, base, baseline);
    commitPublication(connection,peer,f.header,std::move(captured),bytes,notice,event,reliable);
}
void Server::flushPublications(){
    for(auto& job:publisher_->take()) {
        auto found=peers_.find(job.connection);
        if(job.match!=match_ || found==peers_.end() || found->second.pendingPublication!=job.header.sequence)continue;
        auto& peer=found->second;peer.pendingPublication=0;
        if(!job.error.empty())throw std::runtime_error("Publication worker: "+job.error);
        metrics_.bytesSent+=job.wireBytes;metrics_.arenaEncodes+=job.encodes;
        for(size_t i=0;i<job.traffic.size();++i)metrics_.arenaTraffic[i]+=job.traffic[i];
        if(job.retainedBytes)metrics_.arenaBytes=job.retainedBytes;
        ++metrics_.publicationJobs;
        if(job.backpressure)++peer.backpressure;
        if(!job.published)continue;
        // Baselines are the current subscriptions, not an ever-growing cache
        // of visited sectors. Re-entry starts from a self-contained revision.
        for(auto i=peer.sentArenaCheckpoints.begin();i!=peer.sentArenaCheckpoints.end();)
            if(std::none_of(job.revisions.begin(),job.revisions.end(),[&](const auto& r){return r.domain==i->first;}))i=peer.sentArenaCheckpoints.erase(i);
            else ++i;
        for(const auto& revision:job.revisions)peer.sentArenaCheckpoints[revision.domain]=revision.revision;
        ++metrics_.snapshots;peer.lastPublication=now();
        ++peer.reportsSent;peer.reportTick=job.header.tick;
        peer.effects[job.header.sequence]={job.notice,job.event};bound(peer.effects,32);
    }
}

bool Server::sendPublication(Connection connection,const Header& header,
        std::span<const std::byte> bytes,Lane lane,bool arena) {
    if(bytes.size()<=fragmentBytes)return send(connection,bytes,lane);
    bool sent=true;
    for(size_t offset=0;offset<bytes.size();offset+=fragmentBytes) {
        std::vector<std::byte> piece;piece.reserve(sizeof(Header)+sizeof(Fragment)+fragmentBytes);
        auto h=header;h.kind=arena?Kind::ArenaFragment:Kind::Fragment;
        append(piece,h);append(piece,Fragment{uint32_t(bytes.size()),uint32_t(offset)});
        append(piece,bytes.subspan(offset,std::min(fragmentBytes,bytes.size()-offset)));
        sent=send(connection,piece,lane)&&sent;
    }
    return sent;
}
void Server::commitPublication(Connection connection,Peer& peer,const Header& header,
    std::shared_ptr<const std::vector<std::byte>> full,std::span<const std::byte> bytes,uint64_t notice,uint64_t event,bool reliable){
    bool sent=sendPublication(connection,header,bytes,reliable?Lane::Bulk:Lane::State);
    if (sent) {
        ++metrics_.snapshots;
        peer.lastPublication = now();
        peer.effects[header.sequence] = {notice,event};
        peer.snapshots[header.sequence] = std::move(full);
        boundSnapshots(peer.snapshots,peer.snapshotAck);
        while (!peer.effects.empty() && peer.effects.begin()->first < peer.snapshots.begin()->first)
            peer.effects.erase(peer.effects.begin());
    }
}
void Server::step() {
    // A retryable connection loss keeps the pilot and role during the same
    // grace interval. Do not transfer ownership merely because its socket is
    // absent: resumed Welcome must already expose the correct owner controls.
    if (host_ < 0 || (!dormant_.contains(host_) &&
        std::none_of(peers_.begin(), peers_.end(), [&](auto &item) { return item.second.pilot == host_; }))) {
        host_ = -1;rosterDirty_=true;
        for (auto &[_, p] : peers_)
            if (p.pilot >= 0 && p.side>=0 && game_.teamLeader(p.side)==p.pilot && (host_ < 0 || p.pilot < host_))
                host_ = p.pilot;
    }
    if (lobby_) {
        automaticCountdown();
        if (countdown_ >= 0 && --countdown_ <= 0) {
            lobby_ = false;
            countdown_ = -1;
            restartCountdown_=-1;
            game_.startNetworkMission();
        }
        if (lobby_) {
            if (++lobbySteps_ % snapshotInterval == 0) {
                rosterDirty_=true;game_.beginNetworkPublication();
                for (auto &[connection, p] : peers_)
                    if (p.pilot >= 0)
                        publish(connection, p);
            }
            return;
        }
    }
    ++tick_;
    controls_.clear();
    controls_.reserve(peers_.size() + dormant_.size());
    const auto wallTime = now();
    for (auto &[_, peer] : peers_)
        if (peer.pilot >= 0) {
            uint32_t transient = 0;
            while (!peer.inputs.empty() && expandCounter(peer.inputs.front().tick, tick_) <= tick_) {
                auto next = peer.inputs.front();
                peer.inputs.pop_front();
                auto number = expandCounter(next.number, peer.inputAck);
                if (number > peer.inputAck) {
                    peer.inputAck = number;
                    transient |= next.buttons & sampledButtons;
                    peer.held = flight(next);
                    metrics_.inputAge =
                        std::max(metrics_.inputAge, double(tick_ - std::min(tick_, expandCounter(next.tick, tick_))) / tickRate);
                }
            }
            auto *pilot = game_.core()->GetShip(ShipID(peer.pilot));
            if (wallTime - peer.lastInput > 250000 || (pilot && pilot->GetStation())) {
                peer.held = FlightInput{};
                peer.held.active = false;
                transient = 0;
            }
            auto held = peer.held;
            held.forward |= transient & Forward;
            held.backward |= transient & Backward;
            held.left |= transient & Left;
            held.right |= transient & Right;
            held.up |= transient & Up;
            held.down |= transient & Down;
            held.boost |= transient & Boost;
            held.fire |= transient & Fire;
            held.missile |= transient & Missile;
            held.chaff |= transient & Chaff;
            held.deploy |= transient & Deploy;
            held.matchSpeed |= transient & MatchSpeed;
            held.throttleChanged = bool(transient & ThrottleChanged);
            held.manualOverride = bool(transient & ManualOverride);
            peer.held.throttleChanged = false; // A change hint is not a held button.
            peer.held.manualOverride = false;
            controls_.emplace_back(peer.pilot, held);
        }
    for (auto &[id, peer] : dormant_) {
        FlightInput neutral;
        neutral.active = false;
        controls_.emplace_back(id, neutral);
    }
    auto simulationBegin=now();
    game_.authorityStep(controls_, 1. / tickRate);
    // FedSrv's mission owner follows the leader of its owning side. A passed
    // mutiny changes command inside the simulation, outside the action lane.
    if(auto* owner=game_.core()->GetShip(ShipID(host_));owner && owner->GetSide() && owner->GetSide()->GetObjectID()>=0){
        auto leader=game_.teamLeader(owner->GetSide()->GetObjectID());
        if(leader>=0 && leader!=host_){host_=leader;rosterDirty_=true;}
    }
    metrics_.simulationMs=(now()-simulationBegin)*.001;
    // FedSrv returns restartable matches to the team lobby after freezing the
    // result. A new authority retains that public report; welcome/bootstrap
    // delivers it even if an old unreliable publication was never received.
    if(game_.state().complete && session_.scenario==int(Scenario::Conquest)){
        // Preserve final per-recipient messages on the ordered control lane
        // before replacing the authority and sending the next welcome.
        if(!roundEndingPublished_){for(auto& [connection,peer]:peers_)if(peer.pilot>=0)publish(connection,peer,true);roundEndingPublished_=true;}
        if(Allegiance::MissionCanRestart(paused_,game_.core()->GetMissionParams()->bAllowRestart)){
            auto next=session_;++next.seed;if(configure(next,false))return;
        }else game_.core()->SetMissionStage(STAGE_OVER);
    }
    auto publicationBegin=now();
    metrics_.preparationMs=metrics_.captureMs=metrics_.enqueueMs=0;
    if (tick_==1 || tick_ % snapshotInterval == 0) {
        rosterDirty_=true;game_.beginNetworkPublication(tick_);
        metrics_.preparationMs=(now()-publicationBegin)*.001;
    }
    // Each sector cohort still publishes at snapshotRate. Full physical state
    // and private input receipts are captured at this tick; faction map/intel
    // is shared for the publication cycle. This removes the 1,024-peer burst
    // without recapturing/encoding every sector on every simulation tick.
    for (auto &[connection, peer] : peers_)if(peer.pilot>=0) {
        int sector=game_.networkViewSector(peer.pilot);
        unsigned phase=sector<0?0:unsigned(sector)%snapshotInterval;
        if(tick_%snapshotInterval==phase)publish(connection,peer);
    }
    metrics_.publicationMs=(now()-publicationBegin)*.001;
}
bool Server::send(Connection connection, std::span<const std::byte> bytes, Lane lane) {
    bool accepted = transport_->send(connection, bytes, lane);
    if (accepted)
        metrics_.bytesSent += bytes.size();
    return accepted;
}
bool Client::send(std::span<const std::byte> bytes, Lane lane) {
    bool accepted = transport_->send(peer_, bytes, lane);
    if (accepted)
        metrics_.bytesSent += bytes.size();
    return accepted;
}
Client::Client(std::string_view address, Digest content, std::string name, int side, Factory factory,std::array<uint64_t,2> creatorToken,Instance instance)
    : content_(content), factory_(std::move(factory)), address_(address), name_(std::move(name)), side_(side) {
    token_=creatorToken;
    instance_=instance;
    auto [t, c] = connect(address);
    transport_ = std::move(t);
    peer_ = c;
}
Client::~Client() {
    transport_->close(peer_, "Pilot left match");
    if (game_)
        game_->networkActions({});
}
void Client::reconnect() {
    if (!game_ || closed_)
        return;
    ready_ = false;
    reconnecting_ = true;
    if (!disconnected_)
        disconnected_ = now();
    retry_ = now() + 500000;
    transport_->close(peer_, "Connection replaced", true);
    transport_.reset();
    auto [transport, connection] = connect(address_);
    transport_ = std::move(transport);
    peer_ = connection;
    snapshots_.clear();
    snapshotAck_ = 0;
    assemblies_.clear();assemblyBytes_=0;
    samples_.clear();
    presentationTimeValid_ = false;
    lastPresentationNs_ = 0;
    startup_.clear();startupBytes_=0;
    accumulator_ = 0;
    pendingButtons_ = 0;
    lastSnapshotTime_ = 0;
}
void Client::poll() {
    if (game_)
        game_->restoreNetworkPresentation();
    auto incoming = transport_->receive();
    for (auto &e : transport_->events())
        if (e.peer == peer_) {
            if (e.connected) {
                Hello hello{};
                hello.build = buildDigest();
                hello.content = content_;
                hello.side = side_;
                hello.token = token_;
                hello.instance=instance_;
                text(hello.name, name_);
                send(::Native::Net::message(Kind::Hello, hello), Lane::Control);
            } else {
                ready_ = false;
                error_ = e.reason;
                if (game_ && e.retryable) {
                    reconnecting_ = true;
                    if (!disconnected_)
                        disconnected_ = now();
                    retry_ = now() + 500000;
                } else
                    closed_ = true;
            }
        }
    for (auto &m : incoming)
        try {
            message(m);
        } catch (const std::exception &e) {
            error_ = e.what();
            closed_ = true;
            ready_ = false;
            transport_->close(peer_, error_);
        }
    if (reconnecting_ && !closed_) {
        if (now() - disconnected_ > 30000000) {
            closed_ = true;
            error_ = "Reconnection timed out: " + error_;
        } else if (now() > retry_)
            reconnect();
    }
    if (game_ && !reconnecting_ && !closed_ && now() - lastPing_ > 500000) {
        lastPing_ = now();
        send(::Native::Net::message(Kind::Ping, ClockProbe{lastPing_, 0, 0}, 0, 0, snapshotAck_),
                         Lane::State);
    }
}
void Client::message(const Message &m) {
    metrics_.bytesReceived += m.data.size();
    if (m.data.size() > size_t(LZ4_compressBound(maxDeltaData)) + sizeof(Header) + sizeof(Delta))
        throw std::runtime_error("Server record limit");
    Reader r(m.data);
    auto h = r.take<Header>();
    if (h.signature != magic)
        throw std::runtime_error("Foreign server record");
    std::vector<std::byte> unpacked;
    unpackMessage(h, r, unpacked);
    if(h.kind==Kind::ConnectionReport){
        auto report=r.take<ConnectionReport>();
        if(report.count>shipIdentityCapacity || report.reserved || r.rest().size()!=size_t(report.count)*sizeof(PlayerConnectionInfo))throw std::runtime_error("Invalid connection report");
        auto rows=r.array<PlayerConnectionInfo>(report.count);int previous=-1;
        for(const auto& row:rows){if(row.pilot<=previous || row.pilot>=shipIdentityCapacity || row.loss>100 || row.reserved)throw std::runtime_error("Invalid player connection statistics");previous=row.pilot;}
        if(game_ && !reconnecting_ && connectionQueryPending_ && h.tick==match_ && h.sequence==connectionQuery_){connectionReport_=std::move(rows);connectionQueryPending_=false;}
        return;
    }
    if ((!game_ || reconnecting_) && (h.kind == Kind::Delta || h.kind == Kind::Fragment || h.kind==Kind::Arena || h.kind==Kind::ArenaFragment || h.kind==Kind::Report || h.kind==Kind::MissionPolicy || h.kind==Kind::Briefing)) {
        if (startup_.size() >= 1024 || m.data.size()>4*maxFrame-startupBytes_)
            throw std::runtime_error("Startup record limit");
        startup_.push_back(m);startupBytes_+=m.data.size();
        return;
    }
    if(h.kind==Kind::Briefing){
        auto record=r.take<Briefing>();auto bytes=r.rest();
        if(record.reserved || record.generateCivilization>1 || !record.sides || (record.sides&~0x3fu) || record.bytes>maxBriefingBytes || bytes.size()!=record.bytes ||
           std::find(bytes.begin(),bytes.end(),std::byte{})!=bytes.end())throw std::runtime_error("Invalid mission briefing record");
        if(h.tick==match_)game_->missionBriefing(record.sides,std::make_shared<const Allegiance::MissionBriefing>(Allegiance::MissionBriefing{std::string(reinterpret_cast<const char*>(bytes.data()),bytes.size()),record.generateCivilization!=0}));
        return;
    }
    if (h.kind == Kind::Fragment || h.kind==Kind::ArenaFragment) {
        const bool arena=h.kind==Kind::ArenaFragment;
        if (!arena && h.sequence <= snapshotAck_)
            return;
        auto f = r.take<Fragment>();
        if (f.size > size_t(LZ4_compressBound(maxDeltaData)) + sizeof(Header) + sizeof(Delta) || f.size <= fragmentBytes ||
            f.offset >= f.size || f.offset % fragmentBytes ||
            r.rest().size() != std::min(size_t(fragmentBytes), size_t(f.size - f.offset)))
            throw std::runtime_error("Invalid snapshot fragment");
        for (auto i = assemblies_.begin(); i != assemblies_.end();)
            if ((!arena && i->first <= snapshotAck_) || now() - i->second.received > 1000000)
                {assemblyBytes_-=i->second.data.size();i = assemblies_.erase(i);}
            else
                ++i;
        auto &a = assemblies_[h.sequence];
        if (a.data.empty()) {
            if(f.size>4*maxFrame-assemblyBytes_)throw std::runtime_error("Fragment assembly byte limit");
            a.received = now();
            a.data.resize(f.size);
            assemblyBytes_+=a.data.size();
            a.pieces.resize((f.size + fragmentBytes - 1) / fragmentBytes);
        }
        if (a.data.size() != f.size)
            throw std::runtime_error("Fragment size changed");
        auto slot = f.offset / fragmentBytes;
        if (!a.pieces[slot]) {
            std::memcpy(a.data.data() + f.offset, r.rest().data(), r.rest().size());
            a.pieces[slot] = true;
            ++a.count;
        }
        if (a.count == a.pieces.size()) {
            assemblyBytes_-=a.data.size();
            Message whole{m.peer, std::move(a.data)};
            assemblies_.erase(h.sequence);
            metrics_.bytesReceived -= whole.data.size();
            message(whole);
        }
        while(assemblies_.size()>maxReportArenas*4){assemblyBytes_-=assemblies_.begin()->second.data.size();assemblies_.erase(assemblies_.begin());}
        return;
    }
    if (h.kind == Kind::Welcome) {
        connectionReport_.reset();connectionQueryPending_=false;
        auto w = r.take<Welcome>();
        if(!terminated(w.server) || !r.rest().empty() || !validInstance(w.instance))throw std::runtime_error("Invalid welcome identity");
        if(validInstance(instance_) && w.instance!=instance_)throw std::runtime_error("The selected game is no longer available.");
        instance_=w.instance;
        serverName_=w.server;
        if (w.rate != tickRate)
            throw std::runtime_error("Unexpected simulation cadence");
        arenas_.clear();reports_.clear();activeArenas_.clear();
        arrivalJitter_ = 0;
        lastSnapshotTime_ = 0;
        interpolationDelay_ = 1. / snapshotRate;
        bool newRound = game_ && w.match != match_;
        auto history=newRound?game_->state().messages:std::deque<GameMessage>{};
        if (game_ && !reconnecting_ && !newRound)
            throw std::runtime_error("Duplicate welcome");
        if (newRound) {
            for(auto& purchase:purchases_)purchase.result={};
            transfers_.clear();
            dockRefitStarted_=0;
            appliedActions_=0;
            game_->networkActions({});
            game_ = nullptr;
            ready_ = false;
            snapshots_.clear();arenas_.clear();reports_.clear();
            snapshotAck_ = 0;
            assemblies_.clear();assemblyBytes_=0;
            samples_.clear();
            presentationTimeValid_ = false;
            lastPresentationNs_ = 0;
            presentationError_ = {};
            sourceRotationError_ = turretRotationError_ = {1, 0, 0, 0};
            ownSerial_ = ownEpoch_ = 0;
            lastSnapshotTime_ = 0;
            accumulator_ = 0;
            pendingButtons_ = 0;
        }
        match_ = w.match;
        side_ = w.side;
        token_ = w.token;
        epoch_ = w.epoch;
        serverTick_ = h.tick;
        predictionTick_ = h.tick;
        if (!game_)
            game_ = &factory_(options(w.session, side_, name_));
        if(newRound)game_->retainMessages(std::move(history));
        // A new match rebuilds every replica and may assign new pilot IDs.
        // Resume identity is immutable only within the retained match.
        if (reconnecting_ && !newRound && game_->state().shipId != w.pilot)
            throw std::runtime_error("Resume changed pilot identity");
        while (!inputs_.empty() && expandCounter(inputs_.front().number, w.inputAck) <= w.inputAck)
            inputs_.pop_front();
        for (auto i = actions_.begin(); i != actions_.end();)
            if (i->second.record.match != match_) {
                if(i->second.record.op==Operation::PurchaseLoadout)purchases_.push_back({i->first,{}});
                i = actions_.erase(i);
            } else if(i->first<=w.actionAck && i->second.record.op!=Operation::PurchaseLoadout && !transferOperation(i->second.record))
                i=actions_.erase(i);
            else
                ++i;
        for (auto i = configurations_.begin(); i != configurations_.end();)
            if (i->first <= w.actionAck || i->second.match != match_)
                i = configurations_.erase(i);
            else
                ++i;
        // Configuration and gameplay share one reliable sequence. Resuming
        // must preserve that order across both retained record arrays.
        auto pendingAction = actions_.begin();
        auto configuration = configurations_.begin();
        while (pendingAction != actions_.end() || configuration != configurations_.end()) {
            if (configuration == configurations_.end() ||
                (pendingAction != actions_.end() && pendingAction->first < configuration->first)) {
                send(::Native::Net::message(Kind::Action, pendingAction->second.record, h.tick, pendingAction->first),
                    Lane::Control);
                ++pendingAction;
            } else {
                send(::Native::Net::message(Kind::Configure, configuration->second), Lane::Control);
                ++configuration;
            }
        }
        inputs_.clear();
        // The authority discarded unprocessed input on the replaced connection.
        // Restart at its processed receipt, even if the old history was full.
        inputNumber_ = w.inputAck;
        actionNumber_ = std::max(actionNumber_, w.actionAck);
        reconnecting_ = false;
        disconnected_ = retry_ = 0;
        if(ready_)game_->networkActions([this](const Action &a) { return action(a); });
        auto waiting = std::move(startup_);
        startup_.clear();startupBytes_=0;
        for (auto &pending : waiting) {
            metrics_.bytesReceived -= pending.data.size();
            message(pending);
        }
        return;
    }
    if(h.kind==Kind::MissionPolicy){
        auto policy=r.take<MissionPolicy>();
        if(!r.rest().empty() || policy.reserved || policy.reservedTail || (policy.flags&~7u) || !Allegiance::ValidTeamImbalance(policy.imbalance) ||
           std::any_of(policy.alliances.begin(),policy.alliances.end(),[](char group){return group<NA || group>=c_cAlliancesMax;}))throw std::runtime_error("Invalid mission policy");
        if(policy.match==match_ && game_ && !game_->applyMissionPolicy(policy))throw std::runtime_error("Invalid mission policy state");
        return;
    }
    if (h.kind == Kind::Pong) {
        auto p = r.take<ClockProbe>();
        double rtt = now() - p.client - (p.serverSend - p.serverReceive);
        if (rtt >= 0 && rtt < bestRtt_ * 1.1) {
            bestRtt_ = std::min(bestRtt_, rtt);
            double estimate = ((p.serverReceive - p.client) + (p.serverSend - now())) * .5;
            offset_ = metrics_.rtt == 0 ? estimate : offset_ * .9 + estimate * .1;
            metrics_.clockOffset = offset_;
        }
        metrics_.rtt = rtt / 1000.;
        return;
    }
    if (h.kind == Kind::Receipt) {
        auto receipt = r.take<Receipt>();
        auto action = actions_.find(receipt.number);
        const auto* record=action==actions_.end()?nullptr:&action->second.record;
        if(record && record->op==Operation::PurchaseLoadout)
            purchases_.push_back({receipt.number,{receipt.accepted!=0,(receipt.details&1)!=0,(receipt.details&2)!=0}});
        if(receipt.accepted && record && transferOperation(*record))
            transfers_.push_back(receipt.number);
        if (!receipt.accepted && action != actions_.end()) {
            ++metrics_.rejected;
            rejection_ = record->op==Operation::Team?Allegiance::LobbyJoinFailureMessage(Allegiance::LobbyJoinFailure(receipt.details)):game_->networkRejected(*record);
            rejectedModalAction_=(record->op==Operation::PurchaseLoadout || transferOperation(*record))?receipt.number:0;
        }
        if (!receipt.accepted && configurations_.contains(receipt.number)) {
            ++metrics_.rejected;
            rejection_ = "Server declined the mission settings.";
            rejectedModalAction_=0;
        }
        configurations_.erase(receipt.number);
        // A receipt can precede the report containing this action. Keep
        // accepted local predictions until that report's causal watermark.
        if(!receipt.accepted || !record || !predictsClientAction(*record) || receipt.number<=appliedActions_)
            actions_.erase(receipt.number);
        return;
    }
    if(h.kind==Kind::Arena) {
        auto record=r.take<ArenaRecord>();
        if(record.match!=match_)return;
        if(record.domain>=0x1000000 || h.sequence!=((h.tick<<24)|record.domain))
            throw std::runtime_error("Arena identity mismatch");
        auto& arena=arenas_[record.domain];
        if(!arena.revisions.empty() && h.tick<=arena.revisions.rbegin()->first) {
            return;
        }
        Reader delta(r.rest());auto inner=delta.take<Header>();auto basis=delta.take<Delta>();
        if(inner.kind!=Kind::Delta || inner.sequence!=h.tick || inner.tick!=h.tick)
            throw std::runtime_error("Arena revision mismatch");
        auto full=expanded(r.rest(),arena.revisions);
        if(full.empty()){++metrics_.missingBaselines;return;}
        auto world=frame(full);
        bool feedbackArena=record.domain>=0x300000 && record.domain<=0x300006;
        if(world.status.match!=match_ || world.status.pilot!=-1 || (!world.notices.empty() && !feedbackArena) ||
           std::any_of(world.notices.begin(),world.notices.end(),[](const Notice& n){return !(n.flags&NoticeSharedTip);}) ||
           std::any_of(world.events.begin(),world.events.end(),[](const Event& e){return e.pilot>=0;}))
            throw std::runtime_error("Arena contains private state");
        if(feedbackArena && (!world.entities.empty() || !world.parts.empty() || !world.pilots.empty() || !world.members.empty() || !world.teams.empty() || !world.buckets.empty() || std::any_of(world.events.begin(),world.events.end(),[](const Event& e){return !isSharedCue(e);}) || !world.highlights.empty() || !world.resultPlayers.empty() || !world.resultTeams.empty() || !world.shipIntel.empty() || !world.payload.empty()))
            throw std::runtime_error("Feedback arena contains world state");
        arena.revisions[h.tick]=std::move(full);
        if(!basis.baseline)arena.checkpoint=std::max(arena.checkpoint,h.tick);
        boundSnapshots(arena.revisions,arena.checkpoint);
        applyReports();return;
    }
    if(h.kind==Kind::Report) {
        if(h.sequence<=snapshotAck_)return;
        auto manifest=r.take<ArenaManifest>();
        if(manifest.count==0 || manifest.count>maxReportArenas)throw std::runtime_error("Arena manifest size");
        auto references=r.array<ArenaRef>(manifest.count);
        for(const auto& ref:references)
            if((ref.domain&0x800000)?ref.reserved!=presentationArena:(ref.reserved&~stateOnlyArena))
                throw std::runtime_error("Invalid arena dependency flags");
        auto full=expanded(r.rest(),{});
        auto personal=frame(full);
        if(personal.status.match!=match_)return;
        if(personal.header.sequence!=h.sequence || personal.header.tick!=h.tick ||
           !personal.entities.empty() || !personal.parts.empty() || !personal.payload.empty())
            throw std::runtime_error("Private report identity mismatch");
        reports_[h.sequence]={h,std::move(references),std::move(personal)};
        bound(reports_,8);applyReports();return;
    }
    if (h.kind == Kind::Delta) {
        if (!game_)
            throw std::runtime_error("Snapshot before welcome");
        if (h.sequence <= snapshotAck_)
            return;
        auto basis=r.take<Delta>().baseline;
        auto full = expanded(m.data, snapshots_);
        if (full.empty()) {
            ++metrics_.missingBaselines;
            return;
        }
        auto f = frame(full);
        if (f.status.match != match_)
            return;
        if (f.header.signature != magic || f.header.sequence != h.sequence || f.header.tick != h.tick)
            throw std::runtime_error("Snapshot header mismatch");
        snapshots_[h.sequence] = std::move(full);
        boundSnapshots(snapshots_,basis);
        applySnapshot(std::move(f));return;
    }
    throw std::runtime_error("Unexpected server record");
}
void Client::applyReports() {
    for(auto i=reports_.begin();i!=reports_.end();) {
        if(i->first<=snapshotAck_){i=reports_.erase(i);continue;}
        std::vector<Frame> decoded;decoded.reserve(i->second.references.size());bool complete=true;
        for(const auto& reference:i->second.references) {
            auto a=arenas_.find(reference.domain);
            if(a==arenas_.end()){if(reference.reserved&presentationArena)continue;complete=false;break;}
            auto revision=a->second.revisions.find(reference.revision);
            if(revision==a->second.revisions.end()) {
                if(!(reference.reserved&presentationArena)){complete=false;break;}
                // Keep the newest available anchors at or before this report.
                // Their analytic motion and expiry still use authority time.
                revision=a->second.revisions.upper_bound(reference.revision);
                if(revision==a->second.revisions.begin())continue;
                --revision;
            }
            decoded.push_back(frame(revision->second));
            if(reference.reserved&stateOnlyArena)decoded.back().events.clear();
        }
        if(!complete){++i;continue;}
        std::vector<const Frame*> worlds;for(const auto& world:decoded)worlds.push_back(&world);
        auto f=composeReport(combineWorlds(worlds),std::move(i->second.personal));
        activeArenas_=std::move(i->second.references);
        i=reports_.erase(i);applySnapshot(std::move(f));
        for(auto a=arenas_.begin();a!=arenas_.end();){
            auto uses=[&](const auto& refs){return std::any_of(refs.begin(),refs.end(),[&](const auto& ref){return ref.domain==a->first;});};
            bool needed=uses(activeArenas_) || std::any_of(reports_.begin(),reports_.end(),[&](const auto& report){return uses(report.second.references);});
            if(!needed)a=arenas_.erase(a);else ++a;
        }
    }
}
void Client::applySnapshot(Frame f) {
        snapshotAck_ = f.header.sequence;
        serverTick_ = f.header.tick;
        bool wasLobby = game_->state().networkLobby;
        bool hadPilot=game_->playerShip()!=nullptr;
        bool wasPassenger=game_->state().passenger;
        bool wasGhost=hadPilot && game_->playerShip()->IsGhost();
        int oldStation=hadPilot && game_->playerShip()->GetStation()?game_->playerShip()->GetStation()->GetObjectID():NA;
        bool returnedToStation=hadPilot && !game_->state().docked;
        reconcile(f);
        appliedActions_=std::max(appliedActions_,f.header.ack);
        if (wasLobby != game_->state().networkLobby)
            presentationTimeValid_ = false;
        Sample sample;
        sample.tick = f.header.tick;
        sample.received = now();
        sample.entities = std::move(f.entities);
        for (size_t i = 0; i < sample.entities.size(); ++i)
            sample.indices[sample.entities[i].identity.serial] = i;
        const double interval = 1. / snapshotRate;
        double arrival = lastSnapshotTime_ ? double(sample.received - lastSnapshotTime_) / 1e6 : interval;
        arrivalJitter_ = arrivalJitter_ * .95 + std::abs(arrival - interval) * .05;
        interpolationDelay_ = std::clamp(interval + arrivalJitter_ * 2, interval, .2);
        metrics_.interpolationDelay = interpolationDelay_;
        lastSnapshotTime_ = sample.received;
        samples_.push_back(std::move(sample));
        while (samples_.size() > 8)
            samples_.pop_front();
        ++metrics_.snapshots;
        if (!ready_) {
            lastPing_ = now();
            send(::Native::Net::message(Kind::Ping, ClockProbe{lastPing_, 0, 0}, 0, 0, snapshotAck_),
                             Lane::State);
            ready_=true;
            game_->networkActions([this](const Action &a) { return action(a); });
            game_->rememberEditorLoadout(game_->currentLoadout(),true);
        }
        bool enteredMission=(!hadPilot || wasLobby) && !game_->state().networkLobby;
        if(game_->state().ghost && (!hadPilot || !wasGhost) && !game_->state().networkLobby && !game_->state().complete && game_->state().sideActive) {
            // Also align a newly bootstrapped observer with its replica's
            // chosen sector. This is one view intent, not periodic state.
            Action view;view.op=Operation::View;view.args[0]=game_->state().sectorId;view.args[1]=1;action(view);
        }
        auto* station=game_->playerShip()->GetStation();
        bool replacedAtStation=game_->state().pod && station && (wasPassenger || (oldStation>=0 && oldStation!=station->GetObjectID()));
        if((enteredMission || returnedToStation || replacedAtStation) && game_->state().docked && !game_->state().networkLobby)
            if(game_->restoreDockedLoadout(enteredMission || replacedAtStation).accepted)dockRefitStarted_=actionNumber_;
        if (game_->state().docked || game_->state().networkLobby)
            sendInputs(true); // Advance delta/effect receipts without manufacturing flight commands.
        ready_ = true;
}
std::optional<PurchaseResult> Client::takePurchase(uint64_t* action) {
    if(action)*action=0;
    if(purchases_.empty() || (purchases_.front().result.accepted && purchases_.front().number>appliedActions_))return {};
    if(action)*action=purchases_.front().number;
    auto result=purchases_.front().result;purchases_.pop_front();
    if(result.accepted)game_->rememberEditorLoadout(game_->currentLoadout());
    return result;
}
std::optional<uint64_t> Client::takeTransfer() {
    if(transfers_.empty() || transfers_.front()>appliedActions_)return {};
    auto number=transfers_.front();transfers_.pop_front();return number;
}
void Client::reconcile(const Frame &f) {
    game_->restoreNetworkPresentation();
    auto *source = game_->playerShip() ? game_->playerShip()->GetSourceShip() : nullptr;
    auto p = source && source->GetThingSite() ? source->GetPosition() : Vector::GetZero();
    glm::vec3 previous{p.x, p.y, p.z};
    auto previousSourceRotation = orientation(source);
    auto previousTurretRotation = orientation(game_->playerShip());
    // GameState describes the last displayed frame and already includes the
    // decaying correction. Only canonical-to-canonical error can be accumulated.
    game_->applyNetworkFrame(f);
    side_=game_->playerShip()->GetSide()->GetObjectID();
    const Action* selection=nullptr;
    std::array<Game::InvestmentIntent,256> investments;size_t investmentCount=0;
    for(auto i=actions_.begin();i!=actions_.end();) {
        const auto& action=i->second.record;
        if(!predictsClientAction(action)){++i;continue;}
        if(i->first<=f.header.ack)i=actions_.erase(i);
        else {
            if(predictsWeaponSelection(action))selection=&action;
            else investments[investmentCount++]={i->second.side,action.args[0],action.args[1]};
            ++i;
        }
    }
    // Selection records contain the complete intent; only the most recent
    // unincorporated one needs to be replayed, even after a burst of keys.
    if(selection)game_->replayWeaponAction(*selection);
    game_->replayInvestments(std::span(investments).first(investmentCount));
    while (!inputs_.empty() && expandCounter(inputs_.front().number, f.status.inputAck) <= f.status.inputAck)
        inputs_.pop_front();
    for (auto &i : inputs_)
        game_->predict(flight(i), 1. / tickRate, false);
    predictionTick_ = f.header.tick + inputs_.size();
    double correction = glm::length(game_->state().position - previous);
    auto own = std::find_if(f.entities.begin(), f.entities.end(),
                            [&](auto &e) { return e.identity.type == 0 && e.identity.id == f.status.pilot; });
    bool continuous = game_->playerShip()->GetThingSite() && own != f.entities.end() && own->identity.serial == ownSerial_ && own->epoch == ownEpoch_;
    if (own != f.entities.end()) {
        ownSerial_ = own->identity.serial;
        ownEpoch_ = own->epoch;
    }
    if (!continuous) {
        ++metrics_.discontinuities;
        presentationError_ = {};
        sourceRotationError_ = turretRotationError_ = {1, 0, 0, 0};
    } else {
        if (correction < 25)
            presentationError_ += previous - game_->state().position;
        auto accumulate = [](glm::quat &error, glm::quat before, glm::quat after) {
            // Only authority corrections are softened. Normal input rotation is
            // immediately visible; display attitude never feeds prediction.
            if (std::abs(glm::dot(before, after)) < .7071068f)
                error = {1, 0, 0, 0};
            else
                error = glm::normalize(error * before * glm::inverse(after));
        };
        accumulate(sourceRotationError_, previousSourceRotation, orientation(game_->playerShip()->GetSourceShip()));
        accumulate(turretRotationError_, previousTurretRotation, orientation(game_->playerShip()));
    }
    if (continuous && correction > .01) {
        ++metrics_.corrections;
        metrics_.correctionSum += correction;
        metrics_.correctionMax = std::max(metrics_.correctionMax, correction);
    }
    metrics_.remoteAge = double(predictionTick_ - serverTick_) / tickRate;
}
bool Client::queryConnections(){
    if(!ready_ || closed_ || reconnecting_ || connectionQueryPending_)return false;
    std::vector<std::byte> bytes;append(bytes,Header{magic,Kind::ConnectionQuery,match_,connectionQuery_+1});
    if(!send(bytes,Lane::Control))return false;
    ++connectionQuery_;connectionQueryPending_=true;connectionReport_.reset();return true;
}
bool Client::action(Action a) {
    if (!ready_ || closed_ || actions_.size() + configurations_.size() >= 256)
        return false;
    a.number = actionNumber_ + 1;
    a.match = match_;
    auto bytes = ::Native::Net::message(Kind::Action, a, predictionTick_, a.number, snapshotAck_);
    if (!send(bytes, Lane::Control))
        return false;
    actionNumber_ = a.number;
    actions_[a.number] = {a,game_->state().playerSide};
    return true;
}
bool Client::configure(SessionOptions options) {
    if (!ready_ || closed_ || actions_.size() + configurations_.size() >= 256)
        return false;
    Configuration c{};
    c.number = actionNumber_ + 1;
    c.match = match_;
    c.session = session(options);
    auto bytes = ::Native::Net::message(Kind::Configure, c, predictionTick_, c.number, snapshotAck_);
    if (!send(bytes, Lane::Control))
        return false;
    actionNumber_ = c.number;
    configurations_[c.number] = c;
    return true;
}
void Client::input(const FlightInput &controls) {
    if (inputs_.size() >= inputHistory) {
        ++metrics_.inputDrops;
        return;
    }
    auto canonical = game_->canonicalNetworkInput(controls);
    auto next = Net::input(canonical, ++inputNumber_, ++predictionTick_);
    next.buttons |= pendingButtons_;
    canonical = flight(next);
    pendingButtons_ = 0;
    inputs_.push_back(next);
    sendInputs();
    game_->predict(canonical, 1. / tickRate);
}
void Client::sendInputs(bool receiptOnly) {
    struct Bundle {
        InputHeader header;
        std::array<Input, 3> samples;
    } bundle{};
    static_assert(sizeof(Bundle) == sizeof(InputHeader) + 3 * sizeof(Input));
    bundle.header.match = uint32_t(match_);
    bundle.header.ack = uint32_t(snapshotAck_);
    size_t start = inputs_.size() > 3 ? inputs_.size() - 3 : 0;
    size_t count = receiptOnly ? 0 : inputs_.size() - start;
    for (size_t i = 0; i < count; ++i)
        bundle.samples[i] = inputs_[start + i];
    send(std::as_bytes(std::span(&bundle, 1)).first(sizeof(InputHeader) + count * sizeof(Input)), Lane::State);
}
void Client::present(double dt, const FrameTiming &timing) {
    if (samples_.empty() || !game_)
        return;
    // A single lost publication can leave a gap longer than one snapshot.
    // Continue the last authoritative velocity for at most the input watchdog
    // interval; this changes display poses only, never replicated game state.
    constexpr double extrapolationLimit=.25;
    double target = double(samples_.back().tick) / tickRate +
                    std::clamp(double(int64_t(timing.sampledNs / 1000) - samples_.back().received) / 1e6,
                               -.1, interpolationDelay_+extrapolationLimit) - interpolationDelay_;
    // Advance one continuous timeline. Arrival jitter and buffer adaptation
    // steer its rate instead of moving every remote pose at publication edges.
    if (!presentationTimeValid_ || std::abs(target - presentationTime_) > .5) {
        presentationTime_ = target;
        presentationTimeValid_ = true;
    } else
        presentationTime_ += std::clamp(dt, 0., .1) * std::clamp(1 + (target - presentationTime_) * 2, .9, 1.1);
    presentationTime_ = std::min(presentationTime_, double(samples_.back().tick) / tickRate + extrapolationLimit);
    // The continuous clock is anchored at input sampling. Evaluate this frame
    // at its display target without feeding variable render lead into that clock.
    target = std::min(presentationTime_ + timing.leadSeconds(), double(samples_.back().tick) / tickRate + extrapolationLimit);
    metrics_.remotePresentationTime = target;
    const Sample *before = &samples_.front();
    const Sample *after = &samples_.back();
    for (auto &s : samples_) {
        if (double(s.tick) / tickRate <= target)
            before = &s;
        if (double(s.tick) / tickRate >= target) {
            after = &s;
            break;
        }
    }
    double duration = double(after->tick - before->tick) / tickRate;
    float alpha =
        duration > 0 ? std::clamp(float((target - double(before->tick) / tickRate) / duration), 0.f, 1.f) : 1.f;
    presentation_.clear();
    presentation_.reserve(after->entities.size());
    auto v = [](const Vec &p) { return glm::vec3(p.x, p.y, p.z); };
    auto record = [](glm::vec3 p) { return Vec{p.x, p.y, p.z}; };
    for (auto e : after->entities) {
        if (!e.identity.serial)
            continue;
        if(e.flags&Trajectory) {
            e.motion=trajectoryMotion(e,target);
            presentation_.push_back(e);
            continue;
        }
        auto found = before->indices.find(e.identity.serial);
        if (found != before->indices.end()) {
            auto &a = before->entities[found->second];
            if (!(a.flags&Strategic) && !(e.flags&Strategic) && a.epoch == e.epoch && a.sector == e.sector) {
                float t = alpha, t2 = t * t, t3 = t2 * t;
                auto p = (2 * t3 - 3 * t2 + 1) * v(a.motion.position) +
                         (t3 - 2 * t2 + t) * float(duration) * v(a.motion.velocity) +
                         (-2 * t3 + 3 * t2) * v(e.motion.position) + (t3 - t2) * float(duration) * v(e.motion.velocity);
                e.motion.position = record(p);
                e.motion.velocity = record(glm::mix(v(a.motion.velocity), v(e.motion.velocity), t));
                auto orientation = [&](const Motion &m) {
                    auto f = glm::normalize(v(m.forward)), u = glm::normalize(v(m.up));
                    return glm::quat_cast(glm::mat3(glm::normalize(glm::cross(f, u)), u, -f));
                };
                auto q = glm::normalize(glm::slerp(orientation(a.motion), orientation(e.motion), t));
                e.motion.forward = record(q * glm::vec3(0, 0, -1));
                e.motion.up = record(q * glm::vec3(0, 1, 0));
            }
        }
        if (target > double(after->tick) / tickRate)
            e.motion.position =
                record(v(e.motion.position) +
                       v(e.motion.velocity) * float(std::min(extrapolationLimit, target - double(after->tick) / tickRate)));
        presentation_.push_back(e);
    }
    game_->presentNetwork(presentation_);
    metrics_.remoteAge = interpolationDelay_;
}
void Client::step(const FlightInput &controls, double dt, FrameTiming timing) {
    poll();
    if (!ready_ || closed_)
        return;
    const bool timed = timing.sampledNs != 0;
    if (!timed)
        timing.sampledNs = monotonicNs();
    double displayDt = dt;
    if (timed && lastPresentationNs_)
        displayDt = timing.evaluatedNs() > lastPresentationNs_
                        ? double(timing.evaluatedNs() - lastPresentationNs_) * 1e-9
                        : 0;
    lastPresentationNs_ = timed ? timing.evaluatedNs() : 0;
    const double lead = timing.leadSeconds();
    metrics_.presentationLead = lead;
    if (game_->state().networkLobby || game_->state().docked) {
        accumulator_ = 0;
        pendingButtons_ = 0;
        predictionTick_ = serverTick_;
        metrics_.localPresentationFraction = 0;
        present(dt, timing);
        game_->presentPrediction(0, {}, displayDt);
        return;
    }
    if (controls.active)
        pendingButtons_ |= inputButtons(controls) & sampledButtons;
    else
        pendingButtons_ = 0; // Losing focus/input ownership discards deferred presses.
    accumulator_ += std::clamp(dt, 0., .1);
    unsigned count = 0;
    while (accumulator_ >= 1. / tickRate && count++ < 6) {
        accumulator_ -= 1. / tickRate;
        input(controls);
    }
    float retained = float(std::exp(-std::clamp(dt, 0., .1) / .06));
    presentationError_ *= retained;
    const glm::quat identity{1, 0, 0, 0};
    sourceRotationError_ = glm::normalize(glm::slerp(sourceRotationError_, identity, 1 - retained));
    turretRotationError_ = glm::normalize(glm::slerp(turretRotationError_, identity, 1 - retained));
    // Decay the additional display lead in a temporary correction. Persistent
    // correction state remains at sample time, just like the input accumulator.
    float displayRetained = float(std::exp(-lead / .06));
    auto sourceError = glm::normalize(glm::slerp(sourceRotationError_, identity, 1 - displayRetained));
    auto turretError = glm::normalize(glm::slerp(turretRotationError_, identity, 1 - displayRetained));
    metrics_.localPresentationFraction = std::clamp(accumulator_ + lead, 0., .1);
    present(dt, timing);
    game_->presentPrediction(
        metrics_.localPresentationFraction,
        {presentationError_ * displayRetained, glm::mat3_cast(sourceError), glm::mat3_cast(turretError)}, displayDt);
}
} // namespace Native::Net
