// Read-only player-pane projection. Unknown opponents come from public
// membership, never a scan of hidden enemy ship state. Native UI remembers
// only information it has actually received or observed.
bool Game::Impl::shipIntelEye(IshipIGC* ship,IsideIGC* viewer)const {
    auto* source=ship->GetSourceShip();auto* thing=static_cast<Thing*>(source->GetThingSite());
    // GetCurrentEye deliberately returns true for untracked effects. A hull
    // load also briefly has that flag; neither authorizes roster knowledge.
    return source->GetCluster() && thing && thing->sideTracked && thing->visibility && thing->visibility->Eye(viewer);
}
void Game::Impl::observeShipIntel(IshipIGC* ship,IsideIGC* viewer) {
    if(uiPreview || terminating || gameRole==GameRole::Replica || !ship || !viewer ||
       viewer->GetObjectID()<0 || !ship->GetSide() || ship->GetSide()->GetObjectID()<0)return;
    using Status=Allegiance::PlayerShipStatus;
    auto* source=ship->GetSourceShip();auto* station=source->GetStation();auto* cluster=source->GetCluster();
    if(!cluster && station)cluster=station->GetCluster();
    auto& intel=shipIntel[viewer->GetObjectID()][ship->GetObjectID()];auto serial=playerPaneIdentity(ship);
    if(intel.serial!=serial || intel.side!=ship->GetSide()->GetObjectID())intel={};
    intel.serial=serial;intel.id=ship->GetObjectID();intel.side=ship->GetSide()->GetObjectID();intel.pilotType=ship->GetPilotType();
    Net::text(intel.name,ship->GetName());
    auto* hull=source->GetBaseHullType();auto state=ship->GetParentShip()?
        (ship->GetTurretID()==NA?Status::Observer:Status::Turret):station?Status::Docked:cluster?Status::Flying:Status::Dead;
    Allegiance::ObservePlayerShip(intel.status,state,(ship->GetParentShip() || state==Status::Dead)?NA:hull?hull->GetObjectID():NA,
        cluster?cluster->GetObjectID():NA,station?station->GetObjectID():NA,ship->GetParentShip()?source->GetObjectID():NA,uint32_t(std::max(0.,elapsed)*1000));
    if(state==Status::Docked)intel.status.stateTime=uint32_t(std::max(0.,shipPaneStatus(source).dockedAt)*1000);
    if(viewer!=ship->GetSide() && !IsideIGC::AlliedSides(viewer,ship->GetSide())) {
        if(!shipIntel[ship->GetSide()->GetObjectID()][ship->GetObjectID()].serial)observeShipIntel(ship,ship->GetSide());
        auto& own=shipIntel[ship->GetSide()->GetObjectID()][ship->GetObjectID()];
        own.status.flags|=Status::Detected;
    }
    // FedSrv reports passengers with their source pilot's sector and identity.
    if(source==ship)for(auto* l=ship->GetChildShips()->first();l;l=l->next())observeShipIntel(l->data(),viewer);
}
void Game::Impl::hideShipIntel(IshipIGC* ship,IsideIGC* viewer) {
    if(uiPreview || terminating || gameRole==GameRole::Replica || !viewer || viewer->GetObjectID()<0 ||
       !ship->GetSide() || ship->GetSide()->GetObjectID()<0 || IsideIGC::AlliedSides(viewer,ship->GetSide()))return;
    auto& records=shipIntel[viewer->GetObjectID()];
    if(auto i=records.find(ship->GetObjectID());i!=records.end())Allegiance::HidePlayerShip(i->second.status);
    bool detected=false;
    for(auto* l=mission->GetSides()->first();l;l=l->next())if(!IsideIGC::AlliedSides(l->data(),ship->GetSide())) {
        auto& other=shipIntel[l->data()->GetObjectID()];auto i=other.find(ship->GetObjectID());
        detected|=i!=other.end() && i->second.status.current();
    }
    auto& own=shipIntel[ship->GetSide()->GetObjectID()];
    if(auto i=own.find(ship->GetObjectID());i!=own.end()) {
        if(detected)i->second.status.flags|=Allegiance::PlayerShipStatus::Detected;
        else i->second.status.flags&=~Allegiance::PlayerShipStatus::Detected;
    }
    if(ship==ship->GetSourceShip())for(auto* l=ship->GetChildShips()->first();l;l=l->next())hideShipIntel(l->data(),viewer);
}
void Game::Impl::updateShipIntel(IshipIGC* ship,bool docking) {
    if(uiPreview || terminating || gameRole==GameRole::Replica || !ship->GetSide())return;
    auto* source=ship->GetSourceShip();
    for(auto* l=mission->GetSides()->first();l;l=l->next()) {
        auto* viewer=l->data();
        auto& records=shipIntel[viewer->GetObjectID()];auto previous=records.find(ship->GetObjectID());
        if(IsideIGC::AlliedSides(viewer,ship->GetSide()) || shipIntelEye(ship,viewer) ||
           (docking && previous!=records.end() && previous->second.status.current() && source->GetStation() && source->GetStation()->SeenBySide(viewer)))
            observeShipIntel(ship,viewer);
    }
}
PlayerPaneRow Game::Impl::playerPaneRow(const Net::ShipIntel& intel)const {
    PlayerPaneRow row;const auto& status=intel.status;auto* side=mission->GetSide(intel.side);
    row.id=intel.id;row.identity=intel.serial;row.side=intel.side;row.name=intel.name;row.type=intel.pilotType;
    row.human=intel.pilotType>=c_ptPlayer;row.controlled=player && row.id==player->GetObjectID();
    row.commander=commanders[row.side]==row.id;row.allied=player && IsideIGC::AlliedSides(player->GetSide(),side);
    row.current=status.current();row.sector=status.sector;row.parent=status.parent;
    auto* hull=mission->GetHullType(status.hull);auto* cluster=mission->GetCluster(status.sector);
    row.ship=hull?hull->GetName():"<none>";row.location=row.sectorName=cluster?cluster->GetName():"unknown";
    row.icon=hull?(hull->GetIconName()[0]?std::string(hull->GetIconName())+"bmp":"ejectpodbmp"):"unknownshipbmp";
    row.docked=status.state==Allegiance::PlayerShipStatus::Docked;
    row.dockedSeconds=row.docked?int(std::max(0.,elapsed-double(status.stateTime)*.001)):0;
    if(status.state==Allegiance::PlayerShipStatus::Observer || status.state==Allegiance::PlayerShipStatus::Turret) {
        row.ship.clear();row.icon=status.state==Allegiance::PlayerShipStatus::Observer?"iconobserverbmp":"iconturretbmp";
        auto& records=shipIntel[player->GetSide()->GetObjectID()];auto parent=records.find(status.parent);
        if(parent!=records.end())row.location=std::string("aboard ")+parent->second.name;
    }
    return row;
}
void Game::Impl::playerPaneViewer() {
    int side=onTeam()?player->GetSide()->GetObjectID():NA;
    if(playerPaneSide!=side){playerPaneSide=side;playerPaneMemory.clear();}
}
PlayerPaneRow Game::Impl::playerPaneRow(IshipIGC* ship,bool current)const {
    PlayerPaneRow row;auto* side=player->GetSide();auto* source=ship->GetSourceShip();
    row.id=ship->GetObjectID();row.identity=playerPaneIdentity(ship);
    row.side=ship->GetSide()->GetObjectID();row.human=ship->GetPilotType()>=c_ptPlayer;row.allied=IsideIGC::AlliedSides(side,ship->GetSide());
    row.name=ship->GetName();row.type=ship->GetPilotType();row.controlled=ship==player;
    row.commander=row.side>=0 && commanders[row.side]==row.id;row.current=current;
    if(current) {
        auto* hull=source->GetBaseHullType();auto* station=source->GetStation();auto* cluster=source->GetCluster();
        if(!cluster && station)cluster=station->GetCluster();
        row.sector=cluster?cluster->GetObjectID():NA;row.location=row.sectorName=cluster?cluster->GetName():"unknown";
        row.ship=hull?hull->GetName():"<none>";row.docked=station!=nullptr;row.stayDocked=ship->GetStayDocked();
        auto status=shipPaneStatuses.find(source->GetObjectID());
        row.dockedSeconds=station && status!=shipPaneStatuses.end()?int(std::max(0.,elapsed-status->second.dockedAt)):0;
        row.parent=ship->GetParentShip()?ship->GetParentShip()->GetObjectID():NA;
        if(row.parent>=0){row.location=std::string("aboard ")+source->GetName();row.ship.clear();row.icon=ship->GetTurretID()==NA?"iconobserverbmp":"iconturretbmp";}
        else row.icon=hull?(hull->GetIconName()[0]?std::string(hull->GetIconName())+"bmp":"ejectpodbmp"):"unknownshipbmp";
        row.kills=ship->GetKills();row.deaths=ship->GetDeaths();
        auto command=ship->GetCommandID(c_cmdAccepted);if(command>0 && command<c_cidMax)row.command=command;
        auto* target=ship->GetCommandTarget(c_cmdAccepted);
        if(command>=0 && command<c_cidMax && ship->LegalCommand(command,target)) {
            row.objective=c_cdAllCommands[command].szVerb;
            if(target) {
                auto* thing=static_cast<Thing*>(target->GetThingSite());auto* observation=thing?thing->stationObservation(side):nullptr;
                row.objective+=' ';row.objective+=observation?observation->name:GetModelName(target);
            }
        }
        if(ship->GetSide()==side){row.wing=ship->GetWingID();row.money=row.human?readBalance(ship):-1;row.autoDonate=ship->GetAutoDonate()?ship->GetAutoDonate()->GetObjectID():NA;}
    }else {row.location=row.sectorName="unknown";row.ship="<none>";row.icon="unknownshipbmp";}
    return row;
}
void Game::Impl::observePlayerPaneShip(IshipIGC* ship) {
    if(gameRole==GameRole::Authority || terminating || !onTeam())return;
    playerPaneViewer();auto row=playerPaneRow(ship,true);playerPaneMemory.insert_or_assign(row.id,std::move(row));
}
bool Game::bootPlayer(int id,int expectedSide) {
    // BootSink resolves the outstanding request again when OK is pressed.
    // Never retain a ship pointer or use the pane's later selected row.
    auto data=playerPaneData();
    auto row=std::find_if(data.rows.begin(),data.rows.end(),[&](const auto& r){return r.id==id;});
    if(data.closed || data.side!=expectedSide || !data.commander || row==data.rows.end() ||
       !row->human || row->controlled || (!row->requesting &&
       !Allegiance::CanBootTeammate(data.commander,row->controlled,row->side==data.side,data.teamSettingsLocked)))return false;
    if(impl_->gameRole==GameRole::Replica)
        return impl_->submit(Net::Operation::Lobby,{int(LobbyAction::FlightBoot),expectedSide,id});
    auto effect=Allegiance::ConfirmedPlayerBoot(row->requesting,impl_->options.rules.allowDefections);
    bool accepted=effect==Allegiance::PlayerBootEffect::LeaveTeam?moveNetworkPilotToTeam(id,SIDE_TEAMLOBBY):removeLocalPilot(id);
    if(accepted && effect==Allegiance::PlayerBootEffect::LeaveTeam)lobbyBootNotice(id,expectedSide);
    return accepted;
}
PlayerPaneData Game::playerPaneData() {
    auto& g=*impl_;PlayerPaneData result;if(!g.player || !g.onTeam())return result;
    auto* side=g.player->GetSide();result.self=g.player->GetObjectID();result.side=side->GetObjectID();
    result.money=g.readBalance(g.player);result.donating=g.player->GetAutoDonate()?g.player->GetAutoDonate()->GetObjectID():NA;
    result.commander=g.commanders[result.side]==result.self;result.training=bool(g.training);result.closed=g.gameplayClosed();result.teamSettingsLocked=g.options.rules.lockTeamSettings;
    g.playerPaneViewer();
    std::bitset<shipIdentityCapacity> physical,publicPilots;std::array<uint16_t,shipIdentityCapacity> index;
    for(auto* l=g.mission->GetShips()->first();l;l=l->next()) {
        auto* ship=l->data();int id=ship->GetObjectID();if(id<0 || id>=shipIdentityCapacity)continue;
        bool human=ship->GetPilotType()>=c_ptPlayer;auto* source=ship->GetSourceShip();
        bool own=ship->GetSide()==side,current=own || (source->GetCluster() && source->GetCurrentEye(side));
        uint32_t identity=g.playerPaneIdentity(ship);
        auto& intel=g.shipIntel[result.side];auto report=intel.find(id);
        if(report!=intel.end() && report->second.serial==identity && !current)
            g.playerPaneMemory.insert_or_assign(id,g.playerPaneRow(report->second));
        auto known=g.playerPaneMemory.find(id);if(known!=g.playerPaneMemory.end() && known->second.identity!=identity){g.playerPaneMemory.erase(known);known=g.playerPaneMemory.end();}
        if(!human && !current && known==g.playerPaneMemory.end())continue;
        if(!own && !current && known!=g.playerPaneMemory.end()) {
            auto row=known->second;if(!g.shipIntel[result.side].contains(ShipID(id)))row.current=false;index[id]=uint16_t(result.rows.size());result.rows.push_back(std::move(row));physical.set(id);continue;
        }
        auto row=g.playerPaneRow(ship,current);
        if(g.gameRole!=GameRole::Replica && current)g.observeShipIntel(ship,side);
        report=intel.find(id);
        if(report!=intel.end() && report->second.serial==identity) {
            auto status=g.playerPaneRow(report->second);status.wing=row.wing;status.money=row.money;status.autoDonate=row.autoDonate;
            status.kills=row.kills;status.deaths=row.deaths;status.command=row.command;status.objective=std::move(row.objective);status.stayDocked=row.stayDocked;row=std::move(status);
        }
        if(row.current)g.playerPaneMemory.insert_or_assign(id,row);
        index[id]=uint16_t(result.rows.size());result.rows.push_back(std::move(row));physical.set(id);
    }
    for(const auto& pilot:g.state.networkPilots) {
        if(pilot.id<0 || pilot.id>=shipIdentityCapacity)continue;
        publicPilots.set(pilot.id);
        if(physical.test(pilot.id)){auto& row=result.rows[index[pilot.id]];row.name=pilot.name;row.side=pilot.team;row.commander=pilot.commander;row.requesting=pilot.requested==result.side;row.kills=pilot.kills;row.deaths=pilot.deaths;row.wing=pilot.team==result.side?pilot.wing:NA;continue;}
        PlayerPaneRow row;auto known=g.playerPaneMemory.find(pilot.id);
        if(known!=g.playerPaneMemory.end() && known->second.human && known->second.side==pilot.team && known->second.name==pilot.name){row=known->second;if(!g.shipIntel[result.side].contains(ShipID(pilot.id)))row.current=false;}
        else {row.location=row.sectorName="unknown";row.ship="<none>";row.icon="unknownshipbmp";}
        row.id=pilot.id;row.side=pilot.team;row.name=pilot.name;row.human=true;row.type=c_ptPlayer;
        row.commander=pilot.commander;row.controlled=pilot.controlled;row.requesting=pilot.requested==result.side;row.kills=pilot.kills;row.deaths=pilot.deaths;row.wing=pilot.team==result.side?pilot.wing:NA;
        index[pilot.id]=uint16_t(result.rows.size());result.rows.push_back(std::move(row));
    }
    std::erase_if(g.playerPaneMemory,[&](const auto& item){return item.second.human && !physical.test(item.first) && !publicPilots.test(item.first);});
    for(const auto& [id,known]:g.playerPaneMemory)if(!known.human && !physical.test(id)){auto row=known;if(!g.shipIntel[result.side].contains(ShipID(id)))row.current=false;result.rows.push_back(std::move(row));}
    // PlayerInfo membership and faction-private account reports do not require
    // a physical ship replica. Keep actions and captions valid in remote
    // sectors, and resolve passenger labels from the current public name.
    for(auto& row:result.rows){
        auto* owner=g.mission->GetSide(SideID(row.side));row.allied=owner && IsideIGC::AlliedSides(side,owner);
        if(row.human && row.side==result.side && g.gameRole==GameRole::Replica){
            auto account=g.accounts.find(row.id);row.money=account==g.accounts.end()?-1:account->second;
            row.autoDonate=g.pilotDonationTargets.empty()?NA:g.pilotDonationTargets[row.id];
        }
        if(row.human && row.side!=result.side)row.money=row.allied?g.alliedBalance(row.id,row.side):-1;
        if(row.parent>=0 && row.parent<shipIdentityCapacity && publicPilots.test(row.parent))
            row.location="aboard "+result.rows[index[row.parent]].name;
    }
    for(auto* l=g.mission->GetSides()->first();l;l=l->next()) {
        auto* team=l->data();PlayerPaneTeam row;row.id=team->GetObjectID();row.name=team->GetName();row.active=team->GetActiveF();
        constexpr const char* icons[]={"iconteamyellowbmp","iconteambluebmp","iconteampurplebmp","iconteamgreenbmp","iconteamredbmp","iconteamwhitebmp"};
        if(row.id>=0 && row.id<int(std::size(icons)))row.icon=icons[row.id];
        if(auto* civ=team->GetCivilization())row.civilizationIcon=std::string("icon")+civ->GetIconName()+"bmp";
        row.alliance=team->GetAllies();for(const auto& pilot:result.rows)if(pilot.side==row.id && pilot.human)++row.pilots;
        result.teams.push_back(std::move(row));
    }
    int maximum=shipIdentityCapacity,ownPilots=0;
    for(const auto& team:result.teams){
        if(team.id==result.side)ownPilots=team.pilots;
        if(g.options.rules.maxImbalance!=32767 && team.active && team.pilots>0)maximum=std::min(maximum,team.pilots+g.options.rules.maxImbalance);
    }
    result.acceptJoiners=ownPilots<maximum;
    PlayerPaneTeam lobby;lobby.id=SIDE_TEAMLOBBY;lobby.name="Not On A Team";for(const auto& row:result.rows)if(row.side==lobby.id)++lobby.pilots;result.teams.push_back(std::move(lobby));
    return result;
}
