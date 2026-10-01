#pragma once
#include "../frontend.hpp"
#include "../test_fixture.hpp"
#include <cstdio>

namespace NativeTest {
inline void checkPopupInput(Native::Assets& assets){
    using namespace Native;
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);
    auto item=[&](std::string_view label){auto& items=ui.view().items;auto i=std::find_if(items.begin(),items.end(),[&](const auto& c){return c.label.starts_with(label);});require(i!=items.end(),"popup input fixture label missing");return size_t(i-items.begin());};
    auto selected=[&](std::string_view label){return ui.view().items[item(label)].selected;};
    auto moveTo=[&](std::string_view label){auto b=ui.view().items[item(label)].bounds;ui.pointerMove(glm::vec2(b)+glm::vec2(3));};
    auto hit=[&]{auto layout=originalMenuLayout({800,600},ui.view().classicScale,ui.view().canvas);return menuHit(ui.view(),{800,600},1,layout.pixel({790,590}));};
    for(bool other:{false,true}){
        ui.open(Screen::Home);ui.open(Screen::Settings);ui.key(SDLK_G,SDL_SCANCODE_G);ui.key(SDLK_Q,SDL_SCANCODE_Q);
        require(ui.keyboardPopup() && selected("Options use native" )==false,"nested popup fixture missing");
        auto sensitivity=settings.sensitivity;moveTo("Raise Mouse Sensitivity");
        if(other)ui.pointerOther();else ui.pointerContext(item("Raise Mouse Sensitivity"));
        require(ui.keyboardPopup() && settings.sensitivity==sensitivity,"non-left inside press executes or dismisses a menu");
        ui.pointerMove({790,590});require(!hit(),"outside popup position hits a captured parent");
        auto result=other?ui.pointerOther():ui.pointerContext(hit());
        require(result.effect==FrontEffect::None && ui.screen()==Screen::Home && !ui.keyboardPopup(),"outside auxiliary/right press fails to close the whole cascade over Home");
    }

