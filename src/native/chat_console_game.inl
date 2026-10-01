// Original ConsoleData subjects and completion, exposed as stable IDs. Called
// on user interaction; no extra mission walks or allocations in the sim tick.
std::vector<ChatPeer> Game::chatPeers()const {
    const auto& g=*impl_;std::vector<ChatPeer> peers;std::bitset<shipIdentityCapacity> present;if(!g.onTeam())return peers;
    for(auto* l=g.mission->GetShips()->first();l;l=l->next()) {
        auto* ship=l->data();auto* side=ship->GetSide();if(!side)continue;
        if(!IsideIGC::AlliedSides(side,g.player->GetSide()) && ship->GetPilotType()<c_ptPlayer)continue;
        auto* source=ship->GetSourceShip();auto* cluster=source->GetCluster();
        if(!cluster && source->GetStation())cluster=source->GetStation()->GetCluster();
        peers.push_back({ship->GetObjectID(),side->GetObjectID(),ship->GetWingID(),cluster?cluster->GetObjectID():-1,
            ship->GetPilotType(),ship->GetName(),source->GetChildShips()->n()!=0});
        present.set(ship->GetObjectID());
    }
    // Public roster identities survive sector visibility filtering. Talking to
    // an enemy pilot must not require receiving that pilot's physical ship.
    for(auto& pilot:g.state.networkPilots)if(pilot.id>=0 && pilot.id<shipIdentityCapacity && !present.test(pilot.id)) {
        present.set(pilot.id);peers.push_back({pilot.id,pilot.team,pilot.wing,-1,c_ptPlayer,pilot.name,false});
    }
    return peers;
}
std::vector<int> Game::chatSubjects(const ChatAudience& audience)const {
    const auto& g=*impl_;std::vector<int> subjects;if(!g.onTeam() || audience.none)return subjects;
    auto present=[&](int id){return id>=0 && id<=SHRT_MAX && (g.mission->GetShip(ShipID(id)) || std::any_of(g.state.networkPilots.begin(),g.state.networkPilots.end(),[&](auto& p){return p.id==id;}));};
    if(audience.channel==ChatChannel::Target){if(present(audience.recipient))subjects.push_back(audience.recipient);return subjects;}
    if(audience.channel==ChatChannel::Group){for(int id:audience.group)if(present(id) && std::find(subjects.begin(),subjects.end(),id)==subjects.end()) {
        if(id==g.player->GetObjectID())subjects.insert(subjects.begin(),id);else subjects.push_back(id);
    }return subjects;}
    ChatTarget channel;ObjectID recipient;if(!chatRecipient(g.player,audience.channel,audience.recipient,channel,recipient))return subjects;
    auto add=[&](IshipIGC* ship){if(ship && ship->GetMission()) {
        int id=ship->GetObjectID();if(std::find(subjects.begin(),subjects.end(),id)!=subjects.end())return;
        if(ship==g.player)subjects.insert(subjects.begin(),id);else subjects.push_back(id);
    }};
    if(channel==CHAT_LEADERS || channel==CHAT_ADMIN)return subjects;
    auto* source=g.player->GetSourceShip();
    for(auto* l=g.mission->GetShips()->first();l;l=l->next()) {
        auto* ship=l->data();bool own=ship->GetSide()==g.player->GetSide(),include=false;
        switch(channel) {
        case CHAT_EVERYONE:include=true;break;
        case CHAT_TEAM:case CHAT_ALLIES:include=own;break; // Original subjects use our side even for allies.
        case CHAT_WING:include=own && ship->GetWingID()==recipient;break;
        case CHAT_FRIENDLY_SECTOR:{auto* cluster=ship->GetSourceShip()->GetCluster();include=own && cluster && cluster->GetObjectID()==recipient;break;}
        case CHAT_SHIP:include=ship->GetSourceShip()==source;break;
        default:break;
        }
        if(include)add(ship);
    }
    return subjects;
}
std::string Game::chatAudienceName(const ChatAudience& audience)const {
    if(audience.none)return "None";if(audience.channel==ChatChannel::Group)return "Group";
    if(audience.channel==ChatChannel::Target)for(auto& p:impl_->state.networkPilots)if(p.id==audience.recipient)return p.name;
    ChatTarget channel;ObjectID id;const auto& g=*impl_;
    if(!chatRecipient(g.player,audience.channel,audience.recipient,channel,id))return "<unknown>";
    if(channel==CHAT_EVERYONE)return "All";if(channel==CHAT_ALLIES)return "Allies";
    if(channel==CHAT_LEADERS)return "Leaders";if(channel==CHAT_ADMIN)return "Admin";
    return g.chatRecipientName(channel,id);
}
std::vector<OrderTarget> Game::chatTargets(const ChatAudience& audience,int command)const {
    const auto& g=*impl_;std::vector<OrderTarget> targets;if(!g.onTeam() || command<0 || command>=c_cidMax)return targets;
    auto subjects=chatSubjects(audience);
    auto legal=[&](ImodelIGC* target){for(int id:subjects)if(auto* ship=g.mission->GetShip(ShipID(id)))
        if(ship->LegalCommand(CommandID(command),target))return true;return false;};
    auto add=[&](ImodelIGC* target){if((target->GetSide()==g.player->GetSide() || target->SeenBySide(g.player->GetSide())) && legal(target)) {
        auto* cluster=target->GetCluster();
        auto* history=static_cast<Impl::Thing*>(target->GetThingSite())->stationObservation(g.player->GetSide());
        targets.push_back({target->GetObjectID(),target->GetObjectType(),cluster?cluster->GetObjectID():-1,history?history->name:GetModelName(target),cluster?cluster->GetName():""});
    }};
    for(auto* l=g.mission->GetShips()->first();l;l=l->next())add(l->data());
    for(auto* l=g.mission->GetStations()->first();l;l=l->next())add(l->data());
    if(g.viewCluster)for(auto* l=g.viewCluster->GetPickableModels()->first();l;l=l->next())
        if(l->data()->GetObjectType()!=OT_ship && l->data()->GetObjectType()!=OT_station)add(l->data());
    // ConsoleData tries "me" after objects and sectors last. Sector completion
    // represents a provisional cluster buoy without retaining an IGC pointer.
    if(legal(g.player))targets.push_back({g.player->GetObjectID(),OT_ship,g.state.sectorId,"me",g.state.sector});
    for(auto* l=g.mission->GetClusters()->first();l;l=l->next())if(g.knownSector(l->data()))
        targets.push_back({l->data()->GetObjectID(),-2,l->data()->GetObjectID(),l->data()->GetName(),l->data()->GetName()});
    return targets;
}
std::array<bool,9> Game::chatVerbs(const ChatAudience& audience)const {
    std::array<bool,9> allowed{};auto subjects=chatSubjects(audience);const auto& g=*impl_;
    for(int id:subjects)if(auto* ship=g.mission->GetShip(ShipID(id)))for(int i=0;i<9;++i)
        allowed[i]=allowed[i] || ship->LegalCommand(CommandID(i+1));
    return allowed;
}
bool Game::sendChatOrder(ChatChannel channel,int recipient,int command,int type,int id) {
    auto& g=*impl_;if(command<0 || command>=c_cidMax || type<0 || type>OT_buoy || id<0 || id>SHRT_MAX || g.gameplayClosed())return false;
    ChatTarget route;ObjectID to;if(!chatRecipient(g.player,channel,recipient,route,to))return false;
    if(g.gameRole==GameRole::Replica) {
        if(!g.actionSink)return false;Net::Action action{};action.op=Net::Operation::ChatOrder;
        action.args={int(channel),recipient,type,id};action.count=command+1;return g.actionSink(action);
    }
    Allegiance::SimulationRandomScope random(g.randomState);TRef<ImodelIGC> target=g.mission->GetModel(ObjectType(type),ObjectID(id));
    if(!target || !g.seen(target))return false;
    if(route==CHAT_INDIVIDUAL || route==CHAT_GROUP || route==CHAT_GROUP_NOECHO) {
        auto* ship=g.mission->GetShip(ShipID(to));
        if(!ship || !Allegiance::ChatOrderAuthorized(route,g.player->GetSide(),ship->GetSide()))return false;
        if(ship->GetPilotType()<c_ptPlayer)return order(to,type,id,command);
        // Original ConsoleData accepts a command if any selected subject can
        // execute it. A recipient's LegalCommand gates its actual queue.
    }
    g.SendChat(g.player,route,to,NA,nullptr,CommandID(command),target->GetObjectType(),target->GetObjectID(),target);
    g.updateEconomy();g.refresh();return true;
}
bool Game::sendChatWaypoint(ChatChannel channel,int recipient,int command,int sector,glm::vec3 position,bool cluster) {
    auto& g=*impl_;if(command<0 || command>=c_cidMax || sector<0 || sector>SHRT_MAX || g.gameplayClosed() ||
        !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) || glm::length(position)>20000)return false;
    auto* destination=g.mission->GetCluster(SectorID(sector));ChatTarget route;ObjectID to;
    if(!g.knownSector(destination) || !chatRecipient(g.player,channel,recipient,route,to))return false;
    if(g.gameRole==GameRole::Replica)return g.submit(cluster?Net::Operation::ChatCluster:Net::Operation::ChatWaypoint,{int(channel),recipient,command,sector},position);
    Allegiance::SimulationRandomScope random(g.randomState);
    if(cluster && (route==CHAT_INDIVIDUAL || route==CHAT_GROUP || route==CHAT_GROUP_NOECHO)) {
        auto* ship=g.mission->GetShip(ShipID(to));
        if(ship && ship->GetSide()==g.player->GetSide() && ship->GetPilotType()==c_ptBuilder && ship->GetOrdersABM()!=c_aabmBuildable) {
            TRef<ImodelIGC> rock=FindTarget(ship,c_ttNeutral|c_ttAsteroid|c_ttLeastTargeted,nullptr,destination,&Vector::GetZero(),nullptr,ship->GetOrdersABM());
            if(rock)return sendChatOrder(channel,recipient,c_cidBuild,rock->GetObjectType(),rock->GetObjectID());
            return false;
        }
    }
    DataBuoyIGC data{};data.position=cluster?Vector::GetZero():vec(position);data.clusterID=sector;
    data.buoyID=g.mission->GenerateNewBuoyID();data.type=cluster?c_buoyCluster:c_buoyWaypoint;data.visible=true;
    auto* buoy=static_cast<IbuoyIGC*>(g.mission->CreateObject(g.mission->GetLastUpdate(),OT_buoy,&data,sizeof(data)));if(!buoy)return false;
    buoy->AddConsumer();bool ok=sendChatOrder(channel,recipient,cluster?c_cidGoto:command,OT_buoy,buoy->GetObjectID());buoy->ReleaseConsumer();buoy->Release();return ok;
}

