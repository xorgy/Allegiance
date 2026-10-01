// Impl members: authoritative round scoring, using the original IGC score
// object. Records freeze once; replicas receive the public immutable arena.
struct ScoredPilot {
    PlayerScoreObject score{true};std::string name;int side{-1},civilization{-1};bool online{true};
};
std::unordered_map<ShipID,ScoredPilot> scoredPilots;
std::array<ShipID,c_cSidesMax> scoreLeaders=[] {std::array<ShipID,c_cSidesMax> a;a.fill(NA);return a;}();
std::shared_ptr<const Net::Frame> resultReport;
bool scoresFrozen{};
uint64_t endgameRevision{};
PlayerScoreObject* scoring(IshipIGC* ship){
    if(gameRole==GameRole::Replica || !ship || ship->GetPilotType()<c_ptPlayer)return nullptr;
    auto i=scoredPilots.find(ship->GetObjectID());return i==scoredPilots.end()?nullptr:&i->second.score;
}
void connectScore(IshipIGC* ship){
    if(gameRole==GameRole::Replica || training || options.trainingMission || !ship || ship->GetPilotType()<c_ptPlayer)return;
    auto& p=scoredPilots[ship->GetObjectID()];p.name=ship->GetName();p.side=ship->GetSide()->GetObjectID();
    p.civilization=p.side>=0?ship->GetSide()->GetCivilization()->GetObjectID():NA;
    if(p.side>=0 && p.online && !scoresFrozen && !p.score.Connected())p.score.Connect(mission->GetLastUpdate());
}
void disconnectScore(IshipIGC* ship){if(auto* p=scoring(ship)){auto& saved=scoredPilots.at(ship->GetObjectID());saved.name=ship->GetName();p->SetRepair(ship->GetRepair());p->SetDeaths(ship->GetDeaths());p->SetEjections(ship->GetEjections());p->Disconnect(mission->GetLastUpdate());}}
void resetSideScore(IshipIGC* ship){
    // CFSMission::RemovePlayerFromSide calls CFSPlayer::Reset(false): earned
    // score and play/command time reset, while deaths/ejections survive.
    if(auto* p=scoring(ship)){disconnectScore(ship);p->StopCommand(mission->GetLastUpdate());p->Reset(false);}
}
void scoreDiscovery(ImodelIGC* model,IscannerIGC* scanner){
    if(gameRole==GameRole::Replica || scoresFrozen || !scanner)return;
    IshipIGC* ship=scanner->GetObjectType()==OT_ship?static_cast<IshipIGC*>(scanner):nullptr;
    if(model->GetObjectType()==OT_ship){
        auto* target=static_cast<IshipIGC*>(model);if(!target->GetHullType())return;
        constexpr AssetMask highValue=c_amEnemyMiner|c_amEnemyBuilder|c_amEnemyCapital|c_amEnemyCarrier|c_amEnemyBomber|c_amEnemyAPC|c_amEnemyTeleportShip;
        if(!(GetAssetMask(target,target->GetHullType(),false)&highValue) || target->RecentlySpotted())return;
        if(scanner->GetObjectType()==OT_probe)ship=static_cast<IprobeIGC*>(scanner)->GetProbeLauncherShip();
        if(auto* score=scoring(ship))score->AddTargetSpot();target->MarkPreviouslySpotted();
    }else if(auto* score=scoring(ship)){
        if(model->GetObjectType()==OT_warp)score->SpotWarp();
        else if(model->GetObjectType()==OT_asteroid)score->SpotSpecialAsteroid();
    }
}
void scoreLeadership(){
    if(gameRole==GameRole::Replica || training || scoresFrozen)return;
    // Retire every former role before starting new roles. One pilot can move
    // from a higher numbered side to a lower one in the same transaction.
    for(int side=0;side<c_cSidesMax;++side)if(scoreLeaders[side]!=commanders[side]){
        if(auto i=scoredPilots.find(scoreLeaders[side]);i!=scoredPilots.end())i->second.score.StopCommand(mission->GetLastUpdate());
    }
    for(int side=0;side<c_cSidesMax;++side)if(scoreLeaders[side]!=commanders[side]){
        scoreLeaders[side]=commanders[side];
        if(auto i=scoredPilots.find(scoreLeaders[side]);i!=scoredPilots.end())i->second.score.StartCommand(mission->GetLastUpdate());
    }
}
void scoreShipDamage(IshipIGC* victim,ImodelIGC* killer,bool pod,float amount){
    if(gameRole==GameRole::Replica || victim->GetParentShip())return;
    auto* track=victim->GetDamageTrack();auto* first=track?track->GetDamageBuckets()->first():nullptr;
    if(first)for(auto* l=first->next();l;l=l->next()){
        auto* m=l->data()->model();if(m->GetObjectType()==OT_ship && m->GetMission()==mission && !IsideIGC::AlliedSides(m->GetSide(),victim->GetSide()))
            if(auto* score=scoring(static_cast<IshipIGC*>(m))){score->AddAssist();break;}
    }
    if(pod || amount==0 || !killer || !killer->GetSide() || killer->GetSide()==victim->GetSide() || killer->GetObjectType()!=OT_ship)return;
    if(auto* s=scoring(static_cast<IshipIGC*>(killer)))s->AddKill();
    float total=0;for(auto* l=first;l;l=l->next())if(l->data()->model()->GetSide()!=victim->GetSide())total+=l->data()->totalDamage();
    if(total<=0)return;
    for(auto* l=first;l;l=l->next()){
        auto* m=l->data()->model();if(m->GetObjectType()!=OT_ship || m->GetMission()!=mission || IsideIGC::AlliedSides(m->GetSide(),victim->GetSide()))continue;
        auto* ship=static_cast<IshipIGC*>(m);float fraction=l->data()->totalDamage()/total;
        if(auto* s=scoring(ship))s->KillShip(victim,fraction);
        if(auto* parent=ship->GetParentShip())if(auto* s=scoring(parent))s->KillShip(victim,fraction/(2.f+parent->GetHullType()->GetMaxWeapons()-parent->GetHullType()->GetMaxFixedWeapons()));
    }
}
void scoreBase(IshipIGC* ship,bool capture){
    if(auto* s=scoring(ship)){if(capture)s->CaptureBase(true);else s->KillBase(true);}
    if(ship)for(auto* l=ship->GetChildShips()->first();l;l=l->next())if(auto* s=scoring(l->data())){if(capture)s->CaptureBase(false);else s->KillBase(false);}
}
void freezeScores(){
    if(!state.complete || scoresFrozen || training || gameRole==GameRole::Replica)return;
    scoreLeadership();auto now=mission->GetLastUpdate();auto* winner=state.endgame.winner>=0?mission->GetSide(SideID(state.endgame.winner)):nullptr;
    std::array<float,c_cSidesMax> played{},earned{};std::array<ScoredPilot*,c_cSidesMax> commanders{};
    for(auto& [id,p]:scoredPilots){
        if(auto* ship=mission->GetShip(id)){p.name=ship->GetName();if(ship->GetSide()->GetObjectID()>=0){p.side=ship->GetSide()->GetObjectID();p.civilization=ship->GetSide()->GetCivilization()->GetObjectID();}
            p.score.SetRepair(ship->GetRepair());p.score.SetDeaths(ship->GetDeaths());p.score.SetEjections(ship->GetEjections());}
        p.score.Disconnect(now);if(p.side<0 || p.score.GetTimePlayed()<=0)continue;
        bool count=p.score.GetTimePlayed()>180.f && winner;auto* side=mission->GetSide(SideID(p.side));bool won=winner && IsideIGC::AlliedSides(winner,side);
        p.score.EndGame(mission,won && count && p.score.GetTimePlayed()>elapsed*.5f,!won && count);
        played[p.side]+=p.score.GetTimePlayed();earned[p.side]+=p.score.GetScore();
        if(!commanders[p.side] || commanders[p.side]->score.GetTimeCommanded()<p.score.GetTimeCommanded())commanders[p.side]=&p;
    }
    for(int side=0;side<c_cSidesMax;++side)if(auto* p=commanders[side];p && p->score.GetTimeCommanded()>0 && played[side]>0)p->score.SetCommanderScore(earned[side]/played[side]*p->score.GetTimePlayed());
    state.endgame.players.clear();state.endgame.teams.clear();resultReport.reset();
    for(const auto& [id,p]:scoredPilots)if(p.side>=0 && p.score.GetTimePlayed()>0){
        Net::ResultPlayer row;Net::text(row.name,p.name);row.id=id;row.side=p.side;row.civilization=p.civilization;
        row.kills=p.score.GetKills();row.deaths=p.score.GetDeaths();row.ejections=p.score.GetEjections();row.assists=p.score.GetAssists();row.baseKills=p.score.GetPilotBaseKills();row.baseCaptures=p.score.GetPilotBaseCaptures();
        row.flags=(p.score.GetCommandCredit()?1:0)|((p.score.GetWinner() || p.score.GetLoser())?2:0);row.score=p.score.GetScore();row.played=p.score.GetTimePlayed();state.endgame.players.push_back(row);
    }
    std::sort(state.endgame.players.begin(),state.endgame.players.end(),[](auto& a,auto& b){return a.id<b.id;});
    for(auto* l=mission->GetSides()->first();l;l=l->next()){
        auto* side=l->data();Net::ResultTeam row;Net::text(row.name,side->GetName());row.id=side->GetObjectID();row.civilization=side->GetCivilization()->GetObjectID();auto c=side->GetColor();
        row.color[0]=c.R();row.color[1]=c.G();row.color[2]=c.B();row.kills=side->GetKills();row.deaths=side->GetDeaths();row.ejections=side->GetEjections();row.baseKills=side->GetBaseKills();row.baseCaptures=side->GetBaseCaptures();row.flags=side->GetFlags();row.artifacts=side->GetArtifacts();state.endgame.teams.push_back(row);
    }
    state.endgame.pods=mission->GetMissionParams()->bEjectPods;state.endgame.frozen=scoresFrozen=true;++endgameRevision;
}
