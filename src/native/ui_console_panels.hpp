#pragma once
#include "frontend.hpp"
#include "../ClientCore/ChatComposition.h"
#include "../ClientCore/MouseInput.h"

namespace Native {
// Hangar button sinks and mapped console keys dismiss competing panes before
// dispatching their action. This also retires capture and repeated donations.
template<class Players,class Standings>
void dismissConsolePanes(const FrontResult& action,Players& players,Standings& standings,Game* game=nullptr) {
    if(action.dismissPanes&(ofTeam|ofExpandedTeam))players.close();
    if(action.dismissPanes&ofGameState)standings.close();
    // Training owns the same overlay state. Leaving its bit set would reopen
    // a dismissed Game State pane at the next script/UI synchronization.
    if(action.dismissPanes && game && game->state().trainingMission)game->setTrainingPane(action.dismissPanes,false);
}
template<class Players,class Standings>
FrontResult toggleConsoleMouse(Frontend& frontend,bool& enabled,Players& players,Standings& standings,Game& game,bool menuVisible){
    OverlayMask overlays=(players.active()?ofTeam:0)|(standings.active()?ofGameState:0);
    if(menuVisible){auto parent=frontend.consoleScreen();
        if(parent==Screen::Teleport)overlays|=ofTeleportPane;
        if(parent==Screen::Research)overlays|=ofInvestment;
    }
    auto change=Allegiance::ToggleMouseInput(enabled,overlays,frontend.freshInvestmentPane(),game.state().cinematic);
    enabled=change.enabled;
    if(!change.freshInvestment && frontend.freshInvestmentPane())frontend.releaseInvestmentPointer();
    FrontResult action;action.dismissPanes=change.dismiss;
    dismissConsolePanes(action,players,standings,&game);
    if(change.dismiss&overlays&ofTeleportPane)action.effect=FrontEffect::Resume;
    return action;
}

enum class UiCombatView {None,Interface,Hangar,Loadout,Flight,Command,Override};
inline UiCombatView combatView(const Frontend& frontend,const GameState* state,bool menuVisible) {
    if(!state)return UiCombatView::None;
    if(state->complete || state->networkLobby)return UiCombatView::Interface;
    auto parent=menuVisible?frontend.consoleScreen():std::optional<Screen>{};
    if(parent && *parent!=Screen::Hangar && *parent!=Screen::ShipLoadout && *parent!=Screen::Research && *parent!=Screen::Teleport)return UiCombatView::Interface;
    if(state->cinematic)return UiCombatView::Override;
    if(state->commandView)return UiCombatView::Command;
    if(!state->docked)return UiCombatView::Flight;
    return parent==Screen::ShipLoadout?UiCombatView::Loadout:UiCombatView::Hangar;
}
struct UiViewChange {bool changed{},switchView{};};
class UiViewTransitions {
public:
    void reset(){mode_=UiCombatView::None;}
    UiViewChange observe(const Frontend& frontend,const GameState* state,bool menuVisible) {
        auto next=combatView(frontend,state,menuVisible);
        bool changed=mode_!=UiCombatView::None && mode_!=next;
        mode_=next;
        // OverrideCamera replaces the backdrop directly. EndOverrideCamera
        // calls SetViewMode and clears the destination's banishable panes.
        return {changed,changed && next!=UiCombatView::Override};
    }
private:
    UiCombatView mode_{UiCombatView::None};
};
template<class Players,class Standings,class Capture,class Chat,class Gesture,class Mouse>
bool applyUiViewChange(UiViewChange change,Players& players,Standings& standings,Capture& capture,Chat& chat,Gesture& gesture,Mouse& mouse,typename Mouse::Down& pulses,SDL_MouseButtonFlags heldButtons,Game* game) {
    if(!change.changed)return false;
    capture.cancel();gesture.clear();mouse.suppress(heldButtons);pulses.reset();
    if(change.switchView){
        FrontResult action;action.dismissPanes=ofTeam|ofExpandedTeam|ofGameState;
        dismissConsolePanes(action,players,standings,game);
        // ConsoleData::OnSwitchViewMode preserves keyboard composition.
        if(game)chat.viewChanged(*game);
    }
    return true;
}
}
