// Read-only projection of WinTrek::InitializeGameStateContainer/FillWinCondition.
GameStatePanelData Game::gameStateData()const {
    auto& g=*impl_;GameStatePanelData result;if(!g.player || !g.onTeam())return result;
    auto* params=g.mission->GetMissionParams();auto* own=g.player->GetSide();
    struct Standing {std::string name;glm::vec4 color;int kills{},deaths{},ejections{},value{};};
    std::vector<Standing> teams,pilots;int selfTeam=-1,selfPilot=-1;
    std::vector<IsideIGC*> sides;
    for(auto* l=g.mission->GetSides()->first();l;l=l->next()){
        auto* side=l->data();if(side==own)selfTeam=int(teams.size());sides.push_back(side);
        const auto& c=side->GetColor();teams.push_back({censorChat(side->GetName(),g.chatPrefs.censor),{c.R(),c.G(),c.B(),1},side->GetKills(),side->GetDeaths(),side->GetEjections()});
    }
    auto pilot=[&](int id,int side,std::string name,int kills,int deaths,int ejections){
        if(id==g.player->GetObjectID())selfPilot=int(pilots.size());
        glm::vec4 color(1);if(auto* team=g.mission->GetSide(SideID(side))){auto& c=team->GetColor();color={c.R(),c.G(),c.B(),1};}
        pilots.push_back({std::move(name),color,kills,deaths,ejections});
    };
    if(g.gameRole==GameRole::Replica){
        for(const auto& p:g.state.networkPilots)pilot(p.id,p.team,p.name,p.kills,p.deaths,p.ejections);
    }else for(auto* l=g.mission->GetShips()->first();l;l=l->next()){
        auto* ship=l->data();if(ship->GetPilotType()>=c_ptPlayer)pilot(ship->GetObjectID(),ship->GetSide()->GetObjectID(),ship->GetName(),ship->GetKills(),ship->GetDeaths(),ship->GetEjections());
    }
    auto killsGreater=[](const Standing& a,const Standing& b){return Allegiance::GameStateKillsGreater(a.kills,a.deaths,b.kills,b.deaths);};
    auto fill=[](GameStatePanelLine& line,const Standing& s,int rank,bool kills,std::string value){
        line.color=s.color;line.columns[0]="#"+std::to_string(rank);line.columns[1]=s.name;
        line.columns[2]=kills?std::to_string(s.kills)+" kills":std::move(value);
        if(kills){line.columns[3]=std::to_string(s.ejections)+" ejects";line.columns[4]=std::to_string(s.deaths)+" deaths";}
    };
    auto add=[&](std::string title,std::string description,auto value,auto label,bool compare=true){
        GameStatePanelTile tile;tile.title=std::move(title);tile.description=std::move(description);
        for(size_t i=0;i<teams.size();++i)teams[i].value=value(sides[i]);
        auto rank=Allegiance::GameStateRanks<Standing>(teams,selfTeam,[&](const Standing& a,const Standing& b){return compare && a.value>b.value;});
        for(int r=0;r<2;++r)if(rank[r].index>=0){const auto& s=teams[rank[r].index];fill(tile.lines[r],s,rank[r].rank,false,label(s.value));}
        result.tiles.push_back(std::move(tile));
    };
    if(params->IsConquestGame())add("wctitleconquestbmp",params->GetConquestPercentage()==100?"1st team to own all of the starbases wins":"1st team to own "+std::to_string(params->GetConquestPercentage())+"% of the starbases wins",
        [](auto* s){return s->GetConquestPercent();},[](int v){return std::to_string(v)+"% owned";});
    if(params->IsTerritoryGame())add("wctitleterritorybmp","1st team to own "+std::to_string((50+g.mission->GetClusters()->n()*params->GetTerritoryPercentage())/100)+" sectors wins",
        [](auto* s){return s->GetTerritoryCount();},[](int v){return std::to_string(v)+(v==1?" sector owned":" sectors owned");});
    if(params->IsProsperityGame())add("wctitleprosperitybmp","First team to purchase reinforcements wins",[](auto* s){
        auto* last=s->GetBuckets()->last();if(last){auto* buyable=last->data()->GetBuyable();if(buyable->GetObjectType()==OT_development && buyable->GetObjectID()==c_didTeamMoney)return int(last->data()->GetPercentComplete());}return 0;
    },[](int v){return std::to_string(v)+"% invested";},false);
    if(params->IsArtifactsGame())add("wctitleartifactsbmp","First team to own "+std::to_string(params->nGoalArtifactsCount)+" artifacts wins",
        [](auto* s){return s->GetArtifacts();},[](int v){return std::to_string(v)+" owned";});
    if(params->IsFlagsGame())add("wctitlecapturebmp","First team to capture "+std::to_string(params->nGoalFlagsCount)+" flags wins",
        [](auto* s){return s->GetFlags();},[](int v){return std::to_string(v)+" captured";});
    GameStatePanelTile kills;kills.columns=5;kills.rows=4;kills.title=params->IsDeathMatchGame()?"wctitledeathmatchbmp":"wctitledeathsbmp";
    if(params->IsDeathMatchGame())kills.description="1st team to kill "+std::to_string(params->nGoalTeamKills)+" ships wins";
    auto teamRanks=Allegiance::GameStateRanks<Standing>(teams,selfTeam,killsGreater),pilotRanks=Allegiance::GameStateRanks<Standing>(pilots,selfPilot,killsGreater);
    for(int r=0;r<2;++r){if(teamRanks[r].index>=0)fill(kills.lines[r],teams[teamRanks[r].index],teamRanks[r].rank,true,{});
        if(pilotRanks[r].index>=0)fill(kills.lines[r+2],pilots[pilotRanks[r].index],pilotRanks[r].rank,true,{});}
    result.tiles.push_back(std::move(kills));
    result.clock=Allegiance::GameStateClock(params->IsCountdownGame(),int(params->IsCountdownGame()?params->GetCountDownTime()-float(g.elapsed):float(g.elapsed)));
    return result;
}
