#pragma once
#include "network_transport.hpp"
#include "lan_discovery.hpp"
#include "game.hpp"
#include "frame_timing.hpp"
#include "network_arena.hpp"
#include <deque>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <optional>
#include <utility>
#include <glm/gtc/quaternion.hpp>
namespace Native::Net {
struct Metrics {
    uint64_t snapshots{}, bytesSent{}, bytesReceived{}, rejected{}, corrections{}, discontinuities{},
        missingBaselines{}, inputDrops{}, noticesOmitted{}, eventsOmitted{};
    double correctionSum{}, correctionMax{}, rtt{}, clockOffset{}, inputAge{}, remoteAge{};
    double presentationLead{}, localPresentationFraction{}, remotePresentationTime{};
    double interpolationDelay{};
    double simulationMs{}, publicationMs{}; // Most recent authority tick, including publication buffer cleanup.
    double preparationMs{}, captureMs{}, enqueueMs{};
    uint64_t arenaEncodes{}, publicationJobs{}, arenaBytes{},connectionSamples{},connectionReports{},briefingEncodes{};
    std::array<uint64_t,4> arenaTraffic{}; // Map, physical sector, trajectories, private envelopes.
};
class Server {
    struct Peer {
        std::string name;
        int side{};
        int requested{noTeamRequest},lastSide{noTeam};uint32_t bannedSides{};
        bool ready{true},browsing{},expelled{},ownerBoot{};
        int pilot{-1};
        int64_t connected{}, lastInput{}, lastPublication{};
        std::array<uint64_t, 2> token{};
        uint64_t inputAck{}, actionAck{}, snapshotAck{};
        uint64_t noticeAck{}, eventAck{};
        uint64_t reportsSent{}, reportTick{}, backpressure{};
        FlightInput held{};
        std::deque<Input> inputs;
        std::map<uint64_t, std::shared_ptr<const std::vector<std::byte>>> snapshots;
        uint64_t pendingPublication{};
        std::map<uint64_t, std::pair<uint64_t, uint64_t>> effects;
        std::map<uint64_t, Receipt> receipts;
        // Payments live in the shared faction capture, which can precede the
        // private report. Retire local funding only once that capture includes it.
        std::deque<std::pair<uint64_t,Tick>> investments;
        std::unordered_map<uint32_t,uint64_t> sentArenaCheckpoints;
    };
    Game &game_;
    Session session_;
    Digest content_;
    Instance instance_{};
    std::string listingName_,serverName_;
    std::unique_ptr<Transport> transport_;
    std::unique_ptr<LanDiscovery> discovery_;
    std::unordered_map<Connection, Peer> peers_;
    std::unordered_map<int, Peer> dormant_;
    std::vector<Connection> pendingExpulsions_;
    Tick tick_{};
    uint64_t publication_{}, match_{1};
    int64_t epoch_{};
    bool initialClaimed_{};
    bool lobby_{};
    bool paused_{};
    bool roundEndingPublished_{};
    int host_{-1}, countdown_{-1};
    int64_t restartCountdown_{-1};
    uint64_t lobbySteps_{};
    uint32_t autoAccept_{0x3f},forceReady_{},readySides_{};bool lockedLobby_{},lockedSides_{},rosterDirty_{true};
    uint64_t rosterRevision_{};std::shared_ptr<const Frame> roster_;
    int64_t connectionReportTime_{};std::vector<std::byte> connectionReport_;
    struct BriefingMessage {std::shared_ptr<const Allegiance::MissionBriefing> source;uint64_t match{};std::vector<std::byte> bytes;};
    std::array<BriefingMessage,7> briefingMessages_; // six generated faction domains; one shared story domain
    void sendBriefing(Connection,const Peer&);
    Peer* pilotPeer(int);
    bool canRequest(const Peer&,int)const;
    Allegiance::LobbyJoinFailure positionFailure(const Peer&,int)const;
    bool requestTeam(Peer&,int,uint32_t* details=nullptr);
    bool moveTeam(Peer&,int);
    bool lobbyAction(Peer&,const Action&);
    void publishMissionPolicy();
    bool chatAction(Peer&,const Action&);
    void setAutoAccept(int side,bool enabled);
    void expel(Peer&,bool owner=false);
    void cancelCountdown();
    void automaticCountdown();
    bool randomizeTeams();
    bool allReady()const;
    uint32_t dormantSides()const;
    void captureRoster();
    void lobbyStatus(Frame&)const;
    Metrics metrics_;
    std::vector<std::pair<int, FlightInput>> controls_;
    struct Publisher;
    std::unique_ptr<Publisher> publisher_;
    std::function<std::vector<std::byte>(std::string_view)> directory_;
    std::function<GameCreated(const CreateGameRequest&)> createGame_;
    std::array<uint64_t,2> creatorToken_{};std::string creatorName_;
    void message(const Message &);
    void detach(Connection, bool resume);
    bool configure(Session,bool fromClient=true);
    void welcome(Connection, Peer &);
    void publish(Connection, Peer &, bool reliable = false);
    uint64_t reportActionAck(Peer&,Tick captured,bool fresh=false);
    void flushPublications();
    void commitPublication(Connection, Peer &, const Header &,
        std::shared_ptr<const std::vector<std::byte>>, std::span<const std::byte>, uint64_t, uint64_t, bool);
    bool send(Connection, std::span<const std::byte>, Lane);
    bool sendPublication(Connection,const Header&,std::span<const std::byte>,Lane,bool arena=false);

