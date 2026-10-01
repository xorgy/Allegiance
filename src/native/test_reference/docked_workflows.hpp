#pragma once
#include "../frontend.hpp"
#include "../test_fixture.hpp"

namespace NativeTest {
inline void checkDockedWorkflows(Native::Assets& assets){
    using namespace Native;
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);
    SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.enemies=0;options.rules.miners=0;options.rules.developments=false;
    Game game(assets.root(),[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
    int previews=0;ui.loadoutServices([&](const Loadout& l,bool defaults,int hull){++previews;return game.previewLoadout(l,defaults,hull);},[&](PartKind kind,int mount,int hull){return game.loadoutPartChoices(kind,mount,hull);});
    ui.setPlaying(true);ui.open(Screen::Hangar,&game.state());
    require(!game.state().developments && std::none_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.name=="investButtonPane";}),"no-development mission retains the original hangar research button");
    auto params=*game.core()->GetMissionParams();params.bAllowDevelopments=true;game.core()->SetMissionParams(&params);game.step({},.02);ui.sync(game.state());
    require(game.state().developments && std::any_of(ui.view().items.begin(),ui.view().items.end(),[](const auto& item){return item.action.op==MenuOp::Navigate && item.action.value==int(Screen::Research);}),"live mission developments do not restore the hangar action");
    constexpr Binding commands[]={Binding::Cursor,Binding::PaneInventory,Binding::CommandView,Binding::UnitOrders,Binding::PilotOrders,Binding::CameraCockpit,Binding::CameraChase,Binding::CameraExternal,Binding::CameraBase,Binding::ToggleView,Binding::Research,Binding::Teleport};
    for(auto screen:{Screen::Hangar,Screen::ShipLoadout,Screen::Research,Screen::Teleport}){
        if(screen==Screen::Teleport)ui.teleports(game.teleportDestinations(),game.state());else ui.open(screen,&game.state());
        for(auto binding:commands)require(ui.consoleBinding(binding),"active docked pane swallows an original view/console command");
        require(!ui.consoleBinding(Binding::Missile),"docked command routing admits unrelated flight actions");
        ui.error("Docked modal.");for(auto binding:commands)require(!ui.consoleBinding(binding),"view binding escapes a docked modal");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
        for(auto binding:commands)require(ui.consoleBinding(binding),"modal dismissal fails to restore docked view commands");
        ui.open(Screen::Pause,&game.state());for(auto binding:commands)require(!ui.consoleBinding(binding),"global menu forwards docked game bindings");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    }
    for(auto screen:{Screen::Home,Screen::Training,Screen::Hangar,Screen::ShipLoadout}){
        ui.open(screen,&game.state());auto focus=ui.view().focus;
        for(auto key:{SDLK_RETURN,SDLK_SPACE,SDLK_TAB,SDLK_UP,SDLK_DOWN})require(ui.key(key,SDL_GetScancodeFromKey(key,nullptr)).effect==FrontEffect::None && ui.screen()==screen && ui.view().focus==focus,"classic ButtonPane invents keyboard activation/navigation");
    }
    ui.open(Screen::ShipLoadout,&game.state());require(ui.key(SDLK_0,SDL_SCANCODE_0,false,KeyControl).effect==FrontEffect::SaveLoadout && ui.key(SDLK_0,SDL_SCANCODE_0).effect==FrontEffect::UseLoadout,"owned loadout loses original digit presets");
    auto defaults=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& i){return i.action.op==MenuOp::PreviewDefault;});require(defaults!=ui.view().items.end(),"original loadout defaults control missing");
    require(ui.activate(size_t(defaults-ui.view().items.begin())).effect==FrontEffect::RememberLoadout,"automatic default cache is announced as a keyboard preset save");
    auto fitting=ui.stagedLoadout().loadout;TRef<IstationIGC> station=game.playerShip()->GetStation();
    int unchanged=previews;for(int i=0;i<3;++i)ui.sync(game.state());require(previews==unchanged,"steady editor synchronization rebuilds temporary IGC previews");
    for(auto transfer:{Allegiance::CrewTransfer::Board,Allegiance::CrewTransfer::Disembark,Allegiance::CrewTransfer::Teleport}){
        const char* expected=transfer==Allegiance::CrewTransfer::Board?"Boarding Captain's pilot's ship....":
            transfer==Allegiance::CrewTransfer::Disembark?"Disembarking from Captain's pilot's ship....":"Teleporting to Captain's pilot....";
        ui.transferring(transfer,"Captain's pilot",71);
        require(std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.name=="messageText" && c.text==expected;}),"crew/station transfer lockdown loses original text");
        for(auto key:{SDLK_ESCAPE,SDLK_RETURN,SDLK_TAB,SDLK_F1})require(ui.key(key,SDL_GetScancodeFromKey(key,nullptr)).effect==FrontEffect::None && ui.hasMessageBox(),"pending crew transfer can dismiss its lockdown");
        for(auto binding:commands)require(!ui.consoleBinding(binding),"pending crew transfer forwards an original docked view action");
        ui.sync(game.state());ui.transferFinished(70);
        require(ui.hasMessageBox() && ui.stagedLoadout().loadout==fitting,"unrelated/older action completion releases transfer lockdown or alters the editor draft");
        ui.transferFinished(71);require(!ui.hasMessageBox(),"matching transfer completion retains its lockdown");
    }
    ui.transferring(Allegiance::CrewTransfer::Disembark,"Captain",72);ui.purchasing(false);ui.transferFinished(72);
    require(ui.hasMessageBox() && std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.text=="Loading out ship....";}),"disembark completion clears the replacement purchase lockdown");ui.connectionFinished();
    ui.transferring(Allegiance::CrewTransfer::Board,"Captain",73);ui.error("Teleportation failed.");ui.transferFinished(73);
    require(ui.hasMessageBox() && std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.text=="Teleportation failed.";}),"late transfer completion clears a failure dialog");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    ui.transferring(Allegiance::CrewTransfer::Board,"Captain",74);
    require(!ui.actionRejected(73,{}) && ui.hasMessageBox(),"silent older rejection releases a newer transfer");
    require(!ui.actionRejected(73,"Teleportation failed.") && ui.hasMessageBox() && std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.text=="Boarding Captain's ship....";}),"older failure replaces a newer transfer dialog");
    require(!ui.actionRejected(74,{}) && !ui.hasMessageBox() && ui.stagedLoadout().loadout==fitting,"suppressed original flight failure retains its matching lockdown or alters the editor draft");
    ui.transferring(Allegiance::CrewTransfer::Disembark,"Captain",75);require(ui.actionRejected(75,"Teleportation failed.") && ui.hasMessageBox(),"matching disembark failure does not show the original message");
    require(!ui.actionRejected(75,{}) && ui.hasMessageBox(),"late silent rejection dismisses an ordinary failure dialog");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    require(ui.actionRejected(0,"Unnumbered failure.") && ui.hasMessageBox(),"unnumbered connection failure lost its dialog");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    for(bool launchPending:{false,true}){
        ui.purchasing(launchPending);auto pending=launchPending?"Preparing ship for launch....":"Loading out ship....";
        require(std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.name=="messageText" && c.text==pending;}),"purchase lockdown loses original message");
        for(auto key:{SDLK_ESCAPE,SDLK_RETURN,SDLK_TAB,SDLK_F1})require(ui.key(key,SDL_GetScancodeFromKey(key,nullptr)).effect==FrontEffect::None && ui.hasMessageBox(),"pending purchase dismisses its original lockdown");
        for(auto binding:commands)require(!ui.consoleBinding(binding),"pending purchase accepts a docked view command");
        ui.sync(game.state());require(ui.hasMessageBox() && ui.stagedLoadout().loadout==fitting,"live purchase synchronization loses its lockdown or staged fitting");
        ui.connectionFinished();require(!ui.hasMessageBox() && ui.stagedLoadout().loadout==fitting,"purchase completion loses the original editor draft");
    }
    ui.purchasing(false,81);ui.purchasing(true,82);
    require(!ui.purchaseFinished(81) && ui.hasMessageBox() && std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.text=="Preparing ship for launch....";}),"an older purchase releases a newer launch lockdown");
    require(ui.purchaseFinished(82) && !ui.hasMessageBox() && ui.stagedLoadout().loadout==fitting,"matching purchase fails to release its lockdown or alters the draft");
    ui.transferring(Allegiance::CrewTransfer::Disembark,"Captain",83);
    require(ui.purchaseFinished(83) && !ui.hasMessageBox(),"passenger purchase cannot complete its disembark lockdown");
    ui.purchasing(false,84);ui.error("Purchase declined.");
    require(!ui.purchaseFinished(84) && ui.hasMessageBox() && std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.text=="Purchase declined.";}),"late purchase replaces its failure dialog");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    ui.purchasing(false,85);ui.open(Screen::Hangar,&game.state());
    require(!ui.purchaseFinished(85) && !ui.hasMessageBox() && ui.screen()==Screen::Hangar,"retired editor purchase mutates a later screen");ui.open(Screen::ShipLoadout,&game.state());
    ui.purchasing(true);game.launch();int transition=previews;ui.sync(game.state());require(previews==transition+1,"launch context refresh does not recompute its preview exactly once");
    require(ui.hasMessageBox() && !ui.stagedLoadout().valid && !game.previewLoadout(fitting).valid && !game.core()->GetShip(-2),"post-launch editor refresh retains a purchasable preview or a temporary IGC ship");
    ui.connectionFinished();require(!ui.hasMessageBox() && !game.state().docked,"launch report cannot release purchase lockdown");
    require(game.core()->GetIgcSite()->DockWithStationEvent(game.playerShip(),station),"editor transition redocking fixture");game.step({},.01);ui.open(Screen::Hangar,&game.state());ui.open(Screen::ShipLoadout,&game.state());
    require(ui.stagedLoadout().valid && game.state().docked && !game.core()->GetShip(-2),"redocking cannot reopen a valid original loadout editor");
    ui.setPlaying(false);ui.open(Screen::Training);auto launch=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& item){return item.action.op==MenuOp::TrainingStart;});
    require(launch!=ui.view().items.end() && launch->selected && launch->enabled,"training Launch loses the original checked face");
    ui.trainingStarting();
    require(ui.hasMessageBox() && std::any_of(ui.messageView().artwork.begin(),ui.messageView().artwork.end(),[](const auto& c){return c.text=="Training simulation initiated...";}),"original training startup message missing");
    for(auto key:{SDLK_ESCAPE,SDLK_RETURN,SDLK_TAB})require(ui.key(key,SDL_GetScancodeFromKey(key,nullptr)).effect==FrontEffect::None && ui.hasMessageBox(),"pending training startup accepts dismissal");
    ui.connectionFinished();require(!ui.hasMessageBox(),"training startup completion retains its lockdown");
    auto item=[&](MenuOp op){auto i=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& c){return c.action.op==op;});require(i!=ui.view().items.end(),"widget input fixture missing");return size_t(i-ui.view().items.begin());};
    ui.setPlaying(true);ui.open(Screen::Hangar,&game.state());auto hangarLaunch=item(MenuOp::Launch);
    require(ui.pointerPress(hangarLaunch).effect==FrontEffect::Launch && !ui.pointerCaptured(),"authored FrameImageButtonPane does not launch on down without capture");
    require(ui.pointerRelease(hangarLaunch).effect==FrontEffect::None,"animated Launch release launches twice");
    require(ui.pointerPress(hangarLaunch,true).effect==FrontEffect::Launch && !ui.pointerCaptured(),"FrameImageButtonPane incorrectly suppresses double-click down");ui.pointerRelease(hangarLaunch,true);
    ui.error("Launch blocked.");require(ui.pointerPress(hangarLaunch).effect==FrontEffect::None,"animated Launch escapes a modal");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    ui.setPlaying(false);ui.open(Screen::Controls);auto mapping=item(MenuOp::MappingSelect);ui.pointerPress(mapping);
    require(ui.view().items[item(MenuOp::MappingSelect)].selected && !ui.pointerCaptured() && !ui.capturingKey(),"mapping ListPane does not select on down without capture");ui.pointerRelease(mapping);
    ui.pointerPress(item(MenuOp::MappingSelect),true);require(ui.capturingKey() && !ui.pointerCaptured(),"mapping double-click does not enter edit on down");ui.pointerRelease(item(MenuOp::MappingSelect),true);require(ui.capturingKey(),"mapping release resets double-click edit");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.activate(item(MenuOp::MappingCancel));
    GameState lobby;lobby.networked=true;lobby.networkLobby=true;lobby.networkPilots={{7,0,"Owner",true,true,true,true,false},{91,0,"Peer",true,false,false,false,false}};ui.open(Screen::RemoteLobby,&lobby);
    auto peer=[&](){auto i=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& c){return c.action.op==MenuOp::RosterSelect && c.action.delta==91;});require(i!=ui.view().items.end(),"roster input fixture missing");return size_t(i-ui.view().items.begin());};
    ui.pointerPress(peer());require(ui.view().items[peer()].selected && !ui.pointerCaptured(),"roster ListPane does not select on down without capture");ui.pointerRelease(peer());ui.pointerPress(peer(),true);
    require(ui.view().items[peer()].selected && ui.pointerRelease(peer(),true).effect==FrontEffect::None,"roster double-click invokes the single-click deselection handler");
    ui.pointerPress(peer());require(!ui.view().items[peer()].selected,"separate roster single click does not deselect");ui.pointerRelease(peer());
    auto team=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& c){return c.action.op==MenuOp::SelectTeam && c.control=="team1";});require(team!=ui.view().items.end(),"team input fixture missing");
    auto join=ui.pointerPress(size_t(team-ui.view().items.begin()),true);require(join.effect==FrontEffect::NetworkJoinTeam && join.value==1 && !ui.pointerCaptured(),"team ListPane double-click does not join on down");require(ui.pointerRelease({}).effect==FrontEffect::None,"team double-click release joins twice");
    puts("Original docked workflows: four console hosts, mapped view commands/modal capture, mission development visibility, static-button keyboard policy, owned presets and checked training Launch passed");
    puts("Original purchase lockdown: both messages, non-dismissible pending input, live draft retention, safe post-launch preview retirement and redocked editor restoration passed");
    puts("Purchase completion identity: superseding launch, passenger purchase, late failure and retired-screen immunity passed");
    puts("Original boarding/disembark/teleport lockdowns: authored messages, modal input, matching action completion, replacement-purchase handoff and late failure immunity passed");
    puts("Rejected transfer UI: matching silent-flight release, stale silent/text failure immunity, original disembark failure and unnumbered diagnostics passed");
    puts("Original widget input: authored animated down actions, distinct ButtonPane double-clicks, mapping edit/roster toggles/team join on down and capture-free lists passed");
}
inline void checkDockedViewTransitions(Native::Assets& assets){
    using namespace Native;
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.setPlaying(true);
    SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.enemies=0;options.rules.miners=0;
    Game game(assets.root(),[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
    int remembered=0;Loadout saved;
    ui.loadoutServices([&](const Loadout& l,bool defaults,int hull){return game.previewLoadout(l,defaults,hull);},
        [&](PartKind kind,int mount,int hull){return game.loadoutPartChoices(kind,mount,hull);});
    ui.loadoutMemory([&](const Loadout& loadout){++remembered;saved=loadout;});
    auto docked=game.state();int cases=0;
    for(auto host:{Screen::Hangar,Screen::ShipLoadout,Screen::Teleport,Screen::Research})
    for(auto popup:{Screen::Hangar,Screen::Pause,Screen::Settings,Screen::Controls,Screen::Help,Screen::GameDetails,Screen::Credits})
    for(int event=0;event<3;++event){
        ui.open(Screen::Hangar,&docked);ui.open(host,&docked);
        auto fitting=ui.stagedLoadout().loadout;
        if(popup!=Screen::Hangar)ui.open(popup,&docked);
        if(popup==Screen::Controls){ui.key(SDLK_DOWN,SDL_SCANCODE_DOWN);ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);require(ui.capturingKey(),"forced-launch fixture must enter live key remapping");}
        ui.purchasing(false,701);auto before=remembered;
        require(!ui.releaseDockedView(docked) && ui.pendingAction(701),"unchanged dock snapshot closes a live mode or lockdown");
        auto next=docked;
        if(event!=1){next.docked=false;next.station.clear();next.stationId=-1;}
        if(event!=0){next.ghost=true;next.commandView=true;}
        if(event==2)next.sideActive=false;
        if(!ui.releaseDockedView(next))throw std::runtime_error("combat-mode retirement failed: host="+std::to_string(int(host))+" popup="+std::to_string(int(popup))+" event="+std::to_string(event));
        require(!ui.hasMessageBox() && !ui.pointerCaptured() && !ui.keyboardPopup() && !ui.capturingKey() && !ui.closingConsole(),"mode transition retains modal, pointer/menu/key capture or a stale sliding console");
        require(remembered-before==(host==Screen::ShipLoadout?1:0) && (host!=Screen::ShipLoadout || saved==fitting),"mode retirement must remember a loadout draft exactly once, including beneath secondary popups");
        require(!ui.purchaseFinished(701) && !ui.releaseDockedView(next),"old receipt or repeated state reopens a retired docked mode");
        ++cases;
    }
    for(auto screen:{Screen::Home,Screen::Training,Screen::RemoteLobby,Screen::Pilots,Screen::Results}){
        ui.open(screen,&docked);ui.open(Screen::Settings,&docked);
        auto next=docked;next.docked=false;next.ghost=true;
        require(!ui.releaseDockedView(next) && ui.keyboardPopup(),"ship lifecycle closes independent screen/options input");
    }
    ui.open(Screen::Hangar,&docked);auto completed=docked;completed.complete=true;completed.docked=false;
    require(!ui.releaseDockedView(completed),"postgame must take precedence over a flight-mode transition");
    auto lobby=docked;lobby.networkLobby=true;lobby.docked=false;
    require(!ui.releaseDockedView(lobby),"new-round team lobby must take precedence over the previous docked mode");
    ui.open(Screen::Hangar,&docked);auto ghost=docked;ghost.ghost=true;ui.sync(ghost);
    for(const auto& item:ui.view().items)if(item.action.op==MenuOp::Launch || item.action.op==MenuOp::Navigate && item.action.value==int(Screen::ShipLoadout))
        require(!item.enabled,"unchanged money/revision leaves an exhausted pilot's hangar controls enabled");
    for(auto host:{Screen::Home,Screen::Hangar,Screen::ShipLoadout,Screen::RemoteLobby})
    for(auto popup:{Screen::Pause,Screen::Settings})for(int action=0;action<3;++action){
        ui.open(Screen::Hangar,&docked);ui.open(host,&docked);ui.open(popup,&docked);
        if(action==0)ui.purchasing(false,702);
        else if(action==1)ui.transferring(Allegiance::CrewTransfer::Teleport,"Home",702);
        else ui.configuring();
        require(ui.screen()==host && ui.hasMessageBox(),"async lockdown discards an original menu's parent and displays a replacement command menu");
        ui.connectionFinished();require(ui.screen()==host && !ui.keyboardPopup(),"async completion cannot return to its original screen");
    }
    printf("%d original combat-mode retirements: four docked hosts, nested menus/mapping/Help/Details/credits, forced launch, exhausted lives, ongoing elimination, draft/receipt/capture and independent-screen precedence passed.\n",cases);
    puts("24 asynchronous purchase/transfer/configuration dialogs preserve the original screen beneath main/options menus.");
}
}
