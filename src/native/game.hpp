#pragma once
#include "../ClientCore/CustomMaps.h"
#include "renderer.hpp"
#include "session.hpp"
#include "visual_preferences.hpp"
#include "../ClientCore/ChatPreferences.h"
#include "../ClientCore/Ballot.h"
#include "loadouts.hpp"
#include "particles.hpp"
#include "audio_scene.hpp"
#include "quick_chat.hpp"
#include "feedback.hpp"
#include "network_records.hpp"
#include <functional>
#include <optional>
#include <deque>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "../ClientCore/MissionBriefing.h"
#include "../ClientCore/LoadoutMetrics.h"
#include "../ClientCore/LobbyAdmission.h"

class ImissionIGC;
class IshipIGC;

namespace Native {
struct SetupMapPreview {
    struct Sector {int id;glm::vec2 position;std::array<int,6> stations;std::string name;};
    std::vector<Sector> sectors;std::vector<std::array<int,2>> links;std::string error;
};
class TrainingHost;class Audio;
enum class GameRole {Offline,Authority,Replica};
struct PresentationCorrection {
    glm::vec3 position{};
    glm::mat3 sourceRotation{1},turretRotation{1};
};
struct ModelHandle { uint32_t id{};float radius{1};std::span<const ModelFrame> frames;std::span<const ModelLight> lights; };
using ModelLoader=std::function<ModelHandle(const std::string&,const std::string&)>;
struct FlightInput {
    float yaw{},pitch{},roll{},throttle{};
    bool forward{},backward{},left{},right{},up{},down{},boost{},fire{},missile{},chaff{},deploy{};
    bool active{true},throttleChanged{},matchSpeed{};float deadZone{.1f};
    float fieldOfView{}; // zero uses local camera; negative disables cockpit slew scaling
    bool canonical{},manualOverride{}; // Network commands have already applied local device/view policy.
};
enum class CameraMode {Cockpit,Chase,External,Target,FlyBy,Orbit,Station,Missile,Command,Base,Count};
inline constexpr const char* cameraModeNames[]={"Cockpit","Chase","External","Target","Flyby","Orbit","Station tracking","Missile","Command","Base"};
struct CameraInput {
    bool chase{}; // Compatibility shorthand for the default cockpit/chase toggle.
    CameraMode mode{CameraMode::Cockpit};
    float panRight{},panDown{},zoom{}; // Command view input, independent of ship controls.
    float yaw{},pitch{},roll{}; // External camera angular rates, radians/second.
    int viewportHeight{1080};
    float fieldOfView{65},headYaw{},distance{500}; // degrees, radians, original zoom distance
};
inline CameraMode cameraMode(const CameraInput& input){return input.mode==CameraMode::Cockpit && input.chase?CameraMode::Chase:input.mode;}
struct CameraState {
    glm::vec3 position{},forward{0,0,-1},up{0,1,0};
    float fieldOfView{65}; // vertical degrees, including the aleph effect
    bool showPlayer{},animating{};CameraMode mode{CameraMode::Cockpit};
};
// Stable IGC object IDs exposed without including the legacy platform headers.
enum ContactType : int { ContactShip=0,ContactStation=1,ContactMissile=2,ContactMine=3,
    ContactProbe=4,ContactAsteroid=5,ContactWarp=7,ContactTreasure=8 };
struct Contact {
    glm::vec3 position;float radius;bool enemy;int type,id;std::string_view name;
    float hull{1},shield{};bool selected{},neutral{},threat{},threatFlash{};std::string_view icon;
    bool commandSelected{},own{},ally{},objective{},ordered{},artifact{},pedestalFlag{},missileThreat{},plainRock{};int treasureUse{};int side{-1};
    bool ownSide()const{return own;}bool allied()const{return ally;}bool self()const{return false;}
    bool pedestal()const{return pedestalFlag;}bool missileTargetsOwnSide()const{return missileThreat;}
    bool plainAsteroid()const{return plainRock;}int usefulTreasure()const{return treasureUse;}
};
enum class ChatChannel {Everyone,Team,Sector,Wing,Target,Allies,FriendlySector,Ship,Leaders,Group,GroupNoEcho,Admin,Count};
inline constexpr std::array<const char*,size_t(ChatChannel::Count)> chatChannelNames{"All pilots","Team","Sector","Wing","Selected pilot","Allied teams","Friendly sector","Ship crew","Commanders","Group","Group","Admin"};
struct ChatAudience {ChatChannel channel{ChatChannel::Team};int recipient{-1};std::vector<int> group;bool none{};};
struct ChatPeer {int id{},side{},wing{},sector{-1},type{};std::string name;bool crew{};};
struct PlayerPaneRow {
    uint32_t identity{};
    int id{-1},side{-1},wing{-1},money{-1},autoDonate{-1},type{},kills{},deaths{},sector{-1},parent{-1},command{},dockedSeconds{};
    std::string name,ship,location,icon,sectorName,objective;bool human{},commander{},controlled{},current{},docked{},stayDocked{},requesting{},allied{};
    bool operator==(const PlayerPaneRow&)const=default;
};
struct PlayerPaneTeam {
    int id{-1},pilots{},alliance{-1};std::string name,icon,civilizationIcon;bool active{true};
    bool operator==(const PlayerPaneTeam&)const=default;
};
struct PlayerPaneData {
    int self{-1},side{-1},money{},donating{-1};bool commander{},training{},closed{},teamSettingsLocked{},acceptJoiners{};
    std::vector<PlayerPaneRow> rows;std::vector<PlayerPaneTeam> teams;
    bool operator==(const PlayerPaneData&)const=default;
};
struct GameStatePanelLine {
    std::array<std::string,5> columns;glm::vec4 color{1};
    bool operator==(const GameStatePanelLine&)const=default;
};
struct GameStatePanelTile {
    std::string title,description;int columns{3},rows{2};
    std::array<GameStatePanelLine,4> lines;
    bool operator==(const GameStatePanelTile&)const=default;
};
struct GameStatePanelData {
    std::vector<GameStatePanelTile> tiles;std::string clock;
    bool operator==(const GameStatePanelData&)const=default;
};
enum class PartKind { Chaff,Weapon,Missile,Dispenser,Shield,Cloak,Pack,Booster };
struct EquipmentState {
    PartKind kind;int mount{},amount{};std::string name;
    float mounted{},ready{},value{};bool selected{},active{},empty{},fixedWeapon{};
    int readiness{}; // original: absent/cargo=0, mounting=1, arming=2, ready=3
};
struct InventoryRow {
    std::string mdl,name,key,location,gunner;
    float range{},damage{},rate{},count{},mounted{},armed{},afterburnSpeed{},afterburnTime{},shieldStrength{},rechargeTime{},cloakTime{};
    int readiness{},observers{};bool selected{},active{},outOfAmmo{};
};
struct PartChoice {int id;std::string name;int price{};bool otherShip{};};
struct CivilizationChoice {int id;std::string name,icon;};
struct LoadoutSlotPreview {
    PartKind kind{};int mount{},id{-1},amount{};
    std::string name,description,model,gunner;
    Allegiance::LoadoutPartMetrics metrics;
};
struct LoadoutPreview {
    Loadout loadout;
    std::string name,description,civilizationIcon;
    ModelHandle model;
    std::vector<LoadoutSlotPreview> slots;
    std::vector<PartChoice> hulls;
    // Speed, acceleration, turn rate, HP, energy, scan range, signature, mass.
    std::array<float,8> stats{},baseStats{};
    int defense{},value{},oldValue{},money{},fixedWeapons{},weapons{};
    bool valid{},available{},affordable{},equivalent{},canBuy{};
};
struct ResearchState {int id{},type{},price{},invested{},progress{};bool available{},complete{};std::string name;int count{},limit{-1};};
struct ResearchChoice : ResearchState {
    int group{},bought{},timeToBuild{},timeRemaining{};
    uint32_t buildMask{};
    bool canBuy{},predecessorReady{};
    std::string model,description,status;
    std::vector<std::string> prerequisites;
};
inline constexpr int builderPilot=6; // Original IGC PilotType, checked in game.cpp.
inline constexpr int playerPilot=10;
inline constexpr int firstSectorThreat=5; // Original c_cwMinerThreatened, checked in game.cpp.
struct OrderUnit {int id{},type{},targetId{},targetType{};std::string name,order;bool docked{},stayDocked{};};
struct CrewChoice {int id{},turret{-1};std::string name,ship,location;bool boardable{},aboard{},promotable{};};
struct TeleportChoice {
    int id{};std::string name,sector;bool current{},keepsShip{};
    bool boarding{};std::string ship;int manned{},turrets{};
    bool operator==(const TeleportChoice&)const=default;
};
struct OrderTarget {int id{},type{},sectorId{};std::string name,sector;};
struct SectorState {
    int id;std::string name;glm::vec2 position;int friendlyShips{},enemyShips{},stations{};bool current{};int warning{};
    bool known{true};int owner{-1},secondaryOwner{-1};std::array<int,6> population{};
    bool accepted{},queued{},highlight{};int ripcordKind{},restartStation{-1};
};
struct SectorInfoIcon {std::string image;int side{-1},count{},order{};};
struct SectorResourceIcon {std::string image;int count{};unsigned abilities{};};
struct SectorInfo {int id{-1};std::vector<SectorInfoIcon> assets;std::vector<SectorResourceIcon> asteroids;float helium{};int probes{};};
struct SectorLink { int from,to; };
struct GameMessage { double time;std::string text;bool warning{};uint64_t serial{};std::string displayText;uint64_t displayRevision{};double displayTime{};glm::vec4 color{.85f,.85f,.85f,1};bool chat{},leader{},modal{};int chatTarget{-1};bool fromPlayer{},tip{}; };
struct PilotState {
    int id{},side{},wing{},money{},autoDonate{-1};std::string name,ship,location;
    bool commander{},controlled{},docked{},ghost{},allied{},muted{};
};
// A lobby peer may have no visible IGC ship. Its authoritative identity and
// roles must survive roster filtering, duplicate names and report reordering.
struct LobbyPilot {
    int id{-1},team{};std::string name;
    bool ready{},host{},commander{},controlled{},muted{};
    int requested{noTeamRequest},wing{};
    int kills{},deaths{},ejections{};
    bool operator==(const LobbyPilot&)const=default;
};
struct MoneyRequest {int sender{-1},amount{},hull{-1};std::string name,prompt,displayPrompt;};
struct TeamScore {std::string name;int kills{},deaths{},flags{},artifacts{},sectors{},prosperity{};bool active{};int civilization{-1};std::string civilizationIcon;glm::vec4 color{1};bool operator==(const TeamScore&)const=default;};
struct CommandStatus {int command{-1},targetType{-1},targetId{-1};std::string text;};
struct SectorStar {glm::vec3 direction;float brightness;};
struct EndgameState {
    std::vector<Allegiance::EndgamePlayer> players;std::vector<Allegiance::EndgameTeam> teams;
    int winner{-1};bool frozen{},pods{};
    bool operator==(const EndgameState&)const=default;
};
struct GameState {
    std::shared_ptr<const Allegiance::MissionBriefing> briefing;
    EndgameState endgame;
    glm::vec2 steering{};
    RenderLighting lighting;std::span<const SectorStar> stars;int sectorId{-1};
    glm::vec3 position{},cockpit{},velocity{},forward{0,0,-1},up{0,1,0},right{1,0,0};
    float hull{1},shield{1},energy{1},fuel{1},speed{},throttle{},radius{};
    int ammo{},missiles{},kills{},deaths{},ejections{},shipId{},stationId{-1};
    bool pod{},ripcord{},passenger{};int parentId{-1},turret{-1};float ripcordLeft{};
    bool ghost{};int lives{32767};
    bool cinematic{},cinematicDeath{};bool dead{},docked{},coast{},autopilot{},cloaked{},detected{};
    float ammoFraction{},signature{},experience{},missileLock{},missileReady{};
    int selectedWeapon{-1},selectedCargo{-1},fixedWeapons{};double time{};
    int trainingMission{},trainingOverlay{},trainingRadar{},trainingPanes{};bool trainingPaused{},trainingCommand{},trainingPostSlideshow{};
    bool commandView{},commandTopDown{};int commandUnit{-1},commandOrder{},commandUnitCount{};std::string commandUnitName;
    bool sideActive{true};bool networked{},networkLobby{},networkHost{},networkReady{};int networkCountdown{-1},networkHostPilot{-1},missionStage{};std::vector<LobbyPilot> networkPilots;
    uint32_t lobbyFlags{},autoAccept{0x3f},forceReady{},readySides{};int maxImbalance{-1};bool economy{},developments{true};int money{},minedCredits{};uint64_t economyRevision{};std::string civilization;
    std::array<char,6> alliances{-1,-1,-1,-1,-1,-1};bool allowDefections{true},alliedSight{},alliedRipcord{};
    std::vector<PartChoice> hullChoices;
    std::vector<ResearchState> research;
    std::vector<OrderUnit> units;std::vector<TeamScore> teamScores;
    std::vector<PilotState> pilots;MoneyRequest moneyRequest;bool commander{};int wing{};
    Allegiance::PendingBallot ballot;
    std::string carriedObjective;float timeRemaining{};
    std::vector<EquipmentState> equipment;
    std::vector<InventoryRow> inventory;
    std::vector<SectorState> sectors;std::vector<SectorLink> sectorLinks;
    std::array<glm::vec4,6> sideColors{};int playerSide{-1},ripcordSector{-1};bool ripcordShip{};
    std::deque<GameMessage> messages;
    int targetId{-1},targetObjectType{-1};
    CameraMode camera{CameraMode::Cockpit};
    std::string sourcePilot;
    std::string ship,sector,station,target;glm::vec3 targetPosition{};float targetHull{},targetDistance{};
    std::string targetType,order;float targetShield{},targetSpeed{},targetExperience{};
    bool targetEnemy{},targetHasShield{},targetHasModel{};ModelHandle targetModel;DrawItem targetAppearance{};
    glm::mat4 targetOrientation{1},targetViewOrientation{1};
    glm::vec3 leadDirection{},aimPosition{},eyePosition{};float targetRadius{},aimGreen{},viewFov{65};bool hasLead{},inRange{},hasWeaponSolution{},hasExternalAim{};
    std::array<CommandStatus,3> commands; // queued, accepted, current; original IGC slots
    std::string objective,result,sessionName;bool complete{};
    int friendlyKills{},enemyKills{};
    // Original console presentation values; no additional network records.
    float endurance{},cloakingPercent{},stationHull{},stationShield{};
    int carriedFlag{-1},targetSide{-1};bool killCounter{};
    std::string stationSector,targetSector;
    glm::vec4 stationColor{1},targetColor{.5f,.5f,.5f,1};
};
class Game {
public:
    Game(const std::filesystem::path& artwork,ModelLoader loader,uint32_t effectModel,uint32_t warpModel,SessionOptions options={},EffectLoader effects={},GameRole role=GameRole::Offline);
    ~Game();
    static bool validMap(int index,int teams);
    static SetupMapPreview mapPreview(const SessionOptions& options,const std::filesystem::path& artwork={});
    static std::vector<Allegiance::CustomMapInfo> customMaps(const std::filesystem::path& artwork);
    std::span<const Allegiance::CustomMapInfo> customMaps()const;
    static std::array<char,6> effectiveAlliances(const SessionOptions& options);
    static std::string missionError(const SessionOptions& options,std::span<const Allegiance::CustomMapInfo> maps={},bool fromClient=false);
    std::string configurationError(const SessionOptions& options,bool fromClient=false)const;
    std::shared_ptr<const Allegiance::MissionBriefing> missionBriefing(int side)const;
    void missionBriefing(uint32_t sides,std::shared_ptr<const Allegiance::MissionBriefing> briefing);
    std::vector<CivilizationChoice> civilizations(float* winMoney=nullptr)const;
    static std::vector<CivilizationChoice> civilizations(const std::filesystem::path& artwork,float* winMoney=nullptr,std::string_view core="static_core");
    void step(const FlightInput& input,double dt);
    // Authority owns a single mission. Each pilot has independent client control
    // preferences; selecting an actor here does not transfer command authority.
    void resetAuthority(SessionOptions options);
    const SessionOptions& sessionOptions()const;
    bool teamImbalance(int value); // trusted authority policy / ordered replica update
    Net::MissionPolicy missionPolicy(uint64_t match=0)const;
    bool applyMissionPolicy(const Net::MissionPolicy& policy);
    bool changeAlliance(int side,int ally,bool ripcordDisabled=false);
    void authorityStep(std::span<const std::pair<int,FlightInput>> inputs,double dt);
    bool authorityAction(int pilot,const Net::Action& action,uint32_t* details=nullptr,bool echoChatAuthor=true);
    int joinNetworkPilot(std::string_view name,int side,bool first);
    // Admission has already been authorized by the server. Changing membership
    // preserves the pilot and round; No Team is the original IGC lobby side.
    bool moveNetworkPilotToTeam(int pilot,int side);
    bool bootPlayer(int pilot,int side); // F6 BootSink; revalidate at confirmation.
    int teamLeader(int side)const;
    bool setTeamSettings(int side,int civilization,std::string_view name);
    bool setTeamActive(int side,bool active);
    void startNetworkMission(); // Remove inactive sides' prebuilt bases/drones when the lobby starts.
    void lobbyTeamResetNotice(int pilot,bool balance);
    void lobbyBootNotice(int pilot,int formerSide,bool mission=false);
    void lobbyRequestRejected(int pilot,Allegiance::LobbyJoinFailure reason);
    static std::string_view wingName(int wing);
    void retireNetworkPilot(int pilot);
    void networkPilotConnected(int pilot,bool connected); // exclude reconnect grace from score timers
    void beginNetworkPublication(Net::Tick tick=0);
    int networkViewSector(int pilot) const;
    Net::ReportCapture networkReport(int pilot,Net::Tick tick,uint64_t inputAck,Net::Frame::Effects acknowledged={});
    Net::Frame networkFrame(int pilot,Net::Tick tick,uint64_t inputAck,bool shared=false,Net::Frame reusable={},bool deferredProjectiles=false,Net::Frame::Effects acknowledged={});
    void applyNetworkFrame(const Net::Frame& frame);
    void presentNetwork(std::span<const Net::Entity> entities);
    void presentPrediction(double fraction,const PresentationCorrection& correction,double frameDt);
    void restoreNetworkPresentation();
    std::string networkRejected(const Net::Action& action);
    void predict(const FlightInput& input,double dt,bool effects=true);
    FlightInput canonicalNetworkInput(const FlightInput& input);
    void networkActions(std::function<bool(const Net::Action&)> sink);
    // Replay unincorporated local selections after a canonical report, without
    // resending commands or repeating their interface sounds.
    void replayWeaponAction(const Net::Action& action);
    struct InvestmentIntent {int side,bucket,amount;};
    // Canonical reports replace predicted funding. Replay pending intents
    // without retransmission, completion callbacks or repeated UI effects.
    void replayInvestments(std::span<const InvestmentIntent> pending);
    GameRole role()const;
    Feedback takeFeedback();
    void startTraining(Audio& audio,int joystickCount=0,std::function<std::string(std::string_view)> promptFormatter={});
    bool recordKey(short key);
    bool sendChat(ChatChannel channel,const std::string& text,int recipient=-1,bool echoAuthor=true);
    bool vote(bool agree);
    bool sendQuickChat(ChatChannel channel,const QuickChatCommand& command,int recipient=-1,bool voiceOnly=false);
    std::vector<ChatPeer> chatPeers()const;
    PlayerPaneData playerPaneData();
    GameStatePanelData gameStateData()const;
    std::vector<int> chatSubjects(const ChatAudience& audience)const;
    std::string chatAudienceName(const ChatAudience& audience)const;
    std::vector<OrderTarget> chatTargets(const ChatAudience& audience,int command)const;
    std::array<bool,9> chatVerbs(const ChatAudience& audience)const;
    std::string_view commandCursor(int type,int id,std::span<const int> subjects,bool composing,int command,int individual,int button=-1)const;
    bool sendChatOrder(ChatChannel channel,int recipient,int command,int targetType,int targetId);
    bool sendChatWaypoint(ChatChannel channel,int recipient,int command,int sector,glm::vec3 position,bool cluster=false);
    void setTrainingPane(unsigned mask,bool visible);
    void setTrainingRadar(int mode);
    void commandView(bool enabled,bool topDown=false,short logicalKey=-1);
    bool droneStandby(int ship,bool stay);
    bool zoomCommandCamera(bool out,float seconds=.1f);
    void showGrid(bool enabled);
    void showCommandGrid(bool enabled);
    void visualPreferences(const VisualPreferences& preferences);
    void chatPreferences(const Allegiance::ChatPreferences& preferences);
    void quickChats(const QuickChatCatalog& catalog); // catalog must outlive this Game
    void interfacePrompts(std::function<std::string(std::string_view)> formatter);
    bool mutePilot(int id,bool muted);
    bool viewSector(int id);
    bool selectMapSector(int id);
    bool orderSector(int ship,int id);
    bool highlightSector(int id);
    const SectorInfo& sectorInfo(int id);
    void trackCommandView();
    void toggleCommandPerspective();
    bool viewCamera(CameraMode mode,short logicalKey=-1);
    bool pickCommandUnit(int id,bool toggle=false);
    void pickCommandUnits(std::span<const int> ids);
    std::span<const int> commandUnits()const;
    bool takeMeTo(int id);
    void commandOrder(int command);
    bool commandTarget(int type,int id);
    bool commandPosition(glm::vec3 position);

