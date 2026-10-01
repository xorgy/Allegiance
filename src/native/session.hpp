#pragma once
#include <array>
#include <string>
#include <cstdint>
#include <vector>
#include <algorithm>
namespace Native {
inline constexpr int shipIdentityCapacity=32768; // Original nonnegative signed-short IDs.
inline constexpr int noTeam=-2, noTeamRequest=-1;
enum class LobbyAction {ForceReady,AutoAccept,LockLobby,LockSides,Accept,Reject,Boot,CancelRequest,TeamSettings,Randomize,FlightBoot,Alliance,TeamActive};
enum class Scenario { Practice,FlightTraining,WeaponsTraining,FreeFlight,Skirmish,Conquest };
inline constexpr const char* sessionCoreFile(Scenario scenario) {
    return scenario==Scenario::Conquest?"static_core":"PCore014";
}
struct MissionSettings {
    bool operator==(const MissionSettings&)const=default;
    // Original territory sentinel is 100 (disabled), unlike the other goals.
    int conquest{100},territory{100},flags{},artifacts{},teamKills{},minutes{};
    float prosperity{},startingMoney{1},helium{1};
    int startCountdown{15};
    int miners{1},lives{32767},teams{2},maxDrones{4},mapSize{},randomEncounters{};bool friendlyFire{};
    bool ejectPods{true},invulnerableStations{},showMap{},showHome{},developments{true},powerUps{true},alliedSight{},alliedRipcord{};
    bool shipyard{true},tactical{true},supremacy{true},expansion{true},randomWormholes{true},resourceAmounts{true},experimental{};
    int homeAsteroids{20},neutralAsteroids{20},homeMineable{2},neutralMineable{4},homeSpecial{1},neutralSpecial{1};
    int homeTreasures{},neutralTreasures{5};float homeTreasureRate{.2f},neutralTreasureRate{.5f}; // per minute
    bool allowJoiners{true},allowDefections{true},lockTeamSettings{};uint8_t reservedPolicy{};int maxImbalance{1};
};
struct LocalPilot {std::string name;int team{};bool ready{};bool operator==(const LocalPilot&)const=default;};
struct SessionOptions {
    bool operator==(const SessionOptions&)const=default;
    std::string story; // Authority-owned briefing; never accepted from client configuration.
    Scenario scenario{Scenario::Practice};
    std::string pilot{"Cadet"};
    std::string name; // Empty retains the scenario's name; classic Create Game supplies this.
    std::string coreFile; // Empty uses the original scenario/training default.
    std::string customMap; // Catalogued IGC basename; empty selects a generated map.
    int map{},hull{},enemies{1},allies{},difficulty{1},killLimit{};
    bool startInHangar{};
    float observerBattleDepth{}; // Diagnostic only: opposing spawn planes, independent of the docked observer.
    bool variedWingmen{}; // Fleet diagnostic: cycle combat hulls instead of a single interceptor type.
    int trainingMission{}; // original authored mission ID (2,3,4,5,6,8,10); zero uses scenario
    int civilization{},team{};MissionSettings rules;uint32_t seed{1};
    std::array<int,6> teamCivilizations{-1,-1,-1,-1,-1,-1}; // -1 uses lobby default
    std::array<std::string,6> teamNames{}; // Empty keeps the core's default name; original limit is 24 bytes.
    std::vector<LocalPilot> localPilots; // additional offline seats, separate from AI drones
    std::array<char,6> alliances{-1,-1,-1,-1,-1,-1}; // original alliance group IDs
    uint8_t inactiveTeams{};bool requireTeamPlayers{}; // Original empty-team checkbox policy; untouched native scenarios allow empty teams.
};
inline const char* sessionCoreFile(const SessionOptions& session){return session.coreFile.empty()?sessionCoreFile(session.scenario):session.coreFile.c_str();}
inline std::string sessionLoadoutFile(const SessionOptions& session){return std::string("loadouts_")+sessionCoreFile(session)+".dat";}
inline int teamCivilizationIndex(const SessionOptions& options,int side,int count){
    if(count<=0 || side<0 || side>=int(options.teamCivilizations.size()))return -1;
    int index=options.teamCivilizations[side];
    if(index>=0)return index<count?index:-1;
    int64_t relative=int64_t(std::max(0,options.civilization))+side-options.team;
    return int((relative%count+count)%count);
}
struct ScenarioInfo {const char* name;const char* description;int map,enemies,allies,killLimit;};
inline int64_t initialShipCount(const SessionOptions& session) {
    return int64_t(std::min<size_t>(session.localPilots.size(),shipIdentityCapacity))+1+
        (session.scenario==Scenario::Conquest?int64_t(session.rules.teams)*session.rules.miners:int64_t(session.enemies)+session.allies);
}
inline constexpr std::array<ScenarioInfo,6> scenarios{{
    {"Combat practice","An open-ended practice session against an enemy wingman. Respawn and change ships freely.",0,1,0,0},
    {"Flight training","Guided native objectives: accelerate, turn, strafe, use vector lock, and return to a friendly station.",0,0,0,0},
    {"Weapons training","Guided native objectives: select a target, fire your guns, launch a missile, then destroy a training drone.",0,1,0,1},
    {"Free flight","Explore the original training sectors, use alephs, and dock at friendly stations without combat drones.",3,0,0,0},
    {"Team skirmish","Set up a local battle with allied and enemy wingmen. The first team to reach the kill limit wins.",3,3,2,10},
    {"Conquest","Command a civilization on an original generated map. Mine He3, fund research and constructors, and capture or destroy the opposing bases.",0,0,0,0}
}};
inline std::string sessionName(const SessionOptions& session){return session.name.empty()?scenarios[size_t(session.scenario)].name:session.name;}
struct TrainingInfo {int id;const char* name;const char* description;};
inline constexpr std::array<TrainingInfo,7> originalTraining{{
    {2,"Basic flight","Original narrated flight controls and HUD tutorial."},
    {3,"Basic weaponry","Original targeting, guns, missiles and equipment tutorial."},
    {4,"Enemy engagement","Original combat training and tactical exercises."},
    {5,"Command view","Original commander, navigation and unit-order tutorial."},
    {6,"Practice arena","Original authored practice arena and respawning opponents."},
    {8,"Nanite repair","Original repair-ship training mission."},
    {10,"Free flight","Original authored free-flight mission."}
}};
inline constexpr std::array<const char*,4> mapFiles{"training_1","training_3","training_5","training_6"};
// The original four indices remain stable for existing native configurations.
inline constexpr std::array<const char*,15> conquestMapNames{"Single ring","Double ring","HiLo","Diamond ring","Pinwheel","Snowflake","Split base","Brawl","Big ring","HiHigher","Star","Inside out","Grid","East west","Large split"};
inline constexpr std::array<short,15> conquestMapIds{0,1,8,3,2,4,5,6,7,9,10,11,12,13,14};
inline constexpr std::array<const char*,4> mapNames{"Neptune training sector","Stealth combat sectors","Command training sectors","Practice arena sectors"};
inline constexpr std::array<const char*,6> hullNames{"Enhanced fighter","Interceptor","Advanced stealth fighter","Scout / variant 1","Scout / variant 2","Scout / variant 3"};
inline constexpr std::array<short,6> hullIds{414,412,486,210,310,410};
inline constexpr std::array<short,4> battleHullIds{412,414,486,410};
inline constexpr float battleSpawnSpacing=90.f;
}
