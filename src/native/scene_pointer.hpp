#pragma once
#include "command_input.hpp"
#include "chat_console.hpp"

namespace Native {
// ConsoleData::Button receives scene gestures in every flying view when the
// virtual joystick does not own the pointer. Command view always has a cursor.
inline bool scenePointerEnabled(const GameState& state,bool original,bool freePointer) {
    if(state.complete || state.networkLobby)return false;
    return state.commandView || (original && freePointer && !state.docked && !state.dead && !state.cinematicDeath);
}
struct ScenePointerTarget {int type{},id{};};
// The same recipient selection and order dispatch is used from cockpit, chase
// and command cameras. Projection/picking and foreground controls stay outside.
inline bool scenePointerButton(CommandGesture& gesture,Game& game,ChatConsole& chat,
    glm::vec2 point,glm::ivec2 size,const glm::mat4& projection,const CameraState& camera,
    int button,bool down,bool shift,bool original,std::optional<ScenePointerTarget> target) {
    if(button!=SDL_BUTTON_LEFT && button!=SDL_BUTTON_RIGHT)return false;
    gesture.pointer=point;
    if(down){
        // Both buttons establish the ground anchor. A composed Go-to command
        // can be completed with the left button as well as the right button.
        gesture.beginOrder(point,size,projection,camera);
        if(button==SDL_BUTTON_LEFT){gesture.orderStart.reset();gesture.selectionStart=point;}
        return true;
    }
    auto start=button==SDL_BUTTON_LEFT?gesture.selectionStart:gesture.orderStart;
    if(!start)return button==SDL_BUTTON_RIGHT;
    auto delta=point-*start;
    bool objectPick=target && glm::dot(delta,delta)<=100.f;
    if(button==SDL_BUTTON_RIGHT || (original && chat.mode()==Allegiance::ComposeState::Command)){
        bool picked=false;
        if(objectPick)picked=original?chat.pickTarget(game,target->type,target->id):game.commandTarget(target->type,target->id);
        if(!picked)if(auto destination=gesture.destination(size,projection,camera))
            picked=original?chat.pickPosition(game,game.state().sectorId,*destination):game.commandPosition(*destination);
        if(original && button==SDL_BUTTON_LEFT && !picked)chat.cancel(game);
    }else if(Allegiance::CommandSelectionRectangle(delta.x,delta.y)){
        game.pickCommandUnits(commandRectangle(game.contacts(),*start,point,size,projection));
        if(original)chat.selectUnits(game);
    }else if(target){
        if(!original)game.selectTarget(target->type,target->id);
        if(target->type==ContactShip && game.pickCommandUnit(target->id,shift) && original)chat.selectUnits(game);
    }else if(original){
        // Original empty point clicks quit composing while retaining the last
        // command recipients; an empty selection rectangle clears them above.
        chat.cancel(game);
    }else game.pickCommandUnits({});
    if(button==SDL_BUTTON_LEFT)gesture.selectionStart.reset();else gesture.orderStart.reset();
    if(!gesture.selectionStart && !gesture.orderStart)gesture.anchor.reset();
    return true;
}
}
