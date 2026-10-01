// Native authority/replica adapter. The original IGC and shared client policies
// remain the implementation of gameplay; these methods establish ownership.
#include "../Igc/projectileIGC.h"
#include "../Igc/shipIGC.h"
#include "../Igc/missionigc.h"
#include "../Igc/sideigc.h"
#include "../Igc/missileigc.h"
#include "../Igc/chaffigc.h"
#include "../Igc/treasureIGC.h"
namespace Native {
namespace {
Net::Vec wire(const Vector &v) { return {v.x, v.y, v.z}; }
Vector vector(const Net::Vec &v) { return {v.x, v.y, v.z}; }
Net::Motion motion(ImodelIGC *m) {
    const auto& orientation=m->GetOrientation();
    return {wire(m->GetPosition()), wire(m->GetVelocity()), wire(orientation.GetForward()),wire(orientation.GetUp())};
}
void motion(ImodelIGC *m, const Net::Motion &pose) {
    Orientation o;
    o.Set(vector(pose.forward), vector(pose.up));
    m->SetOrientation(o);
    m->SetPosition(vector(pose.position));
    m->SetVelocity(vector(pose.velocity));
}
} // namespace
int Game::joinNetworkPilot(std::string_view name, int sideID, bool first) {
    auto &g = *impl_;
    if (g.gameRole != GameRole::Authority || (sideID < 0 && sideID!=SIDE_TEAMLOBBY) || sideID >= g.mission->GetSides()->n() || name.empty())
        return -1;
    Allegiance::SimulationRandomScope random(g.randomState);
    auto *side = g.mission->GetSide(SideID(sideID));
    if (!side || (sideID>=0 && !side->GetActiveF()))
        return -1;
    if (first && g.player->GetSide() == side) {
        // The initial authority seat is a placeholder until claimed. Its
        // pre-admission simulation time must not become a human's play time.
        g.scoredPilots.erase(g.player->GetObjectID());
        for(auto& leader:g.scoreLeaders)if(leader==g.player->GetObjectID())leader=NA;
        g.player->SetName(std::string(name).c_str());
        g.connectScore(g.player);
        g.scoreLeadership();
        return g.player->GetObjectID();
    }
    IstationIGC *station = nullptr;
    for (auto *l = side->GetStations()->first(); l; l = l->next())
        if (l->data()->GetStationType()->HasCapability(c_sabmRestart)) {
            station = l->data();
            break;
        }
    if (!station && sideID>=0)
        return -1;
    auto *hull = sideID<0?nullptr:g.economy() ? g.startingHull(side) : g.mission->GetHullType(hullIds[g.hullIndex]);
    auto *created = g.ship(std::string(name).c_str(), SideID(sideID), hull?hull->GetObjectID():NA, false);
    created->SetStation(station);
    int id = created->GetObjectID();
    // Economy accounts identify paid hulls, not connected pilots. Practice
    // pilots have no account; a later join must still preserve their seat.
    if (first) {
        TRef<IshipIGC> old = g.player;
        g.player = created;
        g.scoredPilots.erase(old->GetObjectID());
        g.retireAccount(old, old->GetValue());
        old->Terminate();
    }
    created->Release();
    if (g.economy() && sideID>=0)
        g.distributeIncome(side);
    g.economyDirty = true;
    g.updateEconomy();
    g.refresh();
    return id;
}
int Game::teamLeader(int side)const {
    return side>=0 && side<int(impl_->commanders.size())?impl_->commanders[side]:NA;
}
bool Game::setTeamActive(int sideID,bool active){
    auto& g=*impl_;auto params=*g.mission->GetMissionParams();
    if(g.gameRole!=GameRole::Authority || !g.economy() || g.mission->GetMissionStage()!=STAGE_NOTSTARTED || params.bObjectModelCreated || sideID<0 || sideID>=params.nTeams)return false;
    auto* side=g.mission->GetSide(SideID(sideID));
    for(auto* ship=side->GetShips()->first();ship;ship=ship->next())if(ship->data()->GetPilotType()>=c_ptPlayer)return false;
    side->SetActiveF(active);g.options.inactiveTeams=0;
    for(SideID id=0;id<params.nTeams;++id)if(!g.mission->GetSide(id)->GetActiveF())g.options.inactiveTeams|=1u<<id;
    params.bAllowEmptyTeams=g.options.inactiveTeams!=0;g.options.requireTeamPlayers=!params.bAllowEmptyTeams;
    g.mission->SetMissionParams(&params);
    auto previous=missionPolicy(),policy=previous;
    Allegiance::UpdateAlliances(g.options.alliances.data(),params.nTeams,SideID(sideID),NA,
        [&](SideID id){return g.mission->GetSide(id)->GetActiveF();},policy.alliances.data());
    if(policy.alliances!=previous.alliances){
        bool allied=std::any_of(policy.alliances.begin(),policy.alliances.end(),[](char group){return group!=NA;});
        auto rules=Allegiance::LobbyAllianceRules(allied,!params.bAllowAlliedRip);
        policy.imbalance=rules.imbalance;policy.flags=(rules.defections?1:0)|(rules.sight?2:0)|(rules.ripcord?4:0);
        applyMissionPolicy(policy);
    }
    g.economyDirty=true;g.refresh();g.publicationCache.clear();g.publicationIndexValid=false;return true;
}
void Game::startNetworkMission(){
    auto& g=*impl_;if(g.gameRole!=GameRole::Authority)return;
    g.prepareActiveTeams();g.mission->SetMissionStage(STAGE_STARTED);g.updateEconomy();g.refresh();
}
bool Game::setTeamSettings(int sideID,int civilization,std::string_view name) {
    auto& g=*impl_;
    if(g.gameRole!=GameRole::Authority || !g.economy() || g.options.rules.lockTeamSettings ||
       g.mission->GetMissionStage()!=STAGE_NOTSTARTED || sideID<0 || sideID>=g.mission->GetSides()->n() ||
       civilization<0 || civilization>=300 || name.size()>24 ||
       std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32 || c==127;}))return false;
    auto* side=g.mission->GetSide(SideID(sideID));auto* civ=g.mission->GetCivilization(CivID(civilization));
    if(!side || !civ)return false;
    Allegiance::SimulationRandomScope random(g.randomState);
    if(side->GetCivilization()!=civ){
        // FedSrv's original side change updates the base types and technology.
        // Native worlds also contain pregame hulls/miners; refit those without
        // replacing the world, its pilots, connections, or recipient policies.
        side->DestroyBuckets();side->SetCivilization(civ);side->SetInitialTechs(civ->GetBaseTechs());
        if(g.mission->GetMissionParams()->bAllowDevelopments){side->SetGlobalAttributeSet(civ->GetBaseAttributes());side->CreateBuckets();}
        else side->ResetGlobalAttributeSet();
        std::vector<TRef<IshipIGC>> miners;
        for(auto* l=side->GetShips()->first();l;l=l->next()){
            auto* ship=l->data();
            if(ship->GetPilotType()==c_ptMiner){miners.emplace_back(ship);continue;}
            if(ship->GetPilotType()!=c_ptPlayer)continue;
            auto* station=ship->GetStation();
            while(auto* part=ship->GetParts()->first())part->data()->Terminate();
            ship->SetBaseHullType(g.startingHull(side));g.equip(ship);g.rememberLoadout(ship);
            if(station)station->RepairAndRefuel(ship);
            g.accounts[ship->GetObjectID()]=0;
            g.forgetCredits(ship->GetObjectID());
        }
        for(auto& miner:miners)miner->Terminate();
        auto* station=side->GetStations()->first()?side->GetStations()->first()->data():nullptr;
        if(station && g.mission->GetMissionParams()->bAllowDevelopments)
            for(auto* d=g.mission->GetDroneTypes()->first();d;d=d->next())if(d->data()->GetPilotType()==c_ptMiner && station->CanBuy(d->data())){
                for(int i=0;i<g.options.rules.miners;++i)g.createDrone(station,d->data(),nullptr);break;
            }
        g.funds[sideID]=Money(g.mission->GetFloatConstant(c_fcidStartingMoney)*(g.options.rules.startingMoney+civ->GetBonusMoney()));
        g.distributeIncome(side);
    }
    side->SetName(std::string(name).c_str());g.options.teamNames[sideID]=side->GetName();
    int index=0;for(auto* l=g.mission->GetCivilizations()->first();l;l=l->next(),++index)
        if(l->data()==civ){g.options.teamCivilizations[sideID]=index;break;}
    g.economyDirty=true;g.updateEconomy();g.refresh();g.publicationCache.clear();g.publicationIndexValid=false;
    return true;
}
void Game::lobbyTeamResetNotice(int pilot,bool balance) {
    auto& g=*impl_;
    int old=g.player->GetObjectID();
    if(g.activateActor(pilot)){
        Net::Notice notice;notice.flags=Net::NoticeModal;
        g.message(balance?"You have been removed from your team and assigned to another because the teams have been balanced.":
            "You have been removed from your team because the Game Commander flushed all teams.",true,{.85f,.85f,.85f,1},false,false,&notice);
    }
    g.activateActor(old);
}
void Game::lobbyBootNotice(int id,int formerSide,bool mission) {
    auto& g=*impl_;auto* booted=g.mission->GetShip(ShipID(id));if(!booted)return;
    std::string name=booted->GetName();int old=g.player->GetObjectID();
    if(!mission && g.activateActor(id)){
        Net::Notice notice;notice.flags=Net::NoticeModal;
        g.message("You have been booted by your team leader!",true,{.85f,.85f,.85f,1},false,false,&notice);
    }
    // BaseClient::RemovePlayerFromSide reports other human departures only
    // during gameplay, through the existing team chat and quit sound.
    if(g.mission->GetMissionStage()==STAGE_STARTED)if(auto* side=g.mission->GetSide(SideID(formerSide))){
        auto text=name+" was booted from the team by the team leader.";
        for(auto* link=side->GetShips()->first();link;link=link->next())if(auto* pilot=link->data();pilot->GetObjectID()!=id && pilot->GetPilotType()>=c_ptPlayer && g.activateActor(pilot->GetObjectID()))
            g.receiveChat(nullptr,CHAT_TEAM,SideID(formerSide),salQuitSound,text.c_str(),c_cidNone,NA,NA,nullptr,false);
    }
    g.activateActor(old);
}
void Game::lobbyRequestRejected(int pilot,Allegiance::LobbyJoinFailure reason){
    auto& g=*impl_;int old=g.player->GetObjectID();
    if(g.activateActor(pilot)){
        Net::Notice notice;notice.flags=Net::NoticeModal;
        g.message(Allegiance::LobbyJoinFailureMessage(reason),true,{.85f,.85f,.85f,1},false,false,&notice);
    }
    g.activateActor(old);
}
bool Game::moveNetworkPilotToTeam(int id,int sideID) {
    auto& g=*impl_;
    if(g.gameRole==GameRole::Replica || (g.state.complete && sideID!=SIDE_TEAMLOBBY) || id<0 || id>SHRT_MAX || sideID<SIDE_TEAMLOBBY ||
       sideID>=g.mission->GetSides()->n())return false;
    auto* ship=g.mission->GetShip(ShipID(id));
    auto* side=g.mission->GetSide(SideID(sideID));
    if(!ship || ship->GetPilotType()<c_ptPlayer || !side)return false;
    if(ship->GetSide()==side)return true;
    IstationIGC* station=nullptr;
    if(sideID>=0) {
        if(!side->GetActiveF())return false;
        for(auto* l=side->GetStations()->first();l;l=l->next())
            if(l->data()->GetStationType()->HasCapability(c_sabmRestart)){station=l->data();break;}
        if(!station)return false;
    }
    Allegiance::SimulationRandomScope random(g.randomState);
    TRef<IshipIGC> keep=ship;
    int old=g.player->GetObjectID();
    g.activateActor(id);
    // FedSrv promotes a remaining gunner before removing the pilot. The
    // departing pilot must not take the live hull or its other crew away.
    if(auto* child=ship->GetChildShips()->first())g.promoteCrew(child->data());
    if(ship->GetFlag()!=NA && !Allegiance::DropShipFlag(ship,g.mission->GetLastUpdate(),ship->GetPosition(),
                                                       []{return Vector::RandomDirection();}))
        g.respawnFlag(ship->GetFlag());
    if(ship->GetSide()->GetObjectID()>=0)g.resetSideScore(ship);
    g.retireAccount(ship,ship->GetValue());
    ship->Reset(false);
    for(auto& intel:g.shipIntel)intel.erase(ShipID(id));
    if(g.mission->GetMissionStage()!=STAGE_STARTED && g.mission->GetMissionStage()!=STAGE_STARTING)ship->SetExperience(1);
    ship->SetSide(side);g.connectScore(ship);
    ship->SetAutopilot(false);ship->SetStateM(0);ship->SetVelocity(Vector::GetZero());
    ControlData controls;controls.Reset();ship->SetControls(controls);
    g.loadouts.erase(id);g.lastOwnStations.erase(id);
    g.target=nullptr;g.targetRock=g.targetRockSector=-1;
    g.commandViewEnabled=false;g.commandCluster=g.viewCluster=nullptr;
    g.commandUnits.clear();g.commandSelected.reset();g.commandUnit=-1;
    g.selectedWeapon=0;g.selectedCargo=-1;g.groupFire=true;
    g.previousActionButtons=0;g.autopilotInputInitialized=false;g.state.dead=sideID<0;
    g.resetCamera();g.engineLevelsValid=false;
    if(station) {
        if(g.economy()) {
            g.accounts.emplace(id,0);
            auto& commander=g.commanders[sideID];
            if(commander==NA){commander=id;ship->SetWingID(0);}
            else g.setDonation(ship,g.mission->GetShip(commander));
        }
        ship->SetBaseHullType(g.economy()?g.startingHull(side):g.mission->GetHullType(hullIds[g.hullIndex]));
        g.equip(ship);g.rememberLoadout(ship);ship->SetStation(station);
        if(g.economy())g.distributeIncome(side);
    }
    if(old!=id)g.activateActor(old);
    g.economyDirty=true;g.updateEconomy();g.refresh();
    // Side membership invalidates both visibility and personal accounts, even
    // while the lobby clock is stopped at the same publication tick.
    g.publicationCache.clear();g.publicationIndexValid=false;
    return true;
}
void Game::networkPilotConnected(int id,bool connected){
    auto& g=*impl_;if(g.gameRole!=GameRole::Authority || g.scoresFrozen || id<0 || id>SHRT_MAX)return;
    auto* ship=g.mission->GetShip(ShipID(id));auto score=g.scoredPilots.find(ShipID(id));
    if(!ship || score==g.scoredPilots.end() || score->second.online==connected)return;
    score->second.online=connected;
    if(connected)g.connectScore(ship);else g.disconnectScore(ship);
}
void Game::retireNetworkPilot(int id) {
    auto &g = *impl_;
    if (g.gameRole == GameRole::Replica)
        return;
    auto *ship = g.mission->GetShip(ShipID(id));
    if (!ship)
        return;
    // Preserve the flying hull and every remaining crew member on departure.
    if (auto *child = ship->GetChildShips()->first())
        g.promoteCrew(child->data());
    if (ship == g.player) {
        IshipIGC *next = nullptr;
        for (auto *l = g.mission->GetShips()->first(); l; l = l->next())
            if (l->data() != ship && l->data()->GetPilotType() >= c_ptPlayer &&
                g.accounts.contains(l->data()->GetObjectID())) {
                next = l->data();
                break;
            }
        if (next)
            g.activateActor(next->GetObjectID());
        else {
            auto* side=ship->GetSide()->GetObjectID()>=0?ship->GetSide():g.mission->GetSides()->first()->data();
            auto *dummy = g.ship("Server seat", side->GetObjectID(),
                                 side->GetCivilization()->GetLifepod()->GetObjectID(), false);
            dummy->SetDeaths(SHRT_MAX);
            g.scoredPilots.erase(dummy->GetObjectID()); // observer seat is not a departed/connected player
            g.accounts.erase(dummy->GetObjectID());
            g.forgetCredits(dummy->GetObjectID());
            g.player = dummy;
            dummy->Release();
        }
    }
    TRef<IshipIGC> keep = ship;
    if (ship->GetFlag() != NA && !Allegiance::DropShipFlag(ship, g.mission->GetLastUpdate(), ship->GetPosition(),
                                                           []() { return Vector::RandomDirection(); }))
        g.respawnFlag(ship->GetFlag());
    g.retireAccount(ship, ship->GetValue());
    ship->Terminate();
    g.actors.erase(id);
    g.recipientChat.erase(id);
    g.economyDirty = true;
    g.updateEconomy();
    g.refresh();
}
GameRole Game::role() const { return impl_->gameRole; }
void Game::networkActions(std::function<bool(const Net::Action &)> sink) {
    auto& g=*impl_;g.actionSink=std::move(sink);
    if(g.actionSink){g.submit(Net::Operation::ChatPreferences,{int(Allegiance::ChatPreferenceBits(g.chatPrefs)),g.chatPrefs.filterLobby});
        for(const auto& pilot:g.state.networkPilots)if(g.muted(pilot.id))g.submit(Net::Operation::Mute,{pilot.id,1});}
}
const SessionOptions &Game::sessionOptions() const { return impl_->options; }
bool Game::teamImbalance(int value) {
    if(!Allegiance::ValidTeamImbalance(value))return false;
    auto& g=*impl_;g.options.rules.maxImbalance=value;auto params=*g.mission->GetMissionParams();params.iMaxImbalance=short(value);
    g.mission->SetMissionParams(&params);g.state.maxImbalance=value;return true;
}
Net::MissionPolicy Game::missionPolicy(uint64_t match)const {
    const auto& g=*impl_;const auto& params=*g.mission->GetMissionParams();
    Net::MissionPolicy policy{};policy.match=match;policy.imbalance=params.iMaxImbalance;policy.alliances=g.options.alliances;
    policy.flags=(params.bAllowDefections?1:0)|(params.bAllowAlliedViz?2:0)|(params.bAllowAlliedRip?4:0);return policy;
}
bool Game::applyMissionPolicy(const Net::MissionPolicy& policy) {
    auto& g=*impl_;
    if(!Allegiance::ValidTeamImbalance(policy.imbalance) || (policy.flags&~7u) || policy.reserved || policy.reservedTail ||
       std::any_of(policy.alliances.begin(),policy.alliances.end(),[](char group){return group<NA || group>=c_cAlliancesMax;}))return false;
    auto& rules=g.options.rules;rules.maxImbalance=policy.imbalance;g.options.alliances=policy.alliances;
    rules.allowDefections=policy.flags&1;rules.alliedSight=policy.flags&2;rules.alliedRipcord=policy.flags&4;
    auto params=*g.mission->GetMissionParams();params.iMaxImbalance=short(rules.maxImbalance);
    params.bAllowDefections=rules.allowDefections;params.bAllowAlliedViz=rules.alliedSight;params.bAllowAlliedRip=rules.alliedRipcord;
    g.mission->SetMissionParams(&params);
    for(SideID side=0;side<params.nTeams;++side)if(auto* igc=g.mission->GetSide(side))igc->SetAllies(policy.alliances[side]);
    g.state.maxImbalance=rules.maxImbalance;g.state.alliances=policy.alliances;g.state.allowDefections=rules.allowDefections;
    g.state.alliedSight=rules.alliedSight;g.state.alliedRipcord=rules.alliedRipcord;return true;
}
bool Game::changeAlliance(int side,int ally,bool ripcordDisabled) {
    auto& g=*impl_;const auto& params=*g.mission->GetMissionParams();int count=params.nTeams;
    auto active=[&](SideID id){auto* s=g.mission->GetSide(id);return s && s->GetActiveF();};
    bool otherGoals=params.IsArtifactsGame() || params.IsDeathMatchGame() || params.IsFlagsGame() || params.IsProsperityGame() || params.IsTerritoryGame();
    if(g.gameRole!=GameRole::Authority || g.options.scenario!=Scenario::Conquest ||
       !Allegiance::LobbyAlliancesAvailable(true,g.mission->GetMissionStage()==STAGE_NOTSTARTED,otherGoals,count,active) ||
       side<0 || side>=count || !active(SideID(side)) ||
       (ally!=NA && (ally<0 || ally>=count || ally==side || !active(SideID(ally)))))return false;
    auto policy=missionPolicy();
    Allegiance::UpdateAlliances(g.options.alliances.data(),SideID(count),SideID(side),SideID(ally),active,policy.alliances.data());
    bool allied=std::any_of(policy.alliances.begin(),policy.alliances.end(),[](char group){return group!=NA;});
    auto rules=Allegiance::LobbyAllianceRules(allied,ripcordDisabled);
    policy.imbalance=rules.imbalance;policy.flags=(rules.defections?1:0)|(rules.sight?2:0)|(rules.ripcord?4:0);
    return applyMissionPolicy(policy);
}
void Game::resetAuthority(SessionOptions options) {
    auto &g = *impl_;
    if (g.gameRole != GameRole::Authority)
        throw std::logic_error("Only the authority can create a round");
    auto next = std::make_unique<Impl>(g.artworkPath, g.loader, g.effectModel, g.warpModel, std::move(options),
                                       EffectLoader{}, GameRole::Authority);
    next->quickChatCatalog = g.quickChatCatalog;
    next->state.endgame=g.state.endgame;next->state.result=g.state.result;
    next->endgameRevision=next->state.endgame.frozen?1:0;
    impl_ = std::move(next);
}
void Game::authorityStep(std::span<const std::pair<int, FlightInput>> inputs, double dt) {
    auto &g = *impl_;
    if (g.gameRole != GameRole::Authority)
        throw std::logic_error("Not an authoritative game");
    g.authorityInputs.assign(inputs.begin(), inputs.end());
    Allegiance::SimulationRandomScope random(g.randomState);
    g.step({}, dt);
    g.clearNetworkNotices();
}
// Every command is executed as the authenticated pilot, never an ID claimed by
// the requester. Existing methods retain their original authorization checks.
bool Game::authorityAction(int pilot, const Net::Action &action,uint32_t* details,bool echoChatAuthor) {
    if(details)*details=0;
    auto &g = *impl_;
    if (g.gameRole != GameRole::Authority || !Net::terminated(action.text))
        return false;
    int old = g.player->GetObjectID();
    if (!g.activateActor(pilot))
        return false;
    if(!g.onTeam() && action.op!=Net::Operation::Chat && action.op!=Net::Operation::QuickChat && action.op!=Net::Operation::ChatPreferences && action.op!=Net::Operation::Mute) {
        g.activateActor(old);
        return false;
    }
    auto &a = action.args;
    bool ok = true;
    using O = Net::Operation;
    switch (action.op) {
    case O::ChatPreferences:
        ok=a[0]>=0 && a[0]<=15 && a[1]>=1 && a[1]<=3;
        if(ok)g.recipientChat[pilot].preferences={bool(a[0]&1),bool(a[0]&2),bool(a[0]&4),bool(a[0]&8),a[1]};
        break;
    case O::Mute:
        ok=a[0]>=0 && a[0]<=SHRT_MAX && (a[1]==0 || a[1]==1);
        if(ok){auto& muted=g.recipientChat[pilot].muted;std::erase(muted,a[0]);if(a[1])muted.push_back(a[0]);}
        break;
    case O::Launch:
        ok = g.player->GetStation() && !g.player->GetParentShip() && !g.player->IsGhost();
        if(ok)launch();
        break;
    case O::ReturnToBase:
        ok = returnToBase();
        break;
    case O::Respawn:
    case O::Recover:
        ok = g.state.dead && !g.player->IsGhost() && !g.gameplayClosed();
        if (ok)
            recoverToHangar();
        break;
    case O::Eject:
        eject();
        break;
    case O::Ripcord:
        ok = ripcord(a[0]);
        break;
    case O::BuyHull:
        ok = buyHull(a[0]);
        break;
    case O::SelectHull:
        selectHull(a[0]);
        break;
    case O::ReplacePart:
        ok = a[0] >= 0 && a[0] <= int(PartKind::Booster) && replacePart(PartKind(a[0]), a[1], a[2]);
        break;
    case O::Loadout: {
        Loadout l;
        l.hull = a[0];
        if (action.count > action.parts.size()) {
            ok = false;
            break;
        }
        for (size_t i = 0; i < action.count; ++i)
            l.parts.push_back({action.parts[i].id, int8_t(action.parts[i].mount)});
        ok = applyLoadout(l);
        break;
    }
    case O::PurchaseLoadout: {
        if(action.count>action.parts.size() || a[0]<0 || a[0]>SHRT_MAX || (a[1]!=0 && a[1]!=1)){ok=false;break;}
        Loadout requested;requested.hull=int16_t(a[0]);
        for(size_t i=0;i<action.count;++i){auto& p=action.parts[i];if(p.mount < -c_maxCargo || p.mount>=c_maxMountedWeapons){ok=false;break;}requested.parts.push_back({p.id,int8_t(p.mount),p.amount});}
        if(!ok)break;
        auto result=purchaseLoadout(requested,a[1]!=0);ok=result.accepted;
        if(details)*details=uint32_t(result.complete)|(uint32_t(result.launched)<<1);break;
    }
    case O::Invest:
        ok = invest(a[0], a[1]);
        break;
    case O::DroneStandby:
        ok = droneStandby(a[0],a[1]!=0);
        break;
    case O::Order:
        ok = order(a[0], a[1], a[2], a[3]);
        break;
    case O::Position:
        g.commandViewEnabled = true;
        g.commandOrder = CommandID(a[1]);
        g.commandUnits.assign(1, a[0]);
        ok = viewSector(a[2]) && commandPosition({action.position.x, action.position.y, action.position.z});
        break;
    case O::Accept:
        ok = acceptCommand(a[0]);
        break;
    case O::Reject:
        rejectCommand();
        break;
    case O::Clear:
        clearCommand();
        break;
    case O::Autopilot:
        toggleAutopilot();
        break;
    case O::Cloak:
        toggleCloak();
        break;
    case O::Coast:
        toggleVectorLock();
        break;
    case O::Reload:
        ok = reload();
        break;
    case O::Drop:
        selectCargo(a[0]);
        ok = dropCargo();
        break;
    case O::Swap:
        selectCargo(a[2]);
        ok = a[0] >= 0 && a[0] <= int(PartKind::Booster) && swapCargo(PartKind(a[0]), a[1]);
        break;
    case O::Weapon:
        // WinTrek computes this local intent before submitting firing state.
        // Reinterpreting a toggle against the server's older input frame can
        // invert its result; apply the already chosen bounded state instead.
        ok=(a[1]==0 || a[1]==1) && g.setWeaponSelection(a[0],a[1]!=0);
        break;
    case O::Target:
        selectTarget(a[0], a[1]);
        break;
    case O::Wing:
        ok = setWing(a[0], a[1]);
        break;
    case O::Donate:
        ok = donateMoney(a[0], a[1]);
        break;
    case O::AutoDonate:
        ok = setAutoDonate(a[0], a[1]);
        break;
    case O::Request:
        ok = requestMoney(a[0], a[1]);
        break;
    case O::Respond:
        ok = respondMoneyRequest(a[0]);
        break;
    case O::Commander:
        ok = handOverCommand(a[0]);
        break;
    case O::Leader:
        ok = makeTeamLeader(a[0]);
        break;
    case O::Board:
        ok = (a[1]==0 || a[1]==1) && boardShip(a[0], false,a[1]!=0);
        break;
    case O::Leave:
        ok = leaveShip();
        break;
    case O::Teleport:
        ok = teleportStation(a[0]);
        break;
    case O::Promote:
        ok = promoteGunner(a[0]);
        break;
    case O::Chat:
        ok = a[0] >= 0 && a[0] < int(ChatChannel::Count) && sendChat(ChatChannel(a[0]), action.text, a[1],echoChatAuthor);
        break;
    case O::Vote:
        ok=(a[1]==0 || a[1]==1) && g.castBallot(a[0],pilot,a[1]!=0);break;
    case O::QuickChat:
        ok = g.quickChatCatalog && a[0] >= 0 && a[0] < int(ChatChannel::Count) && a[2] >= 0 &&
             a[2] < int(g.quickChatCatalog->commands.size()) &&
             (a[3]==0 || a[3]==1) && sendQuickChat(ChatChannel(a[0]), g.quickChatCatalog->commands[a[2]], a[1],a[3]!=0);
        break;
    case O::MapSector:ok=selectMapSector(a[0]);break;
    case O::SectorOrder:ok=orderSector(a[0],a[1]);break;
    case O::SectorHighlight:ok=highlightSector(a[0]);break;
    case O::ChatOrder:
        ok=action.count>0 && action.count<=10 && sendChatOrder(ChatChannel(a[0]),a[1],int(action.count)-1,a[2],a[3]);break;
    case O::ChatWaypoint:case O::ChatCluster:
        ok=sendChatWaypoint(ChatChannel(a[0]),a[1],a[2],a[3],{action.position.x,action.position.y,action.position.z},action.op==O::ChatCluster);break;
    case O::View:
        commandView(a[1] != 0);
        ok = a[0] < 0 || viewSector(a[0]);
        break;
    default:
        ok = false;
        break;
    }
    g.activateActor(old);
    g.refresh();
    return ok;
}
int Game::networkViewSector(int pilot) const {
    const auto& g=*impl_;auto* ship=g.mission->GetShip(ShipID(pilot));if(!ship)return NA;
    auto actor=g.actors.find(pilot);
    if(actor!=g.actors.end() && actor->second.commandView && actor->second.commandSector)
        return actor->second.commandSector->GetObjectID();
    auto* source=ship->GetSourceShip();auto* cluster=source->GetCluster();
    if(!cluster && source->GetStation())cluster=source->GetStation()->GetCluster();
    return cluster?cluster->GetObjectID():NA;
}
void Game::beginNetworkPublication(Net::Tick tick) {
    auto &g = *impl_;
    // Faction map reports expire on their own publication phase. Keep them
    // here while retiring physical captures, rather than rebuilding all four
    // faction maps in the first sector cohort of every 100 ms interval.
    // Feedback has its own mutation/expiry revision. Retain its immutable
    // capture across publication cycles just as we retain faction maps.
    if(tick>1)std::erase_if(g.publicationCache,[](const auto& item){return item.first>=c_cSidesMax && (item.first<0x300000u || item.first>0x300006u);});
    else g.publicationCache.clear(); // Explicit capture/testing callers request fresh state.
    g.publicationIndexValid=false;
    g.clearNetworkNotices(); // Lobby simulation is paused, but its queues must still be bounded.
    if (g.gameRole != GameRole::Authority || !g.player)
        return;
    int old = g.player->GetObjectID();
    std::bitset<c_cSidesMax> visited;
    for (auto &[id, account] : g.accounts)
        if (auto *pilot = g.mission->GetShip(id)) {
            auto sid = pilot->GetSide()->GetObjectID();
            if (visited.test(sid))
                continue;
            visited.set(sid);
            g.activateActor(id);
            for (auto *l = g.mission->GetClusters()->first(); l; l = l->next()) {
                int friends, enemies, stations;
                auto mask = g.sectorAssets(l->data(), pilot->GetSide(), friends, enemies, stations);
                auto *site = static_cast<Impl::SectorSite *>(l->data()->GetClusterSite());
                site->SetClusterAssetMask(mask);
                site->updateWarnings(l->data());
            }
        }
    g.activateActor(old);
}
Net::Frame Game::networkFrame(int pilot, Net::Tick tick, uint64_t inputAck, bool shared,Net::Frame reusable,bool deferredProjectiles,Net::Frame::Effects acknowledged) {
    if(!shared){impl_->publicationCache.clear();impl_->publicationIndexValid=false;}
    auto report=networkReport(pilot,tick,inputAck,acknowledged);
    std::vector<const Net::Frame*> worlds;std::vector<Net::Frame> stateOnly;stateOnly.reserve(report.worlds.size());
    for(const auto& world:report.worlds) {
        if(world.flags&Net::stateOnlyArena){stateOnly.push_back(*world.frame);stateOnly.back().events.clear();worlds.push_back(&stateOnly.back());}
        else worlds.push_back(world.frame.get());
    }
    auto f=Net::composeReport(Net::combineWorlds(worlds),std::move(report.personal));
    auto newNotices=std::upper_bound(f.notices.begin(),f.notices.end(),acknowledged.notices,
        [](uint64_t number,const Net::Notice& notice){return number<notice.number;});
    f.omitted.notices+=size_t(newNotices-f.notices.begin());f.notices.erase(f.notices.begin(),newNotices);
    auto unacknowledged=std::upper_bound(f.events.begin(),f.events.end(),acknowledged.events,
        [](uint64_t number,const Net::Event& event){return number<event.number;});
    f.omitted.events+=size_t(unacknowledged-f.events.begin());f.events.erase(f.events.begin(),unacknowledged);
    if(!deferredProjectiles)f.materialize();
    return f;
}
Net::ReportCapture Game::networkReport(int pilot, Net::Tick tick, uint64_t inputAck,Net::Frame::Effects acknowledged) {
    auto &g = *impl_;
    if (g.gameRole != GameRole::Authority)
        throw std::logic_error("Replica exporting authority");
    int old = g.player->GetObjectID();
    if (!g.activateActor(pilot))
        throw std::runtime_error("Missing network pilot");
    auto *viewer = g.player->GetSide();
    Net::Frame personal;
    g.indexPublicationEffects();
    g.indexNetworkCapture();
    constexpr bool deferredProjectiles=true;
    auto results=[&](Net::ReportCapture& report){if(!g.state.endgame.frozen)return;
        if(!g.resultReport){Net::Frame f;f.header.tick=g.endgameRevision;f.resultPlayers=g.state.endgame.players;f.resultTeams=g.state.endgame.teams;g.resultReport=std::make_shared<const Net::Frame>(std::move(f));}
        report.worlds.push_back({0x100001,g.resultReport});
    };
    auto feedback=[&](Net::ReportCapture& report,unsigned side){
        bool hasCues=side<c_cSidesMax && std::any_of(g.publicationCommonEvents[side].begin(),g.publicationCommonEvents[side].end(),[](auto* e){return Net::isSharedCue(*e);});
        if(g.publicationCommonNotices[side].empty() && !hasCues)return;
        uint32_t domain=0x300000u|side;auto found=g.publicationCache.find(domain);
        if(found==g.publicationCache.end() || found->second->header.tick!=g.feedbackRevision){
            Net::Frame common;common.header.tick=g.feedbackRevision;common.notices.reserve(g.publicationCommonNotices[side].size());
            for(auto* notice:g.publicationCommonNotices[side])common.notices.push_back(*notice);
            if(hasCues)for(auto* event:g.publicationCommonEvents[side])if(Net::isSharedCue(*event))common.events.push_back(*event);
            auto shared=std::make_shared<const Net::Frame>(std::move(common));found=g.publicationCache.insert_or_assign(domain,std::move(shared)).first;
        }
        report.worlds.push_back({domain,found->second});
    };
    auto finish = [&] {
        personal.header.kind = Net::Kind::Snapshot;
        personal.header.tick = tick;
        personal.status.inputAck = inputAck;
        personal.status.pilot = pilot;
        personal.status.selectedWeapon = g.selectedWeapon;
        personal.status.groupFire = g.groupFire;
        personal.status.selectedCargo = g.selectedCargo;
        auto request = g.moneyRequests.find(pilot);
        personal.status.moneySender = request == g.moneyRequests.end() ? -1 : request->second.sender;
        personal.status.moneyAmount = request == g.moneyRequests.end() ? 0 : request->second.amount;
        personal.status.moneyHull = request == g.moneyRequests.end() ? -1 : request->second.hull;
        personal.status.complete = g.state.complete;
        personal.status.winner=g.state.endgame.winner;personal.status.resultFlags=(g.state.endgame.frozen?1:0)|(g.state.endgame.pods?2:0);
        personal.status.dead = g.state.dead;
        Net::text(personal.status.result, g.state.result);
        if(auto notices=g.publicationNotices.find(pilot);notices!=g.publicationNotices.end())
            for(const auto* notice:notices->second) {
                if(notice->number<=acknowledged.notices)++personal.omitted.notices;
                else personal.notices.push_back(*notice);
            }
        if(auto events=g.publicationPrivateEvents.find(pilot);events!=g.publicationPrivateEvents.end())
            for(const auto* event:events->second) {
                if(event->number<=acknowledged.events)++personal.omitted.events;
                else personal.events.push_back(*event);
            }
        g.activateActor(old);
    };
    if(!g.onTeam()) {
        // No Team has no visibility domain. Retain only public faction data
        // and this pilot's identity, rather than indexing scanner arrays with
        // SIDE_TEAMLOBBY or retaining the former faction's report arenas.
        auto exported=[&](Net::Frame& frame,IbaseIGC* model,Net::Entity& e) {
            e.payloadBytes=model->Export(nullptr);auto offset=frame.payload.size();
            frame.payload.resize(offset+e.payloadBytes);model->Export(frame.payload.data()+offset);
        };
        constexpr uint32_t lobbyDomain=c_cSidesMax;
        std::shared_ptr<const Net::Frame> publicState;
        if(auto cached=g.publicationCache.find(lobbyDomain);cached!=g.publicationCache.end() && cached->second->header.tick==tick)
            publicState=cached->second;
        else {
            Net::Frame frame;frame.header.tick=tick;
            for(auto* l=g.mission->GetSides()->first();l;l=l->next()) {
                auto* side=l->data();Net::Entity e{};e.identity={0,OT_side,side->GetObjectID()};exported(frame,side,e);
                DataSideIGC data;auto offset=frame.payload.size()-sizeof(data);
                std::memcpy(&data,frame.payload.data()+offset,sizeof(data));data.ttbmDevelopmentTechs=data.ttbmInitialTechs;
                data.gasAttributes=side->GetCivilization()->GetBaseAttributes();
                std::memcpy(frame.payload.data()+offset,&data,sizeof(data));frame.entities.push_back(e);
                frame.teams.push_back({side->GetObjectID(),g.commanders[side->GetObjectID()],0,unsigned(side->GetActiveF())});
            }
            publicState=std::make_shared<const Net::Frame>(std::move(frame));g.publicationCache[lobbyDomain]=publicState;
        }
        Net::Frame own;own.header.tick=tick;
        IshipIGC* ship=g.player;auto* thing=static_cast<Impl::Thing*>(ship->GetThingSite());
        Net::Entity e{};e.identity={g.identity(ship),OT_ship,ship->GetObjectID()};e.side=SIDE_TEAMLOBBY;
        // No Team has no physical representation. A fresh pilot may already
        // have an adapter Thing but no initialized hull/pose; departed pilots
        // need not disclose or resend their previous physical state either.
        // Keep canonical empty motion/radius/damage/signature in this record.
        if(auto* hull=ship->GetBaseHullType())e.hullType=hull->GetObjectID();
        e.wing=NA;e.flags=Net::Known;e.life=g.lifeSerial[pilot];
        e.deaths=ship->GetDeaths();e.ejections=ship->GetEjections();
        if(thing && (thing->networkSector!=e.sector || thing->networkStation!=e.station || thing->networkParent!=e.parent || thing->networkHull!=e.hullType)) {
            thing->networkEpoch=++g.nextNetworkEpoch;thing->networkSector=e.sector;thing->networkStation=e.station;
            thing->networkParent=e.parent;thing->networkHull=e.hullType;
        }
        e.epoch=thing?thing->networkEpoch:0;exported(own,ship,e);own.entities.push_back(e);
        Net::Pilot p{};p.id=pilot;p.side=SIDE_TEAMLOBBY;Net::text(p.name,ship->GetName());own.pilots.push_back(p);
        finish();
        Net::ReportCapture report{{{lobbyDomain,std::move(publicState)},{0x200000u|uint16_t(pilot),std::make_shared<const Net::Frame>(std::move(own))}},std::move(personal)};
        feedback(report,c_cSidesMax);results(report);return report;
    }
    auto &observations = g.observedRecords[viewer->GetObjectID()];
    auto capture=[&](unsigned category,uint32_t domain)->std::shared_ptr<const Net::Frame> {
        const auto phase=domain%Net::snapshotInterval;
        const Net::Tick mapCycle=tick>=phase?(tick-phase)/Net::snapshotInterval+1:0;
        if(auto cached=g.publicationCache.find(domain);cached!=g.publicationCache.end() &&
           (category==0?g.publicationMapCycles[domain]==mapCycle:cached->second->header.tick==tick))return cached->second;
        Net::Frame f;f.header.tick=tick;std::unordered_set<uint32_t> staticKeys;
    // Most visible objects are transient projectiles. Allocate their contiguous
    // records once rather than repeatedly growing and copying a multi-MiB array.
    if(category==0)f.entities.reserve((deferredProjectiles?size_t(g.mission->GetShips()->n()+g.mission->GetAsteroids()->n()+g.mission->GetStations()->n()):g.things.size())+g.mission->GetClusters()->n()+g.mission->GetSides()->n());
    if(category==1 && g.viewCluster)f.capturedProjectiles.reserve(g.viewCluster->GetModels()->n());
    if(category==0)f.parts.reserve(size_t(g.mission->GetShips()->n())*8);
    if(category==0)f.payload.reserve(size_t(g.mission->GetShips()->n()+g.mission->GetAsteroids()->n()+g.mission->GetStations()->n())*sizeof(DataShipIGC));
    // Physical effects have no presentation outside the viewed sector. Their
    // common records belong to the shared report, not every pilot's envelope.
    for(const auto* event:g.publicationCommonEvents[viewer->GetObjectID()])
        if(!Net::isSharedCue(*event) && ((category==0 && event->sector<0) || (category==2 && g.viewCluster && event->sector==g.viewCluster->GetObjectID())))
            f.events.push_back(*event);
    auto exported = [&](IbaseIGC *base, Net::Entity &e) {
        int bytes = base->Export(nullptr);
        if (bytes < 0 || bytes > 65536)
            throw std::runtime_error("Invalid IGC export");
        auto start = f.payload.size();
        f.payload.resize(start + bytes);
        base->Export(f.payload.data() + start);
        e.payloadBytes = bytes;
    };
    // Side and sector metadata precede their dependants. Hidden models are not
    // exported; client IGC has nothing it could inspect to recover their pose.
    if(category==0) {
    for (auto *l = g.mission->GetSides()->first(); l; l = l->next()) {
        auto *side = l->data();
        Net::Entity e{};
        e.identity = {0, OT_side, side->GetObjectID()};
        exported(side, e);
        if (side != viewer) {
            DataSideIGC data;
            std::memcpy(&data, f.payload.data() + f.payload.size() - sizeof(data), sizeof(data));
            data.ttbmDevelopmentTechs = data.ttbmInitialTechs;
            std::memcpy(f.payload.data() + f.payload.size() - sizeof(data), &data, sizeof(data));
        }
        f.entities.push_back(e);
        f.teams.push_back({side->GetObjectID(), g.commanders[side->GetObjectID()],
                           side == viewer ? g.funds[side->GetObjectID()] : 0, unsigned(side->GetActiveF())});
        if (side == viewer)
            for (auto *b = side->GetBuckets()->first(); b; b = b->next())
                f.buckets.push_back({side->GetObjectID(), b->data()->GetObjectID(), b->data()->GetMoney(),
                                     float(b->data()->GetTime())});
    }
    for (auto *l = g.mission->GetClusters()->first(); l; l = l->next()) {
        Net::Entity e{};
        e.identity = {0, OT_cluster, l->data()->GetObjectID()};
        exported(l->data(), e);
        f.entities.push_back(e);
    }
    }
    const auto& commandBuoys=g.publicationCommandBuoys[viewer->GetObjectID()];
    const auto& things=category==0?g.publicationGlobalThings:g.publicationSectorThings[g.viewCluster?g.viewCluster->GetObjectID():NA];
    for (auto *thing : things) {
        auto *m = thing->model;
        auto type = m->GetObjectType();
        if(category==0 && type==OT_ship) {
            auto* ship=static_cast<IshipIGC*>(m);
            if(IsideIGC::AlliedSides(viewer,ship->GetSide()) || g.shipIntelEye(ship,viewer))g.observeShipIntel(ship,viewer);
        }
        if (type == OT_projectile) {
            if(category!=1)continue;
            // Projectiles are untracked, non-pickable effects, with no export
            // or equipment. Cache their original trajectory/value record until
            // a setter changes a captured field. Physics motion leaves the
            // trajectory anchor unchanged; membership and launcher ID stay live.
            auto *p = static_cast<CprojectileIGC *>(m);
            auto *side = p->CprojectileIGC::GetSide();
            auto *cluster = p->CprojectileIGC::GetCluster();
            if (cluster != g.viewCluster) {
                if(thing->projectileCaptureIndex!=UINT32_MAX) {
                    auto& cached=g.capturedProjectiles[thing->projectileCaptureIndex];
                    int sector=cluster?cluster->GetObjectID():NA;
                    if(cached.lastSector!=sector){cached.record.epoch=++g.nextNetworkEpoch;cached.lastSector=sector;}
                }
                continue;
            }
            if(thing->projectileCaptureIndex==UINT32_MAX){
                if(g.freeProjectileCaptures.empty()){
                    thing->projectileCaptureIndex=uint32_t(g.capturedProjectiles.size());g.capturedProjectiles.emplace_back();
                }else{
                    thing->projectileCaptureIndex=g.freeProjectileCaptures.back();g.freeProjectileCaptures.pop_back();
                    g.capturedProjectiles[thing->projectileCaptureIndex]={};
                }
            }
            auto& cached=g.capturedProjectiles[thing->projectileCaptureIndex];
            if(cached.revision!=p->NativeCaptureRevision()){
                auto& e=cached.record;auto epoch=e.epoch;e={};e.epoch=epoch;
                e.identity = {thing->networkIdentity, type, p->CprojectileIGC::GetObjectID()};
                const auto &anchor = p->NativeTrajectoryAnchor();
                e.motion = {wire(anchor.position), wire(p->CprojectileIGC::GetVelocity()),
                            wire(anchor.forward), wire(anchor.up)};
                e.side = side ? side->GetObjectID() : NA;
                e.sector = cluster ? cluster->GetObjectID() : NA;
                e.radius = p->CprojectileIGC::GetRadius();
                e.signature = p->CprojectileIGC::GetSignature();
                e.activation = anchor.time.clock() - g.start.clock();
                if (thing->hasModel)
                    e.power = p->CprojectileIGC::GetRotation().angle();
                e.definition = p->CprojectileIGC::GetProjectileType()->GetObjectID();
                e.shot = p->NativeShot();
                e.expiry = p->NativeExpireTime().clock();
                cached.revision=p->NativeCaptureRevision();
            }
            auto& e=cached.record;
            e.ordinal=uint32_t(f.entities.size()+f.capturedProjectiles.size());
            // Launcher identity can change while a retained projectile lives.
            // Read it live; capture slots contain only value records.
            auto* launcher=p->CprojectileIGC::GetLauncher();
            e.launcher=launcher?launcher->GetObjectID():NA;
            // Projectile relationships have no station, parent or hull. Keep
            // their publication epoch beside the dense capture record instead
            // of reading and dirtying scattered presentation Thing objects.
            if(cached.lastSector!=e.sector){e.epoch=++g.nextNetworkEpoch;cached.lastSector=e.sector;}
            if(deferredProjectiles)f.capturedProjectiles.push_back(e);
            else f.entities.push_back(e.entity());
            continue;
        }
        if(category==1)continue;
        auto* side=m->GetSide();auto* cluster=m->GetCluster();auto attributes=m->GetAttributes();
        bool own = side == viewer;
        bool known = m->SeenBySide(viewer);
        bool eye = thing->GetCurrentEye(viewer);
        bool visible = own || eye || ((attributes & c_mtPredictable) && known);
        if (!(attributes & c_mtSeenBySide) && !own)
            visible = false;
        // A team's own navigation/order targets must resolve even when a
        // miner or pilot is travelling through another sector. This conveys
        // its waypoint, without exposing unknown hostile objects there.
        visible |= type == OT_buoy && commandBuoys.contains(m);
        auto* physical=cluster;
        if(type==OT_ship && static_cast<IshipIGC*>(m)->GetParentShip())physical=static_cast<IshipIGC*>(m)->GetSourceShip()->GetCluster();
        if(category==2)visible=physical==g.viewCluster && (visible || !(attributes&c_mtSeenBySide));
        uint32_t key = (uint32_t(uint16_t(type)) << 16) | uint16_t(m->GetObjectID());
        bool predictable = (attributes & c_mtPredictable) != 0;
        if (category==0 && predictable && known)
            staticKeys.insert(key);
        if (!visible || type == OT_side || type == OT_cluster)
            continue;
        if (category==0 && predictable && !own && !eye) {
            auto cached = observations.find(key);
            if (cached != observations.end()) {
                auto e = cached->second.entity;
                e.flags = (e.flags | Net::Known) & ~Net::Eye;
                Net::append(f.payload, std::span(cached->second.payload));
                f.entities.push_back(e);
                continue;
            }
        }
        size_t payloadStart = f.payload.size();
        Net::Entity e{};
        e.identity = {thing->networkIdentity, type, m->GetObjectID()};
        const bool strategic=category==0 && type==OT_ship && m->GetCluster() && !static_cast<IshipIGC*>(m)->GetParentShip();
        // Original SHIP_STATUS locates remote ships by sector, without their
        // precise pose. Viewed/source-sector arenas carry that full state.
        if(strategic)e.flags|=Net::Strategic;
        else e.motion = motion(m);
        e.side = side ? side->GetObjectID() : NA;
        e.sector = cluster ? cluster->GetObjectID() : NA;
        e.radius = m->GetRadius();
        e.signature = m->GetSignature();
        e.flags |= (eye ? Net::Eye : 0) | (known ? Net::Known : 0);
        if(type==OT_asteroid || type==OT_warp) {
            const auto& anchor=type==OT_asteroid?static_cast<CasteroidIGC*>(m)->NativeModelSpinAnchor():static_cast<CwarpIGC*>(m)->NativeModelSpinAnchor();
            e.motion.forward=wire(anchor.forward);e.motion.up=wire(anchor.up);
            e.motion.velocity={};e.activation=anchor.time.clock()-g.start.clock();
            e.engine=wire(m->GetRotation().axis());e.power=m->GetVisibleF()?m->GetRotation().angle():0;
            e.flags|=Net::Trajectory|Net::Spin;
        }
        if (type == OT_ship) {
            auto *s = static_cast<CshipIGC *>(m);
            // This field is the authoritative dock-state timestamp for ships;
            // their motion does not use the trajectory activation field.
            e.activation=uint32_t(std::max(0.,g.shipPaneStatus(s).dockedAt)*1000);
            if (auto *type = g.droneType(s))
                e.definition = type->GetObjectID();
            if(!strategic)e.engine = wire(s->GetEngineVector());
            if(!strategic)e.cloaking = s->GetCloaking();
            e.signature = static_cast<CshipIGC *>(s)->NativeBaseSignature();
            e.hullType = s->GetBaseHullType() ? s->GetBaseHullType()->GetObjectID() : NA;
            e.station = s->GetStation() ? s->GetStation()->GetObjectID() : NA;
            e.parent = s->GetParentShip() ? s->GetParentShip()->GetObjectID() : NA;
            e.turret = s->GetTurretID();
            if(!strategic)e.hull = s->GetFraction();
            if(own){
                if(!strategic){e.energy=s->GetEnergy();e.experience=s->GetExperience();e.fuel=s->GetFuel();e.ammo=s->GetAmmo();e.ore=s->GetOre();}
                if(!strategic)std::copy_n(s->GetControls().jsValues,4,e.controls);
            }
            e.wing = own?s->GetWingID():NA;
            e.kills = s->GetKills();
            e.deaths = s->GetDeaths();
            e.ejections = s->GetEjections();
            e.flag = s->GetFlag();
            if(!strategic)e.state = s->GetStateM();
            e.flags |= (own && s->GetAutopilot() ? Net::Autopilot : 0) | (s->IsGhost() ? Net::Ghost : 0);
            e.flags |= (s->GetWarningMask() & c_wmOutOfBounds ? Net::OutOfBounds : 0) |
                       (s->GetWarningMask() & c_wmCrowdedSector ? Net::CrowdedSector : 0);
            if(s->GetStayDocked())e.flags|=Net::StayDocked;
            if(!strategic)for (Axis axis = 0; axis < 3; ++axis)
                e.turn[axis] = s->GetCurrentTurnRate(axis);
            if(!strategic)e.ripcord = s->fRipcordActive() ? s->GetRipcordTimeLeft() : 0;
            if (s->GetStation())
                e.motion = {};
            if (auto *r = own?s->GetRipcordModel():nullptr) {
                e.ripcordType = r->GetObjectType();
                e.ripcordId = r->GetObjectID();
            }
            static_assert(std::tuple_size_v<decltype(e.commands)> == c_cmdMax);
            if(own)for (Command c = 0; c < c_cmdMax; ++c) {
                auto *t = s->GetCommandTarget(c);
                e.commands[c] = {int16_t(t ? t->GetObjectType() : NA), int16_t(t ? t->GetObjectID() : NA),
                                 s->GetCommandID(c)};
            }
            // Cache the read plan, not changing equipment state. Add/remove/
            // mount invalidates it before any cached pointer can be consumed.
            if(!strategic) {
            if(!thing->partCapturePlan)thing->partCapturePlan=std::make_unique<Impl::PartCapturePlan>();
            auto& plan=*thing->partCapturePlan;
            if(plan.revision!=s->NativeEquipmentRevision()){
                plan.entries.clear();plan.entries.reserve(s->GetParts()->n());
                for(auto* l=s->GetParts()->first();l;l=l->next()){
                    auto* p=l->data();plan.entries.push_back({p,p->GetPartType()->GetObjectID(),p->GetMountID(),p->GetEquipmentType()});
                }
                plan.revision=s->NativeEquipmentRevision();
            }
            for(const auto& item:plan.entries){
                if(!own && item.mount<0)continue;
                auto* p=item.model;
                Net::Part part{};
                part.ship = e.identity;
                part.id=item.id;part.mount=item.mount;
                if(own)part.amount=p->GetAmount();
                part.mounted = p->GetMountedFraction();
                auto equipment=item.equipment;
                if (!strategic && equipment == ET_Weapon) {
                    auto *w = static_cast<CweaponIGC *>(p);
                    part.timeFired = w->NativeNextFire().clock();
                    part.shots = w->NativeShots();
                    part.flags = w->NativeFlags();
                }
                if (equipment == ET_Shield) {
                    part.fraction = static_cast<IshieldIGC *>(p)->GetFraction();
                }
                if (equipment == ET_Afterburner) {
                    part.fraction = static_cast<IafterburnerIGC *>(p)->GetPower();
                }
                if (!strategic && (equipment == ET_Magazine || equipment == ET_Dispenser || equipment == ET_ChaffLauncher)) {
                    auto *launcher = static_cast<IlauncherIGC *>(p);
                    if(own)part.timeLoaded=launcher->GetTimeLoaded().clock();
                    if (equipment == ET_Magazine) {
                        if(own)part.lock=static_cast<ImagazineIGC *>(p)->GetLock();
                        part.shots = static_cast<CmagazineIGC *>(p)->NativeShots();
                    }
                }
                f.parts.push_back(part);
                ++e.partCount;
            }
            }
            // Parts are already compact state records. Do not transmit the
            // second variable-length copy embedded in the legacy ship export.
            DataShipIGC record{};
            static_cast<CshipIGC *>(s)->NativeExportIdentity(record);
            Net::append(f.payload, record);
            e.payloadBytes = sizeof(record);
            if(!strategic && e.hullType>=0 && !s->GetParentShip()) {
                Net::ShipDamageState damage;damage.count=thing->damageCount;
                for(size_t i=0;i<damage.count;++i){const auto& frame=(*thing->damageFrames)[i];
                    damage.frames[i]={{frame.position.x,frame.position.y,frame.position.z},
                                      {frame.direction.x,frame.direction.y,frame.direction.z}};}
                Net::append(f.payload,damage);e.payloadBytes+=sizeof(damage);e.flags|=Net::DamageState;
            }
        } else if (type == OT_missile) {
            auto *missile = static_cast<ImissileIGC *>(m);
            e.definition = missile->GetMissileType()->GetObjectID();
            if (static_cast<CmissileIGC *>(missile)->NativeDud())
                e.flags |= Net::Dud;
            e.launcher = missile->GetLauncher()->GetObjectID();
            if (auto *t = missile->GetTarget()) {
                e.targetType = t->GetObjectType();
                e.targetId = t->GetObjectID();
            }
            e.lock = missile->GetLock();
            e.hull = missile->GetFraction();
            e.shot = static_cast<CmissileIGC *>(missile)->NativeShot();
            e.expiry = static_cast<CmissileIGC *>(missile)->NativeExpireTime().clock();
            e.activation = static_cast<CmissileIGC *>(missile)->NativeActivateTime().clock();
            e.impact = static_cast<CmissileIGC *>(missile)->NativeImpactTime();
        } else if (type == OT_chaff) {
            e.definition = static_cast<CchaffIGC *>(m)->NativeTypeID();
            e.expiry = static_cast<CchaffIGC *>(m)->NativeExpireTime().clock();
        } else {
            exported(m, e);
            if(type==OT_treasure) {
                auto* treasure=static_cast<CtreasureIGC*>(m);
                DataTreasureIGC data;
                std::memcpy(&data,f.payload.data()+payloadStart,sizeof(data));
                const auto& spin=treasure->NativeSpinAnchor();
                e.flags|=Net::Trajectory|Net::Decelerating;
                e.motion={wire(data.p0),wire(data.v0),wire(spin.forward),wire(spin.up)};
                e.activation=data.time0.clock()-g.start.clock();
                e.expiry=treasure->NativeExpireTime().clock();
                e.impact=10.f;
                e.lock=float(spin.time-data.time0);
                e.engine=wire(treasure->GetRotation().axis());
                // A team flag is a decal; its spin is a ThingSite effect,
                // rather than a change to the model's orientation.
                if(data.treasureCode!=c_tcFlag || data.treasureID==SIDE_TEAMLOBBY)
                    e.power=treasure->GetRotation().angle();
            }
            if (type == OT_station) {
                auto *station = static_cast<IstationIGC *>(m);
                e.hull = station->GetFraction();
                e.shield = station->GetShieldFraction();
            }
            if (type == OT_mine)
                e.hull = static_cast<ImineIGC *>(m)->GetStrength();
            if (type == OT_probe)
                e.hull = static_cast<IprobeIGC *>(m)->GetFraction();
            if (type == OT_asteroid) {
                e.hull = static_cast<IasteroidIGC *>(m)->GetFraction();
                e.ore = static_cast<IasteroidIGC *>(m)->GetOreSeenBySide(viewer);
            }
        }
        if (thing->networkSector != e.sector || thing->networkStation != e.station ||
            thing->networkParent != e.parent || thing->networkHull != e.hullType) {
            thing->networkEpoch = ++g.nextNetworkEpoch;
            thing->networkSector = e.sector;
            thing->networkStation = e.station;
            thing->networkParent = e.parent;
            thing->networkHull = e.hullType;
        }
        e.life = type == OT_ship ? g.lifeSerial[m->GetObjectID()] : 0;
        e.epoch = thing->networkEpoch;
        if (type == OT_station && !own && !eye) {
            if (auto *o = thing->stationObservation(viewer); o && o->known) {
                e.identity.serial = o->networkIdentity;
                e.hull = o->intel.hull;
                e.shield = o->intel.shield;
                e.side = o->intel.side;
                e.radius = o->radius;
                e.motion.position = wire(o->record.position);
                e.motion.forward = wire(o->record.forward);
                e.motion.up = wire(o->record.up);
                std::memcpy(f.payload.data() + payloadStart, &o->record, sizeof(o->record));
            }
        }
        // These pointer-free creation exports duplicate the authoritative pose
        // already carried by Entity. Keep their wire copies invariant as a rock
        // or station spins; creation restores the exact pose from Entity below.
        if (type == OT_asteroid) {
            DataAsteroidIGC data;
            std::memcpy(&data, f.payload.data() + payloadStart, sizeof(data));
            data.position = data.forward = data.up = Vector::GetZero();
            std::memcpy(f.payload.data() + payloadStart, &data, sizeof(data));
        } else if (type == OT_station) {
            DataStationIGC data;
            std::memcpy(&data, f.payload.data() + payloadStart, sizeof(data));
            data.position = data.forward = data.up = Vector::GetZero();
            std::memcpy(f.payload.data() + payloadStart, &data, sizeof(data));
        } else if (type == OT_warp) {
            DataWarpIGC data;
            std::memcpy(&data,f.payload.data()+payloadStart,sizeof(data));
            data.position=data.forward=Vector::GetZero();
            std::memcpy(f.payload.data()+payloadStart,&data,sizeof(data));
        }
        if ((category==0 || category==2) && predictable) {
            Impl::ObservedRecord record;
            record.entity = e;
            record.payload.assign(f.payload.begin() + payloadStart, f.payload.end());
            observations[key] = std::move(record);
        }
        f.entities.push_back(e);
    }
    if(category==0){
        // A fresh join receives the faction's history, including contacts no
        // longer in any physical arena. Capture/encode once for the faction.
        f.shipIntel.reserve(g.shipIntel[viewer->GetObjectID()].size());
        for(const auto& [id,intel]:g.shipIntel[viewer->GetObjectID()])if(intel.serial && intel.side>=0)f.shipIntel.push_back(intel);
        std::sort(f.shipIntel.begin(),f.shipIntel.end(),[](auto& a,auto& b){return a.id<b.id;});
        for(int id:g.sectorHighlights[viewer->GetObjectID()])f.highlights.push_back({id});std::sort(f.highlights.begin(),f.highlights.end(),[](auto& a,auto& b){return a.sector<b.sector;});
    }
    // A construction site can remain a remembered asteroid until a scout sees
    // what replaced it. Carry that original stale observation, not the station.
    if(category==0) {
    for (auto i = observations.begin(); i != observations.end();)
        if (!staticKeys.contains(i->first)) {
            auto &e = i->second.entity;
            bool retained = e.identity.type == OT_asteroid &&
                            std::any_of(g.rememberedRocks.begin(), g.rememberedRocks.end(), [&](auto &rock) {
                                return rock.id == e.identity.id && rock.sector == e.sector &&
                                       (rock.sides & (1u << viewer->GetObjectID()));
                            });
            if (retained) {
                auto remembered = e;
                remembered.flags = (remembered.flags | Net::Known) & ~Net::Eye;
                Net::append(f.payload, std::span(i->second.payload));
                f.entities.push_back(remembered);
                ++i;
            } else
                i = observations.erase(i);
        } else
            ++i;
    for (auto *l = g.mission->GetShips()->first(); l; l = l->next()) {
        auto *s = l->data();
        if (s->GetPilotType() < c_ptPlayer || (g.economy() && !g.accounts.contains(s->GetObjectID())) ||
            !IsideIGC::AlliedSides(viewer, s->GetSide()))
            continue;
        Net::Pilot p{};
        p.id = s->GetObjectID();
        p.side = s->GetSide()->GetObjectID();
        p.wing = s->GetSide()==viewer?s->GetWingID():NA;
        p.identity=g.creditIdentity(s);
        if(s->GetSide()==viewer)p.money=g.readBalance(s);
        else if(auto found=g.alliedCreditTotals[viewer->GetObjectID()].find(s->GetObjectID());found!=g.alliedCreditTotals[viewer->GetObjectID()].end() && found->second.identity==p.identity)
            p.money=Allegiance::CreditDifference(found->second.amount);
        p.autoDonate = s->GetSide() == viewer && s->GetAutoDonate() ? s->GetAutoDonate()->GetObjectID() : NA;
        Net::text(p.name, s->GetName());
        f.pilots.push_back(p);
    }
    }
    auto world=std::make_shared<const Net::Frame>(std::move(f));
    if(category==0)g.publicationMapCycles[domain]=mapCycle;
    g.publicationCache[domain]=world;return world;
    };
    Net::ReportCapture report;
    uint32_t side=uint8_t(viewer->GetObjectID());
    report.worlds.push_back({side,capture(0,side)});
    if(g.viewCluster) {
        uint32_t sector=uint16_t(g.viewCluster->GetObjectID());
        if(sector>=0x4000)throw std::runtime_error("Visibility sector identity range");
        uint32_t shots=0x800000u|sector,local=0x400000u|(sector<<8)|side;
        report.worlds.push_back({shots,capture(1,shots)});
        report.worlds.push_back({local,capture(2,local)});
    }
    // Command view may be in another sector while this pilot is still flying.
    // Its own physics sector must remain complete for prediction and turrets.
    auto* source=g.player->GetSourceShip();auto* physical=source->GetCluster();
    if(!physical && source->GetStation())physical=source->GetStation()->GetCluster();
    if(physical && physical!=g.viewCluster){
        auto* viewed=g.viewCluster;g.viewCluster=physical;
        uint32_t sector=uint16_t(physical->GetObjectID());
        if(sector>=0x4000)throw std::runtime_error("Physical sector identity range");
        uint32_t local=0x400000u|(sector<<8)|side;
        report.worlds.push_back({local,capture(2,local),Net::stateOnlyArena});
        g.viewCluster=viewed;
    }
    feedback(report,side);results(report);finish();report.personal=std::move(personal);return report;
}
void Game::applyNetworkFrame(const Net::Frame &frame) {
    restoreNetworkPresentation();
    auto &g = *impl_;
    if (g.gameRole != GameRole::Replica)
        throw std::logic_error("Authority accepting replica state");
    const bool firstReport=!g.player;
    const SideID previousAudience=g.player?g.player->GetSide()->GetObjectID():SIDE_TEAMLOBBY;
    const bool previousGhost=g.player && g.player->IsGhost();
    TRef<IclusterIGC> previousFlightSector=g.player?g.player->GetSourceShip()->GetCluster():nullptr;
    Allegiance::SimulationRandomScope random(g.randomState);
    auto cameraSeat = [&]() {
        std::array<int, 6> seat{NA, NA, NA, NA, NA, 0};
        if (g.player) {
            auto *source = g.player->GetSourceShip();
            seat = {source->GetObjectID(),
                    source->GetCluster() ? source->GetCluster()->GetObjectID() : NA,
                    g.player->GetStation() ? g.player->GetStation()->GetObjectID() : NA,
                    source->GetBaseHullType() ? source->GetBaseHullType()->GetObjectID() : NA,
                    g.player->GetParentShip() ? g.player->GetParentShip()->GetObjectID() : NA,
                    int(g.lifeSerial[g.player->GetObjectID()])};
        }
        return seat;
    };
    auto previousSeat = cameraSeat();
    g.elapsed = double(frame.header.tick) / Net::tickRate;
    g.state.time = g.elapsed;
    int incomingSide=NA;for(const auto& e:frame.entities)if(e.identity.type==OT_ship && e.identity.id==frame.status.pilot){incomingSide=e.side;break;}
    bool sameViewer=g.onTeam() && g.player->GetSide()->GetObjectID()==incomingSide;
    if(g.playerPaneSide!=incomingSide){g.playerPaneSide=incomingSide;g.playerPaneMemory.clear();}
    std::vector<ShipID> rosterBootstrap;
    auto time = g.start + float(g.elapsed);
    static_cast<CmissionIGC *>(g.mission)->NativeSetTime(time);
    size_t offset = 0;
    std::vector<std::max_align_t> aligned;
    aligned.reserve(65536 / sizeof(std::max_align_t));
    std::unordered_set<uint32_t> present;
    size_t creationOffset = 0;
    for (auto &e : frame.entities) {
        if (e.payloadBytes > frame.payload.size() - creationOffset)
            throw std::runtime_error("IGC payload overflow");
        if (e.identity.serial)
            present.insert(e.identity.serial);
        if (e.identity.type == OT_buildingEffect) {
            if (e.payloadBytes != sizeof(DataBuildingEffectExport))
                throw std::runtime_error("Building effect layout");
            DataBuildingEffectExport data;
            std::memcpy(&data, frame.payload.data() + creationOffset, sizeof(data));
            if (data.asteroidID == NA) {
                auto found = g.replicas.find(e.identity.serial);
                if (found != g.replicas.end()) {
                    if (found->second->GetObjectType() != OT_buildingEffect)
                        throw std::runtime_error("Replication identity changed object type");
                    auto *shell = static_cast<IbuildingEffectIGC *>(static_cast<ImodelIGC *>(found->second));
                    // Match WinTrek's quiet construction completion before
                    // retiring either the rock or its builder. Both original
                    // Terminate paths otherwise cancel the attached shell.
                    if (auto *rock = shell->GetAsteroid())
                        rock->SetBuildingEffect(nullptr);
                }
            }
        }
        creationOffset += e.payloadBytes;
    }
    if (creationOffset != frame.payload.size())
        throw std::runtime_error("Extra IGC payload");
    // A snapshot owns one buoy consumer independently of ship commands. Clear
    // all disappearing-model commands before releasing that ownership: an IGC
    // buoy terminates itself when its last consumer leaves. TRef alone only
    // preserves memory and does not preserve membership in the mission.
    std::unordered_set<ImodelIGC *> retiringModels;
    for (auto &[serial, model] : g.replicas)
        if (!present.contains(serial)) {
            // Capture before any retirement can clear the controlled pilot or
            // detach a parent. A disappearing report is loss of observation,
            // not evidence that the player/drone has left the match.
            if(sameViewer && model->GetObjectType()==OT_ship) {
                auto* ship=static_cast<IshipIGC*>(static_cast<ImodelIGC*>(model));auto* source=ship->GetSourceShip();
                if(ship->GetSide()==g.player->GetSide() || (source->GetCluster() && source->GetCurrentEye(g.player->GetSide())))g.observePlayerPaneShip(ship);
            }
            retiringModels.insert(model);
        }
    if (!retiringModels.empty())
        for (auto *l = g.mission->GetShips()->first(); l; l = l->next()) {
            auto *ship = l->data();
            if (ship->GetPilotType() == c_ptBuilder &&
                retiringModels.contains(ship->GetCommandTarget(c_cmdPlan)) &&
                ship->GetCommandTarget(c_cmdPlan)->GetObjectType() == OT_asteroid)
                ship->SetStateBits(drillingMaskIGC | buildingMaskIGC, 0);
            for (Command c = 0; c < c_cmdMax; ++c)
                if (retiringModels.contains(ship->GetCommandTarget(c)))
                    ship->SetCommand(c, nullptr, c_cidNone);
            if (retiringModels.contains(ship->GetRipcordModel()))
                ship->SetRipcordModel(nullptr);
        }
    auto retire = [&](ImodelIGC *model) {
        if (g.target == model)
            g.target = nullptr;
        if (g.player && static_cast<ImodelIGC *>(g.player) == model)
            g.player = nullptr;
        if (!model->GetMission())
            return;
        if (model->GetObjectType() == OT_buoy)
            static_cast<IbuoyIGC *>(model)->ReleaseConsumer();
        else
            model->Terminate();
    };
    {
        struct RetirementScope {
            Impl &g;
            RetirementScope(Impl &game, const std::unordered_set<ImodelIGC *> &models) : g(game) {
                g.replicaRetiring = &models;
            }
            ~RetirementScope() { g.replicaRetiring = nullptr; }
        } scope(g, retiringModels);
        for (auto it = g.replicas.begin(); it != g.replicas.end();)
            if (!present.contains(it->first)) {
                TRef<ImodelIGC> keep = it->second;
                retire(keep);
                it = g.replicas.erase(it);
            } else
                ++it;
    }
    // Creation pass. Payload exports use only pointer-free original IGC layouts;
    // pointer-bearing missile/chaff creation arguments are resolved locally.
    for (bool effects : {false, true}) {
      offset = 0;
      for (auto &e : frame.entities) {
        if (e.payloadBytes > frame.payload.size() - offset)
            throw std::runtime_error("IGC payload overflow");
        auto payloadOffset = offset;
        offset += e.payloadBytes;
        if ((e.identity.type == OT_buildingEffect) != effects)
            continue;
        aligned.resize((e.payloadBytes + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
        if (e.payloadBytes)
            std::memcpy(aligned.data(), frame.payload.data() + payloadOffset, e.payloadBytes);
        auto *data = reinterpret_cast<std::byte *>(aligned.data());
        auto type = e.identity.type;
        if(e.flags&Net::DamageState) {
            if(type!=OT_ship || e.payloadBytes!=sizeof(DataShipIGC)+sizeof(Net::ShipDamageState))
                throw std::runtime_error("Ship damage layout");
        }
        if (type == OT_asteroid) {
            if (e.payloadBytes != sizeof(DataAsteroidIGC))
                throw std::runtime_error("Asteroid layout");
            auto *d = reinterpret_cast<DataAsteroidIGC *>(data);
            d->position = vector(e.motion.position);
            d->forward = vector(e.motion.forward);
            d->up = vector(e.motion.up);
        } else if (type == OT_station) {
            if (e.payloadBytes != sizeof(DataStationIGC))
                throw std::runtime_error("Station layout");
            auto *d = reinterpret_cast<DataStationIGC *>(data);
            d->position = vector(e.motion.position);
            d->forward = vector(e.motion.forward);
            d->up = vector(e.motion.up);
        } else if(type==OT_warp) {
            if(e.payloadBytes!=sizeof(DataWarpIGC))throw std::runtime_error("Warp layout");
            auto* d=reinterpret_cast<DataWarpIGC*>(data);
            d->position=vector(e.motion.position);d->forward=vector(e.motion.forward);
        }
        ImodelIGC *model = nullptr;
        if (type == OT_side) {
            if (e.payloadBytes != sizeof(DataSideIGC))
                throw std::runtime_error("Side layout");
            DataSideIGC side;
            std::memcpy(&side, data, sizeof(side));
            if(side.sideID<0 || side.sideID>=g.mission->GetMissionParams()->nTeams || !g.mission->GetCivilization(side.civilizationID) || !Net::terminated(side.name))throw std::runtime_error("Invalid replicated side");
            auto *existing = g.mission->GetSide(side.sideID);
            if (!existing) {
                auto *created = g.mission->CreateObject(time, OT_side, &side, sizeof(side));
                if (!created)
                    throw std::runtime_error("Replica side");
                created->Release();
            } else {
                auto* civ=g.mission->GetCivilization(side.civilizationID);
                if(!civ || !Net::terminated(side.name))throw std::runtime_error("Invalid side civilization or name");
                if(existing->GetCivilization()!=civ){
                    existing->DestroyBuckets();existing->SetCivilization(civ);
                    if(g.mission->GetMissionParams()->bAllowDevelopments)existing->CreateBuckets();
                }
                existing->SetName(side.name);
                existing->SetDevelopmentTechs(side.ttbmDevelopmentTechs);
                existing->SetInitialTechs(side.ttbmInitialTechs);
                existing->SetGlobalAttributeSet(side.gasAttributes);
                static_cast<CsideIGC *>(existing)->NativeRestoreScores(side);
            }
            // Ordered mission policy owns alliances. A delayed world report
            // must not restore groups from before a menu action, including
            // when it creates a side absent from the previous replica frame.
            g.mission->GetSide(side.sideID)->SetAllies(g.options.alliances[side.sideID]);
            g.options.teamNames[side.sideID]=side.name;
            int index=0;for(auto* l=g.mission->GetCivilizations()->first();l;l=l->next(),++index)
                if(l->data()->GetObjectID()==side.civilizationID){g.options.teamCivilizations[side.sideID]=index;break;}
            continue;
        }
        if (type == OT_cluster) {
            if (!g.mission->GetCluster(e.identity.id)) {
                auto *created = g.mission->CreateObject(time, type, data, e.payloadBytes);
                if (!created)
                    throw std::runtime_error("Replica sector");
                created->Release();
            }
            continue;
        }
        present.insert(e.identity.serial);
        auto found = g.replicas.find(e.identity.serial);
        if (found != g.replicas.end())
            model = found->second;
        if (model && model->GetObjectType() != type)
            throw std::runtime_error("Replication identity changed object type");
        // Original IGC removals can cascade (a rock cancels its shell). A TRef
        // in the identity map preserves memory, never mission membership.
        if (model && model->GetMission() != g.mission) {
            g.replicas.erase(found);
            model = nullptr;
        }
        if (!model && e.shot) {
            auto shot = g.shots.find(e.shot);
            if (shot != g.shots.end()) {
                model = shot->second;
                g.replicas[e.identity.serial] = model;
            }
        }
        if (!model) {
            IbaseIGC *created = nullptr;
            if (type == OT_missile) {
                auto *mt = g.mission->GetExpendableType(e.definition);
                auto *launcher = g.mission->GetShip(e.launcher);
                auto *cluster = g.mission->GetCluster(e.sector);
                if (mt && cluster) {
                    DataMissileIGC d{};
                    d.position = vector(e.motion.position);
                    d.velocity = vector(e.motion.velocity);
                    d.forward = vector(e.motion.forward);
                    d.pmissiletype = static_cast<ImissileTypeIGC *>(mt);
                    d.pLauncher = launcher;
                    d.pTarget = g.mission->GetModel(e.targetType, e.targetId);
                    d.pCluster = cluster;
                    d.lock = e.lock;
                    d.missileID = e.identity.id;
                    d.bDud = bool(e.flags & Net::Dud);
                    created = g.mission->CreateObject(time, type, &d, sizeof(d));
                }
            } else if (type == OT_projectile) {
                DataProjectileIGC d{};
                d.velocity = vector(e.motion.velocity);
                d.forward = vector(e.motion.forward);
                d.lifespan = std::max(.001f, Time(e.expiry) - time);
                d.projectileTypeID = e.definition;
                created = g.mission->CreateObject(time, type, &d, sizeof(d));
            } else if (type == OT_chaff) {
                auto *mt = g.mission->GetExpendableType(e.definition);
                if (mt) {
                    DataChaffIGC d{};
                    d.p0 = vector(e.motion.position);
                    d.v0 = vector(e.motion.velocity);
                    d.time0 = time;
                    d.pcluster = g.mission->GetCluster(e.sector);
                    d.pchafftype = static_cast<IchaffTypeIGC *>(mt);
                    created = g.mission->CreateObject(time, type, &d, sizeof(d));
                }
            } else {
                if (type == OT_ship && e.hullType < 0 && e.side>=0) {
                    auto *d = reinterpret_cast<DataShipIGC *>(data);
                    auto *side = g.mission->GetSide(d->sideID);
                    if (!side)
                        throw std::runtime_error("Passenger side");
                    d->hullID = side->GetCivilization()->GetLifepod()->GetObjectID();
                }
                created = g.mission->CreateObject(time, type, data, e.payloadBytes);
            }
            if (!created) {
                if (type == OT_missile)
                    continue;
                throw std::runtime_error("Replica object creation type " + std::to_string(type));
            }
            model = static_cast<ImodelIGC *>(created);
            g.replicas[e.identity.serial] = model;
            if (type == OT_buoy)
                static_cast<IbuoyIGC *>(model)->AddConsumer();
            created->Release();
            if (e.shot)
                g.NativeProjectileCreated(model, e.shot);
        }
        if (type == OT_ship) {
            auto *ship = static_cast<IshipIGC *>(model);
            if (e.definition >= 0)
                g.droneTypes[e.identity.id] = e.definition;
            else
                g.droneTypes.erase(e.identity.id);
            if (ship->GetParentShip() && ship->GetParentShip()->GetObjectID() != e.parent)
                ship->SetParentShip(nullptr);
            if (e.hullType >= 0 && (!ship->GetBaseHullType() || ship->GetBaseHullType()->GetObjectID() != e.hullType)) {
                while (auto *l = ship->GetParts()->first())
                    l->data()->Terminate();
                ship->SetBaseHullType(g.mission->GetHullType(e.hullType));
            }
            g.lifeSerial[e.identity.id] = e.life;
            ship->SetSide(g.mission->GetSide(e.side));
            if (e.hullType >= 0 && !(e.flags&Net::Strategic)) {
                ship->SetFraction(e.hull);
                ship->SetFuel(e.fuel);
                ship->SetEnergy(e.energy);
                ship->SetAmmo(e.ammo);
            }
            if(e.flags&Net::DamageState) {
                Net::ShipDamageState damage;
                std::memcpy(&damage,data+sizeof(DataShipIGC),sizeof(damage));
                if(damage.count>damage.frames.size())throw std::runtime_error("Ship damage slots");
                auto* thing=static_cast<Impl::Thing*>(ship->GetThingSite());
                if(damage.count && !thing->damageFrames)thing->damageFrames=std::make_unique<std::array<Impl::Thing::DamageFrame,10>>();
                thing->damageCount=int(damage.count);
                for(size_t i=0;i<damage.count;++i) {
                    const auto& frame=damage.frames[i];
                    (*thing->damageFrames)[i]={{frame.position.x,frame.position.y,frame.position.z},
                                              {frame.direction.x,frame.direction.y,frame.direction.z}};
                }
            }
            if(!(e.flags&Net::Strategic)){ship->SetExperience(e.experience);static_cast<CshipIGC *>(ship)->SetOre(e.ore);}
            ship->SetWingID(e.wing);
            ship->SetKills(e.kills);
            ship->SetDeaths(e.deaths);
            ship->SetEjections(e.ejections);
            ship->SetFlag(e.flag);
            static_cast<CshipIGC *>(ship)->NativeRestoreAutopilot(e.flags & Net::Autopilot);
            ship->SetStayDocked(e.flags & Net::StayDocked);
            ControlData controls;
            std::copy_n(e.controls, 4, controls.jsValues);
            if(!(e.flags&Net::Strategic)){
                ship->SetControls(controls);
                for (Axis axis = 0; axis < 3; ++axis)ship->SetCurrentTurnRate(axis, e.turn[axis]);
            }
        }
        // SetCluster is a teleport/arrival operation even when the sector is
        // unchanged: it clears input/ripcord state and recenters command view.
        // A publication of an existing relationship is not another arrival.
        auto *sector = e.sector < 0 ? nullptr : g.mission->GetCluster(e.sector);
        if (model->GetCluster() != sector)
            model->SetCluster(sector);
        if (type != OT_ship)
            motion(model, (e.flags&Net::Trajectory)?Net::trajectoryMotion(e,double(time.clock()-g.start.clock())*.001):e.motion);
        model->SetSignature(e.signature);
        if (e.radius > 0 && model->GetRadius() != e.radius)
            model->SetRadius(e.radius);
        model->SetLastUpdate(time);
        if (e.shot)
            g.confirmedShots.insert(e.shot);
        if (type == OT_asteroid) {
            auto *asteroid = static_cast<IasteroidIGC *>(model);
            asteroid->SetFraction(e.hull);
            asteroid->SetOre(e.ore);
            asteroid->SetOreWithFraction(asteroid->GetOreFraction(), true);
        }
        if (type == OT_probe)
            static_cast<IprobeIGC *>(model)->SetFraction(e.hull);
        if (type == OT_mine)
            static_cast<CmineIGC *>(model)->NativeRestoreStrength(e.hull);
        if (type == OT_buildingEffect) {
            model->Update(time);
            if (model->GetMission() != g.mission)
                continue;
        }
        if (type == OT_missile) {
            auto *missile = static_cast<CmissileIGC *>(model);
            missile->NativeSetID(e.identity.id);
            missile->SetSide(g.mission->GetSide(e.side));
            missile->SetFraction(e.hull);
            missile->NativeRestore(Time(e.expiry), Time(e.activation), e.impact, e.lock);
            if (e.flags & Net::Dud)
                missile->Disarm();
            missile->GetThingSite()->SetAfterburnerThrust(missile->GetOrientation().GetBackward(),
                                                          Time(e.activation) <= time ? 1 : 0);
        }
        if (type == OT_station) {
            auto *station = static_cast<IstationIGC *>(model);
            if (e.payloadBytes == sizeof(DataStationIGC)) {
                DataStationIGC d;
                std::memcpy(&d, data, sizeof(d));
                auto *st = g.mission->GetStationType(d.stationTypeID);
                if (st && st != station->GetBaseStationType())
                    station->SetBaseStationType(st);
            }
            station->SetFraction(e.hull);
            station->SetShieldFraction(e.shield);
            station->SetSide(g.mission->GetSide(e.side));
        }
        if (type == OT_projectile) {
            auto *p = static_cast<IprojectileIGC *>(model);
            static_cast<CprojectileIGC *>(p)->NativeSetExpireTime(Time(e.expiry));
            p->SetSide(g.mission->GetSide(e.side));
        }
        if (type == OT_chaff)
            static_cast<CchaffIGC *>(model)->NativeSetExpireTime(Time(e.expiry));
        if(type==OT_treasure) {
            DataTreasureIGC d;
            std::memcpy(&d,data,sizeof(d));
            auto* treasure=static_cast<CtreasureIGC*>(model);
            treasure->SetAmount(d.amount);
            treasure->SetTreasureID(d.treasureID);
            treasure->NativeSetExpireTime(Time(e.expiry));
            treasure->SetRotation(Rotation(vector(e.engine),e.power));
        }
        auto *thing = static_cast<Impl::Thing *>(model->GetThingSite());
        auto *viewer = g.mission->GetSide(SideID(g.options.team));
        if(thing){
            thing->SetSideVisibility(viewer, bool(e.flags & Net::Known));
            thing->vision().Set(viewer, bool(e.flags & Net::Known), bool(e.flags & Net::Eye));
        }
      }
    }
    if (offset != frame.payload.size())
        throw std::runtime_error("Extra IGC payload");
    // Docking must also wait for newly created stations. Do it before part
    // state is restored, since the original setter deactivates the booster.
    for (auto &e : frame.entities)
        if (e.identity.type == OT_ship)
            if (auto *ship = g.mission->GetShip(e.identity.id))
                ship->SetStation(e.station < 0 ? nullptr : g.mission->GetStation(e.station));
    for (auto &p : frame.parts) {
        auto *ship = g.mission->GetShip(p.ship.id);
        if (!ship)
            continue;
        auto *type = g.mission->GetPartType(p.id);
        if (!type)
            throw std::runtime_error("Unknown replicated part");
        auto *old = ship->GetMountedPart(p.mount < 0 ? NA : type->GetEquipmentType(), Mount(p.mount));
        if (old && old->GetPartType() != type) {
            old->Terminate();
            old = nullptr;
        }
        auto *part = old ? old : ship->CreateAndAddPart(type, Mount(p.mount), p.amount);
        if (!part)
            throw std::runtime_error("Invalid replicated mount");
        part->SetAmount(short(p.amount));
        part->SetMountedFraction(p.mounted);
        if (type->GetEquipmentType() == ET_Weapon)
            static_cast<CweaponIGC *>(part)->NativeRestore(Time(p.timeFired), p.shots, p.flags);
        if (type->GetEquipmentType() == ET_Magazine) {
            static_cast<CmagazineIGC *>(part)->NativeRestoreShots(p.shots);
            static_cast<ImagazineIGC *>(part)->SetLock(p.lock);
        }
        if (type->GetEquipmentType() == ET_Shield)
            static_cast<IshieldIGC *>(part)->SetFraction(p.fraction);
        if (type->GetEquipmentType() == ET_Afterburner)
            static_cast<IafterburnerIGC *>(part)->SetPower(p.fraction);
        if (type->GetEquipmentType() == ET_Magazine || type->GetEquipmentType() == ET_Dispenser ||
            type->GetEquipmentType() == ET_ChaffLauncher)
            static_cast<IlauncherIGC *>(part)->SetTimeLoaded(Time(p.timeLoaded));
    }
    std::unordered_map<uint32_t, std::vector<const Net::Part *>> partIndex;
    for (auto &p : frame.parts)
        partIndex[p.ship.serial].push_back(&p);
    for (auto &e : frame.entities)
        if (e.identity.type == OT_ship) {
            auto *ship = g.mission->GetShip(e.identity.id);
            if (!ship)
                continue;
            for (auto *l = ship->GetParts()->first(); l;) {
                auto *p = l->data();
                l = l->next();
                if (std::none_of(partIndex[e.identity.serial].begin(), partIndex[e.identity.serial].end(),
                                 [&](auto *r) {
                                     return r->id == p->GetPartType()->GetObjectID() && r->mount == p->GetMountID();
                                 }))
                    p->Terminate();
            }
        }
    // Resolve relationships only after every entity exists, including new crew,
    // docking destinations and missile targets in the same publication.
    for (auto &e : frame.entities) {
        if (e.identity.type == OT_missile) {
            auto found = g.replicas.find(e.identity.serial);
            if (found != g.replicas.end() && found->second->GetMission() == g.mission) {
                auto *missile = static_cast<CmissileIGC *>(static_cast<ImodelIGC *>(found->second));
                auto *target = g.mission->GetModel(e.targetType, e.targetId);
                if (missile->GetTarget() != target)
                    missile->SetTarget(target);
                missile->NativeRestore(Time(e.expiry), Time(e.activation), e.impact, e.lock);
            }
        } else if (e.identity.type == OT_projectile) {
            auto found = g.replicas.find(e.identity.serial);
            if (found != g.replicas.end() && found->second->GetMission() == g.mission) {
                auto *p = static_cast<IprojectileIGC *>(static_cast<ImodelIGC *>(found->second));
                if (!p->GetLauncher())
                    if (auto *launcher = g.mission->GetShip(e.launcher))
                        p->SetLauncher(launcher);
            }
        }
        if (e.identity.type != OT_ship)
            continue;
        auto *ship = g.mission->GetShip(e.identity.id);
        if (!ship)
            continue;
        if (e.parent >= 0) {
            auto *parent = g.mission->GetShip(e.parent);
            if (parent) {
                ship->SetParentShip(parent);
                ship->SetTurretID(e.turret);
            }
        }
        for (Command c = 0; c < c_cmdMax; ++c) {
            auto &cmd = e.commands[c];
            auto *target = g.mission->GetModel(cmd.type, cmd.target);
            if (ship->GetCommandTarget(c) != target || ship->GetCommandID(c) != cmd.id)
                ship->SetCommand(c, target, cmd.id);
        }
        if (e.ripcordId >= 0)
            ship->SetRipcordModel(g.mission->GetModel(e.ripcordType, e.ripcordId));
        else
            ship->SetRipcordModel(nullptr);
        // Original command/crew setters may clear drilling and other state
        // bits. The published state belongs after those local side effects.
        ship->SetStateM(e.state);
        ship->ClearWarningMaskBit(c_wmOutOfBounds | c_wmCrowdedSector);
        if (e.flags & Net::OutOfBounds)
            ship->SetWarningMaskBit(c_wmOutOfBounds);
        if (e.flags & Net::CrowdedSector)
            ship->SetWarningMaskBit(c_wmCrowdedSector);
        // Boarding creates a new hit test. Restore the canonical pose after
        // that replacement, never through the old site or its frame pointers.
        const bool strategic=e.flags&Net::Strategic;
        auto& status=g.shipPaneStatus(ship);status.replicaIdentity=e.identity.serial;status.dockedAt=double(e.activation)*.001;
        if(auto* thing=static_cast<Impl::Thing*>(ship->GetThingSite())){
            thing->replicaStrategic=strategic;
            if(!strategic)motion(ship,e.motion);
            // Bootstrap observation is independent of whether the player has
            // opened the roster. Updates are captured again on loss of sight.
            auto cached=g.playerPaneMemory.find(ship->GetObjectID());
            if((e.side==incomingSide || (e.flags&Net::Eye)) && (cached==g.playerPaneMemory.end() || cached->second.identity!=e.identity.serial))rosterBootstrap.push_back(ship->GetObjectID());
        }
        static_cast<CshipIGC *>(ship)->NativeRestoreMotionState(strategic?ship->GetEngineVector():vector(e.engine), e.cloaking, e.ripcord);
    }
    for (auto it = g.replicas.begin(); it != g.replicas.end();)
        if (!present.contains(it->first)) {
            TRef<ImodelIGC> keep = it->second;
            retire(keep);
            it = g.replicas.erase(it);
        } else
            ++it;
    for (auto &t : frame.teams) {
        auto *side = g.mission->GetSide(t.id);
        side->SetActiveF(t.active);
        if(t.active)g.options.inactiveTeams&=~(1u<<t.id);else g.options.inactiveTeams|=1u<<t.id;
        g.commanders[t.id] = t.commander;
        g.funds[t.id] = t.funds;
    }
    g.player = g.mission->GetShip(frame.status.pilot);
    if (!g.player)
        throw std::runtime_error("Snapshot omits controlled pilot");
    if(g.player->GetSide()->GetObjectID()>=0)g.options.team=g.player->GetSide()->GetObjectID();
    if (auto *side = g.player->GetSide();
        side->GetObjectID()>=0 && side->GetBuckets()->n() == 0 && g.mission->GetMissionParams()->bAllowDevelopments)
        side->CreateBuckets();
    g.accounts.clear();
    auto ownAccount=std::find_if(frame.pilots.begin(),frame.pilots.end(),[&](const auto& p){return p.id==frame.status.pilot;});
    g.observedCredits.audience(g.player->GetSide()->GetObjectID(),ownAccount==frame.pilots.end()?0:ownAccount->identity);
    if(g.pilotWings.empty()){
        g.pilotWings.resize(shipIdentityCapacity,NA);
        g.pilotDonationTargets.resize(shipIdentityCapacity,NA);
    }
    for(auto id:g.pilotWingIds){g.pilotWings[id]=NA;g.pilotDonationTargets[id]=NA;}
    g.pilotWingIds.clear();
    for (const auto &p : frame.pilots) {
        g.accounts[p.id] = p.side==g.player->GetSide()->GetObjectID()?p.money:g.observedCredits.receive(p.id,p.side,p.identity,uint32_t(p.money));
        if(p.side>=0 && p.side==g.player->GetSide()->GetObjectID()) {
            g.pilotWings[p.id]=p.wing;g.pilotDonationTargets[p.id]=p.autoDonate;g.pilotWingIds.push_back(p.id);
            if(auto* ship=g.mission->GetShip(p.id))ship->SetWingID(p.wing);
        }
        if(auto* ship=g.mission->GetShip(p.id))ship->SetAutoDonate(g.mission->GetShip(p.autoDonate));
    }
    g.playerPaneViewer();for(auto id:rosterBootstrap)if(auto* ship=g.mission->GetShip(id))g.observePlayerPaneShip(ship);
    for (auto &b : frame.buckets)
        if (auto *side = g.mission->GetSide(b.side))
            if (auto *bucket = side->GetBucket(b.id))
                bucket->SetTimeAndMoney(DWORD(b.time), b.money);
    g.moneyRequests.clear();
    if (frame.status.moneySender >= 0) {
        MoneyRequest request;
        request.sender = frame.status.moneySender;
        request.amount = frame.status.moneyAmount;
        request.hull = frame.status.moneyHull;
        auto member=std::find_if(frame.members.begin(),frame.members.end(),[&](const auto& p){return p.id==request.sender;});
        if(member!=frame.members.end())request.name=member->name;
        else if(auto* sender=g.mission->GetShip(request.sender))request.name=sender->GetName();
        auto* type=g.mission->GetHullType(HullID(request.hull));
        request.prompt=Allegiance::MoneyRequestPrompt(request.name,request.amount,type?type->GetName():"");
        request.displayPrompt=censorChat(request.prompt,g.chatPrefs.censor);
        g.moneyRequests[frame.status.pilot] = std::move(request);
    }
    if (g.player->GetAutopilot() && !g.autopilotInputInitialized) {
        // Engage against the last raw device sample, before the next input
        // tick can move it. Publication timing must not redefine the neutral
        // position or absorb the first manual movement into that baseline.
        g.autopilotAxes = g.lastNetworkDeviceAxes;
        g.autopilotInputInitialized = true;
    }
    g.selectedWeapon=Mount(frame.status.selectedWeapon);g.groupFire=frame.status.groupFire!=0;
    g.state.networkLobby = (!g.onTeam() || frame.status.stage != STAGE_STARTED) && !frame.status.complete;
    g.state.networkHost = frame.status.host == frame.status.pilot;
    g.state.networkHostPilot = frame.status.host;
    g.state.networkCountdown = frame.status.countdown;
    g.state.networkReady = false;
    g.state.lobbyFlags=frame.status.lobbyFlags;g.state.autoAccept=frame.status.autoAccept;
    g.state.forceReady=frame.status.forceReady;g.state.readySides=frame.status.readySides;
    g.state.networkPilots.clear();
    std::bitset<shipIdentityCapacity> publicPilots;
    for (const auto& p : frame.members) {
        if(p.id>=0 && p.id<shipIdentityCapacity)publicPilots.set(p.id);
        auto cached=g.playerPaneMemory.find(p.id);
        if(cached!=g.playerPaneMemory.end() && (!cached->second.human || cached->second.side!=p.side || cached->second.name!=p.name))g.playerPaneMemory.erase(cached);
        if(p.id==frame.status.pilot)g.state.networkReady=p.flags&1;
        g.state.networkPilots.push_back({p.id,p.side,p.name,bool(p.flags&1),p.id==frame.status.host,
            bool(p.flags&4),p.id==frame.status.pilot,g.muted(p.id),p.requested,
            p.side>=0 && p.side==g.player->GetSide()->GetObjectID()?g.pilotWings[p.id]:NA,p.kills,p.deaths,p.ejections});
    }
    std::erase_if(g.playerPaneMemory,[&](const auto& item){return item.second.human && !publicPilots.test(item.first) && !g.mission->GetShip(item.first);});
    g.observedCredits.retain([&](int id){return publicPilots.test(id) || g.accounts.contains(id) || g.mission->GetShip(id);});
    g.state.complete = frame.status.complete;
    g.state.dead = frame.status.dead;
    g.state.result = frame.status.result;
    g.state.endgame.frozen=frame.status.resultFlags&1;g.state.endgame.pods=frame.status.resultFlags&2;g.state.endgame.winner=frame.status.winner;
    g.state.endgame.players=frame.resultPlayers;g.state.endgame.teams=frame.resultTeams;
    g.elapsed = double(frame.header.tick) / Net::tickRate;
    g.state.time = g.elapsed;
    if(g.onTeam() && !frame.shipIntel.empty()) {
        auto& records=g.shipIntel[g.player->GetSide()->GetObjectID()];records.clear();
        for(const auto& intel:frame.shipIntel)records.emplace(intel.id,intel);
        // Full faction status supersedes client-only sightings. Removal is an
        // authoritative roster retirement, including an unseen drone's death.
        std::erase_if(g.playerPaneMemory,[&](const auto& item){return !item.second.human && !records.contains(ShipID(item.first));});
        for(const auto& [id,intel]:records) {
            auto row=g.playerPaneRow(intel);auto previous=g.playerPaneMemory.find(id);
            if(previous!=g.playerPaneMemory.end() && previous->second.identity==intel.serial) {
                row.kills=previous->second.kills;row.deaths=previous->second.deaths;
            }
            g.playerPaneMemory.insert_or_assign(id,std::move(row));
        }
    }
    if(g.player && g.player->GetSide()->GetObjectID()>=0){
        auto& highlights=g.sectorHighlights[g.player->GetSide()->GetObjectID()];highlights.clear();
        for(auto& v:frame.highlights)if(g.mission->GetCluster(SectorID(v.sector)))highlights.insert(v.sector);
        std::erase_if(g.dismissedSectorHighlights,[&](int id){return !highlights.contains(id);});
    }
    g.viewCluster = g.commandViewEnabled && g.commandCluster ? static_cast<IclusterIGC *>(g.commandCluster)
                                                             : g.player->GetSourceShip()->GetCluster();
    if (!g.viewCluster && g.player->GetStation())
        g.viewCluster = g.player->GetStation()->GetCluster();
    if (previousSeat != cameraSeat())
        g.resetCamera();
    g.economyDirty = true;
    g.updateEconomy();
    const bool newAudience=firstReport || previousAudience!=g.player->GetSide()->GetObjectID();
    for (auto &n : frame.notices)
        if (n.number > g.lastNotice) {
            g.lastNotice = n.number;
            if(n.flags&Net::NoticeBallot){g.receiveBallot(n);continue;}
            if(n.flags&Net::NoticeSharedTip){
                if(newAudience || n.sender==g.player->GetObjectID() || n.voice==g.player->GetObjectID())continue;
                g.PlaySoundEffect(n.flags&Net::NoticeWarning?newCriticalMsgSound:newNonCriticalMsgSound);
            }
            bool chat=n.flags&Net::NoticeChat,leader=n.flags&Net::NoticeLeader;
            if(chat) {
                auto flag=[&](Net::NoticeFlag bit){return bool(n.flags&bit);};
                auto disposition=Allegiance::FilterChat(g.chatPrefs,flag(Net::NoticeSender),flag(Net::NoticeHuman),g.muted(n.sender),
                    flag(Net::NoticeEveryone),flag(Net::NoticeHasText),flag(Net::NoticeHasVoice),flag(Net::NoticeKnownVoice),flag(Net::NoticeQuick),
                    flag(Net::NoticeLobby),flag(Net::NoticeIndividual),!g.state.networkLobby);
                if(disposition.discard)continue;
                if(!flag(Net::NoticeSilent) || disposition.quietVoice)
                    g.PlaySoundEffect(Allegiance::ChatSound(disposition.quietVoice?NA:n.voice,flag(Net::NoticeCommand),flag(Net::NoticeEcho),
                        flag(Net::NoticePersonal),flag(Net::NoticeTraining),true,leader && n.sender!=g.player->GetObjectID()));
            }
            auto color=Net::noticeColor(n.color);
            g.message(n.text,n.flags&Net::NoticeWarning,{color[0],color[1],color[2],color[3]},chat,leader,&n);
        }
    for (auto &event : frame.events)
        if (event.number > g.lastNetworkEvent) {
            g.lastNetworkEvent = event.number;
            if(event.kind==Net::EventKind::Explosion && event.model.type==OT_ship) {
                auto cached=g.playerPaneMemory.find(event.model.id);
                auto* live=g.mission->GetShip(event.model.id);
                if(cached!=g.playerPaneMemory.end() && cached->second.identity==event.model.serial && !cached->second.human &&
                    (!live || g.playerPaneIdentity(live)!=event.model.serial))g.playerPaneMemory.erase(cached);
            }
            if (g.elapsed - event.time > 2)
                continue;
            auto *model = event.model.serial ? nullptr : g.mission->GetModel(event.model.type, event.model.id);
            auto found = g.replicas.find(event.model.serial);
            if (found != g.replicas.end())
                model = found->second;
            if (model && model->GetMission() != g.mission)
                model = nullptr;
            switch (event.kind) {
            case Net::EventKind::Explosion:
                if (g.fx && g.viewCluster && event.sector == g.viewCluster->GetObjectID())
                    g.fx->explosion(event.sector, {event.position.x, event.position.y, event.position.z},
                                    {event.velocity.x, event.velocity.y, event.velocity.z}, event.radius, event.value);
                if (g.viewCluster && event.sector == g.viewCluster->GetObjectID())
                    g.sounds.push_back({Allegiance::ExplosionSound(event.radius),
                                        {event.position.x, event.position.y, event.position.z},
                                        {event.velocity.x, event.velocity.y, event.velocity.z},
                                        false});
                break;
            case Net::EventKind::HullHit:
                if (model) {
                    auto *thing = static_cast<Impl::Thing *>(model->GetThingSite());
                    thing->AddHullHit(vector(event.position), vector(event.normal));
                }
                break;
            case Net::EventKind::ShieldHit:
                if (model) {
                    auto ellipse = vector(event.ellipse);
                    static_cast<Impl::Thing *>(model->GetThingSite())
                        ->AddFlare(time, vector(event.position), event.value, event.flags ? &ellipse : nullptr);
                }
                break;
            case Net::EventKind::Sound:
                if ((event.flags & Net::SoundImpact) && model && Allegiance::ReceivesShipNotification(g.player, model))
                    g.PlaySoundEffect(Allegiance::ShipImpactSound(true, event.value == otherShieldHitSound), g.player);
                else if (event.pilot == g.player->GetObjectID())
                    g.PlaySoundEffect(event.value,(event.flags&Net::SoundOwnShip)?model:nullptr);
                else if(event.pilot<0 && event.sector<0){
                    if(!newAudience)g.PlaySoundEffect(event.value);
                }
                else if (g.viewCluster && event.sector == g.viewCluster->GetObjectID() && (!model || model != g.player))
                    g.sounds.push_back({event.value,
                                        {event.position.x, event.position.y, event.position.z},
                                        {event.velocity.x, event.velocity.y, event.velocity.z},
                                        false});
                break;
            case Net::EventKind::Damage:
                if (model)
                    static_cast<Impl::Thing *>(model->GetThingSite())->AddDamage(vector(event.position), event.radius);
                break;
            case Net::EventKind::Feedback:
                if (event.value != effectFire && event.pilot == g.player->GetObjectID())
                    g.feedback.add(ForceEffectID(event.value), LONG(event.flags));
                break;
            case Net::EventKind::Camera:
                if (event.pilot == g.player->GetObjectID()) {
                    g.resetCamera();
                    g.cinematicCluster = g.mission->GetCluster(event.sector);
                    g.cinematicDeath = event.flags & 1;
                    g.cinematicStop = event.time + Allegiance::OverrideCameraDuration(g.cinematicDeath);
                    if (g.cinematicDeath) {
                        g.cinematicPosition = vector(event.position);
                        g.cinematicOrientation.Set(vector(event.normal), vector(event.ellipse));
                    } else {
                        g.cinematicTarget = vector(event.position);
                        g.cinematicOrientation = Allegiance::OverrideCameraOrientation();
                        if (!(event.flags & 2)) {
                            g.throttleResetPending = g.forcedThrottleReset = true;
                            g.throttleReset = 1;
                        }
                    }
                    g.cameraMotion.SetAnimation(0);
                    g.cameraMotion.SetHeadOrientation(0);
                    g.cameraMotion.SetOrientation(g.cinematicOrientation);
                }
                break;
            case Net::EventKind::Jiggle:
                if (event.pilot == g.player->GetObjectID())
                    g.cameraMotion.SetJiggle(event.radius);
                break;
            }
        }
    const auto* lifecycle=g.mission->GetMissionParams();
    const bool autoRestart=(frame.status.lobbyFlags&Allegiance::LobbyAutoRestart)!=0,
        autoStart=(frame.status.lobbyFlags&Allegiance::LobbyAutoStart)!=0,
        gameOpenLocked=(frame.status.lobbyFlags&Allegiance::LobbyGameOpenLocked)!=0,
        allowRestart=(frame.status.lobbyFlags&Allegiance::LobbyNoRestart)==0,
        allowEmpty=(frame.status.lobbyFlags&Allegiance::LobbyRequirePlayers)==0;
    const int minimum=allowEmpty?1:Allegiance::LobbyMinimumPlayers(frame.status.lobbyFlags);
    g.options.requireTeamPlayers=!allowEmpty;
    if(lifecycle->bAutoRestart!=autoRestart || lifecycle->bAutoStart!=autoStart || lifecycle->bLockGameOpen!=gameOpenLocked || lifecycle->bAllowRestart!=allowRestart || lifecycle->bAllowEmptyTeams!=allowEmpty || lifecycle->nMinPlayersPerTeam!=minimum){
        auto updated=*lifecycle;updated.bAutoRestart=autoRestart;updated.bAutoStart=autoStart;updated.bLockGameOpen=gameOpenLocked;updated.bAllowRestart=allowRestart;updated.bAllowEmptyTeams=allowEmpty;updated.nMinPlayersPerTeam=minimum;
        g.mission->SetMissionParams(&updated);
    }
    if (g.mission->GetMissionStage() != frame.status.stage) {
        if (frame.status.stage == STAGE_STARTED)
            g.mission->EnterGame();
        g.mission->SetMissionStage(STAGE(frame.status.stage));
    }
    // Events can start the original launch/death camera. Publish frontend
    // state after applying them, including when several peers' captures arrive
    // together and no intervening prediction tick refreshes the first pilot.
    if(g.onTeam() && g.player->IsGhost() && (firstReport || !previousGhost)) {
        // FedSrv retains the death sector as the observer's view sector;
        // WinTrek returns this pilot to vmCommand after the override. A cold
        // replica may have only the original death event or its restart base.
        auto* sector=g.cinematicCluster?static_cast<IclusterIGC*>(g.cinematicCluster):static_cast<IclusterIGC*>(previousFlightSector);
        if(!sector && g.player->GetStation())sector=g.player->GetStation()->GetCluster();
        g.commandViewEnabled=true;g.commandCluster=sector;g.commandTracking=false;g.recenterCommand(0);g.cameraValid=false;
    }
    g.resolveCommandFocus();g.refresh();
}
void Game::presentNetwork(std::span<const Net::Entity> entities) {
    auto &g = *impl_;
    if (g.gameRole != GameRole::Replica || !g.player)
        return;
    restoreNetworkPresentation();
    for (auto &e : entities) {
        if(e.flags&Net::Strategic)continue;
        if ((e.identity.type == OT_ship &&
             (e.identity.id == g.player->GetObjectID() || e.identity.id == g.player->GetSourceShip()->GetObjectID())) ||
            (e.shot &&
             (e.launcher == g.player->GetObjectID() || e.launcher == g.player->GetSourceShip()->GetObjectID() ||
              uint16_t(e.shot >> 48) == uint16_t(g.player->GetSourceShip()->GetObjectID()))))
            continue;
        auto it = g.replicas.find(e.identity.serial);
        if (it != g.replicas.end()) {
            auto *model = static_cast<ImodelIGC *>(it->second);
            if (model->GetMission() != g.mission)
                continue;
            g.presentationPoses.push_back({model, model->GetPosition(), model->GetVelocity(), model->GetOrientation()});
            motion(model,e.motion);
        }
    }
}
void Game::replayWeaponAction(const Net::Action& action) {
    auto& g=*impl_;if(g.gameRole!=GameRole::Replica)return;
    if(action.op==Net::Operation::Weapon && (action.args[1]==0 || action.args[1]==1))g.setWeaponSelection(action.args[0],action.args[1]!=0);
}
void Game::replayInvestments(std::span<const InvestmentIntent> pending){
    auto& g=*impl_;if(g.gameRole!=GameRole::Replica || !g.onTeam())return;
    bool changed=false;
    for(const auto& intent:pending){
        if(intent.side!=g.player->GetSide()->GetObjectID() || intent.amount<=0)continue;
        IbucketIGC* bucket=nullptr;Money amount=g.planInvestment(intent.bucket,intent.amount,bucket);
        if(amount>0){g.fundInvestment(bucket,amount);changed=true;}
    }
    // One economy/presentation refresh for the whole bounded pending batch.
    if(changed){g.updateEconomy();g.refresh();}
}
// Presentation extrapolation is reversible; it never spends resources, fires a
// weapon, advances the mission clock or feeds a display correction into IGC.
void Game::restoreNetworkPresentation() {
    auto &g = *impl_;
    if (g.presentationPoses.empty())
        return;
    for (auto &pose : g.presentationPoses)
        if (pose.model->GetMission()) {
            pose.model->SetPosition(pose.position);
            pose.model->SetVelocity(pose.velocity);
            pose.model->SetOrientation(pose.orientation);
        }
    g.presentationPoses.clear();
}
void Game::presentPrediction(double fraction, const PresentationCorrection &correction, double frameDt) {
    auto &g = *impl_;
    if (g.gameRole != GameRole::Replica || !g.player)
        return;
    // The camera is a render-frame consumer. Snapshot/replay can replace the
    // canonical clock, while several display frames share one simulation tick.
    // Visual lifetimes are also render-frame consumers: docked command views
    // do not produce predicted inputs, and reconciliation must not age effects.
    if(g.fx)g.fx->step(float(std::clamp(frameDt,0.,.1)));
    if (!g.networkRenderTimeValid) {
        g.networkRenderTime = g.elapsed + fraction;
        g.networkRenderTimeValid = true;
    } else
        g.networkRenderTime += std::clamp(frameDt, 0., .1);
    // Docked clients send receipts, not predicted flight inputs. Complete the
    // local override on the presentation clock without advancing their IGC.
    if(g.cinematicStop>=0 && g.networkRenderTime>=g.cinematicStop)
        g.endCinematic();
    if(std::exchange(g.recoverySoundPending,false) && g.player->GetStation() && !g.state.complete)
        g.PlaySoundEffect(personalJumpSound);
    if (!g.player->GetSourceShip()->GetCluster()) {
        g.refresh();
        return;
    }
    // The fractional tick plus sample-to-display lead may cross a tick boundary;
    // it is reversible projection, not another simulated/input-producing tick.
    float dt = float(std::clamp(fraction, 0., .1));
    auto pose = [&](ImodelIGC *model, bool own) {
        g.presentationPoses.push_back({model, model->GetPosition(), model->GetVelocity(), model->GetOrientation()});
        auto p = model->GetPosition() + model->GetVelocity() * dt;
        if (own)
            p += vec(correction.position);
        model->SetPosition(p);
    };
    auto *source = g.player->GetSourceShip();
    pose(source, true);
    auto *turning = g.player->GetParentShip() ? static_cast<IshipIGC *>(g.player) : source;
    if (turning != source)
        pose(turning, false);
    auto rotate = [&](IshipIGC *ship, const glm::mat3 &error) {
        Orientation o = ship->GetOrientation();
        o.Yaw(ship->GetCurrentTurnRate(c_axisYaw) * dt);
        o.Pitch(-ship->GetCurrentTurnRate(c_axisPitch) * dt);
        o.Roll(ship->GetCurrentTurnRate(c_axisRoll) * dt);
        o.Renormalize();
        o.Set(vec(error * vec(o.GetForward())), vec(error * vec(o.GetUp())));
        ship->SetOrientation(o);
    };
    rotate(source, correction.sourceRotation);
    if (turning != source)
        rotate(turning, correction.turretRotation); // Crew eye offsets rotate with the parent hull between ticks too.
    for (auto &[_, model] : g.shots)
        if (model->GetCluster() && g.ownsShot(model))
            pose(model, false);
    g.refresh();
}
std::string Game::networkRejected(const Net::Action& action) {
    // Concurrent funding/technology changes are ordinary bucket corrections;
    // WinTrek/FedSrv refund them without an action-failure popup or chat tip.
    if(action.op==Net::Operation::Invest)return {};
    // RANDOMIZE_TEAMS is ignored by FedSrv outside NotStarted; TeamScreen
    // deliberately leaves the owner's button enabled through other stages.
    if(action.op==Net::Operation::Lobby && action.args[0]==int(LobbyAction::Randomize))return {};
    if(action.op==Net::Operation::PurchaseLoadout)return "This loadout cannot be purchased here.";
    if(action.op==Net::Operation::Board)return boardingFailure(action.args[0]);
    if(action.op==Net::Operation::Teleport)return "Teleportation failed.";
    if(action.op==Net::Operation::Leave)return boardingFailure(-1);
    std::string message="Server declined the requested action.";impl_->PostPlainText(true,message.c_str());return message;
}
FlightInput Game::canonicalNetworkInput(const FlightInput &input) {
    auto &g = *impl_;
    auto result = input;
    result.canonical = true;
    result.fieldOfView = -1;
    ControlData controls;
    controls.jsValues[c_axisYaw] = input.yaw;
    controls.jsValues[c_axisPitch] = input.pitch;
    controls.jsValues[c_axisRoll] = input.roll;
    controls.jsValues[c_axisThrottle] = input.throttle * 2 - 1;
    // Test raw device motion before view scaling and byte quantization. Replay
    // and authority consume the explicit override instead of device settings.
    if (input.active && g.player && g.player->GetAutopilot()) {
        if (!g.autopilotInputInitialized) {
            std::copy_n(controls.jsValues, 3, g.autopilotAxes.begin());
            g.autopilotInputInitialized = true;
        } else
            result.manualOverride |= Allegiance::AutopilotAxesChanged(controls, g.autopilotAxes.data(), input.deadZone);
    } else if (!g.player || !g.player->GetAutopilot())
        g.autopilotInputInitialized = false;
    if (g.player && g.state.camera == CameraMode::Cockpit && g.player->GetTurretID() == NA &&
        g.player->GetBaseHullType())
        Allegiance::LimitCockpitSlew(controls, g.player->GetHullType(), RadiansFromDegrees(g.state.viewFov),
                                   RadiansFromDegrees(50.f));
    result.yaw = controls.jsValues[c_axisYaw];
    result.pitch = controls.jsValues[c_axisPitch];
    result.roll = controls.jsValues[c_axisRoll];
    g.lastNetworkDeviceAxes = {input.yaw, input.pitch, input.roll};
    return result;
}
void Game::predict(const FlightInput &input, double dt, bool effects) {
    restoreNetworkPresentation();
    auto &g = *impl_;
    if (g.gameRole != GameRole::Replica || !g.player)
        return;
    Allegiance::SimulationRandomScope random(g.randomState);
    dt = std::clamp(dt, 0., .1);
    // IGC's clock is integer milliseconds. Quantize the absolute tick endpoint,
    // as authorityStep does, instead of rounding every 1/60 step up to 17 ms.
    auto begin = g.start + float(g.elapsed), end = g.start + float(g.elapsed + dt);
    g.networkReplaying = !effects;
    if (g.cinematicStop >= 0 && g.elapsed >= g.cinematicStop)
        g.endCinematic();
    g.applyFlight(input);
    auto *source = g.player->GetSourceShip();
    if (source->GetCluster() && !g.state.dead && !g.state.complete) {
        Vector v = source->GetVelocity();
        Orientation o = source->GetOrientation();
        if (g.player->GetParentShip()) {
            source->ExecuteShipMove(begin, end, &v, &o);
            source->SetVelocity(v);
            source->SetOrientation(o);
            o = g.player->GetOrientation();
            g.player->ExecuteTurretMove(begin, end, &o);
            g.player->SetOrientation(o);
        } else {
            source->ExecuteShipMove(begin, end, &v, &o);
            source->SetVelocity(v);
            source->SetOrientation(o);
        }
        if (g.player->GetParentShip()) {
            auto *w = static_cast<IweaponIGC *>(source->GetMountedPart(ET_Weapon, g.player->GetTurretID()));
            if (w)
                w->Update(end);
            g.player->SetLastUpdate(end);
            source->SetLastUpdate(end);
        } else
            source->Update(end);
        source->SetPosition(source->GetPosition() + source->GetVelocity() * (end - begin));
    }
    {
        auto &projectiles = g.predictionProjectiles;
        projectiles.clear();
        for (auto &[_, m] : g.shots)
            if (m->GetMission() && g.ownsShot(m))
                projectiles.emplace_back(m);
        for (auto &m : projectiles) {
            // Updating one missile can retire another projectile in this
            // retained work list. Do not read its released hit test afterward.
            if (m->GetMission() != g.mission)
                continue;
            auto timeStart = m->GetLastUpdate();
            if (end > timeStart) {
                if (m->GetObjectType() == OT_projectile &&
                    static_cast<CprojectileIGC *>(static_cast<ImodelIGC *>(m))->NativeExpireTime() <= end) {
                    if (!g.confirmedShots.contains(
                            static_cast<CprojectileIGC *>(static_cast<ImodelIGC *>(m))->NativeShot()))
                        m->Terminate();
                    continue;
                }
                if (m->GetObjectType() == OT_missile) {
                    auto *missile = static_cast<CmissileIGC *>(static_cast<ImodelIGC *>(m));
                    if (g.confirmedShots.contains(missile->NativeShot()) && end >= missile->NativeExpireTime())
                        continue;
                    m->Update(end);
                    if (!m->GetCluster())
                        continue;
                }
                m->SetPosition(m->GetPosition() + m->GetVelocity() * (end - timeStart));
                m->SetLastUpdate(end);
            }
        }
        projectiles.clear(); // Retain capacity, never retain dead IGC objects between prediction ticks.
    }
    g.networkReplaying = false;
    g.elapsed += dt;
    static_cast<CmissionIGC *>(g.mission)->NativeSetTime(end);
    g.state.time = g.elapsed;
    g.refresh();
    if (effects) {
        if(std::exchange(g.recoverySoundPending,false) && g.player->GetStation() && !g.state.complete)g.PlaySoundEffect(personalJumpSound);
        g.engineLevels.Update(source, float(dt), !g.engineLevelsValid);
        g.engineLevelsValid = true;
        g.chaseMotion.Record(g.elapsed,
                             Vector(source->GetCurrentTurnRate(c_axisYaw), source->GetCurrentTurnRate(c_axisPitch),
                                    source->GetCurrentTurnRate(c_axisRoll)));
        g.emitEffects(float(dt));
    }
}
} // namespace Native
