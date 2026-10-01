#pragma once
// These are the records, not a versioned serialization schema. Incompatible
// builds/content are rejected before any game record is accepted.
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>
#include <memory>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <compare>
#include <cmath>
#include <stdexcept>
#include "session.hpp"
#include "../ClientCore/EndgamePresentation.h"
#include "../ClientCore/PlayerShipStatus.h"
#include "../ClientCore/GameListPresentation.h"
#include "../ClientCore/ConnectionInfoPresentation.h"
#include "../ClientCore/CustomMaps.h"
#include "../ClientCore/CoreCatalog.h"
namespace Native::Net {
static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(float) == 4 && sizeof(double) == 8);
using Digest = std::array<uint8_t, 32>;
// Random for each hosted mission's lifetime, independent of rounds, addresses
// and IGC RNG. Four words keep browse records naturally compact.
using Instance = std::array<uint32_t,4>;
constexpr bool validInstance(const Instance& id){return (id[0]|id[1]|id[2]|id[3])!=0;}
using Tick = uint64_t;
constexpr uint32_t magic = 0x474c4c41;
#ifndef ALLEGIANCE_SNAPSHOT_HZ
#define ALLEGIANCE_SNAPSHOT_HZ 10
#endif
constexpr uint32_t tickRate = 60, snapshotRate = ALLEGIANCE_SNAPSHOT_HZ;
static_assert(snapshotRate >= 10 && snapshotRate <= tickRate && tickRate % snapshotRate == 0);
constexpr uint32_t snapshotInterval = tickRate / snapshotRate;
// Keep an application fragment within one encrypted UDP payload. Fragmenting
// again in GNS multiplies loss and competes for its unreliable segment cache.
constexpr size_t maxPacket = 512 * 1024, maxFrame = 16 * 1024 * 1024, fragmentBytes = 1024, maxEntities = 131072,
                 inputHistory = 256;
