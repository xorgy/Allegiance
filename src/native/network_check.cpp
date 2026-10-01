#include "network_session.hpp"
#include "server_browser.hpp"
#include "server_host.hpp"
#include "public_directory.hpp"
#include "endpoint_resolver.hpp"
#include "network_codec.hpp"
#include "lobby_network_actions.hpp"
#include "../ClientCore/MissionChatCommands.h"
#include "../ClientCore/CarrierLoadout.h"
#include "../ClientCore/ShipCrew.h"
#include "../ClientCore/MissionLifecycle.h"
#include "test_reference/alliances.hpp"
#include "test_reference/weapon_selection.hpp"
#include <lz4.h>
#include "test_fixture.hpp"
#include "../Igc/weaponIGC.h"
#include "../Igc/magazineigc.h"
#include "../Igc/projectileIGC.h"
#include "../Igc/missileigc.h"
#include "../Igc/chaffigc.h"
#include "../Igc/treasureIGC.h"
#include <chrono>
#include <thread>
#include <cstdio>
#include <cmath>
#include <numeric>
#include <limits>
#include <new>
using namespace Native;
using NativeTest::require;
namespace {
constexpr uint32_t publicationRate = Net::snapshotRate;
template<class T> void rawRecordInitialization(const char* name){
    static_assert(Net::Record<T>);
    alignas(T) std::array<std::byte,sizeof(T)> a,b;a.fill(std::byte{0xa5});b.fill(std::byte{0x5a});
    // Default initialization runs each field initializer, without value-
    // initialization's possible whole-object zeroing masking padding holes.
    auto* x=::new(a.data()) T;auto* y=::new(b.data()) T;
    bool same=std::memcmp(x,y,sizeof(T))==0;x->~T();y->~T();
    if(!same)throw std::runtime_error(std::string("Indeterminate bytes in raw wire record ")+name);
}
void rawRecords(){
#define CHECK_RECORD(T) rawRecordInitialization<Net::T>(#T)
    CHECK_RECORD(Header);CHECK_RECORD(MessagePrefix);CHECK_RECORD(InputHeader);CHECK_RECORD(Hello);CHECK_RECORD(Session);CHECK_RECORD(Welcome);
    CHECK_RECORD(Input);CHECK_RECORD(Action);CHECK_RECORD(Configuration);CHECK_RECORD(BrowseQuery);CHECK_RECORD(PlayerQuery);CHECK_RECORD(ServerInfo);CHECK_RECORD(DirectoryInfo);CHECK_RECORD(CoreInfo);CHECK_RECORD(HostedGame);CHECK_RECORD(CreateGameRequest);CHECK_RECORD(GameCreated);CHECK_RECORD(Receipt);CHECK_RECORD(MissionPolicy);CHECK_RECORD(Command);CHECK_RECORD(Entity);
    CHECK_RECORD(ResultPlayer);CHECK_RECORD(ResultTeam);CHECK_RECORD(Part);CHECK_RECORD(Pilot);CHECK_RECORD(Team);CHECK_RECORD(Member);CHECK_RECORD(ShipIntel);CHECK_RECORD(Bucket);CHECK_RECORD(Notice);CHECK_RECORD(Event);
    CHECK_RECORD(SectorHighlight);CHECK_RECORD(Snapshot);CHECK_RECORD(ClockProbe);CHECK_RECORD(Delta);CHECK_RECORD(CopyRange);CHECK_RECORD(XorRange);CHECK_RECORD(Packed);
    CHECK_RECORD(Fragment);CHECK_RECORD(ArenaRecord);CHECK_RECORD(ArenaRef);CHECK_RECORD(ArenaManifest);CHECK_RECORD(ShipDamageState);
    CHECK_RECORD(PlayerConnectionInfo);CHECK_RECORD(ConnectionReport);CHECK_RECORD(Briefing);
    CHECK_RECORD(AuthorityRegistration);CHECK_RECORD(AuthorityRegistered);CHECK_RECORD(RegistryInfo);CHECK_RECORD(RegistryEndpoint);
#undef CHECK_RECORD
    SessionOptions named;named.name=std::string(64,'M');named.customMap="m2waspnova";Net::ServerInfo info;info.session=Net::session(named);Net::text(info.name,named.name);std::vector<std::byte> bytes;Net::append(bytes,info);
    auto decoded=Net::Reader(bytes).take<Net::ServerInfo>();require(std::string(decoded.name)==named.name && Net::options(decoded.session,0,"Pilot").name==named.name,"original 64-byte game name is truncated in browser/session records");
    require(Net::options(decoded.session,0,"Pilot").customMap==named.customMap,"file-backed map lost in direct session/browser records");
    for(std::string_view invalid:{"../map","abcdefghijklm"}){auto bad=decoded.session;Net::text(bad.customMap,invalid);bool rejected=false;try{Net::options(bad,0,"Pilot");}catch(const std::exception&){rejected=true;}require(rejected,"invalid map filename accepted from a wire record");}
    Net::Frame ballot;ballot.header.kind=Net::Kind::Snapshot;Net::Notice n;n.number=1;n.sender=65536;n.flags=Net::NoticeBallot;n.voice=int(Allegiance::BallotKind::Resign);n.color=0;n.time=1;Net::text(n.text,"Voter has proposed resigning.  ");ballot.notices.push_back(n);
    require(Net::frame(Net::packet(ballot)).notices.front().sender==65536,"ballot ID incorrectly limited to ship identity width");
    auto reject=[&](Net::Notice bad){auto invalid=ballot;invalid.notices[0]=bad;bool rejected=false;try{Net::frame(Net::packet(invalid));}catch(const std::exception&){rejected=true;}require(rejected,"malformed ballot notice accepted");};
    n.flags|=Net::NoticeHideBallot;reject(n);n.flags=Net::NoticeBallot;n.voice=0;reject(n);n.voice=int(Allegiance::BallotKind::Resign);n.color=6;reject(n);n.color=0;n.sender=-1;reject(n);
    n.sender=1;n.flags=Net::NoticeHideBallot;reject(n);n.flags=Net::NoticeBallot|Net::NoticeChat;reject(n);
    puts("Raw wire records: all bytes have deterministic defaults; no implicit padding in snapshot/session/admission/configuration records");
}

struct Match {
    std::filesystem::path art;
    Assets assets;
    QuickChatCatalog quickChat;
    std::vector<std::shared_ptr<ModelData>> owners;
    std::unordered_map<std::string, ModelHandle> models;
    ModelLoader loader;
    EffectLoader effects;
    std::unordered_map<std::string, EffectTexture> effectTextures;
    SessionOptions options;
    Loadouts presets;
    Net::Digest digest;
    std::unique_ptr<Game> authority, a, b;
    std::unique_ptr<Net::Server> server;
    std::unique_ptr<Net::Client> ca, cb;
    FlightInput ia, ib;
    uint16_t port;
    bool realtime{};
    std::chrono::steady_clock::time_point deadline;
    std::vector<double> times;
    Match(const std::filesystem::path &root, uint16_t p, bool real = false, bool lobby = false, bool visual = false,
          Scenario scenario = Scenario::Conquest, int specialAsteroids = 1, int maxImbalance = 32767, bool opponents = false,int initialMiners=0,bool defections=true,int teams=2,std::string story={},std::function<void(MissionParams&)> lifecycle={},int lives=32767,std::array<char,6> alliances={-1,-1,-1,-1,-1,-1})
        : art(root), assets(art), quickChat(assets), port(p), realtime(real) {
        loader = [this](const std::string &name, const std::string &) {
            if (auto i = models.find(name); i != models.end())
                return i->second;
            auto m = assets.model(name);
            owners.push_back(m);
            return models.emplace(name, ModelHandle{1, m->radius, m->frames, m->lights}).first->second;
        };
        if (visual)
            effects = [this](const std::string &name) {
                if (auto i = effectTextures.find(name); i != effectTextures.end())
                    return i->second;
                auto image = assets.image(name);
                return effectTextures.emplace(name, EffectTexture{uint32_t(effectTextures.size() + 1), image->width,
                    image->height, int(std::max(1u, image->width / image->height))}).first->second;
            };
        options.scenario = scenario;
        options.enemies = 0;
        options.startInHangar = true;
        options.rules.miners = initialMiners;
        options.rules.lives = lives;
        options.rules.teams = teams;
        options.alliances=alliances;
        options.story=std::move(story);
        options.rules.startingMoney = 9;
        options.rules.showMap = true;
        options.rules.showHome = true;
        options.rules.homeSpecial = specialAsteroids;
        options.seed = 47;
        options.rules.startCountdown = 1;
        options.rules.maxImbalance=maxImbalance; // Most protocol fixtures deliberately permit all peers on one side.
        options.rules.allowDefections=defections;
        digest = Net::contentDigest(art);
        authority = std::make_unique<Game>(art, loader, 0, 0, options, EffectLoader{}, GameRole::Authority);
        authority->quickChats(quickChat);
        if(lifecycle){auto params=*authority->core()->GetMissionParams();lifecycle(params);authority->core()->SetMissionParams(&params);}
        server = std::make_unique<Net::Server>(*authority, options, digest, port, lobby,std::string{},"Native test server");
        ca = client("Alpha", a);
        cb = client("Bravo", b,opponents?1:0);
        ia.active = ib.active = false;
        deadline = std::chrono::steady_clock::now();
    }
    std::unique_ptr<Net::Client> client(const char *name, std::unique_ptr<Game> &game,int side=0) {
        return std::make_unique<Net::Client>(
            "127.0.0.1:" + std::to_string(port), digest, name, side, [this, &game](const SessionOptions &s) -> Game & {
                game = std::make_unique<Game>(art, loader, 0, 0, s, effects, GameRole::Replica);
                game->loadoutPresets(presets);
                game->quickChats(quickChat);
                return *game;
            });
    }
    void tick(int n) {
        for (int j = 0; j < n; ++j) {
            ca->step(ia, 1. / Net::tickRate);
            if(cb)cb->step(ib, 1. / Net::tickRate);
            server->poll();
            auto start = std::chrono::steady_clock::now();
            server->step();
            times.push_back(
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
            ca->poll();
            if(cb)cb->poll();
            if (ca->closed() || (cb && cb->closed()))
                throw std::runtime_error("Connection closed: " + ca->error() + " / " + (cb?cb->error():"departed"));
            if (realtime) {
                deadline += std::chrono::nanoseconds(1000000000 / Net::tickRate);
                std::this_thread::sleep_until(deadline);
            } else
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    template <class Predicate> void wait(Predicate predicate, const char *what, int maxTicks = 1200) {
        for (int i = 0; i < maxTicks && !predicate(); ++i)
            tick(1);
        require(predicate(), what);
    }
    void join() {
        tick(realtime ? 240 : 120);
        require(ca->ready() && cb->ready() && server->clients() == 2, "two authenticated GNS clients join");
        require(a->state().docked && b->state().docked, "replica bootstrap hangar");
        require(a->state().shipId != b->state().shipId, "separate authenticated seats");
        if(!a->state().networkLobby){
            std::optional<PurchaseResult> alpha,bravo;
            wait([&]{if(!alpha)alpha=ca->takePurchase();if(!bravo)bravo=cb->takePurchase();return alpha && bravo;},"original mission-entry default purchases complete");
            require(alpha->accepted && alpha->complete && bravo->accepted && bravo->complete,"mission-entry defaults are affordable and complete");
            require(ca->takeDockRefitStarted() && cb->takeDockRefitStarted(),"mission entry uses the original purchase lifecycle");
        }
        if (options.scenario == Scenario::Conquest)
            require(a->state().money == authority->pilotBalance(a->state().shipId), "authoritative economy bootstrap");
    }
    void report(const char *name) {
        std::sort(times.begin(), times.end());
        printf("%s (%u Hz snapshots): %.1f/%.1f KiB received, %.3f/%.3f m maximum continuous correction, %.3f ms server p99, %.1f/%.1f "
               "ms RTT, %llu/%llu snapshots\n",
               name, publicationRate, ca->metrics().bytesReceived / 1024., cb->metrics().bytesReceived / 1024.,
               ca->metrics().correctionMax, cb->metrics().correctionMax, times[size_t(.99 * (times.size() - 1))],
               ca->metrics().rtt, cb->metrics().rtt, (unsigned long long)ca->metrics().snapshots,
               (unsigned long long)cb->metrics().snapshots);
        printf("  upstream %.1f/%.1f KiB; acknowledged notice/event entries omitted %llu/%llu\n",
               ca->metrics().bytesSent / 1024., cb->metrics().bytesSent / 1024.,
               (unsigned long long)server->metrics().noticesOmitted,
               (unsigned long long)server->metrics().eventsOmitted);
        fflush(stdout);
    }
};
void missionLifecycle(const std::filesystem::path& art){
    auto ready=[](Net::Client& client,bool value){Net::Action action;action.op=Net::Operation::Ready;action.args[0]=value;require(client.action(action),"lifecycle readiness queued");};
    auto end=[](Match& m){auto* core=m.authority->core();auto* base=core->GetSide(1)->GetStations()->first()->data();core->GetIgcSite()->KillStationEvent(base,nullptr,1);m.authority->step({},0);require(m.authority->state().complete,"lifecycle conquest victory fixture");};
    auto count=[](const Game& game,std::string_view text){const auto expected="HQ (all): "+std::string(text);return std::count_if(game.state().messages.begin(),game.state().messages.end(),[&](const auto& notice){return notice.chat && !notice.fromPlayer && notice.text==expected;});};
    {
        Match m(art,27710,false,true,false,Scenario::Conquest,1,32767,true,0,true,2,{},[](auto& p){p.bAllowEmptyTeams=false;});m.join();
        ready(*m.cb,false);m.wait([&]{return !m.b->state().networkReady;},"opposing pilot becomes AFK before enabling auto-start");
        auto params=*m.authority->core()->GetMissionParams();params.bAutoStart=true;m.authority->core()->SetMissionParams(&params);
        ready(*m.ca,true);m.tick(30);require(m.a->state().networkLobby && m.authority->core()->GetMissionStage()==STAGE_NOTSTARTED,"auto-start ignores an unready opposing side");
        require((m.a->state().lobbyFlags&Allegiance::LobbyAutoStart) && m.a->core()->GetMissionParams()->bAutoStart && m.b->core()->GetMissionParams()->bAutoStart,"automatic-start authority policy is absent from lobby/IGC replicas");
        ready(*m.cb,true);m.wait([&]{return !m.a->state().networkLobby && !m.b->state().networkLobby;},"all-ready starts without a host Start action");
        require(m.a->core()->GetMissionParams()->nMinPlayersPerTeam==1 && !m.a->core()->GetMissionParams()->bAllowEmptyTeams,"original minimum team policy does not reach the replica");
        auto match=m.ca->match();end(m);m.wait([&]{return m.ca->match()>match && m.a->state().networkLobby && m.b->state().networkLobby;},"auto-start round returns to its original team lobby");
        require(m.authority->core()->GetMissionParams()->bAutoStart && !m.authority->core()->GetMissionParams()->bAllowEmptyTeams && m.a->state().endgame.frozen,"restart loses authority policy or previous scores");
        m.tick(30);require(m.authority->core()->GetMissionStage()==STAGE_NOTSTARTED,"restart carries ready state into an unsolicited next game");
        ready(*m.ca,true);ready(*m.cb,true);m.wait([&]{return !m.a->state().networkLobby && !m.b->state().networkLobby;},"preserved auto-start begins the second round");
    }
    {
        Match m(art,27711,false,true,false,Scenario::Conquest,1,32767,true,0,true,2,{},[](auto& p){p.bAutoRestart=true;p.bAllowEmptyTeams=false;p.fStartCountdown=45;p.fRestartCountdown=37;});m.join();
        ready(*m.ca,false);ready(*m.cb,false);m.wait([&]{return !m.a->state().networkReady && !m.b->state().networkReady;},"both automatic-restart pilots become AFK");
        constexpr std::string_view warning="Warning: Game will be canceled if all teams are not ready in 30 seconds.";
        m.tick(20*Net::tickRate);require(count(*m.a,warning)==1 && count(*m.b,warning)==1 && m.server->tick()==0,"original restart warning repeats, is missing, or advances the parked simulation");
        m.wait([&]{return m.a->state().missionStage==STAGE_STARTING && m.b->state().missionStage==STAGE_STARTING;},"auto-restart enters its original fifteen-second briefing");
        require((m.a->state().forceReady&3)==3 && (m.a->state().readySides&3)==3 && !m.a->state().networkReady && !m.b->state().networkReady,"force-ready changes individual AFK or fails to override it");
        int before=m.a->state().networkCountdown;ready(*m.ca,false);m.tick(12);
        require(m.a->state().missionStage==STAGE_STARTING && m.a->state().networkCountdown<=before,"an AFK update extends or cancels the scheduled restart");
        m.wait([&]{return !m.a->state().networkLobby && !m.b->state().networkLobby;},"auto-restart launches on its original deadline");
        auto match=m.ca->match();end(m);m.wait([&]{return m.ca->match()>match && m.a->state().networkLobby && m.b->state().networkLobby;},"automatic next-round delay begins");
        require(m.a->state().missionStage==STAGE_NOTSTARTED && m.a->state().networkCountdown>30 && m.a->state().networkCountdown<=37 && m.a->state().endgame.frozen,"auto-restart skips its delay, discards scores or uses the first-start interval");
        Settings settings;Frontend ui(settings);ui.civilizations(Game::civilizations(art));ui.originalInterface(m.assets);ui.setPlaying(true);ui.networkSession(m.a->sessionOptions());ui.open(Screen::Results,&m.a->state());
        ui.sync(m.a->state());require(ui.screen()==Screen::Results,"score screen closes during the automatic restart delay");
        m.tick(23*Net::tickRate);ui.sync(m.a->state());require(ui.screen()==Screen::RemoteLobby && ui.keyboardPopup() && !ui.wantsText(),"automatic briefing does not open the original team/briefing pane at fifteen seconds");
    }
    {
        Match m(art,27712,false,true,false,Scenario::Conquest,1,32767,false,0,true,2,{},[](auto& p){p.bAutoRestart=true;p.bAllowEmptyTeams=false;p.fStartCountdown=45;});m.join();
        m.tick(46*Net::tickRate);
        require(m.a->state().missionStage==STAGE_OVER && m.b->state().missionStage==STAGE_OVER && m.server->tick()==0 && !(m.a->state().readySides&2),"empty opposing side starts or is marked ready despite the minimum player rule");
        require(count(*m.a,Allegiance::MissionRestartCanceledMessage)==1 && count(*m.b,Allegiance::MissionRestartCanceledMessage)==1,"canceled automatic game does not deliver its original message once");
        Settings settings;Frontend ui(settings);ui.originalInterface(m.assets);ui.setPlaying(true);ui.networkSession(m.a->sessionOptions());ui.open(Screen::RemoteLobby,&m.a->state());
        require(std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.name=="textStatus" && c.text=="GAME CANCELED";}),"canceled lobby omits original status");
        require(std::any_of(ui.view().items.begin(),ui.view().items.end(),[](const auto& i){return i.action.op==MenuOp::NetworkStart && !i.enabled;}),"canceled lobby exposes an enabled Start control");
        ready(*m.ca,true);m.tick(18);require(m.a->state().missionStage==STAGE_OVER && count(*m.a,Allegiance::MissionRestartCanceledMessage)==1,"late ready revives a canceled game or repeats cancellation");
    }
    for(bool shutdown:{false,true}){
        Match m(art,shutdown?27713:27714,false,false,false,Scenario::Conquest,1,32767,true,0,true,2,{},[&](auto& p){p.bAllowRestart=shutdown;});m.join();
        auto match=m.ca->match();
        if(shutdown){
            m.server->pause(true);m.a->launch();m.wait([&]{return !m.a->state().docked && (m.a->state().lobbyFlags&Allegiance::LobbyServerPaused);},"server shutdown state is reported without stopping flight");
            auto tick=m.server->tick();m.ia.throttle=1;m.tick(60);require(m.server->tick()>=tick+60 && m.a->state().speed>1,"server shutdown pause freezes live simulation");
            Net::ServerBrowser browser(m.digest);browser.add("127.0.0.1:"+std::to_string(m.port));
            m.wait([&]{browser.poll();return !browser.listings().empty();},"paused server browser query");require(!(browser.listings()[0].info.flags&Net::ServerJoinable),"paused server advertises admission");browser.stop();
            std::unique_ptr<Game> lateGame;auto late=m.client("Late pilot",lateGame);
            m.wait([&]{late->poll();return late->closed();},"shutdown rejects a new admission");require(late->error()==Allegiance::MissionServerPausedMessage && !lateGame,"shutdown admission loses its original diagnostic or allocates a seat");
        }
        end(m);m.wait([&]{return m.a->state().complete && m.b->state().complete && m.a->state().missionStage==STAGE_OVER;},"nonrestartable/paused game freezes results without preparing another round");
        require(m.ca->match()==match && m.a->state().endgame.frozen,"ended match unexpectedly resets or loses scores");
        auto* host=m.a->state().networkHost?m.ca.get():m.cb.get();Net::Action restart;restart.op=Net::Operation::NewRound;auto rejected=host->metrics().rejected;require(host->action(restart),"forbidden manual restart request queued");m.wait([&]{return host->metrics().rejected>rejected;},"forbidden manual restart rejected");
        require(m.ca->match()==match,"manual restart bypasses shutdown/allow-restart policy");
        if(shutdown){m.server->pause(false);m.wait([&]{return m.ca->match()>match && m.a->state().networkLobby && m.b->state().networkLobby;},"unpause prepares a restartable finished mission");require(m.a->state().endgame.frozen && !(m.a->state().lobbyFlags&Allegiance::LobbyServerPaused),"unpause loses previous scores or shutdown status persists");}
    }
    puts("Encrypted mission lifecycle: all-ready auto-start, preserved next-round policy, auto-restart delay/warning/forced readiness, briefing deadline, minimum-size cancellation, score-screen transitions, shutdown admission/draining and nonrestartable rounds passed");
}
void lobbyGrace(const std::filesystem::path& art){
    Match m(art,27721,false,true,false,Scenario::Conquest,1,32767,true);m.join();
    auto* host=m.a->state().networkHost?m.ca.get():m.cb.get();auto* other=host==m.ca.get()?m.cb.get():m.ca.get();
    auto hostGame=[&]()->Game&{return *(host==m.ca.get()?m.a:m.b);};
    auto otherGame=[&]()->Game&{return *(other==m.ca.get()?m.a:m.b);};
    auto action=[](Net::Client& client,Net::Operation op,int a=0,int b=0){Net::Action record{};record.op=op;record.args[0]=a;record.args[1]=b;require(client.action(record),"lobby grace action queued");};
    auto options=hostGame().sessionOptions();options.rules.startCountdown=30;auto match=host->match();require(host->configure(options),"lobby grace countdown configured");
    m.wait([&]{return m.ca->ready() && m.cb->ready() && host->match()>match;},"long countdown configuration completes");
    action(*host,Net::Operation::Lobby,int(LobbyAction::AutoAccept),0);m.wait([&]{return !(hostGame().state().autoAccept&(1u<<hostGame().state().playerSide));},"host faction requires approval");
    std::unique_ptr<Game> visitorGame;auto visitor=m.client("Waiting visitor",visitorGame,hostGame().state().playerSide);
    auto wait=[&](auto predicate,const char* what){m.wait([&]{visitor->poll();if(visitor->closed())throw std::runtime_error("Lobby visitor closed: "+visitor->error());return predicate();},what);};
    wait([&]{return visitor->ready() && visitorGame->state().networkPilots.size()==3;},"No Team visitor enters the original lobby");
    require(visitorGame->state().playerSide==noTeam,"grace fixture visitor was accepted to a faction");
    action(*host,Net::Operation::Ready,1);action(*other,Net::Operation::Ready,1);
    wait([&]{return hostGame().state().readySides==3;},"faction seats ready without the pending visitor");
    action(*host,Net::Operation::Start);wait([&]{return hostGame().state().missionStage==STAGE_STARTING;},"countdown begins before visitor disconnect");
    visitor->reconnect(); // Do not poll its replacement yet: retain a real No Team lease.
    m.wait([&]{return m.server->clients()==2;},"No Team visitor enters authenticated reconnect grace");m.tick(60);
    require(m.server->occupants()==3 && m.authority->core()->GetMissionStage()==STAGE_STARTING && hostGame().state().readySides==3 &&
        !(hostGame().state().lobbyFlags&Allegiance::LobbyReconnecting) && !Allegiance::LobbyReconnectingSides(hostGame().state().lobbyFlags),"No Team reconnect grace cancels the countdown or blocks faction readiness");
    action(*host,Net::Operation::Ready,0);m.wait([&]{return hostGame().state().missionStage==STAGE_NOTSTARTED && !hostGame().state().networkReady;},"actual faction readiness still cancels countdown");
    options=hostGame().sessionOptions();++options.seed;match=host->match();require(host->configure(options),"settings queued while a No Team visitor is dormant");
    m.wait([&]{return m.ca->ready() && m.cb->ready() && host->match()>match;},"No Team lease does not veto host settings");
    require(m.server->clients()==2 && m.server->occupants()==3,"configuration loses or duplicates the dormant visitor");
    wait([&]{return visitor->ready() && visitor->match()==host->match() && visitorGame->state().networkPilots.size()==3;},"No Team visitor resumes across world replacement");
    auto own=std::find_if(visitorGame->state().networkPilots.begin(),visitorGame->state().networkPilots.end(),[](const auto& p){return p.controlled;});
    require(visitorGame->state().playerSide==noTeam && own!=visitorGame->state().networkPilots.end() && own->name=="Waiting visitor" && own->requested==noTeamRequest && !own->ready,
        "resumed No Team visitor loses authentication/membership or carries old-round requests/readiness");
    action(*host,Net::Operation::Ready,1);action(*other,Net::Operation::Ready,1);wait([&]{return hostGame().state().readySides==3;},"configured faction seats ready independently of the visitor");
    action(*host,Net::Operation::Start);wait([&]{return hostGame().state().missionStage==STAGE_STARTING;},"second countdown begins");
    int missingSide=otherGame().state().playerSide,missingPilot=otherGame().state().shipId;other->reconnect();
    auto pump=[&](auto predicate,const char* what){for(int i=0;i<1200 && !predicate();++i){host->step({},1./Net::tickRate);m.server->poll();m.server->step();host->poll();visitor->poll();std::this_thread::sleep_for(std::chrono::milliseconds(2));}require(predicate(),what);};
    pump([&]{return m.server->clients()==2 && Allegiance::LobbyReconnectingSides(hostGame().state().lobbyFlags)==(1u<<missingSide);},"a faction pilot enters grace with a specific readiness mask");
    require(hostGame().state().missionStage==STAGE_NOTSTARTED && hostGame().state().readySides==(3u&~(1u<<missingSide)),"faction reconnect grace fails to cancel countdown or clears unrelated ready sides");
    Settings settings;Frontend ui(settings);ui.originalInterface(m.assets);ui.networkSession(hostGame().sessionOptions());ui.setPlaying(true);ui.open(Screen::RemoteLobby,&hostGame().state());
    auto blockedName=hostGame().state().teamScores[missingSide].name;std::transform(blockedName.begin(),blockedName.end(),blockedName.begin(),[](unsigned char c){return std::toupper(c);});
    auto text=[&](std::string_view name){auto i=std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.name==name;});require(i!=ui.view().artwork.end(),"original readiness label absent");return i->text;};
    require(text("textStatus")==blockedName && text("textStatus2")=="IS NOT READY","one reconnecting faction becomes an invented all-teams readiness failure");
    auto rejected=host->metrics().rejected;action(*host,Net::Operation::Start);pump([&]{return host->metrics().rejected>rejected;},"faction lease still blocks manual start");
    options=hostGame().sessionOptions();++options.seed;match=host->match();rejected=host->metrics().rejected;require(host->configure(options),"faction lease settings rejection queued");
    pump([&]{return host->metrics().rejected>rejected;},"faction lease still protects its world during settings");require(host->match()==match,"rejected settings replaced a dormant faction pilot's world");
    wait([&]{return other->ready() && m.server->clients()==3 && !Allegiance::LobbyReconnectingSides(hostGame().state().lobbyFlags) && hostGame().state().readySides==3;},"faction pilot resumes and restores readiness");
    require(otherGame().state().shipId==missingPilot,"faction grace changes original pilot identity");
    action(*host,Net::Operation::Start);wait([&]{return hostGame().state().missionStage==STAGE_STARTING;},"restored faction readiness permits the original countdown");
    puts("Encrypted lobby reconnect grace: neutral No Team disconnect/countdown/settings, retained authentication across world replacement, cleared old requests, faction-only readiness/status, protected dormant faction world and identity-preserving resume passed");
}
void wings(const std::filesystem::path& art) {
    Match m(art,27669,false,true);
    int unassigned=m.authority->joinNetworkPilot("Unassigned pose fixture",noTeam,false);
    auto* empty=m.authority->core()->GetShip(unassigned);require(empty && !empty->GetBaseHullType(),"Fresh No Team fixture unexpectedly has a hull");
    auto checkEmpty=[&](Net::Tick tick){auto frame=m.authority->networkFrame(unassigned,tick,0);auto canonical=Net::frame(Net::packet(frame));
        auto identity=std::find_if(canonical.entities.begin(),canonical.entities.end(),[&](const auto& e){return e.identity.type==OT_ship && e.identity.id==unassigned;});
        require(identity!=canonical.entities.end() && identity->motion.position.x==0 && identity->motion.velocity.x==0 && identity->hull==0 && identity->radius==0 && identity->signature==0 && identity->wing==NA,"No Team report reads or leaks uninitialized/previous physical state");};
    require(!empty->GetHitTest(),"Bare No Team fixture has a physical hit test");checkEmpty(1);
    // Physical setters require a hull/hit test. Also cover a departed pilot
    // whose allocated physical state is deliberately invalid for publication.
    empty->SetBaseHullType(m.authority->playerShip()->GetBaseHullType());
    float invalid=std::numeric_limits<float>::quiet_NaN();empty->SetPosition(Vector(invalid,invalid,invalid));empty->SetVelocity(Vector(invalid,invalid,invalid));checkEmpty(2);
    empty->SetPosition(Vector::GetZero());empty->SetVelocity(Vector::GetZero());
    m.authority->retireNetworkPilot(unassigned);m.join();
    auto* commander=m.a->state().commander?m.a.get():m.b.get();
    auto* pilot=commander==m.a.get()?m.b.get():m.a.get();
    auto* peer=pilot==m.a.get()?m.ca.get():m.cb.get();
    int captain=commander->state().shipId,id=pilot->state().shipId;
    auto memberWing=[&](const Game& game,int who){for(const auto& row:game.state().networkPilots)if(row.id==who)return row.wing;return -99;};
    require(commander->state().wing==0 && pilot->state().wing==NA,"Original commander/member initial wing policy");
    require(pilot->setWing(id,7),"Pregame own wing queued");
    m.wait([&]{return pilot->state().wing==7 && memberWing(*commander,id)==7;},"Pregame wing changes reach both own ship and faction roster");
    require(commander->setWing(id,4),"Pregame commander assignment queued");
    m.wait([&]{return pilot->state().wing==4 && memberWing(*commander,id)==4;},"Commander assignment updates faction metadata");
    for(auto [who,wing]:std::initializer_list<std::pair<int,int>>{{captain,5},{id,10},{65536,3}}){
        auto rejected=peer->metrics().rejected;require(pilot->setWing(who,wing),"Invalid wing action delivered for authoritative validation");
        m.wait([&]{return peer->metrics().rejected>rejected;},"Unauthorized/out-of-range wing action rejected");
        require(m.authority->core()->GetShip(id)->GetWingID()==4 && m.authority->core()->GetShip(captain)->GetWingID()==0,"Rejected wing request mutates another assignment");
    }
    auto frame=m.authority->networkFrame(captain,m.server->tick()+100,0);
    std::vector<std::byte> retainedPayload;size_t payloadOffset=0;
    std::erase_if(frame.entities,[&](const auto& e){bool remove=e.identity.type==OT_ship && e.identity.id==id;
        if(!remove)retainedPayload.insert(retainedPayload.end(),frame.payload.begin()+payloadOffset,frame.payload.begin()+payloadOffset+e.payloadBytes);
        payloadOffset+=e.payloadBytes;return remove;});frame.payload=std::move(retainedPayload);
    std::erase_if(frame.parts,[&](const auto& p){return p.ship.type==OT_ship && p.ship.id==id;});
    for(const auto& row:commander->state().networkPilots){Net::Member p;p.id=row.id;p.side=row.team;p.flags=(row.ready?1:0)|(row.commander?4:0);Net::text(p.name,row.name);frame.members.push_back(p);}
    auto missing=std::make_unique<Game>(art,m.loader,0,0,m.options,EffectLoader{},GameRole::Replica);
    missing->applyNetworkFrame(Net::frame(Net::packet(frame)));
    require(!missing->core()->GetShip(id) && memberWing(*missing,id)==4,"Faction wing depends on a physical replica");
    auto rows=missing->playerPaneData().rows;auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& p){return p.id==id;});
    require(row!=rows.end() && row->wing==4,"F6 remote teammate loses authoritative wing metadata");
    auto account=std::find_if(frame.pilots.begin(),frame.pilots.end(),[&](const auto& p){return p.id==id;});
    require(account!=frame.pilots.end() && row->money==account->money && row->autoDonate==account->autoDonate,"F6 remote teammate loses known money/donation metadata without a physical replica");
    frame.pilots.erase(std::remove_if(frame.pilots.begin(),frame.pilots.end(),[&](const auto& p){return p.id==id;}),frame.pilots.end());
    missing->applyNetworkFrame(Net::frame(Net::packet(frame)));require(memberWing(*missing,id)==NA,"Retired private metadata remains assigned to a public pilot");
    rows=missing->playerPaneData().rows;row=std::find_if(rows.begin(),rows.end(),[&](const auto& p){return p.id==id;});
    require(row!=rows.end() && row->money==-1 && row->autoDonate==NA,"Retired account/donation metadata remains assigned to a public pilot");
    Net::Frame raw;raw.header.kind=Net::Kind::Snapshot;Net::Pilot valid;valid.id=id;valid.side=0;valid.wing=4;raw.pilots.push_back(valid);
    auto decoded=Net::frame(Net::packet(raw));require(std::memcmp(&decoded.pilots.front(),&valid,sizeof(valid))==0,"Faction pilot record fails exact round trip");
    for(int bad:{-2,10,32767}){raw.pilots[0].wing=bad;bool rejected=false;try{Net::frame(Net::packet(raw));}catch(const std::exception&){rejected=true;}require(rejected,"Invalid private wing accepted");}
    raw.pilots={valid,valid};bool duplicate=false;try{Net::frame(Net::packet(raw));}catch(const std::exception&){duplicate=true;}require(duplicate,"Duplicate private pilot accepted");
    std::unique_ptr<Game> opposing;auto opponent=m.client("Opponent",opposing,1);
    m.wait([&]{opponent->poll();return opponent->ready() && commander->state().networkPilots.size()==3;},"Opponent wing fixture joins");
    require(opposing->state().wing==0 && memberWing(*commander,opposing->state().shipId)==NA,"Opponent commander default or faction wing privacy differs");
    require(opposing->setWing(opposing->state().shipId,9),"Opponent private assignment queued");
    m.wait([&]{opponent->poll();return opposing->state().wing==9;},"Opponent own wing replicated");
    m.authority->core()->GetSide(0)->SetAllies(0);m.authority->core()->GetSide(1)->SetAllies(0);
    auto allies=m.authority->networkFrame(captain,m.server->tick()+101,0);
    auto ally=std::find_if(allies.pilots.begin(),allies.pilots.end(),[&](const auto& p){return p.id==opposing->state().shipId;});
    require(ally!=allies.pilots.end() && ally->wing==NA,"Allied report exposes private wing assignment");
    for(const auto& e:allies.entities)if(e.identity.type==OT_ship && e.side!=0)require(e.wing==NA,"Visible allied ship exposes private wing assignment");
    require(commander->setWing(captain,3),"Outgoing commander explicit wing queued");
    m.wait([&]{opponent->poll();return commander->state().wing==3;},"Outgoing commander's chosen wing replicates");
    require(commander->makeTeamLeader(id),"Pregame leadership transfer queued");
    m.wait([&]{opponent->poll();return pilot->state().commander && !commander->state().commander && pilot->state().wing==0;},"New leader forced into Command wing");
    require(m.authority->core()->GetShip(captain)->GetWingID()==3,"Authority resets outgoing commander's wing");
    // Mimic WinTrek's explicit post-transfer saved-wing restoration.
    require(commander->setWing(captain,7),"Outgoing pilot restores its own saved selection");
    m.wait([&]{opponent->poll();return commander->state().wing==7;},"Outgoing saved selection accepted");
    Net::Action team;team.op=Net::Operation::Team;team.args[0]=1;
    auto* former=commander==m.a.get()?m.ca.get():m.cb.get();require(former->action(team),"Faction transition queued");
    m.wait([&]{opponent->poll();return commander->state().playerSide==1 && commander->state().wing==7;},"Ordinary faction entry preserves wing rather than forcing Command");
    team.args[0]=noTeam;require(former->action(team),"No Team wing transition queued");
    m.wait([&]{opponent->poll();return commander->state().playerSide==noTeam;},"No Team transition completes");
    require(commander->state().wing==NA && memberWing(*commander,captain)==NA,"No Team state/roster exposes remembered private wing");
    auto rejected=former->metrics().rejected;require(commander->setWing(captain,4),"No Team wing request delivered");
    m.wait([&]{opponent->poll();return former->metrics().rejected>rejected;},"No Team assignment rejected");
    team.args[0]=0;require(former->action(team),"Return to faction queued");
    m.wait([&]{opponent->poll();return commander->state().playerSide==0 && commander->state().wing==7;},"Returning member retains saved wing");
    former->reconnect();m.wait([&]{opponent->poll();return former->ready();},"Wing authenticated resume completes");
    require(commander->state().shipId==captain && commander->state().wing==7,"Resume overwrites wing or pilot identity");
    puts("Network wings: pregame ownership/commander changes, invalid requests, missing physical replicas, retired metadata, private/allied visibility, side transitions and authenticated resume passed");
}
void purchases(const std::filesystem::path& art) {
    Match m(art,27665);
    auto initial=m.authority->previewLoadout({},false,-1,true).loadout;
    std::erase_if(initial.parts,[](auto& part){return part.mount<0 && part.mount!=-5;});
    m.presets.remember(0,initial);m.join();
    require(m.a->previewLoadout().equivalent && m.b->previewLoadout().equivalent,"opening the editor after mission entry stages an unsolicited refit");
    auto initialActual=m.a->currentLoadout();require(std::count_if(initialActual.parts.begin(),initialActual.parts.end(),[](auto& part){return part.mount<0;})==1,"mission entry ignores custom-zero cargo layout");
    auto* ship=m.authority->core()->GetShip(m.a->state().shipId);
    TRef<IstationIGC> station=ship->GetStation();
    auto* pack=m.authority->core()->GetAmmoPack();
    auto wanted=m.a->currentLoadout();
    std::erase_if(wanted.parts,[](auto& part){return part.mount==-5;});
    wanted.parts.push_back({pack->GetObjectID(),-5,3});
    int credits=m.a->state().money+ship->GetValue();
    require(m.a->purchaseLoadout(wanted).accepted,"exact loadout purchase queued");
    // Process reliable actions without advancing simulation/publication. A
    // receipt alone must not expose a completion against the old replica.
    for(int i=0;i<500;++i){m.server->poll();m.ca->poll();m.cb->poll();
        require(!m.ca->takePurchase(),"purchase completion precedes its canonical authority report");
        auto* part=ship->GetMountedPart(NA,-5);if(part && part->GetPartType()==pack && part->GetAmount()==3)break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    require(ship->GetMountedPart(NA,-5) && ship->GetMountedPart(NA,-5)->GetAmount()==3,"authority preserves requested cargo quantity");
    std::optional<PurchaseResult> result;uint64_t purchaseAction{},expectedPurchase=m.ca->lastActionNumber();
    auto completed=[&]{if(!result)result=m.ca->takePurchase(&purchaseAction);return bool(result);};
    m.wait(completed,"purchase completion reconciles with the next report");
    require(result->accepted && result->complete && !result->launched && m.a->state().docked,"successful buy remains in hangar");
    require(purchaseAction==expectedPurchase,"successful purchase loses its original action identity");
    require(m.a->playerShip()->GetMountedPart(NA,-5)->GetAmount()==3 && m.a->state().money+m.a->shipValue()==credits,"completion exposes canonical quantity and conserved credits");
    auto live=m.a->currentLoadout();auto invalid=live;invalid.parts.push_back(invalid.parts.front());
    result.reset();
    require(m.a->purchaseLoadout(invalid,true).accepted,"invalid launch purchase queued for authority validation");
    expectedPurchase=m.ca->lastActionNumber();
    m.wait(completed,"rejected purchase completes");
    require(!result->accepted && m.a->state().docked && m.a->currentLoadout()==live && ship->GetStation()==station,"invalid purchase preserves inventory and never launches");
    require(purchaseAction==expectedPurchase,"rejected purchase loses its action identity");
    uint64_t rejectedAction{};auto rejection=m.ca->takeRejection(&rejectedAction);
    require(rejectedAction==expectedPurchase && rejection=="This loadout cannot be purchased here.","purchase failure loses its modal action or original diagnostic");
    require(m.ca->takeRejection(&rejectedAction).empty() && !rejectedAction,"consumed purchase failure retains stale modal identity");
    auto* gun=ship->GetMountedPart(ET_Weapon,0)->GetPartType();
    auto clone=[&](int id,int price,bool available){
        auto data=*static_cast<const DataWeaponTypeIGC*>(gun->GetData());data.partID=PartID(id);data.successorPartID=NA;data.price=price;data.groupID=0;
        if(available)data.ttbmRequired.ClearAll();else data.ttbmRequired.SetAll();
        for(auto* game:{m.authority.get(),m.a.get()}){auto* type=game->core()->CreateObject(game->core()->GetLastUpdate(),OT_partType,&data,sizeof(data));require(type,"purchase fixture part");type->Release();}
    };
    clone(31020,0,false);clone(31021,100000000,true);
    auto denied=live;for(auto& part:denied.parts)if(part.mount==0 && m.a->core()->GetPartType(part.id)->GetEquipmentType()==ET_Weapon)part.id=31021;
    result.reset();
    require(m.a->purchaseLoadout(denied,true).accepted,"unaffordable purchase queued");
    m.wait(completed,"unaffordable purchase rejected");
    require(!result->accepted && ship->GetStation()==station && m.a->currentLoadout()==live,"unaffordable purchase leaves the ship and launch state untouched");
    auto partial=live;for(auto& part:partial.parts)if(part.mount==0 && m.a->core()->GetPartType(part.id)->GetEquipmentType()==ET_Weapon)part.id=31020;
    require(!station->CanBuy(m.authority->core()->GetPartType(31020)),"unavailable part fixture");
    auto* side=ship->GetSide();auto oldTech=side->GetDevelopmentTechs();TechTreeBitMask allTech;allTech.SetAll();side->SetDevelopmentTechs(allTech);
    m.wait([&]{return m.a->playerShip()->GetStation()->CanBuy(m.a->core()->GetPartType(31020));},"temporary stock availability reaches the staging client");
    result.reset();
    require(m.a->purchaseLoadout(partial,true).accepted,"available purchase and launch queued");
    // No authority poll occurs between queueing and withdrawal. Stock becomes
    // unavailable after the client sends its intent, before server execution.
    side->SetDevelopmentTechs(oldTech);
    m.wait(completed,"pending purchase completes after authoritative stock withdrawal");
    require(result->accepted && !result->complete && !result->launched && m.a->state().docked && ship->GetStation()==station,"partial equipment purchase blocks launch");
    require(!ship->GetMountedPart(ET_Weapon,0) && !m.a->playerShip()->GetMountedPart(ET_Weapon,0),"partial completion exposes the actual fitted ship");
    // Lose the receipt after authority execution, then resume. The retained
    // request must obtain the cached result rather than disappearing at ack.
    wanted=live;for(auto& part:wanted.parts)if(part.mount==-5)part.amount=4;
    result.reset();
    require(m.a->purchaseLoadout(wanted).accepted,"resume purchase queued");
    expectedPurchase=m.ca->lastActionNumber();
    for(int i=0;i<500 && ship->GetMountedPart(NA,-5)->GetAmount()!=4;++i){m.server->poll();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    require(ship->GetMountedPart(NA,-5)->GetAmount()==4,"purchase executed before receipt loss");
    m.ca->reconnect();m.wait([&]{return m.ca->ready() && completed();},"cached purchase completion survives reconnect");
    require(result->complete && !result->launched && m.a->playerShip()->GetMountedPart(NA,-5)->GetAmount()==4,"resumed receipt has canonical equipment");
    require(purchaseAction==expectedPurchase,"replayed purchase changes its original action identity");
    result.reset();require(m.a->purchaseLoadout(m.a->currentLoadout(),true).accepted,"single atomic purchase/launch intent queued");
    m.wait(completed,"atomic purchase launches");
    require(result->accepted && result->complete && result->launched && !m.a->state().docked && !ship->GetStation(),"launch completion includes authoritative flight state");
    require(m.a->state().money+m.a->shipValue()==credits,"buy, partial purchase and replay conserve wallet plus equipment");
    require(m.ca->takePurchase()==std::nullopt,"no leftover purchase completion before docking");
    auto* core=m.authority->core();TRef<IstationIGC> home=core->GetSide(ship->GetSide()->GetObjectID())->GetStations()->first()->data();
    auto* lost=ship->GetMountedPart(NA,-5);require(lost,"network dock restoration cargo fixture");int lostType=lost->GetPartType()->GetObjectID();lost->Terminate();
    require(core->GetIgcSite()->DockWithStationEvent(ship,home),"authority docking callback");
    result.reset();m.wait(completed,"automatic dock restoration completes after canonical report");
    require(m.ca->takeDockRefitStarted() && result->accepted && result->complete && !result->launched && m.a->state().docked,"automatic dock restoration does not use original purchase lifecycle");
    require(ship->GetMountedPart(NA,-5) && ship->GetMountedPart(NA,-5)->GetPartType()->GetObjectID()==lostType,"automatic dock restoration forgets the launched cargo");
    int restoredHull=ship->GetBaseHullType()->GetObjectID();int restoredMoney=m.authority->pilotBalance(ship->GetObjectID());
    m.a->launch();m.wait([&]{return !m.a->state().docked;},"launch restored ship before fatal recovery");
    auto position=ship->GetPosition();core->GetIgcSite()->KillShipEvent(core->GetLastUpdate(),ship,nullptr,-1,position,position);
    require(ship->GetStation() && ship->GetBaseHullType()->HasCapability(c_habmLifepod),"authority must not pre-equip a recovered player");
    result.reset();m.wait(completed,"automatic fatal recovery refit completes");
    require(m.ca->takeDockRefitStarted()==purchaseAction && purchaseAction && result->accepted && result->complete && !result->launched && m.a->currentLoadout().hull==restoredHull && !m.a->state().pod,"fatal recovery loses remembered hull, launches it or drops its refit action identity");
    require(m.authority->pilotBalance(ship->GetObjectID())==restoredMoney,"fatal recovery minted credits");
    m.tick(18);require(!m.ca->takePurchase() && !m.ca->takeDockRefitStarted(),"steady docked reports repeatedly purchase equipment");
    std::vector<char> hullData(ship->GetBaseHullType()->Export(nullptr));ship->GetBaseHullType()->Export(hullData.data());
    auto* unavailableHull=reinterpret_cast<DataHullTypeIGC*>(hullData.data());unavailableHull->hullID=31030;unavailableHull->successorHullID=NA;unavailableHull->ttbmRequired.SetAll();
    for(auto* game:{m.authority.get(),m.a.get(),m.b.get()}){auto* type=game->core()->CreateObject(game->core()->GetLastUpdate(),OT_hullType,hullData.data(),int(hullData.size()));require(type,"teleport unavailable hull fixture");type->Release();}
    ship->SetBaseHullType(core->GetHullType(31030));
    DataStationIGC stationData{};require(home->Export(&stationData)==sizeof(stationData),"station destination export");stationData.stationID=30000;stationData.position+=Vector(2000,0,0);std::snprintf(stationData.name,sizeof(stationData.name),"Refit destination");
    auto* destination=static_cast<IstationIGC*>(core->CreateObject(core->GetLastUpdate(),OT_station,&stationData,sizeof(stationData)));require(destination,"teleport destination fixture");destination->Release();
    require(!destination->CanBuy(ship->GetBaseHullType()),"teleport must exercise hull replacement");m.tick(18);
    int teleportCredits=m.a->state().money+ship->GetValue();
    require(m.a->teleportStation(destination->GetObjectID()),"unavailable hull teleport queued");
    auto teleportAction=m.ca->lastActionNumber();
    result.reset();m.wait(completed,"teleport custom-zero replacement completes");
    auto transferred=m.a->currentLoadout();
    require(m.ca->takeDockRefitStarted() && result->accepted && result->complete && !result->launched && ship->GetStation()==destination && transferred.hull==initial.hull && !m.a->state().pod,"teleport replacement ignores original purchase lifecycle or custom-zero hull");
    require(std::count_if(transferred.parts.begin(),transferred.parts.end(),[](auto& part){return part.mount<0;})==1 && m.a->state().money+ship->GetValue()==teleportCredits,"teleport replacement loses custom-zero cargo or credits");
    auto teleport=m.ca->takeTransfer();require(teleport && *teleport==teleportAction,"station transfer loses its matching canonical completion");
    m.report("Purchases: custom-zero mission entry, exact quantities, causal completion, rejection, pending stock withdrawal, reconnect, atomic launch, dock/refit, recovery and teleport replacement");
}
void sectors(const std::filesystem::path& art) {
    Match m(art,27658);m.join();
    auto highlighted=[](const Game& game,int id){return std::any_of(game.state().sectors.begin(),game.state().sectors.end(),[&](auto& sector){return sector.id==id && sector.highlight;});};
    int home=m.a->playerShip()->GetStation()->GetCluster()->GetObjectID();
    require(m.a->state().commander && !m.b->state().commander,"sector highlight commander/wingman fixture");
    require(m.a->highlightSector(home),"commander highlights a known sector");
    m.wait([&]{return highlighted(*m.a,home) && highlighted(*m.b,home);},"commander highlight reaches both faction seats");
    m.authority->beginNetworkPublication();auto shared=m.authority->networkReport(m.a->state().shipId,m.server->tick(),0);
    auto sibling=m.authority->networkReport(m.b->state().shipId,m.server->tick(),0);
    require(shared.worlds.front().frame==sibling.worlds.front().frame && shared.worlds.front().frame->highlights.size()==1 && shared.worlds.front().frame->highlights.front().sector==home,"sector highlights use one shared faction arena");
    auto& source=*shared.worlds.front().frame;auto wire=source;wire.header.kind=Net::Kind::Snapshot;auto base=Net::packet(wire);wire.highlights.clear();auto removed=Net::packet(wire);
    require(Net::expandedAgainst(Net::compressed(removed,1,base),base)==removed,"sector highlight removal deltas roundtrip exactly");
    wire.highlights={{home},{home+1}};auto added=Net::packet(wire);wire.highlights={{home+1},{home}};auto reordered=Net::packet(wire);
    require(Net::expandedAgainst(Net::compressed(reordered,1,added),added)==reordered && Net::frame(reordered).highlights.size()==2,"sector highlight insertion/reordering deltas preserve records");
    require(m.b->highlightSector(home) && !highlighted(*m.b,home) && highlighted(*m.a,home),"wingman clears highlight locally without toggling commander state");
    m.tick(30);require(!highlighted(*m.b,home) && highlighted(*m.a,home),"live faction reports preserve local highlight dismissal");
    require(m.a->highlightSector(home),"commander clears highlight");m.wait([&]{return !highlighted(*m.a,home);},"commander clearing replicates");m.tick(12);
    require(m.a->highlightSector(home),"commander highlights again");m.wait([&]{return highlighted(*m.a,home) && highlighted(*m.b,home);},"new highlight clears the old local dismissal");
    require(!m.a->highlightSector(-1) && !m.a->highlightSector(65536),"invalid highlight IDs are rejected before transmission");
    require(m.a->sendChat(ChatChannel::FriendlySector,"Known sector channel",home) && !m.authority->sendChat(ChatChannel::FriendlySector,"Missing sector",SHRT_MAX),"explicit friendly-sector recipient and invalid-sector rejection");
    m.a->launch();m.b->launch();
    m.wait([&]{return !m.a->state().docked && !m.b->state().docked;},"sector fixture launches");
    m.wait([&]{return !m.a->state().cinematic;},"sector fixture launch cinematic finishes");
    auto* own=m.authority->core()->GetShip(m.a->state().shipId);
    auto* remote=m.authority->core()->GetShip(m.b->state().shipId);
    auto* physical=own->GetCluster();IclusterIGC *view=nullptr,*far=nullptr;
    for(auto* l=m.authority->core()->GetClusters()->first();l;l=l->next())if(l->data()!=physical){if(!view)view=l->data();else if(!far)far=l->data();}
    require(view && far,"three sector fixture");
    remote->SetCluster(far);remote->SetPosition(Vector(4000,0,0));remote->SetVelocity(Vector::GetZero());
    own->SetPosition(Vector(4000,0,0));own->SetVelocity(Vector::GetZero());
    m.ia.active=true;m.ia.throttle=.7f;m.ia.yaw=.07f;
    m.a->commandView(true);require(m.a->viewSector(view->GetObjectID()),"command view selects another sector");
    m.wait([&]{return m.authority->networkViewSector(own->GetObjectID())==view->GetObjectID();},"authority accepts remote view");
    m.tick(90);m.authority->beginNetworkPublication();
    auto report=m.authority->networkReport(own->GetObjectID(),m.server->tick(),0);
    auto entity=[](const Net::Frame& f,int id)->const Net::Entity& {
        for(const auto& e:f.entities)if(e.identity.type==OT_ship && e.identity.id==id)return e;
        throw std::runtime_error("Sector report ship missing");
    };
    require(report.worlds.size()==4,"remote command view retains physical sector dependency");
    const auto& map=*report.worlds.front().frame;
    require((entity(map,own->GetObjectID()).flags&Net::Strategic) &&
            (entity(map,remote->GetObjectID()).flags&Net::Strategic),"map-level ships are tagged strategic");
    require(entity(map,remote->GetObjectID()).partCount==0 && entity(map,remote->GetObjectID()).hull==0 &&
            entity(map,remote->GetObjectID()).fuel==0 && entity(map,remote->GetObjectID()).state==0,
            "strategic roster excludes continuously changing physical and equipment state");
    auto frame=m.authority->networkFrame(own->GetObjectID(),m.server->tick(),0,true);
    const auto& complete=entity(frame,own->GetObjectID());
    require(!(complete.flags&Net::Strategic) && std::memcmp(complete.controls,own->GetControls().jsValues,sizeof(complete.controls))==0,
            "command-view pilot retains current canonical controls");
    require(complete.motion.velocity.x==own->GetVelocity().x && complete.motion.velocity.y==own->GetVelocity().y &&
            complete.motion.forward.x==own->GetOrientation().GetForward().x,"command-view pilot retains exact physical motion");
    const auto& strategic=entity(frame,remote->GetObjectID());
    require((strategic.flags&Net::Strategic) && strategic.sector==far->GetObjectID() && strategic.motion.position.x==0,
            "remote map status retains sector without redundant precise pose");
    m.a->viewSector(far->GetObjectID());
    m.wait([&]{return m.authority->networkViewSector(own->GetObjectID())==far->GetObjectID();},"command view changes sector cohort");
    m.authority->beginNetworkPublication();
    auto viewed=m.authority->networkFrame(own->GetObjectID(),m.server->tick(),0,true);
    const auto& physicalRemote=entity(viewed,remote->GetObjectID());
    require(!(physicalRemote.flags&Net::Strategic) && physicalRemote.motion.position.x==remote->GetPosition().x,
            "viewed remote sector recovers its exact physical pose");
    m.a->commandView(false);m.tick(60);
    require(m.a->playerShip()->GetCluster()->GetObjectID()==physical->GetObjectID(),"leaving command view restores physical sector");
    int foreign=m.authority->joinNetworkPilot("Foreign highlight visibility",1,false);require(foreign>=0,"other faction highlight fixture");
    m.authority->beginNetworkPublication();auto other=m.authority->networkReport(foreign,m.server->tick(),0);
    require(std::all_of(other.worlds.begin(),other.worlds.end(),[](auto& world){return world.frame->highlights.empty();}),"private faction highlights leak to another faction");
    m.authority->retireNetworkPilot(foreign);
    m.report("Sectors: original map clicks/highlights, faction isolation, shared arena/deltas, global contacts, viewed/source state and command-view pilot prediction");
}
void commandFocus(const std::filesystem::path& art) {
    Match m(art,28779);m.join();m.b->launch();
    m.wait([&]{return !m.b->state().docked && !m.b->state().cinematic;},"command focus pilot launches");
    auto* core=m.authority->core();auto* remote=core->GetShip(m.b->state().shipId);
    auto* home=m.a->playerShip()->GetStation()->GetCluster();IclusterIGC* far=nullptr;
    for(auto* l=core->GetClusters()->first();l;l=l->next())if(l->data()->GetObjectID()!=home->GetObjectID()){far=l->data();break;}
    require(far,"command focus remote sector fixture");
    remote->SetCluster(far);remote->SetPosition(Vector(4300,-2500,600));remote->SetVelocity(Vector::GetZero());
    ControlData stopped;stopped.Reset();stopped.jsValues[c_axisThrottle]=-1;remote->SetControls(stopped);remote->SetStateM(0);
    int id=remote->GetObjectID(),sector=far->GetObjectID();CameraInput input;
    auto ready=[&]{auto rows=m.a->playerPaneData().rows;return std::any_of(rows.begin(),rows.end(),[&](const auto& r){return r.id==id && r.current && r.sector==sector;});};
    m.wait(ready,"remote pilot appears in faction sector status");
    auto placeholder=m.a->core()->GetShip(id)->GetPosition();
    require(m.a->playerShip()->GetStation() && (placeholder-remote->GetPosition()).LengthSquared()>10000,"remote pose is not sent with faction-only status");
    auto settle=[&]{for(int i=0;i<180;++i){m.tick(1);m.a->camera(input);}};
    auto center=[&]{auto c=m.a->camera(input);require(!c.animating,"command focus interpolation does not settle");return c.position+c.forward*10000.f;};
    require(m.a->takeMeTo(id),"Take Me To requests the remote pilot's sector");m.a->camera(input);settle();
    auto position=remote->GetPosition();
    require(m.a->state().sectorId==sector && m.a->state().commandUnit==id && glm::distance(center(),glm::vec3(position.x,position.y,position.z))<5,
        "Take Me To leaves a remote pilot centered on its strategic placeholder after precise sector state arrives");

    m.a->commandView(false);m.tick(30);m.a->camera(input);
    require(m.a->takeMeTo(id) && m.a->zoomCommandCamera(false),"pending remote focus accepts manual command zoom");m.a->camera(input);settle();
    auto c=m.a->camera(input);position=remote->GetPosition();
    require(glm::distance(c.position+c.forward*10000.f,glm::vec3(position.x,position.y,position.z))>100,
        "late Take Me To focus overwrites a newer manual camera adjustment");
    m.a->commandView(false);m.tick(30);m.a->camera(input);
    require(m.a->takeMeTo(id) && m.a->viewSector(home->GetObjectID()),"pending focus can be superseded by another sector");settle();
    auto* station=m.a->playerShip()->GetStation();c=m.a->camera(input);auto origin=station->GetPosition();
    auto delta=c.position-glm::vec3(origin.x,origin.y,origin.z);
    require(m.a->state().sectorId==home->GetObjectID() && glm::length(glm::cross(delta,c.forward))<1,
        "sector selection centers on zero or late focus pulls the camera out of the selected home base");

    m.a->commandView(false);m.tick(30);m.a->camera(input);
    int parentId=m.authority->joinNetworkPilot("Remote focus captain",0,false);auto* parent=core->GetShip(parentId);
    require(parent && parent->GetStation(),"remote crew focus parent fixture");parent->SetStation(nullptr);
    parent->SetCluster(far);parent->SetPosition(Vector(-3700,1100,900));parent->SetVelocity(Vector::GetZero());parent->SetControls(stopped);parent->SetStateM(0);
    auto* ownHull=remote->GetBaseHullType();remote->SetParentShip(parent);
    m.wait([&]{auto rows=m.a->playerPaneData().rows;return std::any_of(rows.begin(),rows.end(),[&](const auto& r){return r.id==id && r.parent==parentId;});},"remote crew status identifies its parent");
    require(m.a->takeMeTo(id),"Take Me To accepts an unseen remote observer");m.a->camera(input);settle();position=parent->GetPosition();
    require(glm::distance(center(),glm::vec3(position.x,position.y,position.z))<5,"remote crew focus uses the passenger's placeholder instead of its parent's physical position");
    remote->SetParentShip(nullptr);remote->SetBaseHullType(ownHull);remote->SetVelocity(Vector::GetZero());remote->SetControls(stopped);

    DataStationIGC stationData{};auto* ownBase=core->GetStation(m.a->state().stationId);
    require(ownBase && ownBase->Export(&stationData)==sizeof(stationData),"remote docked focus station export");
    stationData.stationID=30001;stationData.clusterID=sector;stationData.position=Vector(-2100,2300,400);
    auto* destination=static_cast<IstationIGC*>(core->CreateObject(core->GetLastUpdate(),OT_station,&stationData,sizeof(stationData)));
    require(destination,"remote docked focus station fixture");destination->Release();
    remote->SetStation(destination);
    m.wait([&]{return m.b->state().docked && m.b->state().stationId==destination->GetObjectID();},"remote observer returns to its own hull and docks");
    m.tick(30);require(m.a->takeMeTo(id),"Take Me To accepts a docked remote pilot");m.a->camera(input);settle();position=destination->GetPosition();
    require(glm::distance(center(),glm::vec3(position.x,position.y,position.z))<5,"docked Take Me To fails to focus the original station position");

    m.a->commandView(false);m.tick(30);m.a->camera(input);
    m.b->launch();m.wait([&]{return !m.b->state().docked && !m.b->state().cinematic;},"retirement fixture leaves its remote station");
    require(m.a->takeMeTo(id),"retired target pending focus fixture");m.cb.reset();
    m.wait([&]{return m.server->clients()==1;},"pending camera target disconnects before retiring its IGC pilot");
    if(core->GetShip(id))m.authority->retireNetworkPilot(id);settle();
    require(!core->GetShip(id) && std::isfinite(m.a->camera(input).position.x),"retiring a pending focus pilot leaves an unsafe camera target");
    puts("Command focus: faction-only remote status, precise report arrival, original framing, zoom/sector supersession, remote crew/docked targets and disconnected target retirement passed");
}
void compatibility(Match &m, bool content) {
    auto [socket, peer] = Net::connect("127.0.0.1:" + std::to_string(m.port));
    bool sent = false, closed = false;
    for (int i = 0; i < 2000 && !closed; ++i) {
        socket->receive();
        for (auto &event : socket->events()) {
            if (event.connected && !sent) {
                Net::Hello h{};
                h.build = Net::buildDigest();
                h.content = m.digest;
                Net::text(h.name, "Incompatible");
                (content ? h.content : h.build)[0] ^= 1;
                std::vector<std::byte> bytes;
                Net::append(bytes, Net::Header{Net::magic, Net::Kind::Hello});
                Net::append(bytes, h);
                require(socket->send(peer, bytes, Net::Lane::Control), "send incompatible handshake");
                sent = true;
            }
            if (!event.connected) {
                closed = true;
                require(event.reason.find(content ? "data differ" : "builds differ") != std::string::npos,
                        "explicit compatibility rejection");
            }
        }
        m.server->poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    require(sent && closed && m.server->clients() == 2, "incompatible build/content cannot acquire a pilot");
}
void publicScores(const std::filesystem::path& art) {
    Match m(art,27669,false,false,false,Scenario::Conquest,1,32767,true);m.join();int self=m.a->state().shipId,other=m.b->state().shipId;
    m.b->launch();m.wait([&]{return !m.b->state().docked;},"opponent launches in remote home sector");
    m.wait([&]{return !m.a->core()->GetShip(other);},"unseen airborne opponent retires its physical replica");
    auto* core=m.authority->core();auto* victim=core->GetShip(other);auto* killer=core->GetShip(self);
    core->GetIgcSite()->KillShipEvent(core->GetLastUpdate(),victim,killer,100,victim->GetPosition(),Vector::GetZero());
    m.wait([&]{auto& p=m.a->state().networkPilots;auto it=std::find_if(p.begin(),p.end(),[&](auto& p){return p.id==other;});
        return it!=p.end() && it->ejections==1 && m.a->state().kills==1;},"shared roster publishes authoritative kill and unseen ejection");
    require(!m.a->core()->GetShip(other),"public scores never reveal hidden equipment or poses");
    auto gameState=m.a->gameStateData();auto& rows=gameState.tiles.back().lines;
    require(rows[2].columns[1]=="Alpha" && rows[2].columns[2]=="1 kills" && rows[3].columns[1]=="Bravo" && rows[3].columns[3]=="1 ejects","Game State standings consume public scores without hidden ship entities");
    core->GetIgcSite()->KillShipEvent(core->GetLastUpdate(),victim,nullptr,100,victim->GetPosition(),Vector::GetZero());
    m.wait([&]{for(auto& p:m.a->state().networkPilots)if(p.id==other)return p.deaths==1;return false;},"pod death reaches unseen pilot's public score");
    auto pane=m.a->playerPaneData();auto row=std::find_if(pane.rows.begin(),pane.rows.end(),[&](auto& r){return r.id==other;});
    require(row!=pane.rows.end() && !row->current && row->deaths==1 && row->kills==0,"F6 scores update while pilot remains out of sight");
    Net::Frame roster;roster.header.kind=Net::Kind::Snapshot;roster.header.tick=91;
    Net::Member member;member.id=other;member.side=1;member.kills=7;member.deaths=2;member.ejections=3;roster.members.push_back(member);
    Net::ArenaEncoder encoder;for(int recipient=0;recipient<1024;++recipient)encoder.publishWorld(0x100000,91,1,roster);
    require(encoder.encodes()==1,"public score roster is encoded once and unchanged shared revision is reused");
    auto changedAt=core->GetLastUpdate();Net::Action team{};team.op=Net::Operation::Team;team.args[0]=1;
    require(m.ca->action(team),"scored faction transition queued");
    m.wait([&]{return m.a->state().playerSide==1 && m.a->state().kills==0;},"faction transition clears original public kill count");
    m.tick(120);auto finishedAt=core->GetLastUpdate();auto* base=core->GetSide(0)->GetStations()->first()->data();core->GetIgcSite()->KillStationEvent(base,nullptr,1);
    m.wait([&]{return m.a->state().endgame.frozen && m.b->state().endgame.frozen;},"faction-aware postgame arrives over encrypted shared reports");
    require(m.a->state().endgame==m.b->state().endgame,"faction change gives recipients different frozen results");
    auto& final=m.a->state().endgame;auto alpha=std::find_if(final.players.begin(),final.players.end(),[&](const auto& p){return p.id==self;});
    require(alpha!=final.players.end(),"encrypted faction result is missing its pilot");
    require(alpha->side==1 && alpha->kills==0 && alpha->score==0,("encrypted results carry old faction score: side="+std::to_string(alpha->side)+", kills="+std::to_string(alpha->kills)+", score="+std::to_string(alpha->score)).c_str());
    require(alpha->played>1.9f && alpha->played<float(finishedAt-changedAt)+.1f,("encrypted results carry old play time: played="+std::to_string(alpha->played)+", elapsed="+std::to_string(float(finishedAt-changedAt))).c_str());
    auto bravo=std::find_if(final.players.begin(),final.players.end(),[&](const auto& p){return p.id==other;});
    require(bravo!=final.players.end() && bravo->deaths==1 && bravo->ejections==1 && final.players.size()==2,"side changes lose retained lives or expose placeholder rows");
    Settings settings;Frontend ui(settings);ui.originalInterface(m.assets);ui.networkSession(m.options);ui.setPlaying(true);ui.open(Screen::Results,&m.a->state());
    require(std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.name=="resultPlayer/"+std::to_string(self)+"/32" && c.text=="Alpha" && c.color==m.a->state().teamScores[1].color;}),"original results table does not use the pilot's new faction color");
    puts("Encrypted public/F6 scores and faction-aware postgame: original resets, retained lives, identical shared results, actual table faction color and no placeholder rows passed");
}
void scoreReconnect(const std::filesystem::path& art){
    Match m(art,27694,false,false,false,Scenario::Conquest,1,32767,true);m.join();int alpha=m.a->state().shipId,bravo=m.b->state().shipId;
    m.cb->reconnect();
    // Let the old transport detach, but do not poll the replacement client:
    // it has not sent its authenticated Hello, leaving a real dormant seat.
    for(int tick=0;tick<120 && m.server->clients()!=1;++tick){m.server->poll();m.server->step();m.ca->poll();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    require(m.server->clients()==1 && m.authority->core()->GetShip(ShipID(bravo)),"reconnect fixture did not preserve a disconnected physical seat");
    for(int tick=0;tick<120;++tick){m.server->poll();m.server->step();m.ca->poll();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    m.wait([&]{return m.cb->ready() && m.server->clients()==2;},"authenticated dormant pilot resumes");
    require(m.b->state().shipId==bravo,"resume changes scored identity");
    m.tick(60);auto* core=m.authority->core();auto* base=core->GetSide(1)->GetStations()->first()->data();core->GetIgcSite()->KillStationEvent(base,nullptr,1);
    m.wait([&]{return m.a->state().endgame.frozen && m.b->state().endgame.frozen;},"reconnect score report delivered");
    const auto& report=m.a->state().endgame;require(report==m.b->state().endgame,"resumed pilot sees a different result report");
    auto row=[&](int id)->const Net::ResultPlayer&{auto p=std::find_if(report.players.begin(),report.players.end(),[&](const auto& p){return p.id==id;});require(p!=report.players.end(),"resumed score row missing");return *p;};
    float gap=row(alpha).played-row(bravo).played;
    require(gap>=1.95f && gap<2.5f,"encrypted reconnect grace counts as human play time");
    auto frozen=report;m.ca->reconnect();m.wait([&]{return m.ca->ready();},"postgame connection resumes");m.tick(12);
    require(m.a->state().endgame==frozen && m.b->state().endgame==frozen,"postgame resume mutates frozen timers");
    printf("Encrypted scoring resume: real dormant seat, unchanged identity, %.3f s excluded grace, shared frozen results and postgame reconnect passed\n",gap);
}
void consoleRoutes(const std::filesystem::path& art) {
    Match m(art,27668);m.join();int sender=m.a->state().shipId,recipient=m.b->state().shipId;
    auto* source=m.authority->core()->GetShip(ShipID(sender));auto* home=source->GetStation();require(home,"console route home station");
    int station=home->GetObjectID(),sector=home->GetCluster()->GetObjectID();
    require(m.a->sendChatOrder(ChatChannel::Group,sender,c_cidGoto,OT_station,station) &&
        m.a->sendChatOrder(ChatChannel::GroupNoEcho,recipient,c_cidGoto,OT_station,station),"encrypted group order actions queued");
    m.wait([&]{return m.b->state().commands[0].command==c_cidGoto && m.b->state().commands[0].targetId==station;},"group recipient receives queued command");
    require(std::any_of(m.b->state().messages.begin(),m.b->state().messages.end(),[](auto& row){return row.tip && row.text.starts_with(std::string("\x81 ")+static_cast<const char*>(ConvertColorToString(Color::Orange()))) && row.text.ends_with("\x82 ");}),"encrypted commander order loses original orange color/reset");
    auto count=[](const Game& game){return std::count_if(game.state().messages.begin(),game.state().messages.end(),[](auto& row){return row.text.find(" (group): goto")!=std::string::npos;});};
    require(count(*m.a)==1 && count(*m.b)==0 && std::any_of(m.b->state().messages.begin(),m.b->state().messages.end(),[&](auto& row){return row.text.find(" ("+m.b->chatAudienceName({ChatChannel::Target,recipient})+"): goto")!=std::string::npos;}),"group has one author echo and individual recipient delivery");
    auto voice=std::find_if(m.quickChat.commands.begin(),m.quickChat.commands.end(),[](auto& entry){return entry.command==c_cidGoto && entry.targetMask==c_ttMe;});
    require(voice!=m.quickChat.commands.end(),"authored follow-me quick-chat fixture");
    m.a->clearMessages();m.b->clearMessages();
    require(m.a->sendQuickChat(ChatChannel::Target,*voice,recipient,true),"lobby voice-only quick chat queued over encrypted channel");
    m.wait([&]{return std::any_of(m.b->state().messages.begin(),m.b->state().messages.end(),[](auto& row){return row.chat;});},"lobby quick-chat voice reaches individual recipient");
    require(m.b->state().commands[0].command==c_cidGoto && m.b->state().commands[0].targetId==station,"lobby quick-chat voice must not replace queued flight orders");
    require(m.a->setWing(recipient,4),"console recipient wing assignment");m.wait([&]{return m.b->state().wing==4;},"wing assignment replicated");
    m.a->clearMessages();m.b->clearMessages();m.a->clearSounds();m.b->clearSounds();
    require(m.a->state().commander && m.a->setAutoDonate(recipient,-1),"commander donation toggle queued through original action");
    m.wait([&]{for(auto& p:m.b->state().pilots)if(p.id==recipient)return p.autoDonate<0;return false;},"commander turns another pilot's donation off");
    auto economyTip=[](const Game& game,const std::string& text){return std::count_if(game.state().messages.begin(),game.state().messages.end(),[&](const auto& row){return row.tip && row.warning && !row.chat && row.text==text;});};
    const auto investorName=std::string(m.a->playerShip()->GetName()),donorName=std::string(m.b->playerShip()->GetName());
    m.wait([&]{return economyTip(*m.b,"You have stopped donating your income to "+investorName)==1 && economyTip(*m.a,donorName+" has stopped donating to you")==1;},"original income-stop donor and former investor tips delivered");
    m.a->clearMessages();m.b->clearMessages();m.a->clearSounds();m.b->clearSounds();
    require(m.a->setAutoDonate(recipient,-1),"commander donation restore queued");
    m.wait([&]{for(auto& p:m.b->state().pilots)if(p.id==recipient)return p.autoDonate==sender;return false;},"commander restores donation to self");
    m.wait([&]{return economyTip(*m.b,"You have started donating your income to "+investorName)==1 && economyTip(*m.a,donorName+" is now donating to you")==1;},"original income-start donor and investor tips delivered");
    m.tick(12);require(economyTip(*m.b,"You have started donating your income to "+investorName)==1 && economyTip(*m.a,donorName+" is now donating to you")==1,"unchanged income reports replay personal tips");
    require(m.a->sendChat(ChatChannel::Wing,"Explicit recipient wing",4),"explicit wing chat action");
    m.wait([&]{return std::any_of(m.b->state().messages.begin(),m.b->state().messages.end(),[](auto& row){return row.text.find("Explicit recipient wing")!=std::string::npos;});},"explicit wing chat reaches remote pilot");
    m.a->launch();m.wait([&]{return !m.a->state().docked;},"console waypoint pilot launch");m.tick(160);
    require(m.a->sendChatWaypoint(ChatChannel::Target,sender,c_cidGoto,sector,{1000,2000,3000}),"high-level console waypoint action");
    m.wait([&]{return m.a->state().commands[1].command==c_cidGoto && m.a->state().commands[1].targetType==OT_buoy;},"console waypoint accepted and replicated");
    auto* buoy=m.authority->core()->GetModel(OT_buoy,ObjectID(m.a->state().commands[1].targetId));
    require(buoy && (buoy->GetPosition()-Vector(1000,2000,3000)).Length()<.1f && !m.server->metrics().rejected,"authoritative command waypoint preserves location without client object allocation");
    puts("Encrypted console group, explicit-wing, commander donation toggle and authoritative waypoint routes passed");
}
void queuedRequests(const std::filesystem::path& art) {
    Match m(art,27682);m.join();require(m.a->state().commander,"money recipient commander fixture");
    int recipient=m.a->state().shipId,sender=m.b->state().shipId,hull=m.b->playerShip()->GetBaseHullType()->GetObjectID();
    std::string expected=std::string(m.b->playerShip()->GetName())+" has requested $125 to buy a "+m.b->playerShip()->GetBaseHullType()->GetName()+"  Press the [Insert] key to approve it.";
    auto count=[&](const Game& game,std::string_view text){return std::count_if(game.state().messages.begin(),game.state().messages.end(),[&](const auto& message){return message.tip && message.text==text;});};
    auto request=[&](int amount){require(m.b->requestMoney(amount,hull),"encrypted money request submitted");m.wait([&]{return m.a->state().moneyRequest.sender==sender && m.a->state().moneyRequest.amount==amount;},"private money request reaches recipient");};
    request(125);m.wait([&]{return count(*m.a,expected)==1;},"original critical money/hull/Insert prompt delivered");
    require(m.a->state().moneyRequest.name==m.b->playerShip()->GetName() && m.a->state().moneyRequest.prompt==expected && count(*m.b,expected)==0,"money request loses hull/name or leaks to requesting peer");
    m.tick(30);require(count(*m.a,expected)==1,"repeated snapshots redisplay a money request tip");
    auto before=m.authority->pilotBalance(sender),balance=m.authority->pilotBalance(recipient);
    require(m.a->acceptCommand(),"original Insert money approval action submitted");
    m.wait([&]{return m.a->state().moneyRequest.sender<0 && m.authority->pilotBalance(sender)==before+125;},"Insert approves request exactly once");
    require(m.authority->pilotBalance(recipient)==balance-125,"money approval does not conserve balances");
    request(200);before=m.authority->pilotBalance(sender);balance=m.authority->pilotBalance(recipient);m.a->rejectCommand();
    m.wait([&]{return m.a->state().moneyRequest.sender<0;},"original Delete money rejection replicated");
    require(m.authority->pilotBalance(sender)==before && m.authority->pilotBalance(recipient)==balance,"Delete rejection transfers credits");
    request(100);auto* home=m.authority->core()->GetShip(ShipID(recipient))->GetStation();int station=home->GetObjectID();
    require(m.b->sendChatOrder(ChatChannel::Target,recipient,c_cidDefend,OT_station,station),"order replacing money request submitted");
    m.wait([&]{return m.a->state().moneyRequest.sender<0 && m.a->state().commands[0].command==c_cidDefend;},"new order replaces same sender's money request");
    require(m.a->state().commands[0].text==std::string("defend ")+home->GetName(),"queued HUD command loses original lowercase verb/name format");
    require(m.a->acceptCommand(),"Insert accepts original queued order");
    m.wait([&]{return m.a->state().commands[1].command==c_cidDefend && count(*m.a,"Command accepted.")==1 && count(*m.a,"")==1;},"acceptance emits ordinary tip and clears critical slot as WinTrek does");
    request(50);before=m.authority->pilotBalance(sender);balance=m.authority->pilotBalance(recipient);
    m.wait([&]{return m.a->state().moneyRequest.sender<0;},"money request expires after the original fifteen seconds",1100);
    require(m.authority->pilotBalance(sender)==before && m.authority->pilotBalance(recipient)==balance,"expired request changes balances");
    require(m.b->sendChatOrder(ChatChannel::Target,recipient,c_cidGoto,OT_station,station),"late-order fixture submitted");
    m.wait([&]{return m.a->state().commands[0].command==c_cidGoto;},"late queued order delivered");m.tick(920);
    require(m.a->state().commands[0].command==c_cidGoto && m.a->acceptCommand(true),"queued order is lost when its acknowledgement sender expires");
    m.wait([&]{return m.a->state().commands[1].command==c_cidGoto;},"Execute Command accepts the retained late order");
    puts("Encrypted original money/order tips, private delivery, accept/reject/execute actions, replacement, conservation and fifteen-second expiry passed");
}
void ballotParity(const std::filesystem::path& art) {
    Match m(art,27672,false,false,false,Scenario::Conquest,1,32767,false,1,true,3);m.join();
    std::unique_ptr<Game> cg,dg,eg;auto cc=m.client("Charlie",cg),dc=m.client("Delta",dg,1),ec=m.client("Echo",eg,2);
    FlightInput neutral;neutral.active=false;
    auto tick=[&](int count){for(int i=0;i<count;++i){cc->step(neutral,1./Net::tickRate);dc->step(neutral,1./Net::tickRate);ec->step(neutral,1./Net::tickRate);m.tick(1);cc->poll();dc->poll();ec->poll();require(!cc->closed() && !dc->closed() && !ec->closed(),"extra ballot client disconnected");}};
    auto wait=[&](auto predicate,const char* what,int limit=1200){for(int i=0;i<limit && !predicate();++i)tick(1);require(predicate(),what);};
    tick(120);require(cc->ready() && dc->ready() && ec->ready() && m.server->clients()==5,"five encrypted voters on three factions join");
    int alpha=m.a->state().shipId,bravo=m.b->state().shipId,charlie=cg->state().shipId,delta=dg->state().shipId;
    auto contains=[](const Game& g,std::string_view text){return std::any_of(g.state().messages.begin(),g.state().messages.end(),[&](auto& n){return n.text.ends_with(text);});};
    require(m.authority->teamLeader(0)==alpha && m.a->state().networkHost,"original initial owner/commander fixture");
    require(cg->setAutoDonate(alpha),"mutiny donation cascade fixture");
    wait([&]{return m.authority->core()->GetShip(ShipID(charlie))->GetAutoDonate()==m.authority->core()->GetShip(ShipID(alpha));},"supporter's donation registered");
    Allegiance::ChatPreferences prefs;prefs.filterAll=prefs.filterQuick=true;cg->chatPreferences(prefs);require(cg->mutePilot(bravo,true),"muted ballot proposer fixture");
    int funds=m.authority->pilotBalance(alpha)+m.authority->pilotBalance(bravo);
    require(m.b->sendChat(ChatChannel::Everyone,"#MuTiNy"),"encrypted mutiny proposed");
    wait([&]{return cg->state().ballot.id>=0;},"mutiny reaches eligible human even when proposer muted");
    int mutiny=cg->state().ballot.id;
    require(cg->state().ballot.text=="Bravo has proposed to munity.  Press [Y] to vote yes, [N] to vote no." && m.a->state().ballot.id<0 && m.b->state().ballot.id<0 && dg->state().ballot.id<0 && eg->state().ballot.id<0,"mutiny original text, hidden commander/initiator and faction privacy");
    Net::Action forged;forged.op=Net::Operation::Vote;forged.args={mutiny,1};require(!m.authority->authorityAction(delta,forged),"foreign faction can cast mutiny vote");
    forged.args={mutiny,2};require(!m.authority->authorityAction(charlie,forged),"invalid vote Boolean accepted");
    forged.args={mutiny+99,1};require(!m.authority->authorityAction(charlie,forged),"unknown ballot ID accepted");
    require(cg->vote(true) && cg->state().ballot.id<0,"client does not immediately remove voted ballot");
    wait([&]{return m.b->state().commander && !m.a->state().commander && m.b->state().networkHost && m.a->state().networkHostPilot==bravo;},"silent commander no, mutiny leadership and original mission owner replicated");
    require(m.authority->teamLeader(0)==bravo && m.authority->pilotBalance(alpha)==0 && m.authority->pilotBalance(bravo)==funds,"mutiny money transfer does not conserve accounts");
    wait([&]{return m.a->state().money==0 && m.b->state().money==funds && contains(*cg,"Bravo's proposal to mutiny has passed.  ( 2 for 1 against)");},"original mutiny result and corrected old/new leader balances received");
    auto* old=m.authority->core()->GetShip(ShipID(alpha));auto* next=m.authority->core()->GetShip(ShipID(bravo));
    require(old->GetAutoDonate()==next && !next->GetAutoDonate() && m.authority->core()->GetShip(ShipID(charlie))->GetAutoDonate()==next,"encrypted mutiny does not cascade donations");
    forged.args={mutiny,1};require(!m.authority->authorityAction(charlie,forged),"settled duplicate ballot accepted");
    tick(30);require(cg->state().ballot.id<0,"acknowledged/repeated ballot notice reopens a voted prompt");
    require(m.a->sendChat(ChatChannel::Team,"#resign") && m.a->sendChat(ChatChannel::Team,"#draw suffix"),"independent encrypted ballot kinds queued");
    wait([&]{return m.b->state().ballot.text.starts_with("Alpha has proposed resigning.") && cg->state().ballot.id>=0;},"queued team resignation prompt");
    int resign=m.b->state().ballot.id;require(m.b->vote(false) && m.b->state().ballot.id>=0 && m.b->state().ballot.id!=resign,"original client FIFO does not advance to queued draw");
    require(m.b->vote(true) && m.b->state().ballot.id<0,"draw yes does not clear prompt immediately");
    require(dg->sendChat(ChatChannel::Team,"#draw"),"duplicate offer submitted from another faction");
    wait([&]{return contains(*dg,"Draw vote already in progress...");},"original private global-duplicate message");
    require(!contains(*eg,"Draw vote already in progress...") && !contains(*m.b,"Draw vote already in progress..."),"private duplicate feedback leaked across factions");
    // One remaining human abstains; original ties fail after thirty seconds,
    // but the same abstention does not prevent a 2-to-0 draw offer passing.
    wait([&]{return dg->state().ballot.id>=0 && eg->state().ballot.id>=0;},"offer timeout creates opposing-team draw ballots",1900);
    wait([&]{return contains(*m.b,"Alpha's proposal to resign has been defeated.  ( 1 for 1 against 1 abstained)");},"original timed-out tie tally excludes drones");
    require(cg->state().ballot.id<0 && !m.a->state().complete && dg->state().ballot.text.find(" has offered a draw.")!=std::string::npos,"expired team prompts or draw eligibility differ");
    forged.args={resign,1};require(!m.authority->authorityAction(charlie,forged),"expired abstention can vote after settlement");
    std::unique_ptr<Game> lg;auto lc=m.client("Late voter",lg,1);
    for(int i=0;i<120;++i){lc->step(neutral,1./Net::tickRate);tick(1);lc->poll();}
    require(lc->ready() && lg->state().ballot.id<0 && !lg->vote(true),"late peer incorrectly becomes eligible for an existing ballot");
    forged.args={dg->state().ballot.id,1};require(!m.authority->authorityAction(lg->state().shipId,forged),"authority accepts forged late-arrival vote");
    require(dg->vote(true),"first opposing faction accepts draw");tick(30);lc->poll();require(!m.a->state().networkLobby,"draw ended without the other participating faction");
    require(eg->vote(true),"second opposing faction accepts draw");
    wait([&]{return m.a->state().networkLobby && m.b->state().networkLobby && dg->state().networkLobby && eg->state().networkLobby && m.a->state().endgame.frozen;},"accepted draw freezes results and original next-round team lobby");
    lc->poll();
    require(m.a->state().result=="The game was declared a draw" && m.a->state().endgame.winner==NA && m.a->state().ballot.id<0 && cg->state().ballot.id<0 && eg->state().ballot.id<0,"draw reason/winner/prompt reset lost across reliable endgame and new round");
    require(!m.a->vote(true) && !m.b->vote(false),"new-round lobby retains old ballots");
    m.report("Encrypted ballots: faction eligibility, FIFO, mutiny auto-no/owner/funds/donations, exact feedback, notice acknowledgments, timeout/tie, three-team draw and next-round reset");
}
void missionChatParity(const std::filesystem::path& art) {
    Match m(art,27673);m.join();
    auto count=[](const Game& game,std::string_view text){return std::count_if(game.state().messages.begin(),game.state().messages.end(),[&](auto& row){return row.text.ends_with(text);});};
    auto member=[](const Game& game,int id){auto i=std::find_if(game.state().networkPilots.begin(),game.state().networkPilots.end(),[&](auto& row){return row.id==id;});require(i!=game.state().networkPilots.end(),"mission chat roster identity");return *i;};
    int alpha=m.a->state().shipId,bravo=m.b->state().shipId;
    require(m.a->state().networkHost && m.a->state().commander,"mission chat owner fixture");
    for(auto text:{"#draw","#list","#autobalance n","#autoaccept"}){
        auto before=m.a->state().messages.size();
        require(m.a->sendChat(ChatChannel::Target,text,alpha) && m.a->state().messages.size()==before+1 && m.a->state().messages.back().chatTarget==CHAT_INDIVIDUAL,"self chat local echo");
        m.tick(30);require(m.a->state().messages.size()==before+1 && m.a->state().ballot.id<0 && m.a->state().autoAccept==0x3f && m.a->state().maxImbalance==32767 && count(*m.b,text)==0,"individual self chat invoked server commands or leaked to others");
    }
    m.a->clearMessages();
    require(m.a->sendChat(ChatChannel::Team,"Immediate original author echo") && count(*m.a,"Immediate original author echo")==1,"typed author chat waits for the server instead of echoing locally");
    require(m.a->state().messages.back().fromPlayer && m.a->state().messages.back().chatTarget==CHAT_TEAM,"local author echo loses original human/recipient metadata");
    m.wait([&]{return count(*m.b,"Immediate original author echo")==1;},"team recipient gets typed chat");m.tick(30);require(count(*m.a,"Immediate original author echo")==1,"server duplicates locally echoed typed chat");
    std::unique_ptr<Game> dg,wg;auto dc=m.client("Opposing Commander",dg,1);std::unique_ptr<Net::Client> wc;FlightInput neutral;neutral.active=false;
    auto tick=[&](int n){for(int i=0;i<n;++i){dc->step(neutral,1./Net::tickRate);if(wc && !wc->closed())wc->step(neutral,1./Net::tickRate);m.tick(1);dc->poll();if(wc)wc->poll();require(!dc->closed(),"opposing commander disconnected");}};
    auto wait=[&](auto predicate,const char* what,int limit=1200){for(int i=0;i<limit && !predicate();++i)tick(1);require(predicate(),what);};
    tick(120);require(dc->ready() && dg->state().commander && !dg->state().networkHost,"opposing commander authority fixture");
    require(m.a->sendChat(ChatChannel::Team,"#LiSt suffix") && count(*m.a,"#LiSt suffix")==1,"consumed list command is not echoed locally");
    wait([&]{return count(*m.a,Allegiance::MissionChatHelp)==1;},"private original list help");
    require(count(*m.b,Allegiance::MissionChatHelp)==0 && count(*dg,Allegiance::MissionChatHelp)==0 && count(*m.b,"#LiSt suffix")==0,"consumed list command/help leaks to teammates or opponents");
    require(m.a->sendChat(ChatChannel::Team,"#AUTOACCEPT suffix"),"commander runtime admission command");wait([&]{return !(m.a->state().autoAccept&1) && !(dg->state().autoAccept&1);},"runtime autoaccept flag replicated to all factions");
    require(count(*m.a,"#AUTOACCEPT suffix")==1 && count(*dg,"#AUTOACCEPT suffix")==0,"autoaccept author echo or original audience changed");
    require(m.b->sendChat(ChatChannel::Team,"#autoaccept"),"noncommander command falls through");tick(30);require(!(m.a->state().autoAccept&1) && count(*m.a,"#autoaccept")==1,"noncommander changes policy or loses ordinary forwarded chat");
    require(dg->sendChat(ChatChannel::Team,"#autoaccept"),"independent commander admission command");wait([&]{return !(m.a->state().autoAccept&2);},"other commander toggles only its faction");
    wc=m.client("Waiting pilot",wg);tick(120);int waiting=wg->state().shipId;
    require(wc->ready() && member(*m.a,waiting).team==noTeam && member(*m.a,waiting).requested==0,"runtime automatic admission off does not preserve an applicant in No Team");
    auto lobby=[&](Net::Client& client,LobbyAction kind,int value){Net::Action action;action.op=Net::Operation::Lobby;action.args={int(kind),value};require(client.action(action),"mission policy control queued");};
    lobby(*m.ca,LobbyAction::LockSides,1);wait([&]{return m.a->state().lobbyFlags&2;},"sides locked during applicant revalidation");
    m.a->sendChat(ChatChannel::Team,"#autoaccept");wait([&]{return (m.a->state().autoAccept&1) && member(*m.a,waiting).requested==noTeamRequest;},"enabling admission clears a now-invalid pending request");
    require(member(*m.a,waiting).team==noTeam,"locked sides accepted a pending pilot");
    lobby(*m.ca,LobbyAction::LockSides,0);wait([&]{return !(m.a->state().lobbyFlags&2);},"sides unlocked");
    m.a->sendChat(ChatChannel::Team,"#autoaccept");wait([&]{return !(m.a->state().autoAccept&1);},"disable runtime automatic admission again");
    Net::Action request;request.op=Net::Operation::Team;request.args[0]=0;require(wc->action(request),"new applicant request");wait([&]{return member(*m.a,waiting).requested==0;},"new applicant waits for commander");
    m.a->sendChat(ChatChannel::Team,"#autoaccept");wait([&]{return member(*m.a,waiting).team==0 && wg->state().docked;},"runtime autoaccept admits and equips waiting pilot");
    auto match=m.ca->match();auto* world=m.authority->core();auto* ship=world->GetShip(ShipID(alpha));auto* home=ship->GetStation();
    auto policy=[&](std::string text,int value){
        require(m.a->sendChat(ChatChannel::Team,text),"owner balance command");
        wait([&]{return m.a->sessionOptions().rules.maxImbalance==value && m.b->sessionOptions().rules.maxImbalance==value && dg->sessionOptions().rules.maxImbalance==value && wg->sessionOptions().rules.maxImbalance==value;},"ordered live rule record reaches every faction");
        require(world->GetMissionParams()->iMaxImbalance==value && m.authority->sessionOptions().rules.maxImbalance==value && world==m.authority->core() && world->GetShip(ShipID(alpha))==ship && ship->GetStation()==home && m.ca->match()==match,"balance update reset the match, ship or station");
        wait([&]{return count(*dg,text)==1;},"balance command broadcasts to opposing team");require(count(*m.a,text)==1 && m.a->state().messages.back().chatTarget==CHAT_TEAM,"broadcast balance command duplicates or rewrites author's original audience");
    };
    policy("#autobalance 1",1);policy("#autobalance A",32766);policy("#autobalance N",32767);
    require(m.a->sendChat(ChatChannel::Team,"#autobalance x"),"unknown balance parameter forwarded");wait([&]{return count(*dg,"#autobalance x")==1;},"recognized unknown balance command still broadcasts");require(world->GetMissionParams()->iMaxImbalance==32767,"unknown parameter changed rules");
    require(m.a->sendChat(ChatChannel::Team,"#autobalance"),"short balance command forwarded normally");tick(30);require(count(*dg,"#autobalance")==0 && count(*m.b,"#autobalance")==1,"short balance command incorrectly forces public chat");
    require(dg->sendChat(ChatChannel::Everyone,"#autobalance 2"),"non-owner balance command");tick(30);require(world->GetMissionParams()->iMaxImbalance==32767,"other team commander changed game-owner balance controls");
    // A burst of ordered updates cannot be rolled back by older snapshots.
    m.a->sendChat(ChatChannel::Team,"#autobalance 2");m.a->sendChat(ChatChannel::Team,"#autobalance 1burst");m.a->sendChat(ChatChannel::Team,"#autobalance Nburst");
    wait([&]{return count(*dg,"#autobalance Nburst")==1;},"ordered rule burst delivered");tick(30);require(dg->sessionOptions().rules.maxImbalance==32767 && wg->state().maxImbalance==32767,"stale snapshots overwrite ordered live policy");
    m.a->sendChat(ChatChannel::Team,"#ban Waiting pilot");tick(30);require(!wc->closed() && world->GetShip(ShipID(waiting)),"mission owner ban removes an active faction member");
    request.args[0]=noTeam;wc->action(request);wait([&]{return member(*m.a,waiting).team==noTeam;},"owner boot target leaves its faction");
    m.b->sendChat(ChatChannel::Team,"#ban Waiting pilot");tick(30);require(!wc->closed() && count(*m.a,"#ban Waiting pilot")>=1,"nonowner ban does not fall through as ordinary chat");
    m.a->sendChat(ChatChannel::Team,"#ban Waiting pilot ");tick(30);require(!wc->closed(),"owner ban trims exact target names");
    wg->sendChat(ChatChannel::Everyone,"#list");wait([&]{return count(*m.a,"#list")==1;},"No Team list command is ordinary public chat");require(count(*wg,Allegiance::MissionChatHelp)==0,"No Team pilot can invoke started-match command help");
    m.a->sendChat(ChatChannel::Team,"#BaN wAiTiNg PiLoT");wait([&]{return wc->closed() && m.server->clients()==3 && !world->GetShip(ShipID(waiting));},"case-insensitive owner boot retires unassigned connection/identity");
    require(wc->error()=="You have been lobby booted by the mission owner!","owner boot uses wrong original reason or claims unavailable ASGS accounts");
    require(m.a->makeTeamLeader(bravo),"owner authority handover");wait([&]{return m.b->state().networkHost && m.b->state().commander;},"game owner follows command handover");
    m.b->sendChat(ChatChannel::Team,"#autobalance 2");wait([&]{return m.a->state().maxImbalance==2 && dg->state().maxImbalance==2;},"new owner can change live balance");
    m.a->sendChat(ChatChannel::Team,"#autobalance n");tick(30);require(world->GetMissionParams()->iMaxImbalance==2,"previous owner retained balance authority");
    std::unique_ptr<Game> lateGame;auto late=m.client("Policy bootstrap",lateGame,1);
    for(int i=0;i<120;++i){late->step(neutral,1./Net::tickRate);tick(1);late->poll();}
    require(late->ready() && lateGame->sessionOptions().rules.maxImbalance==2 && lateGame->core()->GetMissionParams()->iMaxImbalance==2,"late welcome bootstraps stale mission policy");
    // Remain in No Team until accepted: no effect on the existing draw voters.
    require(lateGame->state().networkLobby,"opposing autoaccept-off bootstrap should remain an applicant");
    m.b->sendChat(ChatChannel::Team,"#draw");wait([&]{return m.a->state().ballot.id>=0;},"draw vote before policy-preserving next round");m.a->vote(true);
    wait([&]{return dg->state().ballot.id>=0;},"other faction draw vote");dg->vote(true);
    wait([&]{return m.a->state().networkLobby && m.a->state().endgame.frozen && m.ca->match()!=match;},"draw advances the match without losing live rules");late->poll();
    require(m.a->sessionOptions().rules.maxImbalance==2 && m.b->sessionOptions().rules.maxImbalance==2 && m.authority->core()->GetMissionParams()->iMaxImbalance==2,"next round reverts runtime balance policy");
    auto oldHelp=count(*m.b,Allegiance::MissionChatHelp);m.b->sendChat(ChatChannel::Everyone,"#list");tick(30);require(count(*m.a,"#list")>=1 && count(*m.b,Allegiance::MissionChatHelp)==oldHelp,"prestart list should forward normally");
    m.b->sendChat(ChatChannel::Team,"#ban Policy bootstrap");wait([&]{late->poll();return late->closed();},"original owner No Team boot also works before game start");
    require(late->error()=="You have been lobby booted by the mission owner!","prestart owner boot reason");
    m.report("Original mission chat: immediate/single author echoes, private help, roles, runtime admissions/revalidation, ordered shared policy changes, exact No Team-only owner boot, handover, late welcome and next-round continuity");
}
void chatParity(const std::filesystem::path& art) {
    Match m(art,27669);m.join();int sender=m.a->state().shipId,recipient=m.b->state().shipId;
    auto yes=std::find_if(m.quickChat.commands.begin(),m.quickChat.commands.end(),[](const auto& e){return e.sonic==15;});
    require(yes!=m.quickChat.commands.end(),"original affirmative network fixture");
    auto contains=[&](std::string_view value){return std::any_of(m.b->state().messages.begin(),m.b->state().messages.end(),[&](auto& row){return row.text.find(value)!=row.text.npos;});};
    auto heard=[&](int id){return std::any_of(m.b->sounds().begin(),m.b->sounds().end(),[&](auto& sound){return sound.id==id;});};
    auto clear=[&]{m.tick(30);m.b->clearMessages();m.b->clearSounds();};
    Allegiance::ChatPreferences prefs;prefs.filterAll=true;m.b->chatPreferences(prefs);clear();
    require(m.a->sendChat(ChatChannel::Everyone,"Filtered public text") && m.a->sendQuickChat(ChatChannel::Everyone,*yes,-1,true),"public chat and voice queued");m.tick(60);
    require(!contains("Filtered public text") && !contains("Affirmative") && !heard(yes->sound),"network public filter discards text and its recording together");
    prefs.filterAll=false;prefs.filterQuick=true;m.b->chatPreferences(prefs);clear();
    require(m.a->sendQuickChat(ChatChannel::Target,*yes,recipient,true),"filtered individual quick voice queued");m.tick(60);
    require(contains("Affirmative") && !heard(yes->sound) && heard(newPersonalMsgSound),"network quick filter retains text and substitutes the original personal cue");
    auto row=std::find_if(m.b->state().messages.begin(),m.b->state().messages.end(),[](auto& r){return r.text.find("Affirmative")!=r.text.npos;});
    auto color=m.authority->core()->GetShip(ShipID(sender))->GetSide()->GetColor();
    require(row!=m.b->state().messages.end() && row->chatTarget==CHAT_INDIVIDUAL && row->fromPlayer && row->leader==m.a->state().commander && glm::distance(glm::vec3(row->color),glm::vec3(color.R(),color.G(),color.B())*.85f)<.008f,"network retains original chat channel, human sender, commander font metadata and side-color tint");
    auto* home=m.authority->core()->GetShip(ShipID(sender))->GetStation();require(home,"muted order station fixture");int station=home->GetObjectID();
    require(m.b->mutePilot(sender,true),"network mute preference queued");clear();
    require(m.a->sendChatOrder(ChatChannel::Target,recipient,c_cidGoto,OT_station,station),"muted order queued");m.tick(60);
    require(!contains("goto") && m.b->state().commands[0].command==c_cidNone && m.authority->core()->GetShip(ShipID(recipient))->GetCommandID(c_cmdQueued)==c_cidNone,"muted sender cannot issue authoritative orders");
    require(m.b->mutePilot(sender,false),"network unmute queued");clear();
    require(m.a->sendChatOrder(ChatChannel::Target,recipient,c_cidGoto,OT_station,station),"unmuted order queued");
    m.wait([&]{return contains("goto") && m.b->state().commands[0].command==c_cidGoto;},"unmuted original order restored");
    Net::Action leave{};leave.op=Net::Operation::Team;leave.args[0]=noTeam;require(m.ca->action(leave),"sender returns to original No Team");
    m.wait([&]{return m.a->state().networkLobby && m.authority->core()->GetShip(ShipID(sender))->GetSide()->GetObjectID()==SIDE_TEAMLOBBY;},"No Team sender retains identity");
    prefs.filterQuick=false;
    for(int mode:{1,2,3}){
        prefs.filterLobby=mode;m.b->chatPreferences(prefs);clear();
        require(m.a->sendChat(ChatChannel::Everyone,"Lobby public") && m.a->sendChat(ChatChannel::Target,"Lobby private",recipient) && m.a->sendQuickChat(ChatChannel::Everyone,*yes,-1,true),"original lobby messages queued");m.tick(60);
        require(contains("Lobby public")==bool(mode==3) && contains("Lobby private")==bool(mode!=2) && heard(yes->sound)==bool(mode==3),"three original lobby filtering modes retain PM distinction and drop recordings");
    }
    require(!m.server->metrics().rejected,"chat preference actions accepted in game and lobby");
    puts("Encrypted chat parity: per-recipient public/voice/lobby filters, original notification cues, commander font/color, mute/unmute and authoritative order exclusion passed");
}
void normal(const std::filesystem::path &art) {
    Match m(art, 27631);
    m.join();
    compatibility(m, false);
    compatibility(m, true);
    int alpha = m.a->state().shipId, bravo = m.b->state().shipId;
    Game *commander = m.a->state().commander ? m.a.get() : m.b.get();
    Game *pilot = commander == m.a.get() ? m.b.get() : m.a.get();
    Net::Client *sender = commander == m.a.get() ? m.ca.get() : m.cb.get();
    int recipient = pilot->state().shipId;
    int old = m.authority->pilotBalance(recipient);
    m.tick(12);commander->clearMessages();pilot->clearMessages();commander->clearSounds();pilot->clearSounds();
    require(commander->donateMoney(recipient, 100), "donation queued");
    for (int i = 0;
         i < 1200 && !(m.authority->pilotBalance(recipient) == old + 100 && pilot->state().money == old + 100); ++i)
        m.tick(1);
    if (m.authority->pilotBalance(recipient) != old + 100 || pilot->state().money != old + 100)
        fprintf(stderr,
                "Donation commander %d client money %d authority %d recipient %d previous %d authority %d client %d "
                "rejected %llu server %llu snapshots %llu/%llu\n",
                commander->state().shipId, commander->state().money,
                m.authority->pilotBalance(commander->state().shipId), recipient, old,
                m.authority->pilotBalance(recipient), pilot->state().money,
                (unsigned long long)sender->metrics().rejected, (unsigned long long)m.server->metrics().rejected,
                (unsigned long long)m.ca->metrics().snapshots, (unsigned long long)m.cb->metrics().snapshots);
    require(m.authority->pilotBalance(recipient) == old + 100 && pilot->state().money == old + 100,
            "donation executed once and replicated");
    const auto donationText=std::string(commander->playerShip()->GetName())+" gave you $100. You now have $"+std::to_string(old+100)+".";
    auto donationTips=[&](const Game& game){return std::count_if(game.state().messages.begin(),game.state().messages.end(),[&](const auto& row){return row.tip && !row.warning && !row.chat && row.text==donationText;});};
    m.wait([&]{return donationTips(*pilot)==1;},"original private donation tip received");
    require(donationTips(*commander)==0 && std::count_if(pilot->sounds().begin(),pilot->sounds().end(),[](const auto& s){return s.id==newNonCriticalMsgSound && s.relative;})==1 && std::none_of(commander->sounds().begin(),commander->sounds().end(),[](const auto& s){return s.id==newNonCriticalMsgSound;}),"donation tip sound is missing, duplicated or leaked to its sender");
    m.tick(12);require(donationTips(*pilot)==1,"steady money reports repeat the recipient's donation tip");
    commander->clearMessages();pilot->clearMessages();commander->clearSounds();pilot->clearSounds();
    auto* donor=m.authority->core()->GetShip(ShipID(recipient));require(donor->GetAutoDonate() && donor->GetAutoDonate()->GetObjectID()==commander->state().shipId,"payday has original investor routing");
    m.authority->core()->GetIgcSite()->PaydayEvent(donor->GetSide(),20);
    const auto donatedPayday="You donated your payday of $10 to "+std::string(commander->playerShip()->GetName())+".";
    auto paydayTip=[](const Game& game,const std::string& text){return std::count_if(game.state().messages.begin(),game.state().messages.end(),[&](const auto& row){return row.tip && !row.warning && !row.chat && row.text==text;});};
    m.wait([&]{return paydayTip(*pilot,donatedPayday)==1 && paydayTip(*commander,"You received $20 in pay and donations.")==1;},"original donated/aggregated paydays delivered over encrypted reports");
    for(auto* peer:{pilot,commander})require(std::count_if(peer->sounds().begin(),peer->sounds().end(),[](const auto& s){return s.id==paydaySound && s.relative;})==1,"network payday loses or repeats its original personal cue");
    Net::Action invalid{};
    invalid.op = Net::Operation::Donate;
    invalid.args = {recipient, -100};
    old = m.authority->pilotBalance(recipient);
    require(sender->action(invalid), "invalid request delivered");
    m.tick(30);
    require(m.authority->pilotBalance(recipient) == old && sender->metrics().rejected,
            "server rejects negative donation without mutation");
    Net::Action respawn{};
    respawn.op = Net::Operation::Respawn;
    auto beforeLife = m.authority->networkFrame(commander->state().shipId, m.server->tick(), 0);
    auto life = [&](const Net::Frame &f) {
        for (auto &e : f.entities)
            if (e.identity.type == OT_ship && e.identity.id == commander->state().shipId)
                return e.life;
        throw std::runtime_error("missing pilot life");
    };
    require(sender->action(respawn), "live respawn rejection request");
    m.tick(30);
    require(life(beforeLife) == life(m.authority->networkFrame(commander->state().shipId, m.server->tick(), 0)) &&
                sender->metrics().rejected == 2,
            "live pilot cannot obtain a free authority respawn");
    require(m.a->sendChat(ChatChannel::Team, "Network team message"), "team chat queued");
    m.tick(30);
    require(std::any_of(m.b->state().messages.begin(), m.b->state().messages.end(),
                        [](auto &message) { return message.text.find("Network team message") != std::string::npos; }),
            "team chat reaches another human pilot");
    require(commander->setWing(pilot->state().shipId, 2), "commander assigns another pilot wing");
    m.wait([&] { return pilot->state().wing == 2; }, "wing assignment replicated");
    require(pilot->requestMoney(75), "money request queued");
    m.wait([&] { return commander->state().moneyRequest.sender == pilot->state().shipId; },
           "commander receives authenticated money request");
    old = m.authority->pilotBalance(pilot->state().shipId);
    require(commander->respondMoneyRequest(true), "commander grants money request");
    m.wait([&] { return m.authority->pilotBalance(pilot->state().shipId) == old + 75; },
           "money request grant applied once");
    auto research = std::find_if(commander->state().research.begin(), commander->state().research.end(),
                                 [](auto &r) { return r.available && r.price > 0 && !r.complete; });
    require(research != commander->state().research.end(), "available research fixture");
    int bucket = research->id;
    require(commander->invest(bucket, 100), "research investment queued");
    m.tick(30);
    auto *original = m.authority->core()->GetSide(0)->GetBucket(bucket);
    auto *replica = commander->core()->GetSide(0)->GetBucket(bucket);
    require(original && replica && original->GetMoney() == replica->GetMoney(),
            "original bucket investment replicated");
    int commanderId = commander->state().shipId;
    auto *outfit = m.authority->core()->GetShip(commanderId);
    int previousHull = outfit->GetBaseHullType()->GetObjectID();
    // Exercise a researched hangar, while leaving the original station/hull
    // purchase checks in place. The starting technology exposes only a scout.
    TechTreeBitMask unlocked;
    unlocked.SetAll();
    outfit->GetSide()->SetDevelopmentTechs(unlocked);
    m.wait([&] { return commander->core()->GetSide(0)->GetDevelopmentTechs() == unlocked; },
           "own researched technology replicated");
    auto hull = std::find_if(commander->state().hullChoices.begin(), commander->state().hullChoices.end(),
                             [&](auto &h) { return h.id != previousHull && h.price <= commander->state().money; });
    require(hull != commander->state().hullChoices.end(), "affordable alternative hangar hull");
    int newHull = hull->id;
    require(commander->buyHull(newHull), "hull purchase queued");
    m.wait(
        [&] {
            return outfit->GetBaseHullType()->GetObjectID() == newHull &&
                   commander->playerShip()->GetBaseHullType()->GetObjectID() == newHull;
        },
        "authoritative hull/loadout transition");
    require(commander->replacePart(PartKind::Pack, -1, -1), "clear cargo queued");
    m.wait([&] { return !outfit->GetMountedPart(NA, -1) && !commander->playerShip()->GetMountedPart(NA, -1); },
           "cargo removal replicated");
    int ammoPack = m.authority->core()->GetAmmoPack()->GetObjectID();
    require(commander->replacePart(PartKind::Pack, -1, ammoPack), "cargo refit queued");
    m.wait(
        [&] {
            auto *p = outfit->GetMountedPart(NA, -1);
            auto *r = commander->playerShip()->GetMountedPart(NA, -1);
            return p && r && p->GetPartType()->GetObjectID() == ammoPack && r->GetPartType()->GetObjectID() == ammoPack;
        },
        "original paid cargo refit replicated");
    auto loadout = commander->currentLoadout();
    require(commander->applyLoadout(loadout), "saved loadout queued");
    m.tick(30);
    m.wait(
        [&] {
            return sender->metrics().rejected == 2 &&
                   commander->state().money == m.authority->pilotBalance(commanderId);
        },
        "saved loadout wallet converges");
    m.a->launch();
    m.b->launch();
    for (int i = 0; i < 600 && (m.a->state().docked || m.b->state().docked); ++i)
        m.tick(1);
    require(!m.a->state().docked && !m.b->state().docked, "both clients launch through authority");
    require(m.a->state().cinematic || m.b->state().cinematic, "original launch camera delivered");
    auto *home = m.authority->core()->GetSide(0)->GetStations()->first()->data();
    require(m.a->order(bravo, OT_station, home->GetObjectID(), c_cidGoto), "pilot navigation order queued");
    m.wait([&] { return m.b->playerShip()->GetCommandID(c_cmdQueued) == c_cidGoto; },
           "original queued command reaches remote pilot");
    require(m.b->acceptCommand(true), "pilot accepts remote order");
    m.wait([&] { return m.b->state().autopilot; }, "accepted command executes original autopilot");
    m.ia.active = m.ib.active = true;
    m.ia.throttle = .5f;
    m.ia.yaw = .25f;
    m.ib.throttle = .3f;
    m.ib.roll = .2f;
    m.tick(240);
    require(m.authority->core()->GetShip(alpha)->GetVelocity().Length() > 1, "authority simulates pilot controls");
    require(m.a->state().speed > 1 && m.b->state().speed > 1, "replicas predict shared IGC movement");
    m.ia.active = m.ib.active = false;
    m.tick(60);
    m.a->clearSounds();m.b->clearSounds();
    m.a->returnToBase();
    m.b->returnToBase();
    for (int t = 0; t < 12000 && (!m.a->state().docked || !m.b->state().docked); t += 30)
        m.tick(30);
    require(m.a->state().docked && m.b->state().docked, "original navigation/collision returns both pilots to hangar");
    for (auto *pilot : {m.a.get(), m.b.get()}) {
        // WinTrek::ChangeStation uses these private cues when entering an
        // own base; it does not append the old native "Docked at" narrative.
        for(int cue:{dockedSound,salWelcomeHomeSound})
            require(std::count_if(pilot->sounds().begin(),pilot->sounds().end(),[&](const auto& sound){return sound.id==cue;})==1,
                    "original docking/welcome cue must reach each independent pilot exactly once");
        require(std::none_of(pilot->state().messages.begin(),pilot->state().messages.end(),[](const auto& message){return message.text.starts_with("Docked at");}),
                "docking adds a narrative absent from the original client");
    }
    m.b->launch();
    m.wait([&] { return !m.b->state().docked; }, "travel fixture launches");
    auto *traveler = m.authority->core()->GetShip(bravo);
    auto *warp = traveler->GetCluster()->GetWarps()->first()->data();
    auto destination = warp->GetDestination()->GetCluster()->GetObjectID();
    auto transitions = m.cb->metrics().discontinuities;
    traveler->SetPosition(warp->GetPosition());
    traveler->SetVelocity(warp->GetOrientation().GetForward() * 200);
    m.authority->core()->GetIgcSite()->HitWarpEvent(traveler, warp);
    for (int i = 0; i < 1200 &&
                    (!m.b->playerShip()->GetCluster() || m.b->playerShip()->GetCluster()->GetObjectID() != destination);
         ++i)
        m.tick(1);
    if (!m.b->playerShip()->GetCluster() || m.b->playerShip()->GetCluster()->GetObjectID() != destination)
        fprintf(stderr, "Warp destination %d authority sector %d replica sector %d docked %d/%d\n", destination,
                traveler->GetCluster() ? traveler->GetCluster()->GetObjectID() : -1,
                m.b->playerShip()->GetCluster() ? m.b->playerShip()->GetCluster()->GetObjectID() : -1,
                bool(traveler->GetStation()), m.b->state().docked);
    require(m.b->playerShip()->GetCluster() && m.b->playerShip()->GetCluster()->GetObjectID() == destination,
            "original warp exit reaches remote pilot");
    require(m.cb->metrics().discontinuities > transitions, "sector travel snaps prediction at its transition epoch");
    auto jumpSounds = m.b->sounds();
    require(std::any_of(jumpSounds.begin(), jumpSounds.end(), [](auto &sound) { return sound.id == jumpSound; }),
            "travel sound reaches the affected remote pilot");
    int enemy = m.authority->addLocalPilot("Hidden enemy", 1);
    require(enemy >= 0, "hidden enemy fixture");
    auto *es = m.authority->core()->GetShip(enemy);
    es->SetStation(nullptr);
    es->SetPosition(Vector(15000, 15000, 15000));
    m.tick(60);
    require(!m.a->core()->GetShip(enemy) && !m.b->core()->GetShip(enemy),
            "hidden enemy live pose absent from replicas");
    auto *enemyBase = m.authority->core()->GetSide(1)->GetStations()->first()->data();
    enemyBase->GetThingSite()->SetSideVisibility(m.authority->core()->GetSide(0), true);
    for (int i = 0; i < 600 && !m.b->core()->GetStation(enemyBase->GetObjectID()); ++i)
        m.tick(1);
    auto *remembered = m.b->core()->GetStation(enemyBase->GetObjectID());
    if (!remembered) {
        fprintf(stderr, "Base %d known %d eye %d attr %u authority sector %d replicas station count %d/%d\n",
                enemyBase->GetObjectID(), enemyBase->SeenBySide(m.authority->core()->GetSide(0)),
                enemyBase->GetCurrentEye(m.authority->core()->GetSide(0)), unsigned(enemyBase->GetAttributes()),
                enemyBase->GetCluster()->GetObjectID(), m.a->core()->GetStations()->n(),
                m.b->core()->GetStations()->n());
        auto frame = m.authority->networkFrame(bravo, m.server->tick(), 0);
        size_t offset = 0;
        for (auto &e : frame.entities) {
            if (e.identity.type == OT_station) {
                DataStationIGC data;
                std::memcpy(&data, frame.payload.data() + offset, sizeof(data));
                fprintf(stderr,
                        "Station entity id %d serial %u side %d sector %d flags %u payload id %d side %d sector %d\n",
                        e.identity.id, e.identity.serial, e.side, e.sector, e.flags, data.stationID, data.sideID,
                        data.clusterID);
            }
            offset += e.payloadBytes;
        }
    }
    require(remembered, "show-map base remembered");
    float fraction = remembered->GetFraction();
    enemyBase->SetFraction(.31f);
    TechTreeBitMask privateTech;
    privateTech.SetAll();
    enemyBase->GetSide()->SetDevelopmentTechs(privateTech);
    m.tick(30);
    remembered = m.b->core()->GetStation(enemyBase->GetObjectID());
    require(remembered && std::abs(remembered->GetFraction() - fraction) < .01f,
            "unseen base retains last observed hull");
    require(m.b->core()->GetSide(1)->GetDevelopmentTechs() != privateTech, "enemy research is private");
    auto *scout = m.authority->core()->GetShip(alpha);
    scout->SetStation(nullptr);
    scout->SetCluster(enemyBase->GetCluster());
    scout->SetPosition(enemyBase->GetPosition() + Vector(1000, 0, 0));
    scout->SetVelocity(Vector::GetZero());
    m.tick(30);
    remembered = m.b->core()->GetStation(enemyBase->GetObjectID());
    require(remembered && std::abs(remembered->GetFraction() - enemyBase->GetFraction()) < .01f,
            "team rediscovery refreshes base hull/type");
    m.report("Normal: compatibility, economy, research, chat, flight, docking, sector travel, visibility");
}
void health(const std::filesystem::path &art) {
    Match m(art, 27649);
    m.join();
    m.a->launch();
    m.wait([&] { return !m.a->state().docked; }, "health fixture launches");
    auto *ship = m.authority->core()->GetShip(m.a->state().shipId);
    const auto hull = ship->GetBaseHullType()->GetObjectID();
    m.ia.active = true;
    m.ia.throttle = 0;
    auto shield = [](IshipIGC *s) { return static_cast<IshieldIGC *>(s->GetMountedPart(ET_Shield, 0)); };
    auto park = [&] {
        auto *cluster = ship->GetCluster();
        const float radius = m.authority->core()->GetFloatConstant(c_fcidRadiusUniverse);
        const float lens = m.authority->core()->GetFloatConstant(c_fcidLensMultiplier);
        for (int i = 0; i < 64; ++i) {
            float angle = float(i) * 2 * pi / 64;
            Vector position(radius * .25f * std::cos(angle), radius * .25f * std::sin(angle), radius * lens * .15f);
            bool clear = true;
            for (auto *l = cluster->GetModels()->first(); l; l = l->next())
                if (l->data() != ship && (l->data()->GetPosition() - position).Length() < l->data()->GetRadius() + ship->GetRadius() + 500)
                    clear = false;
            if (!clear)
                continue;
            ship->SetPosition(position);
            ship->SetVelocity(Vector::GetZero());
            ship->SetAutopilot(false);
            ship->SetStateM(0);
            ship->SetFraction(1);
            require(shield(ship), "health fixture shield");
            shield(ship)->SetFraction(1);
            return;
        }
        throw std::runtime_error("No clear health fixture position");
    };
    park();
    m.tick(600);
    require(m.b->state().docked && ship->GetCluster()->GetShips()->n() == 1, "only one flying pilot in quiet sector");
    printf("Health: quiet-sector authority/replica hull %.3f/%.3f, shield %.3f/%.3f, replica sector cost %.9g\n",
           ship->GetFraction(), m.a->state().hull, shield(ship)->GetFraction(), m.a->state().shield,
           m.a->playerShip()->GetCluster()->GetCost());
    require(ship->GetFraction() == 1 && shield(ship)->GetFraction() == 1, "quiet-sector authority takes no ambient damage");
    require(m.a->state().hull == 1 && m.a->state().shield == 1, "quiet-sector prediction preserves displayed health");

    auto predictionOnly = [&](bool replay) {
        auto *local = m.a->playerShip();
        Vector position = local->GetPosition();
        float fraction = local->GetFraction();
        // Prediction may cross a boundary before the authority. It must not
        // author a damage/death decision, in either forward play or replay.
        local->SetPosition(Vector(10000000, 0, 0));
        m.a->predict(m.ia, 1. / Net::tickRate, !replay);
        require(local->GetFraction() == fraction && !m.a->state().dead && !m.a->state().pod,
                "prediction/replay cannot apply environmental damage or create a zero-health live ship");
        local->SetPosition(position);
        auto dr = local->ReceiveDamage(c_dmgidCollision, 1.e9f, local->GetLastUpdate(), position, position, nullptr);
        require(dr == c_drNoDamage && local->GetFraction() == fraction && shield(local)->GetFraction() == 1,
                "replica rejects locally authored impact damage");
    };
    predictionOnly(false);
    predictionOnly(true);

    // Use the original warp-arrival path, then exercise uninterrupted local
    // prediction in the freshly imported sector.
    auto *warp = ship->GetCluster()->GetWarps()->first()->data();
    auto destination = warp->GetDestination()->GetCluster()->GetObjectID();
    ship->SetPosition(warp->GetPosition());
    ship->SetVelocity(warp->GetOrientation().GetForward() * 200);
    m.authority->core()->GetIgcSite()->HitWarpEvent(ship, warp);
    m.wait([&] { return m.a->playerShip()->GetCluster() && m.a->playerShip()->GetCluster()->GetObjectID() == destination; },
           "health fixture warp arrival replicated");
    park();
    m.tick(600);
    require(ship->GetFraction() == 1 && shield(ship)->GetFraction() == 1 &&
                m.a->state().hull == 1 && m.a->state().shield == 1,
            "sector travel retains healthy armor and shield through repeated snapshots");
    for (auto *l = m.a->core()->GetClusters()->first(); l; l = l->next())
        require(l->data()->GetCost() == 0, "replica sectors have initialized crowding cost before simulation");

    const float radius = m.authority->core()->GetFloatConstant(c_fcidRadiusUniverse);
    const float boundary = std::sqrt(m.authority->core()->GetFloatConstant(c_fcidOutOfBounds)) * radius;
    ship->SetPosition(Vector(boundary * 2, 0, 0));
    m.wait([&] { return m.a->playerShip()->GetWarningMask() & c_wmOutOfBounds; },
           "authority out-of-bounds warning reaches pilot");
    require(ship->GetFraction() < 1 && m.a->state().hull < 1 && !m.a->state().pod,
            "genuine out-of-bounds damage is applied and replicated");
    m.a->predict(m.ia, 1. / Net::tickRate);
    require(m.a->playerShip()->GetWarningMask() & c_wmOutOfBounds, "prediction retains authoritative warning");
    park();
    m.wait([&] { return m.a->state().hull == 1 && !(m.a->playerShip()->GetWarningMask() & c_wmOutOfBounds); },
           "returning inside the sector clears warning and restores test health");

    // Lower only the test world's population threshold. This checks the
    // original penalty without requiring dozens of otherwise irrelevant bots.
    float baseCost = m.authority->core()->GetFloatConstant(c_fcidBaseClusterCost);
    m.authority->core()->SetFloatConstant(c_fcidBaseClusterCost, 100);
    m.wait([&] { return m.a->playerShip()->GetWarningMask() & c_wmCrowdedSector; },
           "genuine authority crowding warning reaches pilot");
    require(shield(ship)->GetFraction() < 1 && m.a->state().shield < 1,
            "original crowded-sector penalty remains authoritative and visible");
    m.authority->core()->SetFloatConstant(c_fcidBaseClusterCost, baseCost);
    park();
    m.wait([&] { return m.a->state().shield == 1 && !(m.a->playerShip()->GetWarningMask() & c_wmCrowdedSector); },
           "normal sector cost clears crowding warning");

    // A real swept ship/asteroid collision must damage a healthy ship and eject
    // a critically damaged hull. Original lifepods bounce without rock damage.
    auto *rock = ship->GetCluster()->GetAsteroids()->first()->data();
    auto ram = [&](float speed = 0) {
        ship->SetAutopilot(false);
        ship->SetStateM(coastButtonIGC);
        ship->SetPosition(rock->GetPosition() + Vector(0, 0, rock->GetRadius() + ship->GetRadius() + 5));
        ship->SetVelocity(Vector(0, 0, -(speed ? speed : ship->GetHullType()->GetMaxSpeed())));
        ControlData controls;
        std::fill_n(controls.jsValues, 4, 0.f);
        controls.jsValues[c_axisThrottle] = -1;
        ship->SetControls(controls);
    };
    shield(ship)->SetFraction(0);
    ram(40);
    m.wait([&] { return ship->GetFraction() < .99f || ship->GetBaseHullType()->GetObjectID() != hull; },
           "authoritative asteroid collision applies armor damage", 600);
    require(ship->GetBaseHullType()->GetObjectID() == hull, "healthy ship survives first test collision");
    float damaged = ship->GetFraction();
    m.wait([&] { return m.a->state().hull < .99f; }, "asteroid damage reaches pilot HUD");
    ship->ReceiveDamage(c_dmgidCollision, -ship->GetHullType()->GetHitPoints() * .1f,
                        ship->GetLastUpdate(), ship->GetPosition(), ship->GetPosition(), ship);
    m.wait([&] { return m.a->state().hull > damaged; }, "authoritative hull repair reaches pilot HUD");
    ship->SetFraction(.001f);
    shield(ship)->SetFraction(0);
    ram();
    m.wait([&] { return ship->GetBaseHullType()->HasCapability(c_habmLifepod); },
           "fatal asteroid collision ejects authority pilot", 180);
    m.wait([&] { return m.a->state().pod && m.a->state().hull > 0 && !m.a->state().dead; },
           "fatal collision replicates a live pod with restored armor");
    int deaths = ship->GetDeaths();
    ship->SetFraction(.001f);
    ram();
    m.wait([&] { return ship->GetVelocity().z > 0; }, "original pod collision bounces off asteroid", 180);
    require(ship->GetFraction() == .001f && ship->GetDeaths() == deaths && !ship->GetStation(),
            "original lifepods are immune to asteroid collision damage");
    ship->ReceiveDamage(c_dmgidCollision, 1.e9f, ship->GetLastUpdate(), ship->GetPosition(), ship->GetPosition(), nullptr);
    m.wait([&] { return ship->GetDeaths() == deaths + 1 && ship->GetStation(); },
           "fatal authority damage to pod consumes one life and recovers to hangar", 180);
    m.wait([&] { return m.a->state().docked && !m.a->state().dead && m.a->state().deaths == deaths + 1; },
           "pod death and hangar recovery reach pilot");
    m.a->launch();
    m.wait([&] { return !m.a->state().docked && !m.a->state().pod && m.a->state().hull > 0 && m.a->state().shield > 0; },
           "recovered pilot launches a healthy refitted ship");
    printf("Health: warp travel, forward/replayed prediction, environmental damage/warnings, asteroid hull damage %.3f, repair, fatal collision, pod immunity/death and relaunch passed\n", damaged);
    m.report("Health and lifecycle authority");
}
void construction(const std::filesystem::path &art) {
    // One complete cycle of the original map generator's three technology
    // asteroid types ensures a home-sector Carbonaceous site for Supremacy.
    Match m(art, 27643, false, false, false, Scenario::Conquest, 3);
    m.join();
    Game *commander = m.a->state().commander ? m.a.get() : m.b.get();
    auto *side = m.authority->core()->GetSide(0);
    auto *home = side->GetStations()->first()->data()->GetCluster();
    // The docked fixture has no scouting pilots. Discover the home-sector
    // construction sites before funding technology-center constructors.
    for (auto *l = home->GetAsteroids()->first(); l; l = l->next())
        l->data()->SetSideVisibility(side, true);
    auto findShell = [&](Game &game) -> IbuildingEffectIGC * {
        auto *sector = game.core()->GetCluster(home->GetObjectID());
        for (auto *l = sector->GetModels()->first(); l; l = l->next())
            if (l->data()->GetObjectType() == OT_buildingEffect)
                return static_cast<IbuildingEffectIGC *>(l->data());
        return nullptr;
    };
    for (int trial = 0; trial < 3; ++trial) {
        const char *name = trial == 1 ? "Supremacy Ctr" : "Light Outpost";
        IbucketIGC *bucket = nullptr;
        for (auto *l = side->GetBuckets()->first(); l; l = l->next()) {
            std::string_view label = l->data()->GetName();
            if (l->data()->GetBucketType() == OT_stationType && label == name && side->CanBuy(l->data()->GetBuyable()))
                bucket = l->data();
        }
        if (!bucket) {
            std::string available;
            for (auto *l = side->GetBuckets()->first(); l; l = l->next())
                if (l->data()->GetBucketType() == OT_stationType)
                    available += " [" + std::string(l->data()->GetName()) +
                                 (side->CanBuy(l->data()->GetBuyable()) ? ": buyable]" : ": unavailable]");
            throw std::runtime_error(std::string("Missing constructor bucket ") + name + "; available:" + available);
        }
        auto *type = static_cast<IstationTypeIGC *>(bucket->GetBuyable());
        // Constructor lifetimes are independent of mining/starting budgets.
        // Seed income through the original payday/donation accounting path.
        m.authority->core()->GetIgcSite()->PaydayEvent(side, float(bucket->GetPrice()) * 4);
        m.tick(30);
        printf("Constructor fixture: %s bucket %d, price %d, pilot %d money %d, buyable %d\n", name,
               bucket->GetObjectID(), bucket->GetPrice(), commander->state().shipId, commander->state().money,
               side->CanBuy(type));
        int sites = 0, knownSites = 0;
        for (auto *l = m.authority->core()->GetAsteroids()->first(); l; l = l->next())
            if (l->data()->HasCapability(type->GetBuildAABM())) {
                ++sites;
                knownSites += l->data()->SeenBySide(side);
            }
        printf("  build mask %x: %d compatible sites, %d known\n", unsigned(type->GetBuildAABM()), sites, knownSites);
        fflush(stdout);
        require(commander->invest(bucket->GetObjectID()), "constructor funding queued by commander");
        m.wait([&] { return bucket->GetMoney() == bucket->GetPrice(); }, "constructor funding applied");
        // Skip the long production timer, but let its final second run through
        // the real bucket update and StationTypeCompleted callback.
        require(bucket->GetTimeToBuild() > 1, "authored constructor production timer");
        bucket->SetTimeAndMoney((bucket->GetTimeToBuild() - 1) * 1000, bucket->GetMoney());
        IshipIGC *builder = nullptr;
        m.wait([&] {
            for (auto *l = side->GetShips()->first(); l; l = l->next())
                if (l->data()->GetPilotType() == c_ptBuilder && l->data()->GetBaseData() == type)
                    builder = l->data();
            return builder != nullptr;
        }, "funded constructor production");
        const int builderId = builder->GetObjectID();
        IasteroidIGC *rock = nullptr;
        float nearest = std::numeric_limits<float>::max();
        constexpr AsteroidAbilityBitMask technologyRocks =
            c_aabmSpecial | (c_aabmSpecial << 1) | (c_aabmSpecial << 2);
        for (auto *l = home->GetAsteroids()->first(); l; l = l->next())
            if (builder->LegalCommand(c_cidBuild, l->data()) &&
                ((type->GetBuildAABM() & technologyRocks) || !l->data()->HasCapability(technologyRocks))) {
                // Reserve the home-sector technology rocks for Supremacy.
                float distance = (builder->GetPosition() - l->data()->GetPosition()).LengthSquared();
                if (distance < nearest) {
                    nearest = distance;
                    rock = l->data();
                }
            }
        require(rock, "compatible home-sector construction site");
        const int rockId = rock->GetObjectID();
        const int stationCount = side->GetStations()->n();
        m.a->commandView(true);
        m.b->commandView(true);
        require(m.a->viewSector(home->GetObjectID()) && m.b->viewSector(home->GetObjectID()),
                "both clients observe the construction sector");
        require(commander->order(builderId, OT_asteroid, rockId, c_cidBuild), "constructor build order queued");
        m.wait([&] { return rock->GetBuildingEffect() != nullptr; }, "constructor travels and drills", 18000);
        TRef<IbuildingEffectIGC> shell = rock->GetBuildingEffect();
        const auto start = m.authority->core()->GetLastUpdate();
        printf("Construction: %s starts on asteroid %d at %.3f s\n", name, rockId, m.authority->state().time);
        fflush(stdout);
        m.wait([&] { return findShell(*m.a) && findShell(*m.b); }, "both replicas import the growing shell");
        for (auto *client : {m.a.get(), m.b.get()}) {
            auto *replicaBuilder = client->core()->GetShip(ShipID(builderId));
            require(replicaBuilder &&
                        (replicaBuilder->GetStateM() & (drillingMaskIGC | buildingMaskIGC)) ==
                        (builder->GetStateM() & (drillingMaskIGC | buildingMaskIGC)),
                    "command relationship import preserves authoritative drilling/building state");
        }
        TRef<IbuildingEffectIGC> shellA = findShell(*m.a), shellB = findShell(*m.b);
        auto checkExport = [&](IbuildingEffectIGC *effect, bool completed) {
            DataBuildingEffectExport record{};
            effect->Export(&record);
            require(record.asteroidID == (completed ? NA : rockId), "shell exports its current asteroid lifetime");
            auto &color = side->GetColor();
            require(record.color.GetRed() == color.GetRed() && record.color.GetGreen() == color.GetGreen() &&
                        record.color.GetBlue() == color.GetBlue() && record.color.GetAlpha() == color.GetAlpha(),
                    "authority and imported shells preserve their creation color without a builder");
            require(std::isfinite(effect->GetRadius()), "finite construction shell radius");
        };
        checkExport(shell, false);
        checkExport(shellA, false);
        checkExport(shellB, false);
        if (trial == 2) {
            m.authority->core()->GetIgcSite()->KillAsteroidEvent(rock, true);
            m.tick(60);
            require(!shell->GetCluster() && !shellA->GetCluster() && !shellB->GetCluster() &&
                        !m.authority->core()->GetShip(builderId) && side->GetStations()->n() == stationCount,
                    "destroying a construction site cancels the shell and builder on all peers");
            puts("Construction: destroying the site cancels the build without a station or ghost shell");
            continue;
        }
        m.wait([&] { return side->GetStations()->n() > stationCount; }, "construction completes", 2400);
        require(!m.authority->core()->GetShip(builderId) && !m.authority->core()->GetAsteroid(rockId),
                "construction consumes builder and asteroid");
        require(shell->GetCluster() == home && !shell->GetAsteroid(), "completed shell survives without builder or rock");
        m.tick(60);
        require(findShell(*m.a) == shellA && findShell(*m.b) == shellB && !shellA->GetAsteroid() &&
                    !shellB->GetAsteroid(),
                "quiet asteroid retirement preserves both existing replica shells");
        for (auto *effect : {static_cast<IbuildingEffectIGC *>(shell), static_cast<IbuildingEffectIGC *>(shellA),
                             static_cast<IbuildingEffectIGC *>(shellB)}) {
            checkExport(effect, true);
            require(!(effect->GetAttributes() & (c_mtHitable | c_mtCastRay)), "completed shell leaves collision lists");
        }
        for (auto *game : {m.a.get(), m.b.get()}) {
            bool inner = false, outer = false;
            for (auto &draw : game->scene(true)) {
                inner |= draw.blend == ModelBlend::Alpha;
                outer |= draw.blend == ModelBlend::Additive;
                require(std::isfinite(draw.color.a) && std::isfinite(draw.transform[0][0]), "finite fading shell draw");
            }
            require(inner && outer, "both original construction shell surfaces still render after completion");
        }
        std::unique_ptr<Game> late;
        auto connection = m.client("Late constructor observer", late);
        FlightInput inactive{};
        inactive.active = false;
        for (int i = 0; i < 180 && !(connection->ready() && late && findShell(*late)); ++i) {
            connection->step(inactive, 1. / Net::tickRate);
            m.tick(1);
            connection->poll();
        }
        require(connection->ready() && late && findShell(*late), "late join imports an already completed shell");
        TRef<IbuildingEffectIGC> lateShell = findShell(*late);
        checkExport(lateShell, true);
        require(!lateShell->GetAsteroid() && !(lateShell->GetAttributes() & (c_mtHitable | c_mtCastRay)),
                "late shell import needs neither asteroid nor constructor and cannot collide");
        for (int i = 0; i < 720 && shell->GetCluster(); ++i) {
            connection->step(inactive, 1. / Net::tickRate);
            m.tick(1);
            connection->poll();
        }
        m.tick(30);
        connection->poll();
        require(!shell->GetCluster() && !shellA->GetCluster() && !shellB->GetCluster() && !lateShell->GetCluster(),
                "original and late-imported shells retire at the end of the fade");
        require(m.authority->core()->GetLastUpdate() - start >= 39.f, "original forty-second construction animation");
        printf("Construction: %s completes and fades through live UDP snapshots\n", name);
    }
    m.report("Construction lifecycle");
}
void dockedCrews(const std::filesystem::path& art){
    Match m(art,27685);m.join();auto* core=m.authority->core();
    Game* host=m.a->state().shipId==m.authority->playerShip()->GetObjectID()?m.a.get():m.b.get();
    Game* gunner=host==m.a.get()?m.b.get():m.a.get();auto& client=gunner==m.a.get()?m.ca:m.cb;
    auto* parent=core->GetShip(host->state().shipId);auto* child=core->GetShip(gunner->state().shipId);auto* station=parent->GetStation();
    IhullTypeIGC* hull=nullptr;for(auto* l=core->GetHullTypes()->first();l;l=l->next())if(l->data()->GetMaxWeapons()-l->data()->GetMaxFixedWeapons()>=2){hull=l->data();break;}
    require(hull,"network docked multi-turret hull");while(auto* l=parent->GetParts()->first())l->data()->Terminate();parent->SetBaseHullType(hull);
    Mount first=hull->GetMaxFixedWeapons(),last=hull->GetMaxWeapons();
    for(Mount mount=first;mount<last;++mount){for(auto* l=core->GetPartTypes()->first();l;l=l->next())if(l->data()->GetEquipmentType()==ET_Weapon && hull->CanMount(l->data(),mount)){parent->CreateAndAddPart(l->data(),mount,1);break;}require(parent->GetMountedPart(ET_Weapon,mount),"network docked equipped turret");}
    m.tick(30);require(gunner->boardShip(parent->GetObjectID()),"network docked boarding queued");auto initialAction=client->lastActionNumber();std::optional<uint64_t> initialTransfer;
    m.wait([&]{if(!initialTransfer)initialTransfer=client->takeTransfer();return initialTransfer && gunner->state().passenger;},"network docked boarding published");
    require(*initialTransfer==initialAction && gunner->state().turret==first,"network docked initial seat/action identity");
    std::vector<IshipIGC*> others;for(Mount mount=first+1;mount<last;++mount){int id=m.authority->addLocalPilot("Occupied docked seat",0);auto* crew=core->GetShip(id);require(crew,"network docked crew occupant");crew->SetParentShip(parent);crew->SetTurretID(mount);crew->SetStation(station);others.push_back(crew);}
    m.tick(30);int value=parent->GetValue(),balance=m.authority->pilotBalance(child->GetObjectID()),identity=child->GetObjectID();
    gunner->clearMessages();gunner->clearSounds();require(gunner->boardShip(parent->GetObjectID(),false,true),"network full-ship reboarding queued");auto reboardAction=client->lastActionNumber();
    std::optional<uint64_t> completed;m.wait([&]{if(!completed)completed=client->takeTransfer();return completed.has_value();},"network full-ship reboarding completed");
    require(*completed==reboardAction && child->GetParentShip()==parent && child->GetTurretID()==first && gunner->state().passenger && gunner->state().turret==first,"network full-ship reboarding is rejected, cycles away from its released seat or completes another action");
    require(parent->GetValue()==value && parent->GetChildShips()->n()==last-first && m.authority->pilotBalance(identity)==balance && !client->takeDockRefitStarted(),"network reboarding changes parent inventory, crew, funds or starts an intermediate purchase");
    auto disembarked=std::find_if(gunner->state().messages.begin(),gunner->state().messages.end(),[](const auto& message){return message.tip && message.text=="You have disembarked.";});
    require(disembarked!=gunner->state().messages.end() && std::any_of(std::next(disembarked),gunner->state().messages.end(),[](const auto& message){return message.tip && message.text.find("You have boarded ")==0;}),"network reboarding loses original ordered personal feedback");
    require(Allegiance::LeaveDockedShip(others.front()),"network release comparison seat");child->SetTurretID(first+1);m.tick(30);
    require(gunner->boardShip(parent->GetObjectID(),false,true),"network first-free reboarding queued");auto firstFreeAction=client->lastActionNumber();completed.reset();
    m.wait([&]{if(!completed)completed=client->takeTransfer();return completed && child->GetTurretID()==first && gunner->state().turret==first;},"network teleport reboarding resets to first free seat");require(*completed==firstFreeAction,"network first-free reboarding completes another action");
    others.front()->SetParentShip(parent);others.front()->SetTurretID(first+1);others.front()->SetStation(station);parent->GetMountedPart(ET_Weapon,first)->Terminate();m.tick(30);
    auto rejected=client->metrics().rejected;require(gunner->boardShip(parent->GetObjectID(),false,true),"network unequipped released-seat request queued");m.wait([&]{return client->metrics().rejected>rejected;},"network authority rejects an unequipped seat");
    require(child->GetParentShip()==parent && child->GetTurretID()==first && m.authority->pilotBalance(identity)==balance && !client->takeTransfer(),"network failed reboarding detaches the existing seat, changes money or completes its lockdown");
    require(!gunner->boardShip(parent->GetObjectID()+65536,false,true),"network boarding aliases an out-of-range target before transmission");
    Net::Action invalid{};invalid.op=Net::Operation::Board;invalid.args[0]=parent->GetObjectID();invalid.args[1]=2;rejected=client->metrics().rejected;require(client->action(invalid),"invalid reboarding mode queued");m.wait([&]{return client->metrics().rejected>rejected;},"authority rejects a noncanonical reboarding mode");
    require(child->GetParentShip()==parent && child->GetTurretID()==first,"invalid boarding mode changes authority crew state");
    Settings settings;Frontend ui(settings);ui.originalInterface(m.assets);ui.setPlaying(true);ui.open(Screen::Hangar,&gunner->state());ui.teleports(gunner->teleportDestinations(),gunner->state());
    auto control=[&](MenuOp op,int value=0){auto i=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& item){return item.action.op==op && item.action.value==value && (op!=MenuOp::TeleportSelect || item.action.delta==0);});require(i!=ui.view().items.end(),"network current-station teleport control");return size_t(i-ui.view().items.begin());};
    ui.activate(control(MenuOp::TeleportSelect,station->GetObjectID()));require(ui.activate(control(MenuOp::TeleportActivate)).effect==FrontEffect::Disembark,"network current station dispatches a teleport instead of disembark");
    gunner->clearSounds();require(gunner->leaveShip(),"network current-station disembark queued");auto leaveAction=client->lastActionNumber();std::optional<PurchaseResult> purchase;
    m.wait([&]{if(!purchase)purchase=client->takePurchase();return purchase && !gunner->state().passenger;},"network current-station own loadout restored");
    require(purchase->accepted && purchase->complete && !purchase->launched && gunner->state().docked && child->GetStation()==station && parent->GetStation()==station && parent->GetChildShips()->n()==last-first-1 && child->GetObjectID()==identity,"network plain disembark moves/launches the parent, remaining crew or pilot identity");
    completed=client->takeTransfer();require(completed && *completed==leaveAction && client->takeDockRefitStarted(),"network disembark/refit loses its matching transfer identity");
    require(std::none_of(gunner->sounds().begin(),gunner->sounds().end(),[](const auto& event){return event.id==personalJumpSound;}),"network current-station disembark plays the teleport sound");
    ui.open(Screen::Hangar,&gunner->state());auto staleAction=client->lastActionNumber()+1;ui.transferring(Allegiance::CrewTransfer::Disembark,parent->GetName(),staleAction);
    child->SetStation(nullptr);m.wait([&]{return !gunner->state().docked;},"stale transfer fixture receives its new flight context");
    rejected=client->metrics().rejected;require(gunner->leaveShip() && client->lastActionNumber()==staleAction,"stale flight disembark queued with its original lockdown identity");m.wait([&]{return client->metrics().rejected>rejected;},"authority rejects stale flight disembark");
    uint64_t failureAction{};auto failure=client->takeRejection(&failureAction);require(failure.empty() && failureAction==staleAction,"suppressed original flight failure loses its action identity or invents a popup");
    require(!ui.actionRejected(failureAction,std::move(failure),true) && !ui.hasMessageBox() && child->GetCluster() && !child->GetStation(),"suppressed rejected transfer retains its lockdown or changes the flight context");
    require(!m.ca->metrics().missingBaselines && !m.cb->metrics().missingBaselines,"docked crew changes break canonical reports");
    m.report("Docked crew parity: full ship/first-free reboarding, atomic rejection, current-station disembark/refit, remaining crew, feedback, action identity and silent stale-flight release");
}
void crews(const std::filesystem::path &art) {
    Match m(art, 27632);
    m.join();
    auto *core = m.authority->core();
    Game *host = m.a->state().shipId == m.authority->playerShip()->GetObjectID() ? m.a.get() : m.b.get();
    Game *gunner = host == m.a.get() ? m.b.get() : m.a.get();
    FlightInput &gunnerInput = gunner == m.a.get() ? m.ia : m.ib;
    auto *parent = core->GetShip(host->state().shipId);
    int childID = gunner->state().shipId;
    auto& gunnerConnection=gunner==m.a.get()?m.ca:m.cb;
    auto tip=[](Game* game,std::string_view text,bool critical=false){return std::count_if(game->state().messages.begin(),game->state().messages.end(),[&](const auto& notice){return notice.text==text && notice.tip && notice.warning==critical && !notice.chat;});};
    auto heard=[](Game* game,int sound){return std::count_if(game->sounds().begin(),game->sounds().end(),[&](const auto& event){return event.id==sound && event.relative;});};
    auto childLoadout=gunner->currentLoadout();
    IhullTypeIGC *hull = nullptr;
    for (auto *l = core->GetHullTypes()->first(); l; l = l->next())
        if (l->data()->GetMaxWeapons() > l->data()->GetMaxFixedWeapons()) {
            hull = l->data();
            break;
        }
    require(hull, "turret hull fixture");
    while (auto *l = parent->GetParts()->first())
        l->data()->Terminate();
    parent->SetBaseHullType(hull);
    parent->SetAmmo(hull->GetMaxAmmo());
    parent->SetEnergy(hull->GetMaxEnergy());
    parent->SetFuel(hull->GetMaxFuel());
    for (Mount mount = hull->GetMaxFixedWeapons(); mount < hull->GetMaxWeapons(); ++mount) {
        for (auto *l = core->GetPartTypes()->first(); l; l = l->next())
            if (l->data()->GetEquipmentType() == ET_Weapon && hull->CanMount(l->data(), mount)) {
                auto *weapon = parent->CreateAndAddPart(l->data(), mount, 1);
                weapon->SetMountedFraction(1);
                weapon->Arm();
                break;
            }
    }
    m.tick(30);
    auto roster=gunner->playerPaneData();auto originalChild=std::find_if(roster.rows.begin(),roster.rows.end(),[&](auto& row){return row.id==childID;});
    require(originalChild!=roster.rows.end(),"gunner identity before boarding");auto childIdentity=originalChild->identity;
    host->clearMessages();host->clearSounds();gunner->clearMessages();gunner->clearSounds();
    require(gunner->boardShip(parent->GetObjectID()), "network gunner boards");
    auto boardAction=gunnerConnection->lastActionNumber();
    Settings transferSettings;Frontend transferUi(transferSettings);transferUi.originalInterface(m.assets);transferUi.setPlaying(true);transferUi.open(Screen::Hangar,&gunner->state());
    transferUi.transferring(Allegiance::CrewTransfer::Board,parent->GetName(),boardAction);
    // Run only reliable control processing. A successful receipt must not
    // unlock the old hangar/seat before a canonical report incorporates it.
    for(int i=0;i<500;++i){m.server->poll();m.ca->poll();m.cb->poll();
        require(!gunnerConnection->takeTransfer() && transferUi.hasMessageBox() && !gunner->state().passenger,"boarding receipt releases its lockdown against the old replica seat");
        if(core->GetShip(childID)->GetParentShip()==parent)break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    require(core->GetShip(childID)->GetParentShip()==parent,"authority accepted original boarding before publication");
    for(int i=0;i<20;++i){m.server->poll();m.ca->poll();m.cb->poll();require(!gunnerConnection->takeTransfer() && !gunner->state().passenger,"successful boarding receipt exposes a completion before publication");std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    gunnerConnection->reconnect();
    std::optional<uint64_t> boardedAction;
    m.wait([&]{if(!boardedAction)boardedAction=gunnerConnection->takeTransfer();return bool(boardedAction);},"canonical boarding completion releases original lockdown");
    require(*boardedAction==boardAction && gunner->state().passenger && gunner->state().shipId==childID,"boarding completion changes action/pilot identity, is lost on resume or precedes canonical crew state");
    transferUi.sync(gunner->state());transferUi.transferFinished(*boardedAction);require(!transferUi.hasMessageBox(),"canonical network boarding retains its original lockdown");
    m.tick(30);
    require(gunner->state().passenger && gunner->state().parentId == parent->GetObjectID() &&
                gunner->state().turret >= 0,
            "crew relationship replicated");
    auto ownBoard="You have boarded "+std::string(parent->GetName())+"'s ship as a turret gunner.";
    auto parentBoard=std::string(core->GetShip(childID)->GetName())+" has boarded your ship as a turret.";
    require(tip(gunner,ownBoard)==1 && tip(host,parentBoard,true)==1 && !tip(host,ownBoard) && !tip(gunner,parentBoard,true),"original boarding tips are absent, duplicated or delivered to another crew seat");
    require(heard(gunner,newNonCriticalMsgSound)==1 && heard(gunner,newCriticalMsgSound)==0 && heard(gunner,boardSound)==0 &&
            heard(host,newCriticalMsgSound)==1 && heard(host,newNonCriticalMsgSound)==0 && heard(host,boardSound)==1,"original boarding sounds are missing, duplicated or leak between pilot and gunner");
    auto boardNotices=gunner->state().messages.size(),parentNotices=host->state().messages.size();m.tick(60);
    require(gunner->state().messages.size()==boardNotices && host->state().messages.size()==parentNotices && heard(gunner,newNonCriticalMsgSound)==1 && heard(host,boardSound)==1,"periodic crew reports repeat boarding notifications or sounds");
    roster=gunner->playerPaneData();auto boarded=std::find_if(roster.rows.begin(),roster.rows.end(),[&](auto& row){return row.id==childID;});
    require(boarded!=roster.rows.end() && boarded->identity==childIdentity && boarded->parent==parent->GetObjectID() && boarded->ship.empty() && boarded->icon=="iconturretbmp","boarding hull reload preserves canonical player identity and original crew status presentation");
    require(gunner->state().sourcePilot==parent->GetName(),"replicated crew caption reads the parent pilot name");
    auto preview=gunner->previewLoadout(childLoadout);require(preview.valid && preview.affordable && preview.oldValue==0,"network passenger stages an affordable own ship without trading the parent");
    auto parentValue=parent->GetValue();auto parentIdentity=parent->GetObjectID();
    auto custom=childLoadout;std::erase_if(custom.parts,[](auto& part){return part.mount<0 && part.mount!=-5;});Loadouts customPresets;customPresets.remember(0,custom);gunner->loadoutPresets(customPresets);
    gunner->clearMessages();gunner->clearSounds();
    require(gunner->leaveShip(),"explicit network disembark queued");
    auto leaveAction=gunnerConnection->lastActionNumber();std::optional<PurchaseResult> disembark;
    m.wait([&]{if(!disembark)disembark=gunnerConnection->takePurchase();return bool(disembark);},"custom-zero disembark replacement completes");
    auto disembarked=gunner->currentLoadout();
    require(gunnerConnection->takeDockRefitStarted() && disembark->accepted && disembark->complete && !gunner->state().passenger && !gunner->state().pod && disembarked.hull==custom.hull &&
            std::count_if(disembarked.parts.begin(),disembarked.parts.end(),[](auto& part){return part.mount<0;})==1 && parent->GetValue()==parentValue,"disembark loses custom-zero fitting or changes the parent");
    require(tip(gunner,"You have disembarked.")==1 && heard(gunner,newNonCriticalMsgSound)==1,"ordered disembark/replacement loses original personal tip or sound");
    auto left=gunnerConnection->takeTransfer();require(left && *left==leaveAction,"disembark/replacement does not publish its matching transfer completion");
    require(gunner->boardShip(parentIdentity),"reboard after explicit disembark");m.wait([&]{return gunner->state().passenger;},"reboarding after custom-zero purchase replicates");
    auto invalid=childLoadout;require(!invalid.parts.empty(),"network invalid crew loadout fixture");invalid.parts.push_back(invalid.parts.front());
    gunner->clearMessages();gunner->clearSounds();
    auto rejected=(gunner==m.a.get()?m.ca:m.cb)->metrics().rejected;require(gunner->applyLoadout(invalid),"invalid passenger purchase queued for authority validation");
    m.wait([&]{return (gunner==m.a.get()?m.ca:m.cb)->metrics().rejected>rejected;},"authority rejects invalid passenger purchase before disembark");
    require(core->GetShip(childID)->GetParentShip()==parent && parent->GetValue()==parentValue,"rejected purchase preserves authoritative crew and parent equipment");
    require(tip(gunner,"You have disembarked.")==0,"rejected remote purchase emits a successful disembark tip");
    require(gunner->applyLoadout(childLoadout),"docked passenger purchases their own ship through the existing loadout intent");
    m.wait([&]{return !gunner->state().passenger && gunner->currentLoadout().hull==childLoadout.hull;},"authoritative disembark and purchase replicate with the same pilot identity");
    require(gunner->state().shipId==childID && parent->GetObjectID()==parentIdentity && parent->GetValue()==parentValue,"network purchase leaves parent and both pilot identities intact");
    require(tip(gunner,"You have disembarked.")==1,"implicit remote purchase disembark loses original console feedback");
    require(gunner->boardShip(parentIdentity),"hangar Launch boards the network gunner fixture");m.wait([&]{return gunner->state().passenger;},"hangar Launch boarding replicates");
    auto launchCredits=gunner->state().money;std::optional<PurchaseResult> launched;
    require(gunner->purchaseLaunchLoadout().accepted,"boarded pilot's original hangar Launch queues an atomic own-ship purchase");
    m.wait([&]{if(!launched)launched=gunnerConnection->takePurchase();return bool(launched);},"boarded pilot's hangar purchase/launch completes");
    auto* launchedShip=gunner->playerShip();
    require(launched->accepted && launched->complete && launched->launched && !gunner->state().passenger && !gunner->state().docked && launchedShip->GetBaseHullType()->GetObjectID()==custom.hull &&
            launchedShip->GetMountedPart(NA,-5) && std::all_of(custom.parts.begin(),custom.parts.end(),[&](const auto& part){auto* type=gunner->core()->GetPartType(part.id);auto* actual=launchedShip->GetMountedPart(part.mount<0?NA:type->GetEquipmentType(),part.mount);return actual && actual->GetPartType()==type;}),"boarded hangar Launch ignores own bank zero or launches the parent");
    require(gunner->state().shipId==childID && parent->GetStation() && parent->GetValue()==parentValue && gunner->state().money+gunner->shipValue()==launchCredits,"boarded hangar Launch changes pilot identity, parent equipment or wallet conservation");
    require(!gunnerConnection->takeDockRefitStarted(),"atomic crew launch starts a duplicate disembark replacement");
    unsigned visibleLaunchTicks=0;
    m.wait([&]{
        if(!gunner->state().cinematic)return true;
        require(!gunner->state().cinematicDeath && launchedShip->GetVisibleF() && gunner->camera({}).showPlayer,"network passenger launch hides its replacement ship during the external camera");
        auto count=gunner->scene(true).size();require(count==gunner->scene(false).size()+1,"network passenger launch omits its replacement hull from the scene");
        ++visibleLaunchTicks;return false;
    },"network passenger launch camera completes");
    require(visibleLaunchTicks>1,"network passenger launch visibility check missed the external camera");
    auto entering=gunner->camera({});require(entering.animating && entering.showPlayer,"network launch exit cuts to cockpit and hides its replacement hull");
    unsigned enteringTicks=0;
    m.wait([&]{
        auto camera=gunner->camera({});if(!camera.animating)return true;
        auto count=gunner->scene(camera.showPlayer).size();
        require(camera.showPlayer && launchedShip->GetVisibleF() && count==gunner->scene(false).size()+1,"network launch-to-cockpit transition loses its replacement hull");
        ++enteringTicks;return false;
    },"network launch-to-cockpit transition completes");
    require(enteringTicks>1 && !gunner->camera({}).showPlayer,"network cockpit transition is missing or retains its external hull after settling");
    require(core->GetIgcSite()->DockWithStationEvent(core->GetShip(childID),parent->GetStation()),"own-ship redocking after network passenger Launch");
    std::optional<PurchaseResult> restored;m.wait([&]{if(!restored)restored=gunnerConnection->takePurchase();return bool(restored) && gunner->state().docked;},"own-ship dock restoration after passenger Launch");
    require(restored->accepted && restored->complete && !restored->launched,"automatic redocking repeats the preceding launch intent");
    require(gunner->boardShip(parentIdentity),"purchased pilot boards again for flight crew tests");
    m.wait([&]{return gunner->state().passenger;},"reboarding replicates");
    host->launch();
    for (int i = 0; i < 600 && gunner->state().docked; ++i)
        m.tick(1);
    m.tick(15);
    require(!gunner->state().docked && gunner->state().passenger, "pilot launch carries network gunner");
    int ammo = parent->GetAmmo();
    float energy = parent->GetEnergy();
    gunnerInput.active = true;
    gunnerInput.fire = true;
    gunnerInput.yaw = .3f;
    gunnerInput.throttle = 1;
    m.tick(120);
    require(parent->GetAmmo() < ammo || parent->GetEnergy() < energy, "gunner controls fire original turret");
    require(gunner->state().ammo == parent->GetAmmo() || std::abs(gunner->state().ammo - parent->GetAmmo()) < 20,
            "turret ammo prediction remains close");
    gunnerInput.active = gunnerInput.fire = false;
    host->clearMessages();host->clearSounds();gunner->clearMessages();gunner->clearSounds();
    require(!gunner->promoteTurret(0) && !host->promoteTurret(4),"passengers and invalid turret shortcuts cannot promote");
    require(host->promoteTurret(gunner->state().turret-parent->GetBaseHullType()->GetMaxFixedWeapons()), "original turret shortcut promotes actual network gunner");
    m.tick(30);
    require(host->state().passenger && !gunner->state().passenger && host->state().parentId == childID,
            "promotion transfers parent hull and original identities");
    require(tip(gunner,"You have disembarked.")==1 && tip(gunner,"You have been promoted to pilot",true)==1 &&
            tip(host,"You have been demoted to a turret gunner",true)==1 && !tip(host,"You have been promoted to pilot",true) && !tip(gunner,"You have been demoted to a turret gunner",true),"promotion tips lose their original wording, critical status or audience");
    require(heard(gunner,newNonCriticalMsgSound)==1 && heard(gunner,newCriticalMsgSound)==1 && heard(host,newNonCriticalMsgSound)==1 && heard(host,newCriticalMsgSound)==1 && !heard(host,boardSound) && !heard(gunner,boardSound),"promotion tip sounds are duplicated or leak between the exchanged seats");
    auto *promoted = core->GetShip(childID);
    core->GetIgcSite()->KillShipEvent(core->GetLastUpdate(), promoted, nullptr, 100, promoted->GetPosition(),
                                      Vector::GetZero());
    m.tick(30);
    require(host->state().pod && gunner->state().pod && !host->state().passenger && !gunner->state().passenger,
            "parent death ejects both network crew into separate pods");
    require(gunner->state().cinematicDeath, "non-primary pilot receives original death camera");
    host->eject();
    gunner->eject();
    m.tick(30);
    require(host->state().docked && gunner->state().docked, "pod abandonment returns both crew to hangar");
    m.report("Crew: boarding, turret firing/prediction, promotion, death/pods, recovery");
}
void lobbyAlliances(const std::filesystem::path& art) {
    Match m(art,27719,false,true,false,Scenario::Conquest,1,32767,true,0,true,4);m.join();
    auto* owner=m.a->state().networkHost?m.ca.get():m.cb.get();auto* guest=owner==m.ca.get()?m.cb.get():m.ca.get();
    auto* other=guest->game();int ownerId=owner->game()->state().shipId,guestId=other->state().shipId,guestSide=other->state().playerSide;
    auto* world=m.authority->core();auto* first=world->GetShip(ownerId);auto* second=world->GetShip(guestId);auto match=owner->match();
    auto stale=m.authority->networkFrame(ownerId,0,0);
    Net::Action ready{};ready.op=Net::Operation::Ready;ready.args[0]=1;require(guest->action(ready),"alliance guest ready");
    Net::Action force{};force.op=Net::Operation::Lobby;force.args[0]=int(LobbyAction::ForceReady);force.args[1]=1;require(guest->action(force),"alliance guest force-ready");
    m.wait([&]{return other->state().networkReady && (other->state().forceReady&(1u<<guestSide));},"ready non-owner commander before alliance change");
    auto queue=[&](Net::Client& client,int side,int ally,bool disabled=false){emitLobbyNetworkActions({FrontEffect::ChangeAlliance,side,int(disabled),-1,ally},true,client.game()->state().networkReady,
        [&](const auto& action){require(client.action(action),"alliance menu intent queued");});};
    auto check=[&](int side,int ally,bool disabled=false){
        auto previous=m.authority->missionPolicy(match);std::array<char,6> expected{};
        OriginalAlliances::update(previous.alliances.data(),4,SideID(side),SideID(ally),[&](SideID id){return world->GetSide(id)->GetActiveF();},expected.data());
        bool allied=std::any_of(expected.begin(),expected.end(),[](char group){return group!=NA;});
        queue(*owner,side,ally,disabled);
        m.wait([&]{return m.a->state().alliances==expected && m.b->state().alliances==expected && m.authority->missionPolicy().alliances==expected &&
            m.a->state().maxImbalance==(allied?32767:32766) && m.b->state().maxImbalance==(allied?32767:32766);},"ordered alliance policy reaches both replicas");
        for(auto* game:{m.authority.get(),m.a.get(),m.b.get()}){
            auto policy=game->missionPolicy();require(policy.flags==unsigned(allied?(disabled?3:7):0),"alliance rules disagree between authority and replicas");
            for(SideID id=0;id<4;++id)require(game->core()->GetSide(id)->GetAllies()==expected[id],"IGC did not receive the canonical alliance groups");
        }
        require(world==m.authority->core() && owner->match()==match && guest->match()==match && world->GetShip(ownerId)==first && world->GetShip(guestId)==second,"alliance menu replaces the world, round or human ships");
        auto wire=Net::session(m.authority->sessionOptions());require(wire.alliances==expected && wire.rules.maxImbalance==(allied?32767:32766) && wire.rules.alliedSight==allied && wire.rules.alliedRipcord==(allied && !disabled),"alliance welcome/next-round rules do not match the live authority");
    };
    check(0,1);
    m.wait([&]{return !other->state().networkReady && !(other->state().forceReady&(1u<<guestSide));},"alliance change withdraws the non-owner commander's ready consent");
    require(owner->game()->state().networkReady,"alliance change makes its mission owner AFK");
    owner->game()->applyNetworkFrame(stale);
    require(owner->game()->missionPolicy().alliances==m.authority->missionPolicy().alliances &&
        IsideIGC::AlliedSides(owner->game()->core()->GetSide(0),owner->game()->core()->GetSide(1)),"delayed side/world report undoes the reliable alliance policy");
    m.tick(12);
    check(2,3);check(0,NA); // The second group survives removal of the first.
    check(2,NA);check(0,1,true);check(2,1,true); // Join a group; preserve ripcord opt-out.
    auto reject=[&](Net::Client& client,int side,int ally,const char* reason){auto before=m.authority->missionPolicy(match);queue(client,side,ally);std::string error;
        m.wait([&]{if(error.empty())error=client.takeRejection();return !error.empty();},reason);
        require(m.authority->missionPolicy(match)==before && client.match()==match,"rejected alliance changes authoritative policy or round");};
    reject(*guest,0,NA,"non-owner alliance rejection");reject(*owner,-1,0,"negative side rejected");reject(*owner,4,0,"absent side rejected");
    reject(*owner,0,0,"self alliance rejected");reject(*owner,0,4,"absent ally rejected");
    // A fresh peer receives policy in the welcome, without replaying actions.
    std::unique_ptr<Game> newcomer;auto third=m.client("Alliance observer",newcomer,3);
    for(int i=0;i<240 && !third->ready();++i){third->step({},1./Net::tickRate);m.tick(1);third->poll();}
    require(third->ready() && newcomer->missionPolicy().alliances==m.authority->missionPolicy().alliances && newcomer->missionPolicy().flags==3,"late-join alliance/ripcord policy missing from bootstrap");
    check(0,NA);check(1,NA);
    for(auto* client:{m.ca.get(),m.cb.get(),third.get()}){ready.args[0]=1;require(client->action(ready),"ready for alliance stage gate");}
    m.wait([&]{return m.a->state().networkReady && m.b->state().networkReady;},"stage-gate ready status");
    Net::Action start{};start.op=Net::Operation::Start;require(owner->action(start),"start after alliance removal");
    m.wait([&]{return world->GetMissionStage()!=STAGE_NOTSTARTED;},"alliance stage gate begins countdown");
    reject(*owner,0,1,"started/counting-down alliance rejection");
    m.report("Lobby alliances: frozen original groups, two groups/merges/removal, owner/stage/ID authority, ready withdrawal, rules/ripcord opt-out, delayed report immunity, late join and stable world/ship identities");
}
void teamActivity(const std::filesystem::path& art){
    Match m(art,27720,false,true,false,Scenario::Conquest,1,32767,true,1,true,4);m.join();
    auto* owner=m.a->state().networkHost?m.ca.get():m.cb.get();auto* guest=owner==m.ca.get()?m.cb.get():m.ca.get();
    auto* world=m.authority->core();auto match=owner->match();
    std::array<int,4> bases{},ships{};for(SideID side=0;side<4;++side){bases[side]=world->GetSide(side)->GetStations()->n();ships[side]=world->GetSide(side)->GetShips()->n();}
    auto queue=[&](Net::Client& client,int side,int active){Net::Action action{};action.op=Net::Operation::Lobby;action.args[0]=int(LobbyAction::TeamActive);action.args[1]=active;action.args[2]=side;require(client.action(action),"team activity queued");};
    auto reject=[&](Net::Client& client,int side,int active,const char* reason){auto rejected=client.metrics().rejected;auto mask=m.authority->sessionOptions().inactiveTeams;queue(client,side,active);
        m.wait([&]{return client.metrics().rejected>rejected;},reason);require(m.authority->sessionOptions().inactiveTeams==mask && client.match()==match,"rejected team activity changes the world or round");};
    auto change=[&](int side,bool active){
        Settings settings;Frontend ui(settings);ui.originalInterface(m.assets);ui.networkSession(owner->game()->sessionOptions());ui.setPlaying(true);ui.open(Screen::RemoteLobby,&owner->game()->state());
        auto name="activeTeamButtonPane"+std::to_string(side+1);auto i=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& item){return item.control==name;});require(i!=ui.view().items.end() && i->enabled && i->selected!=active,"original empty team checkbox fixture");auto index=size_t(i-ui.view().items.begin());
        ui.pointerPress(index);auto result=ui.pointerRelease(index);require(result.effect==FrontEffect::LobbyAction && result.command==int(LobbyAction::TeamActive) && result.targetId==side && result.value==int(active),"original checkbox emits the wrong network intent");
        size_t actions=0;emitLobbyNetworkActions(result,true,owner->game()->state().networkReady,[&](const auto& action){require(owner->action(action),"authored team toggle emitted");++actions;});require(actions==1,"team activity wakes an AFK pilot or sends redundant records");
        m.wait([&]{return world->GetSide(SideID(side))->GetActiveF()==active && m.a->state().teamScores[side].active==active && m.b->state().teamScores[side].active==active;},"active side reaches both encrypted replicas");
        require(world==m.authority->core() && owner->match()==match && guest->match()==match,"team activity replaces pregame world or human identities");
        for(SideID id=0;id<4;++id)require(world->GetSide(id)->GetStations()->n()==bases[id] && world->GetSide(id)->GetShips()->n()==ships[id],"pregame toggling prematurely removes bases/drones");
    };
    change(2,false);require(m.authority->sessionOptions().inactiveTeams==4 && m.authority->core()->GetMissionParams()->bAllowEmptyTeams,"inactive side does not enable original empty-team policy");
    reject(*guest,3,0,"non-owner team activity rejected");reject(*owner,owner->game()->state().playerSide,0,"occupied owner team rejected");reject(*owner,-1,0,"negative side rejected");reject(*owner,4,0,"absent side rejected");reject(*owner,3,2,"invalid activity value rejected");
    Net::Action join{};join.op=Net::Operation::Team;join.args[0]=2;auto rejected=guest->metrics().rejected;require(guest->action(join),"inactive team join queued");m.wait([&]{return guest->metrics().rejected>rejected;},"inactive team cannot accept a join request");
    auto params=*world->GetMissionParams();params.bObjectModelCreated=true;world->SetMissionParams(&params);reject(*owner,3,0,"fixed-world team activity rejected");
    m.wait([&]{return (owner->game()->state().lobbyFlags&Allegiance::LobbyObjectModelCreated)!=0;},"fixed-world policy published");
    {auto rejected=owner->metrics().rejected;require(owner->configure(m.authority->sessionOptions()),"fixed-world settings queued");m.wait([&]{return owner->metrics().rejected>rejected;},"fixed-world settings rejected");require(m.authority->core()==world && owner->match()==match,"fixed-world settings replace the original world");}
    params.bObjectModelCreated=false;world->SetMissionParams(&params);m.wait([&]{return !(owner->game()->state().lobbyFlags&Allegiance::LobbyObjectModelCreated);},"editable world policy restored");
    change(2,true);m.wait([&]{return (m.a->state().lobbyFlags&Allegiance::LobbyRequirePlayers) && (m.b->state().lobbyFlags&Allegiance::LobbyRequirePlayers);},"all-active minimum-player policy replicated");
    require(!world->GetMissionParams()->bAllowEmptyTeams,"all-active team policy fails to restore minimum players");
    Net::Action alliance{};alliance.op=Net::Operation::Lobby;alliance.args[0]=int(LobbyAction::Alliance);alliance.args[1]=2;alliance.args[2]=3;require(owner->action(alliance),"empty side alliance queued");
    m.wait([&]{return m.a->state().alliances[2]!=NA && m.b->state().alliances[2]!=NA;},"empty-side alliance reaches replicas");
    change(2,false);m.wait([&]{return m.a->state().alliances[2]==NA && m.b->state().alliances[3]==NA;},"inactive side leaves and normalizes its alliance");
    // Accepted settings preserve activity; the incoming raw policy cannot
    // bypass the owner/stage/occupancy validation of the checkbox operation.
    auto configured=m.authority->sessionOptions();configured.inactiveTeams=0;configured.rules.friendlyFire=true;require(owner->configure(configured),"settings with forged activity policy queued");
    m.wait([&]{return owner->ready() && guest->ready() && owner->match()>match && guest->match()==owner->match();},"configured round reaches both peers");
    world=m.authority->core();match=owner->match();require(!world->GetSide(2)->GetActiveF() && !m.a->state().teamScores[2].active && !m.b->state().teamScores[2].active && world->GetMissionParams()->bAllowEmptyTeams,"configuration bypasses or loses current activity");
    for(SideID side=0;side<4;++side){bases[side]=world->GetSide(side)->GetStations()->n();ships[side]=world->GetSide(side)->GetShips()->n();}
    change(3,false);
    for(auto* client:{owner,guest}){Net::Action ready{};ready.op=Net::Operation::Ready;ready.args[0]=1;require(client->action(ready),"team activity pilots ready");}
    m.wait([&]{return m.a->state().networkReady && m.b->state().networkReady;},"ready before inactive-team start");
    Net::Action start{};start.op=Net::Operation::Start;require(owner->action(start),"start with two inactive teams");m.wait([&]{return world->GetMissionStage()==STAGE_STARTING;},"team activity countdown");reject(*owner,2,1,"countdown team activity rejected");
    m.wait([&]{return !m.a->state().networkLobby && !m.b->state().networkLobby;},"inactive-team match launches");
    for(auto* game:{m.authority.get(),m.a.get(),m.b.get()})for(SideID side=0;side<4;++side){auto* team=game->core()->GetSide(side);require(team->GetActiveF()==(side<2),"started side activity differs across replicas");
        if(side>=2)require(!team->GetStations()->n() && !team->GetShips()->n(),"inactive side bases/drones survive into combat");
        else if(game->role()==GameRole::Authority || game->state().playerSide==side)require(team->GetStations()->n()==bases[side],"active authority/own-faction bases removed at start");}
    require(!m.authority->state().complete && !m.a->state().complete,"inactive-side cleanup creates a spurious victory");
    int losing=guest->game()->state().playerSide;while(auto* base=world->GetSide(SideID(losing))->GetStations()->first())world->GetIgcSite()->KillStationEvent(base->data(),nullptr,1000);
    m.wait([&]{return owner->ready() && guest->ready() && owner->match()>match && m.a->state().networkLobby && m.b->state().networkLobby;},"inactive-team match returns to its original team lobby");
    for(auto* game:{m.authority.get(),m.a.get(),m.b.get()}){require(!game->sessionOptions().inactiveTeams && !game->core()->GetMissionParams()->bAllowEmptyTeams,"new round retains disabled teams or relaxed minimum-player policy");for(SideID side=0;side<4;++side){require(game->core()->GetSide(side)->GetActiveF(),"new round does not restore all active sides");if(game->role()==GameRole::Authority || game->state().playerSide==side)require(game->core()->GetSide(side)->GetStations()->n(),"new round does not restore authority/own-faction bases");}}
    m.report("Team activity: original checkbox intent, owner/occupancy/ID/fixed-world/countdown authority, no inactive admission, compact bootstrap, alliance normalization, settings preservation, start cleanup and next-round reset");
}
void teamSettings(const std::filesystem::path& art) {
    Match m(art,27658,false,true,false,Scenario::Conquest,1,32767,true,1);m.join();
    auto* client=m.a->state().networkHost?m.cb.get():m.ca.get();
    auto* replica=client==m.cb.get()?m.b.get():m.a.get();auto* other=replica==m.b.get()?m.a.get():m.b.get();
    auto* otherClient=client==m.cb.get()?m.ca.get():m.cb.get();
    int side=replica->state().playerSide,otherSide=other->state().playerSide;
    int id=replica->state().shipId,otherId=other->state().shipId;
    auto* world=m.authority->core();auto* pilot=world->GetShip(id);auto* opposite=world->GetShip(otherId);auto match=client->match();
    int previous=world->GetSide(SideID(side))->GetCivilization()->GetObjectID(),chosen=-1;
    for(auto* l=world->GetCivilizations()->first();l;l=l->next())if(l->data()->GetObjectID()<300 && l->data()->GetObjectID()!=previous){chosen=l->data()->GetObjectID();break;}
    require(chosen>=0 && replica->state().commander && !replica->state().networkHost,"independent team commander fixture");
    auto emit=[&](const FrontResult& result){emitLobbyNetworkActions(result,true,replica->state().networkReady,[&](const Net::Action& record){require(client->action(record),"original lobby UI action queued");});};
    Net::Action personal{};personal.op=Net::Operation::Ready;personal.args[0]=1;require(client->action(personal),"ready commander detail fixture");
    Net::Action consent{};consent.op=Net::Operation::Lobby;consent.args[0]=int(LobbyAction::ForceReady);consent.args[1]=1;require(client->action(consent),"force-ready commander detail fixture");
    m.wait([&]{return replica->state().networkReady && (replica->state().forceReady&(1u<<side));},"commander ready before game details");
    emit({FrontEffect::LobbyVisit,1,0});
    m.wait([&]{return !replica->state().networkReady && !(replica->state().forceReady&(1u<<side));},"opening original game details withdraws both forms of readiness");
    Net::Action admission{};admission.op=Net::Operation::Lobby;admission.args[0]=int(LobbyAction::AutoAccept);admission.args[1]=0;require(otherClient->action(admission),"opposing team disables automatic admission");
    m.wait([&]{return !(replica->state().autoAccept&(1u<<otherSide));},"original pending request fixture");
    auto pending=[&]{for(const auto& p:replica->state().networkPilots)if(p.controlled)return p.requested;return noTeamRequest;};
    emit({FrontEffect::NetworkJoinTeam,otherSide});m.wait([&]{return pending()==otherSide && replica->state().networkReady;},"JOIN wakes the AFK commander and requests the selected team");
    emit({FrontEffect::LobbyVisit,0,1});m.wait([&]{return pending()==noTeamRequest;},"opening previous scores cancels a pending request");
    require(replica->state().networkReady && replica->state().playerSide==noTeam,"previous-score cancellation changes readiness or the pending applicant's No Team seat");
    emit({FrontEffect::NetworkJoinTeam,otherSide});m.wait([&]{return pending()==otherSide;},"second pending request before manual cancel");
    personal.args[0]=0;require(client->action(personal),"manual AFK before JOIN/CANCEL");m.wait([&]{return !replica->state().networkReady;},"manual AFK replicated");
    emit({FrontEffect::LobbyAction,0,int(LobbyAction::CancelRequest)});m.wait([&]{return pending()==noTeamRequest && replica->state().networkReady;},"JOIN/CANCEL wakes the AFK requester before cancellation");
    emit({FrontEffect::NetworkJoinTeam,side});m.wait([&]{return replica->state().playerSide==side && replica->state().commander;},"cancelled requester rejoins its original command seat");
    require(world==m.authority->core() && client->match()==match && world->GetShip(id)==pilot,"lobby detail/join/cancel actions replace the world, round or pilot");
    auto action=[&](int civ,std::string_view name){Net::Action a{};a.op=Net::Operation::Lobby;a.args[0]=int(LobbyAction::TeamSettings);a.args[1]=civ;Net::text(a.text,name);require(client->action(a),"team settings intent queued");};
    action(previous,"Veteran wing");m.wait([&]{return replica->state().teamScores[side].name=="Veteran wing" && other->state().teamScores[side].name=="Veteran wing";},"team name replicated across visibility domains");
    require(world==m.authority->core() && client->match()==match && world->GetShip(id)==pilot && world->GetShip(otherId)==opposite,"renaming keeps every pilot, world and round");
    Net::Action ready{};ready.op=Net::Operation::Ready;ready.args[0]=1;require(otherClient->action(ready),"opposing personal ready queued");
    Net::Action force{};force.op=Net::Operation::Lobby;force.args[0]=int(LobbyAction::ForceReady);force.args[1]=1;require(otherClient->action(force),"opposing team ready queued");
    m.wait([&]{return other->state().networkReady && (replica->state().forceReady&(1u<<otherSide));},"opposing commander's consent recorded");
    require(replica->mutePilot(otherId,true),"recipient policy before faction change");
    action(chosen,"Seasoned pilots");
    m.wait([&]{return replica->state().teamScores[side].civilization==chosen && other->state().teamScores[side].civilization==chosen &&
        replica->state().teamScores[side].name=="Seasoned pilots" && !(other->state().forceReady&(1u<<otherSide)) && !other->state().networkReady;},"faction and name replicate and withdraw opposing ready consent");
    require(world==m.authority->core() && client->match()==match && world->GetShip(id)==pilot && world->GetShip(otherId)==opposite &&
        replica->state().shipId==id && other->state().shipId==otherId && replica->state().commander && other->state().networkHost,"faction change keeps identities, leadership and ownership");
    auto* faction=world->GetSide(SideID(side));auto* civ=faction->GetCivilization();
    require(faction->GetInitialTechs()==civ->GetBaseTechs() && faction->GetDevelopmentTechs()==civ->GetBaseTechs(),"original faction initial/development technology restored");
    int miners=0;for(auto* l=faction->GetShips()->first();l;l=l->next())if(l->data()->GetPilotType()==c_ptMiner)++miners;
    require(miners==1 && pilot->GetStation() && pilot->GetStation()->GetBaseStationType()==civ->GetInitialStationType() &&
        pilot->GetStation()->CanBuy(pilot->GetBaseHullType()),"faction bases, pregame hulls and initial miners are consistent");
    require(m.authority->pilotBalance(id)==Money(world->GetFloatConstant(c_fcidStartingMoney)*(m.options.rules.startingMoney+civ->GetBonusMoney())) &&
        std::any_of(replica->state().networkPilots.begin(),replica->state().networkPilots.end(),[&](const auto& p){return p.id==otherId && p.muted;}),"faction starting money matches IGC and recipient policy survives");
    action(32767,"Invalid faction");m.tick(30);require(faction->GetCivilization()==civ && replica->state().teamScores[side].name=="Seasoned pilots","absent factions rejected atomically");
    action(chosen,std::string(25,'X'));m.tick(30);require(replica->state().teamScores[side].name=="Seasoned pilots","oversized team names rejected");
    auto saved=Net::options(Net::session(m.authority->sessionOptions()),side,"Test");
    require(saved.teamNames[side]=="Seasoned pilots" && saved.teamCivilizations[side]>=0,"team settings retained in raw session records for reconnect/configuration/next round");
    auto settings=m.authority->sessionOptions();settings.rules.lockTeamSettings=true;
    require(otherClient->configure(settings),"owner locks team settings");m.wait([&]{return m.a->sessionOptions().rules.lockTeamSettings && m.b->sessionOptions().rules.lockTeamSettings && m.ca->ready() && m.cb->ready();},"locked settings delivered");
    replica=client==m.cb.get()?m.b.get():m.a.get();
    action(previous,"Forbidden");m.tick(30);require(replica->state().teamScores[side].name=="Seasoned pilots" && replica->state().teamScores[side].civilization==chosen,"authority enforces original team settings lock");
    m.report("Team settings: original leader permissions, in-place faction/base/hull/miner/money change, ready invalidation, identities, recipient state and locks");
}
void lobby(const std::filesystem::path &art) {
    Match m(art, 27634, false, true);
    m.join();
    require(m.a->state().networkLobby && m.b->state().networkLobby && m.server->tick() == 0,
            "lobby pauses authoritative simulation");
    auto roster=[&](Game& game,int id)->const LobbyPilot& {
        auto it=std::find_if(game.state().networkPilots.begin(),game.state().networkPilots.end(),[&](const auto& pilot){return pilot.id==id;});
        require(it!=game.state().networkPilots.end(),"lobby preserves the peer's authoritative pilot identity");return *it;
    };
    int alpha=m.a->state().shipId,bravo=m.b->state().shipId;
    require(m.a->state().networkPilots.size()==2 && roster(*m.a,alpha).controlled && !roster(*m.a,bravo).controlled &&
        roster(*m.a,alpha).host==m.a->state().networkHost && roster(*m.b,bravo).host==m.b->state().networkHost,
        "lobby reports preserve independent identity, controlled-pilot and host roles");
    auto* captain=m.a->state().commander?m.a.get():m.b.get();auto* successor=captain==m.a.get()?m.b.get():m.a.get();
    bool captainOwns=captain->state().networkHost;
    auto* successorClient=successor==m.a.get()?m.ca.get():m.cb.get();
    int captainId=captain->state().shipId,successorId=successor->state().shipId;
    auto captainMoney=m.authority->pilotBalance(captainId),successorMoney=m.authority->pilotBalance(successorId);
    auto* captainShip=m.authority->core()->GetShip(captainId);auto* successorShip=m.authority->core()->GetShip(successorId);
    auto* captainDonation=captainShip->GetAutoDonate();auto* successorDonation=successorShip->GetAutoDonate();
    successorShip->SetWingID(3);
    auto rejected=successorClient->metrics().rejected;
    require(successor->makeTeamLeader(captainId),"unauthorized leadership request queued");
    m.wait([&]{return successorClient->metrics().rejected>rejected;},"noncommander cannot assign a team leader");
    require(captain->makeTeamLeader(successorId),"original Make Leader action queued in pregame lobby");
    m.wait([&]{return roster(*m.a,successorId).commander && roster(*m.b,successorId).commander &&
        !roster(*m.a,captainId).commander && successor->state().commander && (!captainOwns || (successor->state().networkHost && !captain->state().networkHost));},"new leader and owning-side host roles reach both lobby rosters");
    require(successorShip->GetWingID()==0 && m.authority->pilotBalance(captainId)==captainMoney &&
        m.authority->pilotBalance(successorId)==successorMoney && captainShip->GetAutoDonate()==captainDonation && successorShip->GetAutoDonate()==successorDonation,
        "Make Leader assigns command wing without transferring money or automatic donations");
    require(successor->makeTeamLeader(captainId),"original leader restores command");
    m.wait([&]{return captain->state().commander && roster(*m.a,captainId).commander && roster(*m.b,captainId).commander && (!captainOwns || captain->state().networkHost);},"restored commander/owner reaches both rosters");
    captainShip->SetName("Duplicate");successorShip->SetName("Duplicate");
    m.wait([&]{return roster(*m.a,alpha).name=="Duplicate" && roster(*m.a,bravo).name=="Duplicate";},"duplicate names retain independent network identities");
    Net::Client *host = m.a->state().networkHost ? m.ca.get() : m.cb.get();
    auto settings = m.options;
    settings.seed = 49;
    settings.map = 1;
    settings.rules.startCountdown = 1;
    auto *other = host == m.ca.get() ? m.cb.get() : m.ca.get();
    require(other->configure(settings), "non-host configuration queued");
    m.tick(30);
    require(m.a->sessionOptions().seed != 49, "non-host cannot change mission settings");
    Net::Action resumingReady{};
    resumingReady.op = Net::Operation::Ready;
    resumingReady.args[0] = 1;
    require(other->configure(settings) && other->action(resumingReady), "mixed retained control sequence queued");
    other->reconnect();
    m.wait([&] { return other->ready(); }, "mixed configuration/action sequence resumes");
    m.wait([&] { return (other == m.ca.get() ? m.a : m.b)->state().networkReady; },
           "unacknowledged ready action survives reconnect after configuration");
    resumingReady.args[0] = 0;
    require(other->action(resumingReady), "clear resumed readiness");
    m.wait([&] { return !(other == m.ca.get() ? m.a : m.b)->state().networkReady; }, "readiness cleared");
    require(host->configure(settings), "host configuration queued");
    m.wait(
        [&] {
            return m.a->sessionOptions().seed == 49 && m.b->sessionOptions().seed == 49 && m.ca->ready() &&
                   m.cb->ready();
        },
        "host map/rules recreate both replicas");
    Net::Action team{};
    team.op = Net::Operation::Team;
    team.args[0] = 1;
    auto match=m.cb->match();auto* world=m.authority->core();auto* oldPilot=m.authority->core()->GetShip(m.b->state().shipId);
    auto oldId=m.b->state().shipId,oldOther=m.a->state().shipId;
    require(m.cb->action(team), "pilot team change queued");
    m.wait([&] { return m.b->sessionOptions().team == 1 && m.ca->ready() && m.cb->ready(); },
        "team change authenticated and replicated");
    require(m.cb->match()==match && m.ca->match()==match && m.authority->core()==world &&
        world->GetShip(oldId)==oldPilot && m.b->state().shipId==oldId && m.a->state().shipId==oldOther,
        "lobby team change preserves the round, world and every authenticated pilot identity");
    alpha=m.a->state().shipId;bravo=m.b->state().shipId;
    require(roster(*m.a,bravo).team==1 && !roster(*m.a,bravo).controlled && m.a->mutePilot(bravo,true) && roster(*m.a,bravo).muted,
        "opponent lobby pilot can be muted by identity without relying on a visible IGC ship");
    m.tick(30);
    require(roster(*m.a,bravo).muted && !roster(*m.b,bravo).muted && !m.a->mutePilot(alpha,true) && !m.a->mutePilot(SHRT_MAX,true),
        "lobby mute remains private, survives reports and rejects self/unknown identities");
    require(m.a->mutePilot(bravo,false) && !roster(*m.a,bravo).muted,"lobby identity can be unmuted");
    Net::Action start{};
    start.op = Net::Operation::Start;
    require(host->action(start), "unready start request queued");
    m.tick(30);
    require(m.server->tick() == 0, "host cannot bypass pilot readiness");
    Net::Action ready{};
    ready.op = Net::Operation::Ready;
    ready.args[0] = 1;
    require(m.ca->action(ready) && m.cb->action(ready), "both pilots mark ready");
    m.wait([&] { return m.a->state().networkReady && m.b->state().networkReady; }, "readiness replicated");
    require(host->action(start), "ready host starts match");
    m.wait([&] { return !m.a->state().networkLobby && !m.b->state().networkLobby; }, "countdown enters the match");
    require(m.server->tick() > 0, "authority simulation starts after countdown");
    int id = m.a->state().shipId, balance = m.authority->pilotBalance(id);
    m.ca->reconnect();
    m.wait([&] { return m.ca->ready(); }, "reconnection completes");
    require(m.a->state().shipId == id && m.authority->pilotBalance(id) == balance && m.server->clients() == 2,
            "resume keeps one pilot identity and account");
    auto *enemyBase = m.authority->core()->GetSide(1)->GetStations()->first()->data();
    char stationLoss[512];snprintf(stationLoss,sizeof(stationLoss),START_COLOR_STRING "%s's %s" END_COLOR_STRING " in %s was destroyed.",
        (PCC)ConvertColorToString(enemyBase->GetSide()->GetColor()),enemyBase->GetSide()->GetName(),enemyBase->GetName(),enemyBase->GetCluster()->GetName());
    m.authority->core()->GetIgcSite()->KillStationEvent(enemyBase, nullptr, 1);
    m.wait([&] { return m.a->state().networkLobby && m.b->state().networkLobby && m.a->state().endgame.frozen && m.b->state().endgame.frozen; }, "authoritative conquest ending returns peers to lobby with frozen results");
    require(m.a->state().endgame.frozen && m.a->state().endgame==m.b->state().endgame && m.a->state().endgame.winner==0 && m.a->state().endgame.players.size()==2,
        "encrypted recipients receive identical frozen public postgame records with the authoritative winner");
    auto result=m.a->state().endgame;m.tick(15);require(m.a->state().endgame==result && m.b->state().endgame==result,"completed results stay immutable across subsequent reports");
    require(std::any_of(m.b->state().messages.begin(), m.b->state().messages.end(),
                        [&](auto &message) { return message.tip && message.warning && message.text==stationLoss; }),
            "station loss announces to affected opponent pilot");
    m.wait([&] { return m.a->state().networkHost != m.b->state().networkHost; }, "one host after reconnect");
    host = m.a->state().networkHost ? m.ca.get() : m.cb.get();
    m.wait([&] { return m.a->state().networkLobby && m.b->state().networkLobby && m.a->sessionOptions().seed == 50; },
           "finished round automatically returns all peers to readiness lobby");
    require(!m.a->state().networkReady && !m.b->state().networkReady,"new round clears readiness");
    require(m.ca->action(ready) && m.cb->action(ready),"prepare second round");
    m.wait([&]{return m.a->state().networkReady && m.b->state().networkReady;},"second-round readiness replicated");
    require(host->action(start),"start second round");m.wait([&]{return !m.a->state().networkLobby && !m.b->state().networkLobby;},"second round starts");
    int home=m.a->playerShip()->GetStation()->GetCluster()->GetObjectID(),view=-1;
    for(auto* l=m.a->core()->GetClusters()->first();l;l=l->next())if(l->data()->GetObjectID()!=home){view=l->data()->GetObjectID();break;}
    require(view>=0,"separate command-sector fixture");m.a->commandView(true);require(m.a->viewSector(view),"view another sector while retaining prior results");
    m.wait([&]{return m.authority->networkViewSector(m.a->state().shipId)==view;},"remote command sector reaches the authority");
    auto* core=m.authority->core();auto* pilot=core->GetShip(m.a->state().shipId);DataShipIGC drone{};
    drone.shipID=core->GenerateNewShipID();drone.hullID=pilot->GetBaseHullType()->GetObjectID();drone.sideID=pilot->GetSide()->GetObjectID();
    drone.pilotType=c_ptMiner;drone.baseObjectID=NA;drone.partsOffset=sizeof(drone);strcpy(drone.name,"Manifest drone");
    auto* miner=static_cast<IshipIGC*>(core->CreateObject(core->GetLastUpdate(),OT_ship,&drone,sizeof(drone)));require(miner,"maximum-arena feedback fixture");
    miner->SetCluster(pilot->GetStation()->GetCluster());miner->SetPosition(Vector(50000,0,0));miner->Release();
    core->GetIgcSite()->KillShipEvent(core->GetLastUpdate(),miner,nullptr,100,miner->GetPosition(),Vector::GetZero());
    auto reports=m.ca->metrics().snapshots;m.tick(18);require(!m.ca->closed() && m.ca->metrics().snapshots>reports && m.a->state().endgame==result &&
        std::any_of(m.a->state().messages.begin(),m.a->state().messages.end(),[](const auto& message){return message.tip && message.warning && message.text.find("Manifest drone")!=std::string::npos;}),
        "maximum arena manifest loses shared feedback, prior results or distinct physical/view sectors");
    auto* secondBase=m.authority->core()->GetSide(0)->GetStations()->first()->data();m.authority->core()->GetIgcSite()->KillStationEvent(secondBase,nullptr,1);
    m.wait([&]{return m.a->state().networkLobby && m.b->state().networkLobby && m.a->state().endgame.winner==1 && m.b->state().endgame.winner==1;},"second round replaces previous frozen results and returns to lobby");
    team.args[0]=0;require(m.cb->action(team),"reunite commander succession fixture");
    m.wait([&]{return m.a->sessionOptions().team==0 && m.b->sessionOptions().team==0 &&
        m.a->state().commander!=m.b->state().commander;} ,"both pilots rejoin the same side");
    auto* owner=m.a->state().networkHost?m.ca.get():m.cb.get();auto* staying=owner==m.ca.get()?m.b.get():m.a.get();
    team.args[0]=1;auto roundMatch=owner->match();
    require(owner->action(team),"owning commander requests another faction");
    auto* departing=owner==m.ca.get()?m.a.get():m.b.get();
    m.wait([&]{return staying->state().networkHost && staying->state().commander &&
        departing->sessionOptions().team==1 && !departing->state().networkHost;} ,"old side's elected successor receives mission ownership in both replicas");
    require(owner->match()==roundMatch && !((owner==m.ca.get()?m.a.get():m.b.get())->state().networkHost),
        "departing owning commander does not take host controls to their new faction");
    m.report("Lobby: configuration, team changes, readiness, countdown, reconnect, victory, next round");
}
void admission(const std::filesystem::path& art) {
    Match m(art,27659,false,true);m.join();
    auto member=[&](Game& game,int id){
        auto i=std::find_if(game.state().networkPilots.begin(),game.state().networkPilots.end(),[&](const auto& p){return p.id==id;});
        require(i!=game.state().networkPilots.end(),"public mission membership missing");return *i;
    };
    auto* leader=m.a->state().commander?m.ca.get():m.cb.get();
    auto* applicant=leader==m.ca.get()?m.cb.get():m.ca.get();
    auto* leaderGame=leader==m.ca.get()?m.a.get():m.b.get();
    auto* applicantGame=leader==m.ca.get()?m.b.get():m.a.get();
    int leaderId=leaderGame->state().shipId,applicantId=applicantGame->state().shipId;
    auto control=[&](Net::Client& client,LobbyAction kind,int value=0,int target=-1){
        Net::Action action{};action.op=Net::Operation::Lobby;action.args={int(kind),value,target,0};
        require(client.action(action),"lobby control queued");
    };
    auto request=[&](Net::Client& client,int side){Net::Action action{};action.op=Net::Operation::Team;action.args[0]=side;require(client.action(action),"team request queued");};
    control(*leader,LobbyAction::AutoAccept,0);
    m.wait([&]{return !(m.a->state().autoAccept&1) && !(m.b->state().autoAccept&1);},"leader disables automatic admission");
    Net::Action ready{};ready.op=Net::Operation::Ready;ready.args[0]=1;require(applicant->action(ready),"personal ready queued");
    request(*applicant,noTeam);
    m.wait([&]{return member(*leaderGame,applicantId).team==noTeam;},"No Team departure replicated");
    require(member(*applicantGame,applicantId).ready && applicantGame->state().networkLobby && applicantGame->scene(false).empty() && applicantGame->contacts().empty() && !applicantGame->state().commandView,
            "No Team preserves AFK state and clears old faction presentation");
    request(*applicant,0);
    m.wait([&]{return member(*leaderGame,applicantId).requested==0 && member(*applicantGame,applicantId).team==noTeam;},"original request waits for commander acceptance");
    control(*leader,LobbyAction::LockSides,1);
    m.wait([&]{return m.a->state().lobbyFlags&2;},"host locks sides");
    auto rejected=leader->metrics().rejected;control(*leader,LobbyAction::Accept,0,applicantId);
    m.wait([&]{return leader->metrics().rejected>rejected;},"acceptance revalidates locked sides");
    control(*leader,LobbyAction::LockSides,0);control(*leader,LobbyAction::Reject,0,applicantId);
    m.wait([&]{return member(*leaderGame,applicantId).requested==noTeamRequest;},"request rejection retains No Team");
    auto rejectedCaption=std::string(Allegiance::LobbyJoinFailureMessage(Allegiance::LobbyJoinFailure::Rejected));
    m.wait([&]{return std::any_of(applicantGame->state().messages.begin(),applicantGame->state().messages.end(),[&](const auto& message){return message.modal && message.text==rejectedCaption;});},"commander rejection reaches the applicant with its original modal caption");
    request(*applicant,0);m.wait([&]{return member(*leaderGame,applicantId).requested==0;},"second request");
    control(*applicant,LobbyAction::CancelRequest);m.wait([&]{return member(*leaderGame,applicantId).requested==noTeamRequest;},"applicant cancels outstanding request");
    request(*applicant,0);m.wait([&]{return member(*leaderGame,applicantId).requested==0 && member(*applicantGame,applicantId).requested==0;},"third request");
    while(applicant->takeTransfer()){}
    Settings cancelSettings;Frontend cancelUi(cancelSettings);cancelUi.originalInterface(m.assets);cancelUi.networkSession(applicantGame->sessionOptions());cancelUi.setPlaying(true);cancelUi.open(Screen::RemoteLobby,&applicantGame->state());cancelUi.sync(applicantGame->state());if(cancelUi.hasMessageBox())cancelUi.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
    auto cancelButton=std::find_if(cancelUi.view().items.begin(),cancelUi.view().items.end(),[](const auto& item){return item.control=="joinButtonPane";});require(cancelButton!=cancelUi.view().items.end(),"authored request Cancel control missing");
    auto cancellation=cancelUi.activate(size_t(cancelButton-cancelUi.view().items.begin()));require(cancellation.effect==FrontEffect::LobbyAction && cancellation.command==int(LobbyAction::CancelRequest),"authored pending JOIN does not cancel its request");
    uint64_t cancelAction{};emitLobbyNetworkActions(cancellation,true,applicantGame->state().networkReady,[&](const Net::Action& action){require(applicant->action(action),"authored cancellation record queued");if(action.op==Net::Operation::Lobby){cancelAction=applicant->lastActionNumber();cancelUi.cancelingRequest(cancelAction);}});
    for(int i=0;i<20;++i){m.server->poll();m.ca->poll();m.cb->poll();require(!applicant->takeTransfer() && cancelUi.pendingAction(cancelAction),"cancel receipt releases its modal before canonical request removal");std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    std::optional<uint64_t> canceled;m.wait([&]{if(!canceled)canceled=applicant->takeTransfer();return canceled && member(*applicantGame,applicantId).requested==noTeamRequest;},"canonical cancellation releases its original lockdown");require(*canceled==cancelAction,"cancellation completes a different transaction");cancelUi.sync(applicantGame->state());cancelUi.transferFinished(*canceled);require(!cancelUi.hasMessageBox(),"canonical cancel leaves original feedback pending");
    request(*applicant,0);m.wait([&]{return member(*leaderGame,applicantId).requested==0;},"request restored after authored cancellation");
    control(*leader,LobbyAction::Accept,0,applicantId);
    m.wait([&]{return member(*leaderGame,applicantId).team==0;},"commander accepts a valid pending request");
    request(*applicant,noTeam);m.wait([&]{return member(*leaderGame,applicantId).team==noTeam;},"accepted pilot can leave again");
    request(*applicant,0);m.wait([&]{return member(*leaderGame,applicantId).requested==0;},"pending request before enabling Auto Accept");
    control(*leader,LobbyAction::AutoAccept,1);
    m.wait([&]{return member(*leaderGame,applicantId).team==0 && member(*leaderGame,applicantId).requested==noTeamRequest;},"Auto Accept admits existing valid requests");
    rejected=applicant->metrics().rejected;control(*applicant,LobbyAction::ForceReady,1);
    m.wait([&]{return applicant->metrics().rejected>rejected;},"personal pilot cannot force team readiness");
    control(*leader,LobbyAction::Boot,0,applicantId);
    m.wait([&]{return member(*leaderGame,applicantId).team==noTeam;},"commander boots team member");
    rejected=applicant->metrics().rejected;request(*applicant,0);
    m.wait([&]{return applicant->metrics().rejected>rejected;},"boot ban prevents immediate readmission");
    require(applicant->takeRejection()==Allegiance::LobbyJoinFailureMessage(Allegiance::LobbyJoinFailure::Banned),"Boot-ban admission loses the original reason or emits generic failure feedback");
    request(*applicant,1);m.wait([&]{return member(*leaderGame,applicantId).team==1;},"booted pilot can join another faction");
    control(*leader,LobbyAction::LockLobby,1);
    m.wait([&]{return m.a->state().lobbyFlags&1;},"host locks public lobby");
    std::unique_ptr<Game> visitorGame;auto visitor=m.client("Visitor",visitorGame);
    m.wait([&]{visitor->poll();return visitor->closed();},"locked lobby rejects a fresh connection");
    require(visitor->error().find("locked")!=std::string::npos,"admission rejection explains locked lobby");
    control(*leader,LobbyAction::LockLobby,0);m.wait([&]{return !(m.a->state().lobbyFlags&1);},"host unlocks public lobby");
    auto rules=leaderGame->sessionOptions();rules.rules.allowJoiners=false;rules.rules.allowDefections=false;
    require(leader->configure(rules),"host submits original admission settings");
    m.wait([&]{return m.ca->ready() && m.cb->ready() && !m.a->sessionOptions().rules.allowJoiners && !m.b->sessionOptions().rules.allowDefections && m.a->state().networkPilots.size()==2 && m.b->state().networkPilots.size()==2;},"mission admission settings replicate");
    leader=m.a->state().networkHost?m.ca.get():m.cb.get();applicant=leader==m.ca.get()?m.cb.get():m.ca.get();
    leaderGame=leader==m.ca.get()?m.a.get():m.b.get();applicantGame=leader==m.ca.get()?m.b.get():m.a.get();
    leaderId=leaderGame->state().shipId;applicantId=applicantGame->state().shipId;
    ready.args[0]=0;require(leader->action(ready) && applicant->action(ready),"both pilots explicitly mark Away");
    m.wait([&]{return !m.a->state().networkReady && !m.b->state().networkReady;},"personal Away states replicate");
    control(*leader,LobbyAction::ForceReady,1);control(*applicant,LobbyAction::ForceReady,1);
    m.wait([&]{return (m.a->state().forceReady&3)==3 && (m.b->state().forceReady&3)==3;},"both commanders force team readiness independently of personal Away");
    require(!leaderGame->state().networkReady && !applicantGame->state().networkReady,"force-ready does not rewrite personal readiness");
    Net::Action start{};start.op=Net::Operation::Start;require(leader->action(start),"forced-ready match starts");
    m.wait([&]{return !m.a->state().networkLobby && !m.b->state().networkLobby;},"forced-ready countdown completes");
    require(m.a->state().networkPilots.size()==2 && m.b->state().networkPilots.size()==2,"shared public roster remains available during flight");
    control(*leader,LobbyAction::LockSides,1);m.wait([&]{return applicantGame->state().lobbyFlags&2;},"running host locks faction admission");
    while(applicant->takeTransfer()){}
    Settings departureSettings;Frontend departureUi(departureSettings);departureUi.originalInterface(m.assets);departureUi.networkSession(applicantGame->sessionOptions());departureUi.setPlaying(true);departureUi.open(Screen::RemoteLobby,&applicantGame->state());
    auto departureClick=[&](std::string_view name){auto& items=departureUi.view().items;auto i=std::find_if(items.begin(),items.end(),[&](const auto& item){return item.control==name;});require(i!=items.end(),"authored departure control missing");return departureUi.activate(size_t(i-items.begin()));};
    int originalSide=member(*applicantGame,applicantId).team;
    departureClick("noTeam");auto departure=departureClick("joinButtonPane");require(departure.effect==FrontEffect::NetworkJoinTeam && departure.value==noTeam,"locked/no-defections original JOIN cannot leave for No Team");
    uint64_t departureAction{};emitLobbyNetworkActions(departure,true,applicantGame->state().networkReady,[&](const Net::Action& action){require(applicant->action(action),"authored departure record queued");if(action.op==Net::Operation::Team){departureAction=applicant->lastActionNumber();departureUi.quittingTeam(departureAction);}});
    // Process control receipts without advancing the publication clock.
    for(int i=0;i<20;++i){m.server->poll();m.ca->poll();m.cb->poll();require(!applicant->takeTransfer() && departureUi.pendingAction(departureAction),"team receipt releases a lockdown before its authoritative report");std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    std::optional<uint64_t> leftTeam;
    m.wait([&]{if(!leftTeam)leftTeam=applicant->takeTransfer();return leftTeam && member(*leaderGame,applicantId).team==noTeam;},"locked/no-defections mission permits voluntary No Team departure");
    require(*leftTeam==departureAction && applicantGame->state().playerSide==noTeam && member(*applicantGame,applicantId).ready,"canonical departure loses action identity, faction state or original AFK wake-up");
    departureUi.sync(applicantGame->state());departureUi.transferFinished(*leftTeam);require(!departureUi.hasMessageBox(),"canonical encrypted membership leaves original quitting dialog open");
    rejected=applicant->metrics().rejected;request(*applicant,originalSide);
    m.wait([&]{return applicant->metrics().rejected>rejected;},"side lock still rejects faction readmission");
    require(applicant->takeRejection()==Allegiance::LobbyJoinFailureMessage(Allegiance::LobbyJoinFailure::SidesLocked),"locked admission loses its original reason");
    control(*leader,LobbyAction::LockSides,0);m.wait([&]{return !(applicantGame->state().lobbyFlags&2);},"host unlocks faction admission");
    request(*applicant,originalSide);m.wait([&]{return member(*applicantGame,applicantId).team==originalSide && applicantGame->state().docked;},"no-defections pilot can rejoin its remembered faction");
    require(applicantGame->state().shipId==applicantId && applicantGame->playerShip()->GetHullType(),"original-faction readmission replaces pilot identity or fails to equip its hangar ship");
    std::unique_ptr<Game> lateGame;auto late=m.client("Late joiner",lateGame);
    m.wait([&]{late->poll();return late->closed();},"no-joiners mission rejects a fresh late connection");
    require(late->error().find("late joiners")!=std::string::npos,"late admission reason survives encrypted transport");
    int resumeId=applicantGame->state().shipId;applicant->reconnect();
    m.wait([&]{return applicant->ready();},"authenticated reconnect remains allowed without late joiners");
    require(applicantGame->state().shipId==resumeId,"secure resume retains roster identity and team policy");
    Net::ArenaEncoder encoder;Net::Frame roster;roster.header.kind=Net::Kind::Snapshot;
    Net::Member publicPilot;publicPilot.id=leaderId;publicPilot.side=0;Net::text(publicPilot.name,"Pilot");roster.members.push_back(publicPilot);
    for(int recipient=0;recipient<1024;++recipient)encoder.publishWorld(0x100000,1,1,roster);
    require(encoder.encodes()==1,"1024 recipients reuse one public roster encoding");
    auto roundtrip=roster;require(Net::frame(Net::packet(roundtrip)).members==roster.members,"public membership records roundtrip independently of private pilot records");
    {
        Match fresh(art,27660,false,true);fresh.join();
        auto* commander=fresh.a->state().commander?fresh.ca.get():fresh.cb.get();
        auto* commanderGame=commander==fresh.ca.get()?fresh.a.get():fresh.b.get();
        control(*commander,LobbyAction::AutoAccept,0);
        fresh.wait([&]{return !(fresh.a->state().autoAccept&1) && !(fresh.b->state().autoAccept&1);},"fresh admission commander disables Auto Accept");
        std::unique_ptr<Game> waitingGame;auto waiting=fresh.client("Fresh applicant",waitingGame);
        fresh.wait([&]{waiting->poll();if(waiting->closed())throw std::runtime_error("Fresh No Team bootstrap: "+waiting->error());return waiting->ready();},"fresh applicant authenticates without owning a team ship");
        int waitingId=waitingGame->state().shipId;
        fresh.wait([&]{waiting->poll();return commanderGame->state().networkPilots.size()==3 && member(*commanderGame,waitingId).requested==0;},"fresh applicant enters the public pending roster");
        require(member(*waitingGame,waitingId).team==noTeam && waitingGame->state().playerSide==noTeam && waitingGame->state().wing==NA && waitingGame->state().networkLobby && waitingGame->scene(false).empty() && waitingGame->contacts().empty(),"fresh No Team bootstrap has no private faction world or stale faction identity/wing");
        control(*commander,LobbyAction::Accept,0,waitingId);
        fresh.wait([&]{waiting->poll();return member(*waitingGame,waitingId).team==0 && !waitingGame->state().ghost && waitingGame->state().docked;},"fresh accepted applicant receives a real hangar and loadout");
        require(waitingGame->state().shipId==waitingId && waitingGame->playerShip()->GetHullType(),"acceptance preserves the fresh pilot identity and equips its original hull");
    }
    {
        Match limited(art,27661,false,false,false,Scenario::Conquest,1,1);limited.join();
        std::unique_ptr<Game> thirdGame,fourthGame;
        auto third=limited.client("Third",thirdGame);
        limited.wait([&]{third->poll();return third->ready() && thirdGame->state().networkPilots.size()==3;},"original imbalance permits its two-extra newcomer allowance");
        require(member(*thirdGame,thirdGame->state().shipId).team==0,"third unranked pilot admitted at original count boundary");
        auto fourth=limited.client("Fourth",fourthGame);
        limited.wait([&]{third->poll();fourth->poll();return fourth->ready() && fourthGame->state().networkPilots.size()==4;},"over-imbalance newcomer remains connected in No Team");
        int fourthId=fourthGame->state().shipId;
        require(member(*fourthGame,fourthId).team==noTeam && member(*fourthGame,fourthId).requested==noTeamRequest,"rejected count boundary does not create a pending unauthorized request");
        request(*limited.ca,1);
        limited.wait([&]{third->poll();fourth->poll();return member(*fourthGame,limited.a->state().shipId).team==1;},"changing sides repairs the active-team count imbalance");
        request(*fourth,0);
        limited.wait([&]{third->poll();fourth->poll();return member(*fourthGame,fourthId).team==0 && fourthGame->state().docked;},"previously blocked newcomer joins once team counts permit it");
    }
    {
        Match locked(art,27662,false,true);locked.join();
        auto* host=locked.a->state().networkHost?locked.ca.get():locked.cb.get();
        auto rules=(host==locked.ca.get()?locked.a:locked.b)->sessionOptions();rules.rules.lockTeamSettings=true;
        require(host->configure(rules),"host locks team settings");
        locked.wait([&]{return locked.ca->ready() && locked.cb->ready() && locked.a->sessionOptions().rules.lockTeamSettings && locked.b->sessionOptions().rules.lockTeamSettings && locked.a->state().networkPilots.size()==2;},"locked team settings replicate after new setup");
        auto* commander=locked.a->state().commander?locked.ca.get():locked.cb.get();auto* pilot=commander==locked.ca.get()?locked.cb.get():locked.ca.get();
        auto* commanderGame=commander==locked.ca.get()?locked.a.get():locked.b.get();auto* pilotGame=commander==locked.ca.get()?locked.b.get():locked.a.get();int pilotId=pilotGame->state().shipId;
        require(commanderGame->playerPaneData().teamSettingsLocked && commanderGame->playerPaneData().acceptJoiners,"original flight player pane receives replicated team-settings and join availability gates");
        auto rejected=commander->metrics().rejected;control(*commander,LobbyAction::Boot,0,pilotId);
        locked.wait([&]{return commander->metrics().rejected>rejected;},"authority rejects commander Boot under locked team settings");
        require(member(*commanderGame,pilotId).team==0,"rejected locked Boot preserves teammate membership");
        rejected=commander->metrics().rejected;control(*commander,LobbyAction::FlightBoot,0,pilotId);
        locked.wait([&]{return commander->metrics().rejected>rejected;},"F6 confirmation cannot bypass locked team settings");
        rejected=commander->metrics().rejected;control(*commander,LobbyAction::Reject,0,pilotId);
        locked.wait([&]{return commander->metrics().rejected>rejected;},"physical Reject cannot bypass locked teammate Boot");
        request(*pilot,noTeam);locked.wait([&]{return member(*commanderGame,pilotId).team==noTeam;},"locked settings retain voluntary departure");
        control(*commander,LobbyAction::AutoAccept,0);locked.wait([&]{return !(commanderGame->state().autoAccept&1);},"locked applicant fixture disables Auto Accept");
        request(*pilot,0);locked.wait([&]{return member(*commanderGame,pilotId).requested==0;},"locked team accepts join requests");
        control(*commander,LobbyAction::Reject,0,pilotId);locked.wait([&]{return member(*commanderGame,pilotId).requested==noTeamRequest;},"locked settings allow rejecting applicants");
        request(*pilot,0);locked.wait([&]{return member(*commanderGame,pilotId).requested==0;},"rejected applicant is not banned");
        require(commanderGame->bootPlayer(pilotId,0),"F6 applicant confirmation is allowed under locked settings");
        locked.wait([&]{return member(*commanderGame,pilotId).requested==noTeamRequest;},"F6 BootSink resolves an outstanding request as Reject rather than expulsion");
        require(!pilot->closed(),"rejecting an applicant preserves the mission connection");
        request(*pilot,0);locked.wait([&]{return member(*commanderGame,pilotId).requested==0;},"F6 rejection does not ban the applicant");
        control(*commander,LobbyAction::Accept,0,pilotId);locked.wait([&]{return member(*commanderGame,pilotId).team==0;},"locked settings allow accepting valid applicants");
    }
    {
        Match history(art,27663,false,false,false,Scenario::Conquest,1,32767,true,0,false);history.join();
        auto* host=history.a->state().networkHost?history.ca.get():history.cb.get();
        auto* pilot=host==history.ca.get()?history.cb.get():history.ca.get();auto* pilotGame=pilot==history.ca.get()?history.a.get():history.b.get();
        int id=pilotGame->state().shipId,side=pilotGame->state().playerSide;
        std::unique_ptr<Game> invalidGame;auto invalid=history.client("Unclaimed No Team",invalidGame,noTeam);history.wait([&]{invalid->poll();return invalid->closed();},"fresh unauthenticated No Team hello remains invalid");require(invalid->error()=="Invalid pilot or team" && !invalidGame,"No Team resume exception permits an unauthenticated initial membership");
        auto denied=[&](int target,const char* why){auto before=pilot->metrics().rejected;request(*pilot,target);history.wait([&]{return pilot->metrics().rejected>before;},why);require(member(*pilotGame,id).team==noTeam,"rejected defection changes faction membership");};
        auto before=pilot->metrics().rejected;request(*pilot,1-side);history.wait([&]{return pilot->metrics().rejected>before;},"running no-defections pilot cannot switch directly between factions");require(member(*pilotGame,id).team==side,"rejected direct defection removes the current faction");
        request(*pilot,noTeam);history.wait([&]{return member(*pilotGame,id).team==noTeam;},"running no-defections pilot leaves without a Boot ban");
        denied(1-side,"remembered faction prevents a voluntary defection");
        pilot->reconnect();history.wait([&]{return pilot->ready();},"No Team reconnect preserves previous faction history");
        denied(1-side,"reconnecting in No Team cannot erase the no-defections policy");
        request(*pilot,side);history.wait([&]{return member(*pilotGame,id).team==side && pilotGame->state().docked;},"No Team reconnect rejoins the remembered faction with its original identity");
        require(pilotGame->state().shipId==id,"No Team reconnect allocates a different pilot");
        std::unique_ptr<Game> newcomerGame;auto newcomer=history.client("First faction",newcomerGame,side);
        history.wait([&]{newcomer->poll();return newcomer->ready() && newcomerGame->state().playerSide==side;},"fresh pilot with no faction history can join a running no-defections match");
        control(*host,LobbyAction::LockSides,1);history.wait([&]{return pilotGame->state().lobbyFlags&2;},"history fixture locks admission");
        request(*pilot,noTeam);history.wait([&]{newcomer->poll();return member(*pilotGame,id).team==noTeam;},"side lock does not prevent self-departure");
        denied(side,"side lock rejects the remembered faction until admission is unlocked");
        auto* mission=history.authority->core();auto params=*mission->GetMissionParams();params.bAllowRestart=false;mission->SetMissionParams(&params);
        while(auto* base=mission->GetSide(SideID(side))->GetStations()->first())mission->GetIgcSite()->KillStationEvent(base->data(),nullptr,1000);
        history.wait([&]{newcomer->poll();return newcomerGame->state().complete && newcomerGame->state().endgame.frozen && mission->GetMissionStage()==STAGE_OVER;},"nonrestartable mission freezes its original results");
        auto finalResults=newcomerGame->state().endgame;int newcomerId=newcomerGame->state().shipId;
        request(*newcomer,noTeam);history.wait([&]{newcomer->poll();return member(*newcomerGame,newcomerId).team==noTeam;},"ended mission still permits voluntary departure from a locked/eliminated side");
        require(newcomerGame->state().complete && newcomerGame->state().endgame==finalResults && newcomerGame->state().shipId==newcomerId,"postgame departure mutates frozen scores or replaces the pilot");
        Net::Action personal{};personal.op=Net::Operation::Ready;personal.args[0]=1;require(newcomer->action(personal),"postgame personal ready update queued");
        history.wait([&]{newcomer->poll();return member(*newcomerGame,newcomerId).ready;},"postgame personal ready state follows FedSrv policy");
    }
    {
        Match open(art,27664,false,true,false,Scenario::Conquest,1,32767,true,0,true,2,{},[](MissionParams& p){p.bLockGameOpen=true;});open.join();
        auto* host=open.a->state().networkHost?open.ca.get():open.cb.get();
        auto* hostGame=host==open.ca.get()?open.a.get():open.b.get();
        require((open.a->state().lobbyFlags&Allegiance::LobbyGameOpenLocked) && (open.b->state().lobbyFlags&Allegiance::LobbyGameOpenLocked),"authority game-open policy is absent from original lobby reports");
        for(auto kind:{LobbyAction::LockLobby,LobbyAction::AutoAccept}){auto before=host->metrics().rejected;control(*host,kind,0);open.wait([&]{return host->metrics().rejected>before;},"locked-open authority rejects lobby/Auto Accept changes");}
        control(*host,LobbyAction::LockSides,1);open.wait([&]{return hostGame->state().lobbyFlags&2;},"locked-open policy preserves independent side locking");
        auto configured=hostGame->sessionOptions();++configured.seed;auto match=host->match();require(host->configure(configured),"locked-open settings reconfiguration queued");
        open.wait([&]{return open.ca->ready() && open.cb->ready() && host->match()>match;},"locked-open policy survives authority reconfiguration");
        require(open.authority->core()->GetMissionParams()->bLockGameOpen && (open.a->state().lobbyFlags&Allegiance::LobbyGameOpenLocked) && (open.b->state().lobbyFlags&Allegiance::LobbyGameOpenLocked),"reconfiguration silently removes original locked-open controls");
    }
    {
        Match balance(art,27665,false,false,false,Scenario::Conquest,1,32766,true,0,true,2,{}, {},1);balance.join();
        std::unique_ptr<Game> thirdGame,fourthGame;auto third=balance.client("Rank-one newcomer",thirdGame,0);
        balance.wait([&]{third->poll();return third->ready() && thirdGame->state().playerSide==0;},"Enforce Balance accepts a lowest-rank faction tie");
        auto fourth=balance.client("Waiting newcomer",fourthGame,0);
        auto wait=[&](auto predicate,const char* what){balance.wait([&]{third->poll();fourth->poll();return predicate();},what);};
        wait([&]{return fourth->ready();},"Enforce Balance excess newcomer authenticates into No Team");
        require(fourthGame->state().playerSide==noTeam,"Enforce Balance directly admits a pilot to the larger faction");
        auto before=fourth->metrics().rejected;request(*fourth,0);wait([&]{return fourth->metrics().rejected>before;},"Enforce Balance rejects the larger faction");
        require(fourth->takeRejection()==Allegiance::LobbyJoinFailureMessage(Allegiance::LobbyJoinFailure::TeamBalance),"Enforce Balance loses its original rejection caption");
        auto* exhausted=balance.authority->core()->GetShip(ShipID(thirdGame->state().shipId));exhausted->SetDeaths(2);require(exhausted->IsGhost(),"admission ghost fixture did not exhaust its one-life policy");
        request(*fourth,0);wait([&]{return fourthGame->state().playerSide==0;},"ghost pilots do not inflate original admission count/rank sums");
        auto* host=balance.a->state().networkHost?balance.ca.get():balance.cb.get();auto* hostGame=host==balance.ca.get()?balance.a.get():balance.b.get();
        control(*host,LobbyAction::ForceReady,1);wait([&]{return hostGame->state().forceReady&(1u<<hostGame->state().playerSide);},"running team force-ready policy remains accepted by original authority");
    }
    {
        Match starting(art,27668,false,true,false,Scenario::Conquest,1,32766,true);starting.join();
        auto* host=starting.a->state().networkHost?starting.ca.get():starting.cb.get();auto* hostGame=host==starting.ca.get()?starting.a.get():starting.b.get();
        auto configured=hostGame->sessionOptions();configured.rules.startCountdown=30;auto match=host->match();require(host->configure(configured),"balance countdown fixture configured");starting.wait([&]{return starting.ca->ready() && starting.cb->ready() && host->match()>match;},"balance countdown setup replicated");
        hostGame=host==starting.ca.get()?starting.a.get():starting.b.get();
        std::unique_ptr<Game> thirdGame,fourthGame;auto third=starting.client("Pregame newcomer",thirdGame,0);
        starting.wait([&]{third->poll();return third->ready() && thirdGame->state().playerSide==0;},"NotStarted admits pregame faction selection without rank balancing");
        for(auto* client:{starting.ca.get(),starting.cb.get(),third.get()}){Net::Action ready{};ready.op=Net::Operation::Ready;ready.args[0]=1;require(client->action(ready),"countdown pilots ready");}
        starting.wait([&]{third->poll();return hostGame->state().readySides==3;},"rank-one 2/1 rosters satisfy original start threshold");
        Net::Action start{};start.op=Net::Operation::Start;require(host->action(start),"rank-one countdown starts");starting.wait([&]{third->poll();return starting.authority->core()->GetMissionStage()==STAGE_STARTING;},"original countdown stage entered");
        auto fourth=starting.client("Countdown applicant",fourthGame,0);
        auto wait=[&](auto predicate,const char* what){starting.wait([&]{third->poll();fourth->poll();return predicate();},what);};
        wait([&]{return fourth->ready();},"countdown applicant authenticates");require(fourthGame->state().playerSide==noTeam && starting.authority->core()->GetMissionStage()==STAGE_STARTING,"countdown admission skips rank balance and admits to the larger faction");
        auto before=fourth->metrics().rejected;request(*fourth,0);wait([&]{return fourth->metrics().rejected>before;},"countdown rejects the larger faction");require(fourth->takeRejection()==Allegiance::LobbyJoinFailureMessage(Allegiance::LobbyJoinFailure::TeamBalance),"countdown balance loses original rejection reason");
        request(*fourth,1);wait([&]{return fourthGame->state().playerSide==1;},"countdown admits the lowest-rank faction");require(starting.authority->core()->GetMissionStage()==STAGE_NOTSTARTED,"accepted countdown membership fails to withdraw the old countdown");
    }
    puts("Original lobby admission: requests, cancel/accept/reject, Auto Accept, voluntary No Team under locked sides/no defections, causal quitting dialog, remembered-faction reentry/reconnect, fresh admission, Boot bans/team-settings lock, Away/force-ready, late joining, secure resume and shared public roster pass");
}
void flightBoot(const std::filesystem::path& art){
    for(int mode=0;mode<3;++mode){
        bool defections=mode==0,lobbyBoot=mode==2;
        Match m(art,27666,false,false,false,Scenario::Conquest,1,32767,false,0,defections);m.join();
        auto* commander=m.a->state().commander?m.ca.get():m.cb.get();auto* target=commander==m.ca.get()?m.cb.get():m.ca.get();
        auto* commanderGame=commander==m.ca.get()?m.a.get():m.b.get();auto* targetGame=commander==m.ca.get()?m.b.get():m.a.get();
        int commanderId=commanderGame->state().shipId,targetId=targetGame->state().shipId;
        std::unique_ptr<Game> thirdGame;std::unique_ptr<Net::Client> third;
        auto wait=[&](auto predicate,const char* what){
            for(int i=0;i<1200 && !predicate();++i){
                m.ca->step(m.ia,1./Net::tickRate);m.cb->step(m.ib,1./Net::tickRate);
                if(third)third->step({},1./Net::tickRate);
                m.server->poll();m.server->step();m.ca->poll();m.cb->poll();if(third)third->poll();
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            require(predicate(),what);
        };
        auto action=[](Net::Client& client,LobbyAction kind,int side,int id){Net::Action a{};a.op=Net::Operation::Lobby;a.args={int(kind),side,id,0};require(client.action(a),"Boot action queued");};
        auto member=[](Game& game,int id){auto row=std::find_if(game.state().networkPilots.begin(),game.state().networkPilots.end(),[&](auto& p){return p.id==id;});require(row!=game.state().networkPilots.end(),"Boot subject public identity exists");return *row;};
        auto rejected=target->metrics().rejected;action(*target,LobbyAction::FlightBoot,0,commanderId);
        wait([&]{return target->metrics().rejected>rejected;},"authority rejects noncommander FlightBoot");
        rejected=commander->metrics().rejected;action(*commander,LobbyAction::FlightBoot,1,targetId);
        wait([&]{return commander->metrics().rejected>rejected;},"authority rejects a confirmation from another team");
        rejected=commander->metrics().rejected;action(*commander,LobbyAction::FlightBoot,0,commanderId);
        wait([&]{return commander->metrics().rejected>rejected;},"commander cannot boot themselves through F6");
        int crewHull=-1;
        if(!defections && !lobbyBoot){
            auto* parent=m.authority->core()->GetShip(ShipID(targetId));IhullTypeIGC* hull=nullptr;
            for(auto* link=m.authority->core()->GetHullTypes()->first();link;link=link->next())if(link->data()->GetMaxWeapons()>link->data()->GetMaxFixedWeapons()){hull=link->data();break;}
            require(hull,"Boot retirement turret hull fixture");crewHull=hull->GetObjectID();
            while(auto* part=parent->GetParts()->first())part->data()->Terminate();parent->SetBaseHullType(hull);
            parent->SetAmmo(hull->GetMaxAmmo());parent->SetEnergy(hull->GetMaxEnergy());parent->SetFuel(hull->GetMaxFuel());
            for(Mount mount=hull->GetMaxFixedWeapons();mount<hull->GetMaxWeapons();++mount)
                for(auto* part=m.authority->core()->GetPartTypes()->first();part;part=part->next())if(part->data()->GetEquipmentType()==ET_Weapon && hull->CanMount(part->data(),mount)){
                    auto* weapon=parent->CreateAndAddPart(part->data(),mount,1);weapon->SetMountedFraction(1);weapon->Arm();break;
                }
            wait([&]{return targetGame->currentLoadout().hull==crewHull;},"Boot subject's crew hull replicates");
            require(commanderGame->boardShip(targetId),"commander boards the pilot who will be removed");
            wait([&]{return commanderGame->state().passenger && commanderGame->state().parentId==targetId;},"real crew membership is established before Boot");
        }
        if(lobbyBoot)action(*commander,LobbyAction::Boot,0,targetId);
        else require(commanderGame->bootPlayer(targetId,0),"replica confirms F6 through the existing reliable action");
        if(defections || lobbyBoot){
            wait([&]{return member(*commanderGame,targetId).team==noTeam && targetGame->state().docked==false;},"lobby Boot and defections-enabled F6 retain the connected pilot on No Team");
            wait([&]{return std::any_of(targetGame->state().messages.begin(),targetGame->state().messages.end(),[](auto& msg){return msg.modal && msg.text=="You have been booted by your team leader!";});},"side removal delivers the original personal reason through the existing notice stream");
            require(!target->closed() && m.server->clients()==2 && m.authority->core()->GetShip(targetId) && m.authority->core()->GetShip(targetId)->GetSide()->GetObjectID()==SIDE_TEAMLOBBY,"side removal preserves pilot identity and connection");
            require(std::any_of(commanderGame->state().messages.begin(),commanderGame->state().messages.end(),[](auto& msg){return msg.chat && msg.text.find(" was booted from the team by the team leader.")!=std::string::npos && msg.chatTarget==CHAT_TEAM;}),"former teammates receive the original team chat notification");
            Net::Action request{};request.op=Net::Operation::Team;request.args[0]=0;
            rejected=target->metrics().rejected;require(target->action(request),"banned-side request queued");
            wait([&]{return target->metrics().rejected>rejected;},"booted pilot cannot return to the banning faction");
            if(defections){
                third=m.client("Second commander",thirdGame,1);
                wait([&]{return third->ready() && thirdGame->state().commander;},"other faction's real commander joins");
                request.args[0]=1;require(target->action(request),"booted pilot requests another faction");
                wait([&]{return member(*thirdGame,targetId).team==1 && !targetGame->state().networkLobby && targetGame->state().docked;},"pilot joins the unbanned faction");
                require(thirdGame->bootPlayer(targetId,1),"second faction confirms F6 Boot");
                wait([&]{return target->closed() && m.server->clients()==2 && !m.authority->core()->GetShip(targetId);},"being banned from all factions removes the pilot from the mission");
                require(!third->closed() && !commander->closed(),"all-faction removal does not disrupt either commander");
            }
        }else {
            wait([&]{return target->closed() && m.server->clients()==1 && !m.authority->core()->GetShip(targetId);},"no-defections F6 closes the connection and retires the actual pilot");
            require(!commander->closed() && target->error()=="You have been booted by your commander!","removed client receives the original commander-removal reason and the match continues");
            wait([&]{auto* survivor=m.authority->core()->GetShip(ShipID(commanderId));return survivor && !survivor->GetParentShip() && survivor->GetBaseHullType() && survivor->GetBaseHullType()->GetObjectID()==crewHull && !commanderGame->state().passenger && commanderGame->currentLoadout().hull==crewHull;},"Boot promotes the remaining gunner and preserves the crew's real hull, connection and identity");
            target->reconnect();for(int i=0;i<10;++i){m.server->poll();target->poll();}
            require(target->closed() && m.server->clients()==1 && !m.authority->core()->GetShip(targetId),"expelled session cannot reconnect or retain a dormant pilot");
        }
    }
    puts("Encrypted original Boot: F6 defection-dependent mission/side removal, separate lobby behavior, sender/side/self gates, bans, all-faction expulsion and nonresumable retirement passed");
}
void coreGames(const std::filesystem::path& art){
    Match m(art,27760,false,true);m.ca.reset();m.cb.reset();auto catalog=m.assets.coreCatalog();require(!catalog.empty(),"core hosting catalog empty");
    Net::ServerHost host(*m.server,m.digest,m.port,[&](const SessionOptions& options){return std::make_unique<Game>(art,m.loader,0,0,options,EffectLoader{},GameRole::Authority);},catalog);
    Net::ServerBrowser browser(m.digest);auto address="127.0.0.1:"+std::to_string(m.port);browser.add(address);
    std::vector<std::unique_ptr<Game>> games;std::vector<std::unique_ptr<Net::Client>> clients;std::vector<unsigned> factories;
    auto wait=[&](auto condition,const char* what){auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        do{m.server->poll();host.poll();m.server->step();host.step();for(auto& c:clients){c->poll();if(c->closed())throw std::runtime_error(c->error());}browser.poll();if(condition())return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}while(std::chrono::steady_clock::now()<deadline);
        throw std::runtime_error(std::string("Core hosting timeout: ")+what);};
    wait([&]{return !browser.authorities().empty();},"advertised core catalog");require(browser.authorities()[0].cores==catalog,"directory substitutes a scenario default for cores.txt");
    Settings settings;Frontend ui(settings);ui.originalInterface(m.assets);ui.open(Screen::Games);ui.gameAuthorities(browser.authorities());
    auto click=[&](std::string_view control){auto i=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& c){return c.control==control;});require(i!=ui.view().items.end(),"core creation control absent");return ui.activate(size_t(i-ui.view().items.begin()));};
    for(size_t index=0;index<catalog.size();++index){const auto& core=catalog[index];click("newGameButtonPane");click("create/server/1");
        std::vector<int> rows;for(const auto& item:ui.view().items)if(item.action.op==MenuOp::GameCreateCore)rows.push_back(item.action.value);bool selected=false;
        for(int row:rows){click("create/core/"+std::to_string(row));if(Allegiance::CoreFileEqual(ui.createCore(),core)){selected=true;break;}}
        require(selected,"catalogued core inaccessible in original dialog");click("create/server/1");require(click("createOkButtonPane").effect==FrontEffect::CreateGame,"supported core cannot create on selected server");
        auto pilot="Core pilot "+std::to_string(index);auto name="Core mission "+std::to_string(index);std::string requested=core;if(index==0)std::transform(requested.begin(),requested.end(),requested.begin(),[](unsigned char c){return std::toupper(c);});
        require(browser.createGame(address,name,pilot,requested),"catalog core creation did not start");std::optional<Net::GameCreationResult> created;
        wait([&]{if(!created)created=browser.takeCreation();return created.has_value();},"catalog core creation");require(created->error.empty(),"catalog core refused by actual IGC authority");
        games.emplace_back();factories.push_back(0);
        clients.push_back(std::make_unique<Net::Client>(created->address,m.digest,pilot,0,[&,index](const SessionOptions& options)->Game&{
            require(options.coreFile==catalog[index],"Welcome core differs from created authority");++factories[index];games[index]=std::make_unique<Game>(art,m.loader,0,0,options,EffectLoader{},GameRole::Replica);return *games[index];},created->token));
        wait([&]{return clients[index]->ready();},"alternate core join");
        auto& client=*clients[index];auto* game=games[index].get();require(game->state().networkHost && game->sessionOptions().coreFile==core && std::string_view(game->core()->GetMissionParams()->szIGCStaticFile)==core,"replica/IGC/default ownership drops selected core");
        ui.networkSession(game->sessionOptions());ui.setPlaying(true);ui.open(Screen::RemoteLobby,&game->state());
        require(std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.name=="textCorename" && c.text==m.assets.coreName(core);}),"network lobby labels another core");
        auto changed=game->sessionOptions();changed.rules.friendlyFire=!changed.rules.friendlyFire;auto match=client.match();require(client.configure(changed),"alternate core configuration not queued");
        wait([&]{return client.ready() && client.match()>match;},"same-core configuration");game=games[index].get();require(game->sessionOptions().coreFile==core && game->sessionOptions().rules.friendlyFire==changed.rules.friendlyFire && factories[index]==2,"mission reset loses chosen core");
        changed=game->sessionOptions();changed.coreFile=catalog[(index+1)%catalog.size()];if(catalog.size()==1)changed.coreFile="static_core";
        auto rejected=client.metrics().rejected;match=client.match();require(client.configure(changed),"cross-core configuration not queued");
        wait([&]{return client.metrics().rejected>rejected;},"cross-core settings refusal");require(client.match()==match && factories[index]==2 && games[index]->sessionOptions().coreFile==core,"settings change replaces the active core");
        ui.setPlaying(false);ui.open(Screen::Games);ui.gameAuthorities(browser.authorities());
    }
    if(!Allegiance::FindCore(catalog,"static_core")){require(browser.createGame(address,"Unlisted core","Not listed","static_core"),"unlisted core query not queued");std::optional<Net::GameCreationResult> denied;wait([&]{if(!denied)denied=browser.takeCreation();return denied.has_value();},"unlisted core refusal");require(!denied->error.empty(),"installed but unadvertised core bypasses server policy");}
    browser.findPlayer("Core pilot "+std::to_string(catalog.size()-1));std::optional<Net::PlayerSearchResult> found;wait([&]{if(!found)found=browser.takePlayerSearch();return found.has_value();},"multi-core directory refresh");
    require(found->game && browser.listings().size()==catalog.size()+1,"multi-core directory omits actual missions");
    for(const auto& listing:browser.listings())require(Allegiance::CoreFileEqual(listing.info.core,Net::options(listing.info.session,0,"").coreFile),"directory label differs from session core");
    for(auto& client:clients)client->reconnect();wait([&]{return std::all_of(clients.begin(),clients.end(),[](const auto& c){return c->ready();});},"alternate-core reconnect");
    for(size_t i=0;i<games.size();++i){if(games[i]->sessionOptions().coreFile!=catalog[i] || !games[i]->state().networkHost || factories[i]!=2)fprintf(stderr,"Core resume %s: loaded=%s owner=%d factories=%u\n",catalog[i].c_str(),games[i]->sessionOptions().coreFile.c_str(),games[i]->state().networkHost,factories[i]);require(games[i]->sessionOptions().coreFile==catalog[i] && games[i]->state().networkHost && factories[i]==2,"resume drops core identity/ownership or recreates mission");}
    printf("Encrypted core catalog: %zu cores.txt missions, original selection/labels, case-insensitive creation, actual authority/replica cores, same-core reset, cross-core refusal, directory/player search and reconnect passed\n",catalog.size());
}
void createGames(const std::filesystem::path& art){
    auto defaultPorts=Net::gameListenerPorts(27500);require(defaultPorts[0]==27500 && defaultPorts[1]==27502 && defaultPorts.back()==27516 &&
        std::none_of(defaultPorts.begin()+1,defaultPorts.end(),[](auto p){return p==Net::LanDiscovery::discoveryPort;}),"hosting consumes the discovery socket or loses a mission slot");
    for(uint16_t port:{uint16_t(1),uint16_t(27486),uint16_t(27500),uint16_t(27501),uint16_t(65530),uint16_t(65535)}){
        auto ports=Net::gameListenerPorts(port);require(ports.front()==port,"hosting changes its entry port");uint16_t previous=port;bool end=false;
        for(size_t i=1;i<ports.size();++i){if(!ports[i]){end=true;continue;}require(!end && ports[i]>previous && ports[i]!=Net::LanDiscovery::discoveryPort,"listener plan wraps, repeats or consumes discovery port");previous=ports[i];}
    }
    Match m(art,27743,false,true);m.ca.reset();m.cb.reset();
    Net::ServerHost host(*m.server,m.digest,m.port,[&](const SessionOptions& options){
        auto game=std::make_unique<Game>(art,m.loader,0,0,options,EffectLoader{},GameRole::Authority);game->quickChats(m.quickChat);return game;
    });
    const auto address="127.0.0.1:"+std::to_string(m.port);
    Net::ServerBrowser browser(m.digest);browser.add(address);
    std::vector<std::unique_ptr<Game>> replicas;std::vector<std::unique_ptr<Net::Client>> clients;
    auto wait=[&](auto condition,const char* what){
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        do{m.server->poll();host.poll();for(auto& c:clients)c->poll();m.server->step();host.step();browser.poll();
            if(condition())return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}while(std::chrono::steady_clock::now()<deadline);
        throw std::runtime_error(std::string("Create game timeout: ")+what);
    };
    wait([&]{return !browser.authorities().empty();},"authority directory");
    require(browser.listings().size()==1 && browser.authorities()[0].info.count==1 && browser.authorities()[0].info.capacity==Net::maxHostedGames &&
        m.server->clients()==0,"directory creates a pilot or loses authority capacity");
    Settings settings;settings.pilot="Ranked pilot(42)";Frontend ui(settings);ui.originalInterface(m.assets);ui.open(Screen::Games);ui.gameAuthorities(browser.authorities());
    auto item=[&](std::string_view name){auto i=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& item){return item.control==name;});require(i!=ui.view().items.end(),"Create Game authored control missing");return size_t(i-ui.view().items.begin());};
    auto click=[&](std::string_view name){return ui.activate(item(name));};
    click("newGameButtonPane");require(ui.createName()=="Ranked pilot's game","original default game name retains appended rank");click("create/server/1");click("create/core/0");
    require(ui.createAddress()==address && ui.view().items[item("createOkButtonPane")].enabled,"remote server selection does not enable original OK");
    auto full=browser.authorities();full[0].info.count=full[0].info.capacity;ui.gameAuthorities(full);
    require(!ui.view().items[item("createOkButtonPane")].enabled && click("createOkButtonPane").effect==FrontEffect::None,"full server accepts creation");
    auto okFace=std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.name=="createOkButtonPane";});
    require(okFace!=ui.view().artwork.end() && okFace->visibleBounds().z==0,"original OK is drawn for a full server");
    auto incompatible=browser.authorities();incompatible[0].cores={"other_core"};ui.gameAuthorities(incompatible);click("create/server/1");
    require(!ui.view().items[item("createOkButtonPane")].enabled,"unsupported server retains compatible core selection");
    click("create/core/0");require(!ui.view().items[item("createOkButtonPane")].enabled,"core selection keeps an incompatible server");
    ui.gameAuthorities(browser.authorities());click("create/server/1");ui.gameAuthorities({});
    require(!ui.view().items[item("createOkButtonPane")].enabled,"expired remote selection silently becomes local creation");
    ui.gameAuthorities(browser.authorities());click("create/server/1");
    require(click("createOkButtonPane").effect==FrontEffect::CreateGame && ui.screen()==Screen::Games,"remote creation opens offline lobby");
    ui.open(Screen::Games);click("newGameButtonPane");click("create/server/0");click("create/core/0");require(click("createOkButtonPane").effect==FrontEffect::None && ui.screen()==Screen::Lobby,"local fallback no longer creates a freestanding game");

    // A busy child port must not leave a half-created mission or kill the entry.
    auto occupied=Net::listen(uint16_t(m.port+1));
    auto create=[&](std::string_view name,std::string_view pilot,std::string_view core=sessionCoreFile(Scenario::Conquest)){
        require(browser.createGame("localhost:"+std::to_string(m.port),name,pilot,core),"hostname creation did not start");std::optional<Net::GameCreationResult> result;
        wait([&]{if(!result)result=browser.takeCreation();return result.has_value();},"creation reply");return *result;
    };
    auto alpha=create(std::string(64,'A'),"Creator Alpha");
    require(alpha.error.empty() && Net::parseEndpoint(alpha.address).numeric && alpha.address.ends_with(":"+std::to_string(m.port+2)) && host.games()==2 && m.server->clients()==0,"hostname creation loses its observed numeric route, full name, occupied-port handling or allocates an entry pilot");
    auto join=[&](const Net::GameCreationResult& result,const char* name,std::array<uint64_t,2> token){
        size_t index=replicas.size();replicas.emplace_back();
        clients.push_back(std::make_unique<Net::Client>(result.address,m.digest,name,0,[&,index](const SessionOptions& options)->Game&{
            replicas[index]=std::make_unique<Game>(art,m.loader,0,0,options,EffectLoader{},GameRole::Replica);return *replicas[index];},token));
        return clients.size()-1;
    };
    auto intruder=join(alpha,"Other pilot",{});
    wait([&]{return clients[intruder]->closed();},"unreserved join refusal");
    require(clients[intruder]->error().find("mission creator")!=std::string::npos,"another pilot can steal created-game ownership");
    auto ai=join(alpha,"Creator Alpha",alpha.token);wait([&]{return clients[ai]->ready();},"creator admission");
    require(replicas[ai]->state().networkHost && replicas[ai]->sessionOptions().name==std::string(64,'A'),"creator is not mission owner or full game name is lost");
    auto beta=create("Second mission","Creator Beta");auto bi=join(beta,"Creator Beta",beta.token);wait([&]{return clients[bi]->ready();},"second creator admission");
    require(host.games()==3 && replicas[bi]->state().networkHost && replicas[bi]->sessionOptions().name=="Second mission" && alpha.address!=beta.address,"created missions share ownership, names or listeners");
    auto guest=join(alpha,"Alpha guest",{});wait([&]{return clients[guest]->ready();},"normal later admission");
    require(!replicas[guest]->state().networkHost,"later guest replaces creator");
    auto rejected=create("Unsupported core","Nobody","not_a_core");require(!rejected.error.empty() && host.games()==3,"unsupported core creates a mission");
    m.server->pause(true);rejected=create("Paused creation","Nobody");require(!rejected.error.empty() && host.games()==3,"paused authority allows creation");
    auto pausedGuest=join(alpha,"Paused guest",{});wait([&]{return clients[pausedGuest]->closed();},"service pause on child mission");require(clients[pausedGuest]->error()==Allegiance::MissionServerPausedMessage,"created mission ignores service-wide admission pause");m.server->pause(false);
    browser.findPlayer("creator beta");std::optional<Net::PlayerSearchResult> found;
    wait([&]{if(!found)found=browser.takePlayerSearch();return found.has_value();},"cross-mission player search");
    require(found->game && found->game->info.instance==clients[bi]->instance() && browser.listings().size()==3 && browser.authorities()[0].info.count==3,"directory/player search only covers entry mission or loses identity across address families");

    auto request=[&](Net::CreateGameRequest query,bool invalid=false){
        auto transport=Net::clientTransport();auto connection=transport->open(address);std::optional<Net::GameCreated> result;bool closed=false;
        wait([&]{auto incoming=transport->receive();for(const auto& e:transport->events())if(e.peer==connection){if(e.connected){Net::Header h{};h.kind=Net::Kind::CreateGame;std::vector<std::byte> bytes;Net::append(bytes,h);Net::append(bytes,query);require(transport->send(connection,bytes,Net::Lane::Control),"raw creation send failed");}else closed=true;}
            for(const auto& message:incoming){Net::Reader r(message.data);auto h=r.take<Net::Header>();require(h.kind==Net::Kind::GameCreated,"wrong create reply kind");result=r.take<Net::GameCreated>();}
            return invalid?closed:result.has_value();},"idempotent create request");
        transport->close(connection,"Check complete");return result;
    };
    Net::CreateGameRequest query{};query.identity={Net::buildDigest(),m.digest};query.request={771,882};Net::text(query.name,"Retry-safe mission");Net::text(query.pilot,"Retry pilot");Net::text(query.core,sessionCoreFile(Scenario::Conquest));
    auto first=request(query),second=request(query);require(first && second && first->port && first->port==second->port && first->token==second->token && host.games()==4,"repeated creation allocates another mission or reservation");
    Net::text(query.name,"Changed retry");request(query,true);require(host.games()==4,"reused creation identity changes mission");
    query.request={773,884};query.identity.content[0]^=1;request(query,true);require(host.games()==4,"incompatible data allocates a mission");
    query.identity={Net::buildDigest(),m.digest};query.request={775,886};query.identity.build[0]^=1;request(query,true);require(host.games()==4,"incompatible build allocates a mission");
    query.identity={Net::buildDigest(),m.digest};query.request={777,888};query.reserved[0]=1;request(query,true);require(host.games()==4,"malformed record allocates a mission");
    query.reserved={};query.request={779,890};Net::text(query.name,"");auto empty=request(query);require(empty && empty->error[0] && host.games()==4,"empty game name allocates a mission");
    clients.clear();wait([&]{return host.games()==2;},"empty joined mission retirement");
    host.poll(std::chrono::steady_clock::now()+std::chrono::seconds(31));require(host.games()==1,"unclaimed creation consumes a permanent server slot");
    query.request={771,882};Net::text(query.name,"Retry-safe mission");auto expired=request(query);require(expired && expired->error[0] && !expired->port && host.games()==1,"expired request returns a recycled listener or allocates another mission");
    browser.stop();require(!browser.takeCreation(),"stopped browser retains creation result");
    puts("Original remote Create Game: authored server/core selection and gates, full names, encrypted directory/creation, busy ports, creator reservation, separate missions, later guests, unsupported/paused/incompatible refusal, retry identity, cross-mission player search, empty/unclaimed mission retirement and expired reservation refusal passed");
}
void publicDirectoryDiscovery(const std::filesystem::path& art){
    Match m(art,27821,false,true);m.ca.reset();m.cb.reset();
    Net::ServerHost host(*m.server,m.digest,m.port,[&](const SessionOptions& options){
        auto game=std::make_unique<Game>(art,m.loader,0,0,options,EffectLoader{},GameRole::Authority);game->quickChats(m.quickChat);return game;
    },m.assets.coreCatalog());
    Net::PublicDirectory directory(27820);
    Net::PublicDirectory secondaryDirectory(27819);
    const std::string directoryAddress="localhost:27820";
    auto registrar=std::make_unique<Net::DirectoryRegistration>(directoryAddress,m.port,m.digest);
    auto secondaryRegistrar=std::make_unique<Net::DirectoryRegistration>("127.0.0.1:27819",m.port,m.digest);
    Net::ServerBrowser browser(m.digest);require(browser.add(directoryAddress) && browser.add("127.0.0.1:27819"),"public directory entry fixture");
    std::vector<std::unique_ptr<Net::Client>> clients;std::unique_ptr<Game> replica;
    auto wait=[&](auto condition,const char* what){
        auto end=std::chrono::steady_clock::now()+std::chrono::seconds(8);
        do{directory.poll();secondaryDirectory.poll();if(registrar)registrar->poll();if(secondaryRegistrar)secondaryRegistrar->poll();m.server->poll();host.poll();m.server->step();host.step();for(auto& client:clients)client->poll();browser.poll();
            if(condition())return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}while(std::chrono::steady_clock::now()<end);
        throw std::runtime_error(std::string("Public directory timeout: ")+what);
    };
    wait([&]{return registrar->registered() && secondaryRegistrar->registered() && directory.authorities()==1 && secondaryDirectory.authorities()==1;},"verified authority registration");
    // Reset the read-only probe's schedule so the integration test need not
    // wait five real seconds for the normal idle browser refresh.
    browser.stop();wait([&]{return browser.listings().size()==1 && browser.authorities().size()==1;},"original browser expansion from only a public directory endpoint");
    const std::string authorityAddress=browser.listings()[0].address;
    require(Net::parseEndpoint(authorityAddress).numeric && authorityAddress.ends_with(":"+std::to_string(m.port)) && browser.authorities()[0].address==authorityAddress && m.server->clients()==0,"hostname public directory is exposed as a game/creation server, loses its numeric authority route or allocates a pilot");
    Settings settings;settings.pilot="Registry pilot";Frontend ui(settings);ui.originalInterface(m.assets);ui.open(Screen::Games);ui.gameAuthorities(browser.authorities());
    auto item=[&](std::string_view name){auto i=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& row){return row.control==name;});require(i!=ui.view().items.end(),"public discovery original creation control missing");return size_t(i-ui.view().items.begin());};
    ui.activate(item("newGameButtonPane"));ui.activate(item("create/server/1"));
    std::vector<int> coreRows;for(const auto& row:ui.view().items)if(row.action.op==MenuOp::GameCreateCore)coreRows.push_back(row.action.value);
    bool compatible=false;
    for(int row:coreRows){
        // Original list rows remain selectable when gray. Picking an
        // incompatible core retires the server selection; it is not a disabled
        // widget. Use the real catalog/conjunction instead of enabled state.
        ui.activate(item("create/core/"+std::to_string(row)));ui.activate(item("create/server/1"));
        if(ui.createAddress()==authorityAddress && ui.view().items[item("createOkButtonPane")].enabled){compatible=true;break;}
    }
    require(compatible && browser.authorities()[0].cores==m.assets.coreCatalog(),"public authority drops its real catalog or has no compatible authored core row");
    require(ui.view().original && ui.createAddress()==authorityAddress && ui.view().items[item("createOkButtonPane")].enabled,"public authority does not populate the original selectable server/core controls");
    auto creation=ui.activate(item("createOkButtonPane"));require(creation.effect==FrontEffect::CreateGame && ui.createAddress()==authorityAddress,"original dialog sends creation to the registry instead of the authority");
    require(browser.createGame(ui.createAddress(),ui.createName(),settings.pilot,ui.createCore()),"publicly discovered authority refuses creation");
    std::optional<Net::GameCreationResult> created;wait([&]{if(!created)created=browser.takeCreation();return created.has_value();},"remote creation through public discovery");
    require(created->error.empty() && created->address!=authorityAddress && host.games()==2,"public creation does not create a separate real hosted mission");
    clients.push_back(std::make_unique<Net::Client>(created->address,m.digest,"Registry pilot",0,[&](const SessionOptions& session)->Game&{
        replica=std::make_unique<Game>(art,m.loader,0,0,session,EffectLoader{},GameRole::Replica);replica->quickChats(m.quickChat);return *replica;
    },created->token));
    wait([&]{return clients.front()->ready();},"joining the created mission");
    Net::ServerBrowser search(m.digest);search.add(directoryAddress);search.findPlayer("Registry pilot");std::optional<Net::PlayerSearchResult> found;
    wait([&]{search.poll();if(!found)found=search.takePlayerSearch();return found.has_value();},"Find Player across newly expanded public authorities");
    require(found->game && found->game->info.instance==clients.front()->instance(),"public Find Player loses the hosted mission/creator across address families");search.stop();
    registrar.reset();wait([&]{return directory.authorities()==0;},"registration disconnect retirement");
    browser.findPlayer("No such pilot");std::optional<Net::PlayerSearchResult> absent;
    wait([&]{if(!absent)absent=browser.takePlayerSearch();return absent.has_value();},"refreshed multiple-directory membership");
    require(!absent->game && browser.listings().size()==2 && browser.authorities().size()==1,"withdrawing one directory removes another directory's live membership or duplicates it");
    Net::ServerBrowser saved(m.digest);saved.add(authorityAddress);wait([&]{saved.poll();return saved.listings().size()==2;},"explicit saved authority fixture");
    secondaryRegistrar.reset();wait([&]{return secondaryDirectory.authorities()==0;},"last directory publisher retirement");
    browser.findPlayer("No such pilot");wait([&]{saved.poll();return browser.listings().empty() && browser.authorities().empty();},"withdrawn directory membership retired from original lists");
    require(saved.listings().size()==2,"directory withdrawal removes explicitly saved endpoints");saved.stop();
    require(clients.front()->ready() && replica,"directory withdrawal interrupts existing game clients");
    registrar=std::make_unique<Net::DirectoryRegistration>(directoryAddress,m.port,m.digest);
    wait([&]{return registrar->registered() && directory.authorities()==1;},"registration reconnect");browser.stop();
    wait([&]{return browser.listings().size()==2 && browser.authorities().size()==1;},"restored authority and hosted missions");
    auto wrongDigest=m.digest;wrongDigest[0]^=1;
    Net::DirectoryRegistration incompatible(directoryAddress,m.port,wrongDigest);
    wait([&]{incompatible.poll();return !incompatible.error().empty();},"data-incompatible registration rejection");
    require(!incompatible.registered() && directory.authorities()==1,"data-incompatible registration is published or withdraws a valid publisher");
    browser.stop();clients.clear();registrar.reset();
    puts("Encrypted public discovery: verified registration, original server/core/creation controls, real multi-game creation/join, Find Player expansion, coalesced multiple directories, independent withdrawal, saved endpoints, uninterrupted gameplay, reconnect and incompatible-data rejection passed");
}
void browseGames(const std::filesystem::path& art){
    Match m(art,27664,false,true);m.ca.reset();m.cb.reset();
    auto ships=m.authority->core()->GetShips()->n();auto firstPilot=m.authority->state().shipId;
    Net::ServerBrowser browser(m.digest);auto address="127.0.0.1:"+std::to_string(m.port);
    require(browser.add(address) && !browser.add(address),"browser deduplicates endpoints");
    for(int i=0;i<1200 && browser.listings().empty();++i){browser.poll();m.server->poll();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    auto listings=browser.listings();require(listings.size()==1,"encrypted read-only server query returns a live listing");
    require(listings[0].address==address && listings[0].info.players==0 && (listings[0].info.flags&Net::ServerWaiting) && (listings[0].info.flags&Net::ServerJoinable),"unclaimed lobby listing reports authoritative state");
    require(std::string(listings[0].info.name)=="Conquest" && std::string(listings[0].info.core)==m.authority->core()->GetMissionParams()->szIGCStaticFile && std::string(listings[0].info.core)==sessionCoreFile(Scenario::Conquest) && listings[0].info.session.rules.startCountdown==1,"listing carries the actual loaded core, mission rules and names");
    require(m.server->clients()==0 && m.authority->core()->GetShips()->n()==ships && m.authority->state().shipId==firstPilot,"browsing does not allocate or claim a pilot, change ships, or publish a baseline");
    Net::ServerBrowser aliases(m.digest);aliases.add(address);
    for(auto host:{"[::1]:","[::ffff:127.0.0.1]:","localhost:","LOCALHOST:"}){
        auto route=std::string(host)+std::to_string(m.port);aliases.add(route);Net::ServerBrowser single(m.digest);single.add(route);
        for(int i=0;i<1200 && single.listings().empty();++i){single.poll();m.server->poll();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
        auto rows=single.listings();require(rows.size()==1 && rows[0].info.instance==listings[0].info.instance,"encrypted IPv6/mapped-IPv4 alias fails browse or changes mission identity");single.stop();
    }
    auto aliasRevision=aliases.revision();
    for(int i=0;i<1200 && aliases.revision()<aliasRevision+5;++i){aliases.poll();m.server->poll();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    auto shared=aliases.listings();require(aliases.revision()>=aliasRevision+5 && shared.size()==1 && shared[0].info.instance==listings[0].info.instance && Net::validInstance(shared[0].info.instance),"encrypted hostname/IPv4/IPv6 aliases duplicate a mission or change its instance");
    aliases.stop();
    auto retiredInstance=listings[0].info.instance;retiredInstance[0]^=1;bool allocated=false;
    {Net::Client retired(address,m.digest,"Retired selection",0,[&](const SessionOptions&)->Game&{allocated=true;throw std::runtime_error("Retired selection allocated a replica");},{},retiredInstance);
        for(int i=0;i<1200 && !retired.closed();++i){m.server->poll();retired.poll();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
        require(retired.closed() && retired.error()=="The selected game is no longer available." && !allocated && m.server->clients()==0,"port reuse accepts a stale mission selection or allocates a pilot/replica");}
    Net::ServerBrowser discovered(m.digest);
    auto hasFixture=[&]{auto rows=discovered.listings();return std::any_of(rows.begin(),rows.end(),[&](const auto& row){return row.address.ends_with(":"+std::to_string(m.port));});};
    for(int i=0;i<1200 && !hasFixture();++i){discovered.poll(true);m.server->poll();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    auto lan=discovered.listings();
    require(std::any_of(lan.begin(),lan.end(),[&](const auto& row){return row.address.ends_with(":"+std::to_string(m.port)) && std::string(row.info.name)=="Conquest";}),"automatic LAN discovery fails to produce a real encrypted listing without an explicit endpoint");
    require(m.server->clients()==0 && m.authority->core()->GetShips()->n()==ships && m.authority->state().shipId==firstPilot,"LAN discovery/read-only querying claims a pilot or mutates the authority");
    discovered.stop();
    browser.stop();m.ca=std::make_unique<Net::Client>("localhost:"+std::to_string(m.port),m.digest,"Alpha",0,[&](const SessionOptions& options)->Game&{
        m.a=std::make_unique<Game>(art,m.loader,0,0,options,EffectLoader{},GameRole::Replica);m.a->loadoutPresets(m.presets);m.a->quickChats(m.quickChat);return *m.a;
    },std::array<uint64_t,2>{},listings[0].info.instance);m.cb=m.client("Bravo",m.b);m.join();
    require(m.ca->instance()==listings[0].info.instance && m.cb->instance()==listings[0].info.instance,"direct join does not bind subsequent reconnect/round welcomes to the mission instance");
    auto alphaId=m.a->state().shipId;m.ca->reconnect();m.wait([&]{return m.ca->ready();},"hostname authenticated reconnect");
    require(m.a->state().shipId==alphaId && m.ca->instance()==listings[0].info.instance && m.server->clients()==2,"hostname reconnect loses pilot/mission identity or allocates another seat");
    require(m.ca->serverName()=="Native test server" && m.cb->serverName()==listings[0].info.server,"authenticated join loses the advertised server name");
    {Assets assets(art);Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.networkSession(m.a->sessionOptions(),m.ca->serverName());ui.setPlaying(true);ui.open(Screen::RemoteLobby,&m.a->state());
        auto text=[&](std::string_view name,std::string_view value){return std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.name==name && c.text==value;});};
        require(text("textServername","Native test server") && text("textCorename",assets.coreName(sessionCoreFile(Scenario::Conquest))),"connected original lobby loses authenticated server/core labels");}
    require(m.a->state().networkHost || m.b->state().networkHost,"first real join retains game ownership after browsing");
    auto* owner=m.a->state().networkHost?m.ca.get():m.cb.get();
    Net::Action lock{};lock.op=Net::Operation::Lobby;lock.args[0]=int(LobbyAction::LockLobby);lock.args[1]=1;owner->action(lock);
    m.wait([&]{return (m.a->state().lobbyFlags&1) && (m.b->state().lobbyFlags&1);},"authority locks lobby");
    auto revision=browser.revision();
    for(int i=0;i<1200 && browser.revision()==revision;++i){browser.poll();m.tick(1);}
    listings=browser.listings();require(listings.size()==1 && listings[0].info.players==2 && (listings[0].info.flags&Net::ServerLocked) && !(listings[0].info.flags&Net::ServerJoinable),"refreshed listing reports occupants and locked admission");
    auto search=[&](std::string_view name){
        browser.findPlayer(name);std::optional<Net::PlayerSearchResult> result;
        m.wait([&]{browser.poll();if(!result)result=browser.takePlayerSearch();return result.has_value();},"read-only Find Player query completes");
        require(!browser.takePlayerSearch() && m.server->clients()==2,"player lookup repeats a result or creates a pilot");return result->game;
    };
    auto located=search("aLPHa");require(located && located->address==address && located->info.players==2 && !(located->info.flags&Net::ServerPlayerFound),"Find Player fails original exact case-insensitive matching or leaks a query-only flag into game rows");
    require(!search("Alph") && !search("Missing pilot") && !search("") && !search(std::string(32,'X')),"Find Player treats a prefix, missing, empty or impossible-length name as an exact callsign");
    browser.findPlayer("Alpha");browser.findPlayer("Bravo");std::optional<Net::PlayerSearchResult> superseded;
    m.wait([&]{browser.poll();if(!superseded)superseded=browser.takePlayerSearch();return superseded.has_value();},"superseding Find Player search completes");require(superseded->game && superseded->game->address==address,"a superseded browser search prevents its replacement query");
    m.server->pause(true);require(!search("Alpha"),"paused authority is found despite original lobby exclusion");m.server->pause(false);
    browser.findPlayer("Alpha");browser.stop();require(!browser.takePlayerSearch(),"stopped browser retains a completed lookup for a retired UI");
    Net::ServerBrowser empty(m.digest);empty.findPlayer("Alpha");empty.poll();auto absent=empty.takePlayerSearch();require(absent && !absent->game,"empty directory leaves Find Player waiting indefinitely");empty.stop();
    puts("Encrypted Find Player: exact case-insensitive presence, nonmatches/lengths, paused exclusion, supersession, probe-only flags, no pilot allocation, completion consumption and stopped/empty browser retirement passed");
    browser.stop();auto incompatible=m.digest;incompatible[0]^=1;Net::ServerBrowser other(incompatible);other.add(address);
    auto rejected=m.server->metrics().rejected;
    for(int i=0;i<1200 && m.server->metrics().rejected==rejected;++i){other.poll();m.tick(1);}
    require(m.server->metrics().rejected>rejected && other.listings().empty() && m.server->clients()==2,"read-only queries enforce data identity and cannot disturb admitted pilots");
    other.stop();Net::ServerBrowser retired(m.digest);
    auto hasRetiredFixture=[&]{auto rows=retired.listings();return std::any_of(rows.begin(),rows.end(),[&](const auto& row){return row.address.ends_with(":"+std::to_string(m.port));});};
    for(int i=0;i<1200 && !hasRetiredFixture();++i){retired.poll(true);m.server->poll();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    require(hasRetiredFixture(),"LAN retirement fixture did not discover its live server");
    m.ca.reset();m.cb.reset();m.server.reset();
    auto expires=std::chrono::steady_clock::now()+std::chrono::seconds(6);
    while(std::chrono::steady_clock::now()<expires && hasRetiredFixture()){retired.poll(true);std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    require(!hasRetiredFixture(),"automatic LAN list retains a stopped authority beyond its discovery expiry");retired.stop();
    puts("LAN browser: automatic endpoint discovery, encrypted metadata without pilot/world allocation, stopped-authority expiry and probe teardown passed");
    puts("Games browser: live encrypted listing, no pilot/world allocation, first-join ownership, occupancy/lock refresh and incompatible-data rejection passed");
}
void randomizeTeams(const std::filesystem::path& art) {
    auto action=[](Net::Client& client,LobbyAction kind,int value=0,int target=-1){
        Net::Action a{};a.op=Net::Operation::Lobby;a.args={int(kind),value,target,0};require(client.action(a),"lobby action queued");
    };
    auto request=[](Net::Client& client,int side){Net::Action a{};a.op=Net::Operation::Team;a.args[0]=side;require(client.action(a),"team request queued");};
    auto member=[](Game& game,int id){auto i=std::find_if(game.state().networkPilots.begin(),game.state().networkPilots.end(),[&](const auto& p){return p.id==id;});require(i!=game.state().networkPilots.end(),"public pilot identity missing");return *i;};
    for(bool balance:{false,true}){
        Match m(art,27662,false,true,false,Scenario::Conquest,1,balance?32766:32767);m.join();
        auto* owner=m.a->state().networkHost?m.ca.get():m.cb.get();auto* other=owner==m.ca.get()?m.cb.get():m.ca.get();
        auto* ownerGame=owner==m.ca.get()?m.a.get():m.b.get();auto* otherGame=other==m.ca.get()?m.a.get():m.b.get();
        Net::Action start{};start.op=Net::Operation::Start;
        if(balance){
            auto rejected=owner->metrics().rejected;require(owner->action(start),"unbalanced Start queued");
            m.wait([&]{return owner->metrics().rejected>rejected;},"authority rejects unbalanced Start");
            require(m.authority->core()->GetMissionStage()==STAGE_NOTSTARTED,"rejected Start does not advance mission stage");
        }
        request(*other,1);m.wait([&]{return otherGame->state().playerSide==1 && otherGame->state().commander;},"opposing commander fixture");
        int ownerId=ownerGame->state().shipId,otherId=otherGame->state().shipId;auto match=owner->match();uint64_t rejected{};
        std::array<std::unique_ptr<Game>,4> games;
        std::array<std::unique_ptr<Net::Client>,4> clients;
        for(int i=0;i<4;++i)clients[i]=m.client(("Wingman "+std::to_string(i)).c_str(),games[i],i<2?0:1);
        auto poll=[&]{for(auto& client:clients){client->poll();if(client->closed())throw std::runtime_error("Additional pilot disconnected: "+client->error());}};
        m.wait([&]{poll();return std::all_of(clients.begin(),clients.end(),[](const auto& p){return p->ready();}) && ownerGame->state().networkPilots.size()==6;},"four non-commanders authenticate");
        std::array<int,4> ids;for(int i=0;i<4;++i)ids[i]=games[i]->state().shipId;
        Net::Action away{};away.op=Net::Operation::Ready;away.args[0]=0;require(clients[3]->action(away),"AFK pilot fixture");
        action(*other,LobbyAction::AutoAccept,0);
        action(*other,LobbyAction::Boot,0,ids[2]);
        m.wait([&]{poll();return member(*ownerGame,ids[2]).team==noTeam && !member(*ownerGame,ids[3]).ready && !(ownerGame->state().autoAccept&2);},"Boot ban and AFK fixtures replicate");
        rejected=clients[2]->metrics().rejected;
        request(*clients[2],1);
        m.wait([&]{poll();return clients[2]->metrics().rejected>rejected;},"pre-flush Boot ban remains active");
        action(*owner,LobbyAction::AutoAccept,0);
        m.wait([&]{poll();return !(ownerGame->state().autoAccept&1);},"pending-request fixture disables owner Auto Accept");
        request(*clients[2],0);
        m.wait([&]{poll();return member(*ownerGame,ids[2]).requested==0;},"No Team request before Randomize");
        action(*owner,LobbyAction::LockSides,1);m.wait([&]{poll();return ownerGame->state().lobbyFlags&2;},"locked sides fixture");
        rejected=other->metrics().rejected;action(*other,LobbyAction::Randomize);
        m.wait([&]{poll();return other->metrics().rejected>rejected;},"non-owner commander cannot randomize");
        action(*owner,LobbyAction::Randomize);
        m.wait([&]{poll();return ownerGame->state().autoAccept==0 && std::all_of(ids.begin(),ids.end(),[&](int id){return member(*ownerGame,id).team==noTeam;});},"Randomize flushes nonleaders and respects side locks");
        require(owner->match()==match && ownerGame->state().shipId==ownerId && otherGame->state().shipId==otherId &&
            ownerGame->state().networkHost && ownerGame->state().commander && otherGame->state().commander && (ownerGame->state().lobbyFlags&2),"Randomize preserves world, commanders, identities and locks");
        require(member(*ownerGame,ids[2]).requested==(balance?noTeamRequest:0),"balance clears all requests; Flush preserves preexisting No Team request");
        require(!member(*ownerGame,ids[3]).ready,"Randomize does not rewrite AFK state");
        m.wait([&]{poll();return std::any_of(games[1]->state().messages.begin(),games[1]->state().messages.end(),[](const auto& n){return n.modal;});},"original per-pilot reason delivered");
        const auto& messages=games[1]->state().messages;
        require(std::any_of(messages.begin(),messages.end(),[&](const auto& n){return n.modal && n.text==(balance?
            "You have been removed from your team and assigned to another because the teams have been balanced.":
            "You have been removed from your team because the Game Commander flushed all teams.");}),"original balance/flush reason text");
        action(*owner,LobbyAction::LockSides,0);m.wait([&]{poll();return !(ownerGame->state().lobbyFlags&2);},"unlock sides after flush");
        if(!balance){
            request(*clients[2],1);
            m.wait([&]{poll();return member(*ownerGame,ids[2]).requested==1;},"Flush clears Boot ban and leaves Auto Accept off");
        }else{
            action(*owner,LobbyAction::Randomize);
            m.wait([&]{poll();return member(*ownerGame,ids[0]).team>=0 && member(*ownerGame,ids[1]).team>=0 && member(*ownerGame,ids[2]).team>=0;},"Enforce Balance assigns ready pilots");
            require(member(*ownerGame,ids[3]).team==noTeam,"AFK player stays in No Team during balance");
            std::array<int,2> counts{};for(const auto& p:ownerGame->state().networkPilots)if(p.team>=0)++counts[p.team];
            require(std::abs(counts[0]-counts[1])<=1,"rank-one balance spreads ready pilots evenly");
            auto lobbyOrder=std::array<int,3>{ids[2],std::min(ids[0],ids[1]),std::max(ids[0],ids[1])};
            for(int i=0;i<3;++i)require(member(*ownerGame,lobbyOrder[i]).team==i%2,"equal-rank balance retains No Team arrival order and original first-side ties");
            require(owner->action(start),"balanced Start queued");
            m.wait([&]{poll();return ownerGame->state().networkCountdown>=0;},"starting countdown replicated");
            require(m.authority->core()->GetMissionStage()==STAGE_STARTING && ownerGame->core()->GetMissionStage()==STAGE_STARTING,"countdown uses original mission stage");
            rejected=owner->metrics().rejected;require(owner->action(start),"duplicate Start queued");action(*owner,LobbyAction::Randomize);
            m.wait([&]{poll();return owner->metrics().rejected>=rejected+2;},"countdown cannot be restarted or randomized");
            require(owner->action(away),"Away cancels countdown");
            m.wait([&]{poll();return ownerGame->state().networkCountdown<0 && !ownerGame->state().networkReady;},"readiness withdrawal cancels countdown");
            require(m.authority->core()->GetMissionStage()==STAGE_NOTSTARTED && ownerGame->core()->GetMissionStage()==STAGE_NOTSTARTED,"cancellation restores original mission stage");
            away.args[0]=1;require(owner->action(away),"owner ready again");
            m.wait([&]{poll();return ownerGame->state().networkReady;},"readiness restored");
            require(owner->action(start),"restart valid countdown");
            m.wait([&]{poll();return !ownerGame->state().networkLobby && !otherGame->state().networkLobby;},"balanced match starts while AFK pilot remains No Team");
        }
    }
    puts("Original Randomize: Flush/Enforce Balance, commanders, bans, requests, AFK, locks, stable rank-one ties, notices and start/countdown authority pass");
}
void effectLifetimes(const std::filesystem::path &art) {
    Match m(art,27647,false,false,true);
    m.join();
    auto* ship=m.authority->core()->GetShip(m.a->state().shipId);
    auto* cluster=ship->GetStation()->GetCluster();
    m.a->commandView(true);
    require(m.a->viewSector(cluster->GetObjectID()),"docked command-view effect sector");
    auto visible=[&] {
        m.a->camera({});
        auto draws=m.a->particles();
        return std::count_if(draws.begin(),draws.end(),[&](auto& p) {
            auto id=p.texture&0x0fffffffu;
            for(auto name:{"exp20bmp","exp22bmp","exp23bmp","exp24bmp","exp25bmp"})
                if(id==m.effectTextures.at(name).id)return true;
            return false;
        });
    };
    cluster->GetClusterSite()->AddExplosion(ship->GetStation(),c_etSmallShip);
    require(m.authority->sendChat(ChatChannel::Team,"Capture watermark fixture"),"private capture notice fixture");
    m.authority->beginNetworkPublication();
    auto complete=m.authority->networkFrame(ship->GetObjectID(),m.server->tick(),0,false,{},true);
    require(!complete.events.empty() && !complete.notices.empty(),"capture watermark fixture contains effects and notices");
    // Compare to the old post-capture removal on identical state. Alternate
    // per-peer cursors through the same shared world cache, including bootstrap.
    for(Net::Frame::Effects cursor:std::array<Net::Frame::Effects,4>{{{}, {complete.notices.back().number,complete.events.back().number},
        {complete.notices.front().number,complete.events.front().number}, {}}}){
        auto expected=complete;
        size_t notices=std::erase_if(expected.notices,[&](auto& n){return n.number<=cursor.notices;});
        size_t events=std::erase_if(expected.events,[&](auto& e){return e.number<=cursor.events;});
        auto captured=m.authority->networkFrame(ship->GetObjectID(),m.server->tick(),0,true,{},true,cursor);
        require(captured.omitted.notices==notices && captured.omitted.events==events,"capture watermark omission accounting");
        require(Net::packet(captured)==Net::packet(expected),"pre-capture watermarks change original packet bytes/shared cache");
    }
    m.wait([&]{return visible()>0;},"network explosion appears while observer is docked");
    require(m.a->state().docked,"effect aging does not launch the observer");
    m.tick(360);
    require(visible()==0,"docked command-view explosions retire without predicted flight inputs");
    auto frame=m.authority->networkFrame(ship->GetObjectID(),m.server->tick(),0);
    m.a->applyNetworkFrame(frame);
    require(visible()==0,"acknowledged explosion does not restart on another publication");
    // A later subscriber needs the actual persistent origins, even after the
    // effect event window has expired. Repeated hits within one smoke band do
    // not change the original geometry or produce more wire events.
    ship->SetFraction(.65f);auto hit=Vector(5,6,7);
    ship->GetThingSite()->AddDamage(hit,.65f);
    for(int i=0;i<100;++i)ship->GetThingSite()->AddDamage(Vector(30,40,50),.64f);
    m.authority->beginNetworkPublication();
    auto damaged=m.authority->networkFrame(ship->GetObjectID(),m.server->tick(),0);
    require(std::none_of(damaged.events.begin(),damaged.events.end(),[](auto& e){return e.kind==Net::EventKind::Damage;}),
            "persistent smoke is not retransmitted as redundant hit events");
    size_t payload=0;bool found=false;
    for(const auto& entity:damaged.entities) {
        if(entity.identity.type==OT_ship && entity.identity.id==ship->GetObjectID()) {
            require(entity.flags&Net::DamageState,"full ship includes persistent damage state");
            Net::ShipDamageState state;std::memcpy(&state,damaged.payload.data()+payload+sizeof(DataShipIGC),sizeof(state));
            auto local=ship->GetOrientation().TimesInverse(hit*.33f);
            require(state.count==3 && state.frames[0].position.x==local.x && state.frames[2].position.z==local.z,
                    "smoke bands retain original first-hit local geometry");
            require(state.frames[3].position.x==0 && state.frames[3].direction.z==0,"unused smoke slots stay zero");
            found=true;
        }
        payload+=entity.payloadBytes;
    }
    require(found,"damage state fixture ship present");m.a->applyNetworkFrame(damaged);
    ship->SetFraction(1);m.authority->beginNetworkPublication();
    auto repaired=m.authority->networkFrame(ship->GetObjectID(),m.server->tick(),0);m.a->applyNetworkFrame(repaired);
    auto malformed=damaged;payload=0;
    for(const auto& entity:malformed.entities){if(entity.identity.type==OT_ship && entity.identity.id==ship->GetObjectID()) {
        uint32_t count=11;std::memcpy(malformed.payload.data()+payload+sizeof(DataShipIGC),&count,sizeof(count));break;}payload+=entity.payloadBytes;}
    bool rejected=false;try{m.a->applyNetworkFrame(malformed);}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"replica rejects damage state outside fixed smoke slots");
    m.report("Effect lifetimes: docked command view ages and retires explosions without input or replay");
}
void drops(const std::filesystem::path &art) {
    Match m(art,27646);
    m.join();
    m.a->launch();
    m.wait([&]{return !m.a->state().docked;},"drop pilot launches");
    auto* core=m.authority->core();
    auto* ship=core->GetShip(m.a->state().shipId);
    ship->SetPosition(Vector(8000,3000,2000));
    ship->SetVelocity(Vector::GetZero());
    auto* cluster=ship->GetCluster();
    auto create=[&](Vector velocity,Vector offset) {
        DataTreasureIGC data{};
        data.p0=ship->GetPosition()+offset;data.v0=velocity;
        data.time0=core->GetLastUpdate();data.createNow=true;data.lifespan=60;
        data.objectID=core->GenerateNewTreasureID();data.treasureID=100;
        data.clusterID=cluster->GetObjectID();data.treasureCode=c_tcCash;
        auto* drop=static_cast<CtreasureIGC*>(core->CreateObject(core->GetLastUpdate(),OT_treasure,&data,sizeof(data)));
        require(drop,"authoritative drop creation");
        auto id=drop->GetObjectID();drop->Release();return id;
    };
    auto moving=create(Vector(60,40,20),Vector(100,200,300));
    auto still=create(Vector::GetZero(),Vector(-200,100,300));
    for(int i=0;i<65536;++i){auto id=core->GenerateNewTreasureID();
        require(id!=NA && !core->GetTreasure(id),"treasure ID wrap skips live drops and NA");}
    m.tick(30);
    auto initial=m.authority->networkFrame(ship->GetObjectID(),m.server->tick(),0);
    auto entity=[](const Net::Frame& frame,ObjectID id)->const Net::Entity& {
        auto e=std::find_if(frame.entities.begin(),frame.entities.end(),[&](auto& e){return e.identity.type==OT_treasure&&e.identity.id==id;});
        require(e!=frame.entities.end(),"visible drop in publication");return *e;
    };
    for(int phase=0;phase<4;++phase) {
        m.tick(phase==3?450:120);
        auto frame=m.authority->networkFrame(ship->GetObjectID(),m.server->tick(),0);
        for(auto id:{moving,still}) {
            auto& e=entity(frame,id);auto& original=entity(initial,id);
            require((e.flags&(Net::Trajectory|Net::Decelerating))==(Net::Trajectory|Net::Decelerating) &&
                    e.activation==original.activation && e.lock==original.lock && e.power==original.power &&
                    std::memcmp(&e.motion,&original.motion,sizeof(e.motion))==0,
                    "moving, stopped and tumbling drop records remain invariant");
            double clock=double(core->GetLastUpdate().clock()-1000u)*.001;
            auto pose=Net::trajectoryMotion(e,clock);
            auto* drop=core->GetTreasure(id);
            auto vec=[](Net::Vec v){return Vector(v.x,v.y,v.z);};
            require(drop && (drop->GetPosition()-vec(pose.position)).Length()<.01f,
                    "drop trajectory matches original deceleration and stopping distance");
            require((drop->GetOrientation().GetForward()-vec(pose.forward)).Length()<.001f &&
                    (drop->GetOrientation().GetUp()-vec(pose.up)).Length()<.001f,
                    "drop tumble matches original arbitrary-axis rotation");
            Game late(art,m.loader,0,0,m.options,{},GameRole::Replica);
            late.applyNetworkFrame(frame);
            auto* copy=late.core()->GetTreasure(id);
            require(copy && (copy->GetPosition()-drop->GetPosition()).Length()<.01f,
                    "late publication recreates current drop pose from its birth trajectory");
            require(phase!=3 || vec(pose.velocity).Length()==0,"drop velocity clamps to zero after stopping");
        }
    }
    auto* drop=static_cast<CtreasureIGC*>(core->GetTreasure(moving));
    drop->SetAmount(42);drop->ResetExpiration(core->GetLastUpdate());
    m.wait([&]{auto* copy=m.a->core()->GetTreasure(moving);return copy&&copy->GetAmount()==42;},"drop amount changes replicated");
    require(static_cast<CtreasureIGC*>(m.a->core()->GetTreasure(moving))->NativeExpireTime()==drop->NativeExpireTime(),
            "extended drop expiration replicated");
    require(core->GetIgcSite()->HitTreasureEvent(core->GetLastUpdate(),ship,drop),"drop collected by original pickup callback");
    core->GetIgcSite()->KillTreasureEvent(drop);
    m.wait([&]{return !m.a->core()->GetTreasure(moving);},"collected drop removed on replica");
    static_cast<CtreasureIGC*>(core->GetTreasure(still))->NativeSetExpireTime(core->GetLastUpdate()+.02f);
    m.wait([&]{return !core->GetTreasure(still)&&!m.a->core()->GetTreasure(still);},"expired drop removed on authority and replica");
    m.report("Drops: invariant trajectories, stopping, tumble, late import, amount/deadline changes, pickup and expiry");
}
// Frozen former equipment capture: read metadata/state, then clear private
// enemy fields. The production path instead uses invalidated read plans and
// suppresses private reads before extraction.
void referenceEquipment(Game& game,const Net::Frame& frame,IsideIGC* viewer){
    std::vector<Net::Part> expected;
    for(const auto& e:frame.entities)if(e.identity.type==OT_ship){
        auto* ship=game.core()->GetShip(e.identity.id);require(ship,"equipment reference ship");bool own=ship->GetSide()==viewer;
        require(e.energy==(own?ship->GetEnergy():0) && e.fuel==(own?ship->GetFuel():0) && e.ore==(own?ship->GetOre():0) &&
                e.experience==(own?ship->GetExperience():0) && e.ammo==(own?ship->GetAmmo():0),"early private-state filtering differs from former capture");
        require(bool(e.flags&Net::Autopilot)==(own && ship->GetAutopilot()),"private autopilot filtering differs from former capture");
        for(int i=0;i<4;++i)require(e.controls[i]==(own?ship->GetControls().jsValues[i]:0),"private control filtering differs from former capture");
        for(Command c=0;c<c_cmdMax;++c){Net::Command command{};if(own){auto* t=ship->GetCommandTarget(c);command={int16_t(t?t->GetObjectType():NA),int16_t(t?t->GetObjectID():NA),ship->GetCommandID(c)};}
            require(std::memcmp(&e.commands[c],&command,sizeof(command))==0,"private command filtering differs from former capture");}
        size_t first=expected.size();
        for(auto* l=ship->GetParts()->first();l;l=l->next()){
            auto* p=l->data();if(!own && p->GetMountID()<0)continue;
            Net::Part part{};part.ship=e.identity;part.id=p->GetPartType()->GetObjectID();part.mount=p->GetMountID();part.amount=p->GetAmount();part.mounted=p->GetMountedFraction();auto equipment=p->GetEquipmentType();
            if(equipment==ET_Weapon){auto* w=static_cast<CweaponIGC*>(p);part.timeFired=w->NativeNextFire().clock();part.shots=w->NativeShots();part.flags=w->NativeFlags();}
            if(equipment==ET_Shield)part.fraction=static_cast<IshieldIGC*>(p)->GetFraction();
            if(equipment==ET_Afterburner)part.fraction=static_cast<IafterburnerIGC*>(p)->GetPower();
            if(equipment==ET_Magazine || equipment==ET_Dispenser || equipment==ET_ChaffLauncher){part.timeLoaded=static_cast<IlauncherIGC*>(p)->GetTimeLoaded().clock();
                if(equipment==ET_Magazine){part.lock=static_cast<ImagazineIGC*>(p)->GetLock();part.shots=static_cast<CmagazineIGC*>(p)->NativeShots();}}
            if(!own){part.amount=0;part.lock=0;part.timeLoaded=0;}expected.push_back(part);
        }
        require(expected.size()-first==e.partCount,"equipment plan changes original per-ship count");
    }
    require(expected.size()==frame.parts.size() && (expected.empty() || std::memcmp(expected.data(),frame.parts.data(),expected.size()*sizeof(Net::Part))==0),"equipment plan changes original record bytes/order");
}
void sameCapturePacket(Net::Frame& a,Net::Frame& b,const char* message){
    auto direct=Net::packet(a),deferred=Net::packet(b);
    if(direct!=deferred){
        size_t n=std::min(direct.size(),deferred.size()),first=0;while(first<n && direct[first]==deferred[first])++first;
        fprintf(stderr,"%s: first byte %zu, packet sizes %zu/%zu, entities %zu/%zu, parts %zu/%zu, payload %zu/%zu\n",message,first,direct.size(),deferred.size(),a.entities.size(),b.entities.size(),a.parts.size(),b.parts.size(),a.payload.size(),b.payload.size());
    }
    require(direct==deferred,message);
}
void projectiles(const std::filesystem::path &art) {
    Match m(art, 27635);
    m.join();
    m.a->launch();
    m.wait([&] { return !m.a->state().docked; }, "projectile pilot launches");
    m.tick(120);
    auto *core = m.authority->core();
    auto *shooter = core->GetShip(m.a->state().shipId);
    auto *cluster = shooter->GetCluster();
    shooter->SetPosition(Vector(4000, 0, 0));
    shooter->SetVelocity(Vector::GetZero());
    for (auto *l = shooter->GetParts()->first(); l; l = l->next()) {
        l->data()->SetMountedFraction(1);
        l->data()->Arm();
    }
    shooter->SetAmmo(shooter->GetHullType()->GetMaxAmmo());
    shooter->SetEnergy(shooter->GetHullType()->GetMaxEnergy());
    m.tick(30);
    auto projectileCount = [](IclusterIGC *c) {
        int n = 0;
        for (auto *l = c->GetModels()->first(); l; l = l->next())
            n += l->data()->GetObjectType() == OT_projectile;
        return n;
    };
    m.ia.active = true;
    m.ia.fire = true;
    m.tick(12);
    m.ia.fire = false;
    m.tick(12);
    require(projectileCount(cluster) > 0, "authority creates individual gun projectiles");
    require(projectileCount(m.a->core()->GetCluster(cluster->GetObjectID())) > 0,
            "predicted gun projectiles survive confirmation");
    auto verifyEquipment=[&]{auto full=m.authority->networkFrame(shooter->GetObjectID(),m.server->tick(),0);referenceEquipment(*m.authority,full,shooter->GetSide());
        auto compact=m.authority->networkFrame(shooter->GetObjectID(),m.server->tick(),0,false,{},true);referenceEquipment(*m.authority,compact,shooter->GetSide());
        sameCapturePacket(full,compact,"equipment mutations change direct/deferred packet bytes");};
    verifyEquipment();
    TRef<IpartIGC> weapon=shooter->GetMountedPart(ET_Weapon,0);require(weapon,"equipment plan mutation weapon");
    Mount originalMount=weapon->GetMountID(),cargo=-c_maxCargo;float mounted=weapon->GetMountedFraction();
    TRef<IpartIGC> displaced=shooter->GetMountedPart(NA,cargo);float displacedMounted=displaced?displaced->GetMountedFraction():0;
    if(displaced){displaced->SetShip(nullptr,c_mountNA);verifyEquipment();}
    weapon->SetMountID(cargo);verifyEquipment();weapon->SetMountID(originalMount);weapon->SetMountedFraction(mounted);verifyEquipment();
    weapon->SetShip(nullptr,c_mountNA);verifyEquipment();weapon->SetShip(shooter,originalMount);weapon->SetMountedFraction(mounted);verifyEquipment();
    if(displaced){displaced->SetShip(shooter,cargo);displaced->SetMountedFraction(displacedMounted);verifyEquipment();}
    auto trajectories=m.authority->networkFrame(shooter->GetObjectID(),m.server->tick(),0);
    auto deferred=m.authority->networkFrame(shooter->GetObjectID(),m.server->tick(),0,false,{},true);
    require(!deferred.capturedProjectiles.empty() &&
            deferred.entities.size()+deferred.capturedProjectiles.size()==trajectories.entities.size(),
            "worker capture omits only projectile default fields");
    sameCapturePacket(deferred,trajectories,"compact worker capture reconstructs byte-identical records and entity order");
    CprojectileIGC* captured=nullptr;
    for(auto* l=cluster->GetModels()->first();l;l=l->next())if(l->data()->GetObjectType()==OT_projectile){captured=static_cast<CprojectileIGC*>(l->data());break;}
    require(captured,"capture mutation projectile");
    auto initial=std::find_if(trajectories.entities.begin(),trajectories.entities.end(),[&](auto& e){return e.identity.type==OT_projectile && e.shot==captured->NativeShot();});
    require(initial!=trajectories.entities.end(),"mutation projectile has a captured identity");auto identity=initial->identity.serial;
    auto recapture=[&]{auto frame=m.authority->networkFrame(shooter->GetObjectID(),m.server->tick(),0);
        auto found=std::find_if(frame.entities.begin(),frame.entities.end(),[&](auto& e){return e.identity.serial==identity;});
        require(found!=frame.entities.end(),"mutated projectile remains in capture");return *found;};
    auto velocity=captured->GetVelocity();captured->SetVelocity(velocity+Vector(1,2,3));
    auto changed=recapture();require(changed.motion.velocity.x==velocity.x+1 && changed.motion.velocity.y==velocity.y+2 && changed.motion.velocity.z==velocity.z+3,"capture invalidates changed projectile velocity");captured->SetVelocity(velocity);
    float signature=captured->GetSignature();captured->SetSignature(signature+2);require(recapture().signature==signature+2,"capture invalidates projectile signature setter");
    captured->ChangeSignature(1);require(recapture().signature==signature+3,"capture invalidates projectile signature delta");captured->SetSignature(signature);
    auto capturedExpiry=captured->NativeExpireTime();captured->NativeSetExpireTime(capturedExpiry+1.f);require(recapture().expiry==(capturedExpiry+1.f).clock(),"capture invalidates projectile expiry");captured->NativeSetExpireTime(capturedExpiry);
    auto shot=captured->NativeShot();captured->NativeSetShot(shot+1);require(recapture().shot==shot+1,"capture invalidates projectile shot token");captured->NativeSetShot(shot);
    auto* side=captured->GetSide();captured->SetSide(m.authority->core()->GetSide(1));require(recapture().side==1,"capture invalidates projectile side");captured->SetSide(side);
    float radius=captured->GetRadius();if(radius>0){captured->SetRadius(radius+1);require(recapture().radius==radius+1,"capture invalidates projectile radius");captured->SetRadius(radius);}
    auto originalEpoch=recapture().epoch;
    captured->SetSignature(signature+1);require(recapture().epoch==originalEpoch,"trajectory record refresh preserves projectile publication epoch");captured->SetSignature(signature);
    captured->SetCluster(nullptr);auto departed=m.authority->networkFrame(shooter->GetObjectID(),m.server->tick(),0);
    require(std::none_of(departed.entities.begin(),departed.entities.end(),[&](auto& e){return e.identity.serial==identity;}),"projectile outside viewed sector is omitted");
    captured->SetCluster(cluster);
    auto arrived=recapture();require(arrived.sector==cluster->GetObjectID() && arrived.epoch>originalEpoch,"capture invalidates projectile sector arrival");
    auto compact=m.authority->networkFrame(shooter->GetObjectID(),m.server->tick(),0,false,{},true);
    auto direct=m.authority->networkFrame(shooter->GetObjectID(),m.server->tick(),0);
    require(Net::packet(compact)==Net::packet(direct),"mutated dense captures retain byte-identical direct records");
    m.tick(6);
    auto moved=m.authority->networkFrame(shooter->GetObjectID(),m.server->tick(),0);
    size_t stable=0;
    for(auto& e:trajectories.entities)if(e.identity.type==OT_projectile) {
        auto next=std::find_if(moved.entities.begin(),moved.entities.end(),[&](auto& n){return n.identity.serial==e.identity.serial;});
        if(next==moved.entities.end())continue;
        require((next->flags&Net::Trajectory) && next->activation==e.activation && next->expiry==e.expiry &&
                std::memcmp(&next->motion,&e.motion,sizeof(e.motion))==0 && next->power==e.power,
                "unguided projectile trajectory stays byte-identical while it moves");
        Time publication=Time(1000u)+float(double(moved.header.tick)/Net::tickRate);
        auto pose=Net::trajectoryMotion(*next,double(publication.clock()-1000u)*.001);
        ImodelIGC* original=nullptr;
        // Original projectiles have no ObjectID; their native shot token is
        // the authority/prediction identity, independent of list order.
        for(auto* l=cluster->GetModels()->first();l;l=l->next())
            if(l->data()->GetObjectType()==OT_projectile && static_cast<CprojectileIGC*>(l->data())->NativeShot()==next->shot)
                original=l->data();
        require(original && (Vector(pose.position.x,pose.position.y,pose.position.z)-original->GetPosition()).Length()<.05f,
                "constant trajectory reproduces authority position at publication time");
        ++stable;
    }
    require(stable>0,"stable projectile trajectory fixture survives both publications");
    size_t staticSpins=0;
    for(const auto& original:trajectories.entities)if(original.flags&Net::Spin) {
        auto next=std::find_if(moved.entities.begin(),moved.entities.end(),[&](const auto& e){return e.identity==original.identity;});
        if(next==moved.entities.end())continue;
        require(next->activation==original.activation && next->power==original.power &&
                std::memcmp(&next->motion,&original.motion,sizeof(original.motion))==0,
                "static spin anchors remain invariant between reports");
        auto* model=core->GetModel(next->identity.type,next->identity.id);
        auto pose=Net::trajectoryMotion(*next,double(core->GetLastUpdate().clock()-1000u)*.001);
        auto vec=[](Net::Vec v){return Vector(v.x,v.y,v.z);};
        require(model && (model->GetOrientation().GetForward()-vec(pose.forward)).Length()<.001f &&
                (model->GetOrientation().GetUp()-vec(pose.up)).Length()<.001f,
                "static spin anchor reproduces original arbitrary-axis rotation");
        ++staticSpins;
    }
    require(staticSpins>0,"static spin fixture includes visible rocks or warps");
    Net::Entity spin{};spin.motion.forward={0,0,1};spin.motion.up={0,1,0};spin.power=.7f;
    auto spun=Net::trajectoryMotion(spin,1.25);
    Orientation orientation(Vector(0,0,1));orientation.PostRotate(Vector(0,0,1),.7f*1.25f);
    require((Vector(spun.up.x,spun.up.y,spun.up.z)-orientation.GetUp()).Length()<.0001f,"trajectory spin follows original IGC orientation convention");
    auto *magazine = static_cast<ImagazineIGC *>(shooter->GetMountedPart(ET_Magazine, 0));
    require(magazine, "installed missile magazine");
    magazine->SetAmount(20);
    magazine->SetLock(1);
    magazine->SetTimeLoaded(core->GetLastUpdate() - 1);
    m.tick(15);
    m.ia.missile = true;
    m.tick(12);
    m.ia.missile = false;
    m.tick(12);
    require(cluster->GetMissiles()->n() > 0, "authority creates original missiles");
    for (auto *l = cluster->GetMissiles()->first(); l; l = l->next()) {
        auto *missile = l->data();
        auto *replica = m.a->core()->GetModel(OT_missile, missile->GetObjectID());
        require(replica, "confirmed missile has authoritative targetable identity");
    }
    auto *counter = static_cast<IdispenserIGC *>(shooter->GetMountedPart(ET_ChaffLauncher, 0));
    require(counter, "countermeasure deadline fixture");
    DataChaffIGC chaff{};
    chaff.pchafftype = static_cast<IchaffTypeIGC *>(counter->GetExpendableType());
    chaff.p0 = shooter->GetPosition();
    chaff.v0 = Vector::GetZero();
    chaff.pcluster = cluster;
    chaff.time0 = core->GetLastUpdate() - std::min(.5f, chaff.pchafftype->GetLifespan() * .2f);
    auto *aged = static_cast<CchaffIGC *>(core->CreateObject(core->GetLastUpdate(), OT_chaff, &chaff, sizeof(chaff)));
    require(aged, "already-aged countermeasure creation");
    aged->Update(core->GetLastUpdate());
    const auto expiry = aged->NativeExpireTime();
    const auto chaffRadius = aged->GetRadius();
    aged->Release();
    auto deadlines = m.authority->networkFrame(shooter->GetObjectID(), m.server->tick(), 0);
    {
        Game atPublication(art, m.loader, 0, 0, m.options, {}, GameRole::Replica);
        atPublication.applyNetworkFrame(deadlines);
        size_t gunDeadlines = 0, missileDeadlines = 0, chaffDeadlines = 0;
        for (auto *l = atPublication.core()->GetCluster(cluster->GetObjectID())->GetModels()->first(); l; l = l->next()) {
            auto *model = l->data();
            if (model->GetObjectType() == OT_projectile) {
                auto *p = static_cast<CprojectileIGC *>(model);
                auto e = std::find_if(deadlines.entities.begin(), deadlines.entities.end(), [&](const auto &e) {
                    return e.identity.type == OT_projectile && e.shot == p->NativeShot();
                });
                require(e != deadlines.entities.end() && p->NativeExpireTime().clock() == e->expiry,
                        "gun expiry is restored exactly from the original millisecond clock");
                ++gunDeadlines;
            } else if (model->GetObjectType() == OT_missile) {
                auto *p = static_cast<CmissileIGC *>(model);
                auto e = std::find_if(deadlines.entities.begin(), deadlines.entities.end(), [&](const auto &e) {
                    return e.identity.type == OT_missile && e.identity.id == p->GetObjectID();
                });
                require(e != deadlines.entities.end() && p->NativeExpireTime().clock() == e->expiry &&
                            p->NativeActivateTime().clock() == e->activation,
                        "missile expiry/arming restore exact stored deadlines");
                ++missileDeadlines;
            } else if (model->GetObjectType() == OT_chaff) {
                require(static_cast<CchaffIGC *>(model)->NativeExpireTime() == expiry,
                        "late-created replica chaff keeps its authority expiry rather than restarting its lifetime");
                require(model->GetRadius() == chaffRadius,
                        "already-aged countermeasure retains its authoritative growth radius");
                ++chaffDeadlines;
            }
        }
        require(gunDeadlines && missileDeadlines && chaffDeadlines, "all expendable deadline cases are exercised");
        for (auto &p : deadlines.parts) {
            auto *owner = atPublication.core()->GetShip(p.ship.id);
            auto type = atPublication.core()->GetPartType(p.id)->GetEquipmentType();
            auto *part = owner->GetMountedPart(p.mount < 0 ? NA : type, p.mount);
            if (type == ET_Weapon)
                require(static_cast<CweaponIGC *>(part)->NativeNextFire().clock() == p.timeFired,
                        "weapon cooldown restores the exact native clock");
            if (type == ET_Magazine || type == ET_Dispenser || type == ET_ChaffLauncher)
                require(static_cast<IlauncherIGC *>(part)->GetTimeLoaded().clock() == p.timeLoaded,
                        "launcher readiness restores the exact native clock");
        }
    }
    m.tick(3);
    auto laterDeadlines = m.authority->networkFrame(shooter->GetObjectID(), m.server->tick(), 0);
    for (auto &e : deadlines.entities)
        if (e.expiry)
            if (auto later = std::find_if(laterDeadlines.entities.begin(), laterDeadlines.entities.end(),
                                         [&](auto &p) { return p.identity == e.identity; });
                later != laterDeadlines.entities.end())
                require(later->expiry == e.expiry && later->activation == e.activation,
                        "passage of time creates no artificial expiry/arming deltas");
    for (int i = 0; i < 65536; ++i) {
        auto candidate = core->GenerateNewMissileID();
        require(candidate >= 0 && !core->GetModel(OT_missile, candidate),
                "missile ID wrap keeps authority namespace and live identities");
    }
    int enemy = m.authority->addLocalPilot("Hidden launcher", 1);
    require(enemy >= 0, "hidden missile launcher fixture");
    auto *hidden = core->GetShip(enemy);
    hidden->SetStation(nullptr);
    hidden->SetCluster(core->GetSide(1)->GetStations()->first()->data()->GetCluster());
    hidden->SetPosition(Vector(3000, 3000, 0));
    hidden->SetAutopilot(false);
    DataMissileIGC d{};
    d.pmissiletype = magazine->GetMissileType();
    d.pLauncher = hidden;
    d.pTarget = shooter;
    d.pCluster = cluster;
    d.position = shooter->GetPosition() + Vector(0, 0, 300);
    d.forward = Vector(1, 0, 0);
    d.velocity = Vector::GetZero(); // Vector's default constructor leaves its components unset.
    d.missileID = core->GenerateNewMissileID();
    d.lock = 1;
    auto *missile = static_cast<ImissileIGC *>(core->CreateObject(core->GetLastUpdate(), OT_missile, &d, sizeof(d)));
    require(missile, "hidden-launcher missile creation");
    missile->Release();
    auto id = d.missileID;
    m.wait([&] { return m.a->core()->GetModel(OT_missile, id) != nullptr; },
           "visible missile replicated without leaking hidden launcher", 120);
    require(!m.a->core()->GetShip(enemy), "missile visibility does not reveal launcher pose");
    m.report("Projectiles: predicted guns, missile confirmation/identity, hidden launcher");
}
// Compare steady windows, excluding bootstrap/launch/effect-drain bursts. Use
// real 60 Hz authority/input timing and identical game data at both cadences.
void cadence(const std::filesystem::path &art) {
    const auto hz = publicationRate;
    {
        Match m(art, 27642, true, false, false, Scenario::FreeFlight);
        m.join();
        auto measure = [&](const char *phase, int ticks) {
            const auto received = m.ca->metrics().bytesReceived, sent = m.ca->metrics().bytesSent,
                       publications = m.ca->metrics().snapshots;
            const size_t begin = m.times.size();
            m.tick(ticks);
            const double seconds = double(ticks) / Net::tickRate;
            std::vector<double> samples(m.times.begin() + begin, m.times.end());
            const double mean = std::accumulate(samples.begin(), samples.end(), 0.) / samples.size();
            std::sort(samples.begin(), samples.end());
            const auto count = m.ca->metrics().snapshots - publications;
            require(count >= hz * seconds - 2 && count <= hz * seconds + 2,
                    "steady publication count matches the declared cadence");
            printf("Cadence %u Hz %s: downstream %.3f KiB/s, upstream %.3f KiB/s, "
                   "%llu snapshots over %.1f s, server mean/p99 %.3f/%.3f ms\n",
                   hz, phase, (m.ca->metrics().bytesReceived - received) / (1024. * seconds),
                   (m.ca->metrics().bytesSent - sent) / (1024. * seconds),
                   (unsigned long long)count, seconds, mean, samples[size_t(.99 * (samples.size() - 1))]);
            fflush(stdout);
        };
        measure("docked sector", 180);
        m.a->launch();
        m.b->launch();
        m.tick(420);
        auto *core = m.authority->core();
        for (auto *client : {m.a.get(), m.b.get()}) {
            auto *ship = core->GetShip(client->state().shipId);
            require(ship->GetCluster(), "cadence pilots launch");
            ship->SetAutopilot(false);
            ship->SetStateBits(coastButtonIGC, coastButtonIGC);
            ship->SetPosition(Vector(5000, client == m.a.get() ? 5000 : 5500, 5000));
            ship->SetVelocity(Vector(100, 0, 0));
        }
        m.ia.active = m.ib.active = true;
        m.ia.fieldOfView = m.ib.fieldOfView = -1;
        m.tick(120);
        measure("two cruising pilots", 300);
        m.ia.fire = m.ib.fire = true;
        m.tick(60);
        bool projectiles = false;
        for (auto *l = m.authority->playerShip()->GetCluster()->GetModels()->first(); l; l = l->next())
            projectiles |= l->data()->GetObjectType() == OT_projectile;
        require(projectiles, "cadence firing phase produces original projectiles");
        measure("two firing pilots", 180);
        m.ia.fire = m.ib.fire = false;
        m.tick(120);
        auto *pilot = core->GetShip(m.a->state().shipId);
        for (int i = 0; i < 100; ++i) {
            DataShipIGC data{};
            data.shipID = core->GenerateNewShipID();
            data.hullID = pilot->GetBaseHullType()->GetObjectID();
            data.sideID = 0;
            data.pilotType = c_ptWingman;
            data.baseObjectID = NA;
            data.partsOffset = sizeof(data);
            snprintf(data.name, sizeof(data.name), "Cadence %d", i);
            auto *ship = static_cast<IshipIGC *>(core->CreateObject(core->GetLastUpdate(), OT_ship, &data, sizeof(data)));
            require(ship, "cadence fleet creation");
            ship->SetCluster(pilot->GetCluster());
            ship->SetPosition(Vector(7000 + (i % 10) * 100, 5000 + (i / 10) * 100, 6000));
            ship->SetAutopilot(false);
            ship->SetStateBits(coastButtonIGC, coastButtonIGC);
            ship->SetVelocity(Vector(0, 0, 10));
            ship->Release();
        }
        m.tick(120);
        require(m.a->core()->GetShips()->n() >= 102, "cadence fleet reaches the client before measurement");
        measure("100 extra coasting ships", 300);
    }
}
void fleet(const std::filesystem::path &art) {
    Match m(art, 27636, true);
    m.join();
    auto *core = m.authority->core();
    auto *player = core->GetShip(m.a->state().shipId);
    auto *station = player->GetStation();
    constexpr int count = 1500;
    for (int i = 0; i < count; ++i) {
        DataShipIGC d{};
        d.shipID = core->GenerateNewShipID();
        d.hullID = player->GetBaseHullType()->GetObjectID();
        d.sideID = 0;
        d.pilotType = c_ptWingman;
        d.baseObjectID = NA;
        d.partsOffset = sizeof(d);
        snprintf(d.name, sizeof(d.name), "Fleet %d", i);
        auto *ship = static_cast<IshipIGC *>(core->CreateObject(core->GetLastUpdate(), OT_ship, &d, sizeof(d)));
        require(ship, "large fleet fixture");
        ship->SetCluster(station->GetCluster());
        ship->SetPosition(station->GetPosition() + Vector((i % 40) * 100 - 2000, (i / 40) * 100 - 2000, 2500));
        ship->SetAutopilot(false);
        ship->SetVelocity(Vector(0, 0, 10));
        ship->Release();
    }
    auto frame = m.authority->networkFrame(player->GetObjectID(), m.server->tick(), 0);
    auto bytes = Net::packet(frame);
    require(bytes.size() > Net::maxPacket, "fleet exceeds one message budget");
    Net::impairment(5, 20, 3);
    m.wait([&] { return m.a->core()->GetShips()->n() >= count + 2 && m.b->core()->GetShips()->n() >= count + 2; },
           "fragmented large fleet bootstrap survives loss", 600);
    auto before = m.ca->metrics().snapshots;
    m.tick(180);
    Net::impairment(0, 0, 0);
    require(m.ca->metrics().snapshots > before + 10, "large fleet replication continues under packet loss");
    printf("Fleet raw state: %.1f KiB for %d ships\n", bytes.size() / 1024., count);
    m.report("Fleet: 1500 visible ships, 5% loss, 20 ms each direction");
}
void peers(const std::filesystem::path &art) {
    Match m(art, 27637, true);
    m.join();
    constexpr int count = 100;
    std::vector<std::unique_ptr<Game>> games(count - 2);
    std::vector<std::unique_ptr<Net::Client>> clients;
    clients.reserve(count - 2);
    for (int i = 0; i < count - 2; ++i) {
        auto *slot = &games[i];
        auto name = "Peer " + std::to_string(i + 2);
        clients.push_back(std::make_unique<Net::Client>(
            "127.0.0.1:" + std::to_string(m.port), m.digest, name, 0, [&m, slot](const SessionOptions &s) -> Game & {
                *slot = std::make_unique<Game>(m.art, m.loader, 0, 0, s, EffectLoader{}, GameRole::Replica);
                (*slot)->quickChats(m.quickChat);
                return **slot;
            }));
    }
    FlightInput neutral;
    neutral.active = false;
    auto ready = [&] {
        return m.server->clients() == count &&
               std::all_of(clients.begin(), clients.end(), [](auto &c) { return c->ready(); });
    };
    auto cycle = [&] {
        for (auto &c : clients)
            c->step(neutral, 1. / Net::tickRate);
        m.tick(1);
        for (auto &c : clients) {
            c->poll();
            require(!c->closed(), c->error().c_str());
        }
    };
    for (int i = 0; i < 600 && !ready(); ++i)
        cycle();
    require(ready(), "100 real GNS participants acquire independent replicas");
    auto first = m.ca->metrics().snapshots;
    auto last = clients.back()->metrics().snapshots;
    m.times.clear();
    m.deadline = std::chrono::steady_clock::now();
    for (int i = 0; i < 180; ++i)
        cycle();
    require(m.ca->metrics().snapshots > first + 20 && clients.back()->metrics().snapshots > last + 20,
            "publication reaches first and last of 100 peers");
    require(std::all_of(clients.begin(), clients.end(), [](auto &c) { return c->metrics().inputDrops == 0; }),
            "100-peer input histories remain bounded");
    m.report("Peers: 100 authenticated connections and independent IGC replicas");
}
void traffic(const std::filesystem::path &art) {
    // Replay exactly the former positional XOR/LZ4 on the same full records.
    auto positional = [](std::span<const std::byte> current, std::span<const std::byte> old) {
        std::vector<char> difference(current.size()), out(LZ4_compressBound(current.size()));
        for (size_t i = 0; i < current.size(); ++i)
            difference[i] = char(uint8_t(current[i]) ^ (i < old.size() ? uint8_t(old[i]) : 0));
        int n = LZ4_compress_default(difference.data(), out.data(), difference.size(), out.size());
        require(n > 0, "positional delta fixture compression");
        return size_t(n) + sizeof(Net::Header) + 16; // Former Delta has two uint64_t fields.
    };
    auto scene = [](std::span<const int> order, uint64_t sequence) {
        Net::Frame f;
        f.header.kind = Net::Kind::Snapshot;
        f.header.sequence = sequence;
        f.header.tick = sequence * 3;
        for (int id : order) {
            Net::Entity e;
            e.identity = {uint32_t(id + 1), OT_ship, int16_t(id)};
            e.radius = 1;
            e.motion.position = {float(id * 3), float(id), -100};
            e.payloadBytes = 80;
            e.partCount = 4;
            f.entities.push_back(e);
            uint32_t random = id + 1;
            for (int b = 0; b < 80; ++b) {
                random ^= random << 13;
                random ^= random >> 17;
                random ^= random << 5;
                f.payload.push_back(std::byte(random));
            }
            for (int slot = 0; slot < 4; ++slot) {
                Net::Part p;
                p.ship = e.identity;
                p.id = slot;
                p.mount = slot;
                p.amount = id % 100;
                p.mounted = 1;
                f.parts.push_back(p);
            }
        }
        return Net::packet(f);
    };
    std::vector<int> order(1024);
    std::iota(order.begin(), order.end(), 0);
    auto base = scene(order, 1);
    auto initial = Net::compressed(base, 0, {});
    require(Net::expanded(initial, {}) == base, "cold bootstrap compression is byte-exact");
    std::map<uint64_t, std::vector<std::byte>> history{{1, base}};
    auto stable = scene(order, 2);
    auto bytes = Net::compressed(stable, 1, base);
    require(Net::expanded(bytes, history) == stable, "unchanged objects preserve exact records");
    printf("Stable 1,024-object scene: raw %zu, positional %zu, aligned %zu bytes\n",
           stable.size(), positional(stable, base), bytes.size());
    order.erase(order.begin() + 1);
    std::rotate(order.begin(), order.begin() + 400, order.end());
    order.insert(order.begin() + 10, 1100);
    auto changed = scene(order, 3);
    auto delta = Net::compressed(changed, 1, base);
    auto restored = Net::expanded(delta, history);
    require(restored == changed, "insertion/removal/reordering preserves every authority bit");
    Net::frame(restored);
    require(delta.size() < positional(changed, base) / 4, "object identity prevents collateral churn deltas");
    require(Net::expanded(delta, {}).empty(), "unknown acknowledged basis is never guessed");
    printf("Object insertion/removal/reordering: positional %zu, aligned %zu bytes (%.1f%% reduction)\n",
           positional(changed, base), delta.size(), 100 * (1 - double(delta.size()) / positional(changed, base)));
    auto later = scene(order, 4);
    // Skipping sequence 2/3 is harmless: each delta refers to an acknowledged full state.
    require(Net::expanded(Net::compressed(later, 1, base), history) == later,
            "dropped/reordered intervening publications have no chained dependency");
    auto metadata = Net::frame(changed);
    metadata.header.sequence = 5;
    metadata.pilots.push_back({});
    metadata.pilots.back().id = 4;metadata.pilots.back().side=0;
    Net::text(metadata.pilots.back().name, "A changing roster");
    metadata.teams.push_back({0, 4, 1234, 1});
    metadata.buckets.push_back({0, 2, 100, 4});
    metadata.notices.push_back({});
    metadata.notices.back().number = 12;
    Net::text(metadata.notices.back().text, "A changing notice");
    metadata.events.push_back({});
    metadata.events.back().number = 13;
    // Exercise an unaligned payload tail and changing lengths, then remove
    // metadata so every following byte changes its positional delta location.
    metadata.entities[0].payloadBytes += 3;
    metadata.payload.insert(metadata.payload.begin() + 80, 3, std::byte(0x7f));
    auto extended = Net::packet(metadata);
    require(Net::expanded(Net::compressed(extended, 1, base), history) == extended,
            "changed roster/economy/effects and variable payload lengths are byte-exact");
    history[5] = extended;
    require(Net::expanded(Net::compressed(later, 5, extended), history) == later,
            "metadata removal and unaligned sparse XOR tails are byte-exact");
    Net::Action action;
    action.op = Net::Operation::Target;
    action.args = {OT_ship, 123, 0, 0};
    auto packedRecord = [&](Net::Kind kind, const auto &value) {
        std::vector<std::byte> raw;
        Net::append(raw, Net::Header{Net::magic, kind, 123, 45, 6});
        Net::append(raw, value);
        auto packed = Net::compactMessage(raw);
        Net::Reader r(packed);
        auto h = r.take<Net::Header>();
        std::vector<std::byte> unpacked;
        Net::unpackMessage(h, r, unpacked);
        require(h.kind == kind && h.tick == 123 && h.sequence == 45 && h.ack == 6 &&
                    r.rest().size() == sizeof(value) &&
                    !std::memcmp(r.rest().data(), &value, sizeof(value)),
                "compressed complete records retain header/sequence/ack and every native byte");
        return std::pair(raw.size(), packed.size());
    };
    auto actionSizes = packedRecord(Net::Kind::Action, action);
    require(actionSizes.second < actionSizes.first / 2, "target action does not send unused text/loadout padding");
    Net::Configuration config;
    packedRecord(Net::Kind::Configure, config);
    Net::Welcome welcome;
    packedRecord(Net::Kind::Welcome, welcome);
    std::array<Net::Input, 3> inputs;
    for (int i = 0; i < 3; ++i) {
        inputs[i].number = 40 + i;
        inputs[i].tick = 100 + i;
        inputs[i].throttle = Net::compactThrottle(.6f);
        inputs[i].buttons = Net::Active | Net::Fire;
    }
    std::vector<std::byte> inputBytes;
    Net::append(inputBytes, Net::InputHeader{Net::magic, Net::Kind::Input, 7, 6});
    Net::append(inputBytes, std::span(inputs));
    Net::Reader inputReader(inputBytes);
    auto inputHeader = inputReader.take<Net::InputHeader>();
    require(inputHeader.match == 7 && inputHeader.ack == 6 && inputBytes.size() == 64,
            "three canonical loss-recovery samples share one compact match/receipt header");
    for (const auto &sample : inputs) {
        auto restored = inputReader.take<Net::Input>();
        require(!std::memcmp(&sample, &restored, sizeof(sample)), "compact input queues retain every canonical byte");
    }
    require(inputReader.rest().empty(), "input bundle has no unused tail");
    printf("Target action %zu -> %zu bytes; three-input loss bundle 200 raw / 113 formerly packed -> %zu bytes\n",
           actionSizes.first, actionSizes.second, inputBytes.size());
    {
        Net::Reader r(delta);
        auto h = r.take<Net::Header>();
        auto d = r.take<Net::Delta>();
        size_t size = (d.patchBytes ? d.patchBytes : d.size) + d.copies * sizeof(Net::CopyRange);
        std::vector<std::byte> body(size);
        require(d.copies && LZ4_decompress_safe(reinterpret_cast<const char *>(r.rest().data()),
                                               reinterpret_cast<char *>(body.data()), r.rest().size(), size) == int(size),
                "relocation bounds fixture");
        Net::CopyRange invalid{uint32_t(base.size()), 0, 1};
        std::memcpy(body.data(), &invalid, sizeof(invalid));
        std::vector<std::byte> bad;
        Net::append(bad, h);
        Net::append(bad, d);
        size_t begin = bad.size();
        bad.resize(begin + LZ4_compressBound(size));
        int n = LZ4_compress_default(reinterpret_cast<const char *>(body.data()),
                                    reinterpret_cast<char *>(bad.data() + begin), size, bad.size() - begin);
        bad.resize(begin + n);
        bool rejected = false;
        try { Net::expanded(bad, history); } catch (const std::runtime_error &) { rejected = true; }
        require(rejected, "invalid relocation cannot read outside its acknowledged baseline");
    }
    Match m(art, 27641, true, true);
    m.join();
    m.tick(60);
    auto before = m.ca->metrics().snapshots, received = m.ca->metrics().bytesReceived;
    m.tick(120);
    auto publications = m.ca->metrics().snapshots - before;
    require(publications >= 2 && publications <= 6, "unchanged lobby uses a bounded heartbeat instead of 20 Hz duplicates");
    printf("Idle lobby over two seconds: %llu publications, %llu received application bytes\n",
           (unsigned long long)publications, (unsigned long long)(m.ca->metrics().bytesReceived - received));
    Net::Action ready;
    ready.op = Net::Operation::Ready;
    ready.args[0] = 1;
    require(m.ca->action(ready), "lobby readiness change queued");
    m.wait([&] { return m.a->state().networkReady; }, "changed lobby state bypasses heartbeat suppression", 12);
    require(m.a->sendChat(ChatChannel::Team, "Acknowledged traffic audit notice"), "notice compression fixture");
    m.wait([&] {
        return std::any_of(m.b->state().messages.begin(), m.b->state().messages.end(), [](const auto &n) {
            return n.text.find("Acknowledged traffic audit notice") != std::string::npos;
        });
    }, "notice arrives before it can be suppressed");
    m.tick(60);
    require(m.server->metrics().noticesOmitted && m.server->metrics().eventsOmitted,
            "acknowledged notices and notification sounds leave subsequent publications");
    auto count = [](const Game &game) {
        return std::count_if(game.state().messages.begin(), game.state().messages.end(), [](const auto &n) {
            return n.text.find("Acknowledged traffic audit notice") != std::string::npos;
        });
    };
    require(count(*m.b) == 1, "acknowledged notice is applied once");
    m.cb->reconnect();
    m.wait([&] { return m.cb->ready(); }, "traffic basis reset reconnects reliably");
    require(count(*m.b) == 1, "reconnect keeps already delivered notice watermarks");
    m.report("Traffic: lossless records/deltas, idle lobby, acknowledged effects and reconnect");
}
// Match the real frontend: poll before step, render more frequently than either
// prediction or publication, and leave presentation poses for the next poll to
// restore. Tick-rate-only tests cannot see a cached or stepped camera.
void drones(const std::filesystem::path &art) {
    Match m(art,27646);m.join();auto *core=m.authority->core();
    auto *pilot=core->GetShip(m.a->state().shipId);auto *home=pilot->GetStation();auto *side=pilot->GetSide();
    IclusterIGC *cluster=nullptr;
    for(auto*l=core->GetClusters()->first();l;l=l->next())if(!l->data()->GetHomeSector()){cluster=l->data();break;}
    require(cluster,"neutral mining sector");
    DataStationIGC d{};d.stationID=core->GenerateNewStationID();d.stationTypeID=home->GetBaseStationType()->GetObjectID();
    d.sideID=side->GetObjectID();d.clusterID=cluster->GetObjectID();d.position=Vector(1800,0,0);
    d.forward=Vector(0,0,1);d.up=Vector(0,1,0);d.bpHull=d.bpShield=1;Net::text(d.name,"Local mining garrison");
    auto *local=static_cast<IstationIGC *>(core->CreateObject(core->GetLastUpdate(),OT_station,&d,sizeof(d)));
    require(local,"local garrison creates");local->Release();
    IdroneTypeIGC *type=nullptr;
    for(auto*l=core->GetDroneTypes()->first();l;l=l->next())if(l->data()->GetPilotType()==c_ptMiner && home->CanBuy(l->data())){type=l->data();break;}
    require(type,"authored miner type");
    std::vector<IasteroidIGC *> rocks;
    for(auto*l=cluster->GetAsteroids()->first();l && rocks.size()<2;l=l->next())if(l->data()->HasCapability(c_aabmMineHe3))rocks.push_back(l->data());
    require(rocks.size()==2,"two He3 rocks");
    for(size_t i=0;i<rocks.size();++i){rocks[i]->SetPosition(Vector(0,float(i)*1800,0));rocks[i]->SetOre(5000);}
    for(int i=0;i<4;++i)core->GetIgcSite()->DroneTypeCompleted(nullptr,home,type,core->GetLastUpdate());
    std::vector<IshipIGC *> miners;
    for(auto*l=side->GetShips()->first();l;l=l->next())if(l->data()->GetPilotType()==c_ptMiner)miners.push_back(l->data());
    require(miners.size()==4,"four real miners");
    for(size_t i=0;i<miners.size();++i) {
        auto*s=miners[i];auto*r=rocks[i/2];s->SetCluster(cluster);
        s->SetPosition(r->GetPosition()+Vector((i%2?-1.f:1.f)*(r->GetRadius()+s->GetRadius()+5),0,0));
        s->SetVelocity(Vector::GetZero());s->SetCommand(c_cmdAccepted,r,c_cidMine);
    }
    m.tick(30);
    auto row=[&](Game &game)->const ResearchState & {
        for(auto&r:game.state().research)if(r.type==OT_droneType && r.name==type->GetName())return r;
        throw std::runtime_error("miner production row");
    };
    require(row(*m.a).count==4 && !row(*m.a).available && row(*m.b).count==4,"both clients show authoritative production cap");
    auto rejected=m.ca->metrics().rejected;int money=m.authority->pilotBalance(pilot->GetObjectID());
    require(m.a->invest(row(*m.a).id),"over-cap funding request sent through authenticated action path");
    m.wait([&]{return m.ca->metrics().rejected>rejected;},"over-cap funding is rejected by authority");
    require(m.authority->pilotBalance(pilot->GetObjectID())==money,"rejected over-cap funding does not spend credits");
    m.a->launch();m.tick(420);m.a->commandView(true);
    require(m.a->viewSector(cluster->GetObjectID()),"F2 camera selects remote mining sector");
    m.tick(30);CameraInput cameraInput;cameraInput.panRight=.15f;
    auto before=m.a->camera(cameraInput).position;
    for(int i=0;i<180;++i){m.tick(1);m.a->camera(cameraInput);}
    require(glm::length(m.a->camera(cameraInput).position-before)>1000 && m.a->state().sectorId==cluster->GetObjectID(),"pilot-seat command panning and viewed sector survive live UDP snapshots");
    cameraInput.panRight=0;
    std::array<int,4> deliveries{},miningTicks{};std::array<float,4> ore{};
    for(size_t i=0;i<miners.size();++i)ore[i]=miners[i]->GetOre();
    for(int tick=0;tick<24000;++tick) {
        m.tick(1);
        for(size_t i=0;i<miners.size();++i) {
            auto*s=miners[i];miningTicks[i]+=(s->GetStateM()&miningMaskIGC)!=0;
            if(ore[i]>0 && s->GetOre()==0)++deliveries[i];ore[i]=s->GetOre();
            if(s->GetOre()>0 && s->GetCommandID(c_cmdPlan)==c_cidGoto)
                require(s->GetCommandTarget(c_cmdPlan)==local,"contending miner chooses same-sector unload garrison");
            if(tick%600==0) {
                auto*copy=m.a->core()->GetShip(s->GetObjectID());
                require(copy && std::abs(copy->GetOre()-s->GetOre())<2,"remote miner ore continues to replicate during long mining cycles");
            }
        }
    }
    for(size_t i=0;i<miners.size();++i) {
        printf("UDP miner %d: %d deliveries, %d mining ticks\n",miners[i]->GetObjectID(),deliveries[i],miningTicks[i]);
        require(deliveries[i]>=1 && miningTicks[i]>1000,"every contending miner continues productive original AI duties");
    }
    auto *miner=miners.front();
    require(m.a->droneStandby(miner->GetObjectID(),true),"remote standby queues");
    m.wait([&]{return miner->GetStation()==local && miner->GetStayDocked();},"remote miner returns and stays docked",12000);
    m.tick(900);require(miner->GetStation()==local && m.a->core()->GetShip(miner->GetObjectID())->GetStayDocked(),"standby survives original automatic relaunch interval and replication");
    require(m.a->droneStandby(miner->GetObjectID(),false),"remote resume queues");
    m.wait([&]{return (miner->GetStateM()&miningMaskIGC) && !miner->GetStayDocked();},"remote resume relaunches and mines",12000);
    m.ca->reconnect();m.wait([&]{return m.ca->ready();},"mining match reconnect");
    require(row(*m.a).count==4,"reconnect restores drone identities and count");
    m.report("Drones: F2 pan, four miners/two rocks, local unloading, production cap and docked standby/resume");
}
void waypoints(const std::filesystem::path &art) {
    Match m(art, 27645);
    m.join();
    auto *core = m.authority->core();
    auto *pilot = core->GetShip(m.a->state().shipId);
    auto *cluster = pilot->GetStation()->GetCluster();
    Game replica(art, m.loader, 0, 0, m.options, EffectLoader{}, GameRole::Replica);
    DataBuoyIGC data{};
    data.buoyID = core->GenerateNewBuoyID();
    data.clusterID = cluster->GetObjectID();
    data.type = c_buoyWaypoint;
    data.visible = true;
    data.position = Vector(4500, 3000, 1500);
    auto *created = static_cast<IbuoyIGC *>(core->CreateObject(core->GetLastUpdate(), OT_buoy, &data, sizeof(data)));
    require(created && created->GetObjectID() == 0, "first waypoint supplies the matching identity in the crash fixture");
    TRef<IbuoyIGC> waypoint = created;
    created->Release();
    pilot->SetAutopilot(false);
    pilot->SetCommand(c_cmdCurrent, waypoint, c_cidGoto);
    replica.applyNetworkFrame(m.authority->networkFrame(pilot->GetObjectID(), m.server->tick(), 0));
    auto *copy = replica.core()->GetBuoy(data.buoyID);
    require(copy && replica.playerShip()->GetCommandTarget(c_cmdCurrent) == copy, "replica retains the live waypoint command");
    TRef<IbuoyIGC> retired = copy;
    pilot->SetCommand(c_cmdCurrent, nullptr, c_cidNone);
    require(!waypoint->GetMission(), "original authority ends the waypoint when its final consumer releases it");
    replica.applyNetworkFrame(m.authority->networkFrame(pilot->GetObjectID(), m.server->tick() + 1, 0));
    require(!retired->GetMission() && !replica.playerShip()->GetCommandTarget(c_cmdCurrent),
            "replica retires a waypoint without terminating it a second time when replacing its command");
    auto *secondPilot = core->GetShip(m.b->state().shipId);
    auto *remote = core->GetClusters()->last()->data();
    require(remote != cluster, "remote waypoint sector fixture");
    auto createBuoy = [&](IclusterIGC *sector, bool visible, BuoyID id = NA) {
        DataBuoyIGC record{};
        record.buoyID = id == NA ? core->GenerateNewBuoyID() : id;
        record.clusterID = sector->GetObjectID();
        record.position = Vector(4200, 3100, 1700);
        record.type = c_buoyWaypoint;
        record.visible = visible;
        auto *b = static_cast<IbuoyIGC *>(core->CreateObject(core->GetLastUpdate(), OT_buoy, &record, sizeof(record)));
        require(b, "waypoint ownership fixture creation");
        TRef<IbuoyIGC> result = b;
        b->Release();
        return result;
    };
    auto a = createBuoy(cluster, true);
    auto b = createBuoy(remote, false);
    auto marker = createBuoy(cluster, true);
    marker->AddConsumer(); // Authority lifetime owned outside any replicated ship's commands.
    for (Command c = 0; c < c_cmdMax; ++c)
        pilot->SetCommand(c, a, c_cidGoto);
    secondPilot->SetCommand(c_cmdPlan, b, c_cidGoto);
    auto commandsAgree = [&](Game &client) {
        for (auto *l = core->GetShips()->first(); l; l = l->next()) {
            auto *source = l->data();
            auto *destination = client.core()->GetShip(source->GetObjectID());
            if (!destination || source->GetSide() != pilot->GetSide())
                continue;
            for (Command c = 0; c < c_cmdMax; ++c) {
                auto *expected = source->GetCommandTarget(c);
                auto *actual = destination->GetCommandTarget(c);
                require(destination->GetCommandID(c) == source->GetCommandID(c) &&
                            bool(actual) == bool(expected) && (!actual ||
                            (actual->GetMission() && actual->GetObjectType() == expected->GetObjectType() &&
                             actual->GetObjectID() == expected->GetObjectID())),
                        "all four replicated command slots resolve exactly to live authority targets");
            }
        }
    };
    auto apply = [&] {
        auto frame = m.authority->networkFrame(pilot->GetObjectID(), m.server->tick(), 0);
        replica.applyNetworkFrame(frame);
        commandsAgree(replica);
    };
    apply();
    TRef<IbuoyIGC> copyA = replica.core()->GetBuoy(a->GetObjectID());
    TRef<IbuoyIGC> copyB = replica.core()->GetBuoy(b->GetObjectID());
    require(copyA && copyB && copyA->GetVisibleF() && !copyB->GetVisibleF() && copyB->GetCluster()->GetObjectID() == remote->GetObjectID(),
            "nonzero buoy IDs and public/private visibility survive import, including off-screen plan targets");
    auto *copyMarker = replica.core()->GetBuoy(marker->GetObjectID());
    require(copyMarker && copyMarker->GetMission(), "replicated marker lives independently of ship consumers");
    for (int i = 0; i < 12; ++i) {
        apply();
        require(replica.core()->GetBuoy(a->GetObjectID()) == copyA && replica.core()->GetBuoy(b->GetObjectID()) == copyB &&
                    replica.core()->GetBuoy(marker->GetObjectID()) == copyMarker && copyMarker->GetMission(),
                "unchanged snapshots preserve buoy identities and lifetime");
    }
    a->AddConsumer(); b->AddConsumer(); // Original IGC command transaction owns the two endpoints during handoff.
    for (Command c = 0; c < c_cmdMax; ++c)
        pilot->SetCommand(c, b, c_cidGoto);
    secondPilot->SetCommand(c_cmdPlan, a, c_cidGoto);
    a->ReleaseConsumer(); b->ReleaseConsumer();
    apply();
    require(copyA->GetMission() && copyB->GetMission(), "same-publication command handoff preserves both endpoints");
    secondPilot->SetCommand(c_cmdPlan, nullptr, c_cidNone);
    apply();
    require(!copyA->GetMission() && !replica.core()->GetBuoy(a->GetObjectID()), "plan-only buoy retires exactly once");
    auto reused = createBuoy(cluster, true, a->GetObjectID());
    pilot->SetCommand(c_cmdCurrent, reused, c_cidGoto);
    apply();
    require(replica.core()->GetBuoy(reused->GetObjectID()) != copyA && !copyA->GetMission(),
            "buoy ID reuse creates a new replicated lifetime while old references remain detached");
    for (Command c = 0; c < c_cmdMax; ++c)
        pilot->SetCommand(c, nullptr, c_cidNone);
    marker->ReleaseConsumer();
    apply();
    require(!copyB->GetMission() && replica.core()->GetBuoys()->n() == 0,
            "all buoy ownership is released when authority markers and command slots retire");
    // Now exercise the reported surrounding state over actual UDP: four real
    // miners competing for two He3 asteroids while the pilot navigates to an
    // advanced Supremacy station, with repeated ephemeral navigation targets.
    auto *home = pilot->GetStation();
    IdroneTypeIGC *minerType = nullptr;
    for (auto *l = core->GetDroneTypes()->first(); l; l = l->next())
        if (l->data()->GetPilotType() == c_ptMiner && home->CanBuy(l->data())) { minerType = l->data(); break; }
    require(minerType, "authored miner type is available");
    std::vector<IshipIGC *> miners;
    for (int i = 0; i < 4; ++i) {
        core->GetIgcSite()->DroneTypeCompleted(nullptr, home, minerType, core->GetLastUpdate());
        for (auto *l = core->GetShips()->first(); l; l = l->next())
            if (l->data()->GetPilotType() == c_ptMiner && std::find(miners.begin(), miners.end(), l->data()) == miners.end())
                miners.push_back(l->data());
    }
    require(miners.size() == 4, "four original miners spawn");
    std::vector<IasteroidIGC *> rocks;
    for (auto *l = cluster->GetAsteroids()->first(); l && rocks.size() < 2; l = l->next())
        if (l->data()->HasCapability(c_aabmMineHe3) && l->data()->GetOre() > 0)
            rocks.push_back(l->data());
    require(rocks.size() == 2, "two He3 asteroids supply the mining encounter");
    for (size_t i = 0; i < miners.size(); ++i) {
        auto *miner = miners[i];
        auto *rock = rocks[i / 2];
        Vector direction(i % 2 ? -1.f : 1.f, 0, 0);
        miner->SetCluster(cluster);
        miner->SetPosition(rock->GetPosition() + direction * (rock->GetRadius() + miner->GetRadius() + 5));
        miner->SetOrientation(Orientation(-direction));
        miner->SetVelocity(Vector::GetZero());
        miner->SetAutopilot(true);
        miner->SetCommand(c_cmdAccepted, rock, c_cidMine);
    }
    IstationTypeIGC *supremacyType = nullptr;
    for (auto *l = core->GetStationTypes()->first(); l; l = l->next()) {
        std::string_view name = l->data()->GetName();
        if (name.find("Supremacy") != name.npos && name.find("Adv") != name.npos) { supremacyType = l->data(); break; }
    }
    require(supremacyType, "installed core supplies an advanced Supremacy station");
    DataStationIGC stationData{};
    stationData.stationID = core->GenerateNewStationID();
    stationData.stationTypeID = supremacyType->GetObjectID();
    stationData.sideID = pilot->GetSide()->GetObjectID();
    stationData.clusterID = cluster->GetObjectID();
    stationData.position = Vector(-6500, 6500, 6500);
    stationData.forward = Vector(0, 0, 1); stationData.up = Vector(0, 1, 0);
    stationData.bpHull = stationData.bpShield = 1;
    Net::text(stationData.name, "Waypoint regression Supremacy");
    auto *supremacy = static_cast<IstationIGC *>(core->CreateObject(core->GetLastUpdate(), OT_station, &stationData, sizeof(stationData)));
    require(supremacy, "advanced Supremacy fixture creates");
    supremacy->Release();
    m.tick(60);
    m.a->launch();
    m.tick(420);
    pilot->SetPosition(Vector(-6500, -6500, -6500));
    pilot->SetVelocity(Vector::GetZero());
    m.tick(12);
    require(m.a->order(pilot->GetObjectID(), OT_station, supremacy->GetObjectID(), c_cidGoto), "Supremacy autopilot order queues");
    m.wait([&] { return pilot->GetAutopilot() && m.a->state().autopilot; }, "Supremacy autopilot engages on both ends");
    int miningTicks = 0;
    for (int i = 0; i < 600; ++i) {
        // Keep this fixture focused on navigation/ownership rather than the
        // long resource lifecycle. Original AI still arbitrates contention.
        if (i % 60 == 0)
            for (size_t j = 0; j < miners.size(); ++j) {
                auto *miner = miners[j];
                auto *rock = rocks[j / 2];
                if (miner->GetStation()) miner->SetStation(nullptr);
                miner->SetCluster(cluster);
                Vector direction(j % 2 ? -1.f : 1.f, 0, 0);
                miner->SetPosition(rock->GetPosition() + direction * (rock->GetRadius() + miner->GetRadius() + 5));
                miner->SetVelocity(Vector::GetZero());
                miner->SetAutopilot(true);
                miner->SetCommand(c_cmdAccepted, rock, c_cidMine);
            }
        // A plan-only buoy replaces each miner's internal detour for a short
        // interval, without changing its accepted mine order. The next plan
        // restores the real rock; repeated publications exercise retirement.
        if (i % 60 == 24 || i % 60 == 36)
            for (auto *miner : miners) {
                if (i % 60 == 24) {
                    auto detour = createBuoy(cluster, false);
                    miner->SetCommand(c_cmdPlan, detour, c_cidGoto);
                } else
                    miner->SetCommand(c_cmdPlan, miner->GetCommandTarget(c_cmdAccepted), c_cidMine);
            }
        m.tick(1);
        miningTicks += std::any_of(miners.begin(), miners.end(), [](auto *miner) { return miner->GetStateM() & miningMaskIGC; });
        if (i % 60 == 12) { commandsAgree(*m.a); commandsAgree(*m.b); }
    }
    require(miningTicks > 0 && pilot->GetAutopilot() && !pilot->GetStation() &&
                (pilot->GetPosition() - supremacy->GetPosition()).Length() > 1000,
            "live four-miner/two-He3 encounter retains en-route Supremacy autopilot without approaching docking");
    m.ca->reconnect();
    m.wait([&] { return m.ca->ready(); }, "waypoint ownership survives connected-client replacement");
    m.tick(60);
    commandsAgree(*m.a);
    commandsAgree(*m.b);
    printf("Waypoints: four miners / two He3 rocks, %d mining ticks, %s navigation survives live UDP and reconnect\n",
           miningTicks, supremacyType->GetName());
    m.report("Waypoints: retirement, all command slots, identity/visibility, handoff, ID reuse, off-screen plans and miner navigation");
    puts("Waypoints: snapshot retirement of a commanded buoy passes");
}
void inputControls(const std::filesystem::path &art) {
    double axisError = 0, throttleError = 0, fineAxisError = 0, fineThrottleError = 0;
    float lastAxis = -1, lastThrottle = 0;
    for (int i = 0; i <= 1000000; ++i) {
        float axis = -1 + 2.f * i / 1000000;
        float throttle = float(i) / 1000000;
        float a = Net::expandAxis(Net::compactAxis(axis));
        float t = Net::expandThrottle(Net::compactThrottle(throttle));
        require(a >= lastAxis && t >= lastThrottle, "companding remains monotonic over both complete ranges");
        require(Net::compactAxis(-axis) == -Net::compactAxis(axis), "signed axes preserve symmetric control response");
        axisError = std::max(axisError, double(std::abs(a - axis)));
        throttleError = std::max(throttleError, double(std::abs(t - throttle)));
        if (std::abs(axis) <= .01f)
            fineAxisError = std::max(fineAxisError, double(std::abs(a - axis)));
        if (throttle <= .01f)
            fineThrottleError = std::max(fineThrottleError, double(std::abs(t - throttle)));
        lastAxis = a;
        lastThrottle = t;
    }
    require(axisError < .008 && throttleError < .004 && fineAxisError < .0008 && fineThrottleError < .0004,
            "square-root bytes concentrate precision at low control intent");
    require(Net::expandAxis(Net::compactAxis(0)) == 0 && Net::expandAxis(Net::compactAxis(-1)) == -1 &&
                Net::expandAxis(Net::compactAxis(1)) == 1 && Net::expandThrottle(Net::compactThrottle(0)) == 0 &&
                Net::expandThrottle(Net::compactThrottle(1)) == 1, "neutral rotation and throttle endpoints are exact");
    for (int i = -127; i <= 127; ++i)
        require(Net::compactAxis(Net::expandAxis(int8_t(i))) == i, "signed canonical bytes survive decode/re-encode exactly");
    for (int i = 0; i <= 255; ++i)
        require(Net::compactThrottle(Net::expandThrottle(uint8_t(i))) == i, "throttle canonical bytes survive decode/re-encode exactly");
    for (uint64_t near : {uint64_t(1000), uint64_t(UINT32_MAX) - 10, uint64_t(UINT32_MAX) + 10})
        for (int delta = -256; delta <= 256; ++delta)
            require(Net::expandCounter(uint32_t(near + delta), near) == near + delta,
                    "command clocks/numbers and snapshot receipts expand across counter wrap");
    FlightInput raw;
    raw.yaw = .0001f; raw.pitch = -.0001f; raw.roll = 1; raw.throttle = .0001f;
    raw.manualOverride = raw.boost = raw.throttleChanged = true;
    auto sample = Net::input(raw, uint64_t(UINT32_MAX) + 2, uint64_t(UINT32_MAX) + 4);
    auto decoded = Net::flight(sample);
    auto repeated = Net::input(decoded, sample.number, sample.tick);
    require(!std::memcmp(&sample, &repeated, sizeof(sample)) && decoded.canonical && decoded.manualOverride &&
                decoded.boost && decoded.throttleChanged && decoded.fieldOfView < 0,
            "prediction and replay share byte-identical commands and explicit local-policy decisions");
    printf("Input precision: max axis %.7f, throttle %.7f; within 1%% of zero %.7f / %.7f\n",
           axisError, throttleError, fineAxisError, fineThrottleError);
    Match m(art, 27644, false, false, false, Scenario::FreeFlight);
    m.join();
    const auto before = m.ca->metrics().bytesSent;
    m.ia.active = m.ia.fire = m.ia.boost = true;
    m.ia.throttle = m.ia.yaw = 1;
    m.tick(120);
    require(m.ca->metrics().bytesSent - before < 1024 && m.a->state().docked,
            "docked controls produce compact receipts rather than a 60 Hz command stream");
    m.ia = {};
    m.ia.throttle = .025f;
    m.ia.yaw = .0013f;
    m.ia.pitch = -.037f;
    m.ia.roll = .4f;
    m.a->launch();
    m.tick(420);
    auto *ship = m.authority->core()->GetShip(m.a->state().shipId);
    ship->SetAutopilot(false);
    ship->SetPosition(Vector(6000, 6000, 6000));
    ship->SetVelocity(Vector::GetZero());
    m.tick(12);
    auto expected = Net::flight(Net::input(m.a->canonicalNetworkInput(m.ia), 0, 0));
    const auto &controls = ship->GetControls();
    require(controls.jsValues[c_axisYaw] == expected.yaw && controls.jsValues[c_axisPitch] == expected.pitch &&
                controls.jsValues[c_axisRoll] == expected.roll && controls.jsValues[c_axisThrottle] == expected.throttle * 2 - 1,
            "authority receives exactly the locally scaled and quantized control intent");
    for (float deadZone : {.1f, .003f}) {
        m.ia.yaw = m.ia.pitch = m.ia.roll = 0;
        m.ia.deadZone = deadZone;
        m.tick(12);
        require(m.a->returnToBase(), "input policy autopilot order is queued");
        m.wait([&] { return ship->GetAutopilot() && m.a->state().autopilot; }, "input policy autopilot engages");
        m.ia.yaw = deadZone * 1.99f;
        m.tick(12);
        require(ship->GetAutopilot() && m.a->state().autopilot,
                "client dead zone remains authoritative for disengagement below threshold");
        m.ia.yaw = deadZone * 2.01f;
        m.tick(12);
        require(!ship->GetAutopilot() && !m.a->state().autopilot,
                "raw device threshold releases both sides before byte quantization");
    }
    m.ca->reconnect();
    m.wait([&] { return m.ca->ready(); }, "compact command history resumes on a replacement connection");
    m.tick(120);
    require(m.ca->metrics().inputDrops == 0, "compact prediction history remains bounded after reconnect");
    struct RawPilot {
        std::unique_ptr<Net::Transport> socket;
        Net::Connection connection{};
        Net::Welcome welcome;
        bool hello{}, joined{};
        std::string closed;
    };
    auto pump = [&](RawPilot &peer) {
        m.tick(1);
        auto incoming = peer.socket->receive();
        for (auto &event : peer.socket->events()) {
            if (event.connected && !peer.hello) {
                Net::Hello hello;
                hello.build = Net::buildDigest();
                hello.content = m.digest;
                Net::text(hello.name, "Raw command fixture");
                std::vector<std::byte> bytes;
                Net::append(bytes, Net::Header{Net::magic, Net::Kind::Hello});
                Net::append(bytes, hello);
                require(peer.socket->send(peer.connection, bytes, Net::Lane::Control), "raw command handshake sends");
                peer.hello = true;
            } else if (!event.connected)
                peer.closed = event.reason;
        }
        for (auto &msg : incoming) {
            Net::Reader r(msg.data);
            auto h = r.take<Net::Header>();
            if (h.kind != Net::Kind::Welcome && h.kind != Net::Kind::Packed)
                continue;
            std::vector<std::byte> unpacked;
            Net::unpackMessage(h, r, unpacked);
            if (h.kind == Net::Kind::Welcome) {
                peer.welcome = r.take<Net::Welcome>();
                peer.joined = true;
            }
        }
    };
    auto connectRaw = [&] {
        auto [socket, connection] = Net::connect("127.0.0.1:" + std::to_string(m.port));
        RawPilot peer;
        peer.socket = std::move(socket);
        peer.connection = connection;
        for (int i = 0; i < 600 && !peer.joined && peer.closed.empty(); ++i)
            pump(peer);
        require(peer.joined && peer.closed.empty(), "raw command fixture authenticates");
        return peer;
    };
    auto peer = connectRaw();
    Net::Action launch;
    launch.number = 1;
    launch.match = peer.welcome.match;
    launch.op = Net::Operation::Launch;
    std::vector<std::byte> launchBytes;
    Net::append(launchBytes, Net::Header{Net::magic, Net::Kind::Action});
    Net::append(launchBytes, launch);
    require(peer.socket->send(peer.connection, launchBytes, Net::Lane::Control), "raw pilot launch sends");
    auto *rawShip = m.authority->core()->GetShip(ShipID(peer.welcome.pilot));
    for (int i = 0; i < 600 && rawShip->GetStation(); ++i)
        pump(peer);
    require(!rawShip->GetStation(), "raw pilot launches");
    auto *weapon = static_cast<CweaponIGC *>(rawShip->GetMountedPart(ET_Weapon, 0));
    require(weapon, "raw pilot has original gun");
    FlightInput command;
    command.yaw = .03f;
    command.throttle = .35f;
    command.manualOverride = command.fire = command.boost = true;
    std::array<Net::Input, 3> recovery;
    recovery[0] = Net::input(command, 1, m.server->tick() + 1);
    command.fire = command.boost = command.manualOverride = false;
    recovery[1] = Net::input(command, 2, m.server->tick() + 1);
    recovery[2] = Net::input(command, 3, m.server->tick() + 1);
    auto sendBundle = [&](RawPilot &p, std::span<const Net::Input> samples, uint32_t match) {
        std::vector<std::byte> bytes;
        Net::append(bytes, Net::InputHeader{Net::magic, Net::Kind::Input, match, 0});
        Net::append(bytes, samples);
        require(p.socket->send(p.connection, bytes, Net::Lane::State), "raw input bundle sends");
    };
    const auto shots = weapon->NativeShots();
    sendBundle(peer, recovery, uint32_t(peer.welcome.match)); // First two transmissions were lost entirely.
    bool recoveredPress = false;
    for (int i = 0; i < 60; ++i) {
        pump(peer);
        recoveredPress |= weapon->fActive();
    }
    require(recoveredPress && weapon->NativeShots() > shots && !rawShip->GetAutopilot() &&
                rawShip->GetControls().jsValues[c_axisYaw] == Net::expandAxis(recovery[2].yaw),
            "two lost sends recover transient actions and the newest analog intent from one bundle");
    const auto afterRecovery = weapon->NativeShots();
    sendBundle(peer, std::span(recovery).first(1), uint32_t(peer.welcome.match));
    sendBundle(peer, recovery, uint32_t(peer.welcome.match));
    for (int i = 0; i < 30; ++i)
        pump(peer);
    require(weapon->NativeShots() == afterRecovery && !weapon->fActive(),
            "duplicate and reordered old recovery bundles cannot replay a button press");
    command.yaw = 1;
    auto oldMatch = Net::input(command, 4, m.server->tick());
    sendBundle(peer, std::span(&oldMatch, 1), uint32_t(peer.welcome.match - 1));
    for (int i = 0; i < 12; ++i)
        pump(peer);
    require(peer.closed.empty() && rawShip->GetControls().jsValues[c_axisYaw] != 1,
            "stale-match bundles cannot update controls or disconnect the current pilot");
    // Receipt-only packets continue arriving while analog input is withheld.
    // They must acknowledge snapshots without keeping stale controls alive.
    for (int i = 0; i < 180; ++i) {
        if (i % 6 == 0)
            sendBundle(peer, {}, uint32_t(peer.welcome.match));
        pump(peer);
    }
    require(rawShip->GetControls().jsValues[c_axisYaw] == 0 && !(rawShip->GetStateM() & afterburnerButtonIGC),
            "receipt-only traffic cannot defeat the quarter-second input watchdog");
    for (int invalid = 0; invalid < 4; ++invalid) {
        if (invalid)
            peer = connectRaw();
        auto bad = Net::input(command, invalid ? 1 : 4, m.server->tick());
        if (invalid == 0) bad.yaw = -128;
        if (invalid == 1) bad.buttons |= 32768;
        if (invalid == 2) bad.number = 10000;
        if (invalid == 3) bad.tick = uint32_t(m.server->tick() + Net::tickRate);
        sendBundle(peer, std::span(&bad, 1), uint32_t(peer.welcome.match));
        for (int i = 0; i < 600 && peer.closed.empty(); ++i)
            pump(peer);
        require(peer.closed.find("Invalid input bounds") != std::string::npos,
                "invalid signed byte/buttons/sequence/future clock cannot enter authority simulation");
    }
    printf("Compact UDP inputs: two-send recovery, duplicate/reorder suppression, stale match, watchdog and four bounds checks pass\n");
    m.report("Inputs: nonlinear precision, canonical policy, docked suppression, reconnect");
}
void throttleControls(const std::filesystem::path &art) {
    Match m(art, 27650);
    m.join();
    m.a->launch();
    m.wait([&] { return !m.a->state().docked; }, "throttle fixture launches normally");
    auto *ship = m.authority->core()->GetShip(m.a->state().shipId);
    require(!ship->GetAutopilot() && !(ship->GetStateM() & coastButtonIGC) &&
                !m.a->state().autopilot && !m.a->state().coast,
            "fresh launch enables manual throttle without silently enabling vector lock");
    require(!ship->GetMountedPart(ET_Afterburner, 0) && !m.a->playerShip()->GetMountedPart(ET_Afterburner, 0),
            "throttle regression uses the original stock Scout without an afterburner");
    // Isolate flight controls from collisions and boundary damage during the
    // long turns. Do not reset controls, autopilot, vector lock or velocity:
    // earlier flight fixtures did that and could conceal launch-state bugs.
    const float radius = m.authority->core()->GetFloatConstant(c_fcidRadiusUniverse);
    m.authority->core()->SetFloatConstant(c_fcidOutOfBounds, 1000);
    ship->SetPosition(Vector(radius * 4, 0, 0));
    const float fuel = ship->GetFuel();
    m.ia.active = true;
    m.ia.throttle = 1;
    m.ia.boost = m.ia.forward = m.ia.throttleChanged = false;
    auto direction = [&] { auto v = ship->GetVelocity(); return v / v.Length(); };
    auto ordinary = [&] {
        for (auto *s : {ship, m.a->playerShip()})
            require(!(s->GetStateM() & (coastButtonIGC | afterburnerButtonIGC | forwardButtonIGC)) && !s->GetAutopilot(),
                    "normal throttle does not depend on vector lock, Shift, a thrust button or autopilot");
    };
    m.tick(600);
    ordinary();
    require(ship->GetVelocity().Length() > ship->GetHullType()->GetMaxSpeed() * .5f &&
                m.a->state().speed > ship->GetHullType()->GetMaxSpeed() * .5f,
            "stock Scout accelerates under full throttle alone on authority and replica");
    auto initial = direction();
    m.ia.yaw = 1;
    m.wait([&] { return ship->GetOrientation().GetForward() * initial < .05f; },
           "ordinary yaw turns ship sideways to its original travel direction", 1800);
    m.ia.yaw = 0;
    m.tick(30);
    m.wait([&] { return direction() * ship->GetOrientation().GetForward() > .98f; },
           "full throttle redirects velocity toward the sideways heading without Shift", 1800);
    ordinary();
    auto sideways = direction();
    require(sideways * initial < .3f, "sideways turn changes actual trajectory rather than only rotating the model");
    m.ia.yaw = 1;
    m.wait([&] { return ship->GetOrientation().GetForward() * sideways < -.95f; },
           "ordinary yaw points ship backwards relative to its travel", 1800);
    m.ia.yaw = 0;
    m.tick(30);
    m.wait([&] { return ship->GetVelocity().Length() > ship->GetHullType()->GetMaxSpeed() * .4f &&
                       direction() * sideways < -.9f && direction() * ship->GetOrientation().GetForward() > .98f; },
           "full throttle reverses actual travel after turning backwards without Shift", 1800);
    ordinary();
    m.ia.throttle = 0;
    m.wait([&] { return ship->GetVelocity().Length() < 1 && m.a->state().speed < 1; },
           "zero throttle brakes authority and replica without Shift or a change hint", 1800);
    m.ia.throttle = .35f;
    m.tick(600);
    ordinary();
    const auto decoded = Net::expandThrottle(Net::compactThrottle(.35f));
    require(std::abs(ship->GetVelocity().Length() / ship->GetHullType()->GetMaxSpeed() - decoded) < .05f &&
                std::abs(m.a->state().throttle - decoded) < .0001f,
            "partial throttle reaches the quantized target speed and pilot display without Shift");

    // Also protect the intentional original behavior described by the report:
    // lock preserves velocity, Shift temporarily overrides it, release keeps
    // lock enabled, and an explicit second toggle restores normal flight.
    m.a->toggleVectorLock();
    m.wait([&] { return (ship->GetStateM() & coastButtonIGC) && m.a->state().coast; }, "vector lock engages through authority");
    auto locked = ship->GetVelocity();
    auto heading = ship->GetOrientation().GetForward();
    m.ia.yaw = 1;
    m.ia.throttle = 1;
    m.wait([&] { return heading * ship->GetOrientation().GetForward() < .8f; }, "vector lock permits physical turning", 900);
    require((ship->GetVelocity() - locked).Length() < .01f,
            "vector lock preserves world velocity while turning at full throttle");
    m.ia.yaw = 0;
    m.ia.boost = true;
    m.tick(60);
    require((ship->GetVelocity() - locked).Length() > 1 && (ship->GetStateM() & coastButtonIGC) &&
                ship->GetFuel() == fuel && m.a->playerShip()->GetFuel() == fuel,
            "Shift temporarily overrides vector lock with ordinary scout thrust and consumes no booster fuel");
    m.ia.boost = false;
    m.tick(30);
    locked = ship->GetVelocity();
    m.tick(60);
    require((ship->GetVelocity() - locked).Length() < .01f && m.a->state().coast,
            "releasing Shift preserves vector lock and its new world velocity");
    m.a->toggleVectorLock();
    m.wait([&] { return !(ship->GetStateM() & coastButtonIGC) && !m.a->state().coast; }, "second vector-lock toggle restores ordinary flight");
    m.wait([&] { return direction() * ship->GetOrientation().GetForward() > .98f; },
           "unlock restores full-throttle trajectory correction without Shift", 1800);
    ordinary();
    m.ca->reconnect();
    m.wait([&] { return m.ca->ready(); }, "throttle fixture reconnects");
    m.tick(30);
    ordinary();
    require(m.ca->metrics().inputDrops == 0, "throttle and steering regression retains bounded input history");
    m.report("Throttle: stock Scout acceleration, 90/180-degree turns, braking, partial speed, vector-lock/Shift and reconnect");
}
void weaponSelection(const std::filesystem::path& art) {
    Match m(art,27651);m.join();m.a->launch();
    m.wait([&]{return !m.a->state().docked && !m.a->state().cinematic;},"weapon fixture finishes its normal launch");
    int pilot=m.a->state().shipId;auto* ship=m.authority->core()->GetShip(pilot);
    ship->SetPosition(Vector(5000,5000,5000));ship->SetVelocity(Vector::GetZero());
    OriginalWeaponTest::fit(*m.authority,ship,5);
    m.wait([&]{return m.a->playerShip()->GetBaseHullType()->GetObjectID()==ship->GetBaseHullType()->GetObjectID() && m.a->playerShip()->GetMountedPart(ET_Weapon,2);},"sparse multi-gun fitting reaches the real client");
    require(m.authority->controlLocalPilot(m.b->state().shipId) && m.authority->playerShip()->GetStation() && m.authority->state().docked,
            "weapon fixture must retain a different docked authority view while authenticating the flying pilot");
    m.a->toggleAutopilot();m.wait([&]{return ship->GetAutopilot() && m.a->state().autopilot;},"flying caller engages autopilot while the authority view is docked");
    m.a->toggleAutopilot();m.wait([&]{return !ship->GetAutopilot() && !m.a->state().autopilot;},"flying caller disengages autopilot independently of authority view");
    m.tick(30);m.a->clearSounds();m.b->clearSounds();m.ia.active=true;
    auto status=[&]{return m.authority->networkFrame(pilot,m.server->tick(),0).status;};
    auto settled=[&](int selected,bool group){
        auto s=status();return s.selectedWeapon==selected && bool(s.groupFire)==group && m.a->state().selectedWeapon==(group?-1:selected);
    };
    auto fire=[&](int selected,bool group){
        m.ia.fire=true;m.tick(24);
        for(auto* actor:{ship,m.a->playerShip()})
            require((actor->GetStateM()&weaponsMaskIGC)==(group?allWeaponsIGC:oneWeaponIGC) && ((actor->GetStateM()&selectedWeaponMaskIGC)>>selectedWeaponShiftIGC)==selected,
                    "encrypted authority and prediction use different weapon firing bits");
        m.ia.fire=false;m.tick(24);
    };
    for(bool group:{true,false}){
        if(!group){m.a->selectWeapon(0);m.wait([&]{return settled(0,false);},"explicit gun selection reaches authority");}
        int selected=0;
        for(bool previous:{false,false,true,true,false}){
            selected=OriginalWeaponTest::next(5,selected,previous);m.a->cycleWeapon(previous);
            OriginalWeaponTest::hud(*m.a,selected,group);
            m.wait([&]{return settled(selected,group);},"encrypted next/previous preserves original group mode and skips the empty slot");
        }
        fire(selected,group);
        // Several keypresses in one display frame must retain their order
        // across receipts and older reports, without accumulating audio.
        int cues=OriginalWeaponTest::cues(*m.a);
        for(bool previous:{false,true,true,false}){selected=OriginalWeaponTest::next(5,selected,previous);m.a->cycleWeapon(previous);OriginalWeaponTest::hud(*m.a,selected,group);}
        m.wait([&]{return settled(selected,group);},"burst weapon cycling reconciles in original input order");m.tick(60);
        OriginalWeaponTest::hud(*m.a,selected,group);
        require(OriginalWeaponTest::cues(*m.a)==cues && !OriginalWeaponTest::cues(*m.b),"cycling or received reports repeat/leak selection sounds");
    }
    require(OriginalWeaponTest::cues(*m.a)==1 && std::any_of(m.a->sounds().begin(),m.a->sounds().end(),[](const auto& sound){return sound.id==ungroupWeaponsSound;}),"explicit gun selection must play its local cue exactly once");
    m.a->selectWeapon(2);m.wait([&]{return settled(2,true);},"selecting the same active gun restores original group fire");
    m.a->selectWeapon(-1);m.wait([&]{return settled(2,false);},"group toggle restores the remembered hardpoint");
    m.tick(60);require(OriginalWeaponTest::cues(*m.a)==3 && !OriginalWeaponTest::cues(*m.b),"explicit grouping cues are duplicated or broadcast to another pilot");
    m.ca->reconnect();m.wait([&]{return m.ca->ready();},"weapon-selection connection resumes");m.tick(30);
    require(settled(2,false),"resume loses the selected hardpoint or single-gun mode");
    Net::Action correction;correction.op=Net::Operation::Weapon;correction.args[0]=0;
    require(m.authority->authorityAction(pilot,correction),"canonical selection correction");
    m.wait([&]{return settled(0,false);},"client ignores canonical weapon-selection correction");
    require(OriginalWeaponTest::cues(*m.a)==3 && !OriginalWeaponTest::cues(*m.b),"canonical corrections invent interface cues");
    OriginalWeaponTest::fit(*m.authority,ship,1);m.tick(30);m.a->cycleWeapon();m.wait([&]{return settled(0,false);},"single equipped gun must not group itself when cycled");
    fire(0,false);require(OriginalWeaponTest::cues(*m.a)==3 && !OriginalWeaponTest::cues(*m.b),"single-gun wrap emits a grouping cue");
    m.report("Weapon selection: encrypted sparse/single/group cycling, firing masks, queued bursts, one local cue, resume and canonical correction");
}
void flightControls(const std::filesystem::path &art) {
    {
        Match stock(art, 27640);
        stock.join();
        stock.a->launch();
        stock.tick(420);
        auto *scout = stock.authority->core()->GetShip(stock.a->state().shipId);
        require(!scout->GetMountedPart(ET_Afterburner, 0) &&
                    !stock.a->playerShip()->GetMountedPart(ET_Afterburner, 0),
                "installed Conquest stock Scout has no fitted afterburner on either end");
        scout->SetAutopilot(false);
        scout->SetStateBits(coastButtonIGC, 0);
        scout->SetPosition(Vector(5000, 5000, 5000));
        scout->SetVelocity(Vector::GetZero());
        const float fuel = scout->GetFuel();
        stock.ia.active = stock.ia.boost = true;
        stock.ia.throttle = 0;
        stock.tick(60);
        require(scout->GetVelocity().Length() > 1 && scout->GetFuel() == fuel &&
                    stock.a->playerShip()->GetFuel() == fuel,
                "original boost key requests normal forward thrust without burning fuel when no booster is fitted");
        printf("Stock %s: no fitted afterburner, boost-key speed %.3f m/s, fuel unchanged at %.3f\n",
               scout->GetBaseHullType()->GetName(), scout->GetVelocity().Length(), fuel);
    }
    // The installed conquest core's stock Scout has no fitted booster. Exercise
    // fitted-afterburner prediction using the original PCore014 training fighter.
    Match m(art, 27639, false, false, true, Scenario::FreeFlight);
    m.join();
    auto *ship = m.authority->core()->GetShip(m.a->state().shipId);
    auto describe = [&](const char *phase) {
        auto *after = static_cast<IafterburnerIGC *>(ship->GetMountedPart(ET_Afterburner, 0));
        auto *replica = m.a->playerShip();
        auto *predictedAfter = static_cast<IafterburnerIGC *>(replica->GetMountedPart(ET_Afterburner, 0));
        printf("Flight %s: hull %s, state %x/%x, throttle %.3f/%.3f, speed %.3f/%.3f, fuel %.3f/%.3f, "
               "afterburner %s/%s, power %.3f/%.3f\n", phase, ship->GetBaseHullType()->GetName(),
               ship->GetStateM(), replica->GetStateM(), ship->GetControls().jsValues[c_axisThrottle],
               replica->GetControls().jsValues[c_axisThrottle], ship->GetVelocity().Length(),
               replica->GetVelocity().Length(), ship->GetFuel(), replica->GetFuel(),
               after ? after->GetPartType()->GetName() : "none",
               predictedAfter ? predictedAfter->GetPartType()->GetName() : "none", after ? after->GetPower() : 0,
               predictedAfter ? predictedAfter->GetPower() : 0);
        fflush(stdout);
    };
    describe("initial hangar");
    auto choices = m.a->partChoices(PartKind::Booster, 0);
    require(!choices.empty(), "compatible hull can purchase an afterburner");
    auto choice = *std::min_element(choices.begin(), choices.end(), [](auto &a, auto &b) { return a.price < b.price; });
    require(m.a->replacePart(PartKind::Booster, 0, choice.id), "afterburner purchase is queued");
    m.wait([&] { return ship->GetMountedPart(ET_Afterburner, 0) && m.a->playerShip()->GetMountedPart(ET_Afterburner, 0); },
           "purchased afterburner is replicated");
    m.a->launch();
    m.tick(420);
    ship->SetAutopilot(false);
    ship->SetStateBits(coastButtonIGC, 0);
    ship->SetPosition(Vector(5000, 5000, 5000));
    ship->SetVelocity(Vector::GetZero());
    m.ia.active = true;
    m.ia.throttle = 1;
    m.ia.throttleChanged = true;
    m.tick(600);
    describe("full throttle");
    require(ship->GetVelocity().Length() > ship->GetHullType()->GetMaxSpeed() * .5f,
            "full throttle accelerates the authoritative ship");
    m.ia.throttle = 0;
    m.tick(600);
    describe("zero throttle");
    require(ship->GetVelocity().Length() < 1, "zero throttle brakes without vector lock");
    m.ia.throttleChanged = false;
    float fuel = ship->GetFuel();
    m.ia.boost = true;
    m.tick(30);
    describe("boost");
    auto *after = static_cast<IafterburnerIGC *>(ship->GetMountedPart(ET_Afterburner, 0));
    auto *replicaAfter = static_cast<IafterburnerIGC *>(m.a->playerShip()->GetMountedPart(ET_Afterburner, 0));
    require(after->GetPower() > .01f && replicaAfter->GetPower() > .01f && ship->GetFuel() < fuel &&
                m.a->playerShip()->GetFuel() < fuel, "fitted boost ramps power and consumes authority/predicted fuel");
    const auto camera = m.a->camera({});
    const auto forward = m.a->playerShip()->GetOrientation().GetForward();
    require(glm::length(camera.forward - glm::vec3(forward.x, forward.y, forward.z)) > .00001,
            "fitted boost drives the original cockpit camera effect");
    const auto boostTexture = m.effectTextures.at("e_boosterbmp").id;
    auto particles = m.a->particles();
    require(std::any_of(particles.begin(), particles.end(), [&](auto &p) {
                return (p.texture & 0x0fffffffu) == boostTexture;
            }), "fitted boost emits original booster particles");
    m.ia.boost = false;
    m.tick(120);
    describe("released");
    require(after->GetPower() == 0 && replicaAfter->GetPower() == 0, "afterburner winds down on release");
    m.a->toggleVectorLock();
    m.wait([&] { return (ship->GetStateM() & coastButtonIGC) && m.a->state().coast; }, "vector lock is replicated");
    auto velocity = ship->GetVelocity();
    m.ia.throttle = 1;
    m.ia.throttleChanged = true;
    m.tick(60);
    require((ship->GetVelocity() - velocity).Length() < .01f, "vector lock preserves velocity despite throttle changes");
    m.a->toggleVectorLock();
    m.wait([&] { return !(ship->GetStateM() & coastButtonIGC) && !m.a->state().coast; }, "vector lock releases");
    m.ia.throttleChanged = false;
    m.tick(12); // Drain the deliberate held throttle-change fixture before ordering AP.
    require(m.a->returnToBase(), "autopilot order is queued");
    m.wait([&] { return ship->GetAutopilot() && m.a->state().autopilot; }, "autopilot engages on both ends");
    m.ia.throttle = .2f;
    m.ia.throttleChanged = true;
    m.ca->step(m.ia, 0); // One display-frame event with no simulation/input tick.
    m.ia.throttleChanged = false;
    m.ca->step(m.ia, 1. / Net::tickRate);
    require(!m.a->playerShip()->GetAutopilot(), "sub-tick throttle change releases predicted autopilot");
    m.tick(12);
    require(!ship->GetAutopilot() && !m.a->state().autopilot, "sub-tick throttle change releases authoritative autopilot");
    auto shots = [](IshipIGC *pilot) {
        uint64_t count = 0;
        for (auto *l = pilot->GetParts()->first(); l; l = l->next())
            if (l->data()->GetEquipmentType() == ET_Weapon && l->data()->GetMountID() >= 0)
                count += static_cast<CweaponIGC *>(l->data())->NativeShots();
        return count;
    };
    auto fired = shots(m.a->playerShip());
    auto authoritativeFired = shots(ship);
    m.ia.fire = true;
    m.ca->step(m.ia, 0);
    m.ia.fire = false;
    m.ca->step(m.ia, 1. / Net::tickRate);
    bool activated = false;
    for (auto *l = m.a->playerShip()->GetParts()->first(); l; l = l->next())
        if (l->data()->GetEquipmentType() == ET_Weapon && l->data()->GetMountID() >= 0)
            activated |= static_cast<CweaponIGC *>(l->data())->fActive();
    require(activated, "sub-tick fire press activates the predicted weapon");
    // Original IGC Activate schedules the first shot one burst interval later,
    // and retains that pending shot even when the trigger has been released.
    m.wait([&] { return shots(m.a->playerShip()) > fired && shots(ship) > authoritativeFired; },
           "sub-tick fire press produces the original delayed shot on both ends");
    m.tick(30);
    m.ia.boost = true;
    m.ca->step(m.ia, 0);
    m.ia.boost = false;
    m.ca->step(m.ia, 1. / Net::tickRate);
    require(replicaAfter->GetPower() > 0, "sub-tick boost press reaches predicted afterburner update");
    m.wait([&] { return after->GetPower() > 0; }, "sub-tick boost press reaches authoritative afterburner update");
    m.tick(12);
    fired = shots(m.a->playerShip());
    m.ia.fire = true;
    m.ca->step(m.ia, 0);
    m.ia.fire = false;
    m.ia.active = false;
    m.ca->step(m.ia, 1. / Net::tickRate);
    require(shots(m.a->playerShip()) == fired, "inactive input discards deferred button presses");
    m.tick(12);
    // Compare one replica tick to the full original authority simulation from
    // the same canonical state. Six ticks cover every 16/17 ms clock phase.
    FlightInput controls;
    controls.throttle = .65f;
    controls.boost = true;
    controls.yaw = .35f;
    controls.pitch = -.2f;
    controls.roll = .1f;
    controls.fieldOfView = 65;
    controls = Net::flight(Net::input(m.a->canonicalNetworkInput(controls), 0, 0));
    double velocityError = 0, fuelError = 0, turnError = 0;
    for (int i = 0; i < 6; ++i) {
        auto frame = m.authority->networkFrame(ship->GetObjectID(), m.server->tick() + i, 0);
        m.a->applyNetworkFrame(frame);
        m.a->predict(controls, 1. / Net::tickRate, false);
        const std::pair<int, FlightInput> actor{ship->GetObjectID(), controls};
        m.authority->authorityStep(std::span(&actor, 1), 1. / Net::tickRate);
        auto *predicted = m.a->playerShip();
        velocityError = std::max(velocityError, double((predicted->GetVelocity() - ship->GetVelocity()).Length()));
        fuelError = std::max(fuelError, double(std::abs(predicted->GetFuel() - ship->GetFuel())));
        turnError = std::max(turnError, double((predicted->GetOrientation().GetForward() -
                                               ship->GetOrientation().GetForward()).Length()));
    }
    printf("Six-tick original authority/prediction equivalence: velocity %.9f m/s, fuel %.9f, attitude %.9f\n",
           velocityError, fuelError, turnError);
    require(velocityError < .0001 && fuelError < .0001 && turnError < .00001,
            "prediction uses the same integer-millisecond motion/resource endpoints as IGC authority");
    m.report("Flight: throttle/braking, boost/fuel/camera/particles, vector lock and frame-sampled controls");
}
void presentation(const std::filesystem::path &art) {
    Match m(art, 27638);
    m.join();
    m.a->launch();
    m.b->launch();
    m.tick(420); // Finish original launch cinematics before measuring the cockpit.
    auto *core = m.authority->core();
    for (auto *client : {m.a.get(), m.b.get()}) {
        auto *ship = core->GetShip(client->state().shipId);
        ship->SetAutopilot(false);
        ship->SetStateBits(coastButtonIGC, coastButtonIGC);
        ship->SetPosition(Vector(5000, 5000, client == m.a.get() ? 5000 : 5500));
        ship->SetVelocity(Vector(100, 0, 0));
    }
    m.ia.active = m.ib.active = true;
    m.ia.yaw = .08f;
    constexpr double dt = 1. / 165;
    using Clock = std::chrono::steady_clock;
    for (int phase = 0; phase < 2; ++phase) {
        Net::impairment(phase ? 5 : 0, phase ? 20 : 0, phase ? 5 : 0);
        auto deadline = Clock::now();
        auto serverDeadline = deadline;
        CameraInput input;
        std::vector<double> cameraSteps, remoteSteps, angleSteps;
        glm::vec3 lastCamera{}, lastRemote{}, lastForward{};
        double worstCameraPose = 0, worstCameraAngle = 0;
        size_t cameraHolds = 0, remoteHolds = 0;
        for (int frame = 0; frame < 660; ++frame) {
            m.server->poll();
            while (serverDeadline <= Clock::now()) {
                m.server->step();
                serverDeadline += std::chrono::nanoseconds(1000000000 / Net::tickRate);
            }
            m.ca->poll();
            m.cb->poll();
            FrameTiming timing{monotonicNs(), 0};
            timing.targetPresentNs = timing.sampledNs + 2'000'000;
            m.ca->step(m.ia, dt, timing);
            m.cb->step(m.ib, dt, timing);
            require(!m.ca->closed() && !m.cb->closed(), "high-refresh clients remain connected");
            auto camera = m.a->camera(input);
            auto *remote = m.a->core()->GetShip(m.b->state().shipId);
            require(remote, "friendly pilot remains visible in presentation check");
            auto p = remote->GetPosition();
            glm::vec3 remotePosition{p.x, p.y, p.z};
            if (frame >= 165) {
                double distance = glm::length(camera.position - lastCamera);
                double movement = glm::length(remotePosition - lastRemote);
                cameraSteps.push_back(distance);
                remoteSteps.push_back(movement);
                angleSteps.push_back(glm::length(camera.forward - lastForward));
                cameraHolds += distance < .001;
                remoteHolds += movement < .001;
                worstCameraPose =
                    std::max(worstCameraPose, double(glm::length(camera.position - m.a->state().cockpit)));
                worstCameraAngle =
                    std::max(worstCameraAngle, double(glm::length(camera.forward - m.a->state().forward)));
            }
            lastCamera = camera.position;
            lastRemote = remotePosition;
            lastForward = camera.forward;
            deadline += std::chrono::nanoseconds(1000000000 / 165);
            std::this_thread::sleep_until(deadline);
        }
        auto percentile = [](std::vector<double> values, double p) {
            std::sort(values.begin(), values.end());
            return values[size_t(p * (values.size() - 1))];
        };
        printf("Presentation at 165 Hz, %u Hz snapshots, targeting display 2 ms after sampling (%s): "
               "camera/remote holds %zu/%zu of %zu frames; camera pose/angle mismatch "
               "%.6f m/%.6f; "
               "camera step p50/p99 %.6f/%.6f m, remote %.6f/%.6f m\n",
               publicationRate, phase ? "5% loss, 20 ms each direction, 5% reorder" : "localhost", cameraHolds, remoteHolds,
               cameraSteps.size(), worstCameraPose, worstCameraAngle, percentile(cameraSteps, .5),
               percentile(cameraSteps, .99), percentile(remoteSteps, .5), percentile(remoteSteps, .99));
        fflush(stdout);
        printf("Remote interpolation buffer %.3f ms\n", m.ca->metrics().interpolationDelay * 1000);
        printf("Angular frame step p50/p99 %.9f/%.9f\n", percentile(angleSteps, .5), percentile(angleSteps, .99));
        require(cameraHolds == 0 && worstCameraPose < .02 && worstCameraAngle < .0001,
                "cockpit camera follows the displayed ship at every rendered frame");
        require(remoteHolds < remoteSteps.size() / 50, "remote ship presentation is continuous between publications");
        require(percentile(cameraSteps, .99) < percentile(cameraSteps, .5) * 1.5,
                "snapshot reconciliation does not repeatedly add the existing display correction");
        require(percentile(angleSteps, .99) < percentile(angleSteps, .5) * 1.5,
                "predicted cockpit rotation remains smooth across snapshots");
        require(percentile(remoteSteps, .99) < percentile(remoteSteps, .5) * 1.5,
                "remote presentation clock does not jump with snapshot arrivals");
    }
    Net::impairment(0, 0, 0);
    // Freeze input and authority ticks while changing only this frame's display
    // target. This catches ignoring the lead, a one-tick projection clamp, and
    // accidentally advancing either the persistent clock or simulation state.
    for (int i = 0; i < 100; ++i) {
        m.ca->poll();
        m.cb->poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    FrameTiming timing{monotonicNs(), 0};
    m.ca->step(m.ia, 0, timing);
    const auto baseline = m.ca->metrics();
    auto *ship = m.a->playerShip()->GetSourceShip();
    auto vector = [](const Vector &v) { return glm::vec3(v.x, v.y, v.z); };
    const auto displayed = vector(ship->GetPosition());
    m.a->restoreNetworkPresentation();
    const auto canonical = vector(ship->GetPosition()), velocity = vector(ship->GetVelocity());
    const auto rotation = ship->GetOrientation();
    const auto correction = displayed - canonical - velocity * float(baseline.localPresentationFraction);
    for (auto leadNs : {8'000'000ull, 24'000'000ull, 0ull, 24'000'000ull}) {
        timing.targetPresentNs = timing.sampledNs + leadNs;
        const double lead = double(leadNs) * 1e-9;
        m.ca->step(m.ia, 0, timing);
        const auto &metrics = m.ca->metrics();
        require(metrics.snapshots == baseline.snapshots, "display-target probe has no authority updates");
        require(std::abs(metrics.presentationLead - lead) < 1e-9 &&
                    std::abs(metrics.localPresentationFraction - baseline.localPresentationFraction - lead) < 1e-9,
                "local projection includes display lead without advancing input ticks");
        require(std::abs(metrics.remotePresentationTime - baseline.remotePresentationTime - lead) < 1e-9,
                "remote interpolation evaluates the display target without accumulating render lead");
        const auto expected = canonical + velocity * float(baseline.localPresentationFraction + lead) +
                              correction * float(std::exp(-lead / .06));
        require(glm::length(vector(ship->GetPosition()) - expected) < .003,
                "displayed local position reaches the target even across a simulation-tick boundary");
        m.a->restoreNetworkPresentation();
        require(glm::length(vector(ship->GetPosition()) - canonical) < .0001 &&
                    glm::length(vector(ship->GetOrientation().GetForward()) - vector(rotation.GetForward())) < .0001,
                "future display projection leaves canonical position and orientation unchanged");
    }
    require(FrameTiming{timing.sampledNs, 0}.leadSeconds() == 0 &&
                FrameTiming{timing.sampledNs, timing.sampledNs - 1}.leadSeconds() == 0 &&
                FrameTiming{timing.sampledNs, timing.sampledNs + 1'000'000'000}.leadSeconds() == .1,
            "unknown, missed and stalled display targets use bounded presentation projection");
    puts("Display-target probes: 8/24 ms lead, changing budgets, fallback, stall bound and canonical restoration pass");
    // A dropped 10 Hz publication must not freeze remote poses after only
    // 100 ms of extrapolation. Then a prolonged outage must remain bounded.
    timing.targetPresentNs=0;
    m.ca->step(m.ia,0,timing);
    auto previousTime=m.ca->metrics().remotePresentationTime;
    auto *remote=m.a->core()->GetShip(m.b->state().shipId);
    auto previousPosition=vector(remote->GetPosition());
    const auto remoteVelocity=vector(remote->GetVelocity());
    for(int frame=1;frame<=40;++frame){
        FrameTiming stalled{timing.sampledNs+uint64_t(frame*1e9/165),0};
        m.ca->step(m.ia,dt,stalled);
        auto current=m.ca->metrics().remotePresentationTime;
        auto position=vector(remote->GetPosition());
        require(current>previousTime && glm::length(position-previousPosition)>.01,
                "one missing publication keeps remote display motion continuous");
        previousTime=current;previousPosition=position;
    }
    for(int frame=41;frame<=200;++frame){
        FrameTiming stalled{timing.sampledNs+uint64_t(frame*1e9/165),0};
        m.ca->step(m.ia,dt,stalled);
    }
    auto stopped=m.ca->metrics().remotePresentationTime;
    auto stoppedPosition=vector(remote->GetPosition());
    m.ca->step(m.ia,dt,{timing.sampledNs+3'000'000'000,0});
    require(m.ca->metrics().remotePresentationTime==stopped && vector(remote->GetPosition())==stoppedPosition,
            "extended network outage stops bounded remote extrapolation");
    m.a->restoreNetworkPresentation();
    auto canonicalRemote=vector(remote->GetPosition());
    require(glm::length(stoppedPosition-canonicalRemote)<=glm::length(remoteVelocity)*.251f,
            "remote display extrapolation never exceeds the input watchdog interval");
    puts("Missing-publication probes: continuous display through a missed report, bounded outage and canonical restoration pass");
}
void factionHistory(const std::filesystem::path& art) {
    Match m(art,27664,false,false,false,Scenario::Conquest,1,32767,true);m.join();
    int enemy=m.b->state().shipId;auto* ship=m.authority->core()->GetShip(enemy);
    auto* original=ship->GetStation();auto* home=m.authority->core()->GetShip(m.a->state().shipId)->GetStation();
    auto row=[&](Game& game){auto data=game.playerPaneData();auto i=std::find_if(data.rows.begin(),data.rows.end(),[&](auto& r){return r.id==enemy;});require(i!=data.rows.end(),"public opposing human stays in the player console");return *i;};
    require(!row(*m.a).current && row(*m.a).sector==NA,"fresh roster has no unobserved enemy status");
    ship->SetStation(nullptr);ship->SetCluster(home->GetCluster());ship->SetPosition(home->GetPosition()+Vector(2500,0,0));ship->SetVelocity(Vector::GetZero());ship->SetBB(m.authority->core()->GetLastUpdate(),m.authority->core()->GetLastUpdate(),0);
    m.wait([&]{return row(*m.a).current && row(*m.a).sector==home->GetCluster()->GetObjectID();},"scanner encounter reaches the faction status arena");
    m.wait([&]{return m.b->state().detected;},"pilot's original detected/stealth indicator receives authoritative enemy scanner sight");
    auto seen=row(*m.a);
    ship->SetCluster(original->GetCluster());ship->SetPosition(original->GetPosition()+Vector(2500,0,0));ship->SetVelocity(Vector::GetZero());ship->SetBB(m.authority->core()->GetLastUpdate(),m.authority->core()->GetLastUpdate(),0);
    m.wait([&]{return !m.a->core()->GetShip(enemy) && !row(*m.a).current;},"physical departure freezes last-known faction status");
    m.wait([&]{return !m.b->state().detected;},"loss of hostile scanner sight restores the pilot's stealth indicator");
    std::unique_ptr<Game> lateGame;auto late=m.client("Late faction pilot",lateGame);
    m.wait([&]{late->poll();return late->ready() && lateGame && row(*lateGame).sector==seen.sector && !row(*lateGame).current;},"encrypted late-join bootstrap restores faction history");
    require(!lateGame->core()->GetShip(enemy) && row(*lateGame).ship==seen.ship,"late join has the observed hull without a hidden pose or physical model");
    m.authority->beginNetworkPublication();auto first=m.authority->networkReport(m.a->state().shipId,m.server->tick(),0);
    auto second=m.authority->networkReport(lateGame->state().shipId,m.server->tick(),0);
    auto faction=[&](auto& report){auto i=std::find_if(report.worlds.begin(),report.worlds.end(),[](auto& w){return w.domain==0;});require(i!=report.worlds.end(),"shared faction arena present");return i->frame;};
    require(faction(first)==faction(second),"all faction recipients reuse the same status capture");
    Net::ArenaEncoder encoder;auto world=*faction(first);
    for(int i=0;i<1024;++i)encoder.publishWorld(0,1,1,world);
    require(encoder.encodes()==1,"1024 recipients do not re-encode shared ship history");
    world.header.kind=Net::Kind::Snapshot;auto old=Net::packet(world);
    auto changed=world;Net::ShipIntel inserted;inserted.id=30000;inserted.serial=30001;inserted.side=0;Net::text(inserted.name,"Insertion fixture");changed.shipIntel.insert(changed.shipIntel.begin(),inserted);
    std::reverse(changed.shipIntel.begin(),changed.shipIntel.end());changed.shipIntel.pop_back();auto next=Net::packet(changed);
    require(Net::expandedAgainst(Net::compressed(next,1,old),old)==next && Net::frame(next).shipIntel==changed.shipIntel,"status insertion/removal/reordering preserves raw records through delta relocation");
    auto invalid=changed;invalid.shipIntel.front().status.state=255;auto bad=Net::packet(invalid);bool rejected=false;try{Net::frame(bad);}catch(const std::exception&){rejected=true;}require(rejected,"invalid status state is rejected before IGC application");
    m.report("Faction history: scanner transitions, encrypted late join, no hidden pose, shared capture/encoding and status deltas");
}
void impaired(const std::filesystem::path &art) {
    Match m(art, 27633, true);
    Net::impairment(5, 50, 5);
    m.join();
    m.a->launch();
    m.b->launch();
    m.tick(120);
    m.ia.active = m.ib.active = true;
    m.ia.throttle = .55f;
    m.ib.throttle = .4f;
    for (int i = 0; i < 720; ++i) {
        m.ia.yaw = .35f * std::sin(i * .012f);
        m.ia.pitch = .2f * std::cos(i * .013f);
        m.ib.roll = .2f * std::sin(i * .015f);
        m.ia.boost = i > 180 && i < 300;
        m.tick(1);
    }
    m.ia = {};
    m.ib = {};
    m.ia.active = m.ib.active = false;
    m.tick(240);
    printf("Impaired-window RTT: %.1f/%.1f ms\n", m.ca->metrics().rtt, m.cb->metrics().rtt);
    Net::impairment(0, 0, 0);
    m.tick(120);
    require(m.ca->metrics().snapshots > 100 && m.cb->metrics().snapshots > 100,
            "loss/reordering does not starve replication");
    require(m.ca->metrics().inputDrops == 0 && m.cb->metrics().inputDrops == 0,
            "bounded input history survives impairment");
    require(glm::distance(m.a->state().position,
                          glm::vec3(m.authority->core()->GetShip(m.a->state().shipId)->GetPosition().x,
                                    m.authority->core()->GetShip(m.a->state().shipId)->GetPosition().y,
                                    m.authority->core()->GetShip(m.a->state().shipId)->GetPosition().z)) < 5,
            "prediction converges after impairment clears");
    m.report("Impaired: 5% packet loss, 50 ms each direction, 5% reordering");
}
} // namespace
void connectionReports(const std::filesystem::path& art){
    Match match(art,28771,false,false,false,Scenario::Conquest,1,32767,true);match.join();
    require(!match.ca->takeConnections() && !match.cb->takeConnections() && match.server->metrics().connectionSamples==0,"connection statistics are unsolicited per-frame traffic");
    require(match.ca->queryConnections() && !match.ca->queryConnections(),"connection request is not gated against duplicate outstanding queries");
    std::optional<std::vector<Net::PlayerConnectionInfo>> a,b;
    match.wait([&]{if(!a)a=match.ca->takeConnections();return bool(a);},"encrypted Player Pings report reaches its requester");
    require(a->size()==2 && (*a)[0].pilot!=(*a)[1].pilot && !match.cb->takeConnections(),"connection report is not human-only/current-match or is broadcast unsolicited");
    require(match.server->metrics().connectionSamples==2 && match.server->metrics().connectionReports==1,"connection request does not sample the two connected human transports once");
    require(match.cb->queryConnections(),"second faction cannot request public connection info");match.wait([&]{if(!b)b=match.cb->takeConnections();return bool(b);},"second Player Pings response");
    require(*a==*b && match.server->metrics().connectionSamples==2 && match.server->metrics().connectionReports==2,"identical connection reports resample/reencode statistics per recipient within five seconds");
    auto id=match.b->state().shipId;match.cb.reset();match.wait([&]{return match.server->clients()==1;},"departure removes the disconnected transport");
    a.reset();require(match.ca->queryConnections(),"post-departure report query rejected");match.wait([&]{if(!a)a=match.ca->takeConnections();return bool(a);},"post-departure Player Pings report");
    require(a->size()==1 && a->front().pilot!=id && match.server->metrics().connectionSamples==3,"connection cache retains a retired peer or fails membership invalidation");
    puts("Encrypted connection reports: explicit queries only, actual GNS human/faction statistics, requester-only delivery, five-second shared cache and departure invalidation passed");
}
void alliedStationRearm(const std::filesystem::path& art){
    for(bool crew:{false,true}){
        Match m(art,crew?28777:28776);m.join();auto* core=m.authority->core();
        auto* ship=core->GetShip(m.a->state().shipId);auto* side=ship->GetSide();auto* ally=core->GetSide(1);
        side->SetAllies(0);ally->SetAllies(0);TechTreeBitMask tech;tech.SetAll();side->SetDevelopmentTechs(tech);
        if(crew){
            IhullTypeIGC* hull=nullptr;for(auto* l=core->GetHullTypes()->first();l;l=l->next())
                if(l->data()->GetMaxWeapons()>l->data()->GetMaxFixedWeapons() && l->data()->GetPartMask(ET_Magazine,0)){hull=l->data();break;}
            require(hull,"allied station turret/magazine hull fixture");
            while(auto* l=ship->GetParts()->first())l->data()->Terminate();ship->SetBaseHullType(hull);
            auto mount=Mount(hull->GetMaxFixedWeapons());IpartTypeIGC* gun=nullptr;
            for(auto* l=core->GetPartTypes()->first();l;l=l->next())if(l->data()->GetEquipmentType()==ET_Weapon && hull->CanMount(l->data(),mount)){gun=l->data();break;}
            require(gun,"allied station turret weapon fixture");ship->CreateAndAddPart(gun,mount,1)->SetMountedFraction(1);
        }
        IpartTypeIGC* missile=nullptr;for(auto* l=core->GetPartTypes()->first();l;l=l->next())
            if(l->data()->GetEquipmentType()==ET_Magazine && l->data()->GetPrice()==0 && ship->GetHullType()->CanMount(l->data(),0)){missile=l->data();break;}
        require(missile,"allied station free launcher fixture");if(auto* p=ship->GetMountedPart(ET_Magazine,0))p->Terminate();ship->CreateAndAddPart(missile,0,2)->SetMountedFraction(1);
        auto* expected=missile;while(expected->GetSuccessorPartType() && expected->GetSuccessorPartType()->GetRequiredTechs()<=tech)expected=expected->GetSuccessorPartType();
        m.tick(18);if(crew){require(m.b->boardShip(ship->GetObjectID()),"allied station gunner boards");m.wait([&]{return m.b->state().passenger;},"allied station boarding replicates");}
        int pilot=ship->GetObjectID(),gunner=m.b->state().shipId,hull=ship->GetBaseHullType()->GetObjectID();auto credits=m.authority->pilotBalance(pilot);
        m.a->launch();m.wait([&]{return !m.a->state().docked && (!crew || !m.b->state().docked);},"allied station fixture launches");
        ship->GetMountedPart(ET_Magazine,0)->Terminate();
        for(Mount mount=-c_maxCargo;mount<0;++mount)if(auto* p=ship->GetMountedPart(NA,mount))p->Terminate();
        ship->SetAmmo(0);ship->SetFuel(0);ship->SetEnergy(0);ship->SetFraction(.4f);
        TRef<IstationIGC> station=ally->GetStations()->first()->data();int sector=station->GetCluster()->GetObjectID();
        require(core->GetIgcSite()->DockWithStationEvent(ship,station),"allied station collision accepts docking");
        require(!ship->GetStation() && ship->GetCluster()==station->GetCluster() && !ship->GetMountedPart(ET_Magazine,0) &&
            ship->GetMountedPart(NA,-1) && ship->GetMountedPart(NA,-1)->GetPartType()==expected,"allied station substitutes carrier remounting or loses original own-tech cargo rearm");
        m.wait([&]{auto* cargo=m.a->playerShip()->GetMountedPart(NA,-1);return !m.a->state().docked && m.a->state().sectorId==sector && cargo && cargo->GetPartType()->GetObjectID()==expected->GetObjectID() &&
            (!crew || (m.b->state().passenger && !m.b->state().docked && m.b->state().sectorId==sector));},"allied station rearm and relaunch reach both encrypted replicas");
        require(ship->GetFraction()==1 && ship->GetAmmo()==ship->GetHullType()->GetMaxAmmo() && ship->GetFuel()==ship->GetHullType()->GetMaxFuel(),"allied station loses repaired hull/resources");
        require(ship->GetObjectID()==pilot && ship->GetBaseHullType()->GetObjectID()==hull && station->GetSide()==ally && m.authority->pilotBalance(pilot)==credits,"allied rearm changes hull, faction, identity or wallet");
        if(crew)require(core->GetShip(gunner)->GetParentShip()==ship && !core->GetShip(gunner)->GetStation() && m.b->state().parentId==pilot && m.b->state().sourcePilot==ship->GetName(),"allied relaunch loses original crew relationship or pilot display");
        require(!m.ca->takeDockRefitStarted() && !m.cb->takeDockRefitStarted() && !m.ca->takePurchase() && !m.cb->takePurchase(),"allied fly-through starts a duplicate own-base refit transaction");
    }
    puts("Encrypted allied-station rearm: solo/crewed docking, original own-tech cargo restoration, no carrier remounting, repaired resources, relaunch, crew display/identity and wallet preservation passed");
}
void missionBriefingParity(const std::filesystem::path& art){
    {
        Match m(art,28774,false,true,false,Scenario::Conquest,1,32767,true);m.join();
        auto same=[&](const Game& client){auto source=m.authority->missionBriefing(client.state().playerSide);return source && client.state().briefing && source->generateCivilization==client.state().briefing->generateCivilization && source->text==client.state().briefing->text;};
        require(same(*m.a) && same(*m.b) && m.a->state().briefing->generateCivilization && m.server->metrics().briefingEncodes==2,"generated briefings do not use the two authoritative faction bases");
        auto encodes=m.server->metrics().briefingEncodes;m.tick(60);require(m.server->metrics().briefingEncodes==encodes,"briefing encoded or sent as per-tick state");
        Net::Action team{};team.op=Net::Operation::Team;team.args[0]=noTeam;require(m.ca->action(team),"briefing No Team transition not queued");m.wait([&]{return m.a->state().playerSide==noTeam;},"No Team transition clears briefing");require(!m.a->state().briefing,"No Team retains a faction briefing");
        team.args[0]=1;require(m.ca->action(team),"briefing faction change not queued");m.wait([&]{return m.a->state().playerSide==1 && same(*m.a);},"new faction receives its authoritative base briefing");require(m.server->metrics().briefingEncodes==encodes,"shared faction briefing reencoded per recipient");
        m.ca->reconnect();m.wait([&]{return m.ca->ready() && same(*m.a);},"reconnect restores the generated briefing");require(m.server->metrics().briefingEncodes==encodes,"reconnect reencodes unchanged faction briefing");
    }
    {
        std::string story="Server story 100% literal: %s %n [Font=smallFont]\n"+std::string(8192,'S');
        Match m(art,28775,false,true,false,Scenario::Conquest,1,32767,true,0,true,4,story);m.join();
        require(m.a->state().briefing && m.b->state().briefing && !m.a->state().briefing->generateCivilization && m.a->state().briefing->text==story && m.b->state().briefing->text==story && m.server->metrics().briefingEncodes==1,"server story truncated, lost or encoded separately for faction recipients");
        auto match=m.ca->match();auto* core=m.authority->core();m.authority->core()->GetSide(0)->SetActiveF(false);m.authority->core()->GetSide(1)->SetActiveF(false);
        auto reduced=m.options;reduced.rules.teams=2;require(m.ca->configure(reduced),"inactive-team reduction query not queued");std::string rejection;
        m.wait([&]{if(rejection.empty())rejection=m.ca->takeRejection();return !rejection.empty();},"authority rejects all-inactive team reduction");require(m.ca->match()==match && m.authority->core()==core,"rejected reduction replaces the authority");
        m.authority->core()->GetSide(0)->SetActiveF(true);reduced.story="Client cannot replace a server story";require(m.ca->configure(reduced),"valid reduction query not queued");
        m.wait([&]{return m.ca->ready() && m.cb->ready() && m.ca->match()>match && m.ca->match()==m.cb->match() && m.a->state().briefing && m.b->state().briefing;},"accepted configuration restores server-owned story on both replicas");match=m.ca->match();
        require(m.authority->sessionOptions().story==story && m.a->state().briefing->text==story && m.b->state().briefing->text==story && m.server->metrics().briefingEncodes==2,"new match loses server story or reencodes it per recipient");
        auto emptyGoals=reduced;emptyGoals.rules.conquest=0;emptyGoals.rules.territory=100;emptyGoals.rules.prosperity=0;emptyGoals.rules.flags=emptyGoals.rules.artifacts=emptyGoals.rules.teamKills=emptyGoals.rules.minutes=0;
        require(m.ca->configure(emptyGoals),"no-winning-condition query not queued");rejection.clear();m.wait([&]{if(rejection.empty())rejection=m.ca->takeRejection();return !rejection.empty();},"authority rejects client no-winning-condition configuration");require(m.ca->match()==match && m.authority->sessionOptions().rules.conquest==100,"invalid client objectives reset the match");
    }
    puts("Encrypted briefings: exact authoritative home sectors, one encode per faction/shared story domain, no report traffic, No Team/change/reconnect, literal long stories, authority-owned story persistence and original configuration rejection without reset passed");
}
void investmentRaces(const std::filesystem::path& art){
    Match m(art,27687);m.join();
    // Mission entry auto-donates to whichever peer became commander first.
    // Give the other pilot a real account before testing concurrent spending.
    auto* donorGame=m.a->state().commander?m.a.get():m.b.get();
    auto* viewerGame=donorGame==m.a.get()?m.b.get():m.a.get();
    auto* viewerClient=viewerGame==m.a.get()?m.ca.get():m.cb.get();
    int donor=donorGame->state().shipId,viewer=viewerGame->state().shipId;
    Net::Action allowance;allowance.op=Net::Operation::Donate;allowance.args={viewer,64};
    require(m.authority->authorityAction(donor,allowance),"commander gives the other investor a funded account");
    m.wait([&]{return viewerGame->state().money==m.authority->pilotBalance(viewer);},"investment account donation is replicated");
    auto choices=viewerGame->researchChoices();
    auto chosen=std::find_if(choices.begin(),choices.end(),[&](const auto& row){return row.available && row.type==OT_development && !row.complete && row.price>0 && row.price-row.invested<=m.authority->pilotBalance(donor);});
    require(chosen!=choices.end(),"affordable shared development race fixture");
    int bucketId=chosen->id,cost=chosen->price-chosen->invested;
    auto beforeViewer=m.authority->pilotBalance(viewer),beforeDonor=m.authority->pilotBalance(donor);
    Net::Action invest;invest.op=Net::Operation::Invest;invest.args={bucketId,-1};
    require(m.authority->authorityAction(donor,invest),"other pilot funds shared development before the next report");
    auto* bucket=m.authority->core()->GetSide(0)->GetBucket(bucketId);
    require(bucket && bucket->GetMoney()==chosen->price && m.authority->pilotBalance(donor)==beforeDonor-cost,"winning investment payment differs from bucket/account state");
    viewerGame->clearMessages();auto rejected=viewerClient->metrics().rejected;
    require(viewerGame->invest(bucketId,1),"stale visible investment is queued");
    require(viewerGame->state().money==beforeViewer-1 && viewerGame->core()->GetSide(0)->GetBucket(bucketId)->GetMoney()==chosen->invested+1,"investment does not immediately update original local bucket/wallet feedback");
    m.wait([&]{return viewerClient->metrics().rejected==rejected+1 && viewerGame->core()->GetSide(0)->GetBucket(bucketId)->GetMoney()==chosen->price;},"stale investment corrected through the encrypted report");
    uint64_t modal=99;require(viewerClient->takeRejection(&modal).empty() && !modal,"ordinary shared-investment race invents an error popup");
    require(m.authority->pilotBalance(viewer)==beforeViewer && m.authority->pilotBalance(donor)==beforeDonor-cost && bucket->GetMoney()==chosen->price,"stale investment charges an account or overfunds a development");
    require(std::none_of(viewerGame->state().messages.begin(),viewerGame->state().messages.end(),[](const auto& row){return row.text=="Server declined the requested action.";}),"investment correction invents a failure chat tip");
    choices=viewerGame->researchChoices();auto next=std::find_if(choices.begin(),choices.end(),[](const auto& row){return row.available && row.type==OT_development && !row.complete && row.price-row.invested>=17;});
    require(next!=choices.end(),"second valid investment fixture");int nextId=next->id,paid=next->invested;
    require(viewerGame->invest(nextId,17),"valid investment after race is queued");
    require(viewerGame->state().money==beforeViewer-17 && viewerGame->core()->GetSide(0)->GetBucket(nextId)->GetMoney()==paid+17,"valid investment waits for a server report before updating local feedback");
    m.wait([&]{return m.authority->core()->GetSide(0)->GetBucket(nextId)->GetMoney()==paid+17 && viewerGame->core()->GetSide(0)->GetBucket(nextId)->GetMoney()==paid+17;},"post-race investment remains usable and converges");
    require(m.authority->pilotBalance(viewer)==beforeViewer-17 && viewerClient->metrics().rejected==rejected+1,"post-race purchase loses accounting or rejects a valid action");
    // Sector publication phases deliberately reuse a faction map captured
    // earlier in the cycle. Accept a payment after that immutable capture,
    // then deliver the older map with a newer private report/receipt.
    IclusterIGC* watched=nullptr;
    for(auto* l=m.authority->core()->GetClusters()->first();l;l=l->next())
        if(unsigned(l->data()->GetObjectID())%Net::snapshotInterval>=2){watched=l->data();break;}
    require(watched,"funding causal fixture needs a nonzero sector phase");
    viewerGame->commandView(true);require(viewerGame->viewSector(watched->GetObjectID()),"funding observer enters a staggered command sector");
    m.wait([&]{return m.authority->networkViewSector(viewer)==watched->GetObjectID();},"funding observation sector reaches authority");m.tick(24);
    unsigned phase=unsigned(watched->GetObjectID())%Net::snapshotInterval;
    while(m.server->tick()%Net::snapshotInterval!=phase-1)m.tick(1);
    auto drain=[&](auto predicate,const char* what){for(int i=0;i<300 && !predicate();++i){m.server->poll();m.ca->poll();m.cb->poll();std::this_thread::sleep_for(std::chrono::milliseconds(1));}require(predicate(),what);};
    // Drain preceding publications before recording the next report count.
    for(int i=0;i<10;++i){m.server->poll();m.ca->poll();m.cb->poll();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    auto warmTick=m.server->tick();m.authority->beginNetworkPublication(1);
    auto warm=m.authority->networkReport(viewer,warmTick,0);
    require(warm.worlds.front().frame->header.tick==warmTick,"funding faction cache was not freshly primed");
    auto warmBucket=std::find_if(warm.worlds.front().frame->buckets.begin(),warm.worlds.front().frame->buckets.end(),[&](const auto& b){return b.id==nextId;});
    require(warmBucket!=warm.worlds.front().frame->buckets.end(),"cached funding bucket fixture");int warmPaid=warmBucket->money,warmWallet=viewerGame->state().money;
    require(viewerGame->invest(nextId,3) && viewerGame->invest(nextId,5),"pending funding burst is queued");
    require(viewerGame->state().money==warmWallet-8 && viewerGame->core()->GetSide(0)->GetBucket(nextId)->GetMoney()==warmPaid+8,"burst clicks lose immediate original feedback");
    drain([&]{return m.authority->pilotBalance(viewer)==warmWallet-8;},"authority accepts funding before the next simulation tick");
    require(m.server->tick()==warmTick,"funding receipt fixture advanced simulation");
    auto snapshots=viewerClient->metrics().snapshots;m.tick(1);
    drain([&]{return viewerClient->metrics().snapshots>snapshots;},"staggered funding report arrives");
    require(viewerGame->state().money==warmWallet-8 && viewerGame->core()->GetSide(0)->GetBucket(nextId)->GetMoney()==warmPaid+8,"private acknowledgement retires funding before the shared faction arena incorporates it");
    m.tick(2*Net::snapshotInterval);
    require(viewerGame->state().money==warmWallet-8 && viewerGame->core()->GetSide(0)->GetBucket(nextId)->GetMoney()==warmPaid+8,"canonical funding report charges a predicted burst twice");
    require(viewerGame->invest(nextId,2),"funding resume fixture is queued");viewerClient->reconnect();
    m.wait([&]{return viewerClient->ready() && m.authority->pilotBalance(viewer)==warmWallet-10;},"pending funding survives authenticated resume");m.tick(2*Net::snapshotInterval);
    require(viewerGame->state().money==warmWallet-10 && viewerGame->core()->GetSide(0)->GetBucket(nextId)->GetMoney()==warmPaid+10,"resumed funding is lost or spent twice");
    m.report("Investments: concurrent completion, silent correction, conserved accounts and subsequent funding");
}
void customMapConfiguration(const std::filesystem::path& art){
    Match m(art,28773,false,true,false,Scenario::Conquest,1,32767,true);m.join();auto catalog=Game::customMaps(art);uint64_t match=m.ca->match();
    for(size_t index:{size_t(0),catalog.size()-2,catalog.size()-1}){
        const auto& map=catalog.at(index);int teams=map.teams;
        auto options=m.options;options.rules.teams=teams;options.rules.miners=0;options.customMap=map.file;
        auto preview=Game::mapPreview(options,art);
        require(m.ca->configure(options),"host custom-map configuration not queued");
        if(!preview.error.empty()){
            std::string rejection;m.wait([&]{if(rejection.empty())rejection=m.ca->takeRejection();return !rejection.empty();},"server rejects unavailable catalog map without disconnecting");
            require(m.ca->ready() && m.cb->ready() && m.ca->match()==match && m.cb->match()==match,"unavailable map changes match or disconnects its pilots");continue;
        }
        m.wait([&]{return m.ca->ready() && m.cb->ready() && m.ca->match()>match && m.cb->match()==m.ca->match();},"both encrypted peers receive custom-map configuration");match=m.ca->match();
        require(m.a->sessionOptions().customMap==map.file && m.b->sessionOptions().customMap==map.file && std::string(m.authority->core()->GetMissionParams()->szCustomMapFile)==map.file,"authority/welcome/replica lose the selected map");
        require(m.authority->core()->GetClusters()->n()==int(preview.sectors.size()),"encrypted configuration falls back to a generated map");
        auto wrong=options;wrong.rules.teams=teams==2?3:2;require(m.ca->configure(wrong),"invalid map/team test query not queued");std::string rejection;
        m.wait([&]{if(rejection.empty())rejection=m.ca->takeRejection();return !rejection.empty();},"server rejects incompatible custom-map teams");require(m.ca->match()==match && m.authority->sessionOptions().customMap==map.file,"rejected custom map resets the live authority");
    }
    auto generated=m.options;generated.rules.teams=4;require(m.ca->configure(generated),"generated-map restoration not queued");m.wait([&]{return m.ca->ready() && m.cb->ready() && m.ca->match()>match && m.cb->match()==m.ca->match();},"generated map restored after custom configuration");
    require(m.authority->sessionOptions().customMap.empty() && m.a->sessionOptions().customMap.empty() && m.b->sessionOptions().customMap.empty(),"generated map retains a custom override");
    puts("Encrypted custom-map configuration: original multi/single-sector files, authority/replica/browser records, topology, unavailable/team-mismatch rejection without reset/disconnection and four-faction generated restoration passed");
}
void recoveryParity(const std::filesystem::path& art){
    Match m(art,27741,false,false,false,Scenario::Conquest,1,32767,false,0,true,2,{}, {},0);m.join();
    auto* core=m.authority->core();int id=m.a->state().shipId;auto* pilot=core->GetShip(id);TRef<IstationIGC> home=pilot->GetStation();
    auto base=[&](int stationId){DataStationIGC data;require(home->Export(&data)==sizeof(data),"encrypted recovery station export");data.stationID=stationId;data.position+=Vector(30000+1000*(stationId-31000),0,0);strcpy(data.name,"Recovery fallback");
        auto* station=static_cast<IstationIGC*>(core->CreateObject(core->GetLastUpdate(),OT_station,&data,sizeof(data)));require(station,"encrypted recovery fallback");station->Release();return station;};
    auto* alternate=base(31000);auto* transfer=base(31001);
    m.a->launch();m.wait([&]{return !m.a->state().docked && !m.a->state().cinematic;},"recovery fixture finishes its original launch override");
    auto* sector=pilot->GetCluster();for(auto* l=core->GetClusters()->first();l;l=l->next())if(l->data()!=sector){sector=l->data();break;}
    pilot->SetCluster(sector);pilot->SetPosition(Vector(30000,0,0));pilot->SetVelocity(Vector::GetZero());
    m.wait([&]{return m.a->playerShip()->GetCluster() && m.a->playerShip()->GetCluster()->GetObjectID()==sector->GetObjectID();},"remote death sector reaches the pilot replica");
    m.a->clearSounds();core->GetIgcSite()->KillShipEvent(core->GetLastUpdate(),pilot,nullptr,-1,pilot->GetPosition(),Vector::GetZero());
    m.wait([&]{return m.a->state().ghost && m.a->state().commandView && m.a->state().docked;},"exhausted pilot automatically enters command view");
    m.wait([&]{return !m.a->state().cinematic;},"original death override completes");
    auto heard=[&](int sound){return std::count_if(m.a->sounds().begin(),m.a->sounds().end(),[&](const auto& cue){return cue.id==sound;});};
    require(m.a->state().sectorId==sector->GetObjectID() && m.authority->networkViewSector(id)==sector->GetObjectID() && heard(personalJumpSound)==1,"encrypted recovery loses observation sector or original completion cue");
    m.a->commandView(false);require(m.a->state().commandView && !m.a->restoreDockedLoadout().accepted && !m.a->purchaseLaunchLoadout().accepted,"exhausted network pilot can leave command view or refit/launch");
    core->GetIgcSite()->KillStationEvent(home,nullptr,1);
    m.wait([&]{return m.a->state().stationId==alternate->GetObjectID() && !m.b->state().docked;},"lost base relocates its observer and launches the remaining pilot");
    require(m.a->state().ghost && m.a->state().commandView && m.a->state().sectorId==sector->GetObjectID() && m.authority->networkViewSector(id)==sector->GetObjectID(),"observer evacuation discards command observation");
    require(m.a->teleportStation(transfer->GetObjectID()),"exhausted station transfer queued");
    m.wait([&]{return m.a->state().stationId==transfer->GetObjectID() && m.authority->networkViewSector(id)==sector->GetObjectID();},"observer station transfer retains the viewed sector on both peers");
    m.tick(18);require(m.a->state().ghost && m.a->state().commandView && m.a->state().sectorId==sector->GetObjectID() && m.ca->takeTransfer(),"observer transfer loses identity, view or canonical completion");
    m.a->clearSounds();m.ca->reconnect();m.wait([&]{return m.ca->ready();},"authenticated exhausted-pilot reconnect");
    require(m.a->state().shipId==id && m.a->state().ghost && m.a->state().commandView && m.a->state().sectorId==sector->GetObjectID() && !heard(personalJumpSound),"observer reconnect resets command view/identity or replays recovery cues");
    m.report("Encrypted recovery: original exhausted command seat, death sector/cue, ghost mutation gates, lost-base evacuation, station transfer and authenticated resume passed");
}
void alliedCredits(const std::filesystem::path& art){
    Match m(art,27742,false,false,false,Scenario::Conquest,1,32767,true,0,true,4,{}, {},32767,{0,0,0,-1,-1,-1});m.join();
    std::unique_ptr<Game> thirdGame,enemyGame,lateGame;
    auto third=m.client("Third allied observer",thirdGame,2),enemy=m.client("Hostile observer",enemyGame,3);
    std::unique_ptr<Net::Client> late;
    auto tick=[&](int n){for(int i=0;i<n;++i){for(auto* client:{third.get(),enemy.get(),late.get()})if(client)client->step({},1./Net::tickRate);m.tick(1);for(auto* client:{third.get(),enemy.get(),late.get()})if(client){client->poll();require(!client->closed(),"additional allied credit peer remains connected");}}};
    auto wait=[&](auto predicate,const char* message){for(int i=0;i<1200 && !predicate();++i)tick(1);require(predicate(),message);};
    auto amount=[](Game& game,int id){auto data=game.playerPaneData();auto row=std::find_if(data.rows.begin(),data.rows.end(),[&](auto& p){return p.id==id;});require(row!=data.rows.end(),"encrypted credit roster row");return row->money;};
    wait([&]{return third->ready() && enemy->ready();},"four-faction credit observers join");tick(24);
    int donor=m.a->state().shipId,recipient=m.b->state().shipId;
    require(amount(*m.a,recipient)==0 && amount(*m.b,donor)==0 && amount(*thirdGame,recipient)==0 && amount(*enemyGame,donor)==-1,"encrypted bootstrap reveals foreign balances or suppresses original zero observations");
    require(m.a->donateMoney(recipient,100) && m.a->donateMoney(recipient,200),"encrypted allied donations queued");
    wait([&]{return amount(*m.a,recipient)==300 && amount(*m.b,donor)==-300;},"signed allied donations reach both factions");
    require(amount(*thirdGame,recipient)==0 && amount(*enemyGame,donor)==-1,"donation leaks to third ally or hostile faction");
    late=m.client("Later allied observer",lateGame,1);wait([&]{return late->ready();},"late allied observer joins");
    require(amount(*lateGame,donor)==0,"encrypted late observer inherits pre-join donation history");
    require(m.a->donateMoney(recipient,50),"live donation after late join queued");
    wait([&]{return amount(*m.a,recipient)==350 && amount(*lateGame,donor)==-50 && amount(*m.b,donor)==-350;},"shared report supports different client observation baselines");
    require(m.b->donateMoney(donor,30),"encrypted reverse allied donation queued");
    wait([&]{return amount(*m.a,recipient)==320 && amount(*m.b,donor)==-320 && amount(*lateGame,donor)==-20;},"reverse transfer preserves signed credit observations");
    m.cb->reconnect();wait([&]{return m.cb->ready();},"allied credit observation survives authenticated resume");
    require(amount(*m.b,donor)==-320 && amount(*thirdGame,donor)==0,"resume replays or discards observed credits");
    tick(30);require(amount(*m.a,recipient)==320 && amount(*lateGame,donor)==-20,"repeated publications replay donation counters");
    Net::Action leave;leave.op=Net::Operation::Team;leave.args[0]=noTeam;auto rejoin=leave;rejoin.args[0]=1;
    require(m.cb->action(leave) && m.cb->action(rejoin),"leave/rejoin account reset queued in one receive batch");
    wait([&]{return amount(*m.a,recipient)==0 && amount(*m.b,donor)==0;},"unpublished account reset reaches participant and observer baselines");
    require(amount(*lateGame,donor)==-20,"another pilot's account reset discards the retained viewer's observations");
    require(m.a->donateMoney(recipient,25),"donation to reset account queued");
    wait([&]{return amount(*m.a,recipient)==25 && amount(*m.b,donor)==-25 && amount(*lateGame,donor)==-45;},"fresh account and retained faction peer observe the next transfer correctly");
    m.report("Encrypted allied credits: four faction audiences, signed deltas, late-subscriber baselines, reverse transfer, duplicate suppression, authenticated resume and unpublished account resets passed");
}
void combatFeedback(const std::filesystem::path& art){
    Match m(art,27740);m.join();m.tick(60);auto* core=m.authority->core();auto* attacker=core->GetShip(m.a->state().shipId);
    auto tip=[](Game& game,const std::string& text){return std::count_if(game.state().messages.begin(),game.state().messages.end(),[&](auto& n){return n.tip && n.warning && !n.chat && n.text==text;});};
    auto cue=[](Game& game,int id){return std::count_if(game.sounds().begin(),game.sounds().end(),[&](auto& s){return s.id==id;});};
    for(int trial=0;trial<2;++trial){
        m.a->clearMessages();m.b->clearMessages();m.a->clearSounds();m.b->clearSounds();
        DataShipIGC data{};data.shipID=core->GenerateNewShipID();data.hullID=attacker->GetBaseHullType()->GetObjectID();data.sideID=trial?0:1;data.pilotType=c_ptMiner;data.baseObjectID=NA;data.partsOffset=sizeof(data);snprintf(data.name,sizeof(data.name),"Feedback miner %d",trial);
        auto* victim=static_cast<IshipIGC*>(core->CreateObject(core->GetLastUpdate(),OT_ship,&data,sizeof(data)));require(victim,"network combat feedback miner");victim->SetCluster(attacker->GetStation()->GetCluster());victim->SetPosition(Vector(50000,0,0));victim->Release();
        char privateText[512],commonText[512];snprintf(privateText,sizeof(privateText),"You killed " START_COLOR_STRING "%s" END_COLOR_STRING,(PCC)ConvertColorToString(victim->GetSide()->GetColor()),victim->GetName());
        if(trial)snprintf(commonText,sizeof(commonText),"%s was killed by %s",victim->GetName(),attacker->GetName());
        else snprintf(commonText,sizeof(commonText),START_COLOR_STRING "%s %s" END_COLOR_STRING " was killed by %s",(PCC)ConvertColorToString(victim->GetSide()->GetColor()),victim->GetSide()->GetName(),victim->GetName(),attacker->GetName());
        if(trial)Net::impairment(10,15,5);
        core->GetIgcSite()->KillShipEvent(core->GetLastUpdate(),victim,attacker,100,victim->GetPosition(),Vector::GetZero());
        m.wait([&]{return tip(*m.a,privateText)==1 && tip(*m.b,commonText)==1;},"encrypted common/private combat tips",2400);
        require(!tip(*m.a,commonText) && !tip(*m.b,privateText) && cue(*m.a,newCriticalMsgSound)==1 && cue(*m.b,newCriticalMsgSound)==1,"encrypted common/private feedback duplicates text/cues or loses actor exclusion");
        require(cue(*m.a,minerDestroyedSound)==trial && cue(*m.b,minerDestroyedSound)==trial,"encrypted global drone cue loses its faction or freshness");
        m.tick(120);require(tip(*m.a,privateText)==1 && tip(*m.b,commonText)==1 && cue(*m.a,newCriticalMsgSound)==1 && cue(*m.b,newCriticalMsgSound)==1,"repeated/lost arena reports replay acknowledged combat feedback");
        require(cue(*m.a,minerDestroyedSound)==trial && cue(*m.b,minerDestroyedSound)==trial,"repeated/lost arena reports replay the faction drone cue");
        Net::impairment(0,0,0);
    }
    m.report("Encrypted combat feedback: shared faction arena, private principal override, exact critical tips/cues, actor exclusion, acknowledgement and packet-loss replay passed");
}
int main(int argc, char **argv) {
    try {
        rawRecords();
        auto art = Assets::steamArtwork();
        bool slow = false, scale = false;
        std::string only;
        for (int i = 1; i < argc; ++i)
            if (std::string_view(argv[i]) == "--impaired")
                slow = true;
            else if (std::string_view(argv[i]) == "--scale")
                scale = true;
            else if (std::string_view(argv[i]) == "--case" && i + 1 < argc)
                only = argv[++i];
            else
                art = argv[i];
        if (!only.empty() && only != "weapons" && only != "lobbygrace" && only != "directory" && only != "cores" && only != "create" && only != "credits" && only != "recovery" && only != "feedback" && only != "investments" && only != "dockedcrews" && only != "alliances" && only != "teamactivity" && only != "commandfocus" && only != "allied" && only != "briefing" && only != "maps" && only != "pings" && only != "admission" && only != "normal" && only != "sectors" && only != "crews" && only != "lobby" && only != "projectiles" && only != "drops" && only != "effects" &&
            only != "impaired" && only != "fleet" && only != "peers" && only != "presentation" && only != "flight" &&
            only != "traffic" && only != "cadence" && only != "construction" && only != "inputs" && only != "waypoints" && only != "drones" && only != "health" && only != "throttle" && only != "console" && only != "scores" && only!="chat" && only!="teams" && only!="randomize" && only!="browse" && only!="purchase" && only!="boot" && only!="intel" && only!="requests" && only!="ballots" && only!="missionchat" && only!="wings" && only!="lifecycle")
            throw std::runtime_error("Unknown network test case " + only);
        if (only.empty() || only == "normal")
            normal(art);
        if(only.empty() || only=="feedback")combatFeedback(art);
        if(only.empty() || only=="recovery")recoveryParity(art);
        if(only.empty() || only=="credits")alliedCredits(art);
        if(only.empty() || only=="wings")wings(art);
        if(only.empty() || only=="lifecycle")missionLifecycle(art);
        if(only.empty() || only=="lobbygrace")lobbyGrace(art);
        if(only.empty() || only=="pings")connectionReports(art);
        if(only.empty() || only=="maps")customMapConfiguration(art);
        if(only.empty() || only=="investments")investmentRaces(art);
        if(only.empty() || only=="allied")alliedStationRearm(art);
        if(only.empty() || only=="commandfocus")commandFocus(art);
        if(only.empty() || only=="briefing")missionBriefingParity(art);
        if(only.empty() || only=="console")consoleRoutes(art);
        if(only.empty() || only=="requests")queuedRequests(art);
        if(only.empty() || only=="ballots")ballotParity(art);
        if(only.empty() || only=="missionchat")missionChatParity(art);
        if(only.empty() || only=="chat")chatParity(art);
        if(only.empty() || only=="alliances")lobbyAlliances(art);
        if(only.empty() || only=="teamactivity")teamActivity(art);
        if(only.empty() || only=="teams")teamSettings(art);
        if(only.empty() || only=="randomize")randomizeTeams(art);
        if(only.empty() || only=="cores")coreGames(art);
        if(only.empty() || only=="create")createGames(art);
        if(only.empty() || only=="directory")publicDirectoryDiscovery(art);
        if(only.empty() || only=="browse")browseGames(art);
        if(only.empty() || only=="purchase")purchases(art);
        if(only.empty() || only=="boot")flightBoot(art);
        if(only.empty() || only=="intel")factionHistory(art);
        if(only.empty() || only=="scores"){publicScores(art);scoreReconnect(art);}
        if (only.empty() || only == "sectors")
            sectors(art);
        if (only.empty() || only == "health")
            health(art);
        if (only.empty() || only == "crews")
            crews(art);
        if (only.empty() || only == "dockedcrews")
            dockedCrews(art);
        if(only.empty() || only=="admission")admission(art);
        if (only.empty() || only == "lobby")
            lobby(art);
        if (only.empty() || only == "projectiles")
            projectiles(art);
        if (only.empty() || only == "drops")
            drops(art);
        if (only.empty() || only == "effects")
            effectLifetimes(art);
        if (only.empty() || only == "presentation")
            presentation(art);
        if (only.empty() || only == "flight")
            flightControls(art);
        if (only.empty() || only == "throttle")
            throttleControls(art);
        if (only.empty() || only == "weapons")
            weaponSelection(art);
        if (only.empty() || only == "inputs")
            inputControls(art);
        if (only.empty() || only == "waypoints")
            waypoints(art);
        if (only.empty() || only == "drones")
            drones(art);
        if (only.empty() || only == "traffic")
            traffic(art);
        if (only.empty() || only == "construction")
            construction(art);
        if (only == "cadence")
            cadence(art);
        if (slow || only == "impaired")
            impaired(art);
        if (scale || only == "fleet")
            fleet(art);
        if (scale || only == "peers")
            peers(art);
        return 0;
    } catch (const std::exception &e) {
        Net::impairment(0, 0, 0);
        fprintf(stderr, "Network check: %s\n", e.what());
        return 1;
    }
}
