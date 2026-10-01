#include "../ClientCore/ShipLifecycle.h"
#include "../ClientCore/FlagLifecycle.h"
#include "../ClientCore/ShipCrew.h"
#include "../ClientCore/StationEvacuation.h"
#include "test_fixture.hpp"
#include "test_reference/lifecycle.hpp"
#include "test_reference/station_evacuation.hpp"
#include "game.hpp"
#include "../ClientCore/MissionRules.h"
#include <tuple>

namespace {
using NativeTest::require;
void near(Vector a,Vector b,const char* what){require((a-b).LengthSquared()<1e-8f,what);}
void same(IshipIGC* a,IshipIGC* b) {
    near(a->GetPosition(),b->GetPosition(),"reference position");near(a->GetVelocity(),b->GetVelocity(),"reference velocity");
    near(a->GetOrientation().GetForward(),b->GetOrientation().GetForward(),"reference forward");
    near(a->GetOrientation().GetUp(),b->GetOrientation().GetUp(),"reference up");
}
void referenceChecks(NativeTest::Fixture& f) {
    auto* a=f.ship();auto* b=f.ship();auto* credit=f.ship(1);
    for(int mode=0;mode<5;++mode) {
        a->SetBaseHullType(f.mission->GetHullType(414));b->SetBaseHullType(f.mission->GetHullType(414));
        a->SetVelocity(Vector(8,9,10));b->SetVelocity(a->GetVelocity());
        Vector hit(100,200,300),impact(103,197,301);
        ImodelIGC* killer=mode<3?credit:nullptr;
        credit->SetPosition(mode==0?hit:impact);
        credit->SetCluster(mode==2?f.mission->GetClusters()->last()->data():a->GetCluster());
        if(mode==4)impact=hit; // coincident collision must remain finite
        auto rng=[](){return Vector(.6f,0,.8f);};
        Allegiance::EjectIntoPod(f.mission->GetLastUpdate(),a,killer,hit,impact,rng);
        OriginalLifecycle::Eject(f.mission->GetLastUpdate(),b,killer,hit,impact,rng);same(a,b);
        require(a->GetBaseHullType()->HasCapability(c_habmLifepod),"pod hull");
        require(a->GetCurrentTurnRate(c_axisRoll)==pi*2,"original ejection spin");
    }
    bool slotted=false,unslotted=false;
    for(auto* l=f.mission->GetHullTypes()->first();l;l=l->next()) {
        auto* hull=l->data();bool slots=hull->GetLaunchSlots()>0;
        if((slots && slotted)||(!slots&&unslotted))continue;
        auto* ca=f.ship(0,hull->GetObjectID());auto* cb=f.ship(0,hull->GetObjectID());
        ca->SetPosition(Vector(128,-90,72));cb->SetPosition(ca->GetPosition());
        ca->SetVelocity(Vector(22,7,-17));cb->SetVelocity(ca->GetVelocity());
        Orientation o(Vector(.5f,.1f,-.8f));o.Roll(.6f);ca->SetOrientation(o);cb->SetOrientation(o);
        for(int slot=0;slot<std::max(1,int(hull->GetLaunchSlots()))+1;++slot) {
            auto rng=[](float low,float high){return low+.3f*(high-low);};
            Allegiance::LaunchFromCarrier(ca,a,130,rng);OriginalLifecycle::Carrier(cb,b,130,rng);same(a,b);
        }
        (slots?slotted:unslotted)=true;
    }
    require(slotted && unslotted,"both authored carrier launch branches covered");
    a->SetBaseHullType(f.mission->GetHullType(414));a->SetAmmo(20);a->SetFuel(12);
    auto* type=f.mission->GetAmmoPack();auto* part=a->CreateAndAddPart(type,-1,10);require(part,"cargo for salvage");
    unsigned drops=0;
    Allegiance::DropShipInventory(a,[](int,int){return 0;},[&](short amount,IpartTypeIGC* p,float life) {
        auto* t=Allegiance::CreatePartTreasure(f.mission->GetLastUpdate(),a,amount,p,Vector(1,2,3),100,life,[](){return Vector(1,0,0);});
        require(t && t->GetAmount()==amount && t->GetTreasureID()==p->GetObjectID(),"salvage preserves part and amount");
        near(t->GetVelocity(),a->GetVelocity()+Vector(100,0,0),"salvage scatter velocity");++drops;
    });
    require(drops==3 && a->GetParts()->n()==0,"parts/ammo/fuel dropped and inventory removed");
    a->SetFlag(1);a->SetPosition(Vector::GetZero());
    require(Allegiance::DropShipFlag(a,f.mission->GetLastUpdate(),a->GetPosition(),[](){return Vector(1,0,0);}),"flag scattered inside universe");
    require(a->GetFlag()==NA,"ejected ship no longer carries flag");
    auto* flag=a->GetCluster()->GetTreasures()->last()->data();
    require(flag->GetTreasureCode()==c_tcFlag && flag->GetTreasureID()==1,"flag identity preserved in treasure");
    a->SetFlag(1);a->SetPosition(Vector(1e7f,0,0));
    require(!Allegiance::DropShipFlag(a,f.mission->GetLastUpdate(),a->GetPosition(),[](){return Vector(1,0,0);}),"out-of-universe flag requests respawn");
    auto params=*f.mission->GetMissionParams();params.nGoalFlagsCount=1;params.nGoalArtifactsCount=1;f.mission->SetMissionParams(&params);
    a->SetFlag(1);require(Allegiance::SecureFlag(a) && a->GetSide()->GetFlags()==1 && a->GetFlag()==NA,"flag delivery scores and reaches goal");
    a->SetFlag(SIDE_TEAMLOBBY);require(Allegiance::SecureFlag(a) && a->GetSide()->GetArtifacts()==1,"artifact delivery scores separately");
    printf("Lifecycle reference: ejection, slotted/unslotted carrier launches and salvage passed.\n");
}
int crewReferenceChecks(NativeTest::Fixture& f) {
    IhullTypeIGC* hull=nullptr;
    for(auto* l=f.mission->GetHullTypes()->first();l;l=l->next()) {
        auto* h=l->data();if(h->GetMaxWeapons()-h->GetMaxFixedWeapons()<2)continue;
        for(auto* p=h->GetPreferredPartTypes()->first();p;p=p->next())
            if(p->data()->GetEquipmentType()==ET_Weapon && h->CanMount(p->data(),h->GetMaxFixedWeapons())){hull=h;break;}
        if(hull)break;
    }
    require(hull,"authored hull with two equipped turret mounts");
    auto* parent=f.ship(0,hull->GetObjectID());auto* child=f.ship();
    auto* station=f.mission->GetSide(0)->GetStations()->first()->data();
    parent->SetCluster(nullptr);parent->SetStation(station);child->SetCluster(nullptr);child->SetStation(station);
    require(Allegiance::CanBoardShip(child,parent),"docked players can board");
    require(Allegiance::NextTurret(child,parent)==NA,"empty mounts cannot be boarded");
    Mount first=hull->GetMaxFixedWeapons(),last=hull->GetMaxWeapons();
    for(Mount m=first;m<last;++m)for(auto* l=hull->GetPreferredPartTypes()->first();l;l=l->next())
        if(l->data()->GetEquipmentType()==ET_Weapon && hull->CanMount(l->data(),m)){parent->CreateAndAddPart(l->data(),m,1);break;}
    require(Allegiance::NextTurret(child,parent)==first,"first free equipped turret");
    child->SetParentShip(parent);child->SetTurretID(first);
    require(Allegiance::NextTurret(child,parent)==first+1,"cycle skips occupied current seat");
    require(child->GetBaseHullType()==nullptr && child->GetParts()->n()==0 && parent->GetGunner(first)==child,"IGC owns passenger and gunner binding");
    std::vector<IshipIGC*> occupants;
    for(Mount m=first+1;m<last;++m){auto* p=f.ship();p->SetCluster(nullptr);p->SetStation(station);p->SetParentShip(parent);p->SetTurretID(m);occupants.push_back(p);}
    require(Allegiance::NextTurret(child,parent)==NA,"all occupied turrets terminate search");
    require(Allegiance::LeaveDockedShip(child) && child->GetBaseHullType()->HasCapability(c_habmLifepod),"disembark restores an independent pod");
    require(parent->GetGunner(first)==nullptr,"disembark clears original weapon gunner");
    for(auto* p:occupants)require(Allegiance::LeaveDockedShip(p),"release crew");
    require(!Allegiance::CanBoardShip(parent,parent),"cannot board self");
    auto* enemy=f.ship(1);enemy->SetCluster(nullptr);enemy->SetStation(station);require(!Allegiance::CanBoardShip(enemy,parent),"cannot board enemy pilot");
    printf("Crew reference hull: %d %s (%d turrets).\n",hull->GetObjectID(),hull->GetName(),last-first);
    return hull->GetObjectID();
}
void crewSessionChecks(const std::filesystem::path& art,int hullId) {
    Native::Assets assets(art);Native::SessionOptions options;options.enemies=0;options.map=3;options.startInHangar=true;
    Native::Game game(art,[&](auto& name,auto&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options);
    int gunner=game.state().shipId,pilot=game.addLocalPilot("Crew pilot",0,hullId);
    require(pilot>=0 && game.boardShip(pilot),"native docked boarding");
    require(game.state().passenger && game.state().parentId==pilot && game.state().turret>=0 && game.state().docked,"passenger state uses parent hull");
    game.launch();require(game.state().docked,"gunner cannot launch the pilot's ship");
    require(game.boardShip(pilot,true) && game.state().passenger && game.state().turret==-1,"practice observer preserves parent without controlling a gun");
    require(game.leaveShip() && !game.state().passenger && !game.state().pod && game.state().docked && game.state().shipId==gunner,"native docked disembark restores the pilot's own fitting");
    require(game.boardShip(pilot) && game.controlLocalPilot(pilot),"reboard and control local pilot");
    game.launch();require(!game.state().docked,"pilot launch");
    Native::FlightInput thrust;thrust.throttle=.25f;
    for(int i=0;i<180;++i)game.step(thrust,1./165);
    auto position=game.state().position;
    require(game.controlLocalPilot(gunner) && !game.state().docked && game.state().passenger,"launch carries gunner into sector");
    require(glm::distance(position,game.state().position)<.001f,"gunner HUD follows parent position");
    require(!game.reload() && !game.dropCargo() && !game.swapCargo(Native::PartKind::Weapon,0),"gunner cannot rearrange pilot inventory");
    require(!game.leaveShip() && !game.returnToBase() && !game.ripcord(),"gunner cannot disembark/steer/ripcord parent in flight");
    int ammo=game.state().ammo;Native::FlightInput fire;fire.fire=true;fire.throttle=1;fire.yaw=.5f;
    auto facing=game.camera({}).forward;
    for(int i=0;i<180;++i){game.step(fire,1./165);game.worldSounds(true);game.clearSounds();}
    require(game.state().ammo<ammo,"gunner input fires original weapon and consumes parent ammo");
    require(glm::distance(facing,game.camera({}).forward)>.2f,"original turret slew changes camera attitude");
    require(std::abs(game.camera({}).fieldOfView-50.f)<.01f,"turret zoom follows original throttle/FOV");
    auto currentMount=game.state().turret;require(game.boardShip(pilot) && game.state().turret!=currentMount,"cycle equipped turrets while flying");
    require(game.controlLocalPilot(pilot) && game.promoteGunner(gunner),"pilot promotes gunner");
    require(game.state().parentId==gunner && game.state().passenger,"old pilot becomes promoted gunner's passenger");
    require(game.controlLocalPilot(gunner) && !game.state().passenger && game.state().ammo<=ammo,"promoted pilot inherits hull/inventory");
    game.step({},1./165);require(game.returnToBase(),"promoted pilot can dock");
    for(int i=0;i<60000 && !game.state().docked;++i)game.step({},1./165);
    require(game.state().docked,"crewed ship docks through actual IGC navigation/collision");
    require(game.controlLocalPilot(pilot) && game.state().docked && game.state().passenger,"docking carries gunner into station");
    require(game.controlLocalPilot(gunner),"return to pilot");game.launch();
    for(int i=0;i<180;++i)game.step(thrust,1./165);
    game.eject();require(game.state().pod && !game.state().passenger,"pilot ejection produces pod");
    require(game.controlLocalPilot(pilot) && game.state().pod && !game.state().passenger,"parent destruction ejects gunner into separate pod");
    game.eject();require(game.state().docked && !game.state().dead,"gunner pod abandonment recovers at base");
    printf("Native crew: boarding, gunfire, cameras, promotion, docking and parent death passed.\n");
}
void sessionChecks(const std::filesystem::path& art) {
    Native::Assets assets(art);Native::SessionOptions options;options.enemies=0;options.map=3;
    Native::Game game(art,[&](auto& name,auto&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options);
    int id=game.state().shipId;game.eject();
    require(game.state().pod && game.state().shipId==id && game.state().equipment.empty(),"native eject preserves identity and removes parts");
    require(game.returnToBase(),"pod return command");
    for(int i=0;i<40000 && !game.state().docked;++i)game.step({},1./165);
    require(game.state().docked && !game.state().pod && !game.state().dead && game.state().shipId==id,"pod docking/rescue and refit");
    game.launch();game.eject();game.eject();
    require(game.state().docked && game.state().deaths==1 && game.state().shipId==id,"abandon pod restarts same player");
    game.launch();
    int home=-1;for(auto& sector:game.state().sectors)if(sector.current)home=sector.id;
    std::string homeName=game.state().sector;
    auto warp=std::find_if(game.contacts().begin(),game.contacts().end(),[](auto& c){return c.type==Native::ContactWarp;});
    require(warp!=game.contacts().end(),"aleph for ripcord integration");game.selectTarget(warp->type,warp->id);game.toggleAutopilot();
    for(int i=0;i<40000 && game.state().sector==homeName;++i)game.step({},1./165);
    require(game.state().sector!=homeName && game.ripcord(home),"remote home base accepts ripcord");
    require(game.state().ripcord && game.state().ripcordLeft>0,"IGC ripcord countdown starts");
    for(int i=0;i<10000 && game.state().ripcord;++i)game.step({},1./165);
    require(!game.state().ripcord && game.state().sector==homeName && game.state().shipId==id,"ripcord countdown arrives at station launch exit with same ID");
    printf("Native lifecycle: eject, pod flight, rescue, abandon and refit preserve ship ID %d.\n",id);
}
void alliedStation(const std::filesystem::path& art,int hullId) {
    Native::Assets assets(art);Native::SessionOptions options;options.map=3;options.enemies=0;options.startInHangar=true;
    Native::Game game(art,[&](auto& name,auto&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options);
    auto* core=game.core();auto* side=game.playerShip()->GetSide();auto* own=game.playerShip()->GetStation();auto* ally=core->GetSide(1);auto* station=ally->GetStations()->first()->data();
    side->SetAllies(0);ally->SetAllies(0);
    int gunner=game.state().shipId,pilot=game.addLocalPilot("Allied rearm pilot",0,hullId);require(pilot>=0 && game.boardShip(pilot) && game.controlLocalPilot(pilot),"crewed allied rearm fixture");
    auto* ship=game.playerShip();game.launch();int ammo=ship->GetHullType()->GetMaxAmmo();ship->SetAmmo(0);ship->SetFuel(0);ship->SetFraction(.4f);
    auto* magazine=ship->GetMountedPart(ET_Magazine,0);require(magazine,"cached allied launcher");auto part=magazine->GetPartType()->GetObjectID();magazine->Terminate();
    require(core->GetIgcSite()->DockWithStationEvent(ship,station),"crewed ship accepts allied station docking");game.step({},.001);
    require(!ship->GetStation() && ship->GetCluster()==station->GetCluster() && ship->GetFraction()==1 && ship->GetAmmo()==ammo,"allied rearm repairs/refills and relaunches pilot");
    require(ship->GetMountedPart(ET_Magazine,0) && ship->GetMountedPart(ET_Magazine,0)->GetPartType()->GetObjectID()==part,"allied rearm restores depleted cached launcher");
    auto* child=core->GetShip(gunner);require(child->GetParentShip()==ship && !child->GetStation() && child->GetCluster()==ship->GetCluster(),"allied rearm preserves gunner and relaunches crew");
    game.eject();require(game.state().pod,"allied pod rescue fixture");
    require(core->GetIgcSite()->DockWithStationEvent(ship,station) && ship->GetStation()==own,"allied pod docking returns to last own station");
    require(station->GetSide()==ally && side->GetActiveF(),"allied rearm does not capture a friendly base");
    IhullTypeIGC* boarding=nullptr;for(auto* l=core->GetHullTypes()->first();l;l=l->next())if(l->data()->HasCapability(c_habmBoard)){boarding=l->data();break;}
    require(boarding,"authored boarding hull");ship->SetBaseHullType(boarding);
    auto params=*core->GetMissionParams();params.bAllowFriendlyFire=false;core->SetMissionParams(&params);
    require(!Allegiance::ShouldBoardStation(ship,station) && !Allegiance::ShouldBoardStation(ship,own),"allied/own boarding blocked without friendly fire");
    params.bAllowFriendlyFire=true;core->SetMissionParams(&params);require(Allegiance::ShouldBoardStation(ship,station),"original friendly-fire allied boarding authorization");
    params.bAllowFriendlyFire=false;core->SetMissionParams(&params);side->SetAllies(NA);ally->SetAllies(NA);require(Allegiance::ShouldBoardStation(ship,station),"enemy boarding authorization");
    require(Allegiance::SelectRestartStation(ship,station->GetObjectID(),false)==own,"captured/foreign preferred station falls back to own restart");
    own->SetSide(ally);require(!Allegiance::SelectRestartStation(ship,own->GetObjectID(),false),"no allied/foreign rescue without an own restart");
    puts("Allied station: crew rearm, launch, cached launcher, pod return and boarding authority passed.");
}
void limitedLives(const std::filesystem::path& art,bool teammate) {
    Native::Assets assets(art);Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;options.rules.lives=0;options.startInHangar=true;options.enemies=0;
    Native::Game game(art,[&](auto& name,auto&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options);
    auto* ship=game.playerShip();auto* side=ship->GetSide();int id=ship->GetObjectID();auto loadout=game.currentLoadout();
    if(teammate)require(game.addLocalPilot("Surviving pilot",0)>=0,"living teammate for ghost command view");
    require(game.core()->GetMissionParams()->iLives==0 && !ship->IsGhost(),"original zero-extra-death mission setting");
    game.launch();game.eject();require(!ship->IsGhost(),"ejection alone does not consume a life");game.eject();
    require(ship->IsGhost() && game.state().ghost && game.state().lives==-1 && game.state().commandView,"original strict deaths-greater-than-lives ghost threshold and command view");
    require(ship->GetObjectID()==id && ship->GetParts()->n()==0 && ship->GetBaseHullType()->HasCapability(c_habmLifepod),"exhausted pilot retains identity and pod without free refit");
    game.launch();game.respawn();game.selectHull(0);game.commandView(false);
    require(!ship->GetCluster() && ship->IsGhost() && game.state().commandView,"ghost cannot launch, reset, select a hull or leave command view");
    require(!game.buyHull(loadout.hull) && !game.applyLoadout(loadout) && !game.boardShip(id),"ghost cannot purchase, restore a loadout or board");
    require(Allegiance::MissionHasPlayers(game.core(),side,true,[](auto* s){return s->GetPilotType()>=c_ptPlayer;}),"ghost remains a team player");
    if(teammate) {
        require(side->GetActiveF() && !game.state().complete && ship->GetStation(),"living teammate keeps side active and ghost docked");
        game.camera({});require(!game.scene(false).empty(),"ghost can observe a sector");
    } else require(!side->GetActiveF() && game.state().complete && game.state().result.find("elimination")!=std::string::npos,"last living pilot loss eliminates original side");
}
struct EvacuationFixture : NativeTest::Fixture {
    std::vector<std::tuple<int,int,int>> moves;
    std::vector<int> launched;
    std::vector<IshipIGC*> occupants;
    IstationIGC* home{},*destination{};
    EvacuationFixture(const std::filesystem::path& art,bool childFirst,int ghosts,int alternate):Fixture(art) {
        auto params=*mission->GetMissionParams();params.iLives=0;mission->SetMissionParams(&params);
        home=mission->GetSide(0)->GetStations()->first()->data();
        if(alternate) {
            DataStationIGC data;home->Export(&data);data.stationID=30000;data.position+=Vector(30000,0,0);
            destination=static_cast<IstationIGC*>(mission->CreateObject(mission->GetLastUpdate(),OT_station,&data,sizeof(data)));
            require(destination,"evacuation alternate station");destination->Release();
            if(alternate==1) {
                IstationTypeIGC* nonRestart=nullptr;
                for(auto* l=mission->GetStationTypes()->first();l;l=l->next())if(!l->data()->HasCapability(c_sabmRestart)){nonRestart=l->data();break;}
                require(nonRestart,"evacuation non-restart type");destination->SetBaseStationType(nonRestart);
            }
        }
        auto* parent=ship();auto* child=ship();
        parent->SetCluster(nullptr);child->SetCluster(nullptr);
        child->SetParentShip(parent);child->SetTurretID(NA);
        if(childFirst){child->SetStation(home);parent->SetStation(home);}
        else {parent->SetStation(home);child->SetStation(home);}
        occupants={parent,child};
        for(int i=0;i<ghosts;++i) {auto* ghost=ship();ghost->AddDeath();ghost->SetCluster(nullptr);ghost->SetStation(home);occupants.push_back(ghost);}
        moves.clear();
    }
    void ChangeStation(IshipIGC* ship,IstationIGC* old,IstationIGC* next)override {
        moves.emplace_back(ship->GetObjectID(),old?old->GetObjectID():NA,next?next->GetObjectID():NA);
    }
};
void evacuationReference(const std::filesystem::path& art) {
    int cases=0;
    for(bool childFirst:{false,true})for(int ghosts:{0,1,2})for(int alternate:{0,1,2}) {
        EvacuationFixture reference(art,childFirst,ghosts,alternate),native(art,childFirst,ghosts,alternate);
        OriginalStationEvacuation::VacateStation(reference.home,[&](IshipIGC* ship){reference.launched.push_back(ship->GetObjectID());});
        Allegiance::VacateStation(native.home,[&](IshipIGC* ship){native.launched.push_back(ship->GetObjectID());});
        require(native.moves==reference.moves && native.launched==reference.launched,"frozen original evacuation station/callback order");
        require(native.moves.size()==size_t(2+(alternate==2?ghosts:0)) && native.launched.size()==1,"evacuation must launch the crew before relocating observers");
        require(std::get<0>(native.moves[0])==native.occupants[1]->GetObjectID() && std::get<0>(native.moves[1])==native.occupants[0]->GetObjectID(),"evacuation releases a child from its station before the pilot");
        require(native.occupants[1]->GetParentShip()==native.occupants[0],"evacuation retains original crew relationship");
        for(size_t i=2;i<native.occupants.size();++i)require(native.occupants[i]->GetStation()==(alternate==2?native.destination:native.home) && native.occupants[i]->IsGhost(),"observers only move to another restart base");
        ++cases;
    }
    printf("%d frozen station evacuations: list mutation, child order, observers and restart eligibility passed.\n",cases);
}
void stationLoss(const std::filesystem::path& art,bool capture,bool terminal,bool ongoing=false,bool batched=false) {
    Native::Assets assets(art);Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;options.map=6;
    options.rules.lives=0;options.rules.miners=0;options.startInHangar=true;options.enemies=0;
    if(terminal)options.rules.conquest=0; // exercise elimination rather than conquest victory
    if(ongoing){options.rules.teams=3;options.localPilots.push_back({"Active team pilot",2,false});}
    Native::Game game(art,[&](auto& name,auto&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options);
    auto* core=game.core();auto* ghost=game.playerShip();auto* side=ghost->GetSide();TRef<IstationIGC> home=ghost->GetStation();
    auto* homeCluster=home->GetCluster();
    int pilotId=game.addLocalPilot("Evacuating pilot",0),childId=game.addLocalPilot("Evacuating crew",0);
    require(pilotId>=0 && childId>=0,"station-loss living pilot and crew");
    auto* pilot=core->GetShip(pilotId);auto* child=core->GetShip(childId);
    child->SetParentShip(pilot);child->SetTurretID(NA);child->SetStation(home);
    game.launch();game.eject();game.eject();require(ghost->IsGhost() && ghost->GetStation()==home && !game.state().complete,"station-loss exhausted observer fixture");
    int id=ghost->GetObjectID(),deaths=ghost->GetDeaths(),money=game.pilotBalance(id),value=pilot->GetValue();
    IstationIGC* alternate=nullptr;
    for(auto* l=side->GetStations()->first();l;l=l->next())if(l->data()!=home && l->data()->GetBaseStationType()->HasCapability(c_sabmRestart)){alternate=l->data();break;}
    require(alternate,"split-base evacuation destination");
    if(terminal) {
        std::vector<TRef<IstationIGC>> others;
        for(auto* l=side->GetStations()->first();l;l=l->next())if(l->data()!=home)others.emplace_back(l->data());
        for(auto& base:others){
            int stationId=base->GetObjectID();core->GetIgcSite()->KillStationEvent(base,nullptr,1);
            require(!core->GetStation(stationId) && !base->GetCluster() && !base->GetMission(),"destroyed station leaves IGC before its callback returns");
            if(!batched)game.step({},.001);
        }
        require(!game.state().complete && ghost->GetStation()==home,"observer remains at the last surviving own base");
    }
    int attackerId=game.addLocalPilot("Capturing pilot",1);auto* attacker=core->GetShip(attackerId);require(attacker,"station-loss capturing side fixture");
    if(capture)core->GetIgcSite()->CaptureStationEvent(attacker,home);
    else core->GetIgcSite()->KillStationEvent(home,attacker,1);
    if(!terminal) {
        require(!pilot->GetStation() && pilot->GetCluster()==homeCluster && pilot->GetValue()==value,"station loss launches living pilot without losing its hull/equipment");
        require(child->GetParentShip()==pilot && !child->GetStation() && child->GetCluster()==pilot->GetCluster(),"station loss launches the crew together");
        require(ghost->GetStation()==alternate && !ghost->GetCluster() && ghost->GetDeaths()==deaths && ghost->GetObjectID()==id && ghost->GetParts()->n()==0 && game.pilotBalance(id)==money,"station loss relocates observer without launch/refit, identity, life or money mutation");
    }
    game.step({},.001);
    if(terminal) {
        require(game.state().complete!=ongoing && !side->GetActiveF() && !ghost->GetStation() && !ghost->GetCluster() && ghost->IsGhost(),"last base loss eliminates the side without launching its observer");
        require(core->GetShip(pilotId)==pilot && core->GetShip(childId)==child && !pilot->GetParentShip() && !child->GetParentShip() && !pilot->GetCluster() && !child->GetCluster() && !pilot->GetStation() && !child->GetStation(),"original elimination retains human identities, resets crew and removes their world membership");
        for(auto* human:{pilot,child,ghost})require(human->GetBaseHullType()->HasCapability(c_habmLifepod) && human->GetParts()->n()==0 && human->GetAmmo()==0 && human->GetFuel()==0 && human->GetCommandID(c_cmdQueued)==c_cidNone && human->GetAutoDonate()==nullptr,"original elimination resets each human into an unequipped pod");
        require(pilot->GetDeaths()==1 && child->GetDeaths()==1 && ghost->GetDeaths()==deaths && game.pilotBalance(id)==money,"forced elimination deaths include crew, retain exhausted observer counters and preserve account identity");
        if(ongoing) {
            require(!game.state().sideActive && game.state().dead && core->GetMissionStage()==STAGE_STARTED,"eliminated local side is not a completed three-team mission");
            require(!game.state().docked && game.state().station.empty() && game.state().equipment.empty() && game.state().ammo==0 && !game.state().autopilot && game.state().deaths==ghost->GetDeaths(),"eliminated snapshot retires former hangar, equipment and flight state");
            int count=core->GetShips()->n();double time=game.state().time;int recipient=core->GetSide(2)->GetShips()->first()->data()->GetObjectID();
            require(!game.controlLocalPilot(pilotId) && !game.donateMoney(pilotId,1) && !game.requestMoney(1) && !game.setAutoDonate(pilotId,1) && !game.setWing(pilotId,1) && !game.handOverCommand(pilotId) && game.addLocalPilot("Illegal eliminated seat",0)<0,"eliminated team cannot change controls, accounts, leadership or create seats");
            require(game.sendChat(Native::ChatChannel::Everyone,"Eliminated observers can still chat"),"inactive observer retains ordinary text chat");
            game.respawn();game.launch();game.step({},.1);
            require(core->GetShips()->n()==count && game.state().time>time && !ghost->GetCluster() && !game.state().complete,"other active teams continue while eliminated seats remain immutable");
            require(game.controlLocalPilot(recipient) && game.state().sideActive && game.state().docked && !game.state().dead,"offline control can move to a surviving local seat");
        }
    }
    else require(!game.state().complete && game.state().commandView && game.state().ghost && game.state().docked,"relocated observer retains command-view inspection in the running session");
    printf("Station %s: %s and crew terminal state passed.\n",capture?"capture":"destruction",terminal?"last-base observer elimination":"living crew launch and observer relocation");
}
void carrierSessionChecks(const std::filesystem::path& art,bool conquest) {
    Native::Assets assets(art);Native::SessionOptions options;options.scenario=conquest?Native::Scenario::Conquest:Native::Scenario::FreeFlight;options.enemies=0;options.startInHangar=true;
    Native::Game game(art,[&](auto& name,auto&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options);
    auto* core=game.core();auto* ship=game.playerShip();auto* sector=ship->GetStation()->GetCluster();IhullTypeIGC* hull=nullptr;
    for(auto* l=core->GetHullTypes()->first();l;l=l->next())if(l->data()->HasCapability(c_habmCarrier) && l->data()->GetLandSlots()>0){hull=l->data();break;}
    require(hull && ship->GetBaseHullType()->HasCapability(c_habmLandOnCarrier),"authored carrier and compatible landing ship");
    int id;IshipIGC* carrier;
    if(conquest) {
        DataShipIGC data{};data.shipID=core->GenerateNewShipID();data.hullID=hull->GetObjectID();data.sideID=0;data.pilotType=c_ptPlayer;data.baseObjectID=NA;data.partsOffset=sizeof(data);strcpy(data.name,"Carrier fixture");
        carrier=static_cast<IshipIGC*>(core->CreateObject(core->GetLastUpdate(),OT_ship,&data,sizeof(data)));require(carrier,"create original Conquest carrier fixture");id=carrier->GetObjectID();carrier->Release();
    } else {id=game.addLocalPilot("Carrier",0,hull->GetObjectID());require(id>=0,"create local carrier");carrier=core->GetShip(id);}
    carrier->SetStation(nullptr);carrier->SetCluster(sector);carrier->SetPosition(Vector(0,0,-6000));carrier->SetOrientation(Orientation(Vector(0,0,-1)));carrier->SetVelocity(Vector::GetZero());carrier->SetAutopilot(false);
    while(auto* l=sector->GetAsteroids()->first())l->data()->Terminate();
    IpartTypeIGC* magazine=nullptr;
    for(auto* l=core->GetPartTypes()->first();l;l=l->next())if(l->data()->GetEquipmentType()==ET_Magazine && l->data()->GetPrice()==0 && ship->GetHullType()->CanMount(l->data(),0)){magazine=l->data();break;}
    require(magazine,"free compatible carrier missile");
    if(auto* p=ship->GetMountedPart(ET_Magazine,0))p->Terminate();
    ship->CreateAndAddPart(magazine,0,SHRT_MAX);
    game.launch();ship->GetMountedPart(ET_Magazine,0)->Terminate();ship->SetPosition(carrier->GetPosition()+Vector(0,0,1500));ship->SetVelocity(Vector::GetZero());ship->SetOrientation(Orientation(Vector(0,0,-1)));
    auto* weapon=ship->GetMountedPart(ET_Weapon,0);require(weapon,"weapon before carrier replenishment");int partId=weapon->GetPartType()->GetObjectID();weapon->Terminate();
    ship->SetFraction(.5f);ship->SetAmmo(0);ship->SetFuel(0);ship->SetEnergy(0);
    if(auto* shield=static_cast<IshieldIGC*>(ship->GetMountedPart(ET_Shield,0)))shield->SetFraction(.2f);
    ship->SetBB(core->GetLastUpdate(),core->GetLastUpdate(),0);carrier->SetBB(core->GetLastUpdate(),core->GetLastUpdate(),0);
    ship->SetCommand(c_cmdQueued,carrier,c_cidGoto);require(game.acceptCommand(true),"accept and execute carrier rearm order");
    int frames=0;for(;frames<30000 && ship->GetFraction()<.99f && !game.state().pod && !game.state().dead;++frames)game.step({},1./165);
    if(ship->GetFraction()<.99f)fprintf(stderr,"Carrier %d %s: distance %.1f hull %.3f at frame %d command %d\n",id,hull->GetName(),(ship->GetPosition()-carrier->GetPosition()).Length(),ship->GetFraction(),frames,ship->GetCommandID(c_cmdCurrent));
    require(ship->GetMountedPart(ET_Magazine,0),"actual IGC carrier collision restores depleted free launcher");
    if(conquest)require(!ship->GetMountedPart(ET_Weapon,0),"server carrier policy must not replace lost guns");
    else require(ship->GetMountedPart(ET_Weapon,0) && ship->GetMountedPart(ET_Weapon,0)->GetPartType()->GetObjectID()==partId,"offline carrier policy restores cached missing gun");
    require(ship->GetFraction()==1 && ship->GetAmmo()==ship->GetHullType()->GetMaxAmmo() && ship->GetFuel()==ship->GetHullType()->GetMaxFuel(),"carrier restores hull, ammunition and fuel");
    require(!ship->GetStation() && ship->GetCluster()==sector && ship->GetCommandID(c_cmdAccepted)==c_cidDefend,"carrier relaunch resumes original defend command");
    auto after=ship->GetPosition();for(int i=0;i<165;++i)game.step({},1./165);
    require(!game.state().dead && !game.state().pod && (ship->GetPosition()-after).Length()>10,"carrier relaunch clears the collision hull");
    printf("Actual carrier garage landing/rearm/relaunch: %s, %d frames\n",hull->GetName(),frames);
}

}
int main(int argc,char** argv) {
    try{auto art=argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork();
        if(argc>2 && std::string_view(argv[2])=="--station-loss") {evacuationReference(art);stationLoss(art,false,false);stationLoss(art,true,false);stationLoss(art,false,true);stationLoss(art,false,true,true);stationLoss(art,false,true,false,true);stationLoss(art,false,true,true,true);stationLoss(art,true,true,true,true);return 0;}
        int hull;{NativeTest::Fixture f(art);referenceChecks(f);hull=crewReferenceChecks(f);}sessionChecks(art);crewSessionChecks(art,hull);carrierSessionChecks(art,false);carrierSessionChecks(art,true);alliedStation(art,hull);limitedLives(art,true);limitedLives(art,false);evacuationReference(art);stationLoss(art,false,false);stationLoss(art,true,false);stationLoss(art,false,true);stationLoss(art,false,true,true);stationLoss(art,false,true,false,true);stationLoss(art,false,true,true,true);stationLoss(art,true,true,true,true);return 0;
    }catch(const std::exception& e){fprintf(stderr,"lifecycle: %s\n",e.what());return 1;}
}