enum class Kind : uint32_t {
    Hello,
    Welcome,
    Input,
    Action,
    Receipt,
    World,
    Snapshot,
    Delta,
    Fragment,
    Arena,
    ArenaFragment,
    Report,
    Ping,
    Pong,
    Bye,
    Configure,
    Packed,
    Browse,
    ServerInfo,
    MissionPolicy,
    ConnectionQuery,
    ConnectionReport,
    Briefing,
    FindPlayer,
    Directory,
    CreateGame,
    GameCreated,
    RegisterAuthority,
    AuthorityRegistered,
    Registry
};
struct Briefing {uint32_t sides{},generateCivilization{},bytes{},reserved{};};
static_assert(sizeof(Briefing)==16);
constexpr size_t maxBriefingBytes=maxPacket-sizeof(Briefing)-32;
using PlayerConnectionInfo=Allegiance::PlayerConnectionInfo;
struct ConnectionReport {uint32_t count{},reserved{};};
struct Header {
    uint32_t signature{magic};
    Kind kind{};
    Tick tick{};
    uint64_t sequence{}, ack{};
};
static_assert(sizeof(Header) == 32);
// Input has no second copy of the newest sample's number/time. Match and
// publication receipt belong to the bundle, including receipt-only idle sends.
struct MessagePrefix {
    uint32_t signature{magic};
    Kind kind{};
};
struct InputHeader {
    uint32_t signature{magic};
    Kind kind{Kind::Input};
    uint32_t match{}, ack{};
};
static_assert(sizeof(InputHeader) == 16);
struct Hello {
    Digest build{}, content{};
    char name[32]{};
    int32_t side{};
    uint32_t reserved{};
    std::array<uint64_t, 2> token{};
    Instance instance{}; // Zero permits an initial direct-address join.
};
struct Session {
    MissionSettings rules{};
    std::array<int, 6> civilizations{};
    char teamNames[6][32]{};
    char name[Allegiance::GameNameCapacity]{};
    char coreFile[16]{}; // Original 12-byte IGC basename; explicit storage.
    char customMap[16]{}; // Original 12-byte basename; explicit storage, no pointers/padding.
    std::array<char, 6> alliances{};
    uint8_t teamPolicy{}; // Six inactive sides and the original minimum-player policy; replaces reserved storage.
    uint32_t seed{};
    int32_t scenario{}, map{}, hull{}, enemies{}, allies{}, difficulty{}, killLimit{}, civilization{};
};
struct Welcome {
    Session session{};
    char server[32]{}; // Immutable listing identity; sent only on join/resume/round change.
    int32_t pilot{-1}, side{};
    uint32_t reservedSession{};
    uint64_t match{};
    uint32_t rate{tickRate},reservedRate{};
    std::array<uint64_t, 2> token{};
    uint64_t inputAck{}, actionAck{};
    int64_t epoch{};
    Instance instance{};
};
// Lobby browsing is a separate, read-only handshake. It never creates a pilot
// or requests a world baseline. The same build/data gate applies as to joining.
struct BrowseQuery {Digest build{},content{};};
struct PlayerQuery {BrowseQuery identity{};char player[32]{};};
enum ServerInfoFlag : uint32_t {ServerWaiting=1,ServerStarting=2,ServerRunning=4,ServerJoinable=8,ServerLocked=16,ServerPlayerFound=32};
struct ServerInfo {
    Session session{};
    uint32_t players{},noTeam{},capacity{shipIdentityCapacity},flags{};
    int32_t seconds{};
    char name[Allegiance::GameNameCapacity]{},server[32]{},core[16]{};
    std::array<uint8_t,3> reservedName{};
    Instance instance{};
};
// A directory is carried on the authority's existing encrypted browse socket.
// Game ports are relative to that authority's address; a reply cannot redirect
// a client to an arbitrary host. Record layout is part of the build identity.
constexpr uint32_t maxHostedGames=16;
struct DirectoryInfo {uint32_t count{},capacity{},cores{},reserved{};char server[32]{},location[64]{};Instance instance{};};
struct CoreInfo {char file[16]{};};
struct HostedGame {ServerInfo info{};uint32_t port{},reserved{};};
struct CreateGameRequest {
    BrowseQuery identity{};
    std::array<uint64_t,2> request{};
    char name[Allegiance::GameNameCapacity]{},pilot[32]{},core[16]{};
    std::array<uint8_t,7> reserved{};
};
struct GameCreated {std::array<uint64_t,2> token{};uint32_t port{},reserved{};char error[128]{};};
static_assert(sizeof(ServerInfo)==636 && sizeof(DirectoryInfo)==128 && sizeof(CoreInfo)==16 && sizeof(HostedGame)==644);
static_assert(sizeof(CreateGameRequest)==200 && sizeof(GameCreated)==152);
// The public directory advertises authority entry endpoints, never game state.
// Authorities prove ownership through an encrypted browse probe at the observed
// source IP and supplied listener port before an endpoint is published.
constexpr uint32_t registryCapacity=256,registryLeaseSeconds=30;
struct AuthorityRegistration {BrowseQuery identity{};uint32_t port{},reserved{};};
struct AuthorityRegistered {uint32_t lease{registryLeaseSeconds},reserved{};};
struct RegistryInfo {uint64_t revision{};uint32_t count{},reserved{};};
struct RegistryEndpoint {char address[128]{};};
static_assert(sizeof(AuthorityRegistration)==72 && sizeof(AuthorityRegistered)==8 && sizeof(RegistryInfo)==16 && sizeof(RegistryEndpoint)==128);
struct Vec {
    float x{}, y{}, z{};
};
// Persistent smoke origins are state, rather than a hit event every tick.
// Fixed slots keep their delta offsets stable; unused slots remain zero.
struct ShipDamageFrame {Vec position{},direction{};};
struct ShipDamageState {uint32_t count{};std::array<ShipDamageFrame,10> frames{};};
struct Identity {
    uint32_t serial{};
    int16_t type{-1}, id{-1};
    auto operator<=>(const Identity &) const = default;
};
struct Motion {
    Vec position{}, velocity{}, forward{0, 0, -1}, up{0, 1, 0};
};
// Input records are canonical state in the command queue. No client pose or hit
// result is ever an input. Two prior records accompany the newest one for loss.
struct Input {
    uint32_t number{}, tick{};
    uint32_t buttons{};
    int8_t yaw{}, pitch{}, roll{};
    uint8_t throttle{};
};
static_assert(sizeof(Input) == 16);
// Both signs have 127 magnitudes; zero and endpoints are exact. Reserve -128
// so malformed commands cannot exceed the normalized range when decoded.
inline int8_t compactAxis(float value) {
    if (!std::isfinite(value))
        return 0;
    value = std::clamp(value, -1.f, 1.f);
    return int8_t(std::lround(std::copysign(std::sqrt(std::abs(value)) * 127.f, value)));
}
inline float expandAxis(int8_t value) {
    float magnitude = float(value) / 127.f;
    return magnitude * std::abs(magnitude);
}
inline uint8_t compactThrottle(float value) {
    if (!std::isfinite(value))
        return 0;
    return uint8_t(std::lround(std::sqrt(std::clamp(value, 0.f, 1.f)) * 255.f));
}
inline float expandThrottle(uint8_t value) {
    float magnitude = float(value) / 255.f;
    return magnitude * magnitude;
}
// Commands cannot be more than 256 samples outstanding or half a second ahead.
// Expand modulo counters around the local full-width receipt/clock, preserving
// correct ordering across the 32-bit boundary (over two years at 60 Hz).
inline uint64_t expandCounter(uint32_t value, uint64_t near) {
    return near + int64_t(std::bit_cast<int32_t>(value - uint32_t(near)));
}
enum InputButton : uint32_t {
    Forward = 1,
    Backward = 2,
    Left = 4,
    Right = 8,
    Up = 16,
    Down = 32,
    Boost = 64,
    Fire = 128,
    Missile = 256,
    Chaff = 512,
    Deploy = 1024,
    Active = 2048,
    ThrottleChanged = 4096,
    MatchSpeed = 8192,
    ManualOverride = 16384
};
enum class Operation : uint32_t {
    Launch,
    ReturnToBase,
    Respawn,
    Recover,
    Eject,
    Ripcord,
    BuyHull,
    SelectHull,
    ReplacePart,
    Loadout,
    Invest,
    Order,
    Position,
    Accept,
    Reject,
    Clear,
    Autopilot,
    Cloak,
    Coast,
    Reload,
    Drop,
    Swap,
    Weapon,
    Target,
    Wing,
    Donate,
    AutoDonate,
    Request,
    Respond,
    Commander,
    Board,
    Leave,
    Teleport,
    Promote,
    Chat,
    QuickChat,
    View,
    DroneStandby,
    Ready,
    Start,
    Team,
    Configure,
    NewRound,
    Leader,
    Lobby,
    MapSector,
    SectorOrder,
    SectorHighlight,
    ChatOrder,
    ChatWaypoint,
    ChatCluster,
    ChatPreferences,
    Mute,
    PurchaseLoadout,
    Vote
};
struct OutfitPart {
    int16_t id{-1}, mount{}, amount{32767};
};
static_assert(sizeof(OutfitPart)==6);
struct Action {
    uint64_t number{}, match{};
    Operation op{};
    std::array<int32_t, 4> args{};
    Vec position{};
    uint32_t count{};
    std::array<OutfitPart, 32> parts{};
    char text[256]{};
    uint32_t reserved{}; // Explicit tail padding keeps raw-record deltas stable.
};
inline bool predictsWeaponSelection(const Action& action) {
    return action.op==Operation::Weapon; // args: desired fixed hardpoint, desired group mode.
}
inline bool predictsClientAction(const Action& action){
    return predictsWeaponSelection(action) || action.op==Operation::Invest;
}
struct Configuration {
    uint64_t number{}, match{};
    Session session{};
    uint32_t reserved{};
};
struct Receipt {
    uint64_t number{};
    uint32_t accepted{}, details{}; // Purchase: bit 0 all parts bought, bit 1 launched.
};
struct Command {
    int16_t type{-1}, target{-1}, id{-1};
};
static_assert(sizeof(Command) == 6);
static_assert(sizeof(std::array<Command, 4>) == 24);
struct Entity {
    Identity identity{};
    uint32_t epoch{};
    uint32_t life{};
    uint64_t shot{};
    Motion motion{};
    Vec engine{};
    // Four complete command slots occupy the same 24 bytes as the former
    // three padded slots. Plan is essential for miners and autopilot waypoints.
    std::array<Command, 4> commands{};
    float turn[3]{}, controls[4]{}, hull{}, shield{}, energy{}, fuel{}, ore{}, signature{}, experience{}, radius{},
        power{}, ripcord{}, lock{}, impact{}, cloaking{1};
    uint32_t expiry{}, activation{}; // Original deadlines/anchor times; ships use activation for the dock-state timestamp.
    int32_t state{};
    int16_t side{-1}, sector{-1}, station{-1}, parent{-1}, turret{-1}, hullType{-1}, ammo{}, wing{}, kills{}, deaths{},
        ejections{}, flag{-1}, ripcordType{-1}, ripcordId{-1};
    // Static definition: drone type for ships; projectile/missile/chaff type
    // for effects. One type-specific slot keeps the record size unchanged.
    int16_t definition{-1}, launcher{-1}, targetType{-1}, targetId{-1};
    uint32_t flags{}, payloadBytes{}, partCount{};
};
enum EntityFlag : uint32_t { Eye = 1, Known = 2, Autopilot = 4, Ghost = 8, Dud = 16, StayDocked = 32, Trajectory = 64, Decelerating = 128,
    OutOfBounds = 256, CrowdedSector = 512, Strategic = 1024, DamageState = 2048, Spin = 4096 };
