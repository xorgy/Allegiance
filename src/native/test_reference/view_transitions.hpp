#pragma once
#include "../player_pane.hpp"
#include "../game_state_pane.hpp"
#include "../chat_console.hpp"
#include "../command_input.hpp"
#include "../mouse.hpp"
#include "../ui_console_panels.hpp"
#include "../ui_pointer_capture.hpp"
#include "../test_fixture.hpp"

namespace NativeTest {
inline void checkConsoleViewTransitions(Native::Assets& assets){
    using namespace Native;
    SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.enemies=0;options.rules.miners=0;
    Game game(assets.root(),[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
    int peer=game.addLocalPilot("View transition peer",0);require(peer>=0,"view transition peer");
    Settings settings;settings.mouseButtons[0]=int(Binding::Fire);settings.keys[size_t(Binding::YawLeft)]=SDL_SCANCODE_A;settings.modifiers[size_t(Binding::YawLeft)]=KeyAny;
    Frontend ui(settings);ui.originalInterface(assets);ui.setPlaying(true);
    ui.loadoutServices([&](const Loadout& l,bool defaults,int hull){return game.previewLoadout(l,defaults,hull);},[&](PartKind kind,int mount,int hull){return game.loadoutPartChoices(kind,mount,hull);});
    PlayerPane players(assets);GameStatePane standings(assets);QuickChatCatalog quick(assets);ChatConsole chat(assets,quick);CommandGesture gesture;
    struct MapCapture {bool held{};bool pointerCaptured()const{return held;}void cancelPointer(){held=false;}} map;
    UiPointerCapture capture(ui,players,standings,map);
    struct View {GameState state;bool shown{};bool present{true};};
    auto configure=[&](UiCombatView mode){
        View view{game.state(),true,mode!=UiCombatView::None};view.state.docked=true;view.state.commandView=view.state.cinematic=view.state.networkLobby=view.state.complete=false;
        if(mode==UiCombatView::Interface)view.state.networkLobby=true;
        if(mode==UiCombatView::Command)view.state.commandView=true;
        if(mode==UiCombatView::Flight || mode==UiCombatView::Override)view.state.docked=false;
        if(mode==UiCombatView::Override)view.state.cinematic=true;
        ui.open(Screen::Hangar,&view.state);
        if(mode==UiCombatView::Loadout)ui.open(Screen::ShipLoadout,&view.state);
        else if(mode==UiCombatView::Interface)ui.open(Screen::RemoteLobby,&view.state);
        else if(mode==UiCombatView::Command || mode==UiCombatView::Flight || mode==UiCombatView::Override)view.shown=false;
        return view;
    };
    constexpr UiCombatView modes[]={UiCombatView::None,UiCombatView::Interface,UiCombatView::Hangar,UiCombatView::Loadout,UiCombatView::Flight,UiCombatView::Command,UiCombatView::Override};
    size_t cases=0;
    // Reference: screen(), OverrideCamera(), SetViewMode() and the quick-menu
    // close sink. Override entry replaces the backdrop without clearing the
    // old pane flags; ordinary switches clear them. Keyboard composition is
    // retained, while mouse composition and closed quick menus terminate.
    for(auto before:modes)for(auto after:modes)for(int compose=0;compose<6;++compose){
        UiViewTransitions transitions;auto source=configure(before);transitions.observe(ui,source.present?&source.state:nullptr,source.shown);
        players.reset();standings.reset();players.toggle(game);standings.toggle(game);chat.reset();chat.sync(game);
        if(compose==0)require(chat.selectPeer(game,peer),"mouse command selection fixture");
        else if(compose==4)chat.startQuick(game);
        else {
            if(compose==2)chat.verb(game,1);else chat.startChat(game);
            require(chat.selectPeer(game,peer),"keyboard composition recipient fixture");
            chat.text(game,compose==3?"!retained shell draft":"Retained private draft");
            if(compose==5)chat.wings(game);
        }
        auto oldMode=chat.mode();auto draft=chat.typed();auto recipient=chat.audience().recipient;
        map.held=true;gesture.selectionStart=glm::vec2(11,22);gesture.orderStart=glm::vec2(33,44);gesture.anchor=glm::vec3(1,2,3);
        MouseBindings mouse(settings);MouseBindings::Down pulses;pulses.set(size_t(Binding::Fire));KeyboardBindings keyboard(settings);keyboard.keyDown(SDL_SCANCODE_A,SDL_KMOD_NONE);
        auto destination=configure(after);auto change=transitions.observe(ui,destination.present?&destination.state:nullptr,destination.shown);
        bool changed=before!=UiCombatView::None && before!=after,switched=changed && after!=UiCombatView::Override;
        require(change.changed==changed && change.switchView==switched,"view transition differs from original screen/override/mode policy");
        require(applyUiViewChange(change,players,standings,capture,chat,gesture,mouse,pulses,SDL_BUTTON_LMASK,&game)==changed,"production view adapter misses or invents a transition");
        require(players.active()==!switched && standings.active()==!switched,"mode switch leaves banishable panes active or erases override-entry panes");
        require(map.held==!changed && bool(gesture.selectionStart)==!changed && bool(gesture.orderStart)==!changed && bool(gesture.anchor)==!changed,"old map capture or command drag survives view replacement");
        require(pulses.test(size_t(Binding::Fire))==!changed && mouse.sample(SDL_BUTTON_LMASK,true).test(size_t(Binding::Fire))==!changed,"mode switch leaks a held/pulsed mouse action into the new view");
        require(keyboard.sample().test(size_t(Binding::YawLeft)),"view switch clears physical keyboard Boolean input that original ClearButtonStates preserves");
        bool cancelled=switched && (compose==0 || compose==4);
        require(chat.mode()==(cancelled?Allegiance::ComposeState::Idle:oldMode) && (cancelled || (chat.typed()==draft && chat.audience().recipient==recipient)),"mode switch loses keyboard draft/recipient or retains mouse/quick composition");
        if(switched){const auto& v=chat.view({2560,1440},1);require(std::none_of(v.items.begin(),v.items.end(),[](const auto& i){return i.popupLevel>=0;}),"view switch retains an old quick/wing popup");}
        mouse.released(SDL_BUTTON_LEFT);mouse.sample(0,true);require(mouse.pressed(SDL_BUTTON_LEFT).test(size_t(Binding::Fire)),"new mouse press stays suppressed after release");
        // A second observation of the same destination must not erase a new
        // F6 pane, draft or drag opened later in the same SDL event batch.
        if(!players.active())players.toggle(game);if(!standings.active())standings.toggle(game);map.held=true;gesture.orderStart=glm::vec2(7,8);
        auto repeated=transitions.observe(ui,destination.present?&destination.state:nullptr,destination.shown);
        require(!applyUiViewChange(repeated,players,standings,capture,chat,gesture,mouse,pulses,0,&game) && players.active() && standings.active() && map.held && gesture.orderStart,"repeated frame erases newly opened controls in an unchanged view");
        ++cases;
    }
    size_t parents=0;
    for(auto mode:{UiCombatView::Interface,UiCombatView::Hangar,UiCombatView::Loadout,UiCombatView::Flight,UiCombatView::Command})
    for(auto popup:{Screen::Pause,Screen::Settings,Screen::Controls,Screen::Help,Screen::Credits,Screen::GameDetails}){
        auto source=configure(mode);ui.open(popup,&source.state);
        require(combatView(ui,&source.state,true)==mode,"original popup changes its underlying combat/UI mode");++parents;
    }
    // Follow real IGC launch/override completion, docking and command entry,
    // using no model previews, graphics window or renderer.
    ui.open(Screen::Hangar,&game.state());UiViewTransitions actual;actual.observe(ui,&game.state(),true);
    players.reset();standings.reset();players.toggle(game);standings.toggle(game);chat.reset();chat.sync(game);chat.startChat(game);chat.selectPeer(game,peer);chat.text(game,"Physical transition draft");
    TRef<IstationIGC> home=game.playerShip()->GetStation();auto launched=game.purchaseLaunchLoadout();require(launched.accepted && launched.launched && !game.state().docked && game.state().cinematic,"actual IGC launch fixture");
    MouseBindings mouse(settings);MouseBindings::Down pulses;map.held=true;gesture.orderStart=glm::vec2(2,3);
    auto sync=[&]{bool retired=ui.releaseDockedView(game.state());bool shown=game.state().docked && !game.state().commandView && !game.state().cinematic;
        auto change=actual.observe(ui,&game.state(),shown);if(retired){change.changed=true;change.switchView|=!game.state().cinematic;}
        applyUiViewChange(change,players,standings,capture,chat,gesture,mouse,pulses,SDL_BUTTON_LMASK,&game);ui.sync(game.state());};
    sync();require(players.active() && standings.active() && !map.held && !gesture.orderStart,"override entry mishandles resident panes or held map/command input");
    for(int frame=0;frame<600 && game.state().cinematic;++frame){game.step({},1./60);sync();}
    require(!game.state().cinematic && !players.active() && !standings.active() && chat.typed()=="Physical transition draft" && chat.audience().recipient==peer,"actual override completion retains old panes or drops private keyboard composition");
    players.toggle(game);standings.toggle(game);map.held=true;game.playerShip()->SetStation(home);game.step({},.01);sync();
    require(game.state().docked && !players.active() && !standings.active() && !map.held && ui.screen()==Screen::Hangar,"actual docking retains flight panes or map capture");
    players.toggle(game);standings.toggle(game);game.commandView(true);sync();require(!players.active() && !standings.active(),"actual command entry retains old panes");
    players.toggle(game,true);standings.toggle(game);game.commandView(false);sync();require(!players.active() && !standings.active(),"actual command return retains old panes");
    printf("Original view transitions: %zu mode/composition cases, %zu popup parents, map/command capture, physical keyboard versus mouse state, same-batch controls and actual IGC launch/dock/command lifecycle passed\n",cases,parents);
}
}