  public:
    Server(Game &, SessionOptions, const Digest &, uint16_t port, bool lobby = false,
        std::string listingName={},std::string serverName="Native",bool advertise=true);
    ~Server();
    void hosting(std::function<std::vector<std::byte>(std::string_view)> directory,
        std::function<GameCreated(const CreateGameRequest&)> create) {directory_=std::move(directory);createGame_=std::move(create);}
    ServerInfo listing(std::string_view player={})const;
    std::array<uint64_t,2> reserveCreator(std::string name);
    size_t occupants()const;
    void poll();
    void step();
    // FedSrv service pause drains admission/restarts, leaving combat running.
    void pause(bool paused);
    bool paused()const{return paused_;}
    Tick tick() const { return tick_; }
    size_t clients() const;
    size_t readyClients() const;
    std::string publicationDiagnostic();
    const Metrics &metrics() const { return metrics_; }
};
class Client {
  public:
    using Factory = std::function<Game &(const SessionOptions &)>;

  private:
    std::unique_ptr<Transport> transport_;
    Connection peer_{};
    Digest content_;
    Factory factory_;
    Game *game_{};
    std::string address_, name_, error_,serverName_;
    Instance instance_{};
    std::array<uint64_t, 2> token_{};
    int64_t disconnected_{}, retry_{};
    bool reconnecting_{};
    int side_{};
    bool ready_{}, closed_{};
    Tick serverTick_{}, predictionTick_{}, match_{};
    uint64_t inputNumber_{}, actionNumber_{}, snapshotAck_{};
    uint32_t pendingButtons_{};
    int64_t epoch_{}, lastPing_{}, lastPoll_{};
    double accumulator_{}, offset_{}, bestRtt_{1e12}, interpolationDelay_{.075}, arrivalJitter_{};
    int64_t lastSnapshotTime_{};
    double presentationTime_{};
    bool presentationTimeValid_{};
    uint64_t lastPresentationNs_{};
    Metrics metrics_;
    struct Sample {
        Tick tick{};
        int64_t received{};
        std::vector<Entity> entities;
        std::unordered_map<uint32_t, size_t> indices;
    };
    std::deque<Sample> samples_;
    std::vector<Entity> presentation_;
    uint32_t ownSerial_{}, ownEpoch_{};
    glm::vec3 presentationError_{};
    glm::quat sourceRotationError_{1, 0, 0, 0}, turretRotationError_{1, 0, 0, 0};
    std::string rejection_;
    uint64_t rejectedModalAction_{};
    std::deque<Input> inputs_;
    std::map<uint64_t, std::vector<std::byte>> snapshots_;
    struct PendingAction {Action record;int side;}; // Local context; only record goes on the wire.
    std::map<uint64_t, PendingAction> actions_;
    uint64_t appliedActions_{};
    struct PurchaseCompletion { uint64_t number{}; PurchaseResult result; };
    std::deque<PurchaseCompletion> purchases_;
    std::deque<uint64_t> transfers_;
    uint64_t connectionQuery_{};bool connectionQueryPending_{};
    std::optional<std::vector<PlayerConnectionInfo>> connectionReport_;
    uint64_t dockRefitStarted_{};
    std::map<uint64_t, Configuration> configurations_;
    struct Assembly {
        int64_t received{};
        uint32_t count{};
        std::vector<std::byte> data;
        std::vector<bool> pieces;
    };
    std::map<uint64_t, Assembly> assemblies_;
    size_t assemblyBytes_{},startupBytes_{};
    struct ArenaState {
        std::map<uint64_t,std::vector<std::byte>> revisions;
        uint64_t checkpoint{};
    };
    std::unordered_map<uint32_t,ArenaState> arenas_;
    struct PendingReport { Header header; std::vector<ArenaRef> references; Frame personal; };
    std::map<uint64_t,PendingReport> reports_;
    std::vector<ArenaRef> activeArenas_;
    std::deque<Message> startup_;
    void message(const Message &);
    void reconcile(const Frame &);
    void applySnapshot(Frame);
    void applyReports();
    void input(const FlightInput &);
    void sendInputs(bool receiptOnly = false);
    bool send(std::span<const std::byte>, Lane);
    void present(double dt, const FrameTiming &);