// Trajectory activation is an anchor time in milliseconds since match start.
// Position, velocity and spin stay constant on the wire for this lifetime.
inline Motion trajectoryMotion(const Entity& e,double time) {
    Motion pose=e.motion;
    float age=float(std::max(0.,time-double(e.activation)*.001));
    float travel=age;
    if(e.flags&Decelerating) {
        // Original treasure law: constant deceleration along its initial
        // velocity, clamped at rest. impact is deceleration, engine spin axis,
        // and lock the orientation anchor offset from the trajectory birth.
        auto v=pose.velocity;
        float speed=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
        float dt=speed>0?std::min(age,speed/e.impact):0;
        travel=speed>0?dt-.5f*dt*dt*e.impact/speed:0;
        float fraction=speed>0?std::max(0.f,1-e.impact*age/speed):0;
        pose.velocity={v.x*fraction,v.y*fraction,v.z*fraction};
    }
    auto v=e.motion.velocity;
    pose.position={pose.position.x+v.x*travel,pose.position.y+v.y*travel,pose.position.z+v.z*travel};
    if(e.power!=0) {
        bool drop=e.flags&Decelerating;
        float c=std::cos(e.power*(age-(drop?e.lock:0))),s=std::sin(e.power*(age-(drop?e.lock:0)));
        auto a=(drop || (e.flags&Spin))?e.engine:pose.forward;
        auto rotate=[&](Vec u) {
            float d=a.x*u.x+a.y*u.y+a.z*u.z;
            return Vec{u.x*c+(a.y*u.z-a.z*u.y)*s+a.x*d*(1-c),
                       u.y*c+(a.z*u.x-a.x*u.z)*s+a.y*d*(1-c),
                       u.z*c+(a.x*u.y-a.y*u.x)*s+a.z*d*(1-c)};
        };
        pose.up=rotate(pose.up);
        if(drop || (e.flags&Spin))pose.forward=rotate(pose.forward);
    }
    return pose;
}
struct Part {
    Identity ship{};
    int16_t id{-1}, mount{};
    int32_t amount{};
    float mounted{}, fraction{}, lock{};
    uint32_t timeLoaded{}, timeFired{}; // Original IGC deadlines in the match clock.
    uint32_t shots{}, flags{};
};
struct Pilot {
    int16_t id{-1}, side{-1}, wing{-1}, autoDonate{-1};
    int32_t money{}; // Own balance; foreign faction's cumulative observed transfers.
    uint32_t identity{}; // Account generation; survives hull/crew reloads.
    char name[32]{};
};
struct Team {
    int16_t id{-1}, commander{-1};
    int32_t funds{};
    uint32_t active{};
};
// Public mission membership has no faction-private account, equipment or pose.
// Keep it separate from Pilot so a shared roster cannot overwrite private state.
struct Member {
    int16_t id{-1},side{noTeam},requested{noTeamRequest};uint16_t flags{};
    char name[32]{};
    int16_t kills{},deaths{},ejections{};uint16_t reserved{};
    bool operator==(const Member&)const=default;
};
static_assert(sizeof(Member)==48);
// Faction knowledge, independent of the recipient's physical-sector replicas.
// It contains last observed values only: no pose, account, equipment or pointers.
struct ShipIntel {
    uint32_t serial{};
    int16_t id{-1},side{-1},pilotType{};uint16_t reserved{};
    Allegiance::PlayerShipStatus status{};
    char name[32]{};
    bool operator==(const ShipIntel&)const=default;
};
static_assert(sizeof(ShipIntel)==60 && std::is_trivially_copyable_v<ShipIntel>);
using ResultPlayer=Allegiance::EndgamePlayer;
using ResultTeam=Allegiance::EndgameTeam;
struct Bucket {
    int16_t side{-1}, id{-1};
    int32_t money{};
    float time{};
};
enum NoticeFlag : uint32_t {
    NoticeWarning=1u, NoticeChat=2u, NoticeSender=4u, NoticeHuman=8u,
    NoticeEveryone=16u, NoticeLobby=32u, NoticeIndividual=64u,
    NoticeHasText=128u, NoticeHasVoice=256u, NoticeKnownVoice=512u,
    NoticeQuick=1024u, NoticeLeader=2048u, NoticeCommand=4096u,
    NoticeEcho=8192u, NoticeTraining=16384u, NoticeSilent=32768u,
    NoticePersonal=65536u, NoticeModal=131072u,
    NoticeTip=1u<<23, // PostText/ConsoleData tips, independent of chat history.
    NoticeBallot=1u<<24, NoticeHideBallot=1u<<25,
    NoticeSharedTip=1u<<26 // Shared PostText tip/cue; sender/voice exclude two actor IDs.
};
// Preserve the original ChatTarget in spare notice flag bits. Channel tabs
// can filter retained history without another wire field or notice record.
inline constexpr uint32_t noticeChannelShift=18,noticeChannelMask=31u<<noticeChannelShift;
inline uint32_t noticeChannel(int target){return uint32_t(target+1)<<noticeChannelShift;}
inline int noticeChannel(uint32_t flags){return int((flags&noticeChannelMask)>>noticeChannelShift)-1;}
inline uint32_t noticeColor(float r,float g,float b,float a=1) {
    auto byte=[](float x){return uint32_t(std::round(std::clamp(x,0.f,1.f)*255.f));};
    return byte(r)|(byte(g)<<8)|(byte(b)<<16)|(byte(a)<<24);
}
inline std::array<float,4> noticeColor(uint32_t rgba) {
    return {float(rgba&255)/255,float((rgba>>8)&255)/255,float((rgba>>16)&255)/255,float(rgba>>24)/255};
}
struct Notice {
    uint64_t number{};
    double time{};
    uint32_t flags{};
    char text[512]{};
    uint32_t color{0xffd9d9d9};
    int32_t sender{-1},voice{-1}; // Delivery facts survive loss of world visibility.
    // NoticeBallot uses sender for its stable ballot ID, voice for BallotKind,
    // color for the recipient's side, and time for creation. Its text is the
    // original proposal. Reuse the existing private reliable-notice history;
    // ballots add no periodic status fields or extra packet stream.
};
enum class EventKind : uint32_t { Explosion, HullHit, ShieldHit, Sound, Jiggle, Damage, Camera, Feedback };
enum SoundEventFlag : uint32_t {SoundImpact=1u,SoundOwnShip=2u};
struct Event {
    uint64_t number{};
    double time{};
    EventKind kind{};
    Identity model{};
    Vec position{}, velocity{}, normal{}, ellipse{};
    float radius{};
    int32_t value{}, sector{-1}, pilot{-1};
    uint32_t sides{}, flags{};
    uint32_t reserved{};
};
inline bool isSharedCue(const Event& e){return e.kind==EventKind::Sound && e.pilot== -1 && e.sector== -1 && !e.model.serial && !e.flags;}
struct SectorHighlight {int32_t sector{-1};};
struct Snapshot {
    uint64_t inputAck{}, match{};
    int32_t pilot{-1}, selectedWeapon{}, selectedCargo{-1}; // Pilot selection is one of four fixed hardpoints.
    int32_t moneySender{-1}, moneyAmount{}, moneyHull{-1};
    int32_t stage{2}, host{-1}, countdown{-1};
    uint32_t complete{}, dead{};
    uint32_t entities{}, parts{}, pilots{}, teams{}, buckets{}, notices{}, events{}, payloadBytes{},members{};
    uint32_t lobbyFlags{},autoAccept{},forceReady{},readySides{};
    char result[128]{};
    uint32_t highlights{},resultPlayers{},resultTeams{};
    int16_t winner{-1};uint16_t resultFlags{}; // 1 frozen result, 2 eject pods
    uint32_t shipIntel{},groupFire{1}; // Retain the selected hardpoint while grouped, without growing the record.
};
// A live rule change travels once on the ordered control lane. Shared bytes
// are prepared once for all recipients; periodic snapshots stay unchanged.
struct MissionPolicy {
    uint64_t match{};int32_t imbalance{1};
    std::array<char,6> alliances{-1,-1,-1,-1,-1,-1};
    uint8_t flags{}; // 1 defections, 2 allied sight, 4 allied ripcord
    uint8_t reserved{};uint32_t reservedTail{};
    bool operator==(const MissionPolicy&)const=default;
};
static_assert(sizeof(MissionPolicy)==24 && offsetof(MissionPolicy,reservedTail)+sizeof(MissionPolicy::reservedTail)==sizeof(MissionPolicy));
static_assert(sizeof(MissionSettings)==128 && offsetof(MissionSettings,maxImbalance)==offsetof(MissionSettings,reservedPolicy)+sizeof(MissionSettings::reservedPolicy));
static_assert(sizeof(Configuration)==504 && offsetof(Configuration,reserved)+sizeof(Configuration::reserved)==sizeof(Configuration));
static_assert(sizeof(Session)==484 && offsetof(Session,seed)==offsetof(Session,teamPolicy)+sizeof(Session::teamPolicy));
static_assert(sizeof(ServerInfo)==636 && offsetof(ServerInfo,instance)+sizeof(ServerInfo::instance)==sizeof(ServerInfo));
static_assert(sizeof(Snapshot)==264 && offsetof(Snapshot,groupFire)+sizeof(Snapshot::groupFire)==sizeof(Snapshot));
static_assert(offsetof(Welcome,match)==offsetof(Welcome,reservedSession)+sizeof(Welcome::reservedSession));
static_assert(offsetof(Welcome,token)==offsetof(Welcome,reservedRate)+sizeof(Welcome::reservedRate));
struct ClockProbe {
    int64_t client{}, serverReceive{}, serverSend{};
};
struct Delta {
    uint64_t baseline{}, size{};
    uint32_t copies{}, patchBytes{}; // zero selects a dense XOR block
};
// Relocate acknowledged bytes before XOR/LZ4. Insertions, removals and changes
// in visibility must not shift the delta basis of every subsequent object.
struct CopyRange {
    uint32_t source{}, destination{}, size{};
};
struct XorRange {
    uint32_t offset{}, size{};
};
struct Packed {
    Kind kind{};
    uint32_t size{};
};
struct Fragment {
    uint32_t size{}, offset{};
};
// Visibility domains have immutable revisions, independent of a recipient's
// report/input receipts. All fields are native compact, pointer-free records.
struct ArenaRecord { uint64_t match{}; uint32_t domain{}, reserved{}; };
struct ArenaRef { uint32_t domain{}, reserved{}; uint64_t revision{}; };
// Projectile anchors affect presentation only. Their loss must not hold up
// authoritative ship health, controls, economy or visibility changes.
inline constexpr uint32_t presentationArena = 1, stateOnlyArena = 2;
// Smaller stable partitions amortize envelopes and GNS message bookkeeping
// across the dispersed multi-sector roster without replaying unaffected shots.
inline constexpr size_t projectileArenaBuckets = 8;
// Faction, viewed sector, physical sector, public roster, prior-round results
// and faction feedback can accompany the viewed sector's trajectories.
inline constexpr size_t maxReportArenas = projectileArenaBuckets + 6;
struct ArenaManifest { uint32_t count{}, reserved{}; };
template <class T>
concept Record = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;
template <Record T> void append(std::vector<std::byte> &to, const T &value) {
    auto begin = to.size();
    to.resize(begin + sizeof(T));
    std::memcpy(to.data() + begin, &value, sizeof(T));
}
template <class T, size_t Extent>
    requires Record<std::remove_const_t<T>>
