#include "game.hpp"
#include "test_fixture.hpp"
#include "../ClientCore/StationTeleport.h"
#include "../ClientCore/ShipCrew.h"
#include <cstdio>
#include <algorithm>
using NativeTest::require;
namespace {
IstationIGC* copyStation(ImissionIGC* core,IstationIGC* source,int id,SideID side){
    DataStationIGC data;require(source->Export(&data)==sizeof(data),"station export fixture");
    data.stationID=id;data.sideID=side;data.position+=Vector(30000,0,0);strcpy(data.name,"Transfer fixture");
    auto* station=static_cast<IstationIGC*>(core->CreateObject(core->GetLastUpdate(),OT_station,&data,sizeof(data)));
    require(station,"station transfer fixture");station->Release();return station;
}
void eligibility(const std::filesystem::path& art){
    NativeTest::Fixture world(art);auto* station=world.mission->GetStations()->first()->data();int cases=0;
    auto* type=station->GetStationType();
    for(int sender=0;sender<6;++sender)for(int owner=0;owner<6;++owner)for(int allied=0;allied<2;++allied){
        auto* side=world.mission->GetSide(sender);auto* destination=world.mission->GetSide(owner);side->SetAllies(allied?0:NA);destination->SetAllies(allied?0:NA);station->SetSide(destination);
        // Frozen FM_C_DOCKED rejection predicate from 39ef2c072b1eac0e9f97ab1428bc96572cbd5854.
        bool original=!(station==nullptr || (station->GetSide()!=side && !side->AlliedSides(side,station->GetSide())) || !type->HasCapability(c_sabmRestart));
        require(Allegiance::CanTeleportToStation(side,station)==original,"original own/allied/enemy station transfer authorization");++cases;
    }
    require(!Allegiance::CanTeleportToStation(world.mission->GetSide(0),nullptr),"missing transfer target must fail");
    auto data=*static_cast<const DataStationTypeIGC*>(station->GetBaseStationType()->GetData());data.stationTypeID=31000;data.successorStationTypeID=NA;data.sabmCapabilities&=~c_sabmRestart;
    auto* nonRestart=static_cast<IstationTypeIGC*>(world.mission->CreateObject(world.mission->GetLastUpdate(),OT_stationType,&data,sizeof(data)));require(nonRestart,"non-restart station type");
    station->SetBaseStationType(nonRestart);nonRestart->Release();
    require(!Allegiance::CanTeleportToStation(station->GetSide(),station),"own non-restart station must reject teleport");
    printf("%d original station-transfer destination comparisons passed\n",cases);
}
void transfers(const std::filesystem::path& art){
    Native::Assets assets(art);Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;options.startInHangar=true;
    Native::Game game(art,[&](auto& name,auto&){auto model=assets.model(name);return Native::ModelHandle{1,model->radius,model->frames,model->lights};},0,0,options);
    auto* core=game.core();auto* ship=game.playerShip();auto* home=ship->GetStation();int identity=ship->GetObjectID();
    auto* target=copyStation(core,home,30000,0);auto* enemy=copyStation(core,home,30001,1);
    auto* oldHull=ship->GetBaseHullType();int parts=ship->GetParts()->n(),value=ship->GetValue(),money=game.state().money;
    require(!game.teleportStation(home->GetObjectID()) && !game.teleportStation(31000) && !game.teleportStation(enemy->GetObjectID()),"current/missing/enemy station transfer must not mutate ship");
    require(ship->GetStation()==home && ship->GetValue()==value && game.state().money==money,"refused transfer changed equipment or money");
    game.clearMessages();game.clearSounds();
    require(game.teleportStation(target->GetObjectID()),"own restart transfer");
    require(ship->GetObjectID()==identity && ship->GetStation()==target && ship->GetBaseHullType()==oldHull && ship->GetParts()->n()==parts && ship->GetValue()==value && game.state().money==money,"retained hull station transfer must preserve identity, equipment and wallet");
    require(std::count_if(game.sounds().begin(),game.sounds().end(),[](auto& sound){return sound.id==personalJumpSound && sound.relative;})==1,"station transfer must play original personal jump announcement once");
    require(game.state().messages.empty(),"ordinary station teleport adds substitute console text");
    auto* side=ship->GetSide();auto* ally=core->GetSide(1);side->SetAllies(0);ally->SetAllies(0);
    require(game.teleportStation(enemy->GetObjectID()) && ship->GetStation()==enemy && enemy->GetSide()==ally,"allied restart base accepts transfer without capture");
    side->SetAllies(NA);ally->SetAllies(NA);require(game.teleportStation(home->GetObjectID()),"return from revoked allied station to own base");
    auto desired=game.previewLoadout({},false,-1,true).loadout;
    std::erase_if(desired.parts,[&](const auto& part){return (part.mount<0 && part.mount!=-5) || (part.mount==0 && core->GetPartType(part.id)->GetEquipmentType()==ET_Weapon);});
    Native::Loadouts presets;presets.remember(0,desired);game.loadoutPresets(presets);
    auto* original=ship->GetBaseHullType();std::vector<char> bytes(original->Export(nullptr));original->Export(bytes.data());auto* data=reinterpret_cast<DataHullTypeIGC*>(bytes.data());
    data->hullID=30000;data->successorHullID=NA;data->ttbmRequired.SetAll();
    auto* unavailable=static_cast<IhullTypeIGC*>(core->CreateObject(core->GetLastUpdate(),OT_hullType,bytes.data(),int(bytes.size())));require(unavailable,"unavailable hull fixture");ship->SetBaseHullType(unavailable);unavailable->Release();
    require(!target->CanBuy(ship->GetBaseHullType()),"transfer fixture must force original hull refund/replacement branch");
    int total=game.state().money+ship->GetValue();require(game.teleportStation(target->GetObjectID()),"transfer with unavailable hull");
    require(ship->GetObjectID()==identity && ship->GetStation()==target && ship->GetBaseHullType()!=unavailable && !ship->GetBaseHullType()->HasCapability(c_habmLifepod),"unavailable hull must become an original purchasable loadout at the destination");
    require(ship->GetBaseHullType()->GetObjectID()==desired.hull && !ship->GetMountedPart(ET_Weapon,0) && ship->GetMountedPart(NA,-5) && !ship->GetMountedPart(NA,-4),"teleport replacement must use custom zero rather than the previous live fitting");
    require(game.state().money+ship->GetValue()==total,"station refund plus automatic replacement must conserve player credits and ship value");
    game.launch();require(!game.teleportStation(home->GetObjectID()) && game.teleportChoices().empty(),"station teleport must be unavailable while flying");
    puts("Own/allied/refused station travel, original sound, unavailable hull refund/refit, credit conservation and flight refusal passed");
}
void ghost(const std::filesystem::path& art){
    Native::Assets assets(art);Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;options.rules.lives=0;options.startInHangar=true;options.enemies=0;options.map=6;
    Native::Game game(art,[&](auto& name,auto&){auto model=assets.model(name);return Native::ModelHandle{1,model->radius,model->frames,model->lights};},0,0,options);
    require(game.addLocalPilot("Surviving pilot",0)>=0,"ghost transfer surviving teammate");
    auto* ship=game.playerShip();game.launch();game.eject();game.eject();require(ship->IsGhost() && !game.state().complete,"ghost transfer life exhaustion");
    auto choices=game.teleportChoices();auto target=std::find_if(choices.begin(),choices.end(),[](auto& c){return !c.current;});require(target!=choices.end(),"original split-base restart destination");
    int id=ship->GetObjectID(),money=game.state().money;
    require(game.teleportStation(target->id) && game.state().commandView && ship->IsGhost() && ship->GetStation()->GetObjectID()==target->id && ship->GetObjectID()==id && ship->GetParts()->n()==0 && game.state().money==money,"ghost travel preserves observer state, identity and wallet without respawning/refitting");
    game.launch();require(!ship->GetCluster(),"teleport must not let an exhausted pilot launch");
    puts("Ghost station transfer retains observation and exhausted-life rules");
}
void crew(const std::filesystem::path& art){
    Native::Assets assets(art);Native::SessionOptions options;options.startInHangar=true;options.enemies=0;
    Native::Game game(art,[&](auto& name,auto&){auto model=assets.model(name);return Native::ModelHandle{1,model->radius,model->frames,model->lights};},0,0,options);
    auto* core=game.core();auto* ship=game.playerShip();auto* home=ship->GetStation();auto* target=copyStation(core,home,30000,0);
    IhullTypeIGC* gunship=nullptr;for(auto* l=core->GetHullTypes()->first();l;l=l->next()){
        auto* hull=l->data();if(hull->GetMaxWeapons()-hull->GetMaxFixedWeapons()<2)continue;
        for(auto* p=hull->GetPreferredPartTypes()->first();p;p=p->next())if(p->data()->GetEquipmentType()==ET_Weapon && hull->CanMount(p->data(),hull->GetMaxFixedWeapons())){gunship=hull;break;}
        if(gunship)break;
    }
    require(gunship,"crewed transfer hull fixture");int pilot=game.addLocalPilot("Transfer pilot",0,gunship->GetObjectID()),gunner=ship->GetObjectID();
    require(pilot>=0 && game.boardShip(pilot) && game.controlLocalPilot(pilot),"crew boarding before station travel");ship=game.playerShip();auto* passenger=core->GetShip(gunner);
    require(game.teleportStation(target->GetObjectID()) && passenger->GetParentShip()==ship && passenger->GetStation()==target && ship->GetStation()==target,"retained hull transfer must carry its crew together");
    require(game.controlLocalPilot(gunner),"passenger teleport feedback fixture");game.clearMessages();game.clearSounds();
    require(game.teleportStation(home->GetObjectID()),"passenger can disembark and teleport independently");
    require(game.state().messages.size()==1 && game.state().messages.front().text=="You have disembarked." && game.state().messages.front().tip && !game.state().messages.front().warning,"passenger teleport misses original disembark console tip or duplicates feedback");
    require(!passenger->GetParentShip() && passenger->GetStation()==home && ship->GetStation()==target && !passenger->GetBaseHullType()->HasCapability(c_habmLifepod),"passenger travel must preserve parent location and restore its own purchasable ship");
    require(game.boardShip(pilot) && game.controlLocalPilot(pilot),"reboard for unsupported hull crew transfer");int expectedValue=ship->GetValue();std::vector<int> order;
    Allegiance::ApplyStationTeleport(ship,home,true,
        [&](IshipIGC* child){order.push_back(1);require(Allegiance::LeaveDockedShip(child),"unsupported hull must release docked children");},
        [&](Money value){order.push_back(2);require(value==expectedValue && ship->GetChildShips()->n()==0,"original refund must follow crew release and precede part/hull removal");},
        [&](IhullTypeIGC* pod){order.push_back(3);require(pod->HasCapability(c_habmLifepod) && ship->GetParts()->n()==0,"original pod/status callback follows equipment removal");});
    require(order==std::vector<int>({1,2,3}) && passenger->GetStation()==target && !passenger->GetParentShip() && ship->GetStation()==home,"unsupported hull transfer must leave released children at origin and move the independent pilot to destination");
    puts("Retained crew station travel and independent passenger disembark/teleport passed");
}
void dockedReboarding(const std::filesystem::path& art){
    using namespace Native;
    Assets assets(art);SessionOptions options;options.startInHangar=true;options.enemies=0;
    Game game(art,[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
    auto* core=game.core();auto* child=game.playerShip();auto* station=child->GetStation();IhullTypeIGC* hull=nullptr;
    for(auto* l=core->GetHullTypes()->first();l;l=l->next())if(l->data()->GetMaxWeapons()-l->data()->GetMaxFixedWeapons()>=2){hull=l->data();break;}
    require(hull,"docked reboarding multi-turret hull");int id=game.addLocalPilot("Docked captain",0,hull->GetObjectID());auto* parent=core->GetShip(id);require(parent,"docked reboarding captain");
    Mount first=hull->GetMaxFixedWeapons(),last=hull->GetMaxWeapons();
    for(Mount mount=first;mount<last;++mount){
        if(!parent->GetMountedPart(ET_Weapon,mount))for(auto* l=core->GetPartTypes()->first();l;l=l->next())if(l->data()->GetEquipmentType()==ET_Weapon && hull->CanMount(l->data(),mount)){parent->CreateAndAddPart(l->data(),mount,1);break;}
        require(parent->GetMountedPart(ET_Weapon,mount),"equipped docked reboarding turret");
    }
    require(game.boardShip(id) && child->GetTurretID()==first,"initial reboarding crew fixture");
    std::vector<IshipIGC*> others;
    for(Mount mount=first+1;mount<last;++mount){int pilot=game.addLocalPilot("Occupied turret",0);auto* crew=core->GetShip(pilot);require(crew,"docked reboarding crew");crew->SetParentShip(parent);crew->SetTurretID(mount);crew->SetStation(station);others.push_back(crew);}
    int money=game.state().money,value=parent->GetValue(),identity=child->GetObjectID();
    game.clearMessages();game.clearSounds();require(!game.boardShip(id),"ordinary cycle reuses an occupied seat");
    require(game.boardShip(id,false,true) && child->GetParentShip()==parent && child->GetTurretID()==first,"teleport reboarding cannot reuse its released seat on a full ship");
    require(game.state().money==money && parent->GetValue()==value && parent->GetChildShips()->n()==last-first && child->GetObjectID()==identity,"reboarding changes money, parent inventory, crew count or pilot identity");
    require(game.state().messages.size()==2 && game.state().messages.front().text=="You have disembarked." && game.state().messages.back().text=="You have boarded Docked captain's ship as a turret gunner.","same-parent reboarding loses original ordered disembark/board feedback");
    require(Allegiance::LeaveDockedShip(others.front()),"release lower-seat comparison fixture");child->SetTurretID(first+1);
    require(game.boardShip(id,false,true) && child->GetTurretID()==first,"teleport reboarding cycles instead of using the original first-free seat after disembark");
    others.front()->SetParentShip(parent);others.front()->SetTurretID(first+1);others.front()->SetStation(station);
    game.clearMessages();game.clearSounds();require(!game.boardShip(id+65536,false,true) && !game.boardShip(-1,false,true),"boarding aliases an out-of-range pilot ID");
    parent->GetMountedPart(ET_Weapon,first)->Terminate();
    require(!game.boardShip(id,false,true) && child->GetParentShip()==parent && child->GetTurretID()==first && game.state().money==money && game.state().messages.empty(),"failed reboarding detaches the existing crew seat, changes funds or announces success");
    require(game.leaveShip() && !child->GetParentShip() && child->GetStation()==station && parent->GetStation()==station && parent->GetChildShips()->n()==last-first-1,"current-station disembark moves the station or the remaining crew");
    require(std::none_of(game.sounds().begin(),game.sounds().end(),[](const auto& event){return event.id==personalJumpSound;}),"current-station disembark emits the teleport sound");
    Net::Action leave{};leave.op=Net::Operation::Leave;require(game.networkRejected(leave)=="Teleportation failed.","stale disembark loses the original generic boarding failure");
    puts("Docked teleport crew: full-ship reboarding, first-free seat, ordered feedback, failed/aliased-target atomicity, current-station disembark, remaining crew and original failure/sound policy passed");
}
void destinations(const std::filesystem::path& art){
    Native::Assets assets(art);Native::SessionOptions options;options.startInHangar=true;options.enemies=options.allies=0;
    Native::Game game(art,[&](auto& name,auto&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options);
    auto* core=game.core();auto* own=game.playerShip();auto* home=own->GetStation();
    require(std::string_view(core->GetMissionParams()->szIGCStaticFile)==Native::sessionCoreFile(options.scenario),"practice core identity differs from the loaded mission");
    auto* other=copyStation(core,home,30000,0);auto* allied=copyStation(core,home,30001,1);
    core->GetSide(0)->SetAllies(0);core->GetSide(1)->SetAllies(0);
    auto revision=game.state().economyRevision;
    IhullTypeIGC* hull=nullptr;for(auto* l=core->GetHullTypes()->first();l;l=l->next())if(l->data()->GetMaxWeapons()-l->data()->GetMaxFixedWeapons()>=2){hull=l->data();break;}
    require(hull,"destination gunship fixture");int id=game.addLocalPilot("Docked gunship",0,hull->GetObjectID());auto* parent=core->GetShip(id);
    require(parent && game.state().economyRevision>revision,"practice ship status must refresh the live teleport list");
    parent->SetStation(other);
    auto rows=game.teleportDestinations();
    require(rows.size()==3 && rows.front().boarding && rows.front().id==id && rows.front().ship==hull->GetName() && rows.front().manned==0 && rows.front().turrets==hull->GetMaxWeapons()-hull->GetMaxFixedWeapons(),"original destination list must put turret ships before own restart stations");
    require(!rows.front().keepsShip && !rows.front().current && rows[1].current && rows[1].keepsShip && rows[1].id==home->GetObjectID() && rows[2].id==other->GetObjectID(),"destination colors/current station or original station order differ");
    auto travel=game.teleportChoices();require(std::any_of(travel.begin(),travel.end(),[&](auto& r){return r.id==allied->GetObjectID();}),"allied transfer must remain available through existing authority policy");
    require(std::none_of(rows.begin(),rows.end(),[&](auto& r){return !r.boarding && r.id==allied->GetObjectID();}),"original pane must not list allied stations even though transfer authority accepts them");
    int enemy=game.addLocalPilot("Allied gunship",1,hull->GetObjectID());require(enemy>=0,"foreign gunship fixture");
    rows=game.teleportDestinations();require(std::none_of(rows.begin(),rows.end(),[&](auto& r){return r.boarding && r.id==enemy;}),"allied ships must not appear as boarding destinations");
    // Original LastSeenState == c_ssTurret excludes every observer, even
    // when their number exceeds the hull's available turret hardpoints.
    for(int i=0;i<=rows.front().turrets;++i){
        int id=game.addLocalPilot("Ship observer",0);require(id>=0,"observer occupancy fixture");
        auto* observer=core->GetShip(id);observer->SetParentShip(parent);observer->SetTurretID(NA);observer->SetStation(other);
    }
    rows=game.teleportDestinations();require(rows.front().manned==0 && Allegiance::NextTurret(own,parent)!=NA,"observers occupy reported or boardable turret positions");
    for(Mount turret=hull->GetMaxFixedWeapons();turret<hull->GetMaxWeapons();++turret)if(auto* part=parent->GetMountedPart(ET_Weapon,turret))part->Terminate();
    require(!game.boardShip(id) && game.boardingFailure(id)=="Teleportation canceled - no turret positions were open.","unequipped turret rejection must retain original teleport message");
    require(std::any_of(game.sounds().begin(),game.sounds().end(),[](auto& sound){return sound.id==noTurretsSound;}),"original no-turrets message must play noTurretsSound");
    Native::Net::Action rejected{};rejected.op=Native::Net::Operation::Board;rejected.args[0]=id;
    require(game.networkRejected(rejected)=="Teleportation canceled - no turret positions were open.","network boarding rejection differs from offline/original message");
    rejected.op=Native::Net::Operation::Teleport;require(game.networkRejected(rejected)=="Teleportation failed.","network station transfer rejection differs from original message");
    std::vector<int> children;
    for(int seat=0;seat<rows.front().turrets;++seat){int child=game.addLocalPilot("Occupied turret",0);require(child>=0,"turret occupancy fixture");
        auto* passenger=core->GetShip(child);passenger->SetParentShip(parent);passenger->SetTurretID(Mount(hull->GetMaxFixedWeapons()+seat));passenger->SetStation(other);children.push_back(child);}
    rows=game.teleportDestinations();require(rows.front().boarding && rows.front().id==id && rows.front().manned==rows.front().turrets,"fully occupied ships must remain listed, with original turret counts");
    require(game.boardingFailure(id)=="Teleportation failed." && game.boardingFailure(31000)=="Teleportation failed.","full or missing ships must use the original generic failure");
    require(game.controlLocalPilot(children.front()),"current-parent destination fixture");
    rows=game.teleportDestinations();require(rows.front().current && rows.front().keepsShip,"a passenger's current parent must be bold and retain the pod color");
    require(game.controlLocalPilot(own->GetObjectID()),"restore independent observer");
    auto data=*static_cast<const DataStationTypeIGC*>(other->GetBaseStationType()->GetData());data.stationTypeID=31000;data.successorStationTypeID=NA;data.sabmCapabilities&=~c_sabmRestart;
    auto* noRestart=static_cast<IstationTypeIGC*>(core->CreateObject(core->GetLastUpdate(),OT_stationType,&data,sizeof(data)));require(noRestart,"destination non-restart fixture");other->SetBaseStationType(noRestart);noRestart->Release();
    rows=game.teleportDestinations();require(std::none_of(rows.begin(),rows.end(),[&](auto& r){return !r.boarding && r.id==other->GetObjectID();}),"non-restart stations must not appear in original pane");
    parent->SetStation(nullptr);rows=game.teleportDestinations();require(std::none_of(rows.begin(),rows.end(),[&](auto& r){return r.boarding && r.id==id;}),"launched ships must leave the boarding list");
    game.launch();require(game.boardingFailure(id).empty(),"flying pilot must not receive a docked boarding failure popup");require(game.teleportDestinations().empty(),"flight must not expose docked travel destinations");
    rejected.op=Native::Net::Operation::Leave;require(game.networkRejected(rejected).empty(),"stale flight disembark adds a docked failure popup");
    puts("Original combined teleport destinations: ships/stations, membership, observers versus gunners, empty/full turrets, current parent, live practice revisions, launch and restart policy passed");
}
void sectorMap(const std::filesystem::path& art){
    Native::Assets assets(art);Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;options.startInHangar=true;options.allies=options.enemies=0;
    Native::Game game(art,[&](auto& name,auto&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options);
    auto* core=game.core();auto* pilot=game.playerShip();auto* side=pilot->GetSide();auto* home=pilot->GetStation();IclusterIGC* remote=nullptr;
    for(auto* l=core->GetClusters()->first();l;l=l->next())if(l->data()!=home->GetCluster() && l->data()->GetAsteroids()->n() && !l->data()->GetStations()->n()){remote=l->data();break;}
    require(remote,"unknown neutral map sector fixture");
    for(auto* l=remote->GetModels()->first();l;l=l->next())l->data()->SetSideVisibility(side,false);
    require(!game.selectMapSector(remote->GetObjectID()) && !game.orderSector(pilot->GetObjectID(),remote->GetObjectID()),"map actions cannot reveal or order into an unknown sector");
    auto* rock=remote->GetAsteroids()->first()->data();rock->SetSideVisibility(side,true);
    require(game.selectMapSector(remote->GetObjectID()) && game.state().commandView && game.state().sectorId==remote->GetObjectID() && pilot->GetStation()==home,"docked map selection opens command view without teleporting the pilot");
    auto& info=game.sectorInfo(remote->GetObjectID());
    require(info.id==remote->GetObjectID() && info.assets.empty(),"hidden ships or bases leak into sector information");
    float expectedOre=rock->HasCapability(c_aabmMineHe3)?rock->GetOreSeenBySide(side):0;
    require(std::abs(info.helium-expectedOre)<.01f,"sector helium must use observed ore rather than unobserved live resources");
    if(rock->GetCapabilities()&~c_aabmBuildable)require(info.asteroids.size()==1 && info.asteroids.front().count==1,"resource icons must count known rocks only");
    auto* station=copyStation(core,home,30000,side->GetObjectID());station->SetCluster(remote);station->SetSideVisibility(side,true);
    game.commandView(false);require(game.selectMapSector(remote->GetObjectID()),"known station map refresh");
    auto node=std::find_if(game.state().sectors.begin(),game.state().sectors.end(),[&](auto& s){return s.id==remote->GetObjectID();});
    require(node!=game.state().sectors.end() && node->known && node->owner==side->GetObjectID() && node->restartStation==station->GetObjectID(),"original map ownership and restart station selection");
    require(game.sectorInfo(remote->GetObjectID()).assets.size()==1,"newly observed station invalidates cached sector information immediately");
    game.commandView(false);game.launch();
    require(game.selectMapSector(home->GetCluster()->GetObjectID()) && !pilot->GetAutopilot(),"clicking the pilot's sector must not create a movement order");
    require(game.selectMapSector(remote->GetObjectID()) && pilot->GetAutopilot(),"pilot map selection must accept an original sector goto order and enable autopilot");
    auto* target=pilot->GetCommandTarget(c_cmdAccepted);require(target && target->GetObjectType()==OT_buoy && target->GetCluster()==remote && static_cast<IbuoyIGC*>(target)->GetBuoyType()==c_buoyCluster,"sector order retains its cluster buoy after temporary consumers release");
    require(game.state().commands[c_cmdAccepted].command==c_cidGoto,"sector order must retain original goto verb");
    require(!game.orderSector(pilot->GetObjectID()+65536,remote->GetObjectID()) && !game.selectMapSector(remote->GetObjectID()+65536),"map actions cannot truncate invalid identities");
    puts("Sector map: unknown/discovered visibility, observed resources, immediate station info, docked command selection and original pilot goto/buoy lifetime passed");
}

}
int main(int argc,char** argv){try{auto art=argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork();eligibility(art);transfers(art);crew(art);dockedReboarding(art);destinations(art);ghost(art);sectorMap(art);return 0;}catch(const std::exception& e){fprintf(stderr,"Teleport: %s\n",e.what());return 1;}}