// ConsoleData::ResolvePick/Render, using cached subject IDs rather than walking
// the fleet or constructing ChatTarget reports on every pointer sample.
std::string_view Game::commandCursor(int type,int id,std::span<const int> subjects,bool composing,int command,int individual,int button)const {
    const auto& g=*impl_;if(!g.onTeam() || type<0 || type>OT_buoy || id<0 || id>SHRT_MAX)return {};
    auto* model=g.mission->GetModel(ObjectType(type),ObjectID(id));
    if(!model || (model->GetAttributes()&c_mtNotPickable) || !model->GetVisibleF() || !model->GetCluster() || model->GetCluster()!=g.viewCluster)return {};
    auto* side=model->GetSide();
    bool friendly=side && IsideIGC::AlliedSides(side,g.player->GetSide());
    if(button!=1 && type==OT_ship && side==g.player->GetSide())return "select";
    if(button==0)return {};
    int cid=composing?command:c_cidDefault;if(cid<0 || cid>=c_cidMax)return {};
    bool legal=false;for(int subject:subjects)if(subject>=0 && subject<=SHRT_MAX)if(auto* ship=g.mission->GetShip(ShipID(subject)))
        if(ship->LegalCommand(CommandID(cid),model)){legal=true;break;}
    if(!legal)return {};
    if(type==OT_ship && friendly)return "select";
    if(!composing && individual>=0 && individual<=SHRT_MAX)if(auto* ship=g.mission->GetShip(ShipID(individual)))cid=ship->GetDefaultOrder(model);
    return cid>=c_cidDefault && cid<c_cidMax?c_cdAllCommands[cid].szVerb:"default";
}
