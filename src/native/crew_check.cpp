#include "crew_pane.hpp"
#include "test_fixture.hpp"
using namespace Native;
using NativeTest::require;
int main(int argc,char** argv)try {
    require(argc>1,"ARTWORK argument required");Assets assets(argv[1]);SessionOptions options;options.startInHangar=true;options.enemies=0;
    Game game(argv[1],[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
    Loadout own=game.currentLoadout();int self=game.state().shipId,parentId=game.addLocalPilot("Captain's pilot name",0);require(parentId>=0,"crew pilot fixture");
    auto* parent=game.core()->GetShip(parentId);auto* child=game.playerShip();IhullTypeIGC* hull=nullptr;
    for(auto* h=game.core()->GetHullTypes()->first();h;h=h->next())if(h->data()->GetGroupID()>=0 && h->data()->GetMaxWeapons()>h->data()->GetMaxFixedWeapons()){hull=h->data();break;}
    require(hull,"crew turret hull fixture");while(auto* p=parent->GetParts()->first())p->data()->Terminate();parent->SetBaseHullType(hull);
    auto tip=[&](const char* text,bool critical=false){return std::count_if(game.state().messages.begin(),game.state().messages.end(),[&](const auto& message){return message.text==text && message.tip && message.warning==critical && !message.chat;});};
    auto heard=[&](int id){return std::count_if(game.sounds().begin(),game.sounds().end(),[&](const auto& sound){return sound.id==id && sound.relative;});};
    game.clearMessages();game.clearSounds();
    require(game.boardShip(parentId,true) && game.state().passenger && game.state().turret<0,"explicit permitted observer boarding");
    require(tip("You have boarded Captain's pilot name's ship as an observer.")==1 && game.state().messages.size()==1 && heard(newNonCriticalMsgSound)==1 && heard(boardSound)==0,"observer boarding loses original personal console tip/sound or receives its parent's notification");
    require(game.state().sourcePilot=="Captain's pilot name" && game.state().sourcePilot!=game.state().ship,"crew display names the parent pilot, not the hull");
    auto preview=game.previewLoadout();require(preview.valid && !preview.equivalent && preview.oldValue==0 && preview.loadout.hull==hull->GetObjectID(),"observer preview restores the parent loadout without treating the parent's ship as a trade-in");
    CrewPane pane(assets);glm::ivec2 size{1920,1080};auto& observer=pane.view(size,1,game.state(),false,true);require(observer.artwork.empty(),"original flight display is absent for an observer");
    auto& loadout=pane.view(size,1,game.state(),true,true);require(loadout.artwork.size()==1 && loadout.artwork[0].text=="Observer of Captain's pilot name" && loadout.artwork[0].font=="medVerdana" && loadout.artwork[0].bounds.x==598 && loadout.artwork[0].bounds.y==520,"observer loadout uses original text, font and center offset");
    Mount turret=hull->GetMaxFixedWeapons();for(auto* p=game.core()->GetPartTypes()->first();p;p=p->next())if(p->data()->GetEquipmentType()==ET_Weapon && hull->CanMount(p->data(),turret)){auto* weapon=parent->CreateAndAddPart(p->data(),turret,1);weapon->SetMountedFraction(1);weapon->Arm();break;}
    game.clearMessages();game.clearSounds();
    require(parent->GetMountedPart(ET_Weapon,turret) && game.boardShip(parentId) && game.state().turret==turret,"observer enters the original equipped turret");
    require(tip("You have boarded Captain's pilot name's ship as a turret gunner.")==1 && game.state().messages.size()==1 && heard(newNonCriticalMsgSound)==1,"turret boarding loses original console feedback");
    auto& flight=pane.view(size,1,game.state(),false,true);auto& caption=authoredControl(flight.artwork,"TurretHeader");auto& pilot=authoredControl(flight.artwork,"PilotNameImage");require(flight.artwork.size()==2 && caption.text=="Turret of:" && pilot.text=="Captain's pilot name" && caption.bounds.x==1740 && caption.bounds.y==75 && pilot.bounds.y==89 && pilot.additive,"original turret caption anchors and additive bitmap text");
    auto* resident=flight.artwork.data();require(pane.view(size,1,game.state(),false,true).artwork.data()==resident,"unchanged crew display retains its compiled controls");
    require(pane.view(size,2,game.state(),true,true).classicScale==1,"whole-scale crew UI falls back when the authored canvas cannot fit at 2x");
    auto& turretLoadout=pane.view({1920,1200},2,game.state(),true,true);require(turretLoadout.artwork.size()==2 && turretLoadout.artwork[0].image && turretLoadout.artwork[0].name=="InTurretPaneImage" && turretLoadout.artwork[1].text=="Turret of Captain's pilot name" && turretLoadout.classicScale==2,"original turret loadout bitmap and integer scaling");
    auto glass=pane.view(size,1,game.state(),false,true,3);
    require(glass.artwork.size()==2 && glass.artwork[0].bounds.x==float(size.x)-210 && glass.artwork[1].bounds.x==float(size.x)-210 && glass.artwork[0].color==glass.artwork[1].color && glass.artwork[0].color.a==.75f,"Glass HUD crew captions preserve their authored offset, faction tint and opacity");
    require(game.viewCamera(CameraMode::Base),"original View Base logical mode is supported");CameraInput camera;camera.mode=CameraMode::Base;auto base=game.camera(camera);camera.mode=CameraMode::External;auto external=game.camera(camera);
    require(base.mode==CameraMode::Base && glm::distance(base.position,external.position)<.01f && glm::distance(base.forward,external.forward)<.01f,"WinTrek's unsupported Base mode retains its actual external-camera fallback");
    auto parentValue=parent->GetValue();Loadout invalid=own;require(!invalid.parts.empty(),"invalid-slot fixture");invalid.parts.push_back(invalid.parts.front());
    game.clearMessages();game.clearSounds();
    require(!game.applyLoadout(invalid) && child->GetParentShip()==parent && parent->GetValue()==parentValue,"invalid purchase cannot detach a passenger or change the parent's equipment");
    require(game.state().messages.empty() && game.sounds().empty(),"rejected purchase announces a disembark that did not occur");
    require(game.applyLoadout(own) && !game.state().passenger && game.state().shipId==self && child->GetBaseHullType()->GetObjectID()==own.hull && parent->GetValue()==parentValue,"valid docked passenger purchase disembarks and fits their own ship, retaining pilot identity and parent equipment");
    require(tip("You have disembarked.")==1 && game.state().messages.size()==1 && heard(newNonCriticalMsgSound)==1,"implicit purchase disembark lacks original console feedback or adds substitute fitting text");
    require(pane.view(size,1,game.state(),false,true).artwork.empty(),"disembarking clears crew labels");
    Loadouts presets;presets.remember(0,own);game.loadoutPresets(presets);
    require(parent->GetStation()->CanBuy(game.core()->GetHullType(own.hull)),"passenger's cached hull remains purchasable");
    auto* launchHull=static_cast<IhullTypeIGC*>(parent->GetStation()->GetSuccessor(game.core()->GetHullType(own.hull)));
    for(bool observer:{true,false}){
        require(game.boardShip(parentId,observer),"passenger launch boarding fixture");
        require(!child->GetVisibleF(),"boarding does not hide the passenger's empty ship");
        Settings settings;Frontend frontend(settings);frontend.originalInterface(assets);frontend.setPlaying(true);
        frontend.loadoutServices([&](const Loadout& l,bool defaults,int hull){return game.previewLoadout(l,defaults,hull);},[&](PartKind kind,int mount,int hull){return game.loadoutPartChoices(kind,mount,hull);});
        frontend.open(Screen::ShipLoadout,&game.state());
        require(frontend.key(SDLK_0,SDL_SCANCODE_0,false,KeyControl).effect==FrontEffect::None && frontend.key(SDLK_0,SDL_SCANCODE_0).effect==FrontEffect::None,"passenger preset shortcuts copy or replace the parent's fitting");
        frontend.open(Screen::Hangar,&game.state());
        auto launch=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& item){return item.action.op==MenuOp::Launch;});
        require(launch!=frontend.view().items.end() && launch->enabled,"original hangar disables Launch for a docked observer/gunner");
        require(frontend.activate(size_t(launch-frontend.view().items.begin())).effect==FrontEffect::Launch,"passenger launch button loses its original action");
        game.clearMessages();game.clearSounds();
        auto purchase=game.purchaseLaunchLoadout();
        require(purchase.accepted && purchase.complete && purchase.launched && !game.state().passenger && !game.state().docked && child->GetBaseHullType()==launchHull && game.state().shipId==self,"passenger hangar launch does not buy and launch their own cached ship with the available hull upgrade");
        require(child->GetVisibleF() && game.camera({}).showPlayer,"observer/gunner purchase launch leaves the replacement hull invisible");
        require(tip("You have disembarked.")==1 && game.state().messages.size()==1 && heard(newNonCriticalMsgSound)==1,"passenger launch loses disembark feedback or adds native launch text");
        auto external=game.scene(true).size();require(external==game.scene(false).size()+1,"launch camera requests the ship but its model is absent from scene submission");
        require(parent->GetStation() && parent->GetValue()==parentValue && !parent->GetChildShips()->n(),"passenger launch moves, trades or changes their parent ship");
        require(game.core()->GetIgcSite()->DockWithStationEvent(child,parent->GetStation()),"passenger launch redocking fixture");game.step({},.02);
    }
    require(game.boardShip(parentId) && game.controlLocalPilot(parentId),"offline promotion feedback fixture");
    game.clearMessages();game.clearSounds();
    require(game.promoteGunner(self) && game.state().passenger,"offline original promotion transaction");
    require(tip("You have been demoted to a turret gunner",true)==1 && heard(newCriticalMsgSound)==1 && heard(newNonCriticalMsgSound)==1 && heard(boardSound)==0,"promotion loses critical feedback, emits a dock-boarding alert or leaks the promoted pilot's tip sound");
    require(game.state().messages.size()==2 && game.state().messages.front().tip && !game.state().messages.front().warning && game.state().messages.back().warning,"original promotion console ordering is boarding then demotion");
    puts("Original observer/turret displays, parent pilot naming, font/anchor/blend/scaling/cache, View Base fallback and docked disembark/purchase parity passed");return 0;
}catch(const std::exception& e){fprintf(stderr,"Crew check: %s\n",e.what());return 1;}
