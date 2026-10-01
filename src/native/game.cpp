#include "game.hpp"
#include "../ClientCore/MissionBase.h"
#include "../WinTrek/mappvmaker.h"
#include "object_pool.hpp"
#include "ship_notifications.hpp"
#include "../Igc/magazineigc.h"
#include "../Igc/projectileIGC.h"
#include "../Igc/clusterIGC.h"
#include "../Igc/missileigc.h"
#include "../Igc/weaponIGC.h"
#include "../Igc/mineigc.h"
#include "../Igc/asteroidigc.h"
#include "../Igc/warpIGC.h"
#include "scanner_index.hpp"
#include "chat_censor.hpp"
#include "../ClientCore/MissionChatCommands.h"
#include "../ClientCore/ObservedCredits.h"
#include "ui_clock.hpp"
#include "training_host.hpp"
#undef trekClient
#include "audio.hpp"
#include "../training/Training.h"
#include "../training/Clock.h"
#include "../training/CommandAcknowledgedCondition.h"
#include "training_sound_ids.hpp"
#include "spatial.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <unordered_set>
#include <unordered_map>
#include <stdexcept>
#include <cstdarg>
#include <bitset>
#include "legacy.hpp"
#include "igc.h"
#include "../ClientCore/QuickChat.h"
#include "../ClientCore/LobbyActions.h"
#include "../ClientCore/GameListPresentation.h"
#include "../ClientCore/CameraMotion.h"
#include "../ClientCore/CameraFraming.h"
#include "../ClientCore/CameraZoom.h"
#include "../ClientCore/ShipSound.h"
#include "../ClientCore/ClientSoundEvents.h"
#include "../ClientCore/CombatActions.h"
#include "../ClientCore/Inventory.h"
#include "../ClientCore/InventoryHud.h"
#include "../ClientCore/InventoryRows.h"
#include "../ClientCore/InvestmentPresentation.h"
#include "../ClientCore/SectorMapPresentation.h"
#include "../ClientCore/WarpTransit.h"
#include "../ClientCore/Visibility.h"
#include "../ClientCore/ShipLifecycle.h"
#include "../ClientCore/FlightFeedback.h"
#include "../ClientCore/CombatFeedback.h"
#include "../ClientCore/ShipCrew.h"
#include "../ClientCore/CrewPresentation.h"
#include "../ClientCore/GameStatePresentation.h"
#include "../ClientCore/StationTeleport.h"
#include "../ClientCore/StationEvacuation.h"
#include "../ClientCore/SideElimination.h"
#include "../ClientCore/ParticleEffects.h"
#include "../ClientCore/ShieldFlare.h"
#include "../ClientCore/Strobe.h"
#include "../ClientCore/StarField.h"
#include "../ClientCore/CommandPicking.h"
#include "../ClientCore/AlephEffects.h"
#include "../ClientCore/SurfaceEffects.h"
#include "../ClientCore/MissionEconomy.h"
#include "../ClientCore/PlayerEconomy.h"
#include "../ClientCore/PlayerEconomyPresentation.h"
#include "../ClientCore/Loadout.h"
#include "../ClientCore/LoadoutPresentation.h"
#include "../ClientCore/CarrierLoadout.h"
#include "../ClientCore/MissionRules.h"
#include "../ClientCore/WeaponIndicator.h"
#include "../ClientCore/RadarModel.h"
#include "../ClientCore/PlayerCommands.h"
#include "../ClientCore/WeaponSelection.h"
#include "../ClientCore/EquipmentStatus.h"
#include "../ClientCore/MissionLifecycle.h"
#include "../ClientCore/Targeting.h"
#include "../ClientCore/FlightControls.h"
#include "../ClientCore/ShipOrders.h"
#include "../ClientCore/FlagLifecycle.h"
#include "../ClientCore/ClusterWarnings.h"
#include "../ClientCore/Alliances.h"
#include "../ClientCore/LobbyAlliances.h"
#include "../ClientCore/ChatRouting.h"
#include "../ClientCore/ChatPresentation.h"
#include "../ClientCore/QueuedRequests.h"
#include <fstream>

namespace Native {
static_assert(builderPilot==c_ptBuilder);
static_assert(firstSectorThreat==c_cwMinerThreatened);
static_assert(playerPilot==c_ptPlayer);
static_assert(ContactShip==OT_ship && ContactStation==OT_station && ContactMine==OT_mine &&
    ContactMissile==OT_missile && ContactProbe==OT_probe && ContactAsteroid==OT_asteroid &&
    ContactWarp==OT_warp && ContactTreasure==OT_treasure);
static_assert(int(PartKind::Chaff)==ET_ChaffLauncher && int(PartKind::Weapon)==ET_Weapon &&
    int(PartKind::Missile)==ET_Magazine && int(PartKind::Dispenser)==ET_Dispenser &&
    int(PartKind::Shield)==ET_Shield && int(PartKind::Cloak)==ET_Cloak &&
    int(PartKind::Pack)==ET_Pack && int(PartKind::Booster)==ET_Afterburner);
namespace {
glm::vec3 vec(const Vector& v){return {v.x,v.y,v.z};}
Vector vec(glm::vec3 v){return {v.x,v.y,v.z};}
glm::mat4 matrix(const Orientation& o,const Vector& pos,float scale,bool flip=false) {
    return modelTransform(vec(o.GetRight()),vec(o.GetUp()),vec(o.GetBackward()),vec(pos),scale,flip);
}
}
namespace {
static_assert(Allegiance::MapFileCapacity==c_cbFileName);
MissionParams nativeMissionParams(const SessionOptions& options,Time start) {
    MissionParams params;params.nTeams=options.scenario==Scenario::Conquest?short(options.rules.teams):6;
    std::strcpy(params.szIGCStaticFile,sessionCoreFile(options));
    params.bAllowJoiners=options.rules.allowJoiners;params.bAllowDefections=options.rules.allowDefections;params.iMaxImbalance=short(options.rules.maxImbalance);
    params.bLockTeamSettings=options.rules.lockTeamSettings;
    if(std::any_of(options.alliances.begin(),options.alliances.begin()+std::clamp(options.rules.teams,0,6),[](char alliance){return alliance>=0;})){params.bAllowDefections=true;params.iMaxImbalance=32767;}
    params.bAllowEmptyTeams=!options.requireTeamPlayers;params.bInvulnerableStations=options.scenario!=Scenario::Conquest;params.bEjectPods=true;
    params.timeStart=start;params.iLives=short(options.rules.lives);
    if(options.scenario!=Scenario::Conquest)return params;
    auto& r=options.rules;
    params.mmMapType=options.customMap.empty()?conquestMapIds[size_t(options.map)]:0;
    if(options.customMap.size()<sizeof(params.szCustomMapFile))std::strcpy(params.szCustomMapFile,options.customMap.c_str());
    params.iGoalConquestPercentage=short(r.conquest);params.iGoalTerritoryPercentage=short(r.territory);
    params.nGoalFlagsCount=short(r.flags);params.nGoalArtifactsCount=short(r.artifacts);params.nGoalTeamKills=short(r.teamKills);
    params.dtGameLength=r.minutes*60.f;params.fGoalTeamMoney=r.prosperity;params.fStartingMoney=r.startingMoney;params.fHe3Density=r.helium;
    params.fStartCountdown=float(r.startCountdown);
    params.nInitialMinersPerTeam=char(r.miners);params.nMaxDronesPerTeam=char(r.maxDrones);params.bAllowFriendlyFire=r.friendlyFire;
    params.iMapSize=short(r.mapSize);params.iRandomEncounters=short(r.randomEncounters);
    params.bEjectPods=r.ejectPods;params.bInvulnerableStations=r.invulnerableStations;params.bShowMap=r.showMap;params.bShowHomeSector=r.showHome;
    params.bAllowDevelopments=r.developments;params.bPowerUps=r.powerUps;params.bAllowAlliedViz=r.alliedSight;params.bAllowAlliedRip=r.alliedRipcord;
    params.bAllowShipyardPath=r.shipyard;params.bAllowTacticalPath=r.tactical;params.bAllowSupremacyPath=r.supremacy;params.bAllowExpansionPath=r.expansion;
    params.bRandomWormholes=r.randomWormholes;params.bResourceAmountsVisible=r.resourceAmounts;
    params.bExperimental=r.experimental;
    params.nPlayerSectorAsteroids=short(r.homeAsteroids);params.nNeutralSectorAsteroids=short(r.neutralAsteroids);
    params.nPlayerSectorMineableAsteroids=short(r.homeMineable);params.nNeutralSectorMineableAsteroids=short(r.neutralMineable);
    params.nPlayerSectorSpecialAsteroids=short(r.homeSpecial);params.nNeutralSectorSpecialAsteroids=short(r.neutralSpecial);
    params.nPlayerSectorTreasures=short(r.homeTreasures);params.nNeutralSectorTreasures=short(r.neutralTreasures);
    params.nPlayerSectorTreasureRate=r.homeTreasureRate/60;params.nNeutralSectorTreasureRate=r.neutralTreasureRate/60;
    return params;
}
}
std::string_view Game::wingName(int wing) {return wing>=0 && wing<c_widMax?c_pszWingName[wing]:"";}
bool Game::validMap(int index,int teams) {
    if(index<0 || index>=int(conquestMapIds.size()) || teams<2 || teams>6)return false;
    MissionParams params;params.nTeams=short(teams);params.mmMapType=conquestMapIds[size_t(index)];
    return ImapMakerIGC::IsValid(&params)==nullptr;
}
SetupMapPreview Game::mapPreview(const SessionOptions& options,const std::filesystem::path& artwork) {
    SetupMapPreview out;
    if(options.map<0 || options.map>=int(conquestMapIds.size()) || options.rules.teams<2 || options.rules.teams>6){out.error="Invalid map or team count.";return out;}
    if(!Allegiance::ValidMapFile(options.coreFile)){out.error="Invalid game core filename.";return out;}
    if(!Allegiance::ValidMapFile(options.customMap)){out.error="Invalid custom map filename.";return out;}
    if(!artwork.empty())UTL::SetArtPath(artwork.string().c_str());
    auto params=nativeMissionParams(options,Time::Now());ImissionPV preview(nullptr);
    if(auto reason=ImapPVMaker::IsValid(&params)){out.error=reason;return out;}
    if(auto reason=ImapPVMaker::Create(&params,&preview)){out.error=reason;return out;}
    for(auto* link=preview.GetClusters()->first();link;link=link->next()){
        auto* sector=link->data();SetupMapPreview::Sector s{sector->GetObjectID(),{sector->GetScreenX(),sector->GetScreenY()},{}};
        if(!options.customMap.empty())s.name.assign(sector->GetName(),std::find(sector->GetName(),sector->GetName()+c_cbName,'\0'));
        std::copy_n(sector->GetStationsPerSide(),s.stations.size(),s.stations.begin());out.sectors.push_back(s);
    }
    for(auto* link=preview.GetWarps()->first();link;link=link->next()){
        auto* warp=link->data();auto* destination=warp->GetDestination();
        if(destination && warp->GetObjectID()>destination->GetObjectID())out.links.push_back({warp->GetCluster()->GetObjectID(),destination->GetCluster()->GetObjectID()});
    }
    return out;
}
std::array<char,6> Game::effectiveAlliances(const SessionOptions& options) {
    auto current=options.alliances;std::array<char,6> result;
    int teams=std::clamp(options.rules.teams,2,6);for(int i=teams;i<6;++i)current[i]=NA;
    Allegiance::UpdateAlliances(current.data(),short(teams),NA,NA,[&](SideID side){return !(options.inactiveTeams&(1u<<side));},result.data());return result;
}
std::vector<Allegiance::CustomMapInfo> Game::customMaps(const std::filesystem::path& artwork){
    auto path=artwork/"maps.txt";
    if(!std::filesystem::exists(path)){
        for(const auto& entry:std::filesystem::directory_iterator(artwork))if(entry.is_regular_file()){
            auto name=entry.path().filename().string();std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return std::tolower(c);});
            if(name=="maps.txt"){path=entry.path();break;}
        }
    }
    if(!std::filesystem::exists(path))return {};
    std::ifstream input(path);if(!input)throw std::runtime_error("Cannot read maps.txt");return Allegiance::ReadCustomMaps(input);
}
std::string Game::missionError(const SessionOptions& options,std::span<const Allegiance::CustomMapInfo> maps,bool fromClient) {
    if(!Allegiance::ValidMapFile(options.coreFile))return "Invalid game core filename.";
    if((options.trainingMission || options.scenario!=Scenario::Conquest) && !options.coreFile.empty() && !Allegiance::CoreFileEqual(options.coreFile,sessionCoreFile(options.scenario)))return "Training and practice scenarios use their original core.";
    if(options.name.size()>=Allegiance::GameNameCapacity || std::any_of(options.name.begin(),options.name.end(),[](unsigned char c){return c<32 || c==127;}))return "The game name must contain at most 64 printable bytes.";
    if(options.enemies<0 || options.allies<0 || options.enemies>=shipIdentityCapacity || options.allies>=shipIdentityCapacity ||
       options.localPilots.size()>=shipIdentityCapacity || initialShipCount(options)>shipIdentityCapacity)
        return "The requested ships exceed the original ship identity capacity.";
    if(!Allegiance::ValidMapFile(options.customMap))return "Invalid custom map filename.";
    if(options.scenario!=Scenario::Conquest)return options.customMap.empty()?"":"Custom maps require a conquest mission.";
    auto& r=options.rules;
    if(options.inactiveTeams&~0x3fu)return "Invalid team activity policy.";
    if(options.map<0 || options.map>=int(conquestMapIds.size()) || r.teams<2 || r.teams>6 || options.team<0 || options.team>=r.teams)return "Choose a valid map and team.";
    if(options.inactiveTeams&(1u<<options.team))return "The player's team must be active.";
    if(!options.customMap.empty()){
        auto map=Allegiance::FindCustomMap(maps,options.customMap);if(!map)return "Unknown custom map.";
        if(map->teams!=r.teams)return "The map type '"+map->name+"' can only be played with "+std::to_string(map->teams)+" teams";
    }
    for(int i=0;i<std::min(r.teams,6);++i)if(options.teamCivilizations[i]<-1 || options.teamCivilizations[i]>=32767 || options.alliances[i]<-1 || options.alliances[i]>=c_cAlliancesMax)return "Choose a valid team civilization and alliance.";
    for(const auto& name:options.teamNames)if(name.size()>24 || std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32 || c==127;}))return "Team names can contain up to 24 bytes of printable text.";
    if(!std::isfinite(r.prosperity) || !std::isfinite(r.startingMoney) || !std::isfinite(r.helium) || !std::isfinite(r.homeTreasureRate) || !std::isfinite(r.neutralTreasureRate))return "Mission values must be finite.";
    for(auto& pilot:options.localPilots)if(pilot.team<0 || pilot.team>=r.teams || pilot.name.empty() || pilot.name.size()>23 ||
        std::any_of(pilot.name.begin(),pilot.name.end(),[](unsigned char c){return c<32 || c==127;}))return "Each local pilot needs a valid name and team.";
    for(auto& pilot:options.localPilots)if(options.inactiveTeams&(1u<<pilot.team))return "A team with pilots cannot be inactive.";
    if(r.startCountdown<0 || r.startCountdown>60)return "Start countdown must be between zero and sixty seconds.";
    if(r.teams<2 || r.teams>6 || r.miners<0 || r.miners>10 || r.maxDrones<0 || r.maxDrones>10 || r.lives<0 || r.lives>32767 ||
       r.minutes<0 || r.minutes>1440 || r.teamKills<0 || r.teamKills>32767 || r.mapSize<0 || r.mapSize>1 || r.randomEncounters<0 || r.randomEncounters>10)
        return "Mission counts are outside the supported range.";
    if(r.conquest<0 || r.conquest>100 || r.territory<0 || r.territory>100 || r.flags<0 || r.flags>100 || r.artifacts<0 || r.artifacts>100 ||
       r.homeAsteroids<0 || r.homeAsteroids>40 || r.neutralAsteroids<0 || r.neutralAsteroids>40 ||
       r.homeMineable<0 || r.homeMineable>9 || r.neutralMineable<0 || r.neutralMineable>9 ||
       r.homeSpecial<0 || r.homeSpecial>9 || r.neutralSpecial<0 || r.neutralSpecial>9 ||
       r.homeTreasures<0 || r.homeTreasures>20 || r.neutralTreasures<0 || r.neutralTreasures>20)return "Objective or resource counts are outside the supported range.";
    if(!Allegiance::ValidTeamImbalance(r.maxImbalance))return "Invalid team imbalance.";
    if(r.homeTreasureRate<0 || r.neutralTreasureRate<0)return "Treasure rates cannot be negative.";
    auto params=nativeMissionParams(options,Time::Now());
    if(fromClient)if(auto* error=Allegiance::MissionClientFlagsError(params.IsNoWinningConditionsGame(),params.bAutoRestart,params.bObjectModelCreated && params.bClubGame))return error;
    if(auto* error=params.Invalid())return error;return {};
}
struct Game::Impl : IIgcSite {
    std::array<std::shared_ptr<const Allegiance::MissionBriefing>,6> briefings;
    std::shared_ptr<const Allegiance::MissionBriefing> storyBriefing;
    int briefingSide{-2};
    std::shared_ptr<const Allegiance::MissionBriefing> briefingForSide(int side){
        if(side<0 || side>=options.rules.teams || side>=int(briefings.size()))return {};
        if(gameRole==GameRole::Replica)return briefings[side];
        if(!options.story.empty()){
            if(!storyBriefing)storyBriefing=std::make_shared<const Allegiance::MissionBriefing>(Allegiance::MissionBriefing{options.story,false});
            return storyBriefing;
        }
        auto* base=Allegiance::MissionRestartBase(mission->GetSide(SideID(side)),c_sabmRestart);
        const char* home=base && base->GetCluster()?base->GetCluster()->GetName():"";
        auto& briefing=briefings[side];
        if(!briefing || briefing->text!=home)briefing=std::make_shared<const Allegiance::MissionBriefing>(Allegiance::MissionBriefing{home,true});
        return briefing;
    }
    void refreshBriefing(int side){
        if(gameRole==GameRole::Authority)return; // No presentation work in authority pilot captures.
        if(side<0 || side>=int(briefings.size())){briefingSide=side;state.briefing.reset();return;}
        if(briefingSide!=side && gameRole!=GameRole::Replica)briefings[side]=briefingForSide(side);
        briefingSide=side;if(state.briefing!=briefings[side])state.briefing=briefings[side];
    }
#include "endgame_game.inl"
    struct Icon : IObject {std::string name;explicit Icon(const char* n):name(std::string(n)+"bmp"){}};
    std::unordered_map<std::string,TRef<Icon>> icons;
    IObject* LoadRadarIcon(const char* name)override {
        if(uiPreview)return nullptr;
        if(!name || !*name)return nullptr;
        auto [it,inserted]=icons.try_emplace(name);if(inserted)it->second=new Icon(name);return it->second;
    }
    struct Intel {SideID side{NA};float hull{1},shield{};};
    struct StationObservation {
        DataStationIGC record{};uint32_t networkIdentity{};Intel intel;ModelHandle handle;float radius{1};bool known{},hasModel{},flip{},unlit{},pedestal{};
        glm::vec4 color{1};std::string name,type,icon;
    };
    using StationHistory=std::array<StationObservation,c_cSidesMax>;
    std::unordered_map<StationID,StationHistory> stationHistory;
    GameRole gameRole{GameRole::Offline};bool networkReplaying{};
    std::unordered_map<uint64_t,ImodelIGC*> shots;
    std::unordered_set<uint64_t> confirmedShots;
    struct PresentationPose {TRef<ImodelIGC> model;Vector position,velocity;Orientation orientation;};
    std::vector<PresentationPose> presentationPoses;
    double networkRenderTime{};bool networkRenderTimeValid{};
    double cameraClock()const{return gameRole==GameRole::Replica && networkRenderTimeValid?networkRenderTime:elapsed;}
    struct ObservedRecord {Net::Entity entity;std::vector<std::byte> payload;};
    std::array<std::unordered_map<uint32_t,ObservedRecord>,c_cSidesMax> observedRecords;
    std::unordered_map<int,PlayerPaneRow> playerPaneMemory;int playerPaneSide{-1};
    std::array<std::unordered_map<ShipID,Allegiance::ObservedCreditTotal>,c_cSidesMax> alliedCreditTotals;
    std::unordered_map<ShipID,uint32_t> creditIdentities;uint32_t nextCreditIdentity{};
    Allegiance::CreditObservations observedCredits;
    std::array<std::unordered_map<ShipID,Net::ShipIntel>,c_cSidesMax> shipIntel;
    const Net::ShipIntel* ownShipStatus(int id)const{
        if(!onTeam() || id<0 || id>=shipIdentityCapacity)return nullptr;
        int side=player->GetSide()->GetObjectID();const auto& records=shipIntel[side];auto found=records.find(ShipID(id));
        if(found==records.end() || !found->second.serial || found->second.side!=side ||
           found->second.pilotType<c_ptPlayer || !found->second.status.current())return nullptr;
        return &found->second;
    }
    const Net::ShipIntel* ownMemberStatus(const LobbyPilot& member)const{
        return onTeam() && member.team==player->GetSide()->GetObjectID()?ownShipStatus(member.id):nullptr;
    }
    template<class Emit> void reportedGunners(Emit emit)const{
        if(!onTeam())return;
        int side=player->GetSide()->GetObjectID();
        for(const auto& [id,intel]:shipIntel[side])if(intel.serial && intel.side==side && intel.pilotType>=c_ptPlayer &&
           intel.status.current() && intel.status.state==Allegiance::PlayerShipStatus::Turret && intel.status.parent>=0)emit(intel.status.parent);
    }
    // Ship state survives hull/crew reloads, which replace the ThingSite.
    // Values contain no model pointers and retire with the IGC ship identity.
    struct ShipPaneStatus {uint32_t localIdentity{},replicaIdentity{};double dockedAt{};};
    std::unordered_map<ShipID,ShipPaneStatus> shipPaneStatuses;
    ShipPaneStatus& shipPaneStatus(IshipIGC* ship) {
        auto serial=identity(ship);auto& status=shipPaneStatuses[ship->GetObjectID()];
        if(status.localIdentity!=serial)status={serial,0,0};return status;
    }
    uint32_t playerPaneIdentity(IshipIGC* ship)const {
        auto status=shipPaneStatuses.find(ship->GetObjectID());
        if(status!=shipPaneStatuses.end())return status->second.replicaIdentity?status->second.replicaIdentity:status->second.localIdentity;
        auto serial=networkIdentities.find(ship);return serial==networkIdentities.end()?0:serial->second;
    }
    void playerPaneViewer();
    void observeShipIntel(IshipIGC* ship,IsideIGC* viewer);
    bool shipIntelEye(IshipIGC* ship,IsideIGC* viewer)const;
    void hideShipIntel(IshipIGC* ship,IsideIGC* viewer);
    void updateShipIntel(IshipIGC* ship,bool docking=false);
    PlayerPaneRow playerPaneRow(const Net::ShipIntel& intel)const;
    PlayerPaneRow playerPaneRow(IshipIGC* ship,bool current)const;
    void observePlayerPaneShip(IshipIGC* ship);
    std::unordered_map<uint32_t,std::shared_ptr<const Net::Frame>> publicationCache;
    std::array<Net::Tick,c_cSidesMax> publicationMapCycles{};
    std::unordered_map<int,uint32_t> lifeSerial;
    std::unordered_map<uint32_t,uint32_t> slotShots;
    int NativePredictionMode()const override{return gameRole==GameRole::Offline?0:gameRole==GameRole::Authority?1:networkReplaying?3:2;}
    uint64_t NativeShot(IshipIGC* ship,Mount mount,uint32_t count)override{return (uint64_t(uint16_t(ship->GetObjectID()))<<48)|(uint64_t(lifeSerial[ship->GetObjectID()]&65535)<<32)|(uint64_t(uint8_t(mount))<<24)|(count&0xffffff);}
    bool NativeShotExists(uint64_t id)const override{return shots.contains(id);}
    void NativeProjectileCreated(ImodelIGC* model,uint64_t id)override{shots[id]=model;if(model->GetObjectType()==OT_projectile)static_cast<CprojectileIGC*>(model)->NativeSetShot(id);else if(model->GetObjectType()==OT_missile)static_cast<CmissileIGC*>(model)->NativeSetShot(id);}

    bool ownsShot(ImodelIGC* model)const {if(!player)return false;auto* source=player->GetSourceShip();uint64_t key=0;ImodelIGC* launcher=nullptr;if(model->GetObjectType()==OT_projectile){auto* p=static_cast<CprojectileIGC*>(model);key=p->NativeShot();launcher=p->GetLauncher();}else if(model->GetObjectType()==OT_missile){auto* m=static_cast<CmissileIGC*>(model);key=m->NativeShot();launcher=m->GetLauncher();}return launcher==player || launcher==source || (key && uint16_t(key>>48)==uint16_t(source->GetObjectID()));}
    std::function<bool(const Net::Action&)> actionSink;
    bool submit(Net::Operation op,std::array<int32_t,4> args={},glm::vec3 position={},std::string_view text={},std::span<const LoadoutPart> parts={}) {
        if(gameRole!=GameRole::Replica || !actionSink)return false;
        Net::Action action{};action.op=op;action.args=args;action.position={position.x,position.y,position.z};Net::text(action.text,text);
        if(parts.size()>action.parts.size())return false;action.count=parts.size();for(size_t i=0;i<parts.size();++i)action.parts[i]={parts[i].id,parts[i].mount,parts[i].amount};return actionSink(action);
    }
    struct Actor {bool commandView{};IclusterIGC* commandSector{};TRef<ImodelIGC> target;Mount weapon{},cargo{-1};bool group{true},axesInitialized{},dead{};std::array<float,3> axes{};unsigned actions{};};
    std::unordered_map<int,Actor> actors;
    std::vector<std::pair<int,FlightInput>> authorityInputs;
    std::unordered_map<uint32_t,TRef<ImodelIGC>> replicas;
    const std::unordered_set<ImodelIGC*>* replicaRetiring{}; // scoped to the import retirement pass
    std::unordered_map<ImodelIGC*,uint32_t> networkIdentities;
    uint32_t nextIdentity{},nextNetworkEpoch{};uint64_t lastNotice{},eventSerial{},lastNetworkEvent{};
    std::deque<Net::Event> networkEvents;
    struct CachedProjectile {Net::Frame::ProjectileCapture record{};uint64_t revision{};int16_t lastSector{-2};};
    std::vector<CachedProjectile> capturedProjectiles;
    std::vector<uint32_t> freeProjectileCaptures;


    struct CapturedPart {IpartIGC* model;int16_t id;Mount mount;EquipmentType equipment;};
    struct PartCapturePlan {uint64_t revision{};std::vector<CapturedPart> entries;};
    struct AddressedNotice {int pilot;Net::Notice notice;uint32_t sides{};};
    std::deque<AddressedNotice> networkNotices;
    bool publicationEffectsIndexed{};
    std::unordered_map<int,std::vector<const Net::Notice*>> publicationNotices;
    std::array<std::vector<const Net::Notice*>,c_cSidesMax+1> publicationCommonNotices;
    uint64_t feedbackRevision{};
    std::unordered_map<int,std::vector<const Net::Event*>> publicationPrivateEvents;
    std::array<std::vector<const Net::Event*>,c_cSidesMax> publicationCommonEvents;
    void indexPublicationEffects() {
        if(publicationEffectsIndexed)return;
        publicationNotices.clear();publicationPrivateEvents.clear();
        for(auto& side:publicationCommonNotices)side.clear();
        for(auto& side:publicationCommonEvents)side.clear();
        for(const auto& notice:networkNotices) {
            if(notice.sides){for(int side=0;side<=c_cSidesMax;++side)if(notice.sides&(1u<<side))publicationCommonNotices[side].push_back(&notice.notice);}
            else publicationNotices[notice.pilot].push_back(&notice.notice);
        }
        for(const auto& event:networkEvents) {
            if(event.pilot>=0)publicationPrivateEvents[event.pilot].push_back(&event);
            else for(int side=0;side<c_cSidesMax;++side)if(event.sides&(1u<<side))publicationCommonEvents[side].push_back(&event);
        }
        publicationEffectsIndexed=true;
    }
    void clearNetworkNotices(){publicationEffectsIndexed=false;auto popEvent=[&]{if(Net::isSharedCue(networkEvents.front()))++feedbackRevision;networkEvents.pop_front();};while(networkEvents.size()>8192)popEvent();
        auto pop=[&]{if(networkNotices.front().sides)++feedbackRevision;networkNotices.pop_front();};
        while(networkNotices.size()>4096)pop();while(!networkEvents.empty() && elapsed-networkEvents.front().time>2)popEvent();
        while(!networkNotices.empty() && elapsed-networkNotices.front().notice.time>30)pop();}
#include "ballot_game.inl"

    uint32_t identity(ImodelIGC* model){auto [i,added]=networkIdentities.try_emplace(model,0);if(added)i->second=++nextIdentity;return i->second;}
    struct Thing final : ThingSite {
        static void* operator new(size_t n) {
            assert(n==sizeof(Thing));
            return ObjectPool<sizeof(Thing),alignof(Thing)>::allocate();
        }
        static void operator delete(void* p) noexcept {
            ObjectPool<sizeof(Thing),alignof(Thing)>::release(p);
        }
        Impl& game;ImodelIGC* model;uint32_t networkIdentity{},networkEpoch{};int16_t networkSector{-2},networkStation{-2},networkParent{-2},networkHull{-2};
        uint32_t projectileCaptureIndex{UINT32_MAX};
        std::unique_ptr<PartCapturePlan> partCapturePlan;
        bool replicaStrategic{};
        std::unique_ptr<Allegiance::ModelVisibility> visibility;bool sideTracked{};
        Allegiance::ModelVisibility& vision(){if(!visibility)visibility=std::make_unique<Allegiance::ModelVisibility>();return *visibility;}
        Allegiance::ShipSoundLevels soundLevels;
        std::array<bool,c_maxMountedWeapons> burstPlaying{};
        double hitSoundTime=-1,damageReportTime=-100;double soundTime=-1;uint64_t soundKey{};std::shared_ptr<SoundEmitter> emitter;
        std::unique_ptr<std::array<Intel,c_cSidesMax>> intel;
        StationHistory* stationHistory{};
        Intel liveIntel()const {
            Intel i;i.side=model->GetSide()?model->GetSide()->GetObjectID():NA;
            if(model->GetObjectType()==OT_ship){auto* s=static_cast<IshipIGC*>(model);i.hull=s->GetFraction();if(auto* p=static_cast<IshieldIGC*>(s->GetMountedPart(ET_Shield,0)))i.shield=p->GetFraction();}
            else if(model->GetObjectType()==OT_station){auto* s=static_cast<IstationIGC*>(model);i.hull=s->GetFraction();i.shield=s->GetShieldFraction();}
            return i;
        }
        Intel knownIntel(IsideIGC* side)const {
            if(stationHistory)return (*stationHistory)[side->GetObjectID()].intel;
            return (model->GetAttributes()&c_mtPredictable)?(intel?(*intel)[side->GetObjectID()]:Intel{}):liveIntel();
        }
        const StationObservation* stationObservation(IsideIGC* side)const {return stationHistory?&(*stationHistory)[side->GetObjectID()]:nullptr;}
        void observe(IsideIGC* side) {
            if(!stationHistory && !(model->GetAttributes()&c_mtPredictable))return;
            auto current=liveIntel();
            if(stationHistory) {
                auto& o=(*stationHistory)[side->GetObjectID()];model->Export(&o.record);o.networkIdentity=networkIdentity;o.intel=current;o.handle=handle;o.radius=radius;
                o.pedestal=static_cast<IstationIGC*>(model)->GetBaseStationType()->HasCapability(c_sabmPedestal);
                o.known=true;o.hasModel=hasModel;o.flip=flip;o.unlit=unlit;o.color=color;o.name=GetModelName(model);o.type=GetModelType(model);
                if(auto* icon=static_cast<Icon*>(model->GetIcon()))o.icon=icon->name;
            } else {if(!intel)intel=std::make_unique<std::array<Intel,c_cSidesMax>>();(*intel)[side->GetObjectID()]=current;}
        }
        ModelHandle handle{};bool hasModel{},unlit{},flip{},building{};glm::vec4 color{1},innerColor{1};float outerIntensity{1};glm::vec3 complement{1};
        EffectTexture decal;bool hasDecal{},directional{};float decalAngle{},width{1},radius{1},age{},emission{},power{},smokeSize{4},fireDuration{.333f},smokeDuration{2};
        glm::vec3 damagePoint{},previousVelocity{},previous{},trailPrevious{},trailRight{},exhaustDirection{};int previousSector{-1};bool hadPosition{},damaged{},hadTrail{};glm::vec4 trailColor{};
        struct ShieldHit {double start=-1;int kind{};glm::mat4 orientation{1};};std::unique_ptr<std::array<ShieldHit,4>> shieldHits;
        struct Aleph {std::array<ModelHandle,4> levels;double bombEnd=-1;float bombDuration{};};std::unique_ptr<Aleph> aleph;
        struct MineField {std::vector<glm::vec3> positions;int count{};};std::unique_ptr<MineField> mine;
        struct HullHit {double start=-1;glm::vec3 position,normal,forward,right;};
        struct HullHits {std::array<HullHit,64> entries;size_t next{};};std::unique_ptr<HullHits> hullHits;
        struct MiningBolt {std::array<glm::vec3,65> points;int count{};float brightness{},width{};double time=-1;};std::unique_ptr<MiningBolt> bolt;
        struct Exhaust {const ModelFrame* frame;bool rocket;};
        std::vector<Exhaust> exhaust;std::vector<const ModelFrame*> glows;const ModelFrame* trail{};float trailTime{},thrust{},exhaustPhase{};
        size_t thingIndex{},modelIndex=SIZE_MAX;
        void modelMembership(){
            if(hasModel && modelIndex==SIZE_MAX){modelIndex=game.modelThings.size();game.modelThings.push_back(this);}
            else if(!hasModel && modelIndex!=SIZE_MAX){auto* moved=game.modelThings.back();game.modelThings[modelIndex]=moved;moved->modelIndex=modelIndex;game.modelThings.pop_back();modelIndex=SIZE_MAX;}
        }
        Thing(Impl& g,ImodelIGC* m):game(g),model(m),networkIdentity(g.identity(m)),soundKey(++g.soundSerial){
            if(g.fx && g.fx->performance()){smokeSize=2;fireDuration=.33f;smokeDuration=.66f;}
            if(m->GetObjectType()==OT_ship)g.shipPaneStatus(static_cast<IshipIGC*>(m));
            game.publicationIndexValid=false;
            thingIndex=game.things.size();game.things.push_back(this);
            if(m->GetObjectType()==OT_station) {
                stationHistory=&game.stationHistory[m->GetObjectID()];
                // A station reloads its ThingSite when upgraded. Keep the last
                // exported appearance and discovery for sides without an eye.
                for(auto* l=m->GetMission()->GetSides()->first();l;l=l->next())
                    if((*stationHistory)[l->data()->GetObjectID()].known)vision().Set(l->data(),true,false);
            }
        }
        ~Thing(){if(model->GetObjectType()==OT_asteroid)game.investmentHintsDirty=game.mapRockKnowledgeDirty=true;game.publicationIndexValid=false;if(emitter)emitter->active=false;hasModel=false;modelMembership();auto* moved=game.things.back();game.things[thingIndex]=moved;moved->thingIndex=thingIndex;game.things.pop_back();
            if(projectileCaptureIndex!=UINT32_MAX)game.freeProjectileCaptures.push_back(projectileCaptureIndex);
            // SetCluster/SetSide still use this history after the termination
            // notification. Upgrades reload a site with its side intact.
            if(stationHistory && !model->GetSide())game.stationHistory.erase(model->GetObjectID());
        }
        HRESULT LoadModel(int options,const char* name,const char* texture)override {
            if(!name || !*name){hasModel=hasDecal=false;modelMembership();exhaust.clear();glows.clear();trail=nullptr;return S_OK;}
            handle=game.loader(name,texture?texture:"");hasModel=true;flip=options==0;radius=handle.radius;hadPosition=hadTrail=false;
            // Boarding hides the passenger's empty model. WinTrek refreshes
            // visibility when a real hull returns; native camera/fog filtering
            // is separate, so restore this persistent flag at hull load time.
            if(model->GetObjectType()==OT_ship)model->SetVisibleF(true);
            modelMembership();
            exhaust.clear();glows.clear();trail=nullptr;
            for(auto& f:handle.frames) {
                bool rocket=f.name.find("rocket")!=std::string::npos;
                if(rocket || f.name.find("smoke")!=std::string::npos)exhaust.push_back({&f,rocket});
                if(f.name.find("thrust")!=std::string::npos)glows.push_back(&f);
                if(f.name=="trail")trail=&f;
            }
            return S_OK;
        }
        HRESULT LoadDecal(const char* texture,bool direction,float w)override {if(game.fx && texture && *texture){decal=game.fx->load(std::string(texture)+"bmp");hasDecal=true;}directional=direction;width=w;return S_OK;}
        void Spin(float angle)override{if(hasDecal)decalAngle+=angle;}
        HRESULT LoadAleph(const char* texture)override {
            aleph=std::make_unique<Aleph>();
            for(int i=0;i<4;++i)aleph->levels[i]=game.loader("$aleph"+std::to_string(i),texture?texture:"");
            handle=aleph->levels[0];hasModel=unlit=flip=true;modelMembership();color={1,1,1,1};return S_OK;
        }
        void AddPulse(float duration,const Vector&,float,const Color&)override {
            if(aleph){aleph->bombDuration=duration;aleph->bombEnd=game.elapsed+duration;}
        }
        HRESULT LoadEffect(const Color& c)override {
            handle=game.loader("build","");hasModel=unlit=building=true;flip=false;color={c.GetRed(),c.GetGreen(),c.GetBlue(),c.GetAlpha()};
            modelMembership();
            float h,s,b;Color base=c;base.GetHSB(h,s,b);auto other=HSBColor(h+.5f,s,b);complement={other.GetRed(),other.GetGreen(),other.GetBlue()};return S_OK;
        }
        void SetColors(float alpha,float inner,float outer)override{innerColor={complement*inner,alpha};outerIntensity=outer;}
        HRESULT LoadMine(const char* texture,float strength,float r)override {
            radius=r;if(!game.fx)return S_OK;decal=game.fx->load(std::string(texture)+"bmp");decal.frames=1;
            mine=std::make_unique<MineField>();mine->count=Allegiance::MineParticleCount(strength,r);mine->positions.reserve(mine->count);
            for(int i=0;i<mine->count;++i)mine->positions.push_back(game.fx->direction()*(r*std::pow(game.fx->random(0,1),1.f/3)));
            return S_OK;
        }
        void SetMineStrength(float strength)override {if(mine)mine->count=std::min(mine->count,Allegiance::MineParticleCount(strength,radius));}
        Vector GetChildModelOffset(const ZString& name)override {
            for(const auto& frame:handle.frames)if(frame.name==static_cast<const char*>(name))
                return vec(glm::vec3(modelTransform({1,0,0},{0,1,0},{0,0,1},{0,0,0},radius/handle.radius,flip)*glm::vec4(frame.position,1)));
            return Vector::GetZero();
        }
        float GetRadius()override{return radius;}
        void SetRadius(float r)override{radius=r;}
        void SetAfterburnerThrust(const Vector& direction,float p)override{power=p;exhaustDirection=vec(direction);}
        void SetAfterburnerSmokeSize(float s)override{smokeSize=s;}
        void SetAfterburnerFireDuration(float d)override{fireDuration=d;}
        void SetAfterburnerSmokeDuration(float d)override{smokeDuration=d;}
        void SetTrailColor(const Color& c)override{trailColor={c.GetRed(),c.GetGreen(),c.GetBlue(),1};}
        struct DamageFrame {glm::vec3 position,direction;};std::unique_ptr<std::array<DamageFrame,10>> damageFrames;int damageCount{};
        void AddDamage(const Vector& position,float fraction)override{
            int count=Allegiance::DamageFrameCount(fraction);if(count<=damageCount)return;
            auto local=model->GetOrientation().TimesInverse(position*.33f);
            auto direction=local.LengthSquared()>0?local.Normalize():Vector(0,0,1);
            if(count && !damageFrames)damageFrames=std::make_unique<std::array<DamageFrame,10>>();
            while(damageCount<count)(*damageFrames)[damageCount++]={vec(local),vec(direction)};
        }
        void RemoveDamage(float fraction)override{damageCount=std::min(damageCount,Allegiance::DamageFrameCount(fraction));}
        float ripcordTime{-1},alephTime{-1};
        void SetTimeUntilRipcord(float time)override{ripcordTime=time;}
        void SetTimeUntilAleph(float time)override{alephTime=time;}
        void AddFlare(Time,const Vector& position,int id,const Vector* ellipse)override {
            Net::Event event{};event.kind=Net::EventKind::ShieldHit;event.position={position.x,position.y,position.z};event.value=id;if(ellipse){event.ellipse={ellipse->x,ellipse->y,ellipse->z};event.flags=1;}game.recordEvent(event,model);
            if(!game.fx || !model->GetCluster() || !game.eyed(model) || id<0 || id>=3)return;
            if(!shieldHits)shieldHits=std::make_unique<std::array<ShieldHit,4>>();
            for(auto& hit:*shieldHits)if(hit.start<0 || game.elapsed-hit.start>=.75) {
                hit.start=game.elapsed;hit.kind=id;
                auto orientation=Allegiance::ShieldFlareOrientation(model->GetOrientation(),position,model->GetRadius(),ellipse);
                hit.orientation=matrix(orientation,Vector::GetZero(),1);break;
            }
        }
        void AddHullHit(const Vector& position,const Vector& normal)override {
            Net::Event event{};event.kind=Net::EventKind::HullHit;event.position={position.x,position.y,position.z};event.normal={normal.x,normal.y,normal.z};game.recordEvent(event,model);
            if(!game.fx || !model->GetCluster() || !game.eyed(model))return;
            if(!hullHits)hullHits=std::make_unique<HullHits>();
            auto axes=Allegiance::HullImpactAxes(normal);
            hullHits->entries[hullHits->next++%hullHits->entries.size()]={game.elapsed,vec(position),vec(normal),vec(axes.forward),vec(axes.right)};
        }
        void AddExplosion(const Vector& offset,float radius,int type)override {
            if(!game.terminating && (game.gameRole==GameRole::Authority || game.eyed(model)))if(auto* cluster=model->GetCluster())
                cluster->GetClusterSite()->AddExplosion(model->GetPosition()+offset*model->GetOrientation(),radius,type);
        }
        void Terminate()override {if(visibility)visibility->Clear();}
        bool mapInformation()const {
            auto type=model->GetObjectType();return type==OT_ship || type==OT_station || type==OT_asteroid || type==OT_probe;
        }
        void SetCluster(ImodelIGC* m,IclusterIGC* cluster)override {
            // ChangeCluster clears scanner eyes without an Update edge. Keep
            // the old encounter, or the observed docking, before that reset.
            if(visibility && m->GetObjectType()==OT_ship && game.gameRole!=GameRole::Replica && !game.terminating) {
                auto* ship=static_cast<IshipIGC*>(m);
                for(auto* l=m->GetMission()->GetSides()->first();l;l=l->next())if(visibility->Eye(l->data())) {
                    auto* station=ship->GetSourceShip()->GetStation();
                    if(!cluster && station && station->SeenBySide(l->data()))game.observeShipIntel(ship,l->data());
                    else game.hideShipIntel(ship,l->data());
                }
            }
            if(mapInformation())++game.mapIntelRevision;
            if(m->GetObjectType()==OT_asteroid)game.investmentHintsDirty=game.mapRockKnowledgeDirty=true;
            game.publicationIndexValid=false;
            if(projectileCaptureIndex!=UINT32_MAX) {
                auto& capture=game.capturedProjectiles[projectileCaptureIndex];
                int sector=cluster?cluster->GetObjectID():NA;
                if(capture.lastSector!=sector){capture.record.epoch=++game.nextNetworkEpoch;capture.lastSector=sector;}
            }
            sideTracked=(m->GetAttributes()&c_mtSeenBySide)!=0;
            if(sideTracked)vision().ChangeCluster(m);else visibility.reset();hadPosition=hadTrail=false;
            if(emitter && cluster!=game.viewCluster)emitter->active=false;
            UpdateSideVisibility(m,cluster);
        }
        void UpdateSideVisibility(ImodelIGC* m,IclusterIGC* cluster)override {
            sideTracked=(m->GetAttributes()&c_mtSeenBySide)!=0;if(!sideTracked)return;
            vision().Update(m,cluster,[&](IsideIGC* side,bool known,bool now,bool wasEye,bool eye,bool cached){
                if(!known && now && !cached)game.scoreDiscovery(m,visibility->LastSpotter(side));
                // Remember encounters even when F6 is closed. This is an
                // edge notification, not a frame-wide roster scan.
                if(game.gameRole==GameRole::Offline && m->GetObjectType()==OT_ship && game.player && side==game.player->GetSide() && m->GetSide()!=side && !game.terminating && (wasEye || eye))
                    game.observePlayerPaneShip(static_cast<IshipIGC*>(m));
                if(game.gameRole!=GameRole::Replica && m->GetObjectType()==OT_ship && !game.terminating && wasEye!=eye) {
                    auto* ship=static_cast<IshipIGC*>(m);
                    if(eye)game.observeShipIntel(ship,side);else game.hideShipIntel(ship,side);
                }
                if(known!=now && mapInformation())++game.mapIntelRevision;
                if(!known && now && m->GetObjectType()==OT_asteroid)game.investmentHintsDirty=game.mapRockKnowledgeDirty=true;
                if(!known && now && m->GetObjectType()==OT_ship && game.player && side==game.player->GetSide() && !game.terminating && cluster==game.viewCluster)
                    game.PlaySoundEffect(Allegiance::ShipContactSound(cached,!IsideIGC::AlliedSides(m->GetSide(),side)),m);
                if(!known && now && m->GetObjectType()==OT_station)
                    game.forgetRock(static_cast<IstationIGC*>(m)->GetRoidID(),cluster->GetObjectID(),side);
                if(!known && now && m->GetObjectType()==OT_probe && game.player && side==game.player->GetSide() && !game.terminating) {
                    auto* probe=static_cast<IprobeIGC*>(m);
                    if(probe->GetProbeType()->HasCapability(c_eabmRipcord)) {
                        bool friendly=IsideIGC::AlliedSides(probe->GetSide(),side);
                        if(friendly)game.PostText(true,"%s deployed in %s",GetModelName(probe),cluster->GetName());
                        else game.PostText(true,START_COLOR_STRING "%s %s" END_COLOR_STRING " spotted in %s",(PCC)ConvertColorToString(probe->GetSide()->GetColor()),probe->GetSide()->GetName(),GetModelName(probe),cluster->GetName());
                    }
                }
                if(!known && now && m->GetObjectType()==OT_warp) {
                    // Discovering an aleph also discovers its paired endpoint.
                    if(auto* destination=static_cast<IwarpIGC*>(m)->GetDestination())
                        static_cast<Thing*>(destination->GetThingSite())->vision().Set(side,true,false);
                }
            });
            if(cluster && (m->GetAttributes()&c_mtPredictable))
                for(auto* l=cluster->GetMission()->GetSides()->first();l;l=l->next())
                    if(visibility->Eye(l->data()))observe(l->data());
        }
        bool GetSideVisibility(IsideIGC* side)override{return !sideTracked || (visibility && visibility->Known(side));}
        bool GetCurrentEye(IsideIGC* side)override{return !sideTracked || (visibility && visibility->Eye(side));}
        void SetSideVisibility(IsideIGC* side,bool visible)override {
            if(mapInformation() && GetSideVisibility(side)!=visible)++game.mapIntelRevision;
            if(model->GetObjectType()==OT_asteroid && GetSideVisibility(side)!=visible)game.investmentHintsDirty=game.mapRockKnowledgeDirty=true;
            if(sideTracked)vision().Set(side,visible,visible);if(visible)observe(side);
            else if(stationHistory)(*stationHistory)[side->GetObjectID()].known=false;
        }
    };
    void recordEvent(Net::Event e,ImodelIGC* model=nullptr){if(gameRole!=GameRole::Authority || networkReplaying)return;e.number=++eventSerial;e.time=elapsed;if(model){auto* thing=static_cast<Thing*>(model->GetThingSite());e.model={thing->networkIdentity,model->GetObjectType(),model->GetObjectID()};e.sector=model->GetCluster()?model->GetCluster()->GetObjectID():NA;for(auto* l=mission->GetSides()->first();l;l=l->next())if(model->GetSide()==l->data() || thing->GetCurrentEye(l->data()))e.sides|=1u<<l->data()->GetObjectID();}publicationEffectsIndexed=false;networkEvents.push_back(e);}
    std::filesystem::path artworkPath;ModelLoader loader;uint32_t effectModel,warpModel;
    std::unique_ptr<Particles> fx;
    ImissionIGC* mission{};TRef<IshipIGC> player;TRef<ImodelIGC> target;int targetRock=-1,targetRockSector=-1;
    IclusterIGC* viewCluster{};
    std::vector<Thing*> things,modelThings;
    bool publicationIndexValid{};
    std::vector<Thing*> publicationGlobalThings;
    std::unordered_map<int,std::vector<Thing*>> publicationSectorThings;
    std::array<std::unordered_set<ImodelIGC*>,c_cSidesMax> publicationCommandBuoys;
    void indexNetworkCapture() {
        if(publicationIndexValid)return;
        publicationGlobalThings.clear();
        for(auto& [_,sector]:publicationSectorThings)sector.clear();
        for(auto& buoys:publicationCommandBuoys)buoys.clear();
        for(auto* thing:things) {
            auto* model=thing->model;
            if(model->GetObjectType()!=OT_projectile)publicationGlobalThings.push_back(thing);
            auto* cluster=model->GetCluster();
            if(model->GetObjectType()==OT_ship){auto* ship=static_cast<IshipIGC*>(model);if(ship->GetParentShip())cluster=ship->GetSourceShip()->GetCluster();}
            if(cluster)publicationSectorThings[cluster->GetObjectID()].push_back(thing);
        }
        // Projectile retirement swaps the presentation list. Keep persistent
        // state in identity order so that churn cannot reorder every delta.
        auto ordered=[](const Thing* a,const Thing* b){return a->networkIdentity<b->networkIdentity;};
        std::sort(publicationGlobalThings.begin(),publicationGlobalThings.end(),ordered);
        for(auto& [_,sector]:publicationSectorThings)std::sort(sector.begin(),sector.end(),ordered);
        for(auto* l=mission->GetShips()->first();l;l=l->next()) {
            auto* ship=l->data();auto* side=ship->GetSide();if(!side)continue;
            auto& buoys=publicationCommandBuoys[side->GetObjectID()];
            for(Command c=0;c<c_cmdMax;++c)
                if(auto* target=ship->GetCommandTarget(c);target && target->GetObjectType()==OT_buoy)buoys.insert(target);
        }
        publicationIndexValid=true;
    }
    std::vector<TRef<ImodelIGC>> killed,predictionProjectiles;
    std::vector<DrawItem> draws;std::vector<Contact> contacts;std::vector<SoundEvent> sounds;Feedback feedback;
    struct RememberedRock {
        int id,sector;unsigned sides;AsteroidAbilityBitMask capabilities;
        glm::vec3 position;float radius;DrawItem draw;ModelHandle handle;std::string name,type,icon;
    };
    std::vector<RememberedRock> rememberedRocks;
    uint64_t mapIntelRevision{};bool mapRockKnowledgeDirty{true};int mapRockKnowledgeSide{-1};std::vector<int> mapRockSectors;
    std::array<std::unordered_set<int>,c_cSidesMax> sectorHighlights;std::unordered_set<int> dismissedSectorHighlights;int highlightSide{-1};
    SectorInfo cachedSectorInfo;uint64_t sectorInfoIntel{},sectorInfoEconomy{};int sectorInfoSide{-1};double sectorInfoTime{-1};
    bool investmentHintsDirty{true};uint32_t investmentHintMask{};int investmentHintSide{-1};
    std::vector<int> investmentHintSectors;
    void forgetRock(int id,int sector,IsideIGC* viewer) {
        investmentHintsDirty=mapRockKnowledgeDirty=true;
        if(player && player->GetSide()==viewer && id==targetRock && sector==targetRockSector) {
            TRef<ImodelIGC> old=target;target=nullptr;targetRock=targetRockSector=-1;
            for(Command c=0;c<c_cmdMax;++c)if(player->GetCommandTarget(c)==old)player->SetCommand(c,nullptr,c_cidNone);
        }
        for(auto& rock:rememberedRocks)if(rock.id==id && rock.sector==sector)rock.sides&=~(1u<<viewer->GetObjectID());
        std::erase_if(rememberedRocks,[](const auto& rock){return !rock.sides;});
    }
    void KillAsteroidEvent(AsteroidID id,SectorID sector,IsideIGC* side)override {forgetRock(id,sector,side);}
    int displayedFlag=-100;ShipID displayedFlagSource=NA;
    std::unique_ptr<TrainingHost> training;double trainingRestoreAt{-1};
    GameState state;Time start;double elapsed{};uint64_t messageSerial{};float botTimer{};int hullIndex{};bool terminating{};bool groupFire{true};Mount selectedWeapon{};Mount selectedCargo=-1;
    std::array<std::string,3> commandDisplayNames;
    Allegiance::CameraMotion cameraMotion{RadiansFromDegrees(65)};
    Orientation externalCameraOrientation;TRef<ImissileIGC> lastCameraMissile;
    Allegiance::ChaseMotion chaseMotion;
    bool autopilotInputInitialized{},throttleResetPending{},forcedThrottleReset{};float throttleReset{};
    std::array<float,3> autopilotAxes{},lastNetworkDeviceAxes{};unsigned previousActionButtons{};
    VisualPreferences visual;
    CameraInput cameraInput;CameraState cameraState;
    double cinematicStop{-1};bool cinematicDeath{};Vector cinematicTarget,cinematicPosition;Orientation cinematicOrientation;TRef<IclusterIGC> cinematicCluster;
    bool showGrid{},gridWasCommand{},commandViewEnabled{},commandTopDown{},commandTracking{},commandCameraDirty{};
    bool showCommandGrid{true};
    std::array<unsigned,2> nextBotHull{};
    float commandDistance{6000},commandRoll{};Vector commandCenter{0,0,0},commandDisplacement{0,0,0};
    TRef<IclusterIGC> commandCluster;int commandUnit{-1};CommandID commandOrder{c_cidDefault};
    struct CommandFocus {int ship,sector;uint32_t identity;};
    std::optional<CommandFocus> pendingCommandFocus;
    std::vector<int> commandUnits;std::bitset<32768> commandSelected;
    bool inCommandView()const{return training?training->view==TrainingHost::vmCommand:commandViewEnabled;}

    double cameraTime{};bool cameraValid{},cameraWasChase{};uint32_t cameraRandom{0x7a91f04d};
    Allegiance::ShipSoundLevels engineLevels;bool engineLevelsValid{};
    std::array<SoundLoop,5> engineLoops;
    std::array<ModelHandle,3> shieldModels;
    std::vector<SoundLoop> worldLoops;uint64_t soundSerial{};
    std::vector<std::shared_ptr<SoundEmitter>> activeEmitters;
    Allegiance::ShipSoundNotifications soundNotifications;double notificationTime=-1;
    Allegiance::CountdownSounds countdownSounds;
    std::vector<Allegiance::MissileLaunchVectors> missileLaunches;
    struct SavedPart {PartID id;Mount mount;bool bDuplicated{};};
    struct SavedOutfit {std::vector<SavedPart> parts;std::array<PartID,c_maxCargo+3> desired{};};
    std::unordered_map<ShipID,SavedOutfit> loadouts;
    // WinTrek's m_loadouts and m_customLoadouts are client choices, distinct
    // from the authority's current equipment and carrier rearm policy.
    std::unordered_map<ShipID,Loadouts> editorLoadouts;
    Loadouts loadoutPresets;
    bool dockRestorationPending{},recoverySoundPending{};
    std::unordered_map<ShipID,StationID> lastOwnStations;
    void rememberLoadout(IshipIGC* ship) {
        auto& saved=loadouts[ship->GetObjectID()];auto& list=saved.parts;list.clear();
        IpartTypeIGC* desired[c_maxCargo+3];Allegiance::SaveDesiredCarrierLoadout(ship,desired);
        for(size_t i=0;i<saved.desired.size();++i)saved.desired[i]=desired[i]?desired[i]->GetObjectID():NA;
        for(auto* l=ship->GetParts()->first();l;l=l->next())list.push_back({l->data()->GetPartType()->GetObjectID(),l->data()->GetMountID()});
        std::reverse(list.begin(),list.end());std::stable_sort(list.begin(),list.end(),[](auto& a,auto& b){return a.mount<b.mount;});
        if(gameRole!=GameRole::Authority && ship->GetPilotType()>=c_ptPlayer && ship->GetBaseHullType() && !ship->GetBaseHullType()->HasCapability(c_habmLifepod)) {
            Loadout cached;cached.hull=ship->GetBaseHullType()->GetObjectID();
            for(auto* l=ship->GetParts()->first();l;l=l->next())cached.parts.push_back({l->data()->GetPartType()->GetObjectID(),int8_t(l->data()->GetMountID())});
            editorLoadouts[ship->GetObjectID()].remember(0,std::move(cached));
        }
    }
    void restoreLoadout(IshipIGC* ship,bool alliedStation=false) {
        auto found=loadouts.find(ship->GetObjectID());
        if(economy()) {
            IpartTypeIGC* desired[c_maxCargo+3]{};if(found!=loadouts.end())for(size_t i=0;i<found->second.desired.size();++i)desired[i]=mission->GetPartType(found->second.desired[i]);
            if(alliedStation)Allegiance::RearmAtAlliedStation(ship,desired);
            else Allegiance::RearmAtCarrier(ship,ship->GetPilotType()>=c_ptPlayer?desired:nullptr);
            for(auto* l=ship->GetParts()->first();l;l=l->next())warmPart(l->data());return;
        }
        if(found==loadouts.end()) {
            Allegiance::RestoreUncachedCarrierLoadout(ship,[&](IshipIGC* target,IstationIGC* station,Money* budget) {
                for(Mount m=-c_maxCargo;m<0;++m)if(auto* part=target->GetMountedPart(NA,m))part->Terminate();
                Allegiance::BuyPreferredParts(target,station,budget);Allegiance::BuyDefaultCargo(target,station,budget);
            });
            for(auto* l=ship->GetParts()->first();l;l=l->next())warmPart(l->data());return;
        }
        auto& saved=found->second.parts;
        Allegiance::RestoreOfflineCarrierLoadout(ship,
            [&](auto visit){for(auto& part:saved)if(!visit(part))break;},
            [&](auto visit){for(auto i=saved.rbegin();i!=saved.rend();++i)if(!visit(*i))break;},
            [&](auto& part){return mission->GetPartType(part.id);});
        for(auto* l=ship->GetParts()->first();l;l=l->next())warmPart(l->data());
    }

    // Undistributed side income and individual pilot accounts, as in FedSrv.
    std::array<Money,c_cSidesMax> funds{};
    std::unordered_map<ShipID,Money> accounts;
    // Replica faction metadata outlives the physical visibility of a pilot.
    std::vector<int16_t> pilotWings,pilotDonationTargets;std::vector<ShipID> pilotWingIds;
    std::unordered_map<ShipID,DroneTypeID> droneTypes;
    IdroneTypeIGC* droneType(IshipIGC* ship) {
        auto i=droneTypes.find(ship->GetObjectID());return i==droneTypes.end()?nullptr:mission->GetDroneType(i->second);
    }
    int countDrones(IsideIGC* side,IdroneTypeIGC* type) {
        int count=0;for(auto* l=side->GetShips()->first();l;l=l->next())if(droneType(l->data())==type)++count;
        return count;
    }
    bool investmentBuildSpot(IsideIGC* side,IstationTypeIGC* type) {
        for(auto* link=mission->GetAsteroids()->first();link;link=link->next())if(link->data()->HasCapability(type->GetBuildAABM()) && link->data()->SeenBySide(side))return true;
        for(auto& rock:rememberedRocks)if((rock.sides&(1u<<side->GetObjectID())) && (rock.capabilities&type->GetBuildAABM())==type->GetBuildAABM())return true;
        return false;
    }
    bool mayInvest(IbucketIGC* bucket,int droneCount=-1) {
        auto* side=bucket->GetSide();if(!side->CanBuy(bucket->GetBuyable()))return false;
        if(bucket->GetBucketType()==OT_droneType) {
            auto* type=static_cast<IdroneTypeIGC*>(bucket->GetBuyable());int count=droneCount<0?countDrones(side,type):droneCount;
            if(count>=mission->GetMissionParams()->nMaxDronesPerTeam)return false;
        }
        if(bucket->GetBucketType()==OT_stationType) {
            auto* type=static_cast<IstationTypeIGC*>(bucket->GetBuyable());
            for(auto* link=side->GetShips()->first();link;link=link->next())if(link->data()->GetBaseData()==type)return false;
            if(!investmentBuildSpot(side,type))return false;
        }
        return true;
    }
    Money planInvestment(int id,int amount,IbucketIGC*& bucket){
        bucket=nullptr;
        if(gameplayClosed() || !economy() || !onTeam() || id<0 || id>SHRT_MAX || amount==0 || amount< -1)return 0;
        bucket=player->GetSide()->GetBucket(BucketID(id));
        if(!bucket || bucket->GetCompleteF() || !mayInvest(bucket))return 0;
        Money wallet=readBalance(player);
        auto remaining=int64_t(bucket->GetPrice())-bucket->GetMoney();
        return Money(std::max<int64_t>(0,std::min({int64_t(wallet),amount<0?int64_t(wallet):int64_t(amount),remaining})));
    }
    Money fundInvestment(IbucketIGC* bucket,Money amount){
        Money spent=bucket->AddMoney(amount);balance(player)-=spent;return spent;
    }
    std::array<ShipID,c_cSidesMax> commanders=[] {std::array<ShipID,c_cSidesMax> ids;ids.fill(NA);return ids;}();
    std::unordered_map<ShipID,MoneyRequest> moneyRequests;
    std::bitset<32768> mutedPilots;
    Allegiance::ChatPreferences chatPrefs;
    struct RecipientChat {Allegiance::ChatPreferences preferences;std::vector<int> muted;};
    std::unordered_map<int,RecipientChat> recipientChat;
    Allegiance::ChatPreferences recipientPreferences(int pilot)const {
        auto found=recipientChat.find(pilot);return found==recipientChat.end()?Allegiance::ChatPreferences{}:found->second.preferences;
    }
    bool recipientMuted(int pilot,int sender)const {
        auto found=recipientChat.find(pilot);return found!=recipientChat.end() && std::find(found->second.muted.begin(),found->second.muted.end(),sender)!=found->second.muted.end();
    }
    const QuickChatCatalog* quickChatCatalog{};
    uint64_t displayRevision{1};
    std::function<std::string(std::string_view)> interfaceFormatter;
    bool muted(int id)const {return id>=0 && id<int(mutedPilots.size()) && mutedPilots.test(size_t(id));}
    bool muted(IshipIGC* ship)const {return ship && muted(ship->GetObjectID());}
    void setMuted(int id,bool mute) {
        if(id<0 || id>=int(mutedPilots.size()))return;
        mutedPilots.set(size_t(id),mute);
        if(gameRole==GameRole::Replica)submit(Net::Operation::Mute,{id,int(mute)});
        for(auto& pilot:state.pilots)if(pilot.id==id)pilot.muted=mute;
        for(auto& pilot:state.networkPilots)if(pilot.id==id)pilot.muted=mute;
        ++state.economyRevision;
    }
    void setMuted(IshipIGC* ship,bool mute){setMuted(ship->GetObjectID(),mute);}
    Money& balance(IshipIGC* ship){return accounts.at(ship->GetObjectID());}
    Money readBalance(const IshipIGC* ship)const {auto i=accounts.find(ship->GetObjectID());return i==accounts.end()?0:i->second;}
    uint32_t creditIdentity(IshipIGC* ship) {
        auto [entry,added]=creditIdentities.try_emplace(ship->GetObjectID(),0);
        if(added)entry->second=++nextCreditIdentity;return entry->second;
    }
    void forgetCredits(ShipID id) {
        creditIdentities.erase(id);for(auto& ledger:alliedCreditTotals)ledger.erase(id);
    }
    Money alliedBalance(int id,int owner)const {
        if(gameRole==GameRole::Replica)return observedCredits.money(id,owner);
        auto side=player->GetSide()->GetObjectID();if(side<0 || side>=c_cSidesMax)return 0;
        auto found=alliedCreditTotals[side].find(ShipID(id));auto* ship=mission->GetShip(ShipID(id));
        auto generation=creditIdentities.find(ShipID(id));
        return !ship || found==alliedCreditTotals[side].end() || generation==creditIdentities.end() || found->second.identity!=generation->second?0:Allegiance::CreditDifference(found->second.amount);
    }
    void setDonation(IshipIGC* donor,IshipIGC* recipient) {
        TRef<IshipIGC> old=donor->GetAutoDonate();donor->SetAutoDonate(recipient);
        if(old==recipient || terminating || networkReplaying || uiPreview || mission->GetMissionStage()!=STAGE_STARTED)return;
        Allegiance::IncomeDonationFeedback(donor->GetName(),old?old->GetName():nullptr,recipient?recipient->GetName():nullptr,
            [&](bool critical,const char* format,auto... args){PostNotificationText(donor,critical,format,args...);},
            [&](bool critical,const char* format,auto... args){PostNotificationText(old,critical,format,args...);},
            [&](bool critical,const char* format,auto... args){PostNotificationText(recipient,critical,format,args...);},
            [&]{int count=0;for(auto* l=recipient->GetSide()->GetShips()->first();l && count<2;l=l->next())if(l->data()->GetAutoDonate()==recipient)++count;
                if(count==1)playPilotSound(investorSound,recipient);});
    }
    void changeCommander(IsideIGC* side,IshipIGC* leader) {
        auto& commander=commanders[side->GetObjectID()];if(commander==(leader?leader->GetObjectID():NA))return;
        commander=leader?leader->GetObjectID():NA;economyDirty=true;if(!leader)return;
        if(mission->GetMissionStage()==STAGE_STARTED)for(auto* l=side->GetShips()->first();l;l=l->next())if(accounts.contains(l->data()->GetObjectID())) {
            auto* pilot=l->data();Allegiance::TeamLeaderFeedback(pilot==leader,leader->GetName(),
                [&](bool critical,const char* format,auto... args){PostNotificationText(pilot,critical,format,args...);});
        }
        playPilotSound(commanderSound,leader);
    }
    void moneyTransfer(IshipIGC* sender,IshipIGC* recipient,Money amount) {
        balance(sender)-=amount;balance(recipient)+=amount;
        if(sender->GetSide()!=recipient->GetSide()) {
            auto from=sender->GetSide()->GetObjectID(),to=recipient->GetSide()->GetObjectID();
            alliedCreditTotals[from][recipient->GetObjectID()].add(creditIdentity(recipient),amount);
            alliedCreditTotals[to][sender->GetObjectID()].add(creditIdentity(sender),-amount);
        }
        PostNotificationText(recipient,false,"%s gave you $%d. You now have $%d.",sender->GetName(),amount,readBalance(recipient));
    }
    void retireAccount(IshipIGC* ship,Money shipValue) {
        pendingBallots.erase(ship->GetObjectID());
        forgetCredits(ship->GetObjectID());
        if(ship->GetObjectID()>=0)mutedPilots.reset(size_t(ship->GetObjectID()));
        auto account=accounts.find(ship->GetObjectID());if(account==accounts.end())return;
        auto* side=ship->GetSide();auto& commander=commanders[side->GetObjectID()];IshipIGC* replacement=nullptr;
        if(commander==ship->GetObjectID()) {
            std::vector<Allegiance::LeaderContender> scratch(size_t(side->GetShips()->n()));
            replacement=Allegiance::PickDonationLeader(side->GetShips(),ship,0,scratch.data());
            if(!replacement)for(auto* link=side->GetShips()->first();link;link=link->next())
                if(link->data()!=ship && accounts.contains(link->data()->GetObjectID())){replacement=link->data();break;}
            changeCommander(side,replacement);
            if(replacement)replacement->SetWingID(0);
        }
        Allegiance::RedirectDepartingDonations(side->GetShips(),ship,replacement,[&](IshipIGC* donor,IshipIGC* recipient){setDonation(donor,recipient);});
        funds[side->GetObjectID()]+=account->second+shipValue;accounts.erase(account);
        moneyRequests.erase(ship->GetObjectID());
        std::erase_if(moneyRequests,[&](auto& request){return request.second.sender==ship->GetObjectID();});
        std::erase_if(pendingOrders,[&](auto& request){return request.first==ship->GetObjectID() || request.second.sender==ship;});
        distributeIncome(side);economyDirty=true;
    }
    void distributeIncome(IsideIGC* side) {
        int humans=0;for(auto* link=side->GetShips()->first();link;link=link->next())if(accounts.contains(link->data()->GetObjectID()))++humans;
        Money share=Allegiance::PaydayShare(funds[side->GetObjectID()],humans);
        if(share<=0)return;funds[side->GetObjectID()]-=share*humans;
        // Aggregate once for the side; personal reports need credited totals,
        // not another scan of every donor for every pilot.
        std::unordered_map<ShipID,Money> received;received.reserve(humans);
        Allegiance::CreditPayday(side->GetShips(),share,[&](IshipIGC* ship){return accounts.contains(ship->GetObjectID());},
            [&](IshipIGC* ship,Money amount){balance(ship)+=amount;received[ship->GetObjectID()]+=amount;});economyDirty=true;
        for(auto* l=side->GetShips()->first();l;l=l->next())if(accounts.contains(l->data()->GetObjectID())) {
            auto* pilot=l->data();auto* recipient=pilot->GetAutoDonate();playPilotSound(paydaySound,pilot);
            Allegiance::PaydayFeedback(share,recipient?0:received.at(pilot->GetObjectID()),recipient?recipient->GetName():nullptr,
                [&](bool critical,const char* format,auto... args){PostNotificationText(pilot,critical,format,args...);});
        }
    }
    void updateCommander(IsideIGC* side) {
        auto* current=mission->GetShip(commanders[side->GetObjectID()]);int oldVotes=0,otherVotes=0;
        for(auto* link=side->GetShips()->first();link;link=link->next())if(auto* vote=link->data()->GetAutoDonate())(vote==current?oldVotes:otherVotes)++;
        if(oldVotes>=otherVotes)return;
        std::vector<Allegiance::LeaderContender> scratch(size_t(side->GetShips()->n()));
        if(auto* leader=Allegiance::PickDonationLeader(side->GetShips(),current,oldVotes,scratch.data())) {
            changeCommander(side,leader);
        }
    }
    void GetMoneyRequest(IshipIGC* sender,Money amount,HullID hull)override {
        if(terminating || state.complete || !player || !sender || amount<=0 || !economy() || sender->GetSide()!=player->GetSide() || !accounts.contains(sender->GetObjectID()))return;
        queueMoneyRequest(player,sender,amount,hull);
    }
    HullID selectedHull{};
    bool economy()const{return options.scenario==Scenario::Conquest;}
    bool onTeam()const{return player && player->GetSide() && player->GetSide()->GetObjectID()>=0;}
    bool gameplayClosed()const{return state.complete || !onTeam() || (economy() && !player->GetSide()->GetActiveF());}
    IhullTypeIGC* startingHull(IsideIGC* side)const {
        auto* station=side->GetStations()->first()?side->GetStations()->first()->data():nullptr;IhullTypeIGC* best=nullptr;
        if(station)for(auto* l=mission->GetHullTypes()->first();l;l=l->next()) {
            auto* h=l->data();if(h->GetGroupID()<0 || h->HasCapability(c_habmLifepod) || !station->CanBuy(h))continue;
            if(!best || h->GetPrice()<best->GetPrice())best=h;
        }
        return best?best:side->GetCivilization()->GetLifepod();
    }
    static std::string commandName(CommandID id) {
        static const char* names[]={"Default","Attack","Capture","Defend","Pick up","Go to","Repair","Join","Mine","Build"};
        return id>=0 && id<c_cidMax?names[id]:id==c_cidDoNothing?"Hold":"Idle";
    }
    float paydayTime{};bool economyDirty{true},uiPreview{};
    void BucketChangeEvent(BucketChange,IbucketIGC*)override{economyDirty=true;}
    void CommandChangedEvent(Command command,IshipIGC* ship,ImodelIGC* model,CommandID)override {
        if(uiPreview)return;
        economyDirty=true;
        if(command==c_cmdCurrent && ship==player && target!=model){target=model;targetRock=targetRockSector=-1;}
    }
    void ChangeStation(IshipIGC* ship,IstationIGC* old,IstationIGC* next)override {
        if(uiPreview)return;
        if(old!=next)shipPaneStatus(ship).dockedAt=elapsed;
        economyDirty=true;if(terminating || gameRole==GameRole::Replica)return;
        if(!next) {
            if(old && !ship->GetParentShip() && ship->GetBaseHullType() && !ship->IsGhost() && (!economy() || ship->GetSide()->GetActiveF()) && mission->GetMissionStage()==STAGE_STARTED) {
                for(auto* l=ship->GetChildShips()->first();l;l=l->next())l->data()->SetStation(nullptr);
                old->RepairAndRefuel(ship);old->Launch(ship);
                recordShipCinematic(ship,old,true);
                if(player && ship==player->GetSourceShip() && !player->IsGhost()) {
                    beginCinematic(old,false);
                    throttleResetPending=forcedThrottleReset=true;throttleReset=1;
                }
            }
        } else if(!old && next->GetSide()==ship->GetSide()) {
            lastOwnStations[ship->GetObjectID()]=next->GetObjectID();
            notifyShipHumans(ship,[&]{PlaySoundEffect(dockedSound);PlaySoundEffect(salWelcomeHomeSound);});
            TechTreeBitMask tech;tech.ClearAll();
            for(auto* l=ship->GetParts()->first();l;l=l->next()) {
                auto* part=l->data()->GetPartType();tech|=part->GetEffectTechs();
                if(ship->GetSide()->IsNewDevelopmentTechs(part->GetEffectTechs()))sharedTip(1u<<ship->GetSide()->GetObjectID(),NA,NA,true,"%s has secured %s",ship->GetName(),part->GetName());
            }
            if(ship->GetSide()->ApplyDevelopmentTechs(tech))if(auto* score=scoring(ship))score->AddTechsRecovered();
        }
        // WinTrek's client callback queues RestoreLoadout for every new dock,
        // including rescue/evacuation paths that do not pass through collision.
        if(gameRole==GameRole::Offline && !old && next && ship==player && !training &&
           !ship->GetParentShip() && !ship->IsGhost() && mission->GetMissionStage()==STAGE_STARTED)dockRestorationPending=true;
        updateShipIntel(ship,next!=nullptr);
    }
    void SideBuildingTechChange(IsideIGC*)override{economyDirty=true;}
    void StationTypeChange(IstationIGC*)override{economyDirty=true;}
    void SideGlobalAttributeChange(IsideIGC*)override{economyDirty=true;}
    void LoadoutChangeEvent(IshipIGC* ship,IpartIGC* part,LoadoutChange change)override {
        if(uiPreview)return;
        economyDirty=true;
        if(gameRole==GameRole::Authority && part && part->GetMountID()>=0 && (change==c_lcMounted || change==c_lcDismounted || change==c_lcRemoved)){auto type=part->GetEquipmentType();uint32_t slot=(uint32_t(uint16_t(ship->GetObjectID()))<<16)|uint16_t(type==ET_Magazine?256:part->GetMountID());auto& count=slotShots[slot];if(type==ET_Weapon){auto* w=static_cast<CweaponIGC*>(part);count=std::max(count,w->NativeShots());w->NativeRestore(w->NativeNextFire(),count,w->NativeFlags());}else if(type==ET_Magazine){auto* m=static_cast<CmagazineIGC*>(part);count=std::max(count,m->NativeShots());m->NativeRestoreShots(count);}}
        if(gameRole!=GameRole::Offline && change==c_lcHullChange)++lifeSerial[ship->GetObjectID()];
        if(player && ship==player && change==c_lcHullChange) {
            cameraMotion.SetFOV(RadiansFromDegrees(cameraInput.fieldOfView));engineLevelsValid=false;
        }
        if(change==c_lcHullChange || change==c_lcTurretChange || change==c_lcAddPassenger || change==c_lcRemovePassenger)updateShipIntel(ship);
    }
    void positionCommand(const Vector* focus,float duration) {
        if(!player || !inCommandView())return;
        auto* station=player->GetStation();auto* source=player->GetSourceShip();
        auto* cluster=training && training->viewCluster?static_cast<IclusterIGC*>(training->viewCluster):commandCluster?static_cast<IclusterIGC*>(commandCluster):viewCluster;
        bool atStation=station && station->GetCluster()==cluster;
        Allegiance::PositionCommandView(focus,atStation?station->GetPosition():source->GetPosition(),
            atStation?station->GetRadius()*2:5+source->GetRadius()*2,commandCenter,commandDisplacement,commandDistance);
        commandCameraDirty=true;
        cameraMotion.SetHeadOrientation(0);cameraMotion.SetAnimation(duration);
    }
    void recenterCommand(float duration) {pendingCommandFocus.reset();positionCommand(nullptr,duration);}
    const Vector* commandFocusPosition(int id,uint32_t serial,IclusterIGC* cluster)const {
        auto* ship=mission->GetShip(ShipID(id));if(!ship || playerPaneIdentity(ship)!=serial)return nullptr;
        auto* source=ship->GetSourceShip();auto* station=source->GetStation();
        auto* focus=station?static_cast<ImodelIGC*>(station):static_cast<ImodelIGC*>(source);
        auto* thing=static_cast<Thing*>(source->GetThingSite());
        // Faction status supplies a sector, never a precise remote ship pose.
        // A passenger must wait for the parent's physical record as well.
        if(gameRole==GameRole::Replica && !station && (!thing || thing->replicaStrategic))return nullptr;
        return focus->GetCluster()==cluster && (source->GetSide()==player->GetSide() || focus->GetCurrentEye(player->GetSide()))?&focus->GetPosition():nullptr;
    }
    void resolveCommandFocus() {
        if(!pendingCommandFocus)return;
        auto request=*pendingCommandFocus;
        if(!onTeam() || !inCommandView() || !commandCluster || commandCluster->GetObjectID()!=request.sector){pendingCommandFocus.reset();return;}
        auto& records=shipIntel[player->GetSide()->GetObjectID()];auto status=records.find(ShipID(request.ship));
        if(status==records.end() || status->second.serial!=request.identity || !status->second.status.current() || status->second.status.sector!=request.sector){pendingCommandFocus.reset();return;}
        if(auto* position=commandFocusPosition(request.ship,request.identity,commandCluster)) {
            positionCommand(position,2);pendingCommandFocus.reset();
        }
    }
    void ChangeCluster(IshipIGC* ship,IclusterIGC*,IclusterIGC* next)override {
        if(uiPreview)return;
        updateShipIntel(ship);
        economyDirty=true;if(terminating || ship!=player)return;
        Allegiance::ClearArrivedSectorOrders(ship,next);
        ship->SetStateBits(afterburnerButtonIGC,0);
        // FedSrv keeps an exhausted pilot observing the death sector even
        // after its physical ship docks or transfers to another restart base.
        if(ship->IsGhost() && commandViewEnabled && !next)return;
        commandCluster=nullptr;if(training)training->viewCluster=nullptr;
        pendingCommandFocus.reset();
        if(next)viewCluster=next;
        recenterCommand(0);cameraValid=false;
    }

    void updateEconomy() {
        if(!economy() && !training){if(economyDirty){economyDirty=false;++state.economyRevision;}return;}
        float interval=mission->GetFloatConstant(c_fcidSecondsBetweenPaydays);
        if(gameRole!=GameRole::Replica && economy() && mission->GetMissionParams()->bAllowDevelopments && interval>0 && elapsed>=paydayTime+interval) {
            float treasureInterval=float(elapsed-paydayTime);paydayTime+=interval;economyDirty=true;
            for(auto* l=mission->GetSides()->first();l;l=l->next()) {
                auto* side=l->data();Money income=Money(mission->GetFloatConstant(c_fcidIncome)*(1.f+side->GetCivilization()->GetIncomeMoney()));
                for(auto* st=side->GetStations()->first();st;st=st->next())income+=st->data()->GetStationType()->GetIncome();
                funds[side->GetObjectID()]+=income;distributeIncome(side);
            }
            const auto* params=mission->GetMissionParams();
            for(auto* l=mission->GetClusters()->first();l;l=l->next()) {
                auto* cluster=l->data();bool home=cluster->GetHomeSector();
                float count=cluster->GetPendingTreasures()+treasureInterval*(home?params->nPlayerSectorTreasureRate:params->nNeutralSectorTreasureRate);
                int whole=int(count);cluster->SetPendingTreasures(count-whole);
                for(int i=0;i<whole;++i)mission->GenerateTreasure(mission->GetLastUpdate(),cluster,home?params->tsiPlayerRegenerate:params->tsiNeutralRegenerate);
            }
        }
        if(!economyDirty)return;economyDirty=false;++state.economyRevision;scoreLeadership();
        // Native snapshots consume original bucket state; completion is executed
        // by CbucketIGC::Update and the site callbacks, never by a UI timer.
        state.hullChoices.clear();
        if(player->GetStation())for(auto* l=mission->GetHullTypes()->first();l;l=l->next()) {
            auto* h=l->data();if(h->GetGroupID()>=0 && !h->HasCapability(c_habmLifepod) && player->GetStation()->CanBuy(h) && !player->GetStation()->IsObsolete(h))
                state.hullChoices.push_back({h->GetObjectID(),h->GetName(),h->GetPrice()});
        }

        state.research.clear();state.units.clear();state.pilots.clear();
        if(gameRole==GameRole::Offline && economy()){
            // TeamScreen's public roster is separate from the allied account/
            // location rows. Traverse human accounts only, on economy changes;
            // opposing drones and ship visibility are irrelevant to this list.
            state.networkPilots.clear();state.networkPilots.reserve(accounts.size());
            for(const auto& [id,_]:accounts)if(auto* ship=mission->GetShip(id)){
                int side=ship->GetSide()->GetObjectID();bool controlled=ship==player;
                state.networkPilots.push_back({id,side,ship->GetName(),true,controlled,side>=0 && side<c_cSidesMax && commanders[side]==id,controlled,muted(ship)});
                auto& row=state.networkPilots.back();row.wing=ship->GetSide()==player->GetSide()?ship->GetWingID():-1;
                row.kills=ship->GetKills();row.deaths=ship->GetDeaths();row.ejections=ship->GetEjections();
            }
            std::sort(state.networkPilots.begin(),state.networkPilots.end(),[](const auto& a,const auto& b){return a.id<b.id;});
        }
        if(!onTeam()){state.commander=false;state.moneyRequest={};return;}
        auto* side=player->GetSide();
        for(auto* l=side->GetBuckets()->first();l;l=l->next()) {
            auto* b=l->data();bool drone=b->GetBucketType()==OT_droneType;
            int count=drone?countDrones(side,static_cast<IdroneTypeIGC*>(b->GetBuyable())):-1;
            state.research.push_back({b->GetObjectID(),b->GetBucketType(),b->GetPrice(),b->GetMoney(),
                b->GetTimeToBuild()?b->GetPercentComplete():100,mayInvest(b,count),b->GetCompleteF(),b->GetName()});
            if(drone) {
                auto& row=state.research.back();row.count=count;
                row.limit=mission->GetMissionParams()->nMaxDronesPerTeam;
            }
        }
        state.commander=commanders[side->GetObjectID()]==player->GetObjectID();state.wing=player->GetWingID();
        auto request=moneyRequests.find(player->GetObjectID());state.moneyRequest=request==moneyRequests.end()?MoneyRequest{}:request->second;
        for(auto* l=side->GetShips()->first();l;l=l->next()) {
            auto* ship=l->data();auto* target=ship->GetCommandTarget(c_cmdAccepted);
            std::string activity=commandName(ship->GetCommandID(c_cmdAccepted))+(target?std::string(" / ")+target->GetName():"");
            if(ship->GetPilotType()<c_ptPlayer) {
                if(auto* station=ship->GetStation())activity=std::string(ship->GetStayDocked()?"Stay docked / ":"Docked / ")+station->GetName();
                else if(auto* plan=ship->GetCommandTarget(c_cmdPlan)) {
                    activity=commandName(ship->GetCommandID(c_cmdPlan))+std::string(" / ")+plan->GetName();
                    if(ship->GetPilotType()==c_ptMiner && plan->GetObjectType()==OT_station && ship->GetOre()>0)activity="Unload / "+std::string(plan->GetName());
                }
            }
            state.units.push_back({ship->GetObjectID(),ship->GetPilotType(),target?target->GetObjectID():NA,target?target->GetObjectType():NA,
                ship->GetName(),std::move(activity),ship->GetStation()!=nullptr,ship->GetStayDocked()});
        }
        for(auto* team=mission->GetSides()->first();team;team=team->next()) {
            auto* rosterSide=team->data();if(!IsideIGC::AlliedSides(side,rosterSide))continue;
            bool allied=rosterSide!=side;
            for(auto* link=rosterSide->GetShips()->first();link;link=link->next()) {
                auto* ship=link->data();if(ship->GetPilotType()<c_ptPlayer || (gameRole==GameRole::Authority && !accounts.contains(ship->GetObjectID())))continue;
                auto* source=ship->GetSourceShip();bool known=!allied || source->SeenBySide(side);
                auto* station=known?source->GetStation():nullptr;auto* cluster=known?source->GetCluster():nullptr;auto* hull=known?source->GetBaseHullType():nullptr;
                auto* donation=allied?nullptr:ship->GetAutoDonate();
                state.pilots.push_back({ship->GetObjectID(),rosterSide->GetObjectID(),ship->GetWingID(),allied?alliedBalance(ship->GetObjectID(),rosterSide->GetObjectID()):readBalance(ship),donation?donation->GetObjectID():NA,
                    ship->GetName(),hull?hull->GetName():"",station?station->GetName():cluster?cluster->GetName():"Unknown",
                    commanders[rosterSide->GetObjectID()]==ship->GetObjectID(),ship==player,station!=nullptr,ship->IsGhost(),allied,muted(ship)});
            }
        }
    }

    SessionOptions options;std::vector<Allegiance::CustomMapInfo> customMapCatalog;uint32_t randomState;std::array<int,5> objectiveKey{{-1,-1,-1,-1,-1}};int objectiveStep{};float strafeTime{};bool firedGun{},firedMissile{};

    Impl(const std::filesystem::path& artwork,ModelLoader load,uint32_t effect,uint32_t warp,SessionOptions session,EffectLoader effects,GameRole role)
        :artworkPath(artwork),loader(std::move(load)),effectModel(effect),warpModel(warp),options(std::move(session)),customMapCatalog(options.scenario==Scenario::Conquest?Game::customMaps(artwork):std::vector<Allegiance::CustomMapInfo>{}),randomState(options.seed) {
        gameRole=role;
        Allegiance::SimulationRandomScope randomScope(randomState);
        if(effects) {
            fx=std::make_unique<Particles>(std::move(effects));
            for(int i=0;i<3;++i)shieldModels[i]=loader("lens#lens"+std::to_string((i+1)*30),"fx09");
        }
        if(economy())loader("build","");
        state.sessionName=sessionName(options);
        if(auto error=Game::missionError(options,customMapCatalog);!error.empty())throw std::runtime_error(error);
        if(gameRole!=GameRole::Replica && !options.customMap.empty())if(auto preview=Game::mapPreview(options,artwork);!preview.error.empty())throw std::runtime_error(preview.error);
        options.map=std::clamp(options.map,0,int(economy()?conquestMapNames.size():mapFiles.size())-1);hullIndex=std::clamp(options.hull,0,int(hullIds.size())-1);
        options.difficulty=std::clamp(options.difficulty,0,2);
        try {
        // IGC uses zero-valued timer sentinels as well as signed wraparound
        // comparisons. Keep every match independent of the machine's uptime,
        // including offline training respawns after 24.8 days of uptime.
        UTL::SetArtPath(artwork.string().c_str());start=Time(1000u);mission=CreateMission();mission->Initialize(start,this);
        if(options.trainingMission){MissionParams params;params.nTeams=6;params.bAllowJoiners=options.rules.allowJoiners;params.bAllowDefections=options.rules.allowDefections;params.iMaxImbalance=short(options.rules.maxImbalance);
    if(std::any_of(options.alliances.begin(),options.alliances.begin()+std::clamp(options.rules.teams,0,6),[](char alliance){return alliance>=0;})){params.bAllowDefections=true;params.iMaxImbalance=32767;}
    params.bAllowEmptyTeams=true;mission->SetMissionParams(&params);return;}
        if(options.coreFile.empty())options.coreFile=sessionCoreFile(options.scenario);
        auto version=LoadIGCStaticCore(sessionCoreFile(options),mission,false);
        if(!version)throw std::runtime_error("Game core could not be loaded");
        if(economy())options.alliances=Game::effectiveAlliances(options);
        auto params=nativeMissionParams(options,start);
        mission->SetMissionParams(&params);
        if(economy()){
            // Welcome/configuration must describe the effective IGC policy,
            // including invalid/all-sides groups rejected by UpdateAlliances.
            options.rules.maxImbalance=params.iMaxImbalance;options.rules.allowDefections=params.bAllowDefections;
            options.rules.alliedSight=params.bAllowAlliedViz;options.rules.alliedRipcord=params.bAllowAlliedRip;
        }
        if(!economy()){mission->SetFloatConstant(c_fcidRadiusUniverse,20000);mission->SetFloatConstant(c_fcidOutOfBounds,100);}
        if(gameRole==GameRole::Replica)return;
        constexpr CivID civs[]={25,35,18,25,35,18};
        for(SideID sid=0;sid<params.nTeams;++sid) {
            IcivilizationIGC* civ=nullptr;
            if(economy()) {
                int count=mission->GetCivilizations()->n();
                int index=teamCivilizationIndex(options,sid,count);
                if(index<0)throw std::runtime_error("Civilization is absent from this core.");
                options.teamCivilizations[sid]=index;
                auto* l=mission->GetCivilizations()->first();while(index--)l=l->next();civ=l->data();
            } else civ=mission->GetCivilization(civs[sid]);
            DataSideIGC data{};data.sideID=sid;data.civilizationID=civ->GetObjectID();data.squadID=NA;data.gasAttributes=civ->GetBaseAttributes();
            constexpr float colors[6][3]={{200.f/255,15.f/255,200.f/255},{8.f/255,184.f/255,184.f/255},{0,0,233.f/255},{184.f/255,184.f/255,50.f/255},{184.f/255,92.f/255,0},{123.f/255,61.f/255,61.f/255}};
            if(economy())data.color.SetRGBA(colors[sid][0],colors[sid][1],colors[sid][2]);else data.color.SetRGBA(sid==0?.25f:1.f,sid==0?.75f:.2f,.3f);snprintf(data.name,sizeof(data.name),"%s %d",economy()?civ->GetName():"Training",sid+1);
            data.ttbmDevelopmentTechs=civ->GetBaseTechs();data.ttbmInitialTechs=data.ttbmDevelopmentTechs;
            if(!options.teamNames[sid].empty())std::strcpy(data.name,options.teamNames[sid].c_str());
            auto* side=static_cast<IsideIGC*>(mission->CreateObject(start,OT_side,&data,sizeof(data)));
            if(!side)throw std::runtime_error("Cannot create training side");
            if(!economy()){TechTreeBitMask tech;tech.SetAll();side->SetDevelopmentTechs(tech);side->SetBuildingTechs(tech);}
            if(economy() && !params.bAllowDevelopments)side->ResetGlobalAttributeSet();
            side->SetActiveF(true);
            funds[sid]=Money(mission->GetFloatConstant(c_fcidStartingMoney)*(params.fStartingMoney+civ->GetBonusMoney()));side->Release();
        }
        if(gameRole==GameRole::Authority)for(auto* l=mission->GetSides()->first();l;l=l->next()) {
            // Ejection must not open and parse assets on the simulation tick.
            // Every active civilization can use its pod without researching it.
            auto* pod=l->data()->GetCivilization()->GetLifepod();
            loader(pod->GetModelName(),pod->GetTextureName());
            HitTest::Load(pod->GetModelName()); // Immutable collision-hull cache.
        }
        if(gameRole==GameRole::Authority)for(auto* pack:{mission->GetAmmoPack(),mission->GetFuelPack()}) {
            loader(pack->GetModelName(),"");HitTest::Load(pack->GetModelName());
        }
        if(economy()) {
            auto alliances=Game::effectiveAlliances(options);
            for(SideID sid=0;sid<params.nTeams;++sid)mission->GetSide(sid)->SetAllies(alliances[sid]);
            mission->GenerateMission(start,&params,nullptr,nullptr);
        }
        else if(!LoadIGCFile(mapFiles[options.map],mission))throw std::runtime_error("Scenario world not found");
        if(mission->GetClusters()->n()==0)throw std::runtime_error("Scenario has no sectors");
        selectedHull=economy()?startingHull(mission->GetSide(options.team))->GetObjectID():hullIds[hullIndex];
        for(auto* l=mission->GetClusters()->first();l;l=l->next())l->data()->SetActive(true);
        mission->EnterGame();mission->SetMissionStage(STAGE_STARTED);
        fprintf(stderr,"Offline world: %d sectors, %d stations, %d asteroids\n",mission->GetClusters()->n(),mission->GetStations()->n(),mission->GetAsteroids()->n());
        respawn();
        if(economy()) {
            // Pregame seats exist before the original initial payday. They use
            // the side's starting hull and original commander donation policy.
            for(auto& seat:options.localPilots) {
                auto* side=mission->GetSide(SideID(seat.team));auto* hull=startingHull(side);
                auto* pilot=ship(seat.name.c_str(),SideID(seat.team),hull->GetObjectID(),false);
                for(auto* base=side->GetStations()->first();base;base=base->next())if(base->data()->GetStationType()->HasCapability(c_sabmRestart)){pilot->SetStation(base->data());break;}
                pilot->Release();
            }
            for(auto* team=mission->GetSides()->first();team;team=team->next())distributeIncome(team->data());
            if(params.bAllowDevelopments)for(auto* l=mission->GetSides()->first();l;l=l->next()) {
                auto* side=l->data();auto* station=side->GetStations()->first()?side->GetStations()->first()->data():nullptr;if(!station)continue;
                for(auto* d=mission->GetDroneTypes()->first();d;d=d->next())if(d->data()->GetPilotType()==c_ptMiner && station->CanBuy(d->data())) {
                    for(int i=0;i<params.nInitialMinersPerTeam;++i)createDrone(station,d->data(),nullptr);break;
                }
            }
        } else {spawnBots();nextTarget();}
        if(options.startInHangar)if(auto* station=homeStation())player->SetStation(station);
        if(economy()){
            for(SideID side=0;side<params.nTeams;++side)mission->GetSide(side)->SetActiveF(!(options.inactiveTeams&(1u<<side)));
            if(gameRole==GameRole::Offline)prepareActiveTeams();
        }
        updateEconomy();refresh();objectives({});
        dockRestorationPending=false; // The frontend performs OnEnterMission after loading presets.
        }catch(...){terminating=true;target=nullptr;player=nullptr;if(mission){mission->Terminate();delete mission;mission=nullptr;}throw;}
    }
    ~Impl()override {
        training.reset();cinematicCluster=nullptr;commandCluster=nullptr;terminating=true;presentationPoses.clear();
        for(auto& [_,model]:replicas)
            if(model->GetObjectType()==OT_buoy && model->GetMission())
                static_cast<IbuoyIGC*>(static_cast<ImodelIGC*>(model))->ReleaseConsumer();
        replicas.clear();actors.clear();pendingOrders.clear();lastCameraMissile=nullptr;target=nullptr;player=nullptr;killed.clear();predictionProjectiles.clear();
        if(mission){mission->Terminate();delete mission;}
    }
    void prepareActiveTeams(){
        // FedSrv removes inactive bases and skips initial miners at StartGame.
        // Native lobbies prebuild their world, so retain it while toggling and
        // retire these objects only when play actually begins. These are setup
        // removals, not combat deaths or explosions.
        for(auto* team=mission->GetSides()->first();team;team=team->next())if(auto* side=team->data();!side->GetActiveF()){
            std::vector<TRef<IshipIGC>> drones;
            for(auto* link=side->GetShips()->first();link;link=link->next())if(link->data()->GetPilotType()<c_ptPlayer)drones.emplace_back(link->data());
            for(auto& drone:drones)drone->Terminate();
            while(auto* base=side->GetStations()->first())base->data()->Terminate();
        }
        economyDirty=true;publicationCache.clear();publicationIndexValid=false;
    }
    TRef<ThingSite> CreateThingSite(ImodelIGC* model)override{
        // Detached UI ships run IGC equipment policy, but never enter the live
        // renderer, visibility indexes, sound scene or network identity arena.
        if(uiPreview)return new ThingSite;
        return new Thing(*this,model);
    }
    void Preload(const char* model,const char* texture)override {
        // Original static-core initialization calls this before launch. Cached
        // resources make subsequent load callbacks lookups, rather than uploads.
        if(model && std::isalpha(static_cast<unsigned char>(*model)))loader(model,texture?texture:"");
        else if(fx && texture && std::isalpha(static_cast<unsigned char>(*texture)))fx->load(std::string(texture)+"bmp");
    }
    struct SectorSite : ClusterSite {
        Impl& game;int id;Allegiance::SideScanners scanners;std::array<NativeScannerIndex,c_cSidesMax> scannerIndexes;
        AssetMask assetMask{},warningMask{};SideID warningSide{NA};std::array<AssetMask,c_cSidesMax> networkWarningMasks{};std::array<bool,c_cSidesMax> networkWarningsInitialized{};
        std::vector<SectorStar> stars;std::vector<DrawItem> backgrounds;
        SectorSite(Impl& g,IclusterIGC* sector):game(g),id(sector->GetObjectID()) {
            Allegiance::StarRandom random(sector->GetStarSeed());stars.reserve(std::max(0,int(sector->GetNstars())*4));
            for(int i=0;i<int(sector->GetNstars())*4;++i) {
                float sinPitch=random(-1,1),cosPitch=std::sqrt(1-sinPitch*sinPitch),yaw=random(0,2*pi);
                stars.push_back({{std::cos(yaw)*cosPitch,std::sin(yaw)*cosPitch,sinPitch},random(.25f,1)});
            }
        }
        void SetEnvironmentGeo(const char* name)override {
            if(!name || !*name)return;
            backgrounds.push_back({game.loader(name,"").id,glm::mat4(1),glm::vec4(1),true,ModelBlend::Opaque,0,1,false,1});
        }
        HRESULT AddPoster(const char* name,const Vector& direction,float scale)override {
            auto z=vec(direction),x=glm::normalize(glm::cross(glm::vec3(0,1,0),z));auto y=glm::cross(z,x);
            backgrounds.push_back({game.loader(std::string("$poster:")+name,"").id,modelTransform(x,y,z,z*400.f,scale,false),glm::vec4(1),true,ModelBlend::Alpha,0,1,false,2});return S_OK;
        }
        void AddScanner(SideID side,IscannerIGC* scanner)override{InvalidateVisibilityIndex();scanners.Add(side,scanner);}
        void DeleteScanner(SideID side,IscannerIGC* scanner)override{InvalidateVisibilityIndex();scanners.Delete(side,scanner);}
        const ScannerListIGC* GetScanners(SideID side)const override{return scanners.Get(side);}
        void BeginVisibilityScan()override{for(int i=0;i<c_cSidesMax;++i)scannerIndexes[i].begin(scanners.Get(i));}
        void EndVisibilityScan()override{InvalidateVisibilityIndex();}
        void InvalidateVisibilityIndex()override{for(auto& index:scannerIndexes)index.end();}
        IscannerIGC* FindScanner(SideID side,ImodelIGC* model)override{return scannerIndexes[side].find(scanners.Get(side),model);}
        AssetMask GetClusterAssetMask()override{return assetMask;}
        void SetClusterAssetMask(AssetMask mask)override{assetMask=mask;}
        void updateWarnings(IclusterIGC* cluster) {
            auto side=game.player->GetSide()->GetObjectID();
            if(game.gameRole==GameRole::Authority){
                if(!networkWarningsInitialized[side]){networkWarningsInitialized[side]=true;networkWarningMasks[side]=assetMask;return;}
                auto before=networkWarningMasks[side];if(before==assetMask)return;auto after=assetMask;int old=game.player->GetObjectID();std::vector<int> audience;for(auto* l=game.player->GetSide()->GetShips()->first();l;l=l->next())if(l->data()->GetPilotType()>=c_ptPlayer)audience.push_back(l->data()->GetObjectID());
                for(int pilot:audience)if(game.activateActor(pilot))Allegiance::ClusterWarningChanges(before,after,game.mission->GetMissionStage()==STAGE_STARTED,game.mission->GetMissionParams()->bInvulnerableStations,game.player->GetWingID(),[&](bool critical,const char* text){game.PostText(critical,"%s in %s",text,cluster->GetName());},[&]{game.PostText(false,"%s secured.",cluster->GetName());},[&]{game.PostText(false,"%s lost.",cluster->GetName());},[&](SoundID sound){game.PlaySoundEffect(sound);});
                game.activateActor(old);networkWarningMasks[side]=after;return;
            }
            if(side!=warningSide){warningSide=side;warningMask=assetMask;return;}
            Allegiance::ClusterWarningChanges(warningMask,assetMask,game.mission->GetMissionStage()==STAGE_STARTED,
                game.mission->GetMissionParams()->bInvulnerableStations,game.player->GetWingID(),
                [&](bool critical,const char* text){game.PostText(critical,"%s in %s",text,cluster->GetName());},
                [&]{game.PostText(false,"%s secured.",cluster->GetName());},
                [&]{game.PostText(false,"%s lost.",cluster->GetName());},
                [&](SoundID sound){game.PlaySoundEffect(sound);});
            warningMask=assetMask;
        }
        void Terminate()override{scanners.Clear();}
        void AddExplosion(const Vector& position,float radius,int type)override{
            Net::Event event{};event.kind=Net::EventKind::Explosion;event.position={position.x,position.y,position.z};event.radius=radius;event.value=type;event.sector=id;
            // Anonymous projectile explosions have no model; authorize their
            // visible sector rather than sending their location to other sides.
            for(auto* l=game.mission->GetSides()->first();l;l=l->next())for(auto* s=l->data()->GetShips()->first();s;s=s->next())if(s->data()->GetCluster() && s->data()->GetCluster()->GetObjectID()==id){event.sides|=1u<<l->data()->GetObjectID();break;}game.recordEvent(event);
            if(game.terminating || game.gameRole==GameRole::Authority)return;
            if(game.fx)game.fx->explosion(id,vec(position),{},radius,type);
            if(game.viewCluster && game.viewCluster->GetObjectID()==id)game.sounds.push_back({Allegiance::ExplosionSound(radius),vec(position),{},false});
        }
        void AddExplosion(ImodelIGC* model,int type)override{
            Net::Event event{};event.kind=Net::EventKind::Explosion;auto p=model->GetPosition(),v=model->GetVelocity();event.position={p.x,p.y,p.z};event.velocity={v.x,v.y,v.z};event.radius=2*model->GetRadius();event.value=type;game.recordEvent(event,model);
            if(game.terminating || game.gameRole==GameRole::Authority || !game.eyed(model))return;
            if(game.fx)game.fx->explosion(id,vec(model->GetPosition()),vec(model->GetVelocity()),2*model->GetRadius(),type,vec(model->GetOrientation().GetForward()),vec(model->GetOrientation().GetRight()));
            if(game.viewCluster && game.viewCluster->GetObjectID()==id)game.sounds.push_back({Allegiance::ExplosionSound(2*model->GetRadius()),vec(model->GetPosition()),vec(model->GetVelocity()),false});
        }
    };
    TRef<ClusterSite> CreateClusterSite(IclusterIGC* cluster)override{return new SectorSite(*this,cluster);}
    bool seen(ImodelIGC* model)const {return onTeam() && model->GetVisibleF() && player->CanSee(model);}
    bool eyed(ImodelIGC* model)const {
        return onTeam() && model->GetVisibleF() &&
            (model->GetCurrentEye(player->GetSide()) || model->GetSide()==player->GetSide() || player->InScannerRange(model));
    }
    void PaydayEvent(IsideIGC* side,float money)override {if(gameRole==GameRole::Replica)return;if(side && money>0){funds[side->GetObjectID()]+=Money(money);distributeIncome(side);economyDirty=true;if(player && side==player->GetSide())state.minedCredits+=Money(money);}}
    template<class Visitor> void notifyHumans(IsideIGC* side, Visitor visit) {
        if(gameRole!=GameRole::Authority) {if(player && (!side || side==player->GetSide()))visit();return;}
        int old=player?player->GetObjectID():NA;
        std::vector<int> audience;
        for(auto& [id,account]:accounts)if(auto* pilot=mission->GetShip(id);pilot && (!side || pilot->GetSide()==side))audience.push_back(id);
        for(int id:audience)if(activateActor(id))visit();
        if(old!=NA)activateActor(old);
    }
    template<class Visitor> void forShipHumans(ImodelIGC* model,bool currentIfNull,Visitor visit) {
        auto accept=[&](IshipIGC* ship){if(ship && accounts.contains(ship->GetObjectID()))visit(ship->GetObjectID());};
        if(!model){if(currentIfNull)accept(player);return;}
        visitShipNotificationRecipients(model,accept);
    }
    template<class Visitor> void notifyShipHumans(IshipIGC* ship, Visitor visit) {
        if(gameRole!=GameRole::Authority){if(ship==player)visit();return;}
        int old=player?player->GetObjectID():NA;
        // Actor callbacks can detach or promote crew. Keep IDs, not list links,
        // across those callbacks, just as the former fleet-wide scan did.
        std::vector<int> audience;
        forShipHumans(ship,false,[&](int id){audience.push_back(id);});
        for(int id:audience)if(activateActor(id))visit();
        if(old!=NA)activateActor(old);
    }
    void recordShipCinematic(IshipIGC* ship,ImodelIGC* focus,bool resetThrottle) {
        if(gameRole!=GameRole::Authority)return;
        notifyShipHumans(ship,[&]{Net::Event event{};event.kind=Net::EventKind::Camera;event.pilot=player->GetObjectID();event.flags=resetThrottle?0:2;auto p=focus->GetPosition();event.position={p.x,p.y,p.z};recordEvent(event,focus);});
    }
    void DevelopmentCompleted(IbucketIGC* bucket,IdevelopmentIGC* dev,Time)override {
        Allegiance::CompleteDevelopment(bucket->GetSide(),dev);
        if(dev->GetObjectID()==c_didTeamMoney && !state.complete){state.complete=true;state.endgame.winner=bucket->GetSide()->GetObjectID();state.result=std::string(bucket->GetSide()->GetName())+" wins by prosperity";}
        for(auto* l=bucket->GetSide()->GetShips()->first();l;l=l->next())if(l->data()->GetPilotType()>=c_ptPlayer) {
            PostNotificationText(l->data(),true,"%s research completed",dev->GetName());playPilotSound(dev->GetCompletionSound(),l->data());
        }
    }
    void SideDevelopmentTechChange(IsideIGC* side)override {
        economyDirty=true;Allegiance::UpgradeStations(side);
        upgradeDrones(side);
    }
    void upgradeDrones(IsideIGC* side) {
        for(auto* l=side->GetShips()->first();l;l=l->next())if(l->data()->GetPilotType()<c_ptPlayer && l->data()->GetStation())
            Allegiance::UpgradeDrone(l->data(),l->data()->GetStation());
    }
    void HullTypeCompleted(IsideIGC* side,IhullTypeIGC* hull)override{side->AddToStockpile(hull);}
    void PartTypeCompleted(IsideIGC* side,IpartTypeIGC* part)override{
        side->AddToStockpile(part,IlauncherTypeIGC::IsLauncherType(part->GetEquipmentType())?part->GetAmount(nullptr):1);
    }
    IshipIGC* createDrone(IstationIGC* station,IdroneTypeIGC* type,IbaseIGC* base) {
        auto id=mission->GenerateNewShipID();if(id<0)return nullptr;
        const char* name=base && base->GetObjectType()==OT_stationType?static_cast<IstationTypeIGC*>(base)->GetBuilderName():type->GetName();
        auto* ship=CreateDrone(mission,id,type->GetPilotType(),name,type->GetHullTypeID(),
            station->GetSide(),type->GetPilotType()==c_ptMiner?c_aabmMineHe3:0,type->GetShootSkill(),type->GetMoveSkill(),type->GetBravery());
        if(!ship)return nullptr;droneTypes[ship->GetObjectID()]=type->GetObjectID();if(base)ship->SetBaseData(base);ship->CreateDamageTrack();ship->SetSkills(type->GetShootSkill(),type->GetMoveSkill(),type->GetBravery());
        for(auto* l=ship->GetParts()->first();l;l=l->next())warmPart(l->data());
        rememberLoadout(ship);ship->SetStation(station);station->RepairAndRefuel(ship);
        ship->PickDefaultOrder(station->GetCluster(),station->GetPosition(),true);
        ship->SetStation(nullptr);
        if(type->GetPilotType()==c_ptBuilder && ship->GetCommandID(c_cmdCurrent)==c_cidBuild) {
            TRef<ImodelIGC> rock=ship->GetCommandTarget(c_cmdCurrent);
            ship->SetCommand(c_cmdAccepted,nullptr,c_cidDoNothing);ship->SetCommand(c_cmdPlan,rock,c_cidGoto);ship->SetCommand(c_cmdCurrent,rock,c_cidGoto);
        }
        return ship;
    }
    void DroneTypeCompleted(IbucketIGC*,IstationIGC* station,IdroneTypeIGC* type,Time)override {
        if(!station->GetSide()->GetActiveF())return;
        if(auto* ship=createDrone(station,type,type->GetPilotType()==c_ptLayer?type->GetLaidExpendable():nullptr)) {
            if(type->GetPilotType()==c_ptLayer) {
                bool mine=ship->GetBaseData()->GetObjectType()==OT_mineType;
                SendChat(ship,CHAT_TEAM,ship->GetSide()->GetObjectID(),mine?droneWhereToLayMinefieldSound:droneWhereToLayTowerSound,
                    mine?"New minefield requesting location.":"New tower requesting location.");
            } else if(type->GetPilotType()==c_ptMiner && ship->GetCommandID(c_cmdCurrent)!=c_cidMine)
                SendChat(ship,CHAT_TEAM,ship->GetSide()->GetObjectID(),droneWhereToSound,"New miner requesting He3 asteroid.");
        }
    }
    void StationTypeCompleted(IbucketIGC*,IstationIGC* station,IstationTypeIGC* type,Time)override {
        if(station->GetSide()->GetActiveF())if(auto* ship=createDrone(station,type->GetConstructionDroneType(),type))
            SendChat(ship,CHAT_TEAM,ship->GetSide()->GetObjectID(),type->GetConstructorNeedRockSound(),"New constructor requesting asteroid.");
    }
    void CreateBuildingEffect(Time now,IasteroidIGC* asteroid,IshipIGC* builder)override {
        auto* type=static_cast<IstationTypeIGC*>(builder->GetBaseData());
        if(!type || asteroid->GetBuildingEffect())return;
        asteroid->GetCluster()->CreateBuildingEffect(now,asteroid,nullptr,builder,asteroid->GetRadius(),type->GetRadius(),
            builder->GetPosition()-builder->GetOrientation().GetBackward()*builder->GetRadius(),asteroid->GetPosition());
        for(auto* l=mission->GetSides()->first();l;l=l->next())if(builder->SeenBySide(l->data()) || asteroid->GetCurrentEye(l->data()))
            asteroid->SetBuilderSeenSide(l->data()->GetObjectID());
    }
    void KillBuildingEffectEvent(IbuildingEffectIGC* effect)override{effect->Terminate();}
    void BuildStation(IasteroidIGC* asteroid,IsideIGC* side,IstationTypeIGC* type,Time now,bool seenSides[])override {
        if(training) {
            if(auto* station=Allegiance::BuildOfflineStation(asteroid,side,type,now)) {
                station->Release();
            }
            return;
        }
        auto* station=Allegiance::ConstructStation(asteroid,side,type,now);if(!station)return;
        bool selected=target==asteroid;int asteroidId=asteroid->GetObjectID();
        unsigned retained=0;
        for(auto* l=mission->GetSides()->first();l;l=l->next()) {
            auto* viewer=l->data();auto id=viewer->GetObjectID();
            bool announced=viewer==side || asteroid->GetAsteroidCurrentEye(viewer) || asteroid->GetBuilderSeenSide(id) || seenSides[id];
            if(announced)station->SetSideVisibility(viewer,true);
            else if(!station->SeenBySide(viewer)) {
                // Populate the original IGC stale-rock tracking. ClusterIGC
                // already clears it when a ship scans the former asteroid site.
                station->SetRoidSig(asteroid->GetSignature());station->SetRoidPos(asteroid->GetPosition());
                station->SetRoidRadius(asteroid->GetRadius());station->SetRoidCaps(asteroid->GetCapabilities());station->SetRoidSide(id);
                if(asteroid->SeenBySide(viewer))retained|=1u<<id;
            }
        }
        if(retained) {
            auto* thing=static_cast<Thing*>(asteroid->GetThingSite());
            RememberedRock rock{asteroid->GetObjectID(),asteroid->GetCluster()->GetObjectID(),retained,asteroid->GetCapabilities(),
                vec(asteroid->GetPosition()),asteroid->GetRadius(),
                {thing->handle.id,matrix(asteroid->GetOrientation(),asteroid->GetPosition(),thing->radius/thing->handle.radius,thing->flip),thing->color,thing->unlit},thing->handle,GetModelName(asteroid),GetModelType(asteroid),{}};
            if(auto* icon=static_cast<Icon*>(asteroid->GetIcon()))rock.icon=icon->name;
            rememberedRocks.push_back(std::move(rock));investmentHintsDirty=mapRockKnowledgeDirty=true;
        }
        asteroid->Terminate();station->Release();
        if(selected && player && (retained&(1u<<player->GetSide()->GetObjectID())))selectTarget(OT_asteroid,asteroidId);
        upgradeDrones(side);side->UpdateTerritory();checkConquest();
    }
    void KillAsteroidEvent(IasteroidIGC* asteroid,bool explode)override {
        if(explode && (gameRole==GameRole::Authority || eyed(asteroid)))asteroid->GetCluster()->GetClusterSite()->AddExplosion(asteroid,c_etAsteroid);asteroid->Terminate();
    }
    void CaptureStationEvent(IshipIGC* ship,IstationIGC* station)override {
        scoreBase(ship,true);vacateStation(station);station->SetFraction(station->GetFraction()*.5f);auto* type=station->GetStationType();
        station->SetShieldFraction(type->GetMaxShieldHitPoints()?7.f*type->GetShieldRegeneration()/type->GetMaxShieldHitPoints():0);
        TRef<IsideIGC> previous=station->GetSide();ship->GetSide()->AddBaseCapture();
        notifyHumans(nullptr,[&]{
            if(gameRole!=GameRole::Authority || station->SeenBySide(player->GetSide())) {
                PlaySoundEffect(Allegiance::StationCaptureSound(type,IsideIGC::AlliedSides(previous,player->GetSide()),IsideIGC::AlliedSides(ship->GetSide(),player->GetSide())));
                PostText(true,"%s captured %s's %s in %s.",ship->GetName(),previous->GetName(),station->GetName(),station->GetCluster()->GetName());
            }
        });
        station->SetSide(ship->GetSide());
        for(auto* l=mission->GetSides()->first();l;l=l->next())if(station->SeenBySide(l->data())) {
            auto sid=l->data()->GetObjectID();auto owner=ship->GetSide()->GetObjectID();
            auto& history=stationHistory[station->GetObjectID()][sid];history.intel.side=owner;history.record.sideID=owner;
            auto key=(uint32_t(OT_station)<<16)|uint16_t(station->GetObjectID());
            if(auto i=observedRecords[sid].find(key);i!=observedRecords[sid].end()) {
                i->second.entity.side=owner;
                if(i->second.payload.size()==sizeof(DataStationIGC)) {
                    DataStationIGC record;std::memcpy(&record,i->second.payload.data(),sizeof(record));record.sideID=owner;
                    std::memcpy(i->second.payload.data(),&record,sizeof(record));
                }
            }
        }
        previous->UpdateTerritory();ship->GetSide()->UpdateTerritory();checkConquest(nullptr,previous);
    }
    void WarpBombEvent(IwarpIGC* warp,ImissileIGC* missile)override {
        warp->AddBomb(warp->GetLastUpdate()+mission->GetFloatConstant(c_fcidWarpBombDelay),missile->GetMissileType());
    }
    void LayExpendable(Time now,IexpendableTypeIGC* type,IshipIGC* layer)override {
        auto* cluster=layer->GetCluster();if(!cluster)return;
        if(type->GetObjectType()==OT_mineType) {
            DataMineIGC data{};data.pshipLauncher=layer;data.psideLauncher=layer->GetSide();data.mineID=mission->GenerateNewMineID();
            data.time0=now;data.p0=layer->GetPosition();data.pminetype=static_cast<ImineTypeIGC*>(type);data.pcluster=cluster;
            if(auto* obj=mission->CreateObject(now,OT_mine,&data,sizeof(data)))obj->Release();
        } else if(type->GetObjectType()==OT_probeType) {
            DataProbeIGC data{};data.pside=layer->GetSide();data.probeID=mission->GenerateNewProbeID();data.time0=now;
            data.p0=layer->GetPosition();data.pprobetype=static_cast<IprobeTypeIGC*>(type);data.pcluster=cluster;
            if(auto* obj=mission->CreateObject(now,OT_probe,&data,sizeof(data)))obj->Release();
        }
        while(auto* l=layer->GetParts()->first())l->data()->Terminate();layer->SetAmmo(0);layer->SetFuel(0);
        KillShipEvent(now,layer,nullptr,0,layer->GetPosition(),Vector::GetZero());
    }
    bool receivesChat(IshipIGC* to,IshipIGC* sender,ChatTarget channel,ObjectID recipient) const {
        if(!to)return false;
        auto* side=to->GetSide();auto* source=to->GetSourceShip();
        auto* senderSide=sender?sender->GetSide():nullptr;
        auto* cluster=source->GetCluster();if(!cluster && source->GetStation())cluster=source->GetStation()->GetCluster();
        switch(channel) {
        case CHAT_EVERYONE:return true;
        case CHAT_LEADERS:return side && side->GetObjectID()>=0 && side->GetObjectID()<6 && commanders[side->GetObjectID()]==to->GetObjectID();
        case CHAT_ADMIN:return false; // ForwardChatMessage's admin route is NYI.
        case CHAT_TEAM:return side && recipient==side->GetObjectID();
        case CHAT_ALLIES:{auto* to=mission->GetSide(SideID(recipient));return to && (to==side || IsideIGC::AlliedSides(to,side));}
        case CHAT_INDIVIDUAL:case CHAT_INDIVIDUAL_NOFILTER:case CHAT_GROUP:case CHAT_GROUP_NOECHO:return recipient==to->GetObjectID();
        case CHAT_INDIVIDUAL_ECHO:return senderSide==side;
        case CHAT_SHIP:return sender && sender->GetSourceShip()==source;
        case CHAT_WING:return senderSide==side && recipient==to->GetWingID();
        case CHAT_ALL_SECTOR:return cluster && recipient==cluster->GetObjectID();
        // FedSrv's friendly-sector route walks flying ships only. All-sector
        // chat also addresses the separate docked group, as handled above.
        case CHAT_FRIENDLY_SECTOR:return source->GetCluster() && recipient==source->GetCluster()->GetObjectID() && senderSide && (senderSide==side || IsideIGC::AlliedSides(senderSide,side));
        default:return false;
        }
    }
    bool receivesChat(IshipIGC* sender,ChatTarget channel,ObjectID recipient)const{return receivesChat(player,sender,channel,recipient);}
    struct PendingOrder {TRef<IshipIGC> sender;double deadline;};
    std::unordered_map<ShipID,PendingOrder> pendingOrders;
    double nextOrderDeadline{DBL_MAX};
    bool rejectRequest(IshipIGC* recipient,bool clear,bool negative) {
        int id=recipient->GetObjectID();auto request=moneyRequests.find(id);
        auto pending=pendingOrders.find(id);TRef<IshipIGC> sender=pending==pendingOrders.end()?nullptr:static_cast<IshipIGC*>(pending->second.sender);
        return Allegiance::RejectQueuedRequest(request==moneyRequests.end()?0:request->second.amount,recipient->GetCommandID(c_cmdQueued),clear,negative,recipient,
            [&]{moneyRequests.erase(id);economyDirty=true;},
            [&]{recipient->SetCommand(c_cmdQueued,nullptr,c_cidNone);},
            [&]()->IshipIGC*{return sender;},
            [&]{pendingOrders.erase(id);},
            [&](IshipIGC* from){SendChat(recipient,CHAT_INDIVIDUAL,from->GetObjectID(),voNegativeSound,"Negative.");});
    }
    void queueOrder(IshipIGC* recipient,IshipIGC* sender,ImodelIGC* object,CommandID command) {
        if(economy() && !recipient->GetSide()->GetActiveF())return;
        auto id=recipient->GetObjectID();auto found=pendingOrders.find(id);
        if(found!=pendingOrders.end() && found->second.sender!=sender)rejectRequest(recipient,true,false);
        if(moneyRequests.erase(id))economyDirty=true;
        double deadline=training?DBL_MAX:elapsed+Allegiance::QueuedRequestLifetime;
        pendingOrders[id]={sender,deadline};nextOrderDeadline=std::min(nextOrderDeadline,deadline);
        recipient->SetCommand(c_cmdQueued,object,command);
    }
    void queueMoneyRequest(IshipIGC* recipient,IshipIGC* sender,Money amount,HullID hull) {
        if(economy() && !recipient->GetSide()->GetActiveF())return;
        auto id=recipient->GetObjectID();auto found=pendingOrders.find(id);
        if(found!=pendingOrders.end() && found->second.sender!=sender)rejectRequest(recipient,true,false);
        MoneyRequest request{sender->GetObjectID(),amount,hull,sender->GetName()};
        auto* type=mission->GetHullType(hull);
        request.prompt=Allegiance::MoneyRequestPrompt(request.name,amount,type?type->GetName():"");
        request.displayPrompt=censorChat(request.prompt,chatPrefs.censor);
        if(sender!=recipient)PostNotificationText(recipient,true,"%s",request.prompt.c_str());
        moneyRequests[id]=std::move(request);economyDirty=true;
        double deadline=elapsed+Allegiance::QueuedRequestLifetime;pendingOrders[id]={sender,deadline};nextOrderDeadline=std::min(nextOrderDeadline,deadline);
        recipient->SetCommand(c_cmdQueued,nullptr,c_cidNone);
    }
    void expireOrders() {
        if(elapsed<=nextOrderDeadline)return;
        nextOrderDeadline=DBL_MAX;
        for(auto it=pendingOrders.begin();it!=pendingOrders.end();) {
            auto* recipient=mission->GetShip(it->first);
            if(!recipient || elapsed>it->second.deadline) {
                // RejectQueuedCommand(false) forgets its sender (and money),
                // retaining a queued order so the pilot may still accept it.
                if(moneyRequests.erase(it->first))economyDirty=true;
                it=pendingOrders.erase(it);
            } else {nextOrderDeadline=std::min(nextOrderDeadline,it->second.deadline);++it;}
        }
    }
    void ReceiveChat(IshipIGC* sender,ChatTarget channel,ObjectID recipient,SoundID voice,const char* text,CommandID command,ObjectType targetType,ObjectID targetId,ImodelIGC* object=nullptr)override {
        receiveChat(sender,channel,recipient,voice,text,command,targetType,targetId,object,false);
    }
    std::string chatRecipientName(ChatTarget channel,ObjectID recipient) const {
        switch(channel) {
        case CHAT_EVERYONE:return "all";
        case CHAT_ALLIES:return "allies";
        case CHAT_LEADERS:return "leaders";
        case CHAT_ADMIN:return "admin";
        case CHAT_SHIP:return "ship";
        case CHAT_GROUP:case CHAT_GROUP_NOECHO:return "group";
        case CHAT_TEAM:{auto* side=recipient==NA?player->GetSide():mission->GetSide(SideID(recipient));return side?side->GetName():"<unknown>";}
        case CHAT_ALL_SECTOR:case CHAT_FRIENDLY_SECTOR:{
            auto* cluster=recipient==NA?viewCluster:mission->GetCluster(recipient);
            if(!cluster && recipient==NA && player->GetSourceShip()->GetStation())cluster=player->GetSourceShip()->GetStation()->GetCluster();
            return cluster?cluster->GetName():"<unknown>";
        }
        case CHAT_WING:{int wing=recipient==NA?player->GetWingID():recipient;return c_pszWingName[wing>=0 && wing<c_widMax?wing:0];}
        case CHAT_INDIVIDUAL:case CHAT_INDIVIDUAL_NOFILTER:case CHAT_INDIVIDUAL_ECHO:{
            auto* ship=recipient==NA?static_cast<IshipIGC*>(player):mission->GetShip(ShipID(recipient));return ship?ship->GetName():"<unknown>";
        }
        default:return "none";
        }
    }
    std::string chatOrderText(const char* text,CommandID command,ImodelIGC* object) const {
        if(text)return text;
        if(command<0 || command>=c_cidMax)return {};
        std::string order=c_cdAllCommands[command].szVerb;
        if(object) {
            order+=' ';order+=GetModelName(object);
            if(object->GetObjectType()!=OT_buoy || static_cast<IbuoyIGC*>(object)->GetBuoyType()!=c_buoyCluster)
                if(auto* cluster=object->GetCluster();cluster && player->CanSee(object)){order+=" in ";order+=cluster->GetName();}
        }
        return order;
    }
    void receiveChat(IshipIGC* sender,ChatTarget channel,ObjectID recipient,SoundID voice,const char* text,CommandID command,ObjectType targetType,ObjectID targetId,ImodelIGC* object,bool quick,bool localEcho=false) {
        if(channel==CHAT_GROUP_NOECHO)return; // This send-only channel must not reach WinTrek's receive path.
        bool forMe=receivesChat(sender,channel,recipient);
        if(!forMe && !localEcho)return;
        bool sonic=!text && voice!=NA;
        auto* definition=sonic && quickChatCatalog?quickChatCatalog->sonicChat(voice):nullptr;
        auto* recorded=definition && !definition->voices.empty()?&definition->voices[0]:nullptr;
        // Recipient display preferences belong to the client, not the server's
        // currently activated actor. Retain delivery facts for that decision.
        const bool human=sender && sender->GetPilotType()>=c_ptPlayer;
        const bool lobby=sender && (!sender->GetSide() || sender->GetSide()->GetObjectID()==SIDE_TEAMLOBBY);
        const bool individual=channel==CHAT_INDIVIDUAL;
        const bool leader=!sender?channel==CHAT_ADMIN:sender->GetSide() && sender->GetSide()->GetObjectID()>=0 && commanders[sender->GetSide()->GetObjectID()]==sender->GetObjectID();
        Net::Notice details{};details.sender=sender?sender->GetObjectID():-1;
        details.flags=Net::NoticeChat|(sender?Net::NoticeSender:0u)|(human?Net::NoticeHuman:0u)|
            (channel==CHAT_EVERYONE?Net::NoticeEveryone:0u)|(lobby?Net::NoticeLobby:0u)|(individual?Net::NoticeIndividual:0u)|
            (text?Net::NoticeHasText:0u)|(voice!=NA?Net::NoticeHasVoice:0u)|(recorded && recorded->text!="Unknown chat"?Net::NoticeKnownVoice:0u)|
            (quick?Net::NoticeQuick:0u)|(leader?Net::NoticeLeader:0u)|(command!=c_cidNone?Net::NoticeCommand:0u)|
            (channel==CHAT_INDIVIDUAL_ECHO?Net::NoticeEcho:0u)|(training?Net::NoticeTraining:0u)|
            (individual && (recipient==NA || recipient==player->GetObjectID())?Net::NoticePersonal:0u)|Net::noticeChannel(int(channel));
        auto policy=gameRole==GameRole::Authority?recipientPreferences(player->GetObjectID()):chatPrefs;
        auto disposition=Allegiance::FilterChat(policy,sender!=nullptr,human,
            gameRole==GameRole::Authority?recipientMuted(player->GetObjectID(),details.sender):muted(sender),channel==CHAT_EVERYONE,text!=nullptr,voice!=NA,recorded && recorded->text!="Unknown chat",quick,
            lobby,individual,gameRole==GameRole::Authority?mission->GetMissionStage()==STAGE_STARTED && onTeam():!state.networkLobby);
        if(disposition.discard)return;
        // Unknown out-of-table sonic IDs are silent in PlaySonicChat. An empty
        // registered voice list retains its original direct-sound fallback.
        bool silent=sonic && !definition && !disposition.quietVoice;
        if(sonic){text=recorded?recorded->text.c_str():"Unknown chat";if(recorded)voice=recorded->sound;}
        if(disposition.quietVoice)voice=NA;
        std::string adapted;if(text && training && training->promptFormatter){adapted=training->promptFormatter(text);text=adapted.c_str();}
        if(gameplayClosed() && command!=c_cidNone)return;
        if(command>c_cidNone && forMe && sender!=player && !Allegiance::ChatOrderAuthorized(channel,sender?sender->GetSide():nullptr,player->GetSide()))return;
        if(training)Training::RecordChat(channel);
        if(!object)object=mission->GetModel(targetType,targetId);
        if(training && sender && recipient==sender->GetObjectID() && !Training::CommandViewEnabled())object=nullptr;
        if(object && command==c_cidDefault && (channel==CHAT_INDIVIDUAL || channel==CHAT_INDIVIDUAL_NOFILTER || channel==CHAT_INDIVIDUAL_ECHO || channel==CHAT_GROUP || channel==CHAT_GROUP_NOECHO)) {
            auto* to=recipient==NA?static_cast<IshipIGC*>(player):mission->GetShip(ShipID(recipient));if(to)command=to->GetDefaultOrder(object);
        }
        // HQ's original receive path always displays as addressed to the pilot.
        if(!sender)forMe=true;
        auto order=chatOrderText(text,command,object);
        bool recordedChat=Allegiance::RecordChat(command!=c_cidNone,channel==CHAT_INDIVIDUAL_ECHO,forMe,channel==CHAT_INDIVIDUAL || channel==CHAT_INDIVIDUAL_NOFILTER,sender==player);
        if(recordedChat) {
            auto* side=sender && sender->GetSide()?sender->GetSide():player->GetSide();
            auto color=side?side->GetColor():Color::White();
            float tint=sender?Allegiance::ChatTintScale(sender==player,forMe,command!=c_cidNone):1.f;
            details.voice=voice;details.flags|=silent?Net::NoticeSilent:0u;
            message(std::string(sender?sender->GetName():"HQ")+" ("+chatRecipientName(channel,recipient)+"): "+order,false,
                {color.R()*tint,color.G()*tint,color.B()*tint,1},true,leader,&details);
        }
        // A delivered chat notice owns its sound; filtering cannot leave a
        // detached recording playing after its text was discarded.
        if(!silent && (gameRole!=GameRole::Authority || !recordedChat))PlaySoundEffect(Allegiance::ChatSound(voice,command!=c_cidNone,channel==CHAT_INDIVIDUAL_ECHO,
            channel==CHAT_INDIVIDUAL && (recipient==NA || recipient==player->GetObjectID()),bool(training),gameRole!=GameRole::Offline,leader && sender!=player));
        if(forMe && command>c_cidNone && command<c_cidMax && channel!=CHAT_INDIVIDUAL_ECHO) {
            if(object && channel!=CHAT_LEADERS && channel!=CHAT_ADMIN && player->LegalCommand(command,object)) {
                if(sender==player && channel==CHAT_INDIVIDUAL) {
                    // WinTrek's individual order to self is immediately accepted.
                    player->SetCommand(c_cmdAccepted,object,command);setTarget(object,false,command);
                    if(player->GetCluster() && !player->GetParentShip() && object->GetCluster() && player->CanSee(object)) {
                        player->SetAutopilot(true);autopilotInputInitialized=false;PlaySoundEffect(salAutopilotEngageSound);
                    }
                } else {
                    queueOrder(player,sender,object,command);
                    if(sender && sender!=player) {
                        if(Allegiance::RecoveryOrderTip(command==c_cidPickup,object==sender,[&]{return sender->GetBaseHullType() && sender->GetBaseHullType()->HasCapability(c_habmRescue);}))
                            PostText(true,"New orders from %s: prepare for recovery. Press [insert] to accept.",sender->GetName());
                        else if(leader)PostText(true,START_COLOR_STRING "New orders from %s to %s: %s. Press [insert] to accept." END_COLOR_STRING,(PCC)ConvertColorToString(Color::Orange()),sender->GetName(),chatRecipientName(channel,recipient).c_str(),order.c_str());
                        else PostText(true,"New orders from %s to %s: %s. Press [insert] to accept.",sender->GetName(),chatRecipientName(channel,recipient).c_str(),order.c_str());
                    }
                }
            }
        }
    }
    void SendChat(IshipIGC* sender,ChatTarget channel,ObjectID recipient,SoundID voice,const char* text,CommandID command=c_cidNone,ObjectType targetType=NA,ObjectID targetId=NA,ImodelIGC* object=nullptr,bool=false)override {
        sendChat(sender,channel,recipient,voice,text,command,targetType,targetId,object,false);
    }
    void sendChat(IshipIGC* sender,ChatTarget channel,ObjectID recipient,SoundID voice,const char* text,CommandID command,ObjectType targetType,ObjectID targetId,ImodelIGC* object,bool quick,bool echoAuthor=true) {
        auto* knownVoice=(!text && voice!=NA && quickChatCatalog)?quickChatCatalog->voiceForChat(voice):nullptr;
        bool known=knownVoice && knownVoice->text!="Unknown chat";
        bool filtered=gameRole!=GameRole::Authority && Allegiance::FilterChat(chatPrefs,sender!=nullptr,sender && sender->GetPilotType()>=c_ptPlayer,
            muted(sender),channel==CHAT_EVERYONE,text!=nullptr,voice!=NA,known,quick).discard;
        if(!state.complete && !filtered && command>c_cidNone && Allegiance::ChatCarriesOrders(channel)) {
            if(!object)object=mission->GetModel(targetType,targetId);
            auto deliver=[&](IshipIGC* to){
                if(!object || !to || to==player || to->GetPilotType()<c_ptPlayer || !receivesChat(to,sender,channel,recipient) || !Allegiance::ChatOrderAuthorized(channel,sender?sender->GetSide():nullptr,to->GetSide()))return;
                if(gameRole==GameRole::Authority && Allegiance::FilterChat(recipientPreferences(to->GetObjectID()),sender!=nullptr,sender && sender->GetPilotType()>=c_ptPlayer,
                    recipientMuted(to->GetObjectID(),sender?sender->GetObjectID():NA),channel==CHAT_EVERYONE,text!=nullptr,voice!=NA,known,quick,
                    sender && (!sender->GetSide() || sender->GetSide()->GetObjectID()==SIDE_TEAMLOBBY),channel==CHAT_INDIVIDUAL,
                    mission->GetMissionStage()==STAGE_STARTED && to->GetSide() && to->GetSide()->GetObjectID()>=0).discard)return;
                auto resolved=command;
                if(command==c_cidDefault && (channel==CHAT_INDIVIDUAL || channel==CHAT_INDIVIDUAL_NOFILTER || channel==CHAT_GROUP || channel==CHAT_GROUP_NOECHO))resolved=to->GetDefaultOrder(object);
                if(to->LegalCommand(resolved,object))queueOrder(to,sender,object,resolved);
            };
            const ShipListIGC* ships=nullptr;
            switch(channel){
            case CHAT_INDIVIDUAL:case CHAT_INDIVIDUAL_NOFILTER:case CHAT_GROUP:case CHAT_GROUP_NOECHO:deliver(mission->GetShip(ShipID(recipient)));break;
            case CHAT_TEAM:if(auto* side=mission->GetSide(SideID(recipient)))ships=side->GetShips();break;
            case CHAT_WING:if(sender && sender->GetSide())ships=sender->GetSide()->GetShips();break;
            case CHAT_FRIENDLY_SECTOR:if(auto* cluster=mission->GetCluster(recipient))ships=cluster->GetShips();break;
            case CHAT_SHIP:if(sender){auto* source=sender->GetSourceShip();deliver(source);ships=source->GetChildShips();}break;
            default:break;
            }
            if(ships)for(auto* link=ships->first();link;link=link->next())deliver(link->data());
        }
        if(gameRole==GameRole::Authority) {
            int old=player->GetObjectID();std::vector<int> audience;
            for(auto* l=mission->GetShips()->first();l;l=l->next()){auto* to=l->data();if(to->GetPilotType()>=c_ptPlayer && (to==sender || receivesChat(to,sender,channel,recipient)))audience.push_back(to->GetObjectID());}
            for(int id:audience)if(activateActor(id)) {
                if(sender==player && !echoAuthor)continue;
                if(sender==player && channel==CHAT_GROUP_NOECHO)continue;
                auto delivered=sender!=player && (channel==CHAT_GROUP || channel==CHAT_GROUP_NOECHO)?CHAT_INDIVIDUAL:channel;
                receiveChat(sender,delivered,recipient,voice,text,command,targetType,targetId,object,quick,sender==player);
            }
            activateActor(old);return;
        }
        // BaseClient echoes our outgoing message with its original recipient,
        // even when we are not part of that audience. Do not turn it into a
        // private-message echo or queue the remote pilot's command on ourselves.
        if(sender==player && (channel==CHAT_GROUP_NOECHO || !echoAuthor))return;
        // FedSrv turns a group's per-pilot delivery into an individual chat.
        // The local author's ordinary group echo retains its original channel.
        auto delivered=sender!=player && (channel==CHAT_GROUP || channel==CHAT_GROUP_NOECHO)?CHAT_INDIVIDUAL:channel;
        receiveChat(sender,delivered,recipient,voice,text,command,targetType,targetId,object,quick,sender==player);
    }
    std::shared_ptr<SoundEmitter> emitterFor(ImodelIGC* model) {
        auto* thing=static_cast<Thing*>(model->GetThingSite());if(!thing)return {};
        if(!thing->emitter)thing->emitter=std::make_shared<SoundEmitter>();
        auto& emitter=*thing->emitter;emitter.active=true;emitter.relative=model==player && cameraMode(cameraInput)==CameraMode::Cockpit;
        emitter.position=vec(model->GetPosition());emitter.velocity=vec(model->GetVelocity());emitter.orientation=glm::mat3(matrix(model->GetOrientation(),Vector::GetZero(),1));
        return thing->emitter;
    }
    void PlaySoundEffect(SoundID id,ImodelIGC* model=nullptr)override {PlaySoundEffect(id,model,Vector::GetZero());}
    void PlaySoundEffect(SoundID id,ImodelIGC* model,const Vector& offset)override {
        if(networkReplaying || uiPreview)return;
        if(gameRole==GameRole::Authority && id>=0){Net::Event event{};event.kind=Net::EventKind::Sound;event.value=id;if(model){auto p=model->GetPosition()+offset*model->GetOrientation(),v=model->GetVelocity();event.position={p.x,p.y,p.z};event.velocity={v.x,v.y,v.z};}else if(player)event.pilot=player->GetObjectID();recordEvent(event,model);return;}
        if(id<0 || (model && model!=player && (model->GetCluster()!=viewCluster || !eyed(model))))return;
        bool relative=!model || (model==player && cameraMode(cameraInput)==CameraMode::Cockpit);
        sounds.push_back({id,relative?vec(offset):vec(model->GetPosition()+offset*model->GetOrientation()),
            relative?glm::vec3(0):vec(model->GetVelocity()),relative,{0,0,-1},model?emitterFor(model):nullptr,vec(offset)});
    }
    void PlayNotificationSound(SoundID id,ImodelIGC* model)override {
        if(uiPreview)return;
        if(networkReplaying || id<0)return;
        // WinTrek notifications belong to the player's ship (or its parent
        // while manning a turret), not every ship that runs an IGC callback.
        if(gameRole==GameRole::Authority){forShipHumans(model,false,[&](int pilot){Net::Event event{};event.kind=Net::EventKind::Sound;event.value=id;event.pilot=pilot;event.flags=Net::SoundOwnShip;recordEvent(event,model);});return;}
        if(player && Allegiance::ReceivesShipNotification(player,model))PlaySoundEffect(id,model);
    }
    void PlayVisualEffect(VisualEffectID effect,ImodelIGC* model,float intensity)override {
        if(gameRole==GameRole::Authority && effect==effectJiggle){forShipHumans(model,true,[&](int id){Net::Event event{};event.kind=Net::EventKind::Jiggle;event.radius=intensity;event.pilot=id;recordEvent(event,model);});return;}
        if(effect==effectJiggle && (!model || model==player))cameraMotion.SetJiggle(intensity);
    }
    void deathExplosion(ImodelIGC* model) {
        if(model->GetCluster() && (model->GetObjectType()==OT_ship || model->GetObjectType()==OT_station)) {
            int type=model->GetObjectType()==OT_station
                ? (static_cast<IstationIGC*>(model)->GetStationType()->HasCapability(c_sabmFlag)?c_etLargeStation:c_etSmallStation)
                : (static_cast<IshipIGC*>(model)->GetBaseHullType()->HasCapability(c_habmFighter)?c_etSmallShip:c_etBigShip);
            model->GetCluster()->GetClusterSite()->AddExplosion(model,type);
        }
    }
    void kill(ImodelIGC* model) {
        if(terminating || std::any_of(killed.begin(),killed.end(),[&](auto& x){return x==model;}))return;
        deathExplosion(model);
        killed.emplace_back(model);
    }
    void message(std::string text,bool warning=false,glm::vec4 color={.85f,.85f,.85f,1},bool chat=false,bool leader=false,const Net::Notice* details=nullptr) {
        if(networkReplaying)return;
        if(!chat && !warning && !(details && (details->flags&Net::NoticeTip)) && !state.messages.empty() && !state.messages.back().chat && state.messages.back().text==text && elapsed-state.messages.back().time<1)return;
        if(gameRole==GameRole::Authority && player){Net::Notice n=details?*details:Net::Notice{};n.number=++lastNotice;n.time=elapsed;n.flags|=unsigned(warning)|(unsigned(chat)<<1);n.color=Net::noticeColor(color.r,color.g,color.b,color.a);Net::text(n.text,text);publicationEffectsIndexed=false;networkNotices.push_back({player->GetObjectID(),n});}
        auto display=censorChat(text,chatPrefs.censor);
        state.messages.push_back({elapsed,std::move(text),warning,++messageSerial,std::move(display),displayRevision,uiTime(),color,chat,leader,details && (details->flags&Net::NoticeModal),details?Net::noticeChannel(details->flags):-1,details && (details->flags&Net::NoticeHuman),details && (details->flags&Net::NoticeTip)});
        while(state.messages.size()>Allegiance::ChatHistoryCapacity)state.messages.pop_front();
    }
    void PostPlainText(bool critical,const char* text)override {
        std::string adapted;if(interfaceFormatter){adapted=interfaceFormatter(text);text=adapted.c_str();}
        Net::Notice tip{};tip.flags=Net::NoticeTip;
        message(text,critical,{.85f,.85f,.85f,1},false,false,&tip);PlaySoundEffect(critical?newCriticalMsgSound:newNonCriticalMsgSound);
    }
    void PostText(bool critical,const char* format,...)override {
        char text[512];va_list args;va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);PostPlainText(critical,text);
    }
    void sharedTip(uint32_t sides,ShipID excludeA,ShipID excludeB,bool critical,const char* format,...) {
        if(!sides || networkReplaying || uiPreview || terminating)return;
        char text[512];va_list args;va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);
        if(gameRole==GameRole::Authority){Net::Notice n{};n.number=++lastNotice;n.time=elapsed;n.flags=Net::NoticeTip|Net::NoticeSharedTip|unsigned(critical);n.sender=excludeA;n.voice=excludeB;Net::text(n.text,text);networkNotices.push_back({NA,n,sides});publicationEffectsIndexed=false;++feedbackRevision;}
        else if(player && player->GetObjectID()!=excludeA && player->GetObjectID()!=excludeB){int side=player->GetSide()->GetObjectID();if(sides&(1u<<(side>=0?side:c_cSidesMax)))PostPlainText(critical,text);}
    }
    void sharedCue(uint32_t sides,SoundID id) {
        if(!sides || id<0 || networkReplaying || uiPreview || terminating)return;
        if(gameRole==GameRole::Authority){Net::Event e{};e.kind=Net::EventKind::Sound;e.value=id;e.sides=sides;recordEvent(e);++feedbackRevision;}
        else if(player && player->GetSide()->GetObjectID()>=0 && (sides&(1u<<player->GetSide()->GetObjectID())))PlaySoundEffect(id);
    }
    void shipLossFeedback(IshipIGC* victim,ImodelIGC* killer,bool died) {
        auto* launcher=killer && killer->GetObjectType()==OT_ship?static_cast<IshipIGC*>(killer):nullptr;
        // Original mission-wide death reports identify names, not hidden poses.
        // Only two principals need private text; the other tips share faction arenas.
        constexpr uint32_t everyone=(1u<<(c_cSidesMax+1))-1;
        auto post=[&](SideID side,uint32_t audience){Allegiance::ShipLossFeedback(victim,launcher,died,NA,side,[&](bool critical,const char* format,auto... args){sharedTip(audience,victim->GetObjectID(),launcher?launcher->GetObjectID():NA,critical,format,args...);});};
        auto type=victim->GetPilotType();SideID own=victim->GetSide()->GetObjectID();
        if(type==c_ptBuilder || type==c_ptMiner || type==c_ptCarrier){post(own,1u<<own);post(SIDE_TEAMLOBBY,everyone&~(1u<<own));}
        else post(own,everyone);
        if(victim->GetPilotType()>=c_ptPlayer)
            Allegiance::ShipLossFeedback(victim,launcher,died,victim->GetObjectID(),victim->GetSide()->GetObjectID(),[&](bool critical,const char* format,auto... args){PostNotificationText(victim,critical,format,args...);});
        if(launcher && launcher!=victim && launcher->GetPilotType()>=c_ptPlayer)
            Allegiance::ShipLossFeedback(victim,launcher,died,launcher->GetObjectID(),launcher->GetSide()->GetObjectID(),[&](bool critical,const char* format,auto... args){PostNotificationText(launcher,critical,format,args...);});
        if(victim->GetPilotType()<c_ptPlayer)sharedCue(1u<<victim->GetSide()->GetObjectID(),Allegiance::DroneLossSound(victim));
    }
    void playPilotSound(SoundID id,IshipIGC* pilot,bool spatial=false) {
        if(id<0 || !pilot || networkReplaying || uiPreview || terminating)return;
        if(gameRole==GameRole::Authority){Net::Event event{};event.kind=Net::EventKind::Sound;event.value=id;event.pilot=pilot->GetObjectID();event.flags=spatial?Net::SoundOwnShip:0;recordEvent(event,pilot);}
        else if(pilot==player)PlaySoundEffect(id,spatial?pilot:nullptr);
    }
    void PostNotificationText(ImodelIGC* model,bool critical,const char* format,...)override {
        if(networkReplaying || uiPreview || terminating)return;
        if(gameRole==GameRole::Authority && model && model->GetObjectType()==OT_ship){char text[512];va_list args;va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);Net::Notice n{};n.number=++lastNotice;n.time=elapsed;n.flags=Net::NoticeTip|unsigned(critical);Net::text(n.text,text);publicationEffectsIndexed=false;networkNotices.push_back({model->GetObjectID(),n});playPilotSound(critical?newCriticalMsgSound:newNonCriticalMsgSound,static_cast<IshipIGC*>(model));return;}
        if(model!=player)return;char text[512];va_list args;va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);PostPlainText(critical,text);
    }
    void crewBoardFeedback(IshipIGC* pilot,IshipIGC* parent) {
        Allegiance::CrewBoardFeedback(parent?parent->GetName():nullptr,pilot->GetTurretID()==NA,
            [&](bool critical,const char* format,auto... args){PostNotificationText(pilot,critical,format,args...);});
    }
    bool leaveDockedShip(IshipIGC* pilot) {
        if(!Allegiance::LeaveDockedShip(pilot))return false;
        crewBoardFeedback(pilot,nullptr);return true;
    }
    void promoteCrew(IshipIGC* pilot) {
        TRef<IshipIGC> parent=pilot->GetParentShip();
        pilot->Promote();
        crewBoardFeedback(pilot,nullptr);crewBoardFeedback(parent,pilot);
        Allegiance::CrewPromotionFeedback(true,[&](bool critical,const char* text){PostNotificationText(pilot,critical,"%s",text);});
        Allegiance::CrewPromotionFeedback(false,[&](bool critical,const char* text){PostNotificationText(parent,critical,"%s",text);});
    }
    void ActivateTeleportProbe(IprobeIGC* probe)override {
        if(terminating || !probe->GetCluster())return;
        notifyHumans(nullptr,[&]{if(seen(probe)){bool friendly=IsideIGC::AlliedSides(probe->GetSide(),player->GetSide());PostText(true,"%s%s%s active in %s",friendly?"":probe->GetSide()->GetName(),friendly?"":" ",GetModelName(probe),probe->GetCluster()->GetName());}});
    }
    void DestroyTeleportProbe(IprobeIGC* probe)override {
        if(terminating || !probe->GetCluster())return;
        notifyHumans(nullptr,[&]{if(seen(probe)){bool friendly=IsideIGC::AlliedSides(probe->GetSide(),player->GetSide());PostText(friendly,"%s%s%s in %s was destroyed",friendly?"":probe->GetSide()->GetName(),friendly?"":" ",GetModelName(probe),probe->GetCluster()->GetName());}});
    }
    void dropInventory(Time now,IshipIGC* ship,const Vector& hit) {
        Allegiance::DropShipInventory(ship,[](int low,int high){return randomInt(low,high);},
            [&](short count,IpartTypeIGC* part,float life){Allegiance::CreatePartTreasure(now,ship,count,part,hit,100.f,life,[](){return Vector::RandomDirection();});});
    }
    void KillShipEvent(Time now,IshipIGC* ship,ImodelIGC* credit,float amount,const Vector& hit,const Vector& impact)override {
        if(terminating || gameRole==GameRole::Replica)return;
        struct ActorRestore {Impl& game;int old;bool changed;~ActorRestore(){if(changed)game.activateActor(old);}} restore{*this,player?player->GetObjectID():NA,gameRole==GameRole::Authority && ship->GetPilotType()>=c_ptPlayer && ship!=player};
        if(restore.changed)activateActor(ship->GetObjectID());
        if(training && Training::IsTraining() && ship->GetCluster()) {
            if(ship->GetPilotType()!=c_ptLayer)ship->GetCluster()->GetClusterSite()->AddExplosion(ship,c_etSmallShip);
            if(ship==player){Training::ShipDied(credit);return;}
            // Authored derelicts deliberately use c_ptPlayer. Offline WinTrek
            // removes every non-local training ship; it does not rescue those
            // fixtures into a station as a live multiplayer pilot would be.
            Training::ShipKilled(ship,credit);
            dropInventory(now,ship,hit);killed.emplace_back(ship);return;
        }
        if(!ship->GetCluster()) {
            if(ship->GetParentShip())leaveDockedShip(ship);
            if(ship->GetPilotType()<c_ptPlayer)kill(ship);return;
        }
        if(std::any_of(killed.begin(),killed.end(),[&](auto& m){return m==ship;}))return;
        // FedSrv credits the largest active damage bucket, not the last hit.
        if(auto* track=ship->GetDamageTrack())if(auto* l=track->GetDamageBuckets()->first())
            if(l->data()->model()->GetMission()==mission)credit=l->data()->model();
        TRef<ImodelIGC> killer=credit;
        bool pod=ship->GetBaseHullType() && ship->GetBaseHullType()->HasCapability(c_habmLifepod);
        if(amount!=0)shipLossFeedback(ship,killer,ship->GetPilotType()<c_ptPlayer || pod || amount<0 || !mission->GetMissionParams()->bEjectPods);
        scoreShipDamage(ship,killer,pod,amount);
        while(auto* child=ship->GetChildShips()->first())KillShipEvent(now,child->data(),killer,amount,hit,impact);
        if(!pod && amount!=0 && killer && killer->GetSide() && killer->GetSide()!=ship->GetSide()) {
            if(killer->GetObjectType()==OT_ship) {
                auto* credited=static_cast<IshipIGC*>(static_cast<ImodelIGC*>(killer));credited->AddKill();
                if(!IsideIGC::AlliedSides(credited->GetSide(),ship->GetSide()))credited->AddExperience();
                if(credited==player)++state.kills;
            } else killer->GetSide()->AddKill();
            if(killer->GetSide()==player->GetSide())++state.friendlyKills;else ++state.enemyKills;
            if(!state.complete && mission->GetMissionStage()==STAGE_STARTED && Allegiance::CheckKillVictory(killer->GetSide())){state.complete=true;state.endgame.winner=killer->GetSide()->GetObjectID();state.result=std::string(killer->GetSide()->GetName())+" wins by team kills";}
        }
        bool passenger=ship->GetParentShip()!=nullptr;
        if(!pod && !passenger)ship->GetCluster()->GetClusterSite()->AddExplosion(ship,
            ship->GetBaseHullType()->HasCapability(c_habmFighter)?c_etSmallShip:c_etBigShip);
        dropInventory(now,ship,hit);
        SideID flag=ship->GetFlag();
        if(!Allegiance::DropShipFlag(ship,now,hit,[](){return Vector::RandomDirection();}))respawnFlag(flag);
        if(ship->GetPilotType()>=c_ptPlayer) {
            if(gameRole==GameRole::Authority){Orientation orientation;Vector position;const Vector* toward=killer && killer->GetCluster()==ship->GetCluster()?&killer->GetPosition():nullptr;auto* source=ship->GetSourceShip();Allegiance::DeathCameraFrame(source->GetPosition(),source->GetOrientation(),source->GetRadius(),toward,orientation,position);Net::Event event{};event.kind=Net::EventKind::Camera;event.pilot=ship->GetObjectID();event.flags=1;event.position={position.x,position.y,position.z};auto f=orientation.GetForward(),u=orientation.GetUp();event.normal={f.x,f.y,f.z};event.ellipse={u.x,u.y,u.z};recordEvent(event,source);}
            if(ship==player){target=nullptr;selectedWeapon=0;groupFire=true;resetCamera();engineLevelsValid=false;beginCinematic(killer,true);}
            ship->SetAutopilot(false);ship->SetStateM(0);
            if(!pod && amount>=0 && mission->GetMissionParams()->bEjectPods) {
                Allegiance::EjectIntoPod(now,ship,killer,hit,impact,[](){return Vector::RandomDirection();});
                ship->AddEjection();if(ship==player)++state.ejections;
            } else {
                ship->AddDeath();if(ship==player)++state.deaths;ship->SetExperience(0);
                if(ship==player && ship->IsGhost()) {
                    commandViewEnabled=true;commandCluster=ship->GetCluster();commandCenter=ship->GetPosition();commandDisplacement=Vector::GetZero();commandTracking=false;cameraValid=false;
                }
                // FedSrv selects recovery before removing the dying pilot
                // from flight. Loading a pod in the old sector briefly exposes
                // a new hull/sighting and can reveal the remote restart base.
                TRef<IstationIGC> restart=Allegiance::SelectRestartStation(ship);
                ship->SetCluster(nullptr);
                if(ship->GetParentShip())ship->SetParentShip(nullptr);
                ship->SetBaseHullType(ship->GetSide()->GetCivilization()->GetLifepod());
                if(!rescueShip(ship,nullptr,restart)) {
                    ship->SetCluster(nullptr);if(ship==player)state.dead=true;
                }
                if(economy() && ship->IsGhost() && !Allegiance::MissionHasPlayers(mission,ship->GetSide(),false,
                    [](auto* candidate){return candidate->GetPilotType()>=c_ptPlayer;})) {
                    deactivateSide(ship->GetSide());
                    if(auto* winner=Allegiance::AlliedSurvivor(mission)){state.complete=true;state.endgame.winner=winner->GetObjectID();state.result=std::string(winner->GetName())+" wins by elimination";}
                }
            }
        } else {
            // Drones die permanently; the practice session may order replacements.
            if(ship->GetParentShip())ship->SetParentShip(nullptr);
            killed.emplace_back(ship);
        }
    }
    void respawnFlag(SideID flag,IsideIGC* capturer=nullptr) {
        if(!Allegiance::RespawnFlag(mission,mission->GetLastUpdate(),flag,[](int a,int b){return randomInt(a,b);}) && capturer && flag!=SIDE_TEAMLOBBY) {
            state.complete=true;state.endgame.winner=capturer->GetObjectID();state.result=std::string(capturer->GetName())+" wins: the captured flag's team has no pedestal remaining";
        }
    }
    void KillTreasureEvent(ItreasureIGC* treasure)override {
        if(treasure->GetTreasureCode()==c_tcFlag && mission->GetMissionStage()==STAGE_STARTED)respawnFlag(treasure->GetTreasureID());
        treasure->Terminate();
    }
    void KillMissileEvent(ImissileIGC* missile,const Vector& position)override {
        missile->Explode(position);missile->Terminate();
    }
    void KillMineEvent(ImineIGC* mine)override{kill(mine);}
    void KillProbeEvent(IprobeIGC* probe)override{kill(probe);}
    void vacateStation(IstationIGC* station) {
        // ChangeStation owns real launch/rearm and crew presentation. Exhausted
        // observers move to an alternate restart instead of entering flight.
        Allegiance::VacateStation(station,[](IshipIGC*){});
    }
    void deactivateSide(IsideIGC* side) {
        if(!side->GetActiveF())return; // forced deaths can reenter elimination
        side->SetActiveF(false);side->SetTimeEndured(float(elapsed));
        Allegiance::EliminateSideShips(side,
            [&](IshipIGC* ship,float amount) {
                KillShipEvent(mission->GetLastUpdate(),ship,nullptr,amount,ship->GetPosition(),Vector::GetZero());
            },
            [&](IshipIGC* ship) {
                ship->SetAutopilot(false);ship->SetStateM(0);
                moneyRequests.erase(ship->GetObjectID());pendingOrders.erase(ship->GetObjectID());
                notifyShipHumans(ship,[&]{target=nullptr;state.dead=true;resetCamera();engineLevelsValid=false;});
            });
        economyDirty=true;SendChatf(nullptr,CHAT_EVERYONE,NA,NA,"%s is no more.",side->GetName());
    }
    void checkConquest(IstationIGC* stationKilled=nullptr,IsideIGC* previous=nullptr) {
        if(!economy() || state.complete)return;
        auto victory=Allegiance::CheckStationVictory(mission,stationKilled,previous);
        if(victory.eliminated)deactivateSide(victory.eliminated);
        auto* winner=victory.winner;
        if(victory.alliances || victory.eliminated)if(auto* survivor=Allegiance::AlliedSurvivor(mission))winner=survivor;
        bool active=false;for(auto* l=mission->GetSides()->first();l;l=l->next())active|=l->data()->GetActiveF();
        if(winner || !active) {
            state.complete=true;state.endgame.winner=winner?winner->GetObjectID():NA;state.result=winner?std::string(winner->GetName())+" wins":"All sides were destroyed";
        }
    }
    void KillStationEvent(IstationIGC* station,ImodelIGC* killer,float)override {
        if(terminating || station->GetMission()!=mission || !station->GetCluster())return;
        TRef<IstationIGC> retained=station;
        station->GetCluster()->GetClusterSite()->AddExplosion(station,c_etLargeStation);
        uint32_t audience=0,friendly=0;
        for(auto* l=mission->GetSides()->first();l;l=l->next())if(gameRole!=GameRole::Authority || station->SeenBySide(l->data())){
            uint32_t bit=1u<<l->data()->GetObjectID();audience|=bit;if(IsideIGC::AlliedSides(station->GetSide(),l->data()))friendly|=bit;
        }
        Allegiance::StationLossFeedback(station,killer && killer->GetObjectType()==OT_ship?static_cast<IshipIGC*>(killer):nullptr,
            [&](bool critical,const char* format,auto... args){sharedTip(audience,NA,NA,critical,format,args...);});
        sharedCue(friendly,Allegiance::StationDestroyedSound(station->GetStationType(),true));
        sharedCue(audience&~friendly,Allegiance::StationDestroyedSound(station->GetStationType(),false));
        if(training && Training::IsTraining())Training::KillStationEvent(station,killer);
        if(killer && killer->GetSide())killer->GetSide()->AddBaseKill();
        if(killer && killer->GetObjectType()==OT_ship)scoreBase(static_cast<IshipIGC*>(killer),false);
        for(auto* l=mission->GetSides()->first();l;l=l->next())forgetRock(station->GetRoidID(),station->GetCluster()->GetObjectID(),l->data());
        vacateStation(station);deathExplosion(station);checkConquest(station,station->GetSide());
        // Both original offline and server callbacks terminate stations before
        // returning. Later deaths in this update must not count a destroyed
        // base, relocate observers to it, or retain its scanner/collision body.
        station->Terminate();
        for(auto* l=mission->GetSides()->first();l;l=l->next())l->data()->UpdateTerritory();
        checkConquest();
    }
    void TerminateMissionEvent(ImissionIGC* core)override {
        // CmissionIGC detaches ships instead of terminating them. Release site-
        // owned trackers before it destroys its DamageTrackSet.
        for(auto* l=core->GetShips()->first();l;l=l->next())l->data()->DeleteDamageTrack();
    }
    void TerminateModelEvent(ImodelIGC* model)override {
        if(uiPreview){if(model->GetObjectType()==OT_ship)static_cast<IshipIGC*>(model)->DeleteDamageTrack();return;}
        networkIdentities.erase(model);
        if(model->GetObjectType()==OT_projectile || model->GetObjectType()==OT_missile){
            auto id=model->GetObjectType()==OT_projectile?static_cast<CprojectileIGC*>(model)->NativeShot():static_cast<CmissileIGC*>(model)->NativeShot();
            if(auto shot=shots.find(id);shot!=shots.end() && shot->second==model){shots.erase(shot);confirmedShots.erase(id);}
        }

        // Like CFSShip, the site owns the optional damage tracker. IGC does not
        // delete it with the ship; release it even during mission teardown.
        if(model->GetObjectType()==OT_ship){
            disconnectScore(static_cast<IshipIGC*>(model));
            static_cast<IshipIGC*>(model)->DeleteDamageTrack();
            shipPaneStatuses.erase(model->GetObjectID());
            for(auto& intel:shipIntel)intel.erase(ShipID(model->GetObjectID()));
            if(gameRole!=GameRole::Replica || !replicaRetiring || !replicaRetiring->contains(model))playerPaneMemory.erase(model->GetObjectID());
        }
        if(terminating || model->GetObjectType()==OT_projectile)return;
        if(gameRole!=GameRole::Replica && player)Allegiance::TargetKilled(player,model,[&](ImodelIGC* next,CommandID command){setTarget(next,false,command);},[&]{player->SetAutopilot(true);});
        if(target==model){target=nullptr;targetRock=targetRockSector=-1;}
        for(auto& [_,actor]:actors)if(actor.target==model)actor.target=nullptr;
        if(model->GetObjectType()==OT_ship)actors.erase(model->GetObjectID());
        if(static_cast<ImodelIGC*>(lastCameraMissile)==model)lastCameraMissile=nullptr;
        if(gameRole!=GameRole::Replica && model->GetObjectType()==OT_ship){
            retireAccount(static_cast<IshipIGC*>(model),0); // IGC already removed parts
            loadouts.erase(model->GetObjectID());editorLoadouts.erase(model->GetObjectID());lastOwnStations.erase(model->GetObjectID());
            accounts.erase(model->GetObjectID());droneTypes.erase(model->GetObjectID());moneyRequests.erase(model->GetObjectID());
            for(auto& id:commanders)if(id==model->GetObjectID())id=NA;
            economyDirty=true;
            int id=model->GetObjectID();if(id>=0){commandSelected.reset(id);std::erase(commandUnits,id);}
            commandUnit=commandUnits.empty()?-1:commandUnits.front();
        }
        // Snapshot retirement already cleared these relationships in one
        // fleet-wide pass. Cascaded removals still take the individual path.
        if(gameRole==GameRole::Replica && replicaRetiring && replicaRetiring->contains(model))return;
        for(auto* l=mission->GetShips()->first();l;l=l->next()) {
            auto* ship=l->data();
            // A destroyed construction site terminates its shell and queues
            // the builder's death. Native deaths settle after the IGC update,
            // so end construction before clearing the command target that
            // CshipIGC::Terminate otherwise expects to remain an asteroid.
            if(model->GetObjectType()==OT_asteroid && ship->GetPilotType()==c_ptBuilder &&
                ship->GetCommandTarget(c_cmdPlan)==model)
                ship->SetStateBits(drillingMaskIGC|buildingMaskIGC,0);
            for(Command c=0;c<c_cmdMax;++c)if(ship->GetCommandTarget(c)==model)ship->SetCommand(c,nullptr,c_cidNone);
            if(ship->GetRipcordModel()==model)ship->SetRipcordModel(nullptr);
        }
    }
    void PlayFFEffect(ForceEffectID effectID,ImodelIGC* model=nullptr,LONG direction=0)override {
        if(networkReplaying)return;
        if(gameRole==GameRole::Authority){forShipHumans(model,true,[&](int pilot){Net::Event event{};event.kind=Net::EventKind::Feedback;event.value=effectID;event.flags=uint32_t(direction);event.pilot=pilot;recordEvent(event,model);});return;}
        if(!terminating && player && (!model || model==player))feedback.add(effectID,direction);
    }
    void DamageShipEvent(Time now,IshipIGC* ship,ImodelIGC* launcher,DamageTypeID type,float amount,float leakage,const Vector& p1,const Vector& p2)override {
        if(player && launcher && (ship==player->GetSourceShip() || gameRole==GameRole::Authority) && amount>0) {
            Vector delta=p2-p1;auto& orientation=ship->GetOrientation();
            PlayFFEffect(effectBounce,gameRole==GameRole::Authority?ship:nullptr,LONG(DegreesFromRadians(atan2(orientation.GetBackward()*delta,orientation.GetRight()*delta)))+90);
        }
        auto* thing=static_cast<Thing*>(ship->GetThingSite());
        if(launcher && thing && thing->hitSoundTime!=elapsed && (gameRole==GameRole::Authority || ship->GetCluster()==viewCluster)) {
            thing->hitSoundTime=elapsed;
            if(gameRole==GameRole::Authority){Net::Event event{};event.kind=Net::EventKind::Sound;event.flags=1;event.value=Allegiance::ShipImpactSound(false,leakage==0);event.position={p2.x,p2.y,p2.z};auto v=ship->GetVelocity();event.velocity={v.x,v.y,v.z};recordEvent(event,ship);}
            else PlaySoundEffect(Allegiance::ShipImpactSound(ship==player->GetSourceShip(),leakage==0),ship,ship->GetOrientation().TimesInverse(p2-p1));
        }
        if(amount<1 || !launcher || !launcher->GetSide() || (type&c_dmgidNoWarn) || !thing || elapsed-thing->damageReportTime<=10)return;
        auto* side=ship->GetSide();auto* attacker=launcher->GetSide();
        if(ship->GetPilotType()<c_ptPlayer) {
            if(side!=attacker)SendChatf(ship,CHAT_TEAM,side->GetObjectID(),Allegiance::DroneUnderAttackSound(ship),"Under attack by %s in %s",GetModelName(launcher),ship->GetCluster()->GetName());
            if(IsideIGC::AlliedSides(side,attacker) && launcher->GetObjectType()==OT_ship)
                SendChat(ship,CHAT_INDIVIDUAL,launcher->GetObjectID(),droneWatchFireSound,"Watch your fire");
            thing->damageReportTime=elapsed;
        } else if(side!=attacker && IsideIGC::AlliedSides(side,attacker)) {
            SendChat(ship,CHAT_INDIVIDUAL,launcher->GetObjectID(),droneWatchFireSound,"Watch your fire");thing->damageReportTime=elapsed;
        }
    }
    void DamageStationEvent(IstationIGC* station,ImodelIGC* launcher,DamageTypeID type,float amount)override {
        if(gameRole==GameRole::Authority || station->GetCluster()==viewCluster)PlaySoundEffect(station->GetShieldFraction()<0?otherHullHitSound:otherShieldHitSound,station);
        if(amount<1 || !launcher || !launcher->GetSide() || (type&c_dmgidNoWarn) || mission->GetLastUpdate()-station->GetLastDamageReport()<=10)return;
        auto* side=station->GetSide();auto* attacker=launcher->GetSide();
        if(side!=attacker) {
            bool critical=station->GetFraction()<.3f;auto* stationType=station->GetStationType();
            SendChatf(nullptr,side->GetAllies()==NA?CHAT_TEAM:CHAT_ALLIES,side->GetObjectID(),critical?stationType->GetCriticalSound():stationType->GetUnderAttackSound(),
                "%s in %s %sunder attack by %s",station->GetName(),station->GetCluster()->GetName(),critical?"critically damaged and ":"",GetModelName(launcher));
        }
        if(IsideIGC::AlliedSides(side,attacker) && launcher->GetObjectType()==OT_ship)SendChat(nullptr,CHAT_INDIVIDUAL,launcher->GetObjectID(),droneWatchFireSound,"Watch your fire");
        station->SetLastDamageReport(mission->GetLastUpdate());
    }
    bool HandlePickDefaultOrder(IshipIGC* ship)override{return training && Training::IsTraining() && Training::HandlePickDefaultOrder(ship);}
    bool DockWithStationEvent(IshipIGC* ship,IstationIGC* station)override {
        if(training && ship==player && Training::IsTraining()) {
            station->RepairAndRefuel(ship);ship->SetStateBits(keyMaskIGC,0);
            if(!Training::ShipLanded())return true;
        }
        if(ship->GetParentShip())return false;
        bool wasOwn=ship->GetSide()==station->GetSide();
        if(Allegiance::ShouldBoardStation(ship,station) && !mission->GetMissionParams()->bInvulnerableStations)CaptureStationEvent(ship,station);
        else if(!IsideIGC::AlliedSides(ship->GetSide(),station->GetSide()))return false;
        bool pod=ship->GetBaseHullType()->HasCapability(c_habmLifepod);
        if(pod && station->GetSide()!=ship->GetSide()) {
            auto found=lastOwnStations.find(ship->GetObjectID());
            auto* own=Allegiance::SelectRestartStation(ship,found!=lastOwnStations.end()?found->second:NA,false);
            return own?DockWithStationEvent(ship,own):false;
        }
        if(wasOwn && ship->GetFlag()!=NA) {
            SideID flag=ship->GetFlag();if(auto* score=scoring(ship)){if(flag==SIDE_TEAMLOBBY)score->AddArtifact();else score->AddFlag();}bool won=Allegiance::SecureFlag(ship);
            if(flag==SIDE_TEAMLOBBY)SendChatf(nullptr,CHAT_TEAM,ship->GetSide()->GetObjectID(),artifactSecuredSound,"%s has secured an artifact.",ship->GetName());
            else {
                SendChatf(nullptr,CHAT_TEAM,ship->GetSide()->GetObjectID(),enemyFlagSecuredSound,"%s has secured %s's flag.",ship->GetName(),mission->GetSide(flag)->GetName());
                SendChatf(nullptr,CHAT_TEAM,flag,flagSecuredSound,"%s of %s has secured your flag.",ship->GetName(),ship->GetSide()->GetName());
            }
            if(won){state.complete=true;state.endgame.winner=ship->GetSide()->GetObjectID();state.result=std::string(ship->GetSide()->GetName())+" wins by recovered objectives";}
            respawnFlag(flag,ship->GetSide());
        }
        ship->SetStateBits(keyMaskIGC,0);ship->SetStation(station);
        for(auto* l=ship->GetChildShips()->first();l;l=l->next())l->data()->SetStation(station);
        if(pod && !ship->IsGhost()) {
            while(auto* p=ship->GetParts()->first())p->data()->Terminate();
        }
        if(ship->GetPilotType()<c_ptPlayer && economy())Allegiance::UpgradeDrone(ship,station);
        if(ship->GetPilotType()>=c_ptPlayer && station->GetSide()!=ship->GetSide() && !ship->IsGhost())restoreLoadout(ship,true);
        station->RepairAndRefuel(ship);
        for(auto* l=ship->GetParts()->first();l;l=l->next())warmPart(l->data());
        ship->SetGettingAmmo(false);
        if(ship==player || (gameRole==GameRole::Authority && accounts.contains(ship->GetObjectID()))) {
            ship->SetStateM(0);ship->SetAutopilot(false);ControlData controls{};controls.jsValues[c_axisThrottle]=-1;ship->SetControls(controls);
            notifyShipHumans(ship,[&]{state.dead=false;});
        }
        if(ship->GetPilotType()>=c_ptPlayer && station->GetSide()!=ship->GetSide()) {
            ship->SetStation(nullptr);ship->SetCommand(c_cmdPlan,nullptr,c_cidNone);
        }
        return true;
    }
    bool rescueShip(IshipIGC* ship,IshipIGC* rescuer,IstationIGC* station) {
        if(rescuer){
            if(auto* score=scoring(rescuer))score->AddRescue();
            if(rescuer->GetPilotType()>=c_ptPlayer){
                Allegiance::RescuePilotFeedback(true,ship->GetName(),[&](bool critical,const char* format,auto... args){PostNotificationText(rescuer,critical,format,args...);});
                playPilotSound(rescuePlayerSound,rescuer,true);
            }
            Allegiance::RescuePilotFeedback(false,rescuer->GetName(),[&](bool critical,const char* format,auto... args){PostNotificationText(ship,critical,format,args...);});
            playPilotSound(jumpSound,ship);
        }
        return station?DockWithStationEvent(ship,station):false;
    }
    bool RescueShipEvent(IshipIGC* ship,IshipIGC* rescuer)override {
        return rescueShip(ship,rescuer,Allegiance::SelectRestartStation(ship));
    }
    bool ContinueRipcord(IshipIGC* ship,ImodelIGC* destination)override {
        bool allowed=Allegiance::CanContinueRipcord(ship,destination);
        if(!allowed){ship->SetStateBits(droneRipMaskIGC,0);if(ship->GetRipcordModel())PlayNotificationSound(salRipcordAbortedSound,ship);}return allowed;
    }
    void RequestRipcord(IshipIGC* ship,IclusterIGC* cluster)override {
        auto* old=ship->GetRipcordModel();
        if(!cluster){ship->SetRipcordModel(nullptr);ship->SetStateBits(droneRipMaskIGC,0);if(old)PlayNotificationSound(salRipcordAbortedSound,ship);return;}
        if(!ship->GetCluster() || !ship->GetBaseHullType() || ship->GetFlag()!=NA)return;
        if(old && old->GetCluster()==cluster)return;
        if(auto* model=ship->FindRipcordModel(cluster)){
            if(model==old)return;
            ship->SetRipcordModel(model);ship->ResetRipcordTimeLeft();
            if(ship->GetPilotType()>=c_ptPlayer){
                auto* destination=model->GetCluster();
                if(!destination && model->GetObjectType()==OT_ship){auto* source=static_cast<IshipIGC*>(model)->GetSourceShip();if(source->GetStation())destination=source->GetStation()->GetCluster();}
                if(destination){
                    auto post=[&](IshipIGC* pilot){Allegiance::RipcordActivationFeedback(destination->GetName(),cluster->GetName(),destination==cluster,[&](bool critical,const char* format,auto... args){PostNotificationText(pilot,critical,format,args...);});};
                    if(old)post(ship);
                    else if(gameRole==GameRole::Authority)notifyShipHumans(ship,[&]{post(player);});
                    else if(Allegiance::ReceivesShipNotification(player,ship))post(player);
                }
            }
        }else if(ship->GetPilotType()>=c_ptPlayer){
            playPilotSound(salNoRipcordSound,ship,true);
        }
    }
    bool UseRipcord(IshipIGC* ship,ImodelIGC* destination)override {
        if(gameRole==GameRole::Replica)return false;
        if(!ContinueRipcord(ship,destination))return true;
        if(destination->GetObjectType()==OT_ship) {
            auto* carrier=static_cast<IshipIGC*>(destination);float energy=carrier->GetEnergy()-ship->GetBaseHullType()->GetRipcordCost();
            if(energy<0){ship->SetStateBits(droneRipMaskIGC,0);PlayNotificationSound(salRipcordAbortedSound,ship);return true;}carrier->SetEnergy(energy);
        }
        Allegiance::ApplyRipcordArrival(ship,destination,mission->GetFloatConstant(c_fcidExitStationSpeed),[](){return Vector::RandomDirection();});
        if(gameRole==GameRole::Authority)PlayNotificationSound(jumpSound,ship);
        notifyShipHumans(ship,[&]{resetCamera();if(gameRole!=GameRole::Authority)PlaySoundEffect(jumpSound,ship);});return true;
    }
    bool LandOnCarrierEvent(IshipIGC* carrier,IshipIGC* ship,float collision)override {
        // IGC has already checked the garage and carrier/landing capabilities.
        restoreLoadout(ship);replenish(ship);
        Allegiance::LaunchFromCarrier(carrier,ship,mission->GetFloatConstant(c_fcidExitStationSpeed),[](float a,float b){return random(a,b);});
        ship->GetCluster()->RecalculateCollisions(collision,ship,nullptr);
        Allegiance::ResumeAfterCarrier(carrier,ship);ship->SetGettingAmmo(false);
        recordShipCinematic(ship,carrier,false);
        if(gameRole==GameRole::Authority)PlayNotificationSound(salRepairedAtCarrierSound,ship);
        notifyShipHumans(ship,[&]{if(gameRole!=GameRole::Authority){PlayNotificationSound(salRepairedAtCarrierSound,ship);beginCinematic(carrier,false);}});return true;
    }
    void HitWarpEvent(IshipIGC* ship,IwarpIGC* warp)override {
        auto* destination=warp->GetDestination();if(!destination)return;
        // A sighted traversal through a known aleph updates last-known sector,
        // without granting scanner sight on the far side (FedSrv ShipStatusWarped).
        for(auto* l=mission->GetSides()->first();l;l=l->next())if(ship->SeenBySide(l->data()) && warp->SeenBySide(l->data())) {
            auto& records=shipIntel[l->data()->GetObjectID()];
            auto relocate=[&](IshipIGC* pilot){auto i=records.find(pilot->GetObjectID());if(i!=records.end())i->second.status.sector=destination->GetCluster()->GetObjectID();};
            relocate(ship);for(auto* child=ship->GetChildShips()->first();child;child=child->next())relocate(child->data());
        }
        ship->SetCluster(destination->GetCluster());
        Allegiance::ApplyOfflineWarpExit(ship,warp,destination,mission->GetFloatConstant(c_fcidExitWarpSpeed),
            [](float low,float high){return random(low,high);});
        ship->GetThingSite()->SetTrailColor(ship->GetSide()->GetColor());
        ship->SetBB(ship->GetLastUpdate(),ship->GetLastUpdate(),0);
        if(gameRole==GameRole::Authority)PlayNotificationSound(jumpSound,ship);
        notifyShipHumans(ship,[&]{if(gameRole!=GameRole::Authority)PlaySoundEffect(jumpSound,ship);resetCamera();});
    }
    bool HitTreasureEvent(Time,IshipIGC* ship,ItreasureIGC* treasure)override {
        auto code=treasure->GetTreasureCode();auto id=treasure->GetTreasureID();
        if(code==c_tcCash) {
            if(economy() && accounts.contains(ship->GetObjectID()))balance(ship->GetAutoDonate()?ship->GetAutoDonate():ship)+=treasure->GetAmount();
            else {funds[ship->GetSide()->GetObjectID()]+=treasure->GetAmount();if(economy())distributeIncome(ship->GetSide());}
            if(ship->GetPilotType()>=c_ptPlayer) {
                playPilotSound(pickUpCashSound,ship);auto* recipient=ship->GetAutoDonate();
                Allegiance::CashRewardFeedback(treasure->GetAmount(),recipient?recipient->GetName():nullptr,
                    [&](bool critical,const char* format,auto... args){PostNotificationText(ship,critical,format,args...);});
            }
            economyDirty=true;return true;
        }
        if(code==c_tcFlag && (ship->GetFlag()!=NA || id==ship->GetSide()->GetObjectID()))return false;
        Mount mount=ship->HitTreasure(code,id,treasure->GetAmount());
        if(code==c_tcDevelopment)if(auto* score=scoring(ship))score->AddTechsRecovered();
        if(code==c_tcPart && mount==c_mountNA)return false; // Keep an uncollected part in space.
        if(code==c_tcFlag) {
            treasure->SetTreasureID(NA);
            auto announce=[&](IsideIGC* viewer){uint32_t audience=1u<<viewer->GetObjectID();Allegiance::FlagPickupFeedback(ship,id,viewer,
                [&](bool critical,const char* format,auto... args){sharedTip(audience,NA,NA,critical,format,args...);},[&](SoundID sound){sharedCue(audience,sound);});};
            announce(ship->GetSide());if(id>0 && id!=ship->GetSide()->GetObjectID())announce(mission->GetSide(id));
        }
        return true;
    }
    void FireMissile(IshipIGC* ship,ImagazineIGC* magazine,Time fired,ImodelIGC* aim,float lock)override {
        if(!ship->GetCluster())return;
        if(aim && (aim->GetCluster()!=ship->GetCluster() || !ship->CanSee(aim)))aim=nullptr;
        int count=std::min(int(magazine->GetLaunchCount()),int(magazine->GetAmount()));
        if(count<=0)return;
        uint64_t firstShot=gameRole==GameRole::Offline?0:NativeShot(ship,4,static_cast<CmagazineIGC*>(magazine)->NativeShots()+1);uint32_t shotSeed=uint32_t(firstShot)^uint32_t(firstShot>>32);std::optional<Allegiance::SimulationRandomScope> shotRandom;if(gameRole!=GameRole::Offline)shotRandom.emplace(shotSeed);
        missileLaunches.resize(count);
        Allegiance::ComputeMissileLaunches(ship,magazine,fired,count,missileLaunches.data(),
            [](float low,float high){return random(low,high);});
        DataMissileIGC data{};data.pmissiletype=magazine->GetMissileType();data.pLauncher=ship;
        data.pTarget=aim;data.pCluster=ship->GetCluster();data.lock=lock;data.bDud=false;
        PlaySoundEffect(data.pmissiletype->GetLaunchSound(),ship);if(ship==player)PlayFFEffect(effectFire);
        for(const auto& launch:missileLaunches) {
            uint64_t nativeShot=NativePredictionMode()?NativeShot(ship,4,static_cast<CmagazineIGC*>(magazine)->NativeNextShot()):0;
            data.forward=launch.vecForward;data.position=launch.vecPosition;data.velocity=launch.vecVelocity;
            data.missileID=mission->GenerateNewMissileID();if(data.missileID==NA)continue;
            if(!networkReplaying && !NativeShotExists(nativeShot)){auto* missile=mission->CreateObject(fired,OT_missile,&data,sizeof(data));if(missile){if(nativeShot)NativeProjectileCreated(static_cast<ImodelIGC*>(missile),nativeShot);missile->Release();}}
        }
        int remaining=magazine->GetAmount()-count;magazine->SetAmount(remaining);
        if(!remaining)launcherDepleted(ship,magazine,ET_Magazine);
    }
    void FireExpendable(IshipIGC* ship,IdispenserIGC* dispenser,Time fired)override {
        if(gameRole==GameRole::Replica)return;
        PlayFFEffect(effectFire,ship);
        auto* type=dispenser->GetExpendableType();auto kind=type->GetObjectType();auto equipment=type->GetEquipmentType();
        if(!Allegiance::CreateOfflineExpendable(ship,dispenser,fired,[](float low,float high){return random(low,high);}))return;
        if(gameRole==GameRole::Authority)PlayNotificationSound(kind==OT_chaffType?deployChaffSound:kind==OT_mineType?deployMineSound:deployProbeSound,ship);
        notifyShipHumans(ship,[&]{if(gameRole!=GameRole::Authority)PlaySoundEffect(kind==OT_chaffType?deployChaffSound:kind==OT_mineType?deployMineSound:deployProbeSound,ship);});
        int remaining=dispenser->GetAmount()-1;dispenser->SetAmount(remaining);
        if(!remaining)launcherDepleted(ship,dispenser,equipment);
    }
    void launcherDepleted(IshipIGC* ship,IlauncherIGC* launcher,EquipmentType type) {
        bool reloaded=Reload(ship,launcher,type);
        if(gameRole==GameRole::Authority) {
            struct Notifications {
                Impl& game;IshipIGC* ship;
                void PlayNotificationSound(SoundID id,ImodelIGC* model){game.PlayNotificationSound(id,model);}
                void PlaySoundEffect(SoundID id,ImodelIGC* model){game.PlaySoundEffect(id,model);}
                void PostText(bool critical,const char* text){game.notifyShipHumans(ship,[&]{game.PostPlainText(critical,text);});}
            } notifications{*this,ship};
            Allegiance::NotifyLauncherReload(notifications,ship,type,reloaded);
        } else if(ship==player)Allegiance::NotifyLauncherReload(*this,ship,type,reloaded);
    }
    bool Reload(IshipIGC* ship,IlauncherIGC* launcher,EquipmentType type)override {
        Allegiance::ReloadChange changes[Allegiance::MaxReloadChanges];
        int count=Allegiance::ReloadInventory(ship,launcher,type,changes);
        if(launcher && !launcher->GetAmount())launcher->Terminate();
        if(count==0 && type==ET_Weapon && Allegiance::RequestAmmoResupply(ship))
            SendChat(ship,CHAT_TEAM,ship->GetSide()->GetObjectID(),NA,"Out of ammo, I'm getting resupplies.");
        return count>0;
    }
    void replenish(IshipIGC* ship) {
        auto* h=ship->GetHullType();ship->SetFraction(1);ship->SetAmmo(h->GetMaxAmmo());ship->SetFuel(h->GetMaxFuel());ship->SetEnergy(h->GetMaxEnergy());
        if(auto* shield=static_cast<IshieldIGC*>(ship->GetMountedPart(ET_Shield,0)))shield->SetFraction(1);
    }
    template<class Type> void warmVisual(Type* type) {
        if(!type)return;auto* model=type->GetModelName();auto* texture=type->GetModelTexture();
        if(std::isalpha(static_cast<unsigned char>(*model)))loader(model,texture);
        else if(fx && *texture)fx->load(std::string(texture)+"bmp");
    }
    void warmPart(IpartIGC* part) {
        if(gameRole==GameRole::Authority) {
            // The part's dropped model differs from its projectile/missile.
            // Warm both caches when fitting it, before a death needs them.
            auto* name=part->GetPartType()->GetModelName();
            if(name && *name){loader(name,"");HitTest::Load(name);}
        }
        auto kind=part->GetEquipmentType();
        if(kind==ET_Weapon)warmVisual(static_cast<IweaponIGC*>(part)->GetProjectileType());
        else if(kind==ET_Magazine) {
            auto* magazine=static_cast<ImagazineIGC*>(part);warmVisual(magazine->GetMissileType());
            missileLaunches.reserve(std::max(missileLaunches.capacity(),size_t(magazine->GetLaunchCount())));
        }
        else if(kind==ET_ChaffLauncher || kind==ET_Dispenser) {
            auto* type=static_cast<IdispenserIGC*>(part)->GetExpendableType();warmVisual(type);
            if(type->GetObjectType()==OT_probeType)warmVisual(static_cast<IprobeTypeIGC*>(type)->GetProjectileType());
        }
    }
    void equip(IshipIGC* ship) {
        auto* hull=ship->GetHullType();
        if(economy() && ship->GetPilotType()>=c_ptPlayer) {
            auto* station=ship->GetStation();if(!station && ship->GetSide()->GetStations()->first())station=ship->GetSide()->GetStations()->first()->data();
            Money budget=0;if(station){Allegiance::BuyPreferredParts(ship,station,&budget);Allegiance::BuyDefaultCargo(ship,station,&budget,true);}
            for(auto* l=ship->GetParts()->first();l;l=l->next()){l->data()->SetMountedFraction(1);l->data()->Arm();warmPart(l->data());}
            replenish(ship);return;
        }
        for(auto* l=hull->GetPreferredPartTypes()->first();l;l=l->next()) {
            auto* type=l->data();auto eq=type->GetEquipmentType();Mount count=eq==ET_Weapon ? hull->GetMaxWeapons() : 1;
            for(Mount mount=0;mount<count;++mount)if(!ship->GetMountedPart(eq,mount) && hull->CanMount(type,mount)) {
                auto* part=ship->CreateAndAddPart(type,mount,0x7fff);if(part){part->SetMountedFraction(1);part->Arm();
                    warmPart(part);
                }
            }
        }
        replenish(ship);
    }
    IshipIGC* ship(const char* name,SideID side,HullID hull,bool bot) {
        DataShipIGC data{};data.shipID=mission->GenerateNewShipID();if(data.shipID<0)throw std::runtime_error("The original ship identity capacity is exhausted.");data.hullID=hull;data.sideID=side;data.pilotType=bot?c_ptWingman:c_ptPlayer;data.baseObjectID=NA;data.partsOffset=sizeof(data);
        snprintf(data.name,sizeof(data.name),"%s",name);
        auto* result=static_cast<IshipIGC*>(mission->CreateObject(mission->GetLastUpdate(),OT_ship,&data,sizeof(data)));
        if(!result)throw std::runtime_error("Cannot create ship");result->CreateDamageTrack();
        if(economy() && !bot && side>=0) {
            accounts.emplace(result->GetObjectID(),0);
            auto& commander=commanders[side];
            if(commander==NA){commander=result->GetObjectID();result->SetWingID(0);}
            else setDonation(result,mission->GetShip(commander));
        }
        if(side>=0){equip(result);rememberLoadout(result);}result->SetAutopilot(bot);if(!bot)connectScore(result);return result;
    }
    IstationIGC* homeStation()const {
        IstationIGC* best=nullptr;float nearest=FLT_MAX;
        for(auto* l=mission->GetStations()->first();l;l=l->next()) {
            auto* s=l->data();if(s->GetSide()!=(player?player->GetSide():mission->GetSide(economy()?options.team:0)) || !s->GetStationType()->HasCapability(c_sabmLand))continue;
            float distance=player?(s->GetPosition()-player->GetSourceShip()->GetPosition()).LengthSquared():0;
            if(player && s->GetCluster()!=player->GetCluster())distance+=1e12f;
            if(distance<nearest){nearest=distance;best=s;}
        }
        return best;
    }
    bool returnToBase() {
        if(state.complete)return false;
        if(!player || state.dead || player->GetParentShip())return false;if(player->GetStation())return true;
        auto* station=homeStation();if(!station){message("No friendly landing station is available.",true);return false;}
        setTarget(station);player->SetAutopilot(true);autopilotInputInitialized=false;message(std::string("Returning to ")+station->GetName()+". Manual steering cancels.");refresh();return true;
    }
    void respawn() {
        if(player && (economy() || player->IsGhost()))return; // Practice reset must never mint a paid hull.
        resetCamera();cameraMotion.SetJiggle(0);engineLevelsValid=false;previousActionButtons=0;autopilotInputInitialized=false;
        target=nullptr;
        if(player && player->GetMission()) {
            player->SetCluster(nullptr);player->SetStation(nullptr);
            if(player->GetParentShip())player->SetParentShip(nullptr);
            while(auto* l=player->GetParts()->first())l->data()->Terminate();
            player->SetBaseHullType(mission->GetHullType(selectedHull));equip(player);rememberLoadout(player);
            for(Command c=0;c<c_cmdMax;++c)player->SetCommand(c,nullptr,c_cidNone);
            player->SetVelocity(Vector::GetZero());player->SetAutopilot(false);
        } else {player=ship(options.pilot.c_str(),SideID(economy()?options.team:0),selectedHull,false);player->Release();}
        selectedWeapon=0;groupFire=true;state.docked=false;
        IstationIGC* base=homeStation();
        auto* cluster=base?base->GetCluster():mission->GetClusters()->first()->data();
        Vector pos=base?base->GetPosition()+Vector(0,0,base->GetRadius()+400):Vector(0,0,700);
        player->SetPosition(pos);player->SetOrientation(Orientation(Vector(0,0,1)));
        for(int axis=c_axisYaw;axis<=c_axisRoll;++axis)player->SetCurrentTurnRate(axis,0);player->SetCluster(cluster);state.dead=false;
        for(auto* l=mission->GetShips()->first();l;l=l->next())if(l->data()->GetPilotType()<c_ptPlayer && l->data()->GetSide()!=player->GetSide() && l->data()->CanSee(player)) {
            l->data()->SetCommand(c_cmdAccepted,player,c_cidAttack);
            l->data()->SetCommand(c_cmdCurrent,player,c_cidAttack);
        }
        message(std::string("Launched ")+player->GetHullType()->GetName()+".");nextTarget();
    }
    void spawnBots() {
        if(economy())return;
        if(!player)return;
        auto* cluster=player->GetCluster();
        if(!cluster && options.observerBattleDepth>0 && player->GetStation())cluster=player->GetStation()->GetCluster();
        if(!cluster)return;
        for(int side=0;side<2;++side) {
            int count=0;for(auto* l=mission->GetShips()->first();l;l=l->next())if(l->data()->GetPilotType()<c_ptPlayer && l->data()->GetSide()->GetObjectID()==side)++count;
            int total=side?options.enemies:options.allies;
            for(int i=count;i<total;++i) {
                char name[25];snprintf(name,sizeof(name),side?"Drone %d":"Wingman %d",i+1);
                HullID hull=options.variedWingmen?battleHullIds[nextBotHull[side]++%battleHullIds.size()]:412;
                auto* bot=ship(name,SideID(side),hull,true);bool training=options.scenario==Scenario::WeaponsTraining;
                float skill=std::array<float,3>{.35f,.65f,.95f}[options.difficulty];bot->SetSkills(skill,skill,skill);
                bot->SetAutopilot(!training);
                Vector offset;
                if(options.observerBattleDepth>0) {
                    int width=int(std::ceil(std::sqrt(float(total))));
                    offset=Vector((float(i%width)-float(width-1)*.5f)*battleSpawnSpacing,
                        (float(i/width)-float(width-1)*.5f)*battleSpawnSpacing,side?options.observerBattleDepth:-options.observerBattleDepth);
                }
                else if(total<=8)offset=player->GetOrientation().GetForward()*float(side?700+i*150:-150)+Vector(float(i-1)*120,40,0);
                else {
                    // A bounded formation replaces the old indefinitely long
                    // line, which put large fleets outside the universe.
                    int width=int(std::ceil(std::sqrt(float(total))));
                    offset=player->GetOrientation().GetForward()*float(side?1400:-700)+
                        player->GetOrientation().GetRight()*((float(i%width)-float(width-1)*.5f)*120)+
                        player->GetOrientation().GetUp()*((float(i/width)-float(width-1)*.5f)*120);
                }
                bot->SetPosition(options.observerBattleDepth>0?offset:player->GetPosition()+offset);
                bot->SetOrientation(Orientation(options.observerBattleDepth>0?Vector(0,0,side?-1.f:1.f):player->GetPosition()-bot->GetPosition()));
                bot->SetCluster(cluster);bot->Release();
            }
        }
        for(auto* l=mission->GetShips()->first();l;l=l->next())if(l->data()->GetPilotType()<c_ptPlayer) {
            auto* bot=l->data();if(bot->GetCommandID(c_cmdAccepted)!=c_cidNone)continue;IshipIGC* enemy=nullptr;float distance=FLT_MAX;
            for(auto* e=mission->GetShips()->first();e;e=e->next())if(e->data()->GetSide()!=bot->GetSide() && e->data()->GetCluster()==bot->GetCluster() && bot->CanSee(e->data())) {
                float d=(e->data()->GetPosition()-bot->GetPosition()).LengthSquared();if(d<distance){distance=d;enemy=e->data();}
            }
            bot->SetCommand(c_cmdAccepted,enemy,enemy?c_cidAttack:c_cidNone);bot->SetCommand(c_cmdCurrent,enemy,enemy?c_cidAttack:c_cidNone);
        }
    }
    bool targetKey(TrekKey key) {
        if(!player || state.dead || state.complete)return false;
        auto* station=player->GetStation();
        return Allegiance::SelectTargetForKey(player,inCommandView()?viewCluster:station?station->GetCluster():nullptr,key,
            [&](ImodelIGC* model,CommandID command){setTarget(model,false,command);if(inCommandView()){commandTracking=true;commandDisplacement=Vector::GetZero();
                if(model && model->GetObjectType()==OT_ship && model->GetSide()==player->GetSide()){commandUnit=model->GetObjectID();commandUnits.assign(1,commandUnit);commandSelected.reset();commandSelected.set(commandUnit);}}});
    }
    void nextTarget() {targetKey(TK_Target);}
    void setTarget(ImodelIGC* model,bool knownWaypoint=false,CommandID command=c_cidDefault) {
        if(model && !knownWaypoint && !seen(model))return;
        if(!player || state.dead)return;
        if(model && model!=target)PlaySoundEffect(newTargetSound);
        target=model;targetRock=targetRockSector=-1;
        Allegiance::SetCurrentTarget(player,model,command);
        refresh();
    }
    void selectTarget(int type,int id) {
        if(!player)return;
        auto* cluster=player->GetCluster();if(training && training->viewCluster)cluster=training->viewCluster;else if(commandViewEnabled && commandCluster)cluster=commandCluster;
        if(!cluster && player->GetStation())cluster=player->GetStation()->GetCluster();if(!cluster)return;
        for(auto* l=cluster->GetPickableModels()->first();l;l=l->next())
            if(l->data()->GetObjectType()==type && l->data()->GetObjectID()==id && l->data()!=player && seen(l->data())){setTarget(l->data());return;}
        if(type==OT_asteroid)for(auto& rock:rememberedRocks)if(rock.id==id && rock.sector==cluster->GetObjectID() && (rock.sides&(1u<<player->GetSide()->GetObjectID()))) {
            // An observed rock is no longer a physical object. Use an original
            // waypoint for navigation while presenting only the retained data.
            DataBuoyIGC data{};data.position=vec(rock.position);data.buoyID=mission->GenerateNewBuoyID();data.clusterID=rock.sector;data.type=c_buoyWaypoint;
            auto* buoy=static_cast<IbuoyIGC*>(mission->CreateObject(mission->GetLastUpdate(),OT_buoy,&data,sizeof(data)));
            if(buoy){buoy->SetName(rock.name.c_str());buoy->SetRadius(rock.radius);setTarget(buoy,true);buoy->Release();targetRock=rock.id;targetRockSector=rock.sector;refresh();}return;
        }
    }
    bool selectWeapon(int mount) {
        if(!player || state.dead)return false;
        if(player->GetParentShip() || player->GetStation() || mount< -1 || mount>=player->GetHullType()->GetMaxFixedWeapons())return false;
        // These are local interface cues in WinTrek. The predicting client
        // already plays them; the authority must not broadcast another copy.
        bool audible=gameRole!=GameRole::Authority;
        if(mount==-1){groupFire=!groupFire;if(audible)PlaySoundEffect(groupFire?groupWeaponsSound:ungroupWeaponsSound,player);}
        else {auto sound=Allegiance::ToggleWeaponSelection(player,Mount(mount),groupFire,[&](Mount m){selectedWeapon=m;});if(sound==NA)return false;if(audible)PlaySoundEffect(sound,player);}
        refresh();return true;
    }
    bool setWeaponSelection(int mount,bool group) {
        if(!player || state.dead || player->GetParentShip() || player->GetStation() || mount<0 || mount>3)return false;
        selectedWeapon=Mount(mount);groupFire=group;refresh();return true;
    }
    bool cycleWeapon(bool previous) {
        if(!player || state.dead || player->GetParentShip())return false;
        selectedWeapon=Allegiance::CycleWeapon(player,selectedWeapon,previous);refresh();return true;
    }
    void resetManualThrottle() {
        throttleReset=(Allegiance::AutopilotReleaseThrottle(player)+1)*.5f;throttleResetPending=true;forcedThrottleReset=false;
    }
    void toggleAutopilot() {
        if(!player || state.dead || player->GetStation() || state.complete)return;
        if(player->GetParentShip())return;
        bool engage=!player->GetAutopilot();
        player->SetAutopilot(engage);autopilotInputInitialized=false;
        if(!engage)resetManualThrottle();
        PlaySoundEffect(player->GetAutopilot()?salAutopilotEngageSound:salAutopilotDisengageSound);
        refresh();
    }
    bool acceptCommand(bool execute) {
        if(!player || state.dead || state.complete)return false;
        auto request=moneyRequests.find(player->GetObjectID());
        if(request!=moneyRequests.end()) {
            auto value=request->second;auto* sender=mission->GetShip(ShipID(value.sender));
            Money approved=Allegiance::ApprovedMoneyRequest(value.amount,readBalance(player));
            if(sender && sender!=player && approved && accounts.contains(sender->GetObjectID()))moneyTransfer(player,sender,approved);
            moneyRequests.erase(player->GetObjectID());pendingOrders.erase(player->GetObjectID());economyDirty=true;
            if(sender && sender!=player)SendChat(player,CHAT_INDIVIDUAL,sender->GetObjectID(),voAffirmativeSound,"Affirmative.");
            updateEconomy();refresh();return true;
        }
        auto queuedCommand=player->GetCommandID(c_cmdQueued);
        bool accepted=Allegiance::AcceptQueuedOrder(player,execute,
            [&]{PlaySoundEffect(acceptCommandSound);},
            [&](IshipIGC* model){player->SetWingID(model->GetWingID());},
            [&](IshipIGC* model){SendChat(player,CHAT_INDIVIDUAL,model->GetObjectID(),NA,nullptr,c_cidPickup,OT_ship,player->GetObjectID(),player);},
            [&](ImodelIGC* model,CommandID command){setTarget(model,true,command);},
            [&](ImodelIGC* model,CommandID command){player->SetCommand(c_cmdAccepted,model,command);PostText(false,"Command accepted.");PostText(true,"");},
            [&](ImodelIGC* model){return model->GetCluster() && player->CanSee(model);},
            [&]{player->SetAutopilot(true);autopilotInputInitialized=false;PlaySoundEffect(salAutopilotEngageSound);});
        auto pending=pendingOrders.find(player->GetObjectID());
        if(pending!=pendingOrders.end()) {
            TRef<IshipIGC> sender=pending->second.sender;pendingOrders.erase(pending);
            if(sender && sender!=player)SendChat(player,CHAT_INDIVIDUAL,sender->GetObjectID(),voAffirmativeSound,"Affirmative.");
        }
        if(training && accepted)Training::CommandAcknowledgedCondition::SetCommandAcknowledged(queuedCommand);
        refresh();return accepted;
    }
    void rejectCommand(bool negative=true) {
        if(!player)return;
        rejectRequest(player,true,negative);updateEconomy();refresh();
    }
    void settleDeaths() {
        for(size_t i=0;i<killed.size();++i) {TRef<ImodelIGC> model=killed[i];if(model->GetMission()!=mission)continue;if(static_cast<ImodelIGC*>(model)==static_cast<ImodelIGC*>(target))target=nullptr;if(static_cast<ImodelIGC*>(model)==static_cast<IshipIGC*>(player))state.dead=true;model->Terminate();}killed.clear();
    }
    bool activateActor(int id) {
        auto* next=mission->GetShip(ShipID(id));if(!next || next->GetPilotType()<c_ptPlayer)return false;
        if(player && player->GetMission()==mission)actors[player->GetObjectID()]={commandViewEnabled,commandCluster,target,selectedWeapon,selectedCargo,groupFire,autopilotInputInitialized,state.dead,autopilotAxes,previousActionButtons};
        player=next;auto& a=actors[id];commandViewEnabled=a.commandView;commandCluster=a.commandSector;if(a.target && a.target->GetMission()!=mission)a.target=nullptr;target=a.target;selectedWeapon=a.weapon;selectedCargo=a.cargo;groupFire=a.group;
        autopilotInputInitialized=a.axesInitialized;autopilotAxes=a.axes;previousActionButtons=a.actions;state.dead=a.dead;
        viewCluster=player->GetSourceShip()->GetCluster();if(!viewCluster && player->GetStation())viewCluster=player->GetStation()->GetCluster();
        if(commandViewEnabled && commandCluster)viewCluster=commandCluster;economyDirty=true;return true;
    }
    void applyFlight(const FlightInput& input) {
        const bool active=input.active && !(gameRole!=GameRole::Authority && cinematicStop>=0 && cinematicDeath);
        if(onTeam() && !state.dead && !player->GetStation()) {
            ControlData controls;controls.jsValues[c_axisYaw]=input.yaw;controls.jsValues[c_axisPitch]=input.pitch;
            controls.jsValues[c_axisRoll]=input.roll;controls.jsValues[c_axisThrottle]=input.throttle*2-1;
            int buttons=player->GetStateM()&(coastButtonIGC|cloakActiveIGC);
            const unsigned actions=unsigned(input.fire)|(unsigned(input.missile)<<1)|(unsigned(input.boost)<<2)|(unsigned(input.matchSpeed)<<3);
            if(active) {
                const int persistent=buttons;
                buttons|=(input.forward?forwardButtonIGC:0)|(input.backward?backwardButtonIGC:0)|
                    (input.left?leftButtonIGC:0)|(input.right?rightButtonIGC:0)|(input.up?upButtonIGC:0)|
                    (input.down?downButtonIGC:0)|(input.boost?afterburnerButtonIGC:0);
                if(player->GetAutopilot()) {
                    bool used=input.manualOverride || input.throttleChanged || buttons!=persistent || actions || (actions!=previousActionButtons);
                    if(!input.canonical && !autopilotInputInitialized) {
                        std::copy_n(controls.jsValues,3,autopilotAxes.begin());autopilotInputInitialized=true;
                    } else if(!input.canonical)used|=Allegiance::AutopilotAxesChanged(controls,autopilotAxes.data(),input.deadZone);
                    if(used) {
                        player->SetAutopilot(false);resetManualThrottle();PlaySoundEffect(salAutopilotDisengageSound);
                    }
                }
                previousActionButtons=actions;
            } else controls.Reset();
            if(!player->GetAutopilot())autopilotInputInitialized=false;
            if(player->GetAutopilot() && !player->GetParentShip()) {
                controls=player->GetControls();buttons=player->GetStateM();
            } else buttons=Allegiance::ManualWeaponButtons(buttons,selectedWeapon,groupFire,
                active && input.fire,active && input.missile,active && input.boost);
            buttons=Allegiance::ExpendableButtons(buttons,active && input.deploy,active && input.chaff);
            if(training && Training::IsTraining()){Training::ApproveControls(controls);buttons=Training::ApproveActions(buttons);}
            if(!input.canonical && input.fieldOfView>=0 && (input.fieldOfView>0 || (training?training->camera==TrainingHost::cmCockpit:cameraMode(cameraInput)==CameraMode::Cockpit)) && player->GetTurretID()==NA && player->GetBaseHullType())
                Allegiance::LimitCockpitSlew(controls,player->GetHullType(),RadiansFromDegrees(input.fieldOfView>0?input.fieldOfView:cameraInput.fieldOfView),RadiansFromDegrees(50.f));
            Allegiance::ApplyClientControls(player,controls,buttons);
        }
    }
    void step(const FlightInput& input,double dt) {
        if(training && training->paused){Training::HandleMission();refresh();return;}
        if(state.complete){settleDeaths();updateEconomy();refresh();return;}
        if(fx)fx->step(float(dt));
        int oldAmmo=state.ammo,oldMissiles=state.missiles;
        dt=std::clamp(dt,0.0,0.1);elapsed+=dt;state.time=elapsed;
        if(cinematicStop>=0 && elapsed>=cinematicStop)endCinematic();state.throttle=input.throttle;
        if(training){training->delta=float(dt);if(trainingRestoreAt>=0 && elapsed>=trainingRestoreAt){trainingRestoreAt=-1;Training::RestoreShip();}}
        expireOrders();
        if(gameRole==GameRole::Authority) {
            int old=player->GetObjectID();
            for(auto& [id,controls]:authorityInputs)if(activateActor(id))applyFlight(controls);
            activateActor(old);
        } else applyFlight(input);
        mission->Update(start+float(elapsed));
        settleDeaths();
        if(!state.dead && !training) {
            botTimer+=dt;if(botTimer>8){botTimer=0;spawnBots();}

        }
        if(training)Training::HandleMission();
        updateEconomy();
        const auto* params=mission->GetMissionParams();
        updateBallots();
        if(economy() && params->IsCountdownGame())PlaySoundEffect(countdownSounds.Observe(params->GetCountDownTime()-elapsed,false));
        if(!state.complete && economy() && params->IsCountdownGame() && elapsed>params->GetCountDownTime()) {
            auto victory=Allegiance::CheckTimedVictory(mission);state.complete=true;state.endgame.winner=victory.winner?victory.winner->GetObjectID():NA;
            state.result=victory.winner?std::string("Time expired: ")+victory.winner->GetName()+" wins by "+victory.reason:"Time expired: draw";
        }
        refresh();
        if(!state.dead && !state.docked) {
            engineLevels.Update(player->GetSourceShip(),float(dt),!engineLevelsValid);engineLevelsValid=true;
            chaseMotion.Record(elapsed,Vector(player->GetSourceShip()->GetCurrentTurnRate(c_axisYaw),
                player->GetSourceShip()->GetCurrentTurnRate(c_axisPitch),player->GetSourceShip()->GetCurrentTurnRate(c_axisRoll)));
        }
        emitEffects(float(dt));
        firedGun|=state.ammo<oldAmmo;firedMissile|=state.missiles<oldMissiles;
        if(input.left || input.right || input.up || input.down)strafeTime+=dt;
        objectives(input);
    }
    std::span<const SoundLoop> shipSounds(bool interior) {
        if(state.dead || state.docked || !player)return {};
        auto* source=player->GetSourceShip();auto* hull=source->GetHullType();auto* booster=static_cast<IafterburnerIGC*>(source->GetMountedPart(ET_Afterburner,0));
        glm::vec3 engine=state.position;
        auto* thing=static_cast<Thing*>(source->GetThingSite());
        if(thing && thing->trail)engine=glm::vec3(matrix(source->GetOrientation(),source->GetPosition(),thing->radius/thing->handle.radius,thing->flip)*glm::vec4(thing->trail->position,1));
        auto loop=[&](int index,int id,float gain,float pitch,bool atEngine) {
            engineLoops[index]={id,std::pow(10.f,gain/20),pitch,interior?glm::vec3(0):atEngine?engine:state.position,interior?glm::vec3(0):state.velocity,interior};
        };
        loop(0,interior?hull->GetInteriorSound():hull->GetExteriorSound(),0,1,false);
        loop(1,engineLevels.thrust>.01f?(interior?hull->GetMainThrusterInteriorSound():hull->GetMainThrusterExteriorSound()):NA,
            engineLevels.MainGain(interior),engineLevels.MainPitch(),true);
        loop(2,engineLevels.turn>.01f?(interior?hull->GetManuveringThrusterInteriorSound():hull->GetManuveringThrusterExteriorSound()):NA,
            engineLevels.TurnGain(interior),engineLevels.TurnPitch(interior),false);
        loop(3,booster && booster->GetPower()>0?(interior?booster->GetInteriorSound():booster->GetExteriorSound()):NA,0,1,true);
        bool sideThrust=(source->GetStateM()&(backwardButtonIGC|leftButtonIGC|rightButtonIGC))!=0;
        bool fighter=hull->HasCapability(c_habmFighter);
        loop(4,sideThrust?(interior?(fighter?SidethrustInteriorSound:CapSidethrustInteriorSound):
            (fighter?SidethrustExteriorSound:CapSidethrustExteriorSound)):NA,0,1,false);
        return engineLoops;
    }
    std::span<const SoundLoop> worldSounds(bool interior) {
        for(auto& emitter:activeEmitters)emitter->active=false;activeEmitters.clear();
        worldLoops.clear();if(!player || state.dead)return worldLoops;
        if(state.docked && !state.commandView && !state.cinematic) {
            auto* station=player->GetStation();int sound=station?station->GetInteriorSound():NA;
            if(sound!=NA)worldLoops.push_back({sound,1,1,{},{},true,UINT64_MAX});
            return worldLoops;
        }
        if(!viewCluster)return worldLoops;
        Allegiance::IncomingMissileWarning missileWarning;auto* source=player->GetSourceShip();
        for(auto* link=viewCluster->GetModels()->first();link;link=link->next()) {
            auto* model=link->data();auto* thing=static_cast<Thing*>(model->GetThingSite());
            if(!thing)continue;
            // Silent bullets need no source allocation or spatial audio update.
            // Retain an existing emitter, which may carry a one-shot sound.
            if(model->GetObjectType()==OT_projectile && !thing->emitter &&
                static_cast<IprojectileIGC*>(model)->GetProjectileType()->GetAmbientSound()==NA)continue;
            if(model->GetObjectType()==OT_missile)missileWarning.Observe(static_cast<ImissileIGC*>(model),source);
            if(model!=player && !eyed(model))continue;
            activeEmitters.push_back(emitterFor(model));
            bool relative=model==player && interior;
            auto loop=[&](unsigned slot,int id,float gain=1,float pitch=1,const Vector& offset=Vector::GetZero(),const Vector& direction=Vector(0,0,-1)) {
                if(id<0)return;
                worldLoops.push_back({id,gain,pitch,relative?vec(offset):vec(model->GetPosition()+offset*model->GetOrientation()),
                    relative?glm::vec3(0):vec(model->GetVelocity()),relative,(thing->soundKey<<8)|slot,
                    relative?vec(direction):vec(direction*model->GetOrientation())});
            };
            switch(model->GetObjectType()) {
            case OT_asteroid:loop(0,asteroidSound);break;
            case OT_warp:loop(0,alephSound);break;
            case OT_buildingEffect:loop(0,buildSound);break;
            case OT_station:loop(0,static_cast<IstationIGC*>(model)->GetExteriorSound());break;
            case OT_probe:loop(0,static_cast<IprobeIGC*>(model)->GetAmbientSound());break;
            case OT_missile:loop(0,static_cast<ImissileIGC*>(model)->GetMissileType()->GetAmbientSound());break;
            case OT_projectile:loop(0,static_cast<IprojectileIGC*>(model)->GetProjectileType()->GetAmbientSound());break;
            case OT_ship: {
                auto* ship=static_cast<IshipIGC*>(model);auto* hull=ship->GetBaseHullType();
                if(!hull || ship->GetParentShip())break;
                bool own=ship==player->GetSourceShip();bool fresh=thing->soundTime!=elapsed;
                if(fresh)thing->soundLevels.Update(ship,thing->soundTime<0?0:float(elapsed-thing->soundTime),thing->soundTime<0);
                auto& levels=thing->soundLevels;
                auto engine=thing->GetChildModelOffset("trail");
                auto* booster=static_cast<IafterburnerIGC*>(ship->GetMountedPart(ET_Afterburner,0));
                bool sideThrust=(ship->GetStateM()&(backwardButtonIGC|leftButtonIGC|rightButtonIGC))!=0;
                bool fighter=hull->HasCapability(c_habmFighter);
                for(int inside=0;inside<=(own?1:0);++inside) {
                    unsigned base=inside?5:0;float volume=own && (bool(inside)!=interior)?0.f:1.f;
                    loop(base,inside?hull->GetInteriorSound():hull->GetExteriorSound(),volume);
                    if(levels.thrust>.01f)loop(base+1,inside?hull->GetMainThrusterInteriorSound():hull->GetMainThrusterExteriorSound(),volume*std::pow(10.f,levels.MainGain(inside)/20),levels.MainPitch(),engine,Vector(0,0,1));
                    if(levels.turn>.01f)loop(base+2,inside?hull->GetManuveringThrusterInteriorSound():hull->GetManuveringThrusterExteriorSound(),volume*std::pow(10.f,levels.TurnGain(inside)/20),levels.TurnPitch(inside));
                    if(booster && booster->GetPower()>0)loop(base+3,inside?booster->GetInteriorSound():booster->GetExteriorSound(),volume,1,engine,Vector(0,0,1));
                    if(sideThrust)loop(base+4,inside?(fighter?SidethrustInteriorSound:CapSidethrustInteriorSound):(fighter?SidethrustExteriorSound:CapSidethrustExteriorSound),volume);
                }
                if(ship->GetStateM()&miningMaskIGC)loop(10,miningSound);
                if(own) {
                    if(ship->GetWarningMask()&(c_wmOutOfBounds|c_wmCrowdedSector))loop(11,outOfBoundsLoopSound);
                    if(ship->fRipcordActive())loop(12,ripcordOnLoopSound);
                    auto* magazine=static_cast<ImagazineIGC*>(ship->GetMountedPart(ET_Magazine,0));
                    if(magazine && magazine->GetLock()==1)loop(13,missileToneSound);
                    if(notificationTime!=elapsed){soundNotifications.Update(player,[&](SoundID id){PlaySoundEffect(id);});notificationTime=elapsed;}
                }
                for(Mount mount=0;mount<hull->GetMaxWeapons();++mount) {
                    auto selected=Allegiance::SelectWeaponSounds(ship,mount,player,thing->burstPlaying[mount]);
                    auto offset=hull->GetWeaponPosition(mount);auto direction=hull->GetWeaponOrientation(mount).GetForward();
                    loop(32+mount*3,selected.active,1,1,offset,direction);
                    loop(33+mount*3,selected.burst,1,1,offset,direction);
                    loop(34+mount*3,selected.turret,1,1,offset,direction);
                    if(fresh && selected.shot!=NA)PlaySoundEffect(selected.shot,ship,offset);
                    thing->burstPlaying[mount]=selected.burst!=NA;
                }
                thing->soundTime=elapsed;break;
            }
            default:break;
            }
        }
        if(missileWarning.active && player->GetCluster())worldLoops.push_back({missileLockSound,1,missileWarning.Pitch(),
            interior?glm::vec3(0):vec(source->GetPosition()),interior?glm::vec3(0):vec(source->GetVelocity()),interior,UINT64_MAX-1});
        return worldLoops;
    }
    void resetCamera() {pendingCommandFocus.reset();cinematicStop=-1;cinematicCluster=nullptr;cameraValid=false;chaseMotion.Reset();externalCameraOrientation.Reset();lastCameraMissile=nullptr;}
    void endCinematic() {
        // WinTrek::EndOverrideCamera plays this when a death override returns
        // to the hangar or an exhausted pilot's command seat, not a flying pod.
        if(cinematicDeath && !training && player && player->GetStation() && !state.complete) {
            if(networkReplaying)recoverySoundPending=true;
            else PlaySoundEffect(personalJumpSound);
        }
        if(cinematicDeath || !cameraValid || !player || player->GetStation() || player->IsGhost() ||
           !player->GetSourceShip()->GetHullType() || !player->GetSourceShip()->GetCluster()) {resetCamera();return;}
        // WinTrek ends a launch override as cmExternal, then SetViewMode
        // animates from its last pose to the cockpit (or preferred chase view).
        // Preserve the applied pose and clock so the hull remains visible
        // until that normal camera transition has completed.
        cinematicStop=-1;cinematicCluster=nullptr;
        cameraInput.mode=CameraMode::External;cameraInput.chase=false;
    }
    void beginCinematic(ImodelIGC* focus,bool death) {
        if(gameRole==GameRole::Authority)return;
        if(!player || !player->GetSourceShip()->GetCluster())return;
        resetCamera();auto* source=player->GetSourceShip();cinematicCluster=source->GetCluster();
        cinematicStop=elapsed+Allegiance::OverrideCameraDuration(death);cinematicDeath=death;
        cinematicOrientation=Allegiance::OverrideCameraOrientation();
        if(death) {
            const Vector* toward=focus && focus->GetCluster()==cinematicCluster?&focus->GetPosition():nullptr;
            Allegiance::DeathCameraFrame(source->GetPosition(),source->GetOrientation(),source->GetRadius(),toward,cinematicOrientation,cinematicPosition);
        } else cinematicTarget=focus?focus->GetPosition():source->GetPosition()+source->GetOrientation().GetBackward()*source->GetRadius()*2;
        cameraMotion.SetAnimation(0);cameraMotion.SetHeadOrientation(0);cameraMotion.SetOrientation(cinematicOrientation);
    }
    const CameraState& cinematicCamera(const CameraInput& input) {
        const double now=cameraClock();float dt=cameraValid?float(std::max(0.,now-cameraTime)):0;
        if(!cinematicDeath) {
            auto* source=player->GetSourceShip();
            Allegiance::OverrideCameraFrame(source->GetPosition(),cinematicTarget,source->GetRadius(),false,cinematicOrientation,cinematicPosition);
        }
        cameraMotion.SetAnimation(0);cameraMotion.SetPosition(cinematicPosition);cameraMotion.SetOrientation(cinematicOrientation);
        cameraMotion.SetHeadOrientation(0);cameraMotion.SetDefaultFOV(RadiansFromDegrees(input.fieldOfView));cameraMotion.SetFOV(RadiansFromDegrees(input.fieldOfView));
        cameraMotion.Apply(dt,false,false,0,0,0,[](float,float){return 0.f;});
        cameraState={vec(cinematicPosition),vec(cinematicOrientation.GetForward()),vec(cinematicOrientation.GetUp()),input.fieldOfView,!cinematicDeath,false,CameraMode::External};
        state.eyePosition=cameraState.position;state.viewFov=input.fieldOfView;state.camera=CameraMode::External;
        state.hasLead=state.inRange=state.hasWeaponSolution=state.hasExternalAim=false;
        cameraTime=now;cameraInput=input;cameraValid=true;updateTargetView();return cameraState;
    }
    void updateWeaponIndicator() {
        state.hasLead=state.inRange=state.hasWeaponSolution=state.hasExternalAim=false;
        if(!player || state.dead || state.docked)return;
        auto* source=player->GetSourceShip();auto* parent=player->GetParentShip();Mount mount=parent?player->GetTurretID():selectedWeapon;
        auto* weapon=mount==NA?nullptr:static_cast<IweaponIGC*>(source->GetMountedPart(ET_Weapon,mount));
        if(!weapon)return;
        bool external=state.camera==CameraMode::Chase || state.camera==CameraMode::Missile;
        bool visible=target && targetRock<0 && target->GetCluster()==source->GetCluster() && eyed(target);
        if(visible && Allegiance::WeaponIndicatorTarget(weapon,target,player->GetSide(),external)) {
            Vector direction;float t=solveForLead(source,target,weapon,&direction);
            if(t<0 || !std::isfinite(t))return;
            state.hasWeaponSolution=true;state.inRange=t<=weapon->GetLifespan();state.leadDirection=vec(direction);state.targetRadius=target->GetRadius();
            state.hasLead=!external && Allegiance::HasLeadIndicator(source,target,parent!=nullptr);
            if(external) {
                state.hasExternalAim=true;state.aimPosition=vec(player->GetPosition()+player->GetOrientation().GetForward()*(target->GetPosition()-player->GetPosition()).Length());
                state.aimGreen=Allegiance::ExternalAimGreen(t,weapon->GetProjectileType()->GetSpeed(),direction*player->GetOrientation().GetForward(),target->GetRadius());
            }
        } else if(!visible && external && !parent) {
            state.hasExternalAim=true;state.aimPosition=vec(player->GetPosition()+player->GetOrientation().GetForward()*(weapon->GetProjectileType()->GetSpeed()*weapon->GetLifespan()));
        }
    }
    void updateTargetView() {
        if(state.targetHasModel)state.targetViewOrientation=matrix(cameraMotion.GetTargetOrientation(vec(state.targetPosition)),Vector::GetZero(),1);
    }
    const CameraState& camera(const CameraInput& requested) {
        CameraInput input=requested;
        if(cinematicStop>=0)return cinematicCamera(input);
        if(training) {
            input.chase=false;
            constexpr CameraMode modes[]={CameraMode::Cockpit,CameraMode::External,CameraMode::Chase,CameraMode::Target,CameraMode::Base,CameraMode::Missile,
                CameraMode::FlyBy,CameraMode::FlyBy,CameraMode::Orbit,CameraMode::Station,CameraMode::External,CameraMode::Chase,CameraMode::Command,CameraMode::Command};
            input.mode=modes[training->camera];
        }
        if(state.dead || !player || !player->GetSourceShip()->GetHullType())return cameraState;
        if(inCommandView())input.mode=CameraMode::Command;
        if(input.mode==CameraMode::Command && (input.panRight || input.panDown || input.zoom || input.roll))pendingCommandFocus.reset();
        auto mode=cameraMode(input);bool internal=mode==CameraMode::Cockpit;
        const double now=cameraClock();
        if(cameraValid && !commandCameraDirty && now==cameraTime && mode==cameraMode(cameraInput) &&
           input.viewportHeight==cameraInput.viewportHeight && input.fieldOfView==cameraInput.fieldOfView && input.headYaw==cameraInput.headYaw && input.distance==cameraInput.distance &&
           input.yaw==cameraInput.yaw && input.pitch==cameraInput.pitch && input.roll==cameraInput.roll &&
           input.panRight==cameraInput.panRight && input.panDown==cameraInput.panDown && input.zoom==cameraInput.zoom){updateTargetView();return cameraState;}
        float dt=cameraValid?float(std::max(0.,now-cameraTime)):0;
        float fov=internal && player->GetTurretID()!=NA?Allegiance::TurretFieldOfView(player->GetControls().jsValues[c_axisThrottle]):RadiansFromDegrees(input.fieldOfView);
        if(!cameraValid) {
            cameraMotion.SetAnimation(0);cameraWasChase=!internal;
        } else if(mode!=cameraMode(cameraInput)) {
            cameraWasChase=cameraMode(cameraInput)!=CameraMode::Cockpit;
            cameraMotion.SetAnimation(internal?1.f:2.f);
        }
        cameraMotion.SetDefaultFOV(fov);cameraMotion.SetFOV(fov);
        cameraMotion.SetHeadOrientation(input.headYaw);
        auto* source=player->GetSourceShip();auto* hull=source->GetHullType();
        Orientation orientation=source->GetOrientation();Vector position;
        externalCameraOrientation.Pitch(input.pitch*dt);externalCameraOrientation.Yaw(input.yaw*dt);externalCameraOrientation.Roll(input.roll*dt);
        auto chase=[&]{chaseMotion.Compute(source->GetOrientation(),source->GetPosition(),source->GetVelocity(),hull->GetLength(),hull->GetMaxSpeed(),
            cameraMotion.GetHeadOrientation(),input.distance,orientation,position);};
        auto track=[&](const Vector& from,const Vector& toward,float length){orientation=cameraMotion.GetOrientation();Allegiance::TrackCamera(from,toward,length,orientation,position);};
        ImodelIGC* visibleTarget=target && (seen(target) || (targetRock>=0 && target->GetObjectType()==OT_buoy)) && target->GetCluster()==source->GetCluster()?static_cast<ImodelIGC*>(target):nullptr;
        switch(mode) {
        case CameraMode::Cockpit: {
            Mount turret=player->GetTurretID();auto offset=turret==NA?hull->GetCockpit():hull->GetWeaponPosition(turret);
            position=source->GetPosition()+offset*source->GetOrientation();
            if(turret!=NA)orientation=player->GetOrientation();break;
        }
        case CameraMode::Chase:chase();break;
        case CameraMode::Orbit:
            Allegiance::OrbitCamera(externalCameraOrientation,dt);orientation=externalCameraOrientation;
            position=source->GetPosition()+orientation.GetBackward()*(5.f+2.5f*hull->GetLength());break;
        case CameraMode::FlyBy: {
            orientation=externalCameraOrientation;float offset=input.distance;position=source->GetPosition();
            if(visibleTarget)Allegiance::FrameFlyBy(source->GetPosition(),visibleTarget->GetPosition(),orientation,position,offset);
            position+=offset*orientation.GetBackward();break;
        }
        case CameraMode::Command: {
            if(!training || training->displacement) {
                commandRoll=Allegiance::CommandViewRoll(commandRoll,input.roll*dt);
                commandDistance=Allegiance::CommandViewDistance(commandDistance,dt*commandDistance*input.zoom*2);
            }
            orientation=Allegiance::CommandViewOrientation(training?training->camera==TrainingHost::cmExternalCommandViewTD:commandTopDown,commandRoll);
            if(!training || training->displacement)Allegiance::PanCommandView(orientation,
                dt*commandDistance*input.panRight,dt*commandDistance*input.panDown,commandDisplacement,commandCenter);
            if(commandTracking && target && target->GetCluster()==viewCluster && eyed(target)) {
                commandCenter=target->GetPosition()+commandDisplacement;
                commandDistance=std::max(commandDistance,5.f+target->GetRadius()*2);
            }
            Allegiance::ClampCommandView(commandCenter);
            position=commandCenter+orientation.GetBackward()*commandDistance;break;
        }
        case CameraMode::Station:
            if(visibleTarget && visibleTarget!=source && visibleTarget->GetHitTest())track(source->GetPosition(),visibleTarget->GetPosition(),5.f+source->GetRadius()*2.5f);
            else chase();break;
        case CameraMode::Missile: {
            auto* missile=source->GetLastMissileFired();
            if(missile!=lastCameraMissile){cameraMotion.SetOrientation(source->GetOrientation());cameraMotion.SetAnimation(1.f);lastCameraMissile=missile;}
            if(missile && missile->GetCluster()==source->GetCluster()) {
                auto* aim=missile->GetTarget();track(missile->GetPosition(),aim && aim->GetMission()?aim->GetPosition():source->GetPosition(),missile->GetRadius()*3.f);
            } else chase();break;
        }
        default: {
            auto* focus=mode==CameraMode::Target && visibleTarget?visibleTarget:static_cast<ImodelIGC*>(source);
            orientation=externalCameraOrientation;float distance=std::max(input.distance,5.f+2.f*focus->GetRadius());
            position=focus->GetPosition()+distance*orientation.GetBackward();break;
        }
        }
        cameraMotion.SetPosition(position);cameraMotion.SetOrientation(orientation);
        auto* booster=static_cast<IafterburnerIGC*>(source->GetMountedPart(ET_Afterburner,0));
        auto* warp=!internal?nullptr:FindTarget(source,c_ttWarp|c_ttNearest|c_ttAllSides);
        float range=warp?(warp->GetPosition()-source->GetPosition()).Length()-source->GetRadius():0;
        cameraState.animating=cameraMotion.Apply(dt,internal,booster!=nullptr,booster?booster->GetPower():0,
            range,warp?warp->GetRadius():0,[&](float low,float high) {
                // Visual noise cannot perturb IGC combat, AI, or equipment randomness.
                cameraRandom^=cameraRandom<<13;cameraRandom^=cameraRandom>>17;cameraRandom^=cameraRandom<<5;
                return low+(high-low)*float(cameraRandom>>8)*(1.0f/16777216.0f);
            });
        cameraState.position=vec(cameraMotion.GetPosition());cameraState.forward=vec(cameraMotion.GetViewOrientation().GetForward());
        cameraState.up=vec(cameraMotion.GetViewOrientation().GetUp());cameraState.fieldOfView=DegreesFromRadians(cameraMotion.GetViewFOV());
        state.eyePosition=cameraState.position;state.viewFov=cameraState.fieldOfView;
        cameraState.showPlayer=!internal || (cameraState.animating && cameraWasChase);cameraState.mode=mode;bool indicatorModeChanged=state.camera!=mode;state.camera=mode;
        if(indicatorModeChanged)updateWeaponIndicator();
        updateTargetView();
        if(fx && !state.docked && viewCluster)fx->motionDebris(viewCluster->GetObjectID(),cameraState.position,
            cameraState.forward,vec(cameraMotion.GetViewOrientation().GetRight()),cameraState.up,dt,std::tan(cameraMotion.GetViewFOV()*.5f));
        cameraTime=now;cameraInput=input;cameraValid=true;commandCameraDirty=false;
        return cameraState;
    }
    void emitEffects(float dt) {
        if(!fx)return;
        fx->emitters(viewCluster?viewCluster->GetShips()->n():0);
        for(auto* t:things) {
            if(t->replicaStrategic)continue;
            auto* model=t->model;t->age+=dt;auto* cluster=model->GetCluster();if(!cluster)continue;
            if(model->GetObjectType()!=OT_ship && t->exhaust.empty() && !t->trail && !t->damageCount && t->ripcordTime<0 && t->alephTime<0)continue;
            if(cinematicStop>=0 && cinematicDeath && player && model==player->GetSourceShip())continue;
            if(model->GetObjectType()==OT_ship) {
                auto* ship=static_cast<IshipIGC*>(model);auto* booster=static_cast<IafterburnerIGC*>(ship->GetMountedPart(ET_Afterburner,0));
                t->power=booster?booster->GetPower():0;t->exhaustDirection=vec(ship->GetOrientation().GetBackward());
                t->thrust=std::clamp(.5f*(ship->GetControls().jsValues[c_axisThrottle]+1),0.f,1.f);
            }
            if(!eyed(model)){t->hadPosition=t->hadTrail=false;t->emission=t->trailTime=0;continue;}
            auto position=vec(model->GetPosition()),velocity=vec(model->GetVelocity());
            auto transform=matrix(model->GetOrientation(),model->GetPosition(),t->hasModel?t->radius/t->handle.radius:1,t->flip);
            if(!t->hadPosition || t->previousSector!=cluster->GetObjectID() || glm::distance(position,t->previous)>100) {
                t->previous=position;t->previousVelocity=velocity;t->hadPosition=true;t->hadTrail=false;t->emission=t->trailTime=0;
            }
            t->previousSector=cluster->GetObjectID();
            if(cluster!=viewCluster){t->previous=position;t->hadTrail=false;continue;}
            // Run the original presentation equations at a fixed 60 Hz cadence;
            // their per-frame minimum must not multiply density at 165/240 Hz.
            constexpr float interval=1.f/60.f;
            t->emission+=dt;int ticks=std::min(6,int(t->emission/interval));t->emission-=ticks*interval;
            auto acceleration=dt>0?(velocity-t->previousVelocity)/dt:glm::vec3(0);t->previousVelocity=velocity;
            auto random=[&](float a,float b){return fx->random(a,b);};auto direction=[&]{return fx->direction();};
            if(ticks && model->GetObjectType()==OT_ship && !static_cast<IshipIGC*>(model)->GetParentShip()) {
                auto* ship=static_cast<IshipIGC*>(model);t->ripcordTime=ship->fRipcordActive()?ship->GetRipcordTimeLeft():-1;
                t->alephTime=-1;
                if(auto* warp=FindTarget(ship,c_ttWarp|c_ttNearest|c_ttAllSides)) {
                    Vector delta=warp->GetPosition()-ship->GetPosition();float range=delta.Length();
                    float closing=range>0?ship->GetVelocity()*delta/range:0;
                    if(closing>0)t->alephTime=range/closing;
                }
            }

            float exhaustRate=1;
            if(cameraValid && ticks){float depth=glm::dot(position-cameraState.position,cameraState.forward);float focus=cameraInput.viewportHeight*.5f/std::tan(glm::radians(cameraState.fieldOfView)*.5f);float pixels=depth>0?t->smokeSize*focus/depth:0;exhaustRate=std::clamp(pixels*pixels*.25f,.025f,1.f);}
            for(int tick=0;tick<ticks;++tick) {
                float age=t->emission+(ticks-1-tick)*interval;
                auto previousPosition=position-velocity*(age+interval);
                int sector=cluster->GetObjectID();
                int count=Allegiance::ThrustParticleCount(glm::length(velocity)*interval,t->smokeSize,t->power,fx->emissionLimit());
                t->exhaustPhase+=exhaustRate;bool exhaust=t->exhaustPhase>=1;if(exhaust)t->exhaustPhase-=1;
                if(count && exhaust)for(auto source:t->exhaust) {
                    bool rocket=source.rocket || fx->performance();
                    auto origin=glm::vec3(transform*glm::vec4(source.frame->position,1))+previousPosition-position;
                    for(int i=0;i<count;++i) {
                        float size=t->smokeSize*random(.875f,1.125f),powerSize=size*t->power;
                        glm::vec3 modulation{};
                        if(!rocket)modulation={random(-powerSize,powerSize),random(-powerSize,powerSize),random(-powerSize,powerSize)};
                        float jitter=interval/count*(i+random(0,1));glm::vec3 start,drift;
                        if(rocket) {
                            Allegiance::FireParticle(jitter,origin,velocity,acceleration,t->exhaustDirection,9.f,12.f,t->fireDuration,.2f,powerSize,start,drift,random,direction);
                            fx->emit(EffectKind::Fire,sector,start,drift,size,t->fireDuration,{1,1,1,1},-age);
                        }
                        Allegiance::SmokeParticle(jitter,origin,velocity,rocket?acceleration:glm::vec3(0),t->exhaustDirection,
                            rocket?18.f:12.f,rocket?24.f:15.f,.1f,powerSize,start,drift,random,direction);
                        fx->emit(rocket?EffectKind::Smoke:EffectKind::Booster,sector,start,drift+modulation,size,t->smokeDuration,{1,1,1,1},-age);
                    }
                }
                float until=t->ripcordTime;
                if(until<0 || (t->alephTime>=0 && t->alephTime<until))until=t->alephTime;
                auto teleport=Allegiance::TeleportParticles(until>=0?until+age:-1,t->radius,fx->emissionLimit());
                if(teleport.count) {
                    Vector forward=model->GetOrientation().GetForward(),right=model->GetOrientation().GetRight();
                    if(glm::dot(velocity,velocity)>0){forward=vec(glm::normalize(velocity));right=forward.GetOrthogonalVector();}
                    auto z=vec(forward),x=vec(right),y=vec(CrossProduct(right,forward));
                    for(int i=0;i<teleport.count;++i) {
                        float radius=t->radius*random(.75f,1.25f);auto blast=direction()*radius;
                        auto projected=glm::dot(blast,x)*x+glm::dot(blast,y)*y;
                        glm::vec3 start,drift,acceleration;
                        Allegiance::TeleportParticle(teleport.before,t->radius,random(float(i),i+1.f)*interval/teleport.count,
                            previousPosition,velocity,z,blast,projected,start,drift,acceleration);
                        fx->emit(EffectKind::Spark,sector,start,drift,random(teleport.scale*.15f,teleport.scale*.3f),.5f,{1,1,1,1},-age,acceleration);
                    }
                }
                auto orientation=glm::mat3(matrix(model->GetOrientation(),Vector::GetZero(),1));
                int damageParticles=std::min(std::max(int(glm::length(velocity)*interval),1),fx->emissionLimit());
                for(int d=0;exhaust && d<t->damageCount;++d)for(int i=0;i<damageParticles;++i) {
                    auto& frame=(*t->damageFrames)[d];float size=random(1.5f,2.5f),jitter=interval/damageParticles*(i+random(0,1));glm::vec3 start,drift;
                    Allegiance::FireParticle(jitter,previousPosition+orientation*frame.position,velocity,acceleration,orientation*frame.direction,
                        4.f,6.f,.35f,.05f,size,start,drift,random,direction);
                    fx->emit(EffectKind::Smoke,sector,start,drift,size,.35f,{1,1,1,1},-age,drift*-.333f);
                }
            }
            if(visual.trails && t->trail && t->trailColor.a>0) {
                auto origin=glm::vec3(transform*glm::vec4(t->trail->position,1));
                auto right=vec(model->GetOrientation().GetRight())*t->radius*.25f;
                t->trailTime+=dt;
                if(!t->hadTrail){t->trailPrevious=origin;t->trailRight=right;t->hadTrail=true;}
                if(t->trailTime>=.5f){t->trailTime=std::fmod(t->trailTime,.5f);fx->trail(cluster->GetObjectID(),t->trailPrevious,origin,t->trailRight,right,{glm::vec3(t->trailColor)*.5f,t->trailColor.a});t->trailPrevious=origin;t->trailRight=right;}
            }
            t->previous=position;
        }
    }
    std::span<const ParticleDraw> particles() {
        refreshContacts();
        if(!fx || !viewCluster)return {};
        auto* cluster=viewCluster;fx->begin(cluster->GetObjectID(),cameraState.position,cameraState.forward,visual.trails);
        // Native views remember independent preferences. Keep the old command
        // overlay through the camera transition only when that preference is on.
        bool gridVisible=inCommandView()?showCommandGrid:showGrid || (cameraState.animating && gridWasCommand && showCommandGrid);
        if(gridVisible) {
            struct GridLine {glm::vec3 a,b;bool major;};
            static const auto grid=[](){std::array<GridLine,82> lines{};
                for(int i=0;i<=40;++i){float x,y;Allegiance::CommandGridSpan(i,40,7500,x,y);
                    lines[i*2]={{x,-y,0},{x,y,0},i%5==0};lines[i*2+1]={{-y,x,0},{y,x,0},i%5==0};}return lines;}();
            for(auto& l:grid)fx->line(l.a,l.b,1,0,1,{0,0,(l.major?128.f:64.f)/255,1},true);
            for(auto& c:contacts)if(c.type==OT_ship || c.type==OT_station || c.type==OT_asteroid || c.type==OT_warp || c.selected || c.ordered) {
                glm::vec3 base{c.position.x,c.position.y,0};auto color=c.position.z>0?glm::vec4(200.f/255,180.f/255,20.f/255,1):glm::vec4(150.f/255,100.f/255,0,1);
                fx->line(c.position,base,1,0,1,color,true);
                fx->line(base-glm::vec3(15,0,0),base+glm::vec3(15,0,0),1,0,1,{0,0,128.f/255,1},true);
                fx->line(base-glm::vec3(0,15,0),base+glm::vec3(0,15,0),1,0,1,{0,0,128.f/255,1},true);
            }
        }
        if(!cameraState.animating)gridWasCommand=inCommandView();
        for(auto* t:things)if(t->model->GetCluster()==cluster && ((t->aleph || t->mine)?seen(t->model):eyed(t->model))) {
            auto* model=t->model;
            if(t->hasDecal && model->GetObjectType()==OT_projectile && !t->hasModel){
                fx->sprite(t->decal,vec(model->GetPosition()),t->directional?t->radius:t->radius/std::sqrt(2.f),t->directional?vec(model->GetOrientation().GetForward())*t->radius:glm::vec3(0),t->width,t->age,0,{1,1,1,1},t->decalAngle);continue;
            }
            if(t->mine) {
                float bright=Allegiance::MineBrightness(glm::distance(vec(model->GetPosition()),cameraState.position));
                auto transform=matrix(model->GetOrientation(),model->GetPosition(),1,true);
                for(int i=0;i<t->mine->count;++i)fx->sprite(t->decal,glm::vec3(transform*glm::vec4(t->mine->positions[i],1)),1,{},1,0,0,{bright,bright,bright,1},0,false);
            }
            bool hullVisible=model!=player->GetSourceShip() || cameraState.showPlayer || player->GetTurretID()!=NA;
            if(visual.strobes && hullVisible && t->hasModel && !t->handle.lights.empty()) {
                float depth=glm::dot(vec(model->GetPosition())-cameraState.position,cameraState.forward);
                float screenRadius=depth>0?t->radius*cameraInput.viewportHeight*.5f/std::tan(glm::radians(cameraState.fieldOfView)*.5f)/depth:0;
                if(screenRadius>32) {
                    auto transform=matrix(model->GetOrientation(),model->GetPosition(),t->radius/t->handle.radius,t->flip);
                    for(auto& light:t->handle.lights){float phase=light.phase;
                        float brightness=Allegiance::StrobeBrightness(float(elapsed),light.period,phase,light.rampUp,light.hold,light.rampDown);
                        if(brightness>0)fx->sprite(fx->texture(EffectKind::Glow),glm::vec3(transform*glm::vec4(light.position,1)),.5f*brightness*t->radius/t->handle.radius,{},1,0,0,light.color);
                    }
                }
            }
            if(hullVisible && t->hullHits)for(auto& hit:t->hullHits->entries)if(hit.start>=0 && elapsed-hit.start<=.19) {
                float scale=Allegiance::HullImpactScale(float(elapsed-hit.start));auto center=vec(model->GetPosition())+hit.position;
                fx->plane(fx->texture(EffectKind::HitTangent),center,hit.forward*scale,hit.right*scale);
                // DrawDecal's normal strip uses an orthogonal vector in eye space.
                auto forward=glm::normalize(cameraState.forward),up=glm::normalize(cameraState.up),right=glm::normalize(glm::cross(forward,up));
                Vector eyeNormal(glm::dot(hit.normal,right),glm::dot(hit.normal,up),-glm::dot(hit.normal,forward));
                auto orthogonal=eyeNormal.GetOrthogonalVector();float length=orthogonal.Length();
                if(length>0) {
                    orthogonal*=glm::length(hit.normal)*scale/length;
                    fx->plane(fx->texture(EffectKind::HitNormal),center+hit.normal*scale,hit.normal*scale,
                        right*orthogonal.x+up*orthogonal.y-forward*orthogonal.z);
                }
            }
            if(t->aleph) {
                auto transform=matrix(model->GetOrientation(),model->GetPosition(),t->radius,true);
                auto position=glm::vec3(transform*glm::vec4(0,0,-1,1));
                float scale=t->radius*Allegiance::AlephStarScale(float(t->aleph->bombEnd-elapsed),t->aleph->bombDuration);
                for(float rotation:{.25f,-.375f})fx->sprite(fx->texture(EffectKind::AlephStar),position,scale,{},1,0,0,{1,1,1,1},rotation*float(elapsed));
            }
            if(t->trail && t->hadTrail && t->trailColor.a>0 && t->hasModel) {
                auto transform=matrix(model->GetOrientation(),model->GetPosition(),t->radius/t->handle.radius,t->flip);
                auto origin=glm::vec3(transform*glm::vec4(t->trail->position,1));
                auto right=vec(model->GetOrientation().GetRight())*t->radius*.25f;
                fx->ribbon(t->trailPrevious,origin,t->trailRight,right,{glm::vec3(t->trailColor)*.5f,t->trailColor.a},1,Allegiance::TrailBrightness(t->trailTime));
            }
            if(model->GetObjectType()==OT_ship && (static_cast<IshipIGC*>(model)->GetStateM()&miningMaskIGC)) {
                if(!t->bolt)t->bolt=std::make_unique<Thing::MiningBolt>();auto& bolt=*t->bolt;
                if(elapsed-bolt.time>=1./60 || !bolt.count) {
                    auto forward=model->GetOrientation().GetForward();float radius=.5f*model->GetRadius();
                    auto start=model->GetPosition()+forward*radius,end=start+(100.f+radius)*forward;
                    auto up=forward.GetOrthogonalVector().Normalize(),right=CrossProduct(forward,up).Normalize();
                    float size=.125f*(100.f+radius),focus=1/std::tan(RadiansFromDegrees(cameraState.fieldOfView)*.5f);
                    auto imageRadius=[&](Vector at){float depth=glm::dot(vec(at)-cameraState.position,cameraState.forward);return depth>0?.5f*cameraInput.viewportHeight*focus*size/depth:0.f;};
                    float projected=std::max(imageRadius(start),imageRadius(end));int levels=Allegiance::BoltSubdivision(projected);
                    bolt.count=(1<<levels)+1;bolt.brightness=std::min(1.f,projected/128.f);bolt.width=4.f*cameraInput.viewportHeight/1080.f;
                    Allegiance::SubdivideBolt(vec(start),vec(end),vec(up),vec(right),size,levels,
                        [&](int i){return bolt.points[i];},[&](int i,glm::vec3 p){bolt.points[i]=p;},[&](float a,float b){return fx->random(a,b);});bolt.time=elapsed;
                }
                for(int i=1;i<bolt.count;++i)fx->line(bolt.points[i-1],bolt.points[i],bolt.width,float(i-1)/(bolt.count-1),float(i)/(bolt.count-1),{bolt.brightness,bolt.brightness,bolt.brightness,1});
            } else if(t->bolt)t->bolt->count=0;
            if(t->hasDecal)fx->sprite(t->decal,vec(model->GetPosition()),t->directional?t->radius:t->radius/std::sqrt(2.f),t->directional?vec(model->GetOrientation().GetForward())*t->radius:glm::vec3(0),t->width,t->age,0,{1,1,1,1},t->decalAngle);
            if(hullVisible && t->hasModel && (t->power>0 || t->thrust>0)) {
                auto transform=matrix(model->GetOrientation(),model->GetPosition(),t->radius/t->handle.radius,t->flip);
                float value=t->power>0?1:t->thrust;auto color=HSBColor(std::pow(value,5)/6,1-.5f*std::pow(value,20),1-std::pow(1-value,3));
                for(auto* frame:t->glows)
                    fx->sprite(fx->texture(EffectKind::Glow),glm::vec3(transform*glm::vec4(frame->position,1)),(1-(1-value)*(1-value))*t->radius/t->handle.radius,{},1,t->age,0,{color.GetRed(),color.GetGreen(),color.GetBlue(),1});
            }
        }
        return fx->draws();
    }
    void objectives(const FlightInput& input) {
        if(!onTeam()){state.objective.clear();return;}
        if(training)return;
        if(options.scenario==Scenario::FlightTraining) {
            constexpr const char* goals[]={"Increase throttle and reach 30 mps.","Turn the ship away from its starting heading.","Use the strafe controls for one second.","Engage vector lock while moving.","Select a friendly station and dock. Autopilot can assist."};
            bool done=objectiveStep==0?state.speed>=30:objectiveStep==1?state.forward.z<.7f:objectiveStep==2?strafeTime>=1:
                objectiveStep==3?state.coast && state.speed>5:state.docked;
            if(done)++objectiveStep;
            if(objectiveStep>=5){state.complete=true;state.result="Flight training complete";state.objective.clear();}
            else state.objective=std::string("Flight ")+std::to_string(objectiveStep+1)+"/5: "+goals[objectiveStep];
        } else if(options.scenario==Scenario::WeaponsTraining) {
            constexpr const char* goals[]={"Select the enemy drone.","Fire your guns.","Launch a missile at the drone.","Destroy the training drone."};
            bool done=objectiveStep==0?state.targetEnemy:objectiveStep==1?firedGun:objectiveStep==2?firedMissile:state.kills>0;
            if(done)++objectiveStep;
            if(objectiveStep>=4){state.complete=true;state.result="Weapons training complete";state.objective.clear();}
            else state.objective=std::string("Weapons ")+std::to_string(objectiveStep+1)+"/4: "+goals[objectiveStep];
        } else if(economy()) {
            const auto* p=mission->GetMissionParams();auto* side=player->GetSide();
            int prosperity=p->IsProsperityGame()?side->GetProsperityPercentComplete():0;
            std::array<int,5> nextKey{{side->GetFlags(),side->GetArtifacts(),side->GetKills(),prosperity,p->IsCountdownGame()?int(state.timeRemaining):0}};
            if(nextKey==objectiveKey)return;objectiveKey=nextKey;
            std::string goals;
            auto add=[&](std::string value){if(!goals.empty())goals+=" / ";goals+=value;};
            if(p->IsConquestGame())add("Bases "+std::to_string(p->iGoalConquestPercentage)+"%");
            if(p->IsTerritoryGame())add("Territory "+std::to_string(p->iGoalTerritoryPercentage)+"%");
            if(p->IsFlagsGame())add("Flags "+std::to_string(player->GetSide()->GetFlags())+"/"+std::to_string(p->nGoalFlagsCount));
            if(p->IsArtifactsGame())add("Artifacts "+std::to_string(player->GetSide()->GetArtifacts())+"/"+std::to_string(p->nGoalArtifactsCount));
            if(p->IsDeathMatchGame())add("Kills "+std::to_string(player->GetSide()->GetKills())+"/"+std::to_string(p->nGoalTeamKills));
            if(p->IsProsperityGame())add("Prosperity "+std::to_string(prosperity)+"%");
            if(p->IsCountdownGame())add(std::to_string(int(state.timeRemaining)/60)+":"+(int(state.timeRemaining)%60<10?"0":"")+std::to_string(int(state.timeRemaining)%60));
            state.objective=goals.empty()?"Explore, research and build. F5: research and production.":std::move(goals);
        } else if(options.killLimit>0) {
            state.objective="Team 1: "+std::to_string(state.friendlyKills)+"   Team 2: "+std::to_string(state.enemyKills)+"   Goal: "+std::to_string(options.killLimit);
            if(state.friendlyKills>=options.killLimit || state.enemyKills>=options.killLimit){state.complete=true;state.endgame.winner=state.friendlyKills>=options.killLimit?0:1;state.result=state.friendlyKills>=options.killLimit?"Team 1 wins":"Team 2 wins";}
        }
    }
    float launcherReady(IlauncherIGC* launcher,IexpendableTypeIGC* type)const {
        if(!launcher || launcher->GetMountedFraction()<1)return 0;
        float load=type->GetLoadTime();
        return load>0?std::clamp(Allegiance::LauncherArmingFraction(launcher->GetTimeLoaded()-mission->GetLastUpdate(),load),0.0f,1.0f):1;
    }
    bool knownSector(IclusterIGC* c)const {
        if(!player || !c)return false;
        if(c==player->GetSourceShip()->GetCluster() || (player->GetStation() && c==player->GetStation()->GetCluster()))return true;
        for(auto* l=c->GetModels()->first();l;l=l->next())if(l->data()->SeenBySide(player->GetSide()))return true;
        for(auto& rock:rememberedRocks)if(rock.sector==c->GetObjectID() && (rock.sides&(1u<<player->GetSide()->GetObjectID())))return true;
        return false;
    }
    void updateMapRockKnowledge() {
        int side=player->GetSide()->GetObjectID();
        if(!mapRockKnowledgeDirty && mapRockKnowledgeSide==side)return;
        mapRockSectors.clear();mapRockKnowledgeSide=side;mapRockKnowledgeDirty=false;
        for(auto* l=mission->GetAsteroids()->first();l;l=l->next())if(l->data()->GetCluster() && l->data()->SeenBySide(player->GetSide()))mapRockSectors.push_back(l->data()->GetCluster()->GetObjectID());
        for(auto& rock:rememberedRocks)if(rock.sides&(1u<<side))mapRockSectors.push_back(rock.sector);
        std::sort(mapRockSectors.begin(),mapRockSectors.end());mapRockSectors.erase(std::unique(mapRockSectors.begin(),mapRockSectors.end()),mapRockSectors.end());
    }
    AssetMask sectorAssets(IclusterIGC* c,IsideIGC* viewer,int& friendlyShips,int& enemyShips,int& stations,SectorState* sector=nullptr) {
        friendlyShips=enemyShips=stations=0;AssetMask mask=0;std::array<int,c_cSidesMax> bases{};
        auto* source=player->GetSourceShip();auto* hull=source->GetBaseHullType();bool alliedRip=mission->GetMissionParams()->bAllowAlliedRip;
        float ripSpeed=sector && hull?source->GetHullType()->GetRipcordSpeed():-1;
        auto ripCaps=hull && hull->HasCapability(c_habmCanLtRipcord)?(c_habmIsRipcordTarget|c_habmIsLtRipcordTarget):c_habmIsRipcordTarget;
        if(sector){sector->population.fill(0);sector->ripcordKind=0;sector->restartStation=-1;sector->known=sector->current || (player->GetStation() && c==player->GetStation()->GetCluster()) || std::binary_search(mapRockSectors.begin(),mapRockSectors.end(),c->GetObjectID());}
        for(auto* st=c->GetStations()->first();st;st=st->next())if(st->data()->SeenBySide(viewer)) {
            ++stations;auto* observation=static_cast<Thing*>(st->data()->GetThingSite())->stationObservation(viewer);
            auto* side=observation?mission->GetSide(observation->intel.side):st->data()->GetSide();
            mask|=IsideIGC::AlliedSides(side,viewer)?c_amStation:c_amEnemyStation;
            if(sector){sector->known=true;auto* type=observation?mission->GetStationType(observation->record.stationTypeID):st->data()->GetStationType();
                if(side && type && !type->HasCapability(c_sabmPedestal))++bases[side->GetObjectID()];
                bool allowed=side==viewer || (alliedRip && IsideIGC::AlliedSides(side,viewer));
                if(type && allowed){if(type->HasCapability(c_sabmRipcord))sector->ripcordKind=1;if(side==viewer && type->HasCapability(c_sabmRestart) && sector->restartStation<0)sector->restartStation=st->data()->GetObjectID();}
            }
        }
        for(auto* l=c->GetShips()->first();l;l=l->next()) {
            auto* ship=l->data();if(!ship->SeenBySide(viewer) || !ship->GetVisibleF())continue;
            bool friendly=IsideIGC::AlliedSides(ship->GetSide(),viewer);if(friendly)++friendlyShips;else ++enemyShips;
            if(auto* type=ship->GetBaseHullType()){
                mask|=GetAssetMask(ship,type,friendly);
                if(sector){sector->known=true;if(!ship->GetParentShip() && !ship->IsGhost() && !type->HasCapability(c_habmLifepod))++sector->population[ship->GetSide()->GetObjectID()];
                    if(!sector->ripcordKind && ship!=source && (ship->GetSide()==viewer || (alliedRip && friendly)) && GetRipcordCluster(ship,ripCaps)==c)sector->ripcordKind=2;}
            }
        }
        for(auto* l=c->GetProbes()->first();l;l=l->next()) {
            auto* probe=l->data();if(!probe->SeenBySide(viewer))continue;bool friendly=IsideIGC::AlliedSides(probe->GetSide(),viewer);
            if(!friendly && probe->GetProbeType()->HasCapability(c_eabmRipcord))mask|=c_amEnemyTeleport;
            if(sector){sector->known=true;if((probe->GetSide()==viewer || (alliedRip && friendly)) && probe->GetCanRipcord(ripSpeed))sector->ripcordKind=1;}
        }
        if(sector){auto owners=Allegiance::SectorOwners(bases);sector->owner=owners.first;sector->secondaryOwner=owners.second;}
        return mask;
    }
    bool contactsDirty{};
    void refreshContacts() {
        if(!contactsDirty)return;
        contactsDirty=false;contacts.clear();if(!player)return;
        auto* cluster=mission->GetCluster(state.sectorId);auto* source=player->GetSourceShip();
        bool historicalTarget=target && targetRock>=0 && target->GetObjectType()==OT_buoy &&
            cluster && targetRockSector==cluster->GetObjectID();
        if(cluster)for(auto* l=cluster->GetModels()->first();l;l=l->next()) {
            auto* m=l->data();auto type=m->GetObjectType();
            if(m==source || type==OT_projectile || type==OT_chaff || type==OT_buildingEffect || !seen(m))continue;
            auto intel=static_cast<Thing*>(m->GetThingSite())->knownIntel(player->GetSide());
            Contact c{vec(m->GetPosition()),m->GetRadius(),intel.side!=NA && !IsideIGC::AlliedSides(mission->GetSide(intel.side),player->GetSide()),m->GetObjectType(),m->GetObjectID(),GetModelName(m)};c.side=intel.side;
            c.hull=intel.hull;c.shield=intel.shield;
            if(auto* o=static_cast<Thing*>(m->GetThingSite())->stationObservation(player->GetSide())){c.name=o->name;c.icon=o->icon;c.radius=o->radius;}
            else if(auto* icon=static_cast<Icon*>(m->GetIcon()))c.icon=icon->name;
            c.selected=m==target;c.neutral=intel.side==NA;c.own=intel.side==player->GetSide()->GetObjectID();c.ally=!c.own && !c.neutral && !c.enemy;
            c.ordered=m==player->GetCommandTarget(c_cmdAccepted) || m==player->GetCommandTarget(c_cmdQueued);
            c.objective=m->GetFlag()!=NA;c.artifact=m->GetFlag()==SIDE_TEAMLOBBY;
            Allegiance::RadarModel detail{m,source,m->GetSide(),player->GetSide()};
            if(m->GetObjectType()==OT_station) {
                auto* observed=static_cast<Thing*>(m->GetThingSite())->stationObservation(player->GetSide());
                // Capabilities must come from observed station type after an unseen upgrade.
                c.pedestalFlag=observed?observed->pedestal:detail.pedestal();
            }
            if(m->GetObjectType()==OT_treasure){c.treasureUse=detail.usefulTreasure();c.objective=static_cast<ItreasureIGC*>(m)->GetTreasureCode()==c_tcFlag;}
            if(m->GetObjectType()==OT_missile)c.missileThreat=detail.missileTargetsOwnSide();
            if(m->GetObjectType()==OT_asteroid)c.plainRock=detail.plainAsteroid();
            if(m->GetObjectType()==OT_ship) {
                auto* ship=static_cast<IshipIGC*>(m);c.hull=ship->GetFraction();c.commandSelected=m->GetObjectID()>=0 && commandSelected.test(m->GetObjectID());
                if(auto* sh=static_cast<IshieldIGC*>(ship->GetMountedPart(ET_Shield,0)))c.shield=sh->GetFraction();

            }
            for(auto* l=m->GetDamageBuckets()->first();l;l=l->next())if(l->data()->track()==source->GetDamageTrack()) {
                c.threat=true;c.threatFlash=l->data()->flash(mission->GetLastUpdate());break;
            }
            contacts.push_back(c);
        }
        if(cluster)for(auto& rock:rememberedRocks)if(rock.sector==cluster->GetObjectID() && (rock.sides&(1u<<player->GetSide()->GetObjectID()))) {
            Contact c{rock.position,rock.radius,false,OT_asteroid,rock.id,rock.name};c.neutral=true;c.icon=rock.icon;c.plainRock=rock.name.empty() || rock.name.front()=='A';c.selected=historicalTarget && rock.id==targetRock && rock.sector==targetRockSector;contacts.push_back(c);
        }
    }
#include "inventory_game.inl"
    void refresh() {
        state.missionStage=mission->GetMissionStage();
        state.maxImbalance=mission->GetMissionParams()->iMaxImbalance;
        state.alliances=options.alliances;
        state.allowDefections=mission->GetMissionParams()->bAllowDefections;
        state.alliedSight=mission->GetMissionParams()->bAllowAlliedViz;
        state.alliedRipcord=mission->GetMissionParams()->bAllowAlliedRip;
        state.developments=mission->GetMissionParams()->bAllowDevelopments;
        if(player)state.steering={player->GetControls().jsValues[c_axisYaw],player->GetControls().jsValues[c_axisPitch]};
        if(training) {
            state.trainingMission=options.trainingMission;state.trainingOverlay=training->overlay;state.trainingRadar=training->radar;
            state.trainingPanes=training->overlays;state.trainingPaused=training->paused;state.trainingCommand=training->view==TrainingHost::vmCommand;
            if(training->finished){
                state.complete=true;state.trainingPostSlideshow=!training->skipSlideshow;
                // The original training destructor shows the debrief after both
                // goal completion and failure. Slideshow policy is not victory.
                state.result=player->GetCluster() || player->GetStation()?"Training ended":"Training ended: ship destroyed";
            }
        }
        if(state.complete && !training && mission->GetMissionStage()==STAGE_STARTED)mission->SetMissionStage(STAGE_OVER);
        freezeScores();contacts.clear();if(!player)return;
        refreshBallot();
        state.cinematic=cinematicStop>=0;state.cinematicDeath=state.cinematic && cinematicDeath;
        state.ghost=player->IsGhost();state.lives=mission->GetMissionParams()->iLives-player->GetDeaths();
        state.teamScores.resize(mission->GetSides()->n());size_t sideIndex=0;
        for(auto* l=mission->GetSides()->first();l;l=l->next()) {
            auto* side=l->data();auto& score=state.teamScores[sideIndex++];score.name=side->GetName();score.active=side->GetActiveF();
            score.civilization=side->GetCivilization()->GetObjectID();score.civilizationIcon=side->GetCivilization()->GetIconName();
            auto color=side->GetColor();score.color={color.R(),color.G(),color.B(),1};
            score.kills=side->GetKills();score.deaths=side->GetDeaths();score.flags=side->GetFlags();score.artifacts=side->GetArtifacts();score.sectors=side->GetTerritoryCount();score.prosperity=mission->GetMissionParams()->IsProsperityGame()?side->GetProsperityPercentComplete():0;
        }
        state.timeRemaining=std::max(0.f,mission->GetMissionParams()->GetCountDownTime()-float(elapsed));
        state.killCounter=mission->GetMissionParams()->IsDeathMatchGame();
        if(!onTeam()) {
            state.endurance=state.cloakingPercent=state.stationHull=state.stationShield=0;state.carriedFlag=state.targetSide=-1;state.stationSector.clear();state.targetSector.clear();
            // The lobby side is deliberately absent from faction arrays and
            // scanner masks. A departed pilot retains only public match data.
            state.shipId=player->GetObjectID();state.kills=player->GetKills();state.deaths=player->GetDeaths();state.ejections=player->GetEjections();state.wing=NA;
            state.playerSide=player->GetSide()?player->GetSide()->GetObjectID():NA;refreshBriefing(state.playerSide);
            state.ship.clear();state.sourcePilot.clear();state.station.clear();state.stationId=NA;state.sector.clear();state.sectorId=NA;
            state.networked=gameRole==GameRole::Replica;state.economy=economy();state.sideActive=false;state.commander=false;
            state.dead=true;state.docked=state.passenger=state.commandView=state.cinematic=state.cinematicDeath=false;
            state.coast=state.autopilot=state.ripcord=state.cloaked=state.detected=state.pod=false;
            state.hull=state.shield=state.energy=state.fuel=state.speed=state.throttle=state.signature=state.experience=0;
            state.ammo=state.missiles=state.fixedWeapons=0;state.money=state.minedCredits=0;
            state.ammoFraction=state.missileLock=state.missileReady=state.ripcordLeft=0;
            state.selectedWeapon=state.selectedCargo=NA;
            state.position=state.cockpit=state.velocity={};state.stars={};state.steering={};
            state.target.clear();state.targetType.clear();state.order.clear();state.objective.clear();state.carriedObjective.clear();state.civilization.clear();
            state.targetId=state.targetObjectType=state.parentId=state.turret=state.commandUnit=NA;
            state.hasLead=state.targetHasModel=state.targetHasShield=false;state.commandUnitName.clear();state.commandUnitCount=0;
            state.equipment.clear();state.inventory.clear();state.hullChoices.clear();state.research.clear();state.units.clear();state.pilots.clear();state.sectors.clear();state.sectorLinks.clear();state.moneyRequest={};
            for(auto& command:state.commands)command={};
            return;
        }
        auto flag=player->GetSourceShip()->GetFlag();auto sourceId=player->GetSourceShip()->GetObjectID();
        state.carriedFlag=flag;
        if(flag!=displayedFlag || sourceId!=displayedFlagSource) {
            displayedFlag=flag;displayedFlagSource=sourceId;state.carriedObjective.clear();
            if(flag==SIDE_TEAMLOBBY)state.carriedObjective="Carrying an artifact / deliver to your base";
            else if(flag>=0)if(auto* owner=mission->GetSide(flag))state.carriedObjective=std::string("Carrying ")+owner->GetName()+"'s flag / deliver to your base";
        }
        auto* source=player->GetSourceShip();
        selectedCargo=Allegiance::CanonicalCargoMount(source,selectedCargo);state.selectedCargo=selectedCargo;
        state.passenger=source!=player;state.parentId=state.passenger?source->GetObjectID():NA;state.turret=player->GetTurretID();state.sourcePilot=source->GetName();
        state.sideActive=!economy() || player->GetSide()->GetActiveF();state.networked=gameRole==GameRole::Replica;state.economy=economy();state.money=economy()?readBalance(player):funds[player->GetSide()->GetObjectID()];state.civilization=player->GetSide()->GetCivilization()->GetName();
        // Identity and station membership remain valid after the original
        // Reset(false). Do not retain a dead pilot's former hangar/equipment.
        state.shipId=player->GetObjectID();state.kills=player->GetKills();state.deaths=player->GetDeaths();state.ejections=player->GetEjections();
        state.docked=player->GetStation()!=nullptr;state.station=state.docked?player->GetStation()->GetName():"";state.stationId=state.docked?player->GetStation()->GetObjectID():NA;
        state.endurance=player->GetEndurance();state.cloakingPercent=100*(1-source->GetCloaking());
        state.stationSector.clear();state.stationHull=state.stationShield=0;
        if(auto* station=player->GetStation()) {
            state.stationSector=station->GetCluster()->GetName();state.stationHull=station->GetFraction();state.stationShield=station->GetShieldFraction();
            auto color=station->GetSide()->GetColor();state.stationColor={color.R(),color.G(),color.B(),1};
        }
        if(state.dead) {
            state.equipment.clear();state.ammo=state.missiles=state.fixedWeapons=0;
            state.ripcord=state.autopilot=state.coast=state.cloaked=false;state.ripcordLeft=0;
        }
        if(!state.dead) {
            state.position=vec(source->GetPosition());state.cockpit=vec(source->GetPosition()+(state.turret==NA?source->GetHullType()->GetCockpit():source->GetHullType()->GetWeaponPosition(state.turret))*source->GetOrientation());state.velocity=vec(source->GetVelocity());auto& attitude=(state.turret==NA?source:static_cast<IshipIGC*>(player))->GetOrientation();state.forward=vec(attitude.GetForward());state.up=vec(attitude.GetUp());state.right=vec(attitude.GetRight());
            state.pod=!state.passenger && source->GetBaseHullType()->HasCapability(c_habmLifepod);
            state.ripcord=source->fRipcordActive();state.ripcordLeft=state.ripcord?source->GetRipcordTimeLeft():0;
            state.coast=(source->GetStateM()&coastButtonIGC)!=0;state.autopilot=source->GetAutopilot();state.cloaked=source->GetCloaking()<1;
            state.throttle=std::clamp((player->GetControls().jsValues[c_axisThrottle]+1)*0.5f,0.0f,1.0f);
            state.signature=source->GetSignature()*100/source->GetSide()->GetGlobalAttributeSet().GetAttribute(c_gaSignature);state.experience=100*(player->GetExperienceMultiplier()-1);state.selectedWeapon=state.turret==NA?(groupFire?-1:selectedWeapon):state.turret;
            state.radius=source->GetRadius();state.hull=source->GetFraction();state.speed=source->GetVelocity().Length();state.ship=source->GetHullType()->GetName();state.ammo=source->GetAmmo();
            auto* h=source->GetHullType();state.fixedWeapons=state.passenger?0:h->GetMaxFixedWeapons();state.ammoFraction=h->GetMaxAmmo()>0?float(state.ammo)/h->GetMaxAmmo():0;state.fuel=h->GetMaxFuel()>0?source->GetFuel()/h->GetMaxFuel():0;state.energy=h->GetMaxEnergy()>0?source->GetEnergy()/h->GetMaxEnergy():0;
            auto* shield=static_cast<IshieldIGC*>(source->GetMountedPart(ET_Shield,0));state.shield=shield?shield->GetFraction():0;
            auto* magazine=static_cast<ImagazineIGC*>(source->GetMountedPart(ET_Magazine,0));state.missiles=magazine?magazine->GetAmount():0;
            state.missileLock=magazine?magazine->GetLock():0;state.missileReady=magazine?launcherReady(magazine,magazine->GetMissileType()):0;
            state.equipment.resize(source->GetParts()->n());size_t index=0;
            for(auto* l=source->GetParts()->first();l;l=l->next()) {
                auto* part=l->data();auto& e=state.equipment[index++];auto type=part->GetEquipmentType();
                e.kind=static_cast<PartKind>(type);e.mount=part->GetMountID();e.name=part->GetPartType()->GetName();
                e.amount=part->GetAmount();e.mounted=part->GetMountedFraction();e.active=part->fActive();
                e.fixedWeapon=type==ET_Weapon && e.mount>=0 && e.mount<h->GetMaxFixedWeapons();
                e.selected=state.passenger?(e.kind==PartKind::Weapon && e.mount==state.turret):e.mount<0?e.mount==selectedCargo:e.fixedWeapon && (groupFire || e.mount==selectedWeapon);e.ready=1;e.value=1;e.empty=false;
                if(type==ET_Magazine){e.ready=state.missileReady;e.value=state.missileLock;e.empty=!e.amount;}
                if(type==ET_ChaffLauncher || type==ET_Dispenser){auto* d=static_cast<IdispenserIGC*>(part);e.ready=launcherReady(d,d->GetExpendableType());e.empty=!e.amount;}
                e.readiness=Allegiance::EquipmentReadyState(part,[&](IlauncherIGC*){return e.ready;});
                if(type==ET_Weapon){auto* w=static_cast<IweaponIGC*>(part);e.empty=w->GetAmmoPerShot()>0 && state.ammo==0;}
                if(type==ET_Shield)e.value=state.shield;
                if(type==ET_Afterburner){e.value=state.fuel;e.empty=source->GetFuel()<=0;}
                if(type==ET_Cloak)e.value=1-source->GetCloaking();
            }
            if(player->GetStation() && !state.passenger) {
                constexpr const char* names[]={"countermeasure","weapon","missile","dispenser","shield","cloak","pack","afterburner"};
                for(EquipmentType type=0;type<=ET_Afterburner;++type)if(type!=ET_Pack) {
                    int count=type==ET_Weapon?h->GetMaxWeapons():1;
                    for(Mount mount=0;mount<count;++mount)if(h->GetPartMask(type,mount) && !source->GetMountedPart(type,mount)) {
                        EquipmentState empty{};empty.kind=PartKind(type);empty.mount=mount;empty.name=std::string("Empty ")+names[type]+" slot";
                        empty.empty=true;state.equipment.push_back(std::move(empty));
                    }
                }
            }
            std::sort(state.equipment.begin(),state.equipment.end(),[](auto& a,auto& b){
                // Fixed guns first, then missile/other equipment, then cargo.
                int ka=a.mount<0?100-int(a.mount):a.kind==PartKind::Weapon?int(a.mount):10+int(a.kind);
                int kb=b.mount<0?100-int(b.mount):b.kind==PartKind::Weapon?int(b.mount):10+int(b.kind);return ka<kb;
            });
        }
        refreshInventory();
        state.commandView=inCommandView();state.commandTopDown=training && state.commandView?training->camera==TrainingHost::cmExternalCommandViewTD:commandTopDown;state.commandOrder=commandOrder;state.commandUnit=commandUnit;state.commandUnitCount=int(commandUnits.size());
        auto* selected=mission->GetShip(ShipID(commandUnit));state.commandUnitName=selected?selected->GetName():"";
        if(!selected)state.commandUnit=-1;
        auto* cluster=player->GetCluster();if(!cluster && player->GetStation())cluster=player->GetStation()->GetCluster();
        if(cinematicStop>=0 && cinematicCluster)cluster=cinematicCluster;
        else if(training && training->viewCluster)cluster=training->viewCluster;else if(commandViewEnabled && commandCluster)cluster=commandCluster;
        if(cluster)viewCluster=cluster;
        state.sectorId=cluster?cluster->GetObjectID():-1;state.stars={};
        if(cluster) {
            if(visual.stars)state.stars=static_cast<SectorSite*>(cluster->GetClusterSite())->stars;
            auto primary=cluster->GetLightColor(),alternate=cluster->GetLightColorAlt();
            state.lighting={glm::vec4(vec(cluster->GetLightDirection()),visual.bidirectional?cluster->GetBidirectionalAmbientLevel():cluster->GetAmbientLevel()),
                {primary.GetRed(),primary.GetGreen(),primary.GetBlue(),0},visual.bidirectional?glm::vec4(alternate.GetRed(),alternate.GetGreen(),alternate.GetBlue(),0):glm::vec4(0)};
        }
        state.sector=cluster?cluster->GetName():"";state.target.clear();state.targetType.clear();state.targetSector.clear();state.targetColor={.5f,.5f,.5f,1};state.targetSide=-1;state.targetId=state.targetObjectType=-1;state.hasLead=false;state.targetHasModel=false;state.targetHasShield=false;state.targetShield=0;state.targetExperience=0;state.detected=false;state.order.clear();
        if(target && target->GetCluster()==cluster && seen(target)) {
            auto* t=static_cast<ImodelIGC*>(target);state.target=GetModelName(t);state.targetId=t->GetObjectID();state.targetObjectType=t->GetObjectType();state.targetPosition=vec(t->GetPosition());
            state.targetDistance=glm::distance(state.position,state.targetPosition);state.targetSpeed=t->GetVelocity().Length();
            auto* thing=static_cast<Thing*>(t->GetThingSite());auto intel=thing->knownIntel(player->GetSide());
            state.targetSector=t->GetCluster()->GetName();state.targetSide=intel.side;if(auto* side=mission->GetSide(intel.side)){auto color=side->GetColor();state.targetColor={color.R(),color.G(),color.B(),1};}
            state.targetEnemy=intel.side!=NA && !IsideIGC::AlliedSides(mission->GetSide(intel.side),player->GetSide());state.targetHull=intel.hull;
            if(t->GetObjectType()==OT_ship) {
                auto* ship=static_cast<IshipIGC*>(t);state.targetHull=ship->GetFraction();state.targetType=ship->GetHullType()->GetName();
                auto* shield=static_cast<IshieldIGC*>(ship->GetMountedPart(ET_Shield,0));state.targetHasShield=shield!=nullptr;
                state.targetShield=shield?shield->GetFraction():0;state.targetExperience=100*(ship->GetExperienceMultiplier()-1);
            } else if(t->GetObjectType()==OT_station) {
                auto* station=static_cast<IstationIGC*>(t);state.targetShield=intel.shield;
                state.targetHasShield=true;state.targetType=thing->stationObservation(player->GetSide())->type;state.target=thing->stationObservation(player->GetSide())->name;
            }
            if(thing && thing->hasModel) {
                auto* o=thing->stationObservation(player->GetSide());state.targetHasModel=o?o->hasModel:true;
                state.targetModel=o?o->handle:thing->handle;state.targetOrientation=matrix(t->GetOrientation(),Vector::GetZero(),1,o?o->flip:thing->flip);
                state.targetAppearance={state.targetModel.id,glm::mat4(1),o?o->color:thing->color,o?o->unlit:thing->unlit};
                if(thing->aleph){state.targetAppearance.aleph=true;state.targetAppearance.blend=ModelBlend::Additive;state.targetAppearance.textureOffset=Allegiance::AlephTextureOffset(float(elapsed));}
            }

        }
        for(Command slot=c_cmdQueued;slot<=c_cmdCurrent;++slot) {
            auto& status=state.commands[slot];auto* model=player->GetCommandTarget(slot);auto command=player->GetCommandID(slot);
            int id=model?model->GetObjectID():-1,type=model?model->GetObjectType():-1;
            std::string_view name;if(model){auto* observed=static_cast<Thing*>(model->GetThingSite())->stationObservation(player->GetSide());name=observed?std::string_view(observed->name):std::string_view(GetModelName(model));}
            if(status.command!=command || status.targetId!=id || status.targetType!=type || commandDisplayNames[slot]!=name) {
                status.command=command;status.targetId=id;status.targetType=type;commandDisplayNames[slot]=name;status.text.clear();
                if(command>=0 && command<c_cidMax){status.text=c_cdAllCommands[command].szVerb;if(model){status.text+=' ';status.text+=name;}}
            }
        }
        if(state.autopilot) {
            auto* plan=player->GetCommandTarget(c_cmdPlan);
            if(plan){auto* observed=static_cast<Thing*>(plan->GetThingSite())->stationObservation(player->GetSide());state.order=commandName(player->GetCommandID(c_cmdPlan))+" / "+(observed?observed->name:GetModelName(plan));}
        }
        bool historicalTarget=false;
        if(target && targetRock>=0 && target->GetObjectType()==OT_buoy && cluster && targetRockSector==cluster->GetObjectID())
            for(auto& rock:rememberedRocks)if(rock.id==targetRock && rock.sector==targetRockSector && (rock.sides&(1u<<player->GetSide()->GetObjectID()))) {
                historicalTarget=true;state.target=rock.name;state.targetType=rock.type;state.targetId=rock.id;state.targetObjectType=OT_asteroid;
                state.targetPosition=rock.position;state.targetDistance=glm::distance(state.position,rock.position);state.targetEnemy=false;
                state.targetSpeed=0;state.targetHull=1;state.targetHasShield=false;state.targetHasModel=true;state.targetModel=rock.handle;
                state.targetOrientation=rock.draw.transform;state.targetOrientation[3]={0,0,0,1};
                state.targetOrientation=glm::scale(state.targetOrientation,glm::vec3(rock.handle.radius/rock.radius));
                state.targetAppearance=rock.draw;state.targetAppearance.transform=glm::mat4(1);
                state.order=state.autopilot?"GO TO "+rock.name:"";break;
            }
        updateWeaponIndicator();
        // Authority decisions/visibility use IGC directly. Build radar labels
        // only when a view actually requests them, including diagnostics.
        contactsDirty=true;
        if(gameRole!=GameRole::Authority)refreshContacts();
        if(!state.dead && !state.docked) {
            if(gameRole==GameRole::Replica && onTeam()) {
                const auto& records=shipIntel[player->GetSide()->GetObjectID()];auto status=records.find(player->GetObjectID());
                state.detected=status!=records.end() && (status->second.status.flags&Allegiance::PlayerShipStatus::Detected);
            }else for(auto* l=mission->GetSides()->first();l;l=l->next())
                if(!IsideIGC::AlliedSides(player->GetSide(),l->data()))state.detected|=source->GetCurrentEye(l->data());
        }
        if(training && training->detected)state.detected=*training->detected;
        state.sectors.resize(mission->GetClusters()->n());state.sectorLinks.clear();size_t n=0;
        if(gameRole!=GameRole::Authority)updateMapRockKnowledge();
        state.playerSide=player->GetSide()->GetObjectID();refreshBriefing(state.playerSide);if(highlightSide!=state.playerSide){dismissedSectorHighlights.clear();highlightSide=state.playerSide;}for(auto* l=mission->GetSides()->first();l;l=l->next()){auto color=l->data()->GetColor();state.sideColors[l->data()->GetObjectID()]={color.R(),color.G(),color.B(),1};}
        auto* ripModel=source->GetRipcordModel();state.ripcordSector=ripModel && ripModel->GetCluster()?ripModel->GetCluster()->GetObjectID():-1;state.ripcordShip=ripModel && ripModel->GetObjectType()==OT_ship;
        for(auto* l=mission->GetClusters()->first();l;l=l->next()) {
            auto* c=l->data();auto& s=state.sectors[n++];s.id=c->GetObjectID();s.name=c->GetName();s.position={c->GetScreenX(),c->GetScreenY()};
            s.current=c==cluster;auto mask=sectorAssets(c,player->GetSide(),s.friendlyShips,s.enemyShips,s.stations,gameRole==GameRole::Authority?nullptr:&s);
            auto* site=static_cast<SectorSite*>(c->GetClusterSite());site->SetClusterAssetMask(mask);
            if(gameRole!=GameRole::Authority)site->updateWarnings(c);
            s.warning=GetClusterWarning(mask,mission->GetMissionParams()->bInvulnerableStations);
            s.accepted=player->GetCommandTarget(c_cmdAccepted) && player->GetCommandTarget(c_cmdAccepted)->GetCluster()==c;
            s.queued=player->GetCommandTarget(c_cmdQueued) && player->GetCommandTarget(c_cmdQueued)->GetCluster()==c;s.highlight=!dismissedSectorHighlights.contains(s.id) && (c->GetHighlight() || sectorHighlights[player->GetSide()->GetObjectID()].contains(s.id));
            for(auto* w=c->GetWarps()->first();w;w=w->next())if(w->data()->SeenBySide(player->GetSide()))if(auto* dest=w->data()->GetDestination()) {
                s.known=true;int id=dest->GetCluster()->GetObjectID();if(s.id<id)state.sectorLinks.push_back({s.id,id});
            }
        }
        if(gameRole!=GameRole::Authority)for(auto* l=mission->GetShips()->first();l;l=l->next())if(auto* station=l->data()->GetStation()) {
            auto* ship=l->data();auto* hull=ship->GetBaseHullType();if(!hull || ship->GetParentShip() || ship->IsGhost() || hull->HasCapability(c_habmLifepod) || !ship->SeenBySide(player->GetSide()))continue;
            auto sector=std::find_if(state.sectors.begin(),state.sectors.end(),[&](const auto& s){return s.id==station->GetCluster()->GetObjectID();});if(sector!=state.sectors.end())++sector->population[ship->GetSide()->GetObjectID()];
        }
    }
    void selectCommandSector(IclusterIGC* cluster) {
        commandCluster=cluster;if(training)training->viewCluster=cluster;
        recenterCommand(0);refresh();
    }
    const std::vector<DrawItem>& scene(bool thirdPerson) {
        if(!onTeam()){draws.clear();return draws;}
        draws.clear();auto* cluster=player?player->GetCluster():nullptr;
        if(cinematicStop>=0 && cinematicCluster)cluster=cinematicCluster;
        else if(training && training->viewCluster)cluster=training->viewCluster;else if(commandViewEnabled && commandCluster)cluster=commandCluster;
        if(!cluster)cluster=viewCluster;
        if(cluster)for(auto item:static_cast<SectorSite*>(cluster->GetClusterSite())->backgrounds) {
            if((item.background==1 && !visual.environment) || (item.background==2 && !visual.posters))continue;
            item.transform[3]+=glm::vec4(cameraState.position,0);draws.push_back(item);
        }
        for(auto* thing:modelThings) {
            auto* m=thing->model;if(thing->replicaStrategic || !thing->hasModel || m->GetCluster()!=cluster || !seen(m) || (m==player->GetSourceShip() && ((cinematicStop>=0 && cinematicDeath) || (!thirdPerson && player->GetTurretID()==NA))))continue;
            if(thing->shieldHits)for(auto& hit:*thing->shieldHits)if(hit.start>=0 && elapsed-hit.start<.75) {
                const auto& texture=fx->texture(EffectKind::Shield);int frame=int((elapsed-hit.start)*texture.frames)%texture.frames;
                float width=float(texture.width)/texture.frames;
                auto transform=matrix(m->GetOrientation(),m->GetPosition(),m->GetRadius())*hit.orientation;
                draws.push_back({shieldModels[hit.kind].id,transform,{1,1,1,1},true,ModelBlend::Additive,
                    (frame*width+.5f)/texture.width,(width-1)/texture.width});
            }
            if(thing->aleph) {
                float depth=glm::dot(vec(m->GetPosition())-cameraState.position,cameraState.forward);
                float projected=depth>0?.5f*cameraInput.viewportHeight*thing->radius/(depth*std::tan(RadiansFromDegrees(cameraState.fieldOfView)*.5f)):0;
                auto& level=thing->aleph->levels[Allegiance::AlephDetail(projected)];
                draws.push_back({level.id,matrix(m->GetOrientation(),m->GetPosition(),thing->radius,true),{1,1,1,1},true,
                    ModelBlend::Additive,Allegiance::AlephTextureOffset(float(elapsed)),1,true});
            } else if(thing->building) {
                auto base=glm::translate(glm::mat4(1),vec(m->GetPosition()));float radius=m->GetRadius();auto motion=Allegiance::BuildShellAnimation(float(elapsed));
                auto inner=glm::scale(base,glm::vec3(radius*motion.innerScale));inner=glm::rotate(inner,motion.innerZ,glm::vec3(0,0,1));inner=glm::rotate(inner,motion.innerY,glm::vec3(0,1,0));
                auto outer=glm::scale(base,glm::vec3(radius));outer=glm::rotate(outer,motion.outerX,glm::vec3(1,0,0));outer=glm::rotate(outer,motion.outerY,glm::vec3(0,1,0));
                draws.push_back({thing->handle.id,inner,thing->innerColor,true,ModelBlend::Alpha});
                draws.push_back({thing->handle.id,outer,{glm::vec3(thing->color)*thing->outerIntensity,thing->color.a},true,ModelBlend::Additive});
            } else if(auto* o=thing->stationObservation(player->GetSide())) {
                if(o->hasModel)draws.push_back({o->handle.id,matrix(m->GetOrientation(),m->GetPosition(),o->radius/o->handle.radius,o->flip),o->color,o->unlit});
            } else draws.push_back({thing->handle.id,matrix(m->GetOrientation(),m->GetPosition(),thing->radius/thing->handle.radius,thing->flip),thing->color,thing->unlit});
        }
        if(cluster)for(auto& rock:rememberedRocks)if(rock.sector==cluster->GetObjectID() && (rock.sides&(1u<<player->GetSide()->GetObjectID())))draws.push_back(rock.draw);
        return draws;
    }
};
namespace {
std::vector<CivilizationChoice> missionCivilizations(ImissionIGC* mission,float* winMoney){
    if(winMoney)*winMoney=mission->GetFloatConstant(c_fcidWinTheGameMoney);
    std::vector<CivilizationChoice> result;result.reserve(mission->GetCivilizations()->n());
    for(auto* l=mission->GetCivilizations()->first();l;l=l->next())result.push_back({l->data()->GetObjectID(),l->data()->GetName(),l->data()->GetIconName()});return result;
}
}
std::vector<CivilizationChoice> Game::civilizations(float* winMoney)const{return missionCivilizations(impl_->mission,winMoney);}
std::vector<CivilizationChoice> Game::civilizations(const std::filesystem::path& art,float* winMoney,std::string_view core) {
    UTL::SetArtPath(art.string().c_str());TRef<IIgcSite> site=new IIgcSite;auto* mission=CreateMission();mission->Initialize(Time(0u),site);
    std::vector<CivilizationChoice> result;
    try{if(core.empty() || !Allegiance::ValidMapFile(core))throw std::runtime_error("Invalid civilization core");std::string file(core);if(!LoadIGCStaticCore(file.c_str(),mission,false))throw std::runtime_error("Cannot load civilization catalog");
        result=missionCivilizations(mission,winMoney);
    }catch(...){mission->Terminate();delete mission;throw;}
    mission->Terminate();delete mission;return result;
}
Game::Game(const std::filesystem::path& artwork,ModelLoader loader,uint32_t effect,uint32_t warp,SessionOptions options,EffectLoader effects,GameRole role):impl_(std::make_unique<Impl>(artwork,std::move(loader),effect,warp,std::move(options),std::move(effects),role)){}
std::span<const Allegiance::CustomMapInfo> Game::customMaps()const{return impl_->customMapCatalog;}
std::string Game::configurationError(const SessionOptions& options,bool fromClient)const{
    if(auto error=missionError(options,impl_->customMapCatalog,fromClient);!error.empty())return error;
    return options.customMap.empty()?std::string{}:mapPreview(options,impl_->artworkPath).error;
}
std::shared_ptr<const Allegiance::MissionBriefing> Game::missionBriefing(int side)const{return impl_->briefingForSide(side);}
void Game::missionBriefing(uint32_t sides,std::shared_ptr<const Allegiance::MissionBriefing> briefing){
    auto& g=*impl_;for(int side=0;side<std::min(g.options.rules.teams,6);++side)if(sides&(1u<<side))g.briefings[side]=briefing;
    if(g.state.playerSide>=0 && (sides&(1u<<g.state.playerSide)))g.state.briefing=std::move(briefing);
}
Game::~Game() {
    // Authored training action destructors call back through TrainingHost into
    // this Game. Destroy them while impl_ still points to the live core: libc++
    // unique_ptr clears its stored pointer before invoking Impl's destructor.
    if(impl_)impl_->training.reset();
}
void Game::retainMessages(std::deque<GameMessage> history){auto& g=*impl_;while(history.size()>Allegiance::ChatHistoryCapacity)history.pop_front();g.messageSerial=history.empty()?0:history.back().serial;g.state.messages=std::move(history);}
ImissionIGC* Game::core()const{return impl_->mission;}
IshipIGC* Game::playerShip()const{return impl_->player;}
void Game::step(const FlightInput& input,double dt){if(impl_->gameRole==GameRole::Replica){predict(input,dt);return;}Allegiance::SimulationRandomScope randomScope(impl_->randomState);impl_->step(input,dt);if(std::exchange(impl_->dockRestorationPending,false))restoreDockedLoadout();}
Feedback Game::takeFeedback(){auto result=impl_->feedback;impl_->feedback={};return result;}
void Game::cycleWeapon(bool previous){auto& g=*impl_;if(g.cycleWeapon(previous))g.submit(Net::Operation::Weapon,{g.selectedWeapon,int(g.groupFire)});}
void Game::nextTarget(){impl_->nextTarget();impl_->submit(Net::Operation::Target,{impl_->state.targetObjectType,impl_->state.targetId});}
bool Game::targetKey(short key){Allegiance::SimulationRandomScope randomScope(impl_->randomState);auto accepted=impl_->targetKey(key);if(accepted)impl_->submit(Net::Operation::Target,{impl_->state.targetObjectType,impl_->state.targetId});return accepted;}
void Game::selectTarget(int type,int id){Allegiance::SimulationRandomScope randomScope(impl_->randomState);impl_->selectTarget(type,id);impl_->submit(Net::Operation::Target,{type,id});}
void Game::selectWeapon(int mount){auto& g=*impl_;if(g.selectWeapon(mount))g.submit(Net::Operation::Weapon,{g.selectedWeapon,int(g.groupFire)});}
void Game::selectCargo(int mount){auto& g=*impl_;if(g.player && !g.player->GetParentShip() && mount<0 && mount>=-c_maxCargo){g.selectedCargo=Allegiance::CanonicalCargoMount(g.player,Mount(mount));g.refresh();}}
void Game::nextCargo(){auto& g=*impl_;if(g.player && !g.player->GetParentShip()){g.selectedCargo=Allegiance::NextCargoMount(g.player,g.selectedCargo);g.refresh();}}
bool Game::reload() {if(impl_->gameRole==GameRole::Replica)return impl_->submit(Net::Operation::Reload,{});if(impl_->gameplayClosed())return false;Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;if(!g.player || g.player->GetParentShip() || g.state.dead || g.player->GetStation())return false;
    Allegiance::ManualReload(g.player,[&](IlauncherIGC* p,EquipmentType type){g.Reload(g.player,p,type);});
    g.PlayNotificationSound(salReloadingSound,g.player);g.refresh();return true;
}
bool Game::dropCargo() {if(impl_->gameRole==GameRole::Replica)return impl_->submit(Net::Operation::Drop,{impl_->selectedCargo});if(impl_->gameplayClosed())return false;Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;if(!g.player || g.player->GetParentShip() || g.state.dead || !g.player->GetCluster())return false;
    g.selectedCargo=Allegiance::CanonicalCargoMount(g.player,g.selectedCargo);
    auto* selected=g.player->GetMountedPart(NA,g.selectedCargo);
    if(!selected){nextCargo();return false;}
    auto* part=Allegiance::LastCargoOfType(g.player,selected->GetPartType());short amount;
    if(Allegiance::TreasureAmount(part,amount))Allegiance::CreatePartTreasure(g.player->GetLastUpdate(),g.player,amount,
        part->GetPartType(),g.player->GetPosition(),20,600,[](){return Vector::RandomDirection();});
    part->Terminate();g.PlaySoundEffect(dropSound,g.player);g.refresh();return true;
}
bool Game::swapCargo(PartKind kind,int mount) {if(impl_->gameRole==GameRole::Replica)return impl_->submit(Net::Operation::Swap,{int(kind),mount,impl_->selectedCargo});if(impl_->gameplayClosed())return false;Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;auto* ship=static_cast<IshipIGC*>(g.player);
    if(!ship || ship->GetParentShip() || g.state.dead || !ship->GetCluster() || int(kind)<0 || int(kind)>ET_Afterburner || kind==PartKind::Pack)return false;
    EquipmentType type=EquipmentType(kind);auto* hull=ship->GetHullType();
    if(mount<0 || mount>=(type==ET_Weapon?hull->GetMaxWeapons():1))return false;
    PartMask mask=hull->GetPartMask(type,Mount(mount));if(!mask)return false;
    auto* old=ship->GetMountedPart(type,Mount(mount));
    if(!Allegiance::SelectCargoType(ship,g.selectedCargo,type,mask,old?old->GetPartType():nullptr))return false;
    auto* incoming=ship->GetMountedPart(NA,g.selectedCargo);
    if(incoming && !hull->CanMount(incoming->GetPartType(),Mount(mount)))return false;
    if(!incoming && !old)return false;
    if(old)Allegiance::SwapPartMounts(old,g.selectedCargo);
    else {
        Allegiance::SwapPartMounts(incoming,Mount(mount));
        if(type==ET_Magazine || type==ET_Dispenser || type==ET_ChaffLauncher)g.Reload(ship,static_cast<IlauncherIGC*>(incoming),type);
        else if(type==ET_Afterburner)g.Reload(ship,nullptr,type);
    }
    g.PlaySoundEffect(incoming?mountSound:unmountSound,ship);
    g.selectedCargo=Allegiance::NextCargoMount(ship,g.selectedCargo);
    Allegiance::SelectCargoType(ship,g.selectedCargo,type,mask,incoming?incoming->GetPartType():nullptr);
    g.refresh();return true;
}
std::vector<PartChoice> Game::cargoMounts()const {
    auto& g=*impl_;std::vector<PartChoice> result;auto* ship=static_cast<IshipIGC*>(g.player);
    if(!ship || ship->GetParentShip() || !ship->GetCluster())return result;
    auto* cargo=ship->GetMountedPart(NA,Allegiance::CanonicalCargoMount(ship,g.selectedCargo));auto* hull=ship->GetHullType();
    for(EquipmentType type=0;type<=ET_Afterburner;++type)if(type!=ET_Pack && (!cargo || cargo->GetEquipmentType()==type))
        for(Mount mount=0;mount<(type==ET_Weapon?hull->GetMaxWeapons():1);++mount) {
            auto* part=ship->GetMountedPart(type,mount);
            if((cargo && hull->CanMount(cargo->GetPartType(),mount) && (!part || part->GetPartType()!=cargo->GetPartType())) || (!cargo && part))
                result.push_back({int(type)*32+mount,(type==ET_Weapon?"Weapon "+std::to_string(mount+1)+": ":std::string())+(part?part->GetPartType()->GetName():"Empty slot")});
        }
    return result;
}
void Game::toggleAutopilot(){if(impl_->gameRole==GameRole::Replica){impl_->submit(Net::Operation::Autopilot,{});return;}Allegiance::SimulationRandomScope randomScope(impl_->randomState);impl_->toggleAutopilot();}
bool Game::acceptCommand(bool execute){if(impl_->gameRole==GameRole::Replica)return impl_->submit(Net::Operation::Accept,{int(execute)});Allegiance::SimulationRandomScope randomScope(impl_->randomState);return impl_->acceptCommand(execute);}
void Game::rejectCommand(){if(impl_->gameRole==GameRole::Replica){impl_->submit(Net::Operation::Reject,{});return;}if(impl_->gameplayClosed())return;impl_->rejectCommand();}
void Game::clearCommand(){if(impl_->gameRole==GameRole::Replica){impl_->submit(Net::Operation::Clear,{});return;}if(impl_->gameplayClosed())return;auto& g=*impl_;if(g.player){g.player->SetCommand(c_cmdAccepted,nullptr,c_cidNone);g.refresh();}}
void Game::targetCommand(){auto& g=*impl_;if(g.player){g.setTarget(g.player->GetCommandTarget(c_cmdAccepted),true,g.player->GetCommandID(c_cmdAccepted));g.submit(Net::Operation::Target,{g.state.targetObjectType,g.state.targetId});}}
float Game::matchTargetThrottle(float fallback)const {auto& g=*impl_;return g.player && g.target && g.seen(g.target)?(Allegiance::MatchTargetThrottle(g.player,g.target)+1)*.5f:fallback;}
void Game::toggleCloak(){if(impl_->gameRole==GameRole::Replica){impl_->submit(Net::Operation::Cloak,{});return;}if(impl_->gameplayClosed())return;auto& g=*impl_;if(g.player && !g.player->GetParentShip() && !g.state.dead){g.player->SetStateBits(cloakActiveIGC,g.training?Training::ApproveActions(g.player->GetStateM()^cloakActiveIGC):g.player->GetStateM()^cloakActiveIGC);g.refresh();}}
void Game::toggleVectorLock(){if(impl_->gameRole==GameRole::Replica){impl_->submit(Net::Operation::Coast,{});return;}if(impl_->gameplayClosed())return;auto& g=*impl_;if(g.player && !g.player->GetParentShip() && !g.state.dead){g.player->SetStateBits(coastButtonIGC,g.training?Training::ApproveActions(g.player->GetStateM()^coastButtonIGC):g.player->GetStateM()^coastButtonIGC);g.PlaySoundEffect(vectorLockSound);g.refresh();}}
bool Game::takeThrottleReset(float& throttle,bool* forced){auto& g=*impl_;if(!g.throttleResetPending)return false;throttle=g.throttleReset;if(forced)*forced=g.forcedThrottleReset;g.throttleResetPending=false;return true;}
void Game::eject(){if(impl_->gameRole==GameRole::Replica){impl_->submit(Net::Operation::Eject,{});return;}if(impl_->gameplayClosed())return;Allegiance::SimulationRandomScope randomScope(impl_->randomState);auto& g=*impl_;if(!g.player || g.state.dead || g.player->GetStation())return;
    auto* ship=g.player->GetSourceShip();Vector at=ship->GetPosition();
    g.KillShipEvent(g.mission->GetLastUpdate(),g.player,nullptr,0,at,at);g.refresh();}
bool Game::ripcord(int sector){if(sector<NA || sector>SHRT_MAX)return false;if(impl_->gameRole==GameRole::Replica)return impl_->submit(Net::Operation::Ripcord,{sector});if(impl_->gameplayClosed())return false;Allegiance::SimulationRandomScope randomScope(impl_->randomState);auto& g=*impl_;if(!g.player || g.player->GetParentShip() || g.state.dead || !g.player->GetCluster())return false;
    if(g.player->GetFlag()!=NA || g.player->GetBaseHullType()->HasCapability(c_habmNoRipcord)){g.playPilotSound(salShipCantRipcordSound,g.player,true);return false;}
    if(sector==NA && g.player->fRipcordActive()){g.RequestRipcord(g.player,nullptr);g.refresh();return true;}
    auto resolve=[&](ImodelIGC* model)->IclusterIGC*{
        if(auto* cluster=model->GetCluster())return g.player->CanSee(model)?cluster:nullptr;
        if(model->GetObjectType()==OT_ship){auto& intel=g.shipIntel[g.player->GetSide()->GetObjectID()];auto found=intel.find(model->GetObjectID());if(found!=intel.end() && found->second.status.current())return g.mission->GetCluster(found->second.status.sector);}
        return nullptr;
    };
    auto* cluster=sector>=0?g.mission->GetCluster(SectorID(sector)):Allegiance::RipcordKeyboardCluster(g.player,resolve);
    if(!cluster)return false;g.RequestRipcord(g.player,cluster);bool active=g.player->fRipcordActive();g.refresh();return active;}
void Game::respawn(){if(impl_->gameRole==GameRole::Replica){impl_->submit(Net::Operation::Respawn,{});return;}if(impl_->gameplayClosed())return;Allegiance::SimulationRandomScope randomScope(impl_->randomState);impl_->respawn();impl_->refresh();}
void Game::recoverToHangar(){if(impl_->gameRole==GameRole::Replica){impl_->submit(Net::Operation::Recover,{});return;}if(impl_->gameplayClosed())return;Allegiance::SimulationRandomScope randomScope(impl_->randomState);auto& g=*impl_;if(!g.state.dead || g.player->IsGhost())return;g.respawn();if(auto* station=g.homeStation())g.player->SetStation(station);g.refresh();}
void Game::nextHull(){Allegiance::SimulationRandomScope randomScope(impl_->randomState);selectHull((impl_->hullIndex+1)%hullIds.size());}
void Game::selectHull(int index){if(impl_->gameRole==GameRole::Replica){impl_->submit(Net::Operation::SelectHull,{index});return;}if(impl_->gameplayClosed())return;Allegiance::SimulationRandomScope randomScope(impl_->randomState);auto& g=*impl_;if(g.economy() || g.training){buyHull(index);return;}if(index<0 || index>=int(hullIds.size()) || !g.player || g.player->IsGhost() || g.player->GetParentShip() || !g.player->GetStation())return;
    auto position=g.player->GetPosition();auto orientation=g.player->GetOrientation();
    g.hullIndex=index;g.selectedHull=hullIds[index];while(auto* part=g.player->GetParts()->first())part->data()->Terminate();
    g.player->SetBaseHullType(g.mission->GetHullType(hullIds[index]));g.player->SetPosition(position);g.player->SetOrientation(orientation);
    g.player->GetThingSite()->SetTrailColor(g.player->GetSide()->GetColor());g.equip(g.player);g.rememberLoadout(g.player);g.selectedWeapon=0;g.groupFire=true;g.refresh();}
bool Game::buyHull(int id) {if(impl_->gameRole==GameRole::Replica)return impl_->submit(Net::Operation::BuyHull,{id});if(impl_->gameplayClosed())return false;Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;if((!g.economy() && !g.training) || !g.player || g.player->IsGhost() || g.player->GetParentShip() || !g.player->GetStation())return false;
    auto* station=g.player->GetStation();auto* hull=g.mission->GetHullType(HullID(id));
    if(!hull || hull->GetGroupID()<0 || !station->CanBuy(hull) || station->IsObsolete(hull) || hull->HasCapability(c_habmLifepod))return false;
    Money budget=g.economy()?g.balance(g.player)+g.player->GetValue():INT_MAX/4;if(budget<hull->GetPrice())return false;
    budget-=hull->GetPrice();while(auto* p=g.player->GetParts()->first())p->data()->Terminate();
    g.player->SetBaseHullType(hull);g.selectedHull=hull->GetObjectID();Allegiance::BuyPreferredParts(g.player,station,&budget);Allegiance::BuyDefaultCargo(g.player,station,&budget,true);if(g.economy())g.balance(g.player)=budget;
    for(auto* l=g.player->GetParts()->first();l;l=l->next()){l->data()->SetMountedFraction(1);l->data()->Arm();g.warmPart(l->data());}
    station->RepairAndRefuel(g.player);g.rememberLoadout(g.player);g.selectedWeapon=0;g.groupFire=true;g.economyDirty=true;g.updateEconomy();g.refresh();return true;
}
Loadout Game::currentLoadout()const {
    const auto& g=*impl_;Loadout result;
    if(!g.player || g.player->IsGhost() || g.player->GetParentShip() || !g.player->GetStation() || g.state.dead)return result;
    result.hull=g.player->GetBaseHullType()->GetObjectID();
    for(auto* l=g.player->GetParts()->first();l;l=l->next())result.parts.push_back({l->data()->GetPartType()->GetObjectID(),int8_t(l->data()->GetMountID()),l->data()->GetAmount()});
    return result;
}
void Game::loadoutPresets(const Loadouts& presets){impl_->loadoutPresets=presets;}
void Game::loadoutPresetFeedback(const Loadout& loadout,int bank,bool saved) {
    auto& g=*impl_;auto* hull=g.mission->GetHullType(loadout.hull);if(!hull || bank<0 || bank>=int(Loadouts::count))return;
    Allegiance::LoadoutPresetFeedback(hull->GetName(),bank,saved,
        [&](bool critical,const char* format,auto... args){g.PostText(critical,format,args...);});
    g.PlaySoundEffect(saved?positiveButtonClickSound:mountSound);
}
void Game::rememberEditorLoadout(const Loadout& loadout,bool onlyIfAbsent){
    auto& g=*impl_;if(loadout.hull<0 || !g.player || g.gameRole==GameRole::Authority)return;
    auto* hull=g.mission->GetHullType(loadout.hull);if(!hull || hull->HasCapability(c_habmLifepod))return;
    if(onlyIfAbsent && g.editorLoadouts.contains(g.player->GetObjectID()))return;
    g.editorLoadouts[g.player->GetObjectID()].remember(0,loadout);
}
LoadoutPreview Game::previewLoadout(const Loadout& requested,bool defaults,int targetHull,bool replace)const {
    auto& g=*impl_;LoadoutPreview result;auto current=currentLoadout();
    auto* source=g.player?g.player->GetSourceShip():nullptr;
    if(g.player && g.player->GetParentShip() && g.player->GetStation() && !g.player->IsGhost() && !g.state.dead && source->GetBaseHullType()){
        current.hull=source->GetBaseHullType()->GetObjectID();
        for(auto* l=source->GetParts()->first();l;l=l->next())current.parts.push_back({l->data()->GetPartType()->GetObjectID(),int8_t(l->data()->GetMountID()),l->data()->GetAmount()});
    }
    if(current.hull<0 || g.uiPreview || g.mission->GetShip(-2))return result;
    Loadout chosen=requested.hull<0?current:requested;
    auto* saved=g.mission->GetHullType(chosen.hull);if(targetHull>=0)chosen.hull=int16_t(targetHull);
    auto* hull=g.mission->GetHullType(chosen.hull);auto* station=g.player->GetStation();
    bool restoringPod=hull && hull->HasCapability(c_habmLifepod) && requested.hull<0 && !defaults && targetHull<0;
    if(!hull || (!restoringPod && (hull->GetGroupID()<0 || hull->HasCapability(c_habmLifepod))) || chosen.parts.size()>128)return result;
    bool occupied[ET_MAX][c_maxMountedWeapons+c_maxCargo]{};
    for(auto& item:chosen.parts) {
        auto* type=g.mission->GetPartType(item.id);if(!type)continue;
        if(item.mount < -c_maxCargo || item.mount>=c_maxMountedWeapons || item.amount<=0)return result;
        int et=item.mount<0?0:type->GetEquipmentType();
        if(et<0 || et>=ET_MAX || (item.mount>=0 && (et==ET_Pack || (et!=ET_Weapon && item.mount!=0))))return result;
        if(occupied[et][item.mount+c_maxCargo])return result;occupied[et][item.mount+c_maxCargo]=true;
    }
    // Use the same temporary-ship policy as WinTrek/load.cpp. No preview object
    // or IGC pointer escapes this scope, and browsing cannot advance sim RNG.
    struct PreviewScope {bool& flag;explicit PreviewScope(bool& f):flag(f){flag=true;}~PreviewScope(){flag=false;}} scope(g.uiPreview);
    auto random=g.randomState;Allegiance::SimulationRandomScope randomScope(random);
    DataShipIGC data{};data.shipID=-2;data.hullID=chosen.hull;data.sideID=g.player->GetSide()->GetObjectID();data.pilotType=c_ptCheatPlayer;data.baseObjectID=NA;data.partsOffset=sizeof(data);
    auto destroy=[](IshipIGC* ship){if(ship){ship->Terminate();ship->Release();}};
    std::unique_ptr<IshipIGC,decltype(destroy)> temporary(static_cast<IshipIGC*>(g.mission->CreateObject(g.mission->GetLastUpdate(),OT_ship,&data,sizeof(data))),destroy);
    if(!temporary)return result;auto* ship=temporary.get();
    auto clear=[&]{while(auto* p=ship->GetParts()->first())p->data()->Terminate();};
    auto fillDefault=[&]{Money budget=INT_MAX/4;Allegiance::BuyPreferredParts(ship,station,&budget);Allegiance::BuyDefaultCargo(ship,station,&budget);};
    if(defaults)fillDefault();
    else if(requested.hull<0 && targetHull<0) {
        // ReplaceLoadout spends the wallet; RestoreLoadout also trades in the
        // owned ship. Initial mission entry, disembark and TELEPORT_ACK use the
        // former with the most recent custom-bank-zero fitting.
        Money budget=g.economy()?Money(std::min<int64_t>(INT_MAX,int64_t(g.balance(g.player))+(replace?0:source->GetValue()))):INT_MAX/4;
        auto cache=g.editorLoadouts.find(g.player->GetObjectID());
        const Loadout* remembered=cache!=g.editorLoadouts.end() && !cache->second.banks[0].empty()?&cache->second.banks[0].front():nullptr;
        if(!replace && remembered && remembered->hull==current.hull) {
            struct Part {PartID id;Mount mount;bool bDuplicated{};};std::vector<Part> parts;parts.reserve(remembered->parts.size());
            for(const auto& part:remembered->parts)parts.push_back({part.id,part.mount});
            Allegiance::RestoreMatchingLoadout<Part>(source,ship,station,hull,budget,
                [&](auto visit){for(auto& part:parts)visit(part);},
                [&](auto visit){for(auto it=parts.rbegin();it!=parts.rend();++it)if(!visit(*it))break;},
                [&](const Part& part){return g.mission->GetPartType(part.id);});
            hull=ship->GetBaseHullType();chosen.hull=hull->GetObjectID();
        } else {
            auto* preset=replace?(g.loadoutPresets.banks[0].empty()?nullptr:&g.loadoutPresets.banks[0].front()):restoringPod?remembered:g.loadoutPresets.find(0,current.hull);
            if(replace || restoringPod || (preset && preset->hull==current.hull)) {
                auto* cachedHull=preset?g.mission->GetHullType(preset->hull):nullptr;
                auto* replacement=cachedHull?Allegiance::PurchasableLoadoutHull(station,cachedHull,budget):nullptr;
                if(replacement){ship->SetBaseHullType(replacement);budget-=replacement->GetPrice();Allegiance::BuyCachedLoadoutParts(ship,station,cachedHull,&budget,[&](auto buy){for(const auto& part:preset->parts)buy(g.mission->GetPartType(part.id),Mount(part.mount));});hull=replacement;chosen.hull=hull->GetObjectID();}
                else {
                    for(auto* link=g.mission->GetHullTypes()->first();link;link=link->next()) {
                        auto* candidate=link->data();if(candidate->GetGroupID()<0 || candidate->HasCapability(c_habmLifepod))continue;
                        if((replacement=Allegiance::PurchasableLoadoutHull(station,candidate,budget)))break;
                    }
                    if(!replacement)return result;
                    ship->SetBaseHullType(replacement);budget-=replacement->GetPrice();
                    Allegiance::BuyPreferredParts(ship,station,&budget);Allegiance::BuyDefaultCargo(ship,station,&budget);
                    hull=replacement;chosen.hull=hull->GetObjectID();
                }
            } else {Allegiance::BuyPreferredParts(ship,station,&budget);Allegiance::BuyDefaultCargo(ship,station,&budget);}
        }
    }
    else if(targetHull>=0) {
        // LoadoutImpl::RefreshLoadout uses ReplaceLoadout: a cached hull is
        // upgraded through the station, or replaced by its first buyable hull.
        Money budget=INT_MAX/4;
        auto* replacement=Allegiance::PurchasableLoadoutHull(station,hull,budget);
        bool cached=replacement!=nullptr;
        if(!replacement)for(auto* link=g.mission->GetHullTypes()->first();link;link=link->next()){
            auto* candidate=link->data();if(candidate->GetGroupID()<0 || candidate->HasCapability(c_habmLifepod))continue;
            if((replacement=Allegiance::PurchasableLoadoutHull(station,candidate,budget)))break;
        }
        if(!replacement)return result;
        hull=replacement;chosen.hull=hull->GetObjectID();ship->SetBaseHullType(hull);budget-=hull->GetPrice();
        if(cached)Allegiance::BuyCachedLoadoutParts(ship,station,saved,&budget,[&](auto buy){for(auto& p:chosen.parts)buy(g.mission->GetPartType(p.id),Mount(p.mount));});
        else {Allegiance::BuyPreferredParts(ship,station,&budget);Allegiance::BuyDefaultCargo(ship,station,&budget);}
    }
    else for(auto& p:chosen.parts)if(auto* type=g.mission->GetPartType(p.id))
        if((p.mount<0 || type->GetEquipmentType()!=ET_Weapon || p.mount<hull->GetMaxWeapons()) && hull->CanMount(type,Mount(p.mount)))ship->CreateAndAddPart(type,Mount(p.mount),p.amount);
    auto metrics=[&](const IhullTypeIGC* type){return std::array<float,8>{type->GetMaxSpeed(),type->GetThrust()/ship->GetMass(),
        (type->GetMaxTurnRate(c_axisYaw)+type->GetMaxTurnRate(c_axisPitch)+type->GetMaxTurnRate(c_axisRoll))*180.f/(3.f*pi),
        type->GetHitPoints(),type->GetMaxEnergy(),type->GetScannerRange(),
        ship->GetSignature()/g.player->GetSide()->GetGlobalAttributeSet().GetAttribute(c_gaSignature),ship->GetMass()};};
    result.valid=true;result.loadout.hull=chosen.hull;result.name=hull->GetName();result.description=hull->GetDescription();result.civilizationIcon=g.player->GetSide()->GetCivilization()->GetIconName();
    result.model=g.loader(hull->GetModelName(),hull->GetTextureName());result.fixedWeapons=hull->GetMaxFixedWeapons();result.weapons=hull->GetMaxWeapons();result.defense=hull->GetDefenseType();
    result.value=ship->GetValue();result.oldValue=g.player->GetValue();result.money=g.economy()?g.balance(g.player):INT_MAX/4;
    result.available=(!g.economy() && !g.training) || (station->CanBuy(hull) && !station->IsObsolete(hull));
    result.affordable=int64_t(result.money)+result.oldValue>=result.value;result.canBuy=result.affordable;
    if(!result.canBuy)for(const auto& pilot:g.state.pilots)if(pilot.side==g.player->GetSide()->GetObjectID() && pilot.id!=g.player->GetObjectID() && pilot.money>=int64_t(result.value)-result.money-result.oldValue){result.canBuy=true;break;}
    result.stats=metrics(ship->GetHullType());
    for(auto* l=ship->GetParts()->first();l;l=l->next())result.loadout.parts.push_back({l->data()->GetPartType()->GetObjectID(),int8_t(l->data()->GetMountID()),l->data()->GetAmount()});
    auto slot=[&](EquipmentType et,Mount mount){LoadoutSlotPreview s;s.kind=PartKind(et);s.mount=mount;
        if(et==ET_Weapon && mount>=hull->GetMaxFixedWeapons()){
            auto* live=source->GetHullType();int turret=mount+live->GetMaxFixedWeapons()-hull->GetMaxFixedWeapons();
            if(turret>=live->GetMaxFixedWeapons() && turret<live->GetMaxWeapons())if(auto* gunner=source->GetGunner(Mount(turret)))s.gunner=gunner->GetName();
        }
        if(auto* p=ship->GetMountedPart(mount<0?NA:et,mount)){auto* t=p->GetPartType();s.kind=PartKind(t->GetEquipmentType());s.id=t->GetObjectID();s.name=Allegiance::BuyableName(t->GetName(),t->GetPrice());s.description=t->GetDescription();s.model=t->GetModelName();s.amount=p->GetAmount();s.metrics=Allegiance::DescribeLoadoutPart(g.mission,ship->GetHullType(),ship->GetSide()->GetGlobalAttributeSet(),t);}
        result.slots.push_back(std::move(s));};
    for(EquipmentType et=0;et<ET_MAX;++et)if(et!=ET_Pack)for(Mount m=0;m<(et==ET_Weapon?hull->GetMaxWeapons():1);++m)if(hull->GetPartMask(et,m))slot(et,m);
    for(Mount m=-c_maxCargo;m<0;++m)slot(ET_Pack,m);
    // HullMenu/GetNextHullType retain the editor's current hull, including one
    // that is no longer sold here. The owned ship is only the trade-in source.
    for(auto* l=g.mission->GetHullTypes()->first();l;l=l->next()){auto* h=l->data();if(h==hull ||
        (h->GetGroupID()>=0 && !h->HasCapability(c_habmLifepod) && ((!g.economy() && !g.training) || (station->CanBuy(h) && !station->IsObsolete(h)))))result.hulls.push_back({h->GetObjectID(),h->GetName(),h->GetPrice()});}
    result.equivalent=ship->EquivalentShip(g.player);
    clear();fillDefault();result.baseStats=metrics(hull);
    return result;
}
std::optional<LoadoutSlotPreview> Game::loadoutPartInfo(int id,int hullId)const {
    const auto& g=*impl_;auto* hull=g.mission->GetHullType(HullID(hullId));auto* part=g.mission->GetPartType(PartID(id));
    if(!hull || !part || !g.player || !g.player->GetSide())return {};
    LoadoutSlotPreview result;result.id=id;result.kind=PartKind(part->GetEquipmentType());
    result.name=Allegiance::BuyableName(part->GetName(),part->GetPrice());
    result.description=part->GetDescription();result.model=part->GetModelName();
    result.metrics=Allegiance::DescribeLoadoutPart(g.mission,hull,g.player->GetSide()->GetGlobalAttributeSet(),part,true);
    return result;
}
std::vector<PartChoice> Game::loadoutPartChoices(PartKind kind,int mount,int hullId)const {
    auto& g=*impl_;std::vector<PartChoice> result;auto* hull=g.mission->GetHullType(HullID(hullId));
    if(!hull || !g.player || !g.player->GetStation() || mount < -c_maxCargo || mount>=(kind==PartKind::Weapon?hull->GetMaxWeapons():1))return result;
    result.push_back({-1,"-Make Slot Empty-",0});auto* station=g.player->GetStation();
    for(auto* l=g.mission->GetPartTypes()->first();l;l=l->next()){auto* t=l->data();
        if(t->GetGroupID()>=0 && (mount<0 || t->GetEquipmentType()==EquipmentType(kind)) && hull->CanMount(t,Mount(mount)) &&
            ((!g.economy() && !g.training) || (station->CanBuy(t) && !station->IsObsolete(t)))){
            bool usable=t->GetEquipmentType()==ET_Pack;for(Mount m=0;!usable && m<(t->GetEquipmentType()==ET_Weapon?hull->GetMaxWeapons():1);++m)usable=hull->CanMount(t,m);
            result.push_back({t->GetObjectID(),t->GetName(),t->GetPrice(),mount<0 && !usable});}}
    return result;
}
std::string Game::loadoutName(const Loadout& loadout)const {
    auto* hull=impl_->mission->GetHullType(loadout.hull);return hull?hull->GetName():"Unavailable hull";
}
PurchaseResult Game::purchaseLoadout(const Loadout& requested,bool launchRequested){
    auto& g=*impl_;
    if(g.gameRole==GameRole::Replica){bool accepted=g.submit(Net::Operation::PurchaseLoadout,{requested.hull,int(launchRequested)},{},{},requested.parts);if(accepted)rememberEditorLoadout(requested);return {accepted,false,false};}
    if(g.gameplayClosed() || !g.player || g.player->IsGhost() || !g.player->GetStation() || g.state.dead || requested.parts.size()>32)return {};
    auto* ship=static_cast<IshipIGC*>(g.player);TRef<IstationIGC> station=ship->GetStation();
    auto* hull=g.mission->GetHullType(requested.hull);auto* oldHull=ship->GetBaseHullType();
    if(!hull || hull->HasCapability(c_habmLifepod) || hull->GetGroupID()<0)return {};
    if(hull!=oldHull){if(!station->CanBuy(hull))return {};hull=static_cast<IhullTypeIGC*>(station->GetSuccessor(hull));}
    bool occupied[ET_MAX][c_maxMountedWeapons+c_maxCargo]{};
    Loadout anticipated;anticipated.hull=hull->GetObjectID();
    for(const auto& item:requested.parts){
        if(item.mount < -c_maxCargo || item.mount>=c_maxMountedWeapons || item.amount<=0)return {};
        auto* part=g.mission->GetPartType(item.id);if(!part)continue;
        int type=item.mount<0?0:part->GetEquipmentType();
        if(type<0 || type>=ET_MAX || (item.mount>=0 && (type==ET_Pack || (type!=ET_Weapon && item.mount!=0))) || occupied[type][item.mount+c_maxCargo])return {};
        occupied[type][item.mount+c_maxCargo]=true;
        auto* bought=station->CanBuy(part)?static_cast<IpartTypeIGC*>(station->GetSuccessor(part)):part;
        if(hull->CanMount(part,Mount(item.mount)) && !hull->CanMount(bought,Mount(item.mount)))return {};
        anticipated.parts.push_back({bought->GetObjectID(),item.mount,item.amount});
    }
    auto preview=previewLoadout(anticipated);
    if(!preview.valid || (g.economy() && int64_t(g.balance(ship))+ship->GetValue()<preview.value))return {};
    // Preserve crew and inventory on invalid or unaffordable requests. Preset
    // fitting remains a separate operation; this is WinTrek's actual purchase.
    Allegiance::SimulationRandomScope randomScope(g.randomState);
    if(ship->GetParentShip() && !g.leaveDockedShip(ship))return {};
    const Money before=ship->GetValue();
    const auto size=sizeof(ShipLoadout)+requested.parts.size()*sizeof(ExpandedPartData);
    std::vector<std::byte> storage(size);auto* loadout=reinterpret_cast<ShipLoadout*>(storage.data());loadout->hullID=requested.hull;
    auto* data=loadout->PartData0();for(size_t i=0;i<requested.parts.size();++i){data[i].partID=requested.parts[i].id;data[i].mountID=requested.parts[i].mount;data[i].amount=requested.parts[i].amount;data[i].fractionMounted=1;}
    bool complete=ship->PurchaseShipLoadout(short(size),loadout);
    // IGC's return value misses an unavailable part with no reusable copy.
    // Verify the result before permitting launch, without changing WinTrek.
    for(const auto& item:requested.parts){auto* part=g.mission->GetPartType(item.id);
        if(!part){complete=false;continue;}
        auto* expected=station->CanBuy(part)?static_cast<IpartTypeIGC*>(station->GetSuccessor(part)):part;
        auto* actual=ship->GetMountedPart(item.mount<0?NA:expected->GetEquipmentType(),item.mount);
        if(!actual || actual->GetPartType()!=expected || actual->GetAmount()<std::min(item.amount,expected->GetAmount(ship)))complete=false;
    }
    if(g.economy())g.balance(ship)-=ship->GetValue()-before;
    for(auto* link=ship->GetChildShips()->first();link;){auto* next=link->next();auto* child=link->data();
        if((oldHull!=ship->GetBaseHullType() && child->GetTurretID()<0) || (child->GetTurretID()>=0 && !ship->GetMountedPart(ET_Weapon,child->GetTurretID())))g.leaveDockedShip(child);
        link=next;
    }
    for(auto* link=ship->GetParts()->first();link;link=link->next()){link->data()->Arm();g.warmPart(link->data());}
    g.selectedHull=ship->GetBaseHullType()->GetObjectID();g.rememberLoadout(ship);g.selectedWeapon=0;g.groupFire=true;g.selectedCargo=-1;
    ship->GetThingSite()->SetTrailColor(ship->GetSide()->GetColor());g.economyDirty=true;g.updateEconomy();g.refresh();
    if(launchRequested && complete)launch();
    return {true,complete,launchRequested && complete && ship->GetStation()==nullptr};
}
PurchaseResult Game::purchaseLaunchLoadout(){
    auto& g=*impl_;
    if(!g.player || !g.player->GetStation() || g.player->IsGhost() || g.gameplayClosed())return {};
    // HangarScreen buys the owned fitting before launch. DisembarkAndLaunch
    // instead replaces the passenger's empty ship from their own bank zero or
    // the station defaults, never using the parent's ship as a trade-in.
    if(g.player->GetParentShip()){
        auto restored=previewLoadout({},false,-1,true);
        return restored.valid && restored.affordable?purchaseLoadout(restored.loadout,true):PurchaseResult{};
    }
    return purchaseLoadout(currentLoadout(),true);
}
PurchaseResult Game::restoreDockedLoadout(bool replace){
    auto& g=*impl_;g.dockRestorationPending=false;
    if(!g.player || !g.player->GetStation() || g.player->GetParentShip() || g.player->IsGhost() || g.training || g.gameplayClosed())return {};
    auto restored=previewLoadout({},false,-1,replace);return restored.valid && restored.affordable?purchaseLoadout(restored.loadout):PurchaseResult{};
}
bool Game::applyLoadout(const Loadout& loadout) {if(impl_->gameRole==GameRole::Replica)return impl_->submit(Net::Operation::Loadout,{loadout.hull},{},{},loadout.parts);if(impl_->gameplayClosed())return false;Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;auto* ship=static_cast<IshipIGC*>(g.player);
    if(!ship || ship->IsGhost() || !ship->GetStation() || g.state.dead || loadout.parts.size()>128)return false;
    auto* station=ship->GetStation();auto* saved=g.mission->GetHullType(loadout.hull);
    if(!saved || saved->HasCapability(c_habmLifepod) || saved->GetGroupID()<0)return false;
    Money budget=g.economy()?g.balance(ship)+ship->GetValue():INT_MAX/4;
    auto* hull=(g.economy() || g.training)?Allegiance::PurchasableLoadoutHull(station,saved,budget):saved;
    bool fallback=!hull;
    if(fallback)for(auto* l=g.mission->GetHullTypes()->first();l;l=l->next()) {
        auto* candidate=l->data();if(candidate->GetGroupID()>=0 && !candidate->HasCapability(c_habmLifepod))
            if((hull=Allegiance::PurchasableLoadoutHull(station,candidate,budget)))break;
    }
    if(!hull)return false;
    // Validate persisted slots before changing the real ship. Missing core parts
    // are skipped; duplicate or impossible slots must never reach IGC asserts.
    bool occupied[ET_MAX][c_maxMountedWeapons+c_maxCargo]{};
    for(auto& item:loadout.parts) {
        if(item.mount < -c_maxCargo || item.mount>=c_maxMountedWeapons)return false;
        auto* part=g.mission->GetPartType(item.id);if(!part)continue;
        int type=item.mount<0?0:part->GetEquipmentType();
        if(type<0 || type>=ET_MAX || (item.mount>=0 && (type==ET_Pack || (type!=ET_Weapon && item.mount!=0))))return false;
        if(occupied[type][item.mount+c_maxCargo])return false;occupied[type][item.mount+c_maxCargo]=true;
    }
    // Disembark only after validating the purchase. Cancelling or an invalid
    // request must leave the crew relationship and the parent's loadout intact.
    if(ship->GetParentShip() && !g.leaveDockedShip(ship))return false;
    budget-=hull->GetPrice();
    while(auto* l=ship->GetParts()->first())l->data()->Terminate();ship->SetBaseHullType(hull);g.selectedHull=hull->GetObjectID();
    if(fallback){Allegiance::BuyPreferredParts(ship,station,&budget);Allegiance::BuyDefaultCargo(ship,station,&budget,true);}
    else if(g.economy() || g.training)Allegiance::BuyCachedLoadoutParts(ship,station,saved,&budget,[&](auto buy){for(auto& item:loadout.parts)buy(g.mission->GetPartType(item.id),Mount(item.mount));});
    else for(auto& item:loadout.parts)if(auto* part=g.mission->GetPartType(item.id))if((item.mount<0 || part->GetEquipmentType()!=ET_Weapon || item.mount<hull->GetMaxWeapons()) && hull->CanMount(part,Mount(item.mount)))ship->CreateAndAddPart(part,Mount(item.mount),SHRT_MAX);
    if(g.economy())g.balance(ship)=budget;
    for(auto* l=ship->GetParts()->first();l;l=l->next()){l->data()->SetMountedFraction(1);l->data()->Arm();g.warmPart(l->data());}
    station->RepairAndRefuel(ship);g.rememberLoadout(ship);g.selectedWeapon=0;g.groupFire=true;g.selectedCargo=-1;
    ship->GetThingSite()->SetTrailColor(ship->GetSide()->GetColor());g.economyDirty=true;g.updateEconomy();g.refresh();
    return true;
}
std::vector<ResearchChoice> Game::researchChoices()const {
    auto& g=*impl_;std::vector<ResearchChoice> result;if(!g.onTeam() || !g.economy())return result;
    auto* side=g.player->GetSide();
    for(auto* l=side->GetBuckets()->first();l;l=l->next()) {
        auto* b=l->data();ResearchChoice row;row.id=b->GetObjectID();row.type=b->GetBucketType();row.group=b->GetGroupID();
        row.price=b->GetPrice();row.invested=b->GetMoney();row.progress=b->GetPercentComplete();row.bought=b->GetPercentBought();row.complete=b->GetCompleteF();
        row.name=b->GetName();row.model=b->GetModelName();row.description=b->GetDescription();row.timeToBuild=int(b->GetTimeToBuild());row.timeRemaining=row.timeToBuild-int(b->GetTime()/1000);
        row.canBuy=side->CanBuy(b);row.predecessorReady=!b->GetPredecessor() || b->GetPredecessor()->GetCompleteF();row.available=g.mayInvest(b);
        for(auto* prerequisite:Allegiance::InvestmentPrerequisites(b))row.prerequisites.emplace_back(prerequisite->GetName());
        if(row.type==OT_droneType) {
            row.count=g.countDrones(side,static_cast<IdroneTypeIGC*>(b->GetBuyable()));row.limit=g.mission->GetMissionParams()->nMaxDronesPerTeam;
            if(row.count>=row.limit)row.status="At maximum "+row.name+"s";
        } else if(row.type==OT_stationType) {
            auto* type=static_cast<IstationTypeIGC*>(b->GetBuyable());row.buildMask=type->GetBuildAABM();
            for(auto* station=side->GetStations()->first();station;station=station->next())if(station->data()->GetBaseStationType()->GetClassID()==type->GetClassID())++row.count;
            IshipIGC* builder=nullptr;for(auto* ship=side->GetShips()->first();ship;ship=ship->next())if(ship->data()->GetBaseData()==type){builder=ship->data();break;}
            if(row.bought>=100)row.status="Preparing builder ...";
            else if(builder)row.status=builder->GetCommandTarget(c_cmdAccepted) && builder->GetCommandID(c_cmdAccepted)==c_cidBuild?"Constructor en route ...":"Constructor waiting ...";
            else if(!g.investmentBuildSpot(side,type))row.status=std::string("no ")+IasteroidIGC::GetTypeName(type->GetBuildAABM())+" asteroid available";
        } else if(row.type==OT_partType || row.type==OT_hullType)row.count=side->GetStockpile(b->GetBuyable());
        result.push_back(std::move(row));
    }
    return result;
}
std::span<const int> Game::investmentSectors(uint32_t buildMask) {
    auto& g=*impl_;auto mask=Allegiance::InvestmentAsteroidMask(AsteroidAbilityBitMask(buildMask));
    int side=g.onTeam()?g.player->GetSide()->GetObjectID():-1;
    if(g.investmentHintsDirty || g.investmentHintMask!=uint32_t(mask) || g.investmentHintSide!=side) {
        g.investmentHintSectors.clear();g.investmentHintMask=mask;g.investmentHintSide=side;
        // Rebuild only after knowledge/rock changes or a different hovered
        // purchase. Unchanged rendered frames retain the compact sector list.
        if(mask && side>=0) {
            auto* viewer=g.player->GetSide();
            for(auto* l=g.mission->GetAsteroids()->first();l;l=l->next()) {
                auto* rock=l->data();if(rock->GetCluster() && rock->HasCapability(mask) && rock->SeenBySide(viewer))g.investmentHintSectors.push_back(rock->GetCluster()->GetObjectID());
            }
            for(auto& rock:g.rememberedRocks)if((rock.sides&(1u<<side)) && (rock.capabilities&mask)==mask)g.investmentHintSectors.push_back(rock.sector);
            std::sort(g.investmentHintSectors.begin(),g.investmentHintSectors.end());
            g.investmentHintSectors.erase(std::unique(g.investmentHintSectors.begin(),g.investmentHintSectors.end()),g.investmentHintSectors.end());
        }
        g.investmentHintsDirty=false;
    }
    return g.investmentHintSectors;
}
bool Game::invest(int id,int amount) {
    auto& g=*impl_;IbucketIGC* bucket=nullptr;Money payment=g.planInvestment(id,amount,bucket);if(payment<=0)return false;
    // BaseClient::AddMoneyToBucket updates the local wallet and bar first and
    // sends the actual bounded payment. A failed send must leave both intact.
    if(g.gameRole==GameRole::Replica && !g.submit(Net::Operation::Invest,{id,payment}))return false;
    Allegiance::SimulationRandomScope randomScope(g.randomState);
    payment=g.fundInvestment(bucket,payment);g.updateEconomy();g.refresh();return payment>0;
}
bool Game::droneStandby(int id,bool stay) {
    auto& g=*impl_;if(g.gameRole==GameRole::Replica)return g.submit(Net::Operation::DroneStandby,{id,int(stay)});
    if(g.gameplayClosed() || id<0 || id>SHRT_MAX)return false;
    Allegiance::SimulationRandomScope randomScope(g.randomState);
    auto* ship=g.mission->GetShip(ShipID(id));
    if(!ship || ship->GetSide()!=g.player->GetSide() || !Allegiance::SetDroneStandby(ship,stay))return false;
    g.SendChat(ship,CHAT_INDIVIDUAL,g.player->GetObjectID(),voAffirmativeSound,"Affirmative.");
    if(stay && !ship->GetStation())g.SendChat(ship,CHAT_TEAM,ship->GetSide()->GetObjectID(),NA,"Heading back to base. I'll sit tight.");
    g.economyDirty=true;g.updateEconomy();g.refresh();return true;
}
bool Game::order(int id,int targetType,int targetId,int command) {if(impl_->gameRole==GameRole::Replica)return impl_->submit(Net::Operation::Order,{id,targetType,targetId,command});if(impl_->gameplayClosed())return false;Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;if(!g.player || (command!=c_cidDoNothing && (command<0 || command>=c_cidMax)))return false;
    auto* ship=g.mission->GetShip(ShipID(id));if(!ship || ship->GetParentShip() || !ship->GetBaseHullType() || ship->GetSide()!=g.player->GetSide())return false;
    TRef<ImodelIGC> target=targetType>=0?g.mission->GetModel(ObjectType(targetType),ObjectID(targetId)):nullptr;
    bool remembered=false;
    if(!target && targetType==OT_asteroid && ship->GetPilotType()==c_ptBuilder &&
        (command==c_cidDefault || command==c_cidBuild || command==c_cidGoto)) {
        for(auto& rock:g.rememberedRocks)if(rock.id==targetId && (rock.sides&(1u<<ship->GetSide()->GetObjectID()))) {
            DataBuoyIGC data{};data.position=vec(rock.position);data.buoyID=g.mission->GenerateNewBuoyID();data.clusterID=rock.sector;
            data.type=ship->GetCluster() && ship->GetCluster()->GetObjectID()==rock.sector?c_buoyWaypoint:c_buoyCluster;
            target=static_cast<ImodelIGC*>(g.mission->CreateObject(g.mission->GetLastUpdate(),OT_buoy,&data,sizeof(data)));
            if(target){target->Release();remembered=true;}break;
        }
    }
    auto cid=Allegiance::ResolveDefaultOrder(ship,target,CommandID(command));
    if((targetType>=0 && !target) || (target && !g.seen(target)) || (!remembered && !ship->LegalCommand(cid,target)))return false;
    IclusterIGC* miningCluster=nullptr;
    if(ship->GetPilotType()==c_ptMiner && target && target->GetObjectType()==OT_warp)
        if(auto* dest=static_cast<IwarpIGC*>(static_cast<ImodelIGC*>(target))->GetDestination())miningCluster=dest->GetCluster();
    if(ship->GetPilotType()>=c_ptPlayer) {
        if(!target)return false;
        g.SendChat(g.player,CHAT_INDIVIDUAL,id,NA,nullptr,cid,target->GetObjectType(),target->GetObjectID(),target);
        g.updateEconomy();g.refresh();return true;
    }
    ship->SetAutopilot(true);Allegiance::ApplyAcceptedOrder(ship,target,cid);Allegiance::RememberMinerOrder(ship,target,g.mission->GetLastUpdate(),miningCluster);
    g.SendChat(ship,CHAT_INDIVIDUAL,g.player->GetObjectID(),voAffirmativeSound,"Affirmative.");
    g.updateEconomy();g.refresh();return true;
}

namespace {
bool chatRecipient(IshipIGC* player,ChatChannel channel,int recipient,ChatTarget& target,ObjectID& id) {
    if(!player)return false;target=CHAT_EVERYONE;id=NA;
    switch(channel) {
    case ChatChannel::Everyone:break;
    case ChatChannel::Team:target=CHAT_TEAM;id=player->GetSide()->GetObjectID();break;
    case ChatChannel::Allies:target=CHAT_ALLIES;id=player->GetSide()->GetObjectID();break;
    case ChatChannel::Sector:{target=CHAT_ALL_SECTOR;auto* cluster=player->GetSourceShip()->GetCluster();if(!cluster && player->GetStation())cluster=player->GetStation()->GetCluster();if(!cluster)return false;id=cluster->GetObjectID();break;}
    case ChatChannel::Wing:target=CHAT_WING;id=recipient<0?player->GetWingID():recipient;if(id<0)id=0;if(id>=c_widMax)return false;break;
    case ChatChannel::Target:target=CHAT_INDIVIDUAL;if(recipient==NA)recipient=player->GetObjectID();if(recipient<0 || recipient>SHRT_MAX || !player->GetMission()->GetShip(ShipID(recipient)))return false;id=ObjectID(recipient);break;
    case ChatChannel::FriendlySector:{if(recipient>SHRT_MAX)return false;target=CHAT_FRIENDLY_SECTOR;auto* cluster=recipient>=0 && recipient<=SHRT_MAX?player->GetMission()->GetCluster(SectorID(recipient)):player->GetSourceShip()->GetCluster();if(recipient<0 && !cluster && player->GetStation())cluster=player->GetStation()->GetCluster();if(!cluster)return false;id=cluster->GetObjectID();break;}
    case ChatChannel::Ship:if(player->GetSourceShip()->GetChildShips()->n())target=CHAT_SHIP;else{target=CHAT_TEAM;id=player->GetSide()->GetObjectID();}break;
    case ChatChannel::Leaders:target=CHAT_LEADERS;break;
    case ChatChannel::Admin:target=CHAT_ADMIN;break;
    case ChatChannel::Group:case ChatChannel::GroupNoEcho:
        target=channel==ChatChannel::Group?CHAT_GROUP:CHAT_GROUP_NOECHO;
        if(recipient<0 || recipient>SHRT_MAX || !player->GetMission()->GetShip(ShipID(recipient)))return false;id=ObjectID(recipient);break;
    default:return false;
    }return true;
}
}
bool Game::sendChat(ChatChannel channel,const std::string& text,int recipient,bool echoAuthor) {Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;ChatTarget target;ObjectID id;
    if(!g.player || text.empty() || text.size()>255 || text.find('\0')!=text.npos)return false;
    auto echo=[&]{
        bool route=chatRecipient(g.player,channel,recipient,target,id);
        if(!route && (channel==ChatChannel::Target || channel==ChatChannel::Group || channel==ChatChannel::GroupNoEcho) && recipient>=0 && recipient<=SHRT_MAX &&
           std::any_of(g.state.networkPilots.begin(),g.state.networkPilots.end(),[&](auto& p){return p.id==recipient;})){
            target=channel==ChatChannel::Target?CHAT_INDIVIDUAL:channel==ChatChannel::Group?CHAT_GROUP:CHAT_GROUP_NOECHO;id=ObjectID(recipient);route=true;
        }
        if(route && target!=CHAT_GROUP_NOECHO)g.receiveChat(g.player,target,id,NA,text.c_str(),c_cidNone,NA,NA,nullptr,false,true);
    };
    // BaseClient never submits an individual message addressed to itself.
    // In particular, server chat commands in that route are just local text.
    if(g.gameRole!=GameRole::Authority && channel==ChatChannel::Target && (recipient==NA || recipient==g.player->GetObjectID()) && text[0]!='!'){
        if(echoAuthor)echo();g.refresh();return true;
    }
    if(g.gameRole==GameRole::Replica){
        if(!g.submit(Net::Operation::Chat,{int(channel),recipient},{},text))return false;
        // BaseClient displays typed outgoing chat locally, before the server
        // can consume a command or change its recipients. Server::chatAction
        // excludes its author from the forwarded text, avoiding a second echo.
        if(echoAuthor && text[0]!='!')echo();g.refresh();return true;
    }
    if(text[0]=='!') {
        auto command=Allegiance::ParseMuteChatCommand(text.c_str());
        if(command.recognized)for(auto* link=g.mission->GetShips()->first();link;link=link->next())
            if(Allegiance::ChatNamesEqual(link->data()->GetName(),command.name)) {
                // BaseClient shell commands permit self-mute; the roster's
                // ordinary mute action intentionally excludes the current pilot.
                g.setMuted(link->data(),command.muted);
                break;
            }
        return true; // Unknown shell commands also leave composition silently.
    }
    bool local=g.gameRole==GameRole::Offline;
    auto command=Allegiance::ParseMissionChat(text,{local && g.player->GetObjectID()==g.commanders[g.options.team],
        g.onTeam() && g.player->GetObjectID()==g.commanders[g.player->GetSide()->GetObjectID()],g.onTeam(),
        g.mission->GetMissionStage()==STAGE_STARTED && !g.training && !g.state.complete});
    auto ballot=Allegiance::BallotCommand(text);
    bool voting=ballot!=Allegiance::BallotKind::None && g.onTeam() && g.mission->GetMissionStage()==STAGE_STARTED && !g.training && !g.state.complete;
    if(local && echoAuthor && (command.kind==Allegiance::MissionChatKind::List || command.kind==Allegiance::MissionChatKind::Ban || command.kind==Allegiance::MissionChatKind::AutoBalance || voting))echo();
    if(command.kind==Allegiance::MissionChatKind::List){g.SendChat(nullptr,CHAT_INDIVIDUAL_NOFILTER,g.player->GetObjectID(),NA,Allegiance::MissionChatHelp);g.refresh();return true;}
    if(local && command.kind==Allegiance::MissionChatKind::Ban){
        int id=-1;for(auto* l=g.mission->GetShips()->first();l;l=l->next())if(auto* ship=l->data();ship->GetPilotType()>=c_ptPlayer && ship->GetSide()->GetObjectID()<0 && Allegiance::ChatNameEqual(command.target,ship->GetName())){id=ship->GetObjectID();break;}
        if(id>=0)retireNetworkPilot(id);return true;
    }
    if(local && command.kind==Allegiance::MissionChatKind::AutoAccept)g.state.autoAccept^=1u<<g.player->GetSide()->GetObjectID();
    if(local && command.kind==Allegiance::MissionChatKind::AutoBalance){if(command.imbalance>=0)teamImbalance(command.imbalance);channel=ChatChannel::Everyone;recipient=-1;echoAuthor=false;}
    if(voting){
        g.proposeBallot(ballot);g.refresh();return true;
    }
    if(!chatRecipient(g.player,channel,recipient,target,id))return false;
    g.sendChat(g.player,target,id,NA,text.c_str(),c_cidNone,NA,NA,nullptr,false,echoAuthor);g.refresh();return true;
}
bool Game::vote(bool agree) {
    auto& g=*impl_;g.refreshBallot();if(g.state.ballot.id<0)return false;
    auto id=g.state.ballot.id;
    if(g.gameRole==GameRole::Replica){if(!g.submit(Net::Operation::Vote,{id,int(agree)}))return false;}
    else if(!g.castBallot(id,g.player->GetObjectID(),agree))return false;
    auto queue=g.pendingBallots.find(g.player->GetObjectID());
    if(queue!=g.pendingBallots.end() && !queue->second.empty() && queue->second.front().id==id)queue->second.pop_front();
    g.refreshBallot();return true;
}
bool Game::sendQuickChat(ChatChannel channel,const QuickChatCommand& entry,int recipient,bool voiceOnly) {if(impl_->gameRole==GameRole::Replica){if(!impl_->quickChatCatalog)return false;auto& list=impl_->quickChatCatalog->commands;auto it=std::find_if(list.begin(),list.end(),[&](auto& e){return &e==&entry;});return it!=list.end() && impl_->submit(Net::Operation::QuickChat,{int(channel),recipient,int(it-list.begin()),int(voiceOnly)});}Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;ChatTarget target;ObjectID id;
    if(entry.command<c_cidDoNothing || entry.command>=c_cidMax || !chatRecipient(g.player,channel,recipient,target,id))return false;
    voiceOnly=voiceOnly || g.state.networkLobby || g.gameplayClosed();
    CommandID command=voiceOnly?c_cidNone:CommandID(entry.command);
    TRef<ImodelIGC> model=voiceOnly?nullptr:Allegiance::QuickChatTarget(g.player,command,entry.targetMask,AbilityBitMask(entry.abilities));
    if(g.gameplayClosed() && command!=c_cidNone)return false;
    if(g.training && !voiceOnly)Training::CommandAcknowledgedCondition::SetCommandAcknowledged(CommandID(entry.command));
    // FedSrv commands drones only through explicit individual/group routes.
    // Team, wing and friendly-sector broadcasts deliver orders to humans.
    if(model && command>=c_cidDefault && (channel==ChatChannel::Target || channel==ChatChannel::Group || channel==ChatChannel::GroupNoEcho)) {
        auto* ship=g.mission->GetShip(ShipID(recipient));
        if(ship && ship->GetPilotType()<c_ptPlayer && ship->GetSide()==g.player->GetSide())order(recipient,model->GetObjectType(),model->GetObjectID(),command);
    }
    bool logical=entry.sonic>=0 && g.quickChatCatalog;
    g.sendChat(g.player,target,id,logical?entry.sonic:entry.sound,logical?nullptr:entry.text.c_str(),command,model?model->GetObjectType():NA,model?model->GetObjectID():NA,model,true);
    g.refresh();return true;
}

void Game::chatPreferences(const Allegiance::ChatPreferences& preferences){
    auto& g=*impl_;
    if(g.gameRole==GameRole::Replica)g.submit(Net::Operation::ChatPreferences,{int(Allegiance::ChatPreferenceBits(preferences)),preferences.filterLobby});
    if(g.chatPrefs.censor!=preferences.censor){
        ++g.displayRevision;for(auto& message:g.state.messages){message.displayText=censorChat(message.text,preferences.censor);message.displayRevision=g.displayRevision;}
        for(auto& [id,request]:g.moneyRequests)request.displayPrompt=censorChat(request.prompt,preferences.censor);
        auto request=g.moneyRequests.find(g.player->GetObjectID());if(request!=g.moneyRequests.end())g.state.moneyRequest.displayPrompt=request->second.displayPrompt;
    }
    g.chatPrefs=preferences;
}
void Game::quickChats(const QuickChatCatalog& catalog){impl_->quickChatCatalog=&catalog;}
void Game::interfacePrompts(std::function<std::string(std::string_view)> formatter){impl_->interfaceFormatter=std::move(formatter);}
bool Game::mutePilot(int id,bool mute) {
    auto& g=*impl_;if(id<0 || id>SHRT_MAX)return false;
    auto* ship=g.mission->GetShip(ShipID(id));
    bool known=ship || std::any_of(g.state.networkPilots.begin(),g.state.networkPilots.end(),[&](const auto& pilot){return pilot.id==id;});
    if(!known || (g.player && id==g.player->GetObjectID() && mute))return false;
    g.setMuted(id,mute);return true;
}

void Game::visualPreferences(const VisualPreferences& preferences){
    auto& g=*impl_;if(g.visual.trails!=preferences.trails)for(auto* t:g.things){t->hadTrail=false;t->trailTime=0;}
    if(g.fx)g.fx->debris(preferences.debris);
    g.visual=preferences;g.refresh();
}
void Game::showGrid(bool enabled){impl_->showGrid=enabled;}
void Game::showCommandGrid(bool enabled){impl_->showCommandGrid=enabled;}
void Game::commandView(bool enabled,bool topDown,short logicalKey) {Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;if(!g.onTeam() || g.state.cinematic || (g.player->IsGhost() && !enabled))return;
    if(!recordKey(logicalKey<0?(enabled?TK_ViewCommand:TK_ConModeCombat):logicalKey))return;
    if(g.gameRole==GameRole::Replica)g.submit(Net::Operation::View,{-1,int(enabled)});
    g.pendingCommandFocus.reset();
    if(g.inCommandView())g.gridWasCommand=true;
    g.commandViewEnabled=enabled;g.commandTopDown=topDown;
    if(g.training){g.training->view=enabled?TrainingHost::vmCommand:TrainingHost::vmCombat;
        g.training->camera=enabled?(topDown?TrainingHost::cmExternalCommandViewTD:TrainingHost::cmExternalCommandView34):TrainingHost::cmCockpit;}
    if(enabled)g.recenterCommand(0);
    else {g.commandCluster=nullptr;if(g.training)g.training->viewCluster=nullptr;}
    // Retain the preceding camera frame so the shared camera controller can
    // interpolate the original combat/command transition.
    g.commandTracking=false;g.commandCameraDirty=true;g.refresh();
}
void Game::toggleCommandPerspective() {Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;if(!recordKey(TK_ToggleCommand))return;
    g.commandTopDown=!g.commandTopDown;g.commandCameraDirty=true;
    if(g.inCommandView())g.recenterCommand(2);
    g.refresh();
}
bool Game::zoomCommandCamera(bool out,float seconds){
    auto& g=*impl_;
    if(!g.inCommandView() || g.state.complete || !std::isfinite(seconds) || seconds<0 || seconds>1 || (g.training && !g.training->displacement))return false;
    if(seconds>0)g.pendingCommandFocus.reset();
    g.commandDistance=Allegiance::ZoomCameraDistance(g.commandDistance,seconds,out,50,20000,2);g.commandCameraDirty=true;return true;
}
bool Game::viewCamera(CameraMode mode,short logicalKey) {Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;constexpr TrekKey keys[]={TK_ConModeCombat,TK_ViewChase,TK_ViewExternal,TK_ViewTarget,TK_ViewFlyby,TK_ViewExternalOrbit,TK_ViewExternalStation,TK_ViewMissile,TK_NoKeyMapping,TK_ViewBase};
    if(!g.onTeam() || int(mode)<0 || int(mode)>=int(std::size(keys)) || mode==CameraMode::Command || g.state.cinematic || g.player->IsGhost())return false;
    if(!recordKey(logicalKey<0?keys[int(mode)]:logicalKey))return false;
    if(g.gameRole==GameRole::Replica)g.submit(Net::Operation::View,{-1,0});
    g.pendingCommandFocus.reset();
    g.commandViewEnabled=false;g.commandCluster=nullptr;
    if(g.training) {
        constexpr TrainingHost::CameraMode modes[]={TrainingHost::cmCockpit,TrainingHost::cmExternalChase,TrainingHost::cmExternal,
            TrainingHost::cmExternalTarget,TrainingHost::cmExternalFlyBy,TrainingHost::cmExternalOrbit,TrainingHost::cmExternalStation,TrainingHost::cmExternalMissile,TrainingHost::cmExternalCommandViewTD,TrainingHost::cmExternalBase};
        g.training->view=TrainingHost::vmCombat;g.training->camera=modes[int(mode)];g.training->viewCluster=nullptr;
    }
    g.refresh();return true;
}
void Game::trackCommandView() {
    auto& g=*impl_;g.pendingCommandFocus.reset();g.commandTracking=!g.commandTracking;
    g.message(g.commandTracking?"Command view tracking enabled":"Command view tracking disabled");
}
bool Game::viewSector(int id) {Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;if(!g.inCommandView() || id<0 || id>SHRT_MAX)return false;
    auto* cluster=g.mission->GetCluster(SectorID(id));
    if(!cluster || std::none_of(g.state.sectors.begin(),g.state.sectors.end(),[&](auto& s){return s.id==id;}))return false;
    if(g.gameRole==GameRole::Replica)g.submit(Net::Operation::View,{id,1});
    g.selectCommandSector(cluster);return true;
}
bool Game::selectMapSector(int id) {
    auto& g=*impl_;auto* cluster=id>=0 && id<=SHRT_MAX?g.mission->GetCluster(SectorID(id)):nullptr;
    if(!g.onTeam() || g.state.dead || !g.knownSector(cluster))return false;
    if(g.gameRole==GameRole::Replica){if(g.player->GetStation()){g.commandViewEnabled=true;g.cameraValid=false;g.selectCommandSector(cluster);}return g.submit(Net::Operation::MapSector,{id});}
    Allegiance::SimulationRandomScope random(g.randomState);
    if(g.player->fRipcordActive() && !g.player->GetAutopilot()){g.RequestRipcord(g.player,cluster);g.refresh();return true;}
    if(g.player->GetStation()){commandView(true);return viewSector(id);}
    if(g.player->GetCluster()==cluster)return true;
    return orderSector(g.player->GetObjectID(),id);
}
bool Game::orderSector(int shipId,int id) {
    auto& g=*impl_;auto* cluster=id>=0 && id<=SHRT_MAX?g.mission->GetCluster(SectorID(id)):nullptr;
    if(g.gameplayClosed() || !g.knownSector(cluster) || shipId<0 || shipId>SHRT_MAX)return false;
    auto* ship=g.mission->GetShip(ShipID(shipId));if(!ship || ship->GetSide()!=g.player->GetSide() || ship->GetParentShip())return false;
    if(g.gameRole==GameRole::Replica)return g.submit(Net::Operation::SectorOrder,{shipId,id});
    Allegiance::SimulationRandomScope random(g.randomState);
    DataBuoyIGC data{};data.clusterID=id;data.buoyID=g.mission->GenerateNewBuoyID();data.type=c_buoyCluster;data.visible=true;
    auto* buoy=static_cast<IbuoyIGC*>(g.mission->CreateObject(g.mission->GetLastUpdate(),OT_buoy,&data,sizeof(data)));if(!buoy)return false;
    buoy->AddConsumer();bool ok=order(shipId,OT_buoy,buoy->GetObjectID(),c_cidGoto);buoy->ReleaseConsumer();buoy->Release();return ok;
}
bool Game::highlightSector(int id) {
    auto& g=*impl_;auto* cluster=id>=0 && id<=SHRT_MAX?g.mission->GetCluster(SectorID(id)):nullptr;
    if(!g.onTeam() || !g.knownSector(cluster))return false;
    auto& highlighted=g.sectorHighlights[g.player->GetSide()->GetObjectID()];
    if(g.commanders[g.player->GetSide()->GetObjectID()]!=g.player->GetObjectID()){g.dismissedSectorHighlights.insert(id);g.refresh();return true;}
    if(g.gameRole==GameRole::Replica)return g.submit(Net::Operation::SectorHighlight,{id});
    if(!highlighted.erase(id))highlighted.insert(id);g.dismissedSectorHighlights.erase(id);g.refresh();return true;
}
const SectorInfo& Game::sectorInfo(int id) {
    auto& g=*impl_;int side=g.onTeam()?g.player->GetSide()->GetObjectID():-1;
    auto* cluster=id>=0 && id<=SHRT_MAX?g.mission->GetCluster(SectorID(id)):nullptr;
    if(g.cachedSectorInfo.id==id && g.sectorInfoSide==side && g.sectorInfoIntel==g.mapIntelRevision && g.sectorInfoEconomy==g.state.economyRevision && g.elapsed-g.sectorInfoTime<.05)return g.cachedSectorInfo;
    auto& info=g.cachedSectorInfo;info.id=id;info.assets.clear();info.asteroids.clear();info.helium=0;info.probes=0;
    g.sectorInfoSide=side;g.sectorInfoIntel=g.mapIntelRevision;g.sectorInfoEconomy=g.state.economyRevision;g.sectorInfoTime=g.elapsed;
    if(side<0 || !cluster)return info;auto* viewer=g.player->GetSide();
    auto add=[&](std::string_view image,int owner,int order){
        if(image.empty())return;auto found=std::find_if(info.assets.begin(),info.assets.end(),[&](auto& row){return row.image==image && row.side==owner;});
        if(found!=info.assets.end())++found->count;else info.assets.push_back({std::string(image),owner,1,order});
    };
    for(auto* l=cluster->GetStations()->first();l;l=l->next())if(l->data()->SeenBySide(viewer)) {
        auto* station=l->data();auto* observed=static_cast<Impl::Thing*>(station->GetThingSite())->stationObservation(viewer);
        if(observed && !observed->pedestal)add(observed->icon,observed->intel.side,-1+(observed->intel.side==side?c_cSidesMax*2:c_cSidesMax+observed->intel.side)*0x10000);
    }
    for(auto* l=cluster->GetShips()->first();l;l=l->next()) {
        auto* ship=l->data();if(!ship->SeenBySide(viewer) || ship->GetParentShip() || ship->IsGhost() || !ship->GetVisibleF())continue;
        if(auto* hull=ship->GetBaseHullType())if(auto* icon=static_cast<Impl::Icon*>(hull->GetIcon()))add(icon->name,ship->GetSide()->GetObjectID(),hull->GetObjectID()+(ship->GetSide()==viewer?c_cSidesMax:ship->GetSide()->GetObjectID())*0x10000);
    }
    std::stable_sort(info.assets.begin(),info.assets.end(),[](auto& a,auto& b){return a.order<b.order;});
    auto resource=[&](unsigned caps,std::string_view icon){if(!(caps&~c_aabmBuildable) || icon.empty())return;
        auto found=std::find_if(info.asteroids.begin(),info.asteroids.end(),[&](auto& row){return row.abilities==caps;});
        if(found!=info.asteroids.end())++found->count;else info.asteroids.push_back({std::string(icon),1,caps});};
    for(auto* l=cluster->GetAsteroids()->first();l;l=l->next())if(l->data()->SeenBySide(viewer)){
        auto* rock=l->data();if(auto* icon=static_cast<Impl::Icon*>(rock->GetIcon()))resource(rock->GetCapabilities(),icon->name);
        if(rock->HasCapability(c_aabmMineHe3))info.helium+=rock->GetOreSeenBySide(viewer);
    }
    for(auto& rock:g.rememberedRocks)if(rock.sector==id && (rock.sides&(1u<<side)))resource(rock.capabilities,rock.icon);
    std::sort(info.asteroids.begin(),info.asteroids.end(),[](auto& a,auto& b){return a.abilities<b.abilities;});
    for(auto* l=cluster->GetProbes()->first();l;l=l->next())if(l->data()->GetSide()==viewer && (l->data()->GetProbeType()->GetCapabilities()==0 || l->data()->GetProbeType()->GetCapabilities()>=c_eabmRescue))++info.probes;
    return info;
}
bool Game::pickCommandUnit(int id,bool toggle) {
    auto& g=*impl_;auto* ship=id>=0 && id<=SHRT_MAX?g.mission->GetShip(ShipID(id)):nullptr;
    if(!ship || !g.player || !IsideIGC::AlliedSides(ship->GetSide(),g.player->GetSide()))return false;
    if(!toggle){g.commandUnits.clear();g.commandSelected.reset();}
    if(g.commandSelected.test(id)) {
        // Original Shift-click retains the last recipient.
        if(g.commandUnits.size()>1){std::erase(g.commandUnits,id);g.commandSelected.reset(id);}
    } else {
        if(ship==g.player)g.commandUnits.insert(g.commandUnits.begin(),id);else g.commandUnits.push_back(id);
        g.commandSelected.set(id);
    }
    g.commandUnit=g.commandUnits.front();g.commandOrder=c_cidDefault;g.refresh();return true;
}
void Game::pickCommandUnits(std::span<const int> ids) {
    auto& g=*impl_;g.commandUnits.clear();g.commandSelected.reset();
    for(int id:ids)if(id>=0 && id<=SHRT_MAX && !g.commandSelected.test(id)) {
        auto* ship=g.mission->GetShip(ShipID(id));
        if(ship && g.player && IsideIGC::AlliedSides(ship->GetSide(),g.player->GetSide())) {
            if(ship==g.player)g.commandUnits.insert(g.commandUnits.begin(),id);else g.commandUnits.push_back(id);
            g.commandSelected.set(id);
        }
    }
    g.commandUnit=g.commandUnits.empty()?-1:g.commandUnits.front();g.commandOrder=c_cidDefault;g.refresh();
}
std::span<const int> Game::commandUnits()const{return impl_->commandUnits;}
bool Game::takeMeTo(int id) {Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;if(!g.onTeam() || id<0 || id>=shipIdentityCapacity)return false;
    auto data=playerPaneData();auto row=std::find_if(data.rows.begin(),data.rows.end(),[&](auto& r){return r.id==id;});if(row==data.rows.end())return false;
    g.pendingCommandFocus.reset();
    // PickShip selects a recipient even when the camera cannot visit that ship.
    pickCommandUnit(id);
    auto* cluster=row->sector>=0?g.mission->GetCluster(SectorID(row->sector)):nullptr;
    auto* ownCluster=g.player->GetSourceShip()->GetCluster();
    if(!cluster || !g.knownSector(cluster) || (ownCluster && ownCluster!=cluster))return true;
    if(!g.inCommandView())commandView(true);if(!g.inCommandView() || !viewSector(cluster->GetObjectID()))return false;
    const Vector* position=row->current?g.commandFocusPosition(id,row->identity,cluster):nullptr;
    g.positionCommand(position,2);
    if(!position && row->current && g.gameRole==GameRole::Replica)g.pendingCommandFocus=Impl::CommandFocus{id,cluster->GetObjectID(),row->identity};
    g.refresh();return true;
}
void Game::commandOrder(int command){auto& g=*impl_;if(command>=0 && command<c_cidMax){g.commandOrder=CommandID(command);g.refresh();}}
bool Game::commandTarget(int type,int id){Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;if(!g.inCommandView())return false;bool accepted=false;
    for(int ship:g.commandUnits)accepted=order(ship,type,id,g.commandOrder)||accepted;
    return accepted;
}
bool Game::commandPosition(glm::vec3 position) {if(impl_->gameRole==GameRole::Replica){bool sent=false;for(int id:impl_->commandUnits)sent=impl_->submit(Net::Operation::Position,{id,impl_->commandOrder,impl_->viewCluster?impl_->viewCluster->GetObjectID():-1},position)||sent;return sent;}if(impl_->gameplayClosed())return false;Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;
    if(!g.inCommandView() || g.commandUnits.empty() || !g.viewCluster ||
       !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) || glm::length(position)>20000)return false;
    DataBuoyIGC data{};data.position=vec(position);data.clusterID=g.viewCluster->GetObjectID();data.buoyID=g.mission->GenerateNewBuoyID();data.type=c_buoyWaypoint;data.visible=true;
    auto* buoy=static_cast<IbuoyIGC*>(g.mission->CreateObject(g.mission->GetLastUpdate(),OT_buoy,&data,sizeof(data)));if(!buoy)return false;
    // IGC hides unconsumed buoys. Hold the provisional order's consumer until
    // SetCommand has installed the ship's consumers (or destroys it on failure).
    buoy->AddConsumer();bool accepted=commandTarget(OT_buoy,buoy->GetObjectID());
    buoy->ReleaseConsumer();buoy->Release();return accepted;
}

std::vector<OrderTarget> Game::orderTargets(int id,int command)const {
    const auto& g=*impl_;std::vector<OrderTarget> result;
    if(!g.player || command<0 || command>=c_cidMax)return result;
    auto* ship=g.mission->GetShip(ShipID(id));if(!ship || ship->GetParentShip() || !ship->GetBaseHullType() || ship->GetSide()!=g.player->GetSide())return result;
    // Enumerate on demand when issuing a command, not in the frame loop. Legal
    // targets include known objects in remote sectors, using IGC's command rules.
    for(auto* l=g.mission->GetClusters()->first();l;l=l->next()) {
        auto* cluster=l->data();
        for(auto* m=cluster->GetPickableModels()->first();m;m=m->next()) {
            auto* target=m->data();
            if(g.seen(target) && ship->LegalCommand(Allegiance::ResolveDefaultOrder(ship,target,CommandID(command)),target))
                {
                auto* history=static_cast<Impl::Thing*>(target->GetThingSite())->stationObservation(g.player->GetSide());
                result.push_back({target->GetObjectID(),target->GetObjectType(),cluster->GetObjectID(),history?history->name:GetModelName(target),cluster->GetName()});
            }
        }
    }
    if(ship->GetPilotType()==c_ptBuilder && (command==c_cidDefault || command==c_cidBuild || command==c_cidGoto))
        for(auto& rock:g.rememberedRocks)if((rock.sides&(1u<<ship->GetSide()->GetObjectID())) && (rock.capabilities&ship->GetOrdersABM())==ship->GetOrdersABM())
            result.push_back({rock.id,OT_asteroid,rock.sector,rock.name,g.mission->GetCluster(rock.sector)->GetName()});
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return std::tie(a.sectorId,a.type,a.name,a.id)<std::tie(b.sectorId,b.type,b.name,b.id);});
    return result;
}
#include "chat_console_game.inl"
#include "player_pane_game.inl"
#include "game_state_game.inl"

std::vector<PartChoice> Game::partChoices(PartKind kind,int mount)const {
    auto& g=*impl_;std::vector<PartChoice> result;if(!g.player)return result;
    if(g.player->GetParentShip() || mount < -c_maxCargo || (mount>=0 && (kind==PartKind::Pack || mount>=(kind==PartKind::Weapon?g.player->GetHullType()->GetMaxWeapons():1))))return result;
    auto* hull=g.player->GetHullType();for(auto* l=g.mission->GetPartTypes()->first();l;l=l->next()) {
        auto* part=l->data();if((mount<0 || part->GetEquipmentType()==EquipmentType(kind)) && hull->CanMount(part,Mount(mount)) && ((!g.economy() && !g.training) || (g.player->GetStation() && g.player->GetStation()->CanBuy(part) && !g.player->GetStation()->IsObsolete(part))))result.push_back({part->GetObjectID(),part->GetName(),part->GetPrice()});
    }
    std::sort(result.begin(),result.end(),[](auto& a,auto& b){return a.name<b.name;});
    if(mount<0)result.insert(result.begin(),{-1,"Empty this cargo slot"});return result;
}
bool Game::replacePart(PartKind kind,int mount,int id) {if(impl_->gameRole==GameRole::Replica)return impl_->submit(Net::Operation::ReplacePart,{int(kind),mount,id});if(impl_->gameplayClosed())return false;Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;if(!g.player || g.player->IsGhost() || g.player->GetParentShip() || g.state.dead || !g.player->GetStation())return false;
    if(g.player->GetParentShip() || mount < -c_maxCargo || (mount>=0 && (kind==PartKind::Pack || mount>=(kind==PartKind::Weapon?g.player->GetHullType()->GetMaxWeapons():1))))return false;
    if(mount<0 && id==-1) {if(auto* old=g.player->GetMountedPart(NA,Mount(mount))){if(g.economy())g.balance(g.player)+=old->GetPrice();old->Terminate();}g.rememberLoadout(g.player);g.refresh();return true;}
    auto* type=g.mission->GetPartType(PartID(id));if(!type || (mount>=0 && type->GetEquipmentType()!=EquipmentType(kind)) || !g.player->GetHullType()->CanMount(type,Mount(mount)))return false;
    if(g.training && (!g.player->GetStation()->CanBuy(type) || g.player->GetStation()->IsObsolete(type)))return false;
    auto* old=g.player->GetMountedPart(mount<0?NA:EquipmentType(kind),Mount(mount));
    if(g.economy()) {
        if(!g.player->GetStation()->CanBuy(type))return false;
        Money& money=g.balance(g.player);Money budget=money+(old?old->GetPrice():0);
        if(budget<type->GetPrice())return false;if(old)old->Terminate();
        if(!Allegiance::BuyPartOnBudget(g.player,type,Mount(mount),&budget))return false;
        money=budget;auto* part=g.player->GetMountedPart(mount<0?NA:EquipmentType(kind),Mount(mount));
        part->SetMountedFraction(1);part->Arm();g.warmPart(part);g.rememberLoadout(g.player);g.refresh();return true;
    }
    if(old)old->Terminate();
    auto* part=g.player->CreateAndAddPart(type,Mount(mount),0x7fff);if(!part)return false;part->SetMountedFraction(1);part->Arm();
    g.warmPart(part);g.rememberLoadout(g.player);
    g.replenish(g.player);g.refresh();return true;
}
bool Game::returnToBase(){if(impl_->gameRole==GameRole::Replica)return impl_->submit(Net::Operation::ReturnToBase,{});Allegiance::SimulationRandomScope randomScope(impl_->randomState);return impl_->returnToBase();}
int Game::addLocalPilot(const std::string& name,int sideId,int hullId) {if(impl_->gameRole==GameRole::Replica)return -1;if(impl_->gameplayClosed())return -1;Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;if(g.mission->GetShips()->n()>=shipIdentityCapacity)return -1;auto* side=g.mission->GetSide(SideID(sideId));if(!side || !side->GetActiveF())return -1;
    IstationIGC* station=nullptr;
    for(auto* l=side->GetStations()->first();l;l=l->next())if(l->data()->GetStationType()->HasCapability(c_sabmRestart)){station=l->data();break;}
    if(!station)return -1;
    auto* hull=hullId<0?g.startingHull(side):g.mission->GetHullType(HullID(hullId));
    if(!hull || hull->HasCapability(c_habmLifepod) || (g.economy() && (!station->CanBuy(hull) || station->IsObsolete(hull) || hull->GetPrice()>(side==g.player->GetSide()?g.balance(g.player):g.funds[sideId]))))return -1;
    auto* pilot=g.ship(name.c_str(),SideID(sideId),hull->GetObjectID(),false);
    if(g.economy()) {
        if(side==g.player->GetSide())g.balance(g.player)-=hull->GetPrice();else g.funds[sideId]-=hull->GetPrice();
        g.distributeIncome(side);
    }
    pilot->SetStation(station);int id=pilot->GetObjectID();pilot->Release();g.economyDirty=true;g.updateEconomy();g.refresh();return id;
}
bool Game::removeLocalPilot(int id) {if(impl_->gameRole==GameRole::Replica)return false;if(impl_->state.complete)return false;Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;auto* pilot=g.mission->GetShip(ShipID(id));
    if(!g.economy() || !pilot || pilot==g.player || !g.accounts.contains(pilot->GetObjectID()))return false;
    TRef<IshipIGC> keep=pilot;TRef<IsideIGC> side=pilot->GetSide();
    if(auto* child=pilot->GetChildShips()->first())g.promoteCrew(child->data());
    SideID flag=pilot->GetFlag();
    if(flag!=NA && !Allegiance::DropShipFlag(pilot,g.mission->GetLastUpdate(),pilot->GetPosition(),[](){return Vector::RandomDirection();}))g.respawnFlag(flag);
    g.retireAccount(pilot,pilot->GetValue());pilot->Terminate();
    if(!Allegiance::MissionHasPlayers(g.mission,side,false,[](IshipIGC* ship){return ship->GetPilotType()>=c_ptPlayer;})) {
        g.deactivateSide(side);if(auto* winner=Allegiance::AlliedSurvivor(g.mission)){g.state.complete=true;g.state.endgame.winner=winner->GetObjectID();g.state.result=std::string(winner->GetName())+" wins";}
    }
    g.economyDirty=true;g.updateEconomy();g.refresh();return true;
}
int Game::pilotBalance(int id)const {
    auto& g=*impl_;auto* ship=g.mission->GetShip(ShipID(id));return ship?g.readBalance(ship):0;
}
bool Game::donateMoney(int id,int amount) {if(impl_->gameRole==GameRole::Replica)return impl_->submit(Net::Operation::Donate,{id,amount});if(impl_->gameplayClosed())return false;
    if(id<0 || id>SHRT_MAX)return false;
    auto& g=*impl_;auto* recipient=g.mission->GetShip(ShipID(id));
    if(!g.economy() || amount<=0 || !recipient || recipient==g.player || !g.accounts.contains(recipient->GetObjectID()) ||
       !IsideIGC::AlliedSides(g.player->GetSide(),recipient->GetSide()) || g.balance(g.player)<amount)return false;
    g.moneyTransfer(g.player,recipient,amount);
    if(recipient->GetSide()!=g.player->GetSide())g.PostText(false,"You gave %s $%d. You now have $%d.",recipient->GetName(),amount,g.readBalance(g.player));
    g.economyDirty=true;g.updateEconomy();g.refresh();return true;
}
bool Game::setAutoDonate(int id,int amount) {if(impl_->gameRole==GameRole::Replica)return impl_->submit(Net::Operation::AutoDonate,{id,amount});if(impl_->gameplayClosed())return false;
    auto& g=*impl_;auto* requested=id<0?nullptr:g.mission->GetShip(ShipID(id));
    if(amount==-1) {
        if(!g.economy() || !requested || requested==g.player || requested->GetSide()!=g.player->GetSide() ||
           !g.accounts.contains(id) || g.commanders[g.player->GetSide()->GetObjectID()]!=g.player->GetObjectID())return false;
        auto* target=requested->GetAutoDonate()?nullptr:Allegiance::AutoDonationTarget(requested,g.player,[&](IshipIGC* ship){g.setDonation(ship,nullptr);});
        g.setDonation(requested,target);g.updateCommander(g.player->GetSide());g.economyDirty=true;g.updateEconomy();g.refresh();return true;
    }
    if(!g.economy() || amount<0 || amount>g.balance(g.player) ||
       (id>=0 && (!requested || requested==g.player || requested->GetSide()!=g.player->GetSide() || !g.accounts.contains(requested->GetObjectID()))))return false;
    auto* recipient=Allegiance::AutoDonationTarget(g.player,requested,[&](IshipIGC* ship){g.setDonation(ship,nullptr);});
    if(recipient && amount){g.balance(g.player)-=amount;g.balance(recipient)+=amount;}
    g.setDonation(g.player,recipient);g.updateCommander(g.player->GetSide());g.economyDirty=true;g.updateEconomy();g.refresh();return true;
}
bool Game::requestMoney(int amount,int hull) {if(impl_->gameRole==GameRole::Replica)return impl_->submit(Net::Operation::Request,{amount,hull});if(impl_->gameplayClosed())return false;
    if(hull< -1 || hull>SHRT_MAX)return false;
    auto& g=*impl_;if(!g.economy() || amount<=0 || (hull>=0 && !g.mission->GetHullType(HullID(hull))))return false;
    auto* recipient=Allegiance::MoneyRequestRecipient(g.player,Money(amount),[&](IshipIGC* ship){return g.readBalance(ship);});
    if(!recipient || !g.accounts.contains(recipient->GetObjectID()))return false;
    g.queueMoneyRequest(recipient,g.player,amount,HullID(hull));
    g.economyDirty=true;g.updateEconomy();g.refresh();return true;
}
bool Game::respondMoneyRequest(bool accept) {if(impl_->gameRole==GameRole::Replica)return impl_->submit(Net::Operation::Respond,{int(accept)});if(impl_->gameplayClosed())return false;
    auto& g=*impl_;auto i=g.moneyRequests.find(g.player->GetObjectID());if(i==g.moneyRequests.end())return false;
    if(accept)return g.acceptCommand(false);
    g.rejectCommand();return true;
}
bool Game::setWing(int id,int wing) {if(impl_->gameRole==GameRole::Replica)return impl_->submit(Net::Operation::Wing,{id,wing});if(impl_->gameplayClosed() || id<0 || id>SHRT_MAX)return false;
    auto& g=*impl_;auto* pilot=g.mission->GetShip(ShipID(id));
    if(wing<0 || wing>=c_widMax || !pilot || pilot->GetSide()!=g.player->GetSide() ||
       (pilot!=g.player && g.commanders[pilot->GetSide()->GetObjectID()]!=g.player->GetObjectID()))return false;
    pilot->SetWingID(WingID(wing));g.economyDirty=true;g.updateEconomy();g.refresh();return true;
}
bool Game::handOverCommand(int id,bool transferFunds) {if(impl_->gameRole==GameRole::Replica)return impl_->submit(transferFunds?Net::Operation::Commander:Net::Operation::Leader,{id});if(impl_->gameplayClosed() || id<0 || id>SHRT_MAX)return false;
    auto& g=*impl_;auto* pilot=g.mission->GetShip(ShipID(id));
    if(!g.economy() || !pilot || pilot==g.player || pilot->GetSide()!=g.player->GetSide() ||
       !g.accounts.contains(pilot->GetObjectID()) || g.commanders[pilot->GetSide()->GetObjectID()]!=g.player->GetObjectID())return false;
    if(transferFunds){g.setDonation(pilot,nullptr);Money amount=g.balance(g.player);g.balance(g.player)=0;g.balance(pilot)+=amount;g.setDonation(g.player,pilot);}
    pilot->SetWingID(0);g.changeCommander(pilot->GetSide(),pilot);
    g.economyDirty=true;g.updateEconomy();g.refresh();return true;
}
bool Game::controlLocalPilot(int id) {if(impl_->gameRole==GameRole::Replica)return false;if(impl_->state.complete)return false;Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;auto* pilot=g.mission->GetShip(ShipID(id));
    if(!pilot || pilot->GetSide()->GetObjectID()<0 || (g.economy() && !pilot->GetSide()->GetActiveF()) || pilot->GetPilotType()<c_ptPlayer || pilot->IsGhost() || (!pilot->GetBaseHullType() && !pilot->GetParentShip()))return false;
    if(g.player)g.player->SetStateM(0);
    g.player=pilot;g.autopilotInputInitialized=false;g.previousActionButtons=0;g.throttleResetPending=false;g.target=pilot->GetCommandTarget(c_cmdCurrent);g.targetRock=g.targetRockSector=-1;g.state.dead=false;g.selectedWeapon=0;g.groupFire=true;
    g.selectedHull=pilot->GetSourceShip()->GetBaseHullType()->GetObjectID();
    g.resetCamera();g.engineLevelsValid=false;g.soundNotifications={};g.notificationTime=-1;
    g.economyDirty=true;g.updateEconomy();g.refresh();return true;
}
std::vector<CrewChoice> Game::crewChoices()const {
    auto& g=*impl_;std::vector<CrewChoice> result;if(!g.player)return result;
    auto* source=g.player->GetSourceShip();
    for(auto* l=g.player->GetSide()->GetShips()->first();l;l=l->next()) {
        auto* pilot=l->data();if(pilot->GetPilotType()<c_ptPlayer || pilot->IsGhost())continue;
        auto* hull=pilot->GetSourceShip()->GetBaseHullType();auto* station=pilot->GetStation();auto* sector=pilot->GetCluster();
        result.push_back({pilot->GetObjectID(),pilot->GetTurretID(),pilot->GetName(),hull?hull->GetName():"",
            station?station->GetName():sector?sector->GetName():"Unavailable",
            Allegiance::CanBoardShip(g.player,pilot) && Allegiance::NextTurret(g.player,pilot)!=NA,
            pilot==source,pilot->GetParentShip()==g.player});
    }
    return result;
}
std::string Game::pilotName(int id)const{
    if(id<0 || id>=shipIdentityCapacity)return {};
    if(impl_->gameRole==GameRole::Replica){
        for(const auto& member:impl_->state.networkPilots)if(member.id==id)return member.name;
        if(std::find(impl_->pilotWingIds.begin(),impl_->pilotWingIds.end(),id)!=impl_->pilotWingIds.end())
            if(const auto* intel=impl_->ownShipStatus(id))return intel->name;
        return {};
    }
    auto* ship=impl_->mission->GetShip(ShipID(id));return ship?ship->GetName():"";
}
bool Game::boardShip(int id,bool observer,bool disembarkFirst) {if(id<0 || id>=shipIdentityCapacity)return false;if(impl_->gameRole==GameRole::Replica)return impl_->submit(Net::Operation::Board,{id,int(disembarkFirst)});if(impl_->gameplayClosed())return false;Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;auto* parent=g.mission->GetShip(ShipID(id));
    if(!Allegiance::CanBoardShip(g.player,parent))return false;
    if(disembarkFirst && (g.player->GetCluster() || !g.player->GetStation() || parent->GetCluster() || !parent->GetStation()))return false;
    if(observer && g.economy())return false; // Original observer mode requires cheat permission.
    bool releaseSeat=disembarkFirst && g.player->GetParentShip();
    Mount turret=observer?NA:releaseSeat?Allegiance::DockedBoardTurret(g.player,parent):Allegiance::NextTurret(g.player,parent);if(!observer && turret==NA)return false;
    while(auto* l=g.player->GetChildShips()->first())if(!g.leaveDockedShip(l->data()))return false;
    if(releaseSeat){if(!g.leaveDockedShip(g.player))return false;}
    else if(g.player->GetParentShip() && g.player->GetParentShip()!=parent)g.crewBoardFeedback(g.player,nullptr);
    if(g.economy())g.balance(g.player)+=g.player->GetValue();
    g.player->SetParentShip(parent);g.player->SetTurretID(turret);
    if(parent->GetStation())g.player->SetStation(parent->GetStation());
    g.player->SetOrientation(turret==NA?parent->GetOrientation():parent->GetHullType()->GetWeaponOrientation(turret)*parent->GetOrientation());
    g.player->SetStateM(0);g.player->SetAutopilot(false);g.selectedWeapon=0;g.groupFire=true;
    g.resetCamera();g.engineLevelsValid=false;g.economyDirty=true;g.updateEconomy();
    g.crewBoardFeedback(g.player,parent);
    if(parent->GetStation()){
        Allegiance::CrewPassengerFeedback(g.player->GetName(),turret==NA,
            [&](bool critical,const char* format,auto... args){g.PostNotificationText(parent,critical,format,args...);});
        g.playPilotSound(boardSound,parent);
    }
    g.refresh();return true;
}
std::string Game::boardingFailure(int id) {
    auto& g=*impl_;if(!g.player || g.player->GetCluster())return {};
    bool vacant=false;
    if(g.gameRole==GameRole::Replica){
        const IhullTypeIGC* hull=nullptr;int manned=0;
        for(const auto& member:g.state.networkPilots)if(const auto* intel=g.ownMemberStatus(member)){
            if(member.id==id)hull=g.mission->GetHullType(intel->status.hull);
        }
        if(!hull && std::find(g.pilotWingIds.begin(),g.pilotWingIds.end(),id)!=g.pilotWingIds.end())
            if(const auto* intel=g.ownShipStatus(id))hull=g.mission->GetHullType(intel->status.hull);
        g.reportedGunners([&](int parent){if(parent==id)++manned;});
        vacant=hull && manned<hull->GetMaxWeapons()-hull->GetMaxFixedWeapons();
    }else{
        auto* parent=id>=0 && id<shipIdentityCapacity?g.mission->GetShip(ShipID(id)):nullptr;
        auto* hull=parent?parent->GetBaseHullType():nullptr;
        vacant=hull && Allegiance::MannedTurrets(parent)<hull->GetMaxWeapons()-hull->GetMaxFixedWeapons();
    }
    // TeleportPane::OnBoardFailed tests reported occupancy, even when a free
    // slot has no equipped gun. Keep its original message/sound distinction.
    if(vacant) {
        g.PlaySoundEffect(noTurretsSound);return "Teleportation canceled - no turret positions were open.";
    }
    return "Teleportation failed.";
}
bool Game::leaveShip() {if(impl_->gameRole==GameRole::Replica)return impl_->submit(Net::Operation::Leave,{});if(impl_->gameplayClosed())return false;Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;if(!g.player || !g.leaveDockedShip(g.player))return false;
    g.player->SetStateM(0);g.resetCamera();g.engineLevelsValid=false;
    g.economyDirty=true;g.updateEconomy();g.refresh();
    if(g.gameRole==GameRole::Offline)restoreDockedLoadout(true);
    return true;
}
int Game::shipValue()const{return impl_->player->GetValue();}
std::vector<TeleportChoice> Game::teleportChoices()const {
    const auto& g=*impl_;std::vector<TeleportChoice> result;
    if(!g.player || !g.player->GetStation() || g.state.complete || g.training)return result;
    for(auto* l=g.mission->GetStations()->first();l;l=l->next()){
        auto* station=l->data();if(!Allegiance::CanTeleportToStation(g.player->GetSide(),station))continue;
        result.push_back({station->GetObjectID(),station->GetName(),station->GetCluster()->GetName(),station==g.player->GetStation(),!g.player->GetParentShip() && g.player->GetBaseHullType() && station->CanBuy(g.player->GetBaseHullType())});
    }
    return result;
}
std::vector<TeleportChoice> Game::teleportDestinations()const {
    const auto& g=*impl_;std::vector<TeleportChoice> result;
    if(!g.player || !g.onTeam() || !g.player->GetStation() || g.state.complete || g.training)return result;
    auto* side=g.player->GetSide();
    // TeleportPane::RebuildList lists docked player ships with turrets first,
    // including fully occupied ships. Authority revalidates the chosen seat.
    if(g.gameRole==GameRole::Replica){
        // PlayerInfo/LastSeenState drives the original pane, independently of
        // physical interest. Count reported gunners once, excluding observers,
        // rather than scanning the crew per destination.
        std::unordered_map<int,int> manned;
        g.reportedGunners([&](int parent){++manned[parent];});
        auto append=[&](int id,const char* name,const Net::ShipIntel* intel){
            if(!intel || id==g.player->GetObjectID() || intel->status.state!=Allegiance::PlayerShipStatus::Docked)return;
            auto* hull=g.mission->GetHullType(intel->status.hull);if(!hull)return;
            int turrets=hull->GetMaxWeapons()-hull->GetMaxFixedWeapons();if(!turrets)return;
            auto* cluster=g.mission->GetCluster(intel->status.sector);
            TeleportChoice row{id,name,cluster?cluster->GetName():"unknown",id==g.state.parentId,
                bool(g.player->GetParentShip()) || (g.player->GetBaseHullType() && g.player->GetBaseHullType()->HasCapability(c_habmLifepod))};
            row.boarding=true;row.ship=hull->GetName();row.turrets=turrets;
            if(auto found=manned.find(id);found!=manned.end())row.manned=found->second;
            result.push_back(std::move(row));
        };
        std::bitset<shipIdentityCapacity> publicMembers;
        for(const auto& member:g.state.networkPilots){
            if(member.id>=0 && member.id<shipIdentityCapacity)publicMembers.set(member.id);
            append(member.id,member.name.c_str(),g.ownMemberStatus(member));
        }
        // Active private accounts also cover local pilot fixtures and pilots
        // retained during reconnect grace, which need not have a live peer.
        for(auto id:g.pilotWingIds)if(!publicMembers.test(id))if(const auto* intel=g.ownShipStatus(id))append(id,intel->name,intel);
    }else for(auto* l=side->GetShips()->first();l;l=l->next()){
        auto* ship=l->data();auto* hull=ship->GetBaseHullType();auto* station=ship->GetStation();
        if(ship==g.player || ship->GetParentShip() || ship->GetPilotType()<c_ptPlayer || ship->IsGhost() || !hull || !station || ship->GetCluster())continue;
        int turrets=hull->GetMaxWeapons()-hull->GetMaxFixedWeapons();if(!turrets)continue;
        TeleportChoice row{ship->GetObjectID(),ship->GetName(),station->GetCluster()->GetName(),ship==g.player->GetParentShip(),
            bool(g.player->GetParentShip()) || (g.player->GetBaseHullType() && g.player->GetBaseHullType()->HasCapability(c_habmLifepod))};
        row.boarding=true;row.ship=hull->GetName();row.turrets=turrets;row.manned=Allegiance::MannedTurrets(ship);result.push_back(std::move(row));
    }
    for(auto* l=side->GetStations()->first();l;l=l->next()){
        auto* station=l->data();if(!station->GetBaseStationType()->HasCapability(c_sabmRestart))continue;
        result.push_back({station->GetObjectID(),station->GetName(),station->GetCluster()->GetName(),station==g.player->GetStation(),
            station==g.player->GetStation() || (g.player->GetBaseHullType() && station->CanBuy(g.player->GetBaseHullType()))});
    }
    return result;
}
bool Game::teleportStation(int id) {if(impl_->gameRole==GameRole::Replica)return impl_->submit(Net::Operation::Teleport,{id});Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;IshipIGC* ship=g.player;auto* station=g.mission->GetStation(StationID(id));
    if(!ship || !ship->GetStation() || ship->GetCluster() || g.state.complete || g.training ||
       !Allegiance::CanTeleportToStation(ship->GetSide(),station) || (ship->GetStation()==station && !ship->GetParentShip()))return false;
    bool disembarked=ship->GetParentShip()!=nullptr;
    if(ship->GetParentShip() && !g.leaveDockedShip(ship))return false;
    bool newHull=Allegiance::ApplyStationTeleport(ship,station,!station->CanBuy(ship->GetBaseHullType()),
        [&](IshipIGC* child){g.leaveDockedShip(child);},
        [&](Money value){if(g.economy())g.balance(ship)+=value;},[](IhullTypeIGC*){});
    if(station->GetSide()==ship->GetSide())g.lastOwnStations[ship->GetObjectID()]=station->GetObjectID();
    // TELEPORT_ACK changes station membership without changing an observer's
    // viewed sector. WinTrek's old-station/new-station callback is otherwise NYI.
    g.commandViewEnabled=ship->IsGhost();if(!g.commandViewEnabled)g.commandCluster=nullptr;ship->SetAutopilot(false);ship->SetStateM(0);
    g.resetCamera();g.engineLevelsValid=false;g.selectedWeapon=0;g.groupFire=true;
    g.economyDirty=true;g.updateEconomy();g.refresh();
    // TELEPORT_ACK requests the original cached/default purchase if this base
    // cannot retain the hull. Authority has already refunded the old ship.
    if((newHull || disembarked) && g.gameRole==GameRole::Offline)restoreDockedLoadout(true);
    g.PlaySoundEffect(personalJumpSound);g.refresh();return true;
}
bool Game::swapTurret(int index) {
    auto& g=*impl_;if(!g.player || g.player->GetParentShip() || !g.player->GetCluster())return false;
    auto mount=Allegiance::TurretShortcutMount(g.player->GetBaseHullType(),index);return mount!=NA && swapCargo(PartKind::Weapon,mount);
}
bool Game::promoteTurret(int index) {
    auto& g=*impl_;if(!g.player || g.player->GetParentShip())return false;
    auto mount=Allegiance::TurretShortcutMount(g.player->GetBaseHullType(),index);if(mount==NA)return false;
    for(auto* child=g.player->GetChildShips()->first();child;child=child->next())if(child->data()->GetTurretID()==mount)return promoteGunner(child->data()->GetObjectID());
    return false;
}
bool Game::promoteGunner(int id) {if(impl_->gameRole==GameRole::Replica)return impl_->submit(Net::Operation::Promote,{id});if(impl_->gameplayClosed())return false;Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;auto* child=g.mission->GetShip(ShipID(id));
    if(!g.player || !child || child->GetParentShip()!=g.player || child->GetTurretID()==NA)return false;
    g.promoteCrew(child);g.player->SetStateM(0);child->SetStateM(0);child->SetAutopilot(false);
    g.player->SetOrientation(child->GetHullType()->GetWeaponOrientation(g.player->GetTurretID())*child->GetOrientation());
    g.rememberLoadout(child);g.resetCamera();g.engineLevelsValid=false;g.economyDirty=true;g.updateEconomy();
    g.refresh();return true;
}
void Game::launch(){if(impl_->gameRole==GameRole::Replica){rememberEditorLoadout(currentLoadout());impl_->submit(Net::Operation::Launch,{});return;}Allegiance::SimulationRandomScope randomScope(impl_->randomState);auto& g=*impl_;if(!g.state.complete && g.player && !g.player->IsGhost() && !g.player->GetParentShip() && g.player->GetStation()){
    TRef<IstationIGC> station=g.player->GetStation();g.player->SetAutopilot(false);g.player->SetStateM(0);
    ControlData controls{};controls.jsValues[c_axisThrottle]=-1;g.player->SetControls(controls);
    g.resetCamera();g.rememberLoadout(g.player);g.player->SetStation(nullptr);g.player->SetBB(g.mission->GetLastUpdate(),g.mission->GetLastUpdate(),0);
    g.cameraMotion.SetJiggle(0);g.engineLevelsValid=false;
    g.refresh();}}
const GameState& Game::state()const{return impl_->state;}
uint32_t Game::simulationRandomState()const{return impl_->randomState;}
std::array<double,10> Game::simulationPhases()const{std::array<double,10> result{};for(auto* l=impl_->mission->GetClusters()->first();l;l=l->next()){const auto& phases=static_cast<CclusterIGC*>(l->data())->m_nativeProfile;for(size_t i=0;i<result.size();++i)result[i]+=phases[i];}return result;}
const CameraState& Game::camera(const CameraInput& input){return impl_->camera(input);}
const std::vector<DrawItem>& Game::scene(bool thirdPerson){return impl_->scene(thirdPerson);}
std::span<const ParticleDraw> Game::particles(){return impl_->particles();}
void Game::particleQuality(int quality){
    auto& g=*impl_;if(!g.fx)return;bool performance=g.fx->performance();g.fx->quality(quality);
    if(performance!=g.fx->performance())for(auto* t:g.things)if(t->model->GetObjectType()==OT_ship){
        t->smokeSize=g.fx->performance()?2.f:4.f;t->fireDuration=g.fx->performance()?.33f:.333f;t->smokeDuration=g.fx->performance()?.66f:2.f;
    }
}
const std::vector<Contact>& Game::contacts()const{impl_->refreshContacts();return impl_->contacts;}
std::span<const SoundEvent> Game::sounds()const{return impl_->sounds;}
std::span<const SoundLoop> Game::shipSounds(bool interior){return impl_->shipSounds(interior);}
std::span<const SoundLoop> Game::worldSounds(bool interior){return impl_->worldSounds(interior);}
void Game::clearSounds(){impl_->sounds.clear();}
void Game::clearMessages(){impl_->state.messages.clear();}
std::vector<std::string_view> Game::radarIcons()const {
    std::vector<std::string_view> names;names.reserve(impl_->icons.size());
    for(auto& [key,icon]:impl_->icons)names.push_back(icon->name);return names;
}
std::vector<std::string_view> Game::inventoryScripts()const {
    std::vector<std::string_view> names;
    for(auto* l=impl_->mission->GetPartTypes()->first();l;l=l->next()) {
        std::string_view name=l->data()->GetInventoryLineMDLName();
        if(std::find(names.begin(),names.end(),name)==names.end())names.push_back(name);
    }
    return names;
}
std::vector<int> Game::soundIds()const {
    std::vector<int> ids={explodeShipSound,myHullHitSound,myShieldHitSound,jumpSound,deployChaffSound,deployMineSound,deployProbeSound,
        SidethrustInteriorSound,SidethrustExteriorSound,CapSidethrustInteriorSound,CapSidethrustExteriorSound,
        mountedSound,startReloadSound,salReloadingAmmoSound,salReloadingFuelSound,salReloadingMissilesSound,
        salReloadingChaffSound,salReloadingDispenserSound,salRepairedAtCarrierSound,pickUpPartSound,salCargoFullSound,newTargetSound,vectorLockSound,acceptCommandSound,groupWeaponsSound,ungroupWeaponsSound,salAutopilotEngageSound,salAutopilotDisengageSound,voAffirmativeSound,voNegativeSound,salMissilesDepletedSound,salChaffDepletedSound,salDispenserEmptySound,asteroidSound,alephSound,buildSound,miningSound,outOfBoundsLoopSound,ripcordOnLoopSound,missileToneSound,missileLockSound,
        hullLowSound,ammoLowSound,fuelLowSound,energyLowSound,salNoAmmoSound,salNoFuelSound,
        constructorRunningSound,droneTooMuchDamageSound,droneEnemyOnScopeSound,droneComingHomeEmptySound,newCommandMsgSound,newPersonalMsgSound,newOfflineChatMsgSound,otherHullHitSound,otherShieldHitSound,largeExplosionSound,mediumExplosionSound,smallExplosionSound,
        dockedSound,salWelcomeHomeSound,personalJumpSound,newCriticalMsgSound,newNonCriticalMsgSound,paydaySound,investorSound,commanderSound,pickUpCashSound,positiveButtonClickSound,
        rescuePlayerSound,salShipCantRipcordSound,salNoRipcordSound,salRipcordAbortedSound,
        carrierDestroyedSound,minerDestroyedSound,minefieldLayerDestroyedSound,towerLayerDestroyedSound,
        minerThreatenedSound,constructorThreatenedSound,carrierThreatenedSound,bomberDetectedSound,carrierDetectedSound,capitalDetectedSound,teleportDetectedSound,transportDetectedSound,stationThreatenedSound,sectorSecuredSound,sectorLostSound,carrierUnderAttackSound,minerCriticalSound,minerUnderAttackSound,droneUnderAttackSound,droneWatchFireSound,
        ripcord1Sound,ripcord2Sound,ripcord3Sound,ripcord4Sound,ripcord5Sound,ripcord6Sound,ripcord7Sound,ripcord8Sound,ripcord9Sound,ripcord10Sound};
    ids.insert(ids.end(),{salShieldsOfflineSound,rockHitSound,turretLimitSound,collisionSound,salCloakEngageSound,salCloakDisengageSound,
        droneInTransitSound,droneTooCrowdedSound,droneWhereToSound,droneWhereToLayMinefieldSound,droneWhereToLayTowerSound,
        artifactSecuredSound,enemyFlagSecuredSound,artifactFoundSound,enemyFlagLostSound,flagLostSound,flagSecuredSound,salReloadingSound,mountSound,unmountSound,dropSound});
    ids.insert(ids.end(),std::begin(Allegiance::ClientEventSoundIds),std::end(Allegiance::ClientEventSoundIds));
    // Warm all sound-bearing definitions in this core once, including future
    // construction/upgrades. No decoder or repeat-fire mix runs on first use.
    for(auto* l=impl_->mission->GetStationTypes()->first();l;l=l->next()) {
        auto* t=l->data();ids.insert(ids.end(),{t->GetExteriorSound(),t->GetInteriorSound(),t->GetInteriorAlertSound(),t->GetCompletionSound(),
            t->GetUnderAttackSound(),t->GetCriticalSound(),t->GetDestroyedSound(),t->GetCapturedSound(),t->GetEnemyCapturedSound(),t->GetEnemyDestroyedSound(),
            t->GetConstructorNeedRockSound(),t->GetConstructorUnderAttackSound(),t->GetConstructorDestroyedSound()});
    }
    for(auto* l=impl_->mission->GetHullTypes()->first();l;l=l->next()) {
        auto* h=l->data();ids.insert(ids.end(),{h->GetInteriorSound(),h->GetExteriorSound(),h->GetMainThrusterInteriorSound(),h->GetMainThrusterExteriorSound(),h->GetManuveringThrusterInteriorSound(),h->GetManuveringThrusterExteriorSound()});
        for(Mount m=0;m<h->GetMaxWeapons();++m){auto& p=h->GetHardpointData(m);ids.insert(ids.end(),{p.interiorSound,p.turnSound});}
    }
    for(auto* l=impl_->mission->GetPartTypes()->first();l;l=l->next()) {
        auto* part=l->data();
        switch(part->GetEquipmentType()) {
        case ET_Weapon:{auto* d=static_cast<const DataWeaponTypeIGC*>(part->GetData());ids.insert(ids.end(),{d->activateSound,d->singleShotSound,d->burstSound});break;}
        case ET_Shield:{auto* d=static_cast<const DataShieldTypeIGC*>(part->GetData());ids.insert(ids.end(),{d->activateSound,d->deactivateSound});break;}
        case ET_Afterburner:{auto* d=static_cast<const DataAfterburnerTypeIGC*>(part->GetData());ids.insert(ids.end(),{d->interiorSound,d->exteriorSound});break;}
        case ET_Cloak:{auto* d=static_cast<const DataCloakTypeIGC*>(part->GetData());ids.insert(ids.end(),{d->engageSound,d->disengageSound});break;}
        default:break;
        }
    }
    for(auto* l=impl_->mission->GetDevelopments()->first();l;l=l->next())ids.push_back(l->data()->GetCompletionSound());
    for(auto* l=impl_->mission->GetExpendableTypes()->first();l;l=l->next()) {
        auto* t=l->data();
        if(t->GetObjectType()==OT_missileType){auto* m=static_cast<ImissileTypeIGC*>(t);ids.insert(ids.end(),{m->GetLaunchSound(),m->GetAmbientSound()});}
        else if(t->GetObjectType()==OT_probeType)ids.push_back(static_cast<IprobeTypeIGC*>(t)->GetAmbientSound());
    }
    for(auto* l=impl_->mission->GetProjectileTypes()->first();l;l=l->next())ids.push_back(l->data()->GetAmbientSound());
    for(auto* l=impl_->mission->GetShips()->first();l;l=l->next()) {
        auto* ship=l->data();auto* h=ship->GetBaseHullType();if(!h)continue;
        ids.insert(ids.end(),{h->GetInteriorSound(),h->GetExteriorSound(),h->GetMainThrusterInteriorSound(),h->GetMainThrusterExteriorSound(),
            h->GetManuveringThrusterInteriorSound(),h->GetManuveringThrusterExteriorSound()});
        for(auto* p=ship->GetParts()->first();p;p=p->next()) {
            if(p->data()->GetEquipmentType()==ET_Weapon){auto* w=static_cast<IweaponIGC*>(p->data());ids.insert(ids.end(),{w->GetSingleShotSound(),w->GetBurstSound(),w->GetActivateSound()});}
            if(p->data()->GetEquipmentType()==ET_Magazine)ids.push_back(static_cast<ImagazineIGC*>(p->data())->GetMissileType()->GetLaunchSound());
            if(p->data()->GetEquipmentType()==ET_Afterburner) {
                auto* booster=static_cast<IafterburnerIGC*>(p->data());ids.push_back(booster->GetInteriorSound());ids.push_back(booster->GetExteriorSound());
            }
        }
    }
    std::sort(ids.begin(),ids.end());ids.erase(std::unique(ids.begin(),ids.end()),ids.end());return ids;
}
}

#include "training_host.inl"
#include "network_game.inl"