    void nextTarget();
    void cycleWeapon(bool previous=false);
    bool targetKey(short key);
    void selectTarget(int type,int id);
    void selectWeapon(int mount); // -1 selects group fire; 0..3 selects a fixed mount.
    void selectCargo(int mount);
    void nextCargo();
    bool dropCargo();
    bool swapCargo(PartKind kind,int mount);
    bool reload();
    std::vector<PartChoice> cargoMounts() const;
    void toggleAutopilot();
    bool acceptCommand(bool execute=false);
    void rejectCommand();
    void clearCommand();
    void targetCommand();
    float matchTargetThrottle(float fallback) const;
    void toggleCloak();
    void toggleVectorLock();
    bool takeThrottleReset(float& throttle,bool* forced=nullptr); // keyboard fallback; prefer a valid physical throttle
    void eject();
    bool ripcord(int sector=-1);
    void respawn();
    void recoverToHangar();
    void nextHull();
    void selectHull(int index);
    bool buyHull(int id);
    Loadout currentLoadout() const;
    LoadoutPreview previewLoadout(const Loadout& requested={},bool defaults=false,int targetHull=-1,bool replace=false) const;
    void loadoutPresets(const Loadouts&);
    void loadoutPresetFeedback(const Loadout&,int bank,bool saved);
    void rememberEditorLoadout(const Loadout&,bool onlyIfAbsent=false);
    std::optional<LoadoutSlotPreview> loadoutPartInfo(int part,int hull) const;
    std::vector<PartChoice> loadoutPartChoices(PartKind kind,int mount,int hull) const;
    std::string loadoutName(const Loadout& loadout) const;
    bool applyLoadout(const Loadout& loadout);
    PurchaseResult purchaseLoadout(const Loadout& loadout,bool launch=false);
    PurchaseResult restoreDockedLoadout(bool replace=false);
    bool invest(int bucketId,int amount=-1);
    std::vector<ResearchChoice> researchChoices()const;
    std::span<const int> investmentSectors(uint32_t buildMask);
    bool order(int shipId,int targetType,int targetId,int command);
    std::vector<OrderTarget> orderTargets(int shipId,int command) const;
    std::vector<PartChoice> partChoices(PartKind kind,int mount) const;
    bool replacePart(PartKind kind,int mount,int id);
    // Local session authority; creates a docked pilot with a validated loadout.
    int addLocalPilot(const std::string& name,int side,int hullId=-1);
    bool removeLocalPilot(int id); // returns account and ship value to the side
    bool controlLocalPilot(int id);
    int pilotBalance(int id) const;
    bool donateMoney(int target,int amount);
    bool setAutoDonate(int target,int amount=0); // -1 disables future donations
    bool requestMoney(int amount,int hull=-1);
    bool respondMoneyRequest(bool accept);
    bool setWing(int pilot,int wing);
    bool handOverCommand(int pilot,bool transferFunds=true);
    bool makeTeamLeader(int pilot){return handOverCommand(pilot,false);}
    std::vector<CrewChoice> crewChoices() const;
    std::string pilotName(int id) const;
    bool boardShip(int id,bool observer=false,bool disembarkFirst=false);
    std::string boardingFailure(int id);
    bool leaveShip();
    int shipValue()const;
    void saveEndgame(const std::filesystem::path& file,const EndgameState& results,std::string_view reason,std::string_view server)const;
    std::vector<TeleportChoice> teleportChoices()const;
    std::vector<TeleportChoice> teleportDestinations()const;
    bool teleportStation(int stationId);
    bool promoteGunner(int id);
    bool promoteTurret(int index);
    bool swapTurret(int index);
    bool returnToBase();
    void launch();
    PurchaseResult purchaseLaunchLoadout();
    // Original mission services for training actions and local authority adapters.
    // Their lifetimes end with this Game; callers must use IGC ownership rules.
    ImissionIGC* core() const;
    IshipIGC* playerShip() const;
    const GameState& state() const;
    uint32_t simulationRandomState() const;
    std::array<double,10> simulationPhases() const;
    const CameraState& camera(const CameraInput& input);
    const std::vector<DrawItem>& scene(bool thirdPerson);
    std::span<const ParticleDraw> particles();
    void particleQuality(int quality);
    const std::vector<Contact>& contacts() const;
    std::span<const SoundEvent> sounds() const;
    std::span<const SoundLoop> shipSounds(bool interior);
    std::span<const SoundLoop> worldSounds(bool interior);
    void clearSounds();
    void clearMessages();
    void retainMessages(std::deque<GameMessage> history);
    std::vector<int> soundIds() const;
    std::vector<std::string_view> radarIcons() const;
    std::vector<std::string_view> inventoryScripts() const;
private:
    friend class TrainingHost;
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
