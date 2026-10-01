#pragma once
#include "frontend.hpp"

namespace Native {
enum class UiPointerOwner {None,Frontend,Players,GameState,Map};

// GroupImage routes every mouse edge and motion to its captured child before
// testing newer images, including popups. Menu lifetime alone is not capture.
template<class Players,class State,class Map>
class UiPointerCapture {
public:
    UiPointerCapture(Frontend& frontend,Players& players,State& state,Map& map)
        :frontend_(frontend),players_(players),state_(state),map_(map){}
    UiPointerOwner owner()const {
        if(frontend_.buttonCaptured())return UiPointerOwner::Frontend;
        if(players_.pointerCaptured())return UiPointerOwner::Players;
        if(state_.pointerCaptured())return UiPointerOwner::GameState;
        if(map_.pointerCaptured())return UiPointerOwner::Map;
        return UiPointerOwner::None;
    }
    bool active()const{return owner()!=UiPointerOwner::None;}
    bool move(glm::ivec2 size,glm::vec2 point){
        switch(owner()){
        case UiPointerOwner::Frontend:
            frontend_.pointerMove(originalMenuLayout(size,frontend_.view().classicScale,frontend_.view().canvas).local(point),size);
            frontend_.pointerHover(menuHit(frontend_.view(),size,1,point));break;
        case UiPointerOwner::Players:players_.hover(size,point);break;
        case UiPointerOwner::GameState:state_.hover(size,point);break;
        case UiPointerOwner::Map:map_.mapPointerMove(point);break;
        case UiPointerOwner::None:return false;
        }
        return true;
    }
    template<class FrontAction,class MapAction>
    bool button(glm::ivec2 size,glm::vec2 point,int button,bool down,bool twice,bool control,
                FrontAction&& frontAction,MapAction&& mapAction){
        auto held=owner();if(held==UiPointerOwner::None)return false;
        move(size,point);
        switch(held){
        case UiPointerOwner::Frontend:
            if(down)frontAction(frontend_.pointerCapturedPress(button));
            else {
                if(button==SDL_BUTTON_LEFT || button==SDL_BUTTON_RIGHT)frontAction(frontend_.pointerRelease(menuHit(frontend_.view(),size,1,point),twice,button));
            }
            break;
        case UiPointerOwner::Players:
            if(auto result=players_.pointer(size,point,button,down,twice))frontAction(*result);break;
        case UiPointerOwner::GameState:state_.pointer(size,point,button,down);break;
        case UiPointerOwner::Map:
            if(auto result=map_.mapPointerButton(point,button,down,control);result.action)mapAction(*result.action);break;
        case UiPointerOwner::None:break;
        }
        return true;
    }
    void cancel(){frontend_.cancelPointer();players_.cancelPointer();state_.cancelPointer();map_.cancelPointer();}
private:
    Frontend& frontend_;Players& players_;State& state_;Map& map_;
};
}
