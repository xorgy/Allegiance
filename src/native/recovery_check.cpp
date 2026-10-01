// Original FedSrv recovery/evacuation and WinTrek command/hangar transitions.
// Real IGC and serialized reports only; no renderer, window or scene checks.
#include "game.hpp"
#include "test_fixture.hpp"
#include "../ClientCore/ShipLifecycle.h"
#include <algorithm>
using namespace Native;
using NativeTest::require;
namespace {
auto loader=[](auto&,auto&){return ModelHandle{1,1};};
SessionOptions setup(int lives=32767){SessionOptions o;o.scenario=Scenario::Conquest;o.startInHangar=true;o.enemies=0;o.rules.miners=0;o.rules.teams=3;o.rules.conquest=0;o.rules.territory=100;o.rules.lives=lives;return o;}
IstationIGC* copyStation(Game& game,IstationIGC* source,SideID side,StationID id){
    DataStationIGC data;require(source->Export(&data)==sizeof(data),"recovery station export");data.stationID=id;data.sideID=side;data.position+=Vector(30000,0,0);strcpy(data.name,"Alternate restart");
    auto* station=static_cast<IstationIGC*>(game.core()->CreateObject(game.core()->GetLastUpdate(),OT_station,&data,sizeof(data)));require(station,"recovery station fixture");station->Release();return station;
}
void receive(Game& replica,Game& authority,int id,int tick){auto f=authority.networkFrame(id,tick,0);replica.applyNetworkFrame(Net::frame(Net::packet(f)));}
int heard(const Game& game,int sound){return std::count_if(game.sounds().begin(),game.sounds().end(),[&](const auto& e){return e.id==sound;});}
void ghostRecovery(const std::filesystem::path& art){
    int cases=0;
    for(bool remote:{false,true})for(bool commanding:{false,true}){
        auto o=setup(0);Game host(art,loader,0,0,o,{},GameRole::Authority);auto* pilot=host.playerShip();auto* core=host.core();auto* home=pilot->GetStation();
        require(host.addLocalPilot("Surviving teammate",0)>=0,"ghost recovery keeps faction alive");host.launch();auto* sector=pilot->GetCluster();
        if(remote)for(auto* l=core->GetClusters()->first();l;l=l->next())if(l->data()!=sector){sector=l->data();break;}
        pilot->SetCluster(sector);pilot->SetPosition(Vector(50000,0,0));pilot->SetVelocity(Vector::GetZero());
        for(int i=0;i<31;++i)host.authorityStep({},.1); // Retire the earlier launch override before exercising F2.
        Game replica(art,loader,0,0,o,{},GameRole::Replica);receive(replica,host,pilot->GetObjectID(),240);
        if(commanding){replica.commandView(true,true);require(replica.viewSector(home->GetCluster()->GetObjectID()),"pre-death command sector");}
        core->GetIgcSite()->KillShipEvent(core->GetLastUpdate(),pilot,nullptr,-1,pilot->GetPosition(),Vector::GetZero());receive(replica,host,pilot->GetObjectID(),241);
        require(pilot->IsGhost() && pilot->GetStation() && !host.state().complete,"actual exhausted-life restart");
        require(replica.state().ghost && replica.state().docked && replica.state().commandView,"exhausted network pilot does not automatically enter command view");
        require(replica.state().sectorId==sector->GetObjectID(),"ghost recovery loses the dying ship's sector");
        replica.clearSounds();for(int i=0;i<55;++i)replica.predict({},.1);
        require(!replica.state().cinematic && replica.state().commandView && replica.state().sectorId==sector->GetObjectID(),"death override ends in the wrong observer view/sector");
        require(heard(replica,personalJumpSound)==1,"death-to-hangar/command completion misses its original personal jump cue");
        replica.commandView(false);require(replica.state().commandView && !replica.viewCamera(CameraMode::Cockpit) && !replica.restoreDockedLoadout().accepted,"ghost enters pilot view or purchases a replacement hull");
        replica.clearSounds();receive(replica,host,pilot->GetObjectID(),242);replica.predict({},.1);require(!heard(replica,personalJumpSound),"repeated recovery report replays the completion cue");
        Game newcomer(art,loader,0,0,o,{},GameRole::Replica);receive(newcomer,host,pilot->GetObjectID(),242);require(newcomer.state().ghost && newcomer.state().commandView,"cold exhausted-pilot replica stays in the cockpit");
        newcomer.clearSounds();for(int i=0;i<55;++i)newcomer.predict({},.1,false);require(!heard(newcomer,personalJumpSound),"prediction replay emits the presentation-only recovery cue");newcomer.predict({},.1);require(heard(newcomer,personalJumpSound)==1,"prediction replay loses the pending recovery cue");
        Game display(art,loader,0,0,o,{},GameRole::Replica);receive(display,host,pilot->GetObjectID(),242);
        auto simulationTime=display.core()->GetLastUpdate();double reportedTime=display.state().time;display.clearSounds();
        for(int i=0;i<55;++i)display.presentPrediction(0,{},.1);
        require(!display.state().cinematic && heard(display,personalJumpSound)==1,"docked display frames fail to complete the death override/cue without flight inputs");
        require(display.core()->GetLastUpdate()==simulationTime && display.state().time==reportedTime && display.state().docked,"docked override completion advances IGC or launches the observer");
        display.presentPrediction(0,{},.1);require(heard(display,personalJumpSound)==1,"docked display repeats the completion cue");
        auto* alternate=copyStation(host,home,0,31000);require(host.teleportStation(alternate->GetObjectID()),"ghost station transfer");
        if(!host.state().commandView || host.state().sectorId!=sector->GetObjectID())throw std::runtime_error("ghost station transfer discards its observed sector: expected="+std::to_string(sector->GetObjectID())+", actual="+std::to_string(host.state().sectorId)+", authority view="+std::to_string(host.networkViewSector(pilot->GetObjectID())));
        receive(replica,host,pilot->GetObjectID(),243);require(replica.state().stationId==alternate->GetObjectID() && replica.state().commandView && replica.state().sectorId==sector->GetObjectID(),"ghost station transfer changes replica observation");++cases;
    }
    printf("%d exhausted-pilot report cases: pilot/command entry, local/remote death sector, override completion, ghost gates and cold replica passed\n",cases);
}
void alliedPodReturn(const std::filesystem::path& art){
    int cases=0;
    for(auto role:{GameRole::Offline,GameRole::Authority})for(int status=0;status<3;++status){
        auto o=setup();Game game(art,loader,0,0,o,{},role);auto* ship=game.playerShip();auto* core=game.core();TRef<IstationIGC> home=ship->GetStation();
        auto* alternate=copyStation(game,home,0,31000);auto* allied=copyStation(game,home,1,31001);
        auto policy=game.missionPolicy();policy.alliances[0]=policy.alliances[1]=0;require(game.applyMissionPolicy(policy),"allied-pod policy");
        auto wanted=game.currentLoadout();game.rememberEditorLoadout(wanted);int money=game.state().money;game.launch();game.eject();require(ship->GetBaseHullType()->HasCapability(c_habmLifepod),"actual pod ejection");
        if(status==1)core->GetIgcSite()->KillStationEvent(home,nullptr,1);else if(status==2)home->SetSide(core->GetSide(1));
        auto* expected=status?alternate:static_cast<IstationIGC*>(home);
        require(core->GetIgcSite()->DockWithStationEvent(ship,allied) && ship->GetStation()==expected && ship->GetSide()==core->GetSide(0),"allied pod does not return to its last surviving own base/fallback");
        require(ship->GetCluster()==nullptr && ship->GetParentShip()==nullptr && !ship->IsGhost(),"allied pod recovery leaves a flight/crew relationship");
        if(role==GameRole::Offline){game.step({},.001);require(game.currentLoadout().hull==wanted.hull && !game.state().pod && game.state().money==money,"allied pod recovery loses cached hull or mints credits");}
        else {Game replica(art,loader,0,0,o,{},GameRole::Replica);receive(replica,game,ship->GetObjectID(),60);require(replica.state().docked && replica.state().stationId==expected->GetObjectID() && replica.state().pod && !replica.state().dead,"allied-pod destination/lifecycle not retained in raw reports");}
        ++cases;
    }
    printf("%d allied-pod recovery cases: offline/authority, retained/destroyed/captured last base, own fallback, cached hull and wallet passed\n",cases);
}
IhullTypeIGC* equipCrewHull(Game& game){
    auto* core=game.core();auto* pilot=game.playerShip();IhullTypeIGC* best=nullptr;
    for(auto* l=core->GetHullTypes()->first();l;l=l->next()){
        auto* hull=l->data();if(hull->GetMaxWeapons()-hull->GetMaxFixedWeapons()<2)continue;
        for(auto* p=hull->GetPreferredPartTypes()->first();p;p=p->next())if(p->data()->GetEquipmentType()==ET_Weapon && hull->CanMount(p->data(),hull->GetMaxFixedWeapons())){best=hull;break;}
        if(best)break;
    }
    require(best,"two-gunner recovery hull");while(auto* l=pilot->GetParts()->first())l->data()->Terminate();pilot->SetBaseHullType(best);
    for(Mount m=best->GetMaxFixedWeapons();m<best->GetMaxWeapons();++m)for(auto* p=best->GetPreferredPartTypes()->first();p;p=p->next())if(p->data()->GetEquipmentType()==ET_Weapon && best->CanMount(p->data(),m)){require(pilot->CreateAndAddPart(p->data(),m,32767),"recovery turret equipment");break;}
    return best;
}
std::vector<int> crew(Game& game){
    auto* parent=game.playerShip();equipCrewHull(game);std::vector<int> ids{parent->GetObjectID()};
    for(int i=0;i<2;++i){int id=game.addLocalPilot(i?"Second gunner":"First gunner",0);require(id>=0,"recovery gunner identity");Net::Action board;board.op=Net::Operation::Board;board.args[0]=parent->GetObjectID();require(game.authorityAction(id,board),"actual recovery gunner boarding");ids.push_back(id);}
    // Observer permission is a separate authenticated-cheat surface. Construct
    // its original IGC relationship here without granting that permission.
    int observer=game.addLocalPilot("Observer",0);require(observer>=0,"recovery observer identity");auto* child=game.core()->GetShip(observer);child->SetParentShip(parent);child->SetTurretID(NA);child->SetStation(parent->GetStation());ids.push_back(observer);
    require(parent->GetChildShips()->n()==3,"complete recovery crew");return ids;
}
void crewLoss(const std::filesystem::path& art){
    int cases=0;
    for(int fate=0;fate<4;++fate){
        auto o=setup(fate==3?0:32767);o.rules.ejectPods=fate!=2;Game host(art,loader,0,0,o,{},GameRole::Authority);auto* core=host.core();auto ids=crew(host);
        require(host.addLocalPilot("Remaining team pilot",0)>=0,"crew recovery surviving faction pilot");host.launch();
        std::vector<std::unique_ptr<Game>> views;for(int id:ids){views.push_back(std::make_unique<Game>(art,loader,0,0,o,EffectLoader{},GameRole::Replica));receive(*views.back(),host,id,60);}
        auto* parent=core->GetShip(ids.front());core->GetIgcSite()->KillShipEvent(core->GetLastUpdate(),parent,nullptr,fate==1 || fate==3?-1:100,parent->GetPosition(),Vector::GetZero());
        require(parent->GetChildShips()->n()==0,"parent loss retains attached crew");
        for(size_t i=0;i<ids.size();++i){auto* pilot=core->GetShip(ids[i]);receive(*views[i],host,ids[i],61);
            require(pilot && !pilot->GetParentShip() && pilot->GetBaseHullType()->HasCapability(c_habmLifepod) && views[i]->state().shipId==ids[i] && !views[i]->state().passenger,"crew loss replaces identity, misses pod or retains parent");
            require(pilot->GetDeaths()==int(fate!=0) && pilot->GetEjections()==int(fate==0),"crew death/ejection consumes the wrong life counter");
            require(bool(pilot->GetStation())==(fate!=0) && bool(pilot->GetCluster())==(fate==0) && views[i]->state().docked==(fate!=0),"crew recovery enters the wrong flight/hangar state");
            require(views[i]->state().ghost==(fate==3) && (fate!=3 || views[i]->state().commandView),"exhausted crew member lacks command view");++cases;
        }
    }
    printf("%d crew-loss reports: captain, two gunners and observer, ejection/fatal/no-pods/exhausted lives, preserved IDs and independent recovery passed\n",cases);
}
void evacuation(const std::filesystem::path& art){
    int cases=0;
    for(bool captured:{false,true}){
        auto o=setup(0);Game host(art,loader,0,0,o,{},GameRole::Authority);auto* core=host.core();auto ids=crew(host);TRef<IstationIGC> base=host.playerShip()->GetStation();auto* alternate=copyStation(host,base,0,31000);
        auto* sector=base->GetCluster();int ghost=host.addLocalPilot("Exhausted observer",0);require(ghost>=0,"evacuation ghost");core->GetShip(ghost)->SetDeaths(1);ids.push_back(ghost);
        int solo=host.addLocalPilot("Independent pilot",0);require(solo>=0,"evacuation solo pilot");ids.push_back(solo);int parts=host.playerShip()->GetParts()->n();
        if(captured){int enemy=host.addLocalPilot("Capturing pilot",1);require(enemy>=0,"capture actor");core->GetIgcSite()->CaptureStationEvent(core->GetShip(enemy),base);}
        else core->GetIgcSite()->KillStationEvent(base,nullptr,1);
        require(host.playerShip()->GetChildShips()->n()==3 && host.playerShip()->GetParts()->n()==parts,"base loss dismantles the crew or its equipment");
        for(int id:ids){auto* ship=core->GetShip(id);bool exhausted=id==ghost;
            require(exhausted?ship->GetStation()==alternate && !ship->GetCluster():!ship->GetStation() && ship->GetCluster()==sector,"base loss launches an exhausted observer or fails to launch a live crew/pilot");
            Game replica(art,loader,0,0,o,{},GameRole::Replica);receive(replica,host,id,60);require(replica.state().docked==exhausted && replica.state().ghost==exhausted && replica.state().passenger==(id!=ids[0] && id!=ghost && id!=solo),"evacuation report loses original crew/observer state");++cases;
        }
    }
    printf("%d base-loss reports: capture/destruction, preserved crew/equipment, solo launch and exhausted-observer relocation passed\n",cases);
}
}
int main(int argc,char** argv){
    auto art=argc>1?std::filesystem::path(argv[1]):Assets::steamArtwork();int failures=0;
    for(auto check:{ghostRecovery,alliedPodReturn,crewLoss,evacuation})
        try{check(art);}catch(const std::exception& e){fprintf(stderr,"Recovery: %s\n",e.what());++failures;}
    return failures?1:0;
}