void append(std::vector<std::byte> &to, std::span<T, Extent> values) {
    auto begin = to.size();
    to.resize(begin + values.size_bytes());
    if (!values.empty())
        std::memcpy(to.data() + begin, values.data(), values.size_bytes());
}
class Reader {
    std::span<const std::byte> bytes_;

  public:
    explicit Reader(std::span<const std::byte> bytes) : bytes_(bytes) {}
    template <Record T> T take() {
        T result;
        if (bytes_.size() < sizeof(T))
            throw std::runtime_error("Truncated network record");
        std::memcpy(&result, bytes_.data(), sizeof(T));
        bytes_ = bytes_.subspan(sizeof(T));
        return result;
    }
    template <Record T> std::vector<T> array(size_t count) {
        if (count > maxEntities || count > bytes_.size() / sizeof(T))
            throw std::runtime_error("Invalid network array");
        std::vector<T> result(count);
        if (count)
            std::memcpy(result.data(), bytes_.data(), count * sizeof(T));
        bytes_ = bytes_.subspan(count * sizeof(T));
        return result;
    }
    std::span<const std::byte> rest() const { return bytes_; }
};
template <size_t N> void text(char (&to)[N], std::string_view from) {
    auto n = std::min(N - 1, from.size());
    std::memcpy(to, from.data(), n);
    to[n] = 0;
}
template <size_t N> bool terminated(const char (&s)[N]) { return std::memchr(s, 0, N) != nullptr; }
struct Frame {
    // Capture cursors/statistics only; these are never serialized.
    struct Effects {uint64_t notices{},events{};};
    Effects omitted{};
    // Capture-only DTO, never part of a wire record. Projectiles have no ship
    // controls, equipment or payload. Expand those default fields on the
    // publication worker, retaining their original place in the entity list.
    struct ProjectileCapture {
        uint32_t ordinal{};
        Identity identity{};
        uint32_t epoch{};
        uint64_t shot{};
        Motion motion{};
        float radius{},signature{},power{};
        uint32_t expiry{},activation{};
        int16_t side{-1},sector{-1},definition{-1},launcher{-1};
        Entity entity() const {
            Entity e{};
            e.identity=identity;e.epoch=epoch;e.shot=shot;e.motion=motion;
            e.radius=radius;e.signature=signature;e.power=power;
            e.expiry=expiry;e.activation=activation;
            e.side=side;e.sector=sector;e.definition=definition;e.launcher=launcher;
            e.flags=Eye|Known|Trajectory;
            return e;
        }
    };
    static_assert(std::is_trivially_copyable_v<ProjectileCapture>);
    static_assert(sizeof(ProjectileCapture)<sizeof(Entity)/2);
    Header header{};
    Snapshot status{};
    std::vector<Entity> entities;
    std::vector<Part> parts;
    std::vector<Pilot> pilots;
    std::vector<Member> members;
    std::vector<ShipIntel> shipIntel;
    std::vector<Team> teams;
    std::vector<Bucket> buckets;
    std::vector<Notice> notices;
    std::vector<Event> events;
    std::vector<SectorHighlight> highlights;
    std::vector<ResultPlayer> resultPlayers;std::vector<ResultTeam> resultTeams;
    std::vector<std::byte> payload;
    std::vector<ProjectileCapture> capturedProjectiles;
    void materialize() {
        if(capturedProjectiles.empty())return;
        size_t plain=entities.size(),shot=capturedProjectiles.size();
        entities.resize(plain+shot);
        // Backward merge safely reuses the frame's contiguous buffer. Payload
        // and equipment order are unchanged because projectiles have neither.
        for(size_t at=entities.size();at--;) {
            if(shot && capturedProjectiles[shot-1].ordinal==at)
                entities[at]=capturedProjectiles[--shot].entity();
            else {if(!plain)throw std::logic_error("Invalid captured trajectory order");entities[at]=entities[--plain];}
        }
        capturedProjectiles.clear();
    }
    void clear() {
        header={};status={};omitted={};entities.clear();parts.clear();pilots.clear();members.clear();shipIntel.clear();teams.clear();
        buckets.clear();notices.clear();events.clear();highlights.clear();resultPlayers.clear();resultTeams.clear();payload.clear();capturedProjectiles.clear();
    }
};
struct ReportCapture {
    struct World {uint32_t domain{};std::shared_ptr<const Frame> frame;uint32_t flags{};};
    std::vector<World> worlds;
    Frame personal;
};
Session session(const SessionOptions &);
SessionOptions options(const Session &, int side, std::string_view name);
std::vector<std::byte> packet(Frame &);
Frame composeReport(const Frame&,Frame);
Frame combineWorlds(std::span<const Frame* const>);
Frame frame(std::span<const std::byte>);
Digest buildDigest();
Digest contentDigest(const std::filesystem::path &);
int64_t now();
} // namespace Native::Net