  public:
    Client(std::string_view address, Digest content, std::string name, int side, Factory,std::array<uint64_t,2> creatorToken={},Instance instance={});
    ~Client();
    void poll();
    void step(const FlightInput &, double dt, FrameTiming timing = {});
    void reconnect();
    bool action(Action);
    uint64_t lastActionNumber()const{return actionNumber_;}
    bool configure(SessionOptions);
    bool queryConnections();
    std::optional<std::vector<PlayerConnectionInfo>> takeConnections(){return std::exchange(connectionReport_,std::nullopt);}
    uint64_t match() const { return match_; }
    bool ready() const { return ready_; }
    bool closed() const { return closed_; }
    const std::string &error() const { return error_; }
    const std::string &serverName()const{return serverName_;}
    const Instance& instance()const{return instance_;}
    // Modal transactions retain their action identity so a delayed failure
    // cannot overwrite a newer purchase/boarding/station-travel lockdown.
    std::string takeRejection(uint64_t* modalAction=nullptr) {
        if(modalAction)*modalAction=rejectedModalAction_;
        rejectedModalAction_=0;return std::exchange(rejection_, {});
    }
    // A successful receipt is usable only after its canonical loadout and
    // wallet have been reconciled from an authority report. The optional action
    // identifies the UI lockdown; neither it nor the result changes the wire.
    std::optional<PurchaseResult> takePurchase(uint64_t* action=nullptr);
    // Boarding/station travel/team departure releases its lockdown after the report
    // incorporating that action has been reconciled, just like a purchase.
    std::optional<uint64_t> takeTransfer();
    uint64_t takeDockRefitStarted(){return std::exchange(dockRefitStarted_,0);}
    Game *game() const { return game_; }
    const Metrics &metrics() const { return metrics_; }
};
FlightInput flight(const Input &);
Input input(const FlightInput &, uint64_t number, Tick tick);
} // namespace Native::Net
