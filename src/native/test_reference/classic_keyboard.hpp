#pragma once
#include "../frontend.hpp"
#include "../test_fixture.hpp"
#include "../../ClientCore/GlobalKeyFilter.h"

namespace NativeTest {
inline void checkClassicKeyboard(Native::Assets& assets){
    using namespace Native;
    size_t filters=0;
    for(unsigned flags=0;flags<128;++flags)for(TrekKey key=TK_NoKeyMapping;key<=TK_Max;++key){
        Allegiance::GlobalKeyContext context{bool(flags&1),bool(flags&2),bool(flags&4),bool(flags&8),bool(flags&16),bool(flags&32),bool(flags&64)};
        // Frozen OnTrekKeyFilter dispatch and consumption rules, including its
        // intentionally consumed but unavailable Quick Chat key.
        using D=Allegiance::GlobalKeyDisposition;D expected=D::Pass;
        if(!context.splash){
            if(key==TK_ScrnShot || key==TK_Help || key==TK_ChatPageUp || key==TK_ChatPageDown)expected=D::Dispatch;
            if(key==TK_ToggleMouse && context.inGame && context.overrideView && !context.lockedDown && !context.composing)expected=D::Dispatch;
            if(key==TK_MainMenu && (!context.inGame || !context.overrideView) && !context.lockedDown && !context.composing)expected=D::Dispatch;
            if(key==TK_QuickChatMenu){expected=D::Consume;if(context.mission && (!context.inGame || !context.overrideView) && !context.lockedDown && context.quickChatScreen)expected=D::Dispatch;}
        }
        require(Allegiance::FilterGlobalKey(key,context)==expected,"original global input filter dispatch/consume/pass differs");++filters;
    }
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);QuickChatCatalog quick(assets);ui.quickChats(quick);SessionOptions options;options.scenario=Scenario::Conquest;ui.networkSession(options);ui.setPlaying(true);
    GameState lobby;lobby.networked=lobby.networkLobby=lobby.networkHost=lobby.commander=lobby.docked=true;lobby.playerSide=0;lobby.shipId=40;lobby.networkHostPilot=40;
    lobby.networkPilots={{40,0,"Commander",true,true,true,true,false,noTeamRequest,1},{41,0,"Private recipient",true,false,false,false,false,noTeamRequest,1}};
    auto item=[&](MenuOp op,int id=-1){const auto& rows=ui.view().items;auto i=std::find_if(rows.begin(),rows.end(),[&](const auto& c){return c.action.op==op && (id<0 || c.action.delta==id);});require(i!=rows.end(),"classic keyboard action fixture");return size_t(i-rows.begin());};
    auto enter=[&]{ui.open(Screen::Home);ui.open(Screen::RemoteLobby,&lobby);ui.activate(item(MenuOp::RosterSelect,41));ui.activate(item(MenuOp::ComposeChat));while(!ui.chatText().empty())ui.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);ui.text("Retained private draft");};
    for(auto popup:{Screen::Pause,Screen::Settings,Screen::Controls,Screen::Credits,Screen::GameDetails}){
        enter();require(ui.wantsText() && ui.mainMenuInput(),"plain lobby EditPane blocks the original global main-menu filter");
        ui.open(popup);require(!ui.mainMenuInput() && !ui.wantsText(),"foreground popup/mapping capture differs");
        if(popup==Screen::Credits){
            int focus=ui.view().focus;for(auto [key,scan]:{std::pair{SDLK_RETURN,SDL_SCANCODE_RETURN},std::pair{SDLK_SPACE,SDL_SCANCODE_SPACE},std::pair{SDLK_TAB,SDL_SCANCODE_TAB}}){
                require(ui.key(key,scan).effect==FrontEffect::None && ui.screen()==popup && ui.view().focus==focus && ui.chatText()=="Retained private draft","foreground Help/credits sends or edits the retained background chat");
            }
        }
        ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
        require(ui.screen()==Screen::RemoteLobby && ui.wantsText() && ui.mainMenuInput() && ui.chatText()=="Retained private draft","popup return loses lobby draft/focus/menu eligibility");
        auto chat=ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);if(chat.effect!=FrontEffect::SendChat || chat.targetId!=41)throw std::runtime_error("Popup "+std::to_string(int(popup))+" returns chat effect "+std::to_string(int(chat.effect))+" recipient "+std::to_string(chat.targetId));
    }
    // ButtonPane inherits IKeyboardInput's no-op handler. The original lists,
    // EditPanes, dialogs and menus have separate handlers; do not invent a
    // keyboard focus ring or activate an arbitrary button after those return.
    size_t cases=0;
    constexpr std::pair<SDL_Keycode,SDL_Scancode> keys[]={{SDLK_TAB,SDL_SCANCODE_TAB},{SDLK_RETURN,SDL_SCANCODE_RETURN},{SDLK_SPACE,SDL_SCANCODE_SPACE},{SDLK_UP,SDL_SCANCODE_UP},{SDLK_DOWN,SDL_SCANCODE_DOWN}};
    for(auto screen:{Screen::Home,Screen::Training,Screen::Hangar,Screen::RemoteLobby,Screen::Pilots,Screen::Results,Screen::Credits,Screen::Games,Screen::Teleport,Screen::Research}){
        ui.open(Screen::Home);auto state=lobby;state.networkLobby=screen==Screen::RemoteLobby || screen==Screen::Pilots;state.complete=screen==Screen::Results;state.economy=true;
        ui.open(screen,&state);require(ui.view().original,"keyboard matrix accidentally exercises the experimental UI");
        if(ui.wantsText())ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);int focus=ui.view().focus;
        for(auto [key,scan]:keys){auto result=ui.key(key,scan);require(result.effect==FrontEffect::None && ui.screen()==screen && ui.view().focus==focus,"classic screen falls into experimental button navigation/activation");++cases;}
    }
    // Keep the optional native interface's deliberate keyboard navigation.
    Frontend modern(settings);modern.open(Screen::Home);int before=modern.view().focus;modern.key(SDLK_TAB,SDL_SCANCODE_TAB);
    require(!modern.view().original && modern.view().focus!=before,"classic key policy regresses optional native menu navigation");
    enter();auto* retained=ui.view().artwork.data();auto focus=ui.view().focus;ui.open(Screen::Help);
    require(ui.helpOn() && ui.screen()==Screen::RemoteLobby && ui.wantsText() && !ui.keyboardPopup() && ui.mainMenuInput() && retained==ui.view().artwork.data() && focus==ui.view().focus,"independent Help replaces the screen or changes keyboard focus");
    ui.text(" while Help is open");ui.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);
    require(ui.chatText()=="Retained private draft while Help is ope" && ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).targetId==41 && ui.helpOn(),"Help blocks the original focused EditPane or changes its recipient");
    while(!ui.chatText().empty())ui.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);ui.text("Retained private draft");
    require(ui.pageChat(true) && ui.screen()==Screen::RemoteLobby && ui.helpOn(),"Help disconnects the original background chat pager");
    require(ui.lobbyQuickChat() && ui.keyboardPopup() && !ui.mainMenuInput(),"Help disconnects lobby Quick Chat or lets globals bypass its popup");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);require(ui.helpOn() && !ui.keyboardPopup(),"Quick Chat dismissal changes independent Help");
    ui.key(SDLK_F1,SDL_SCANCODE_F1);require(ui.helpOn(),"Help hardcodes a physical key instead of using translated bindings");
    ui.open(Screen::Pause);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);require(ui.screen()==Screen::RemoteLobby && ui.helpOn(),"main menu changes Help or loses its actual parent");
    ui.error("Modal over Help");require(ui.keyboardPopup() && !ui.mainMenuInput(),"Help bypasses a real modal's keyboard capture");ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
    ui.toggleHelp();require(!ui.helpOn() && ui.screen()==Screen::RemoteLobby && ui.wantsText() && ui.chatText()=="Retained private draft","mapped Help toggle loses its original lobby recipient/draft");
    printf("Original global key filter: %zu logical-key/context comparisons, independent Help, translated toggling, main-menu parent and nested modal capture passed\n",filters);
    printf("Classic keyboard: %zu screen/key cases, five popup/mapping captures, nonmodal Help editing/sending, global-menu priority above lobby EditPane, retained private draft/recipient and optional native navigation passed\n",cases);
}
}