    ui.open(Screen::Home);ui.open(Screen::Settings);ui.key(SDLK_G,SDL_SCANCODE_G);ui.key(SDLK_Q,SDL_SCANCODE_Q);
    auto before=settings.sensitivity;ui.pointerPress(item("Raise Mouse Sensitivity"));
    require(!ui.view().capturedButton(),"IMenu row acquires ordinary button capture");ui.pointerHover(item("Lower Mouse Sensitivity"));
    require(selected("Lower Mouse Sensitivity"),"dragging a menu row stops sibling selection");ui.pointerRelease(item("Lower Mouse Sensitivity"));
    require(settings.sensitivity<before,"IMenu drag release executes pressed row instead of hovered row");
    ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);

    ui.open(Screen::Settings);ui.key(SDLK_D,SDL_SCANCODE_D);ui.pointerHover(item("Brightness Down"));
    auto gamma=settings.gamma;ui.key(SDLK_H,SDL_SCANCODE_H);
    require(selected("Brightness Down") && !selected("Higher Resolution") && settings.gamma==gamma,"disabled accelerator changes the original menu selection or settings");
    auto disabled=ui.view().items[item("Higher Resolution")].bounds;auto layout=originalMenuLayout({800,600},1);
    auto disabledHit=menuHit(ui.view(),{800,600},1,layout.pixel(glm::vec2(disabled)+glm::vec2(3)));
    require(disabledHit && *disabledHit==item("Higher Resolution"),"disabled menu row lost original hover input");ui.pointerHover(disabledHit);
    require(selected("Higher Resolution") && ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect==FrontEffect::None,"disabled row hover/Enter differs from IMenu");
    moveTo("Brightness Down");ui.pointerHover(item("Brightness Down"));ui.pointerWheel(-1);
    require(selected("Brightness Down"),"fitting IMenu acquires wheel navigation");
    ui.pointerMove({10,10});ui.pointerContext({});require(ui.keyboardPopup(),"popup border is mistaken for an outside hit");
    ui.pointerOther();require(ui.keyboardPopup(),"auxiliary press dismisses an inside popup border");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);

    ui.open(Screen::Games);
    auto action=[&](MenuOp op,int value=0,int delta=-999){auto& v=ui.view().items;auto i=std::find_if(v.begin(),v.end(),[&](const auto& c){return c.action.op==op && c.action.value==value && (delta==-999 || c.action.delta==delta);});require(i!=v.end(),"popup action fixture missing");return size_t(i-v.begin());};
    ui.activate(action(MenuOp::GameFilter));ui.text("Preserved filter draft");
    auto combo=action(MenuOp::GameFilterCombo);auto anchor=ui.view().items[combo].bounds;ui.pointerMove(glm::vec2(anchor)+glm::vec2(3));ui.pointerPress(combo);
    require(!ui.wantsText() && ui.view().items[action(MenuOp::GameFilterAccept)].blocked,"combo fixture does not capture parent text/actions");
    ui.pointerHover(action(MenuOp::GameFilterChoice,0,2));ui.pointerMove({790,590});ui.pointerContext({});
    require(ui.screen()==Screen::Games && ui.wantsText() && !ui.view().items[action(MenuOp::GameFilterAccept)].blocked,"outside right press accepts/cancels the filter dialog instead of only its menu");
    require(std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.textMode==OriginalControl::TextMode::Edit && c.text=="Preserved filter draft";}),"combo dismissal loses its parent's edit draft");
    combo=action(MenuOp::GameFilterCombo);ui.pointerPress(combo);require(ui.view().items[action(MenuOp::GameFilterChoice,0,2)].selected,"combo cancellation loses IMenu's hovered selection");
    ui.pointerMove({790,590});ui.pointerOther();require(ui.wantsText(),"auxiliary combo dismissal does not restore text focus");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);

    SessionOptions options;options.scenario=Scenario::Conquest;ui.networkSession(options);ui.setPlaying(true);
    GameState lobby;lobby.networked=lobby.networkLobby=lobby.networkHost=lobby.commander=true;lobby.playerSide=0;lobby.shipId=40;lobby.networkHostPilot=40;
    lobby.networkPilots={{40,0,"Owner",true,true,true,true,false},{41,0,"Peer",true,false,false,false,false}};
    ui.open(Screen::RemoteLobby,&lobby);ui.text("Preserved private draft");
    ui.pointerContext(action(MenuOp::RosterSelect,1,41));require(ui.keyboardPopup(),"player popup fixture missing");
    ui.pointerMove({790,590});ui.pointerContext({});
    require(!ui.keyboardPopup() && ui.wantsText() && ui.chatText()=="Preserved private draft" && ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).targetId==41,"outside player popup dismissal loses private selection/draft/focus");
    ui.open(Screen::ConfirmLeave);ui.pointerMove({790,590});ui.pointerOther();ui.pointerContext({});
    require(ui.hasMessageBox(),"outside menu policy dismisses a protected confirmation");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);

    GameState flight;flight.docked=false;flight.ship="Scout";ui.open(Screen::Hangar,&flight);ui.open(Screen::Pause,&flight);ui.key(SDLK_G,SDL_SCANCODE_G);
    ui.pointerMove({790,590});require(ui.pointerOther().effect==FrontEffect::Resume && !ui.keyboardPopup(),"outside auxiliary menu dismissal does not resume flight");
    ui.open(Screen::GameDetails,&flight);ui.pointerMove({790,590});require(ui.pointerOther().effect==FrontEffect::Resume,"outside auxiliary press fails to dismiss the original read-only Details popup");
    puts("Original IMenu input: cascade/border/auxiliary clicks, disabled accelerators/hover, fitting-menu wheel, filter draft/focus, private recipients, protected confirmation and flight return passed");
}
}
