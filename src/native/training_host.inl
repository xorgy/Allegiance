// Included after Game::Impl: direct native adapters keep authored training out
// of the Vulkan/UI implementation and avoid a second gameplay model.
namespace Native {
TrainingHost* trainingHost=nullptr;
TrainingHost::TrainingHost(Game& g,Audio& a,int count):game(g),audio(a),fGroupFire(g.impl_->groupFire),joystickCount(count) {
    if(trainingHost)throw std::runtime_error("An authored training mission is already active");
    trainingHost=this;
}
TrainingHost::~TrainingHost() {
    if(trainingHost==this) {
        Training::SetSkipPostSlideshow();Training::EndMission();Training::HandleMission();
        viewCluster=nullptr;trainingHost=nullptr;
    }
}
ImissionIGC* TrainingHost::GetCore()const{return game.impl_->mission;}
IshipIGC* TrainingHost::GetShip()const {
    auto& g=*game.impl_;
    if(!g.player) {
        DataShipIGC data{};data.shipID=data.hullID=data.sideID=data.baseObjectID=NA;
        data.pilotType=c_ptPlayer;data.partsOffset=sizeof(data);strcpy(data.name,"Cadet");
        auto* ship=static_cast<IshipIGC*>(g.mission->CreateObject(g.mission->GetLastUpdate(),OT_ship,&data,sizeof(data)));
        if(!ship)throw std::runtime_error("Cannot create the training pilot");
        g.player=ship;ship->Release();ship->CreateDamageTrack();
    }
    return g.player;
}
ShipID TrainingHost::GetShipID()const{return GetShip()->GetObjectID();}
IsideIGC* TrainingHost::GetSide()const{return GetShip()->GetSide();}
ImissionIGC* TrainingHost::ResetStaticData() {
    if(GetCore()->GetClusters()->n())throw std::runtime_error("Training requires a fresh mission");
    return GetCore();
}
void TrainingHost::SetMissionParameters(const char* core,int version) {
    MissionParams p=*GetCore()->GetMissionParams();strcpy(p.szIGCStaticFile,core);p.verIGCcore=version;
    p.nTeams=6;p.bAllowEmptyTeams=true;p.bInvulnerableStations=false;p.bEjectPods=false;
    GetCore()->SetMissionParams(&p);
}
void TrainingHost::RegisterPilot(IshipIGC* ship,SideID side,ShipID id) {
    TRef<IshipIGC> keep=ship;ship->SetSide(nullptr);
    if(ship->GetObjectID()!=id){GetCore()->DeleteShip(ship);ship->SetObjectID(id);GetCore()->AddShip(ship);}
    ship->SetSide(GetCore()->GetSide(side));
    if(!ship->GetDamageTrack())ship->CreateDamageTrack();
}
void TrainingHost::ClearLoadout(){game.impl_->loadouts.clear();}
void TrainingHost::SaveLoadout(IshipIGC* ship){game.impl_->rememberLoadout(ship);}
void TrainingHost::ClearMessages(){game.clearMessages();}
void TrainingHost::OnEnterGame(){GetCore()->EnterGame();GetCore()->SetMissionStage(STAGE_STARTED);Training::SetupShipAndCamera();}
void TrainingHost::BuyDefaultLoadout(IshipIGC* ship,IstationIGC* station,IhullTypeIGC* hull,Money* budget) {
    if(hull){while(auto* p=ship->GetParts()->first())p->data()->Terminate();ship->SetBaseHullType(hull);}
    else for(Mount m=-c_maxCargo;m<0;++m)if(auto* p=ship->GetMountedPart(NA,m))p->Terminate();
    Allegiance::BuyPreferredParts(ship,station,budget);Allegiance::BuyDefaultCargo(ship,station,budget);
}
void TrainingHost::OverrideThrottle(float value){auto& g=*game.impl_;g.throttleReset=(value+1)*.5f;g.throttleResetPending=true;g.forcedThrottleReset=true;}
void TrainingHost::SetSelectedWeapon(Mount mount){if(Allegiance::CanSelectWeapon(GetShip(),mount))game.impl_->selectedWeapon=mount;}
void TrainingHost::ToggleWeapon(Mount mount){game.selectWeapon(mount);}
void TrainingHost::SwapPart(EquipmentType type,Mount mount){game.swapCargo(PartKind(type),mount);}
bool TrainingHost::flyingF()const{return GetShip()->GetCluster()!=nullptr;}
void TrainingHost::SetTarget(ImodelIGC* target,CommandID command){game.impl_->setTarget(target,true,command);}
void TrainingHost::SetQueuedCommand(IshipIGC* sender,CommandID command,ImodelIGC* target){game.impl_->queueOrder(GetShip(),sender,target,command);}
void TrainingHost::PostText(bool critical,const char* format,...) {
    char text[1024];va_list args;va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);game.impl_->message(promptFormatter?promptFormatter(text):text,critical);
}
void TrainingHost::ReceiveChat(IshipIGC* sender,ChatTarget recipient,ObjectID id,SoundID sound,const char* text,
    CommandID command,ObjectType type,ObjectID target,ImodelIGC* model,bool) {
    game.impl_->ReceiveChat(sender,recipient,id,sound,text,command,type,target,model);
}
TRef<ISoundInstance> TrainingHost::StartSound(SoundID sound){return audio.startCue(sound);}
void TrainingHost::EjectPlayer(ImodelIGC* launcher) {
    auto& g=*game.impl_;g.beginCinematic(launcher,true);g.trainingRestoreAt=g.elapsed+Allegiance::OverrideCameraDuration(true);view=vmOverride;camera=cmExternalOverride;
    if(launcher && launcher->GetCluster())viewCluster=launcher->GetCluster();else viewCluster=GetShip()->GetCluster();
}
void TrainingHost::FinishMission(bool skip){finished=true;skipSlideshow=skip;paused=false;overlay=NoTrainingOverlay;}
void TrainingHost::PositionCommandView(const Vector* position,float dt) {
    auto& g=*game.impl_;
    g.commandCenter=position?*position:GetShip()->GetStation()?GetShip()->GetStation()->GetPosition():GetShip()->GetPosition();
    g.commandDisplacement=Vector::GetZero();g.commandTracking=false;
    if(position)g.commandDistance=10000.f;
    else g.commandDistance=std::max(g.commandDistance,GetShip()->GetStation()?GetShip()->GetStation()->GetRadius()*2:5+GetShip()->GetRadius()*2);
    g.cameraMotion.SetAnimation(dt);g.commandCameraDirty=true;camera=cmExternalCommandView34;
}
IbaseIGC* TrainingHost::GetPickedObject()const{return GetCore()->GetShip(ShipID(game.impl_->commandUnit));}
void Game::startTraining(Audio& audio,int joystickCount,std::function<std::string(std::string_view)> promptFormatter) {Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;if(!g.options.trainingMission || g.training)throw std::runtime_error("No fresh authored training session");
    g.training=std::make_unique<TrainingHost>(*this,audio,joystickCount);
    g.training->promptFormatter=std::move(promptFormatter);
    try {
        audio.preload(trainingSoundIds);
        if(!Training::StartMission(g.options.trainingMission))throw std::runtime_error("Unknown authored training mission");
        g.state.sessionName="Original training "+std::to_string(g.options.trainingMission);
        g.selectedHull=g.player->GetBaseHullType()->GetObjectID();g.refresh();
    } catch(...) {g.training.reset();throw;}
}
bool Game::recordKey(short key) {Allegiance::SimulationRandomScope randomScope(impl_->randomState);
    auto& g=*impl_;if(!g.training)return true;
    bool allowed=Training::RecordKeyPress(key);
    if(allowed)switch(key) {
    case TK_ViewExternal:g.training->view=TrainingHost::vmCombat;g.training->camera=TrainingHost::cmExternal;break;
    case TK_ViewTarget:g.training->view=TrainingHost::vmCombat;g.training->camera=TrainingHost::cmExternalTarget;break;
    case TK_ViewFlyby:g.training->view=TrainingHost::vmCombat;g.training->camera=TrainingHost::cmExternalFlyBy;break;
    case TK_ViewExternalOrbit:g.training->view=TrainingHost::vmCombat;g.training->camera=TrainingHost::cmExternalOrbit;break;
    case TK_ViewExternalStation:g.training->view=TrainingHost::vmCombat;g.training->camera=TrainingHost::cmExternalStation;break;
    case TK_ViewMissile:g.training->view=TrainingHost::vmCombat;g.training->camera=TrainingHost::cmExternalMissile;break;
    case TK_ViewChase:g.training->view=TrainingHost::vmCombat;g.training->camera=g.training->camera==TrainingHost::cmCockpit?TrainingHost::cmExternalChase:TrainingHost::cmCockpit;break;
    case TK_ConModeCombat:g.training->view=TrainingHost::vmCombat;g.training->camera=TrainingHost::cmCockpit;break;
    case TK_ViewCommand:g.training->view=TrainingHost::vmCommand;g.training->PositionCommandView(nullptr,0);break;
    case TK_ToggleCommand:if(g.training->view==TrainingHost::vmCommand)g.training->camera=g.training->camera==TrainingHost::cmExternalCommandView34?TrainingHost::cmExternalCommandViewTD:TrainingHost::cmExternalCommandView34;break;
    default:break;
    }
    g.refresh();return allowed;
}
void Game::setTrainingPane(unsigned mask,bool visible) {
    auto& g=*impl_;if(!g.training)return;
    if(visible)g.training->TurnOnOverlayFlags(mask);else g.training->TurnOffOverlayFlags(mask);g.refresh();
}
void Game::setTrainingRadar(int mode){auto& g=*impl_;if(g.training){g.training->SetRadarMode(mode);g.refresh();}}
}
namespace Training {
Time Clock::Now(){return Native::trainingHost->GetCore()->GetLastUpdate();}
bool Clock::IsPaused(){return Native::trainingHost->paused;}
void Clock::Pause(){Native::trainingHost->paused=true;}
void Clock::Continue(){Native::trainingHost->paused=false;}
}
