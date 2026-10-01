#pragma once
#include "settings.hpp"
#include <bitset>
#include <cmath>
#include <algorithm>

namespace Native {
// Original logical button order differs from SDL's middle/right order. Wheel
// entries are pulses, never persistent buttons. Compile aliases on Apply only.
class MouseBindings {
public:
    using Down=std::bitset<size_t(Binding::Count)>;
    explicit MouseBindings(const Settings& settings){configure(settings);}
    void configure(const Settings& settings){
        for(auto& row:bindings_)row.reset();
        auto add=[&](int button,int binding){if(button>=0 && button<10 && binding>=0 && binding<int(Binding::Count))bindings_[button].set(size_t(binding));};
        for(int i=0;i<10;++i)add(i,settings.mouseButtons[i]);
        for(auto& a:settings.mouseAliases)add(a.button,int(a.binding));
    }
    static int index(Uint8 button){
        if(button==SDL_BUTTON_LEFT)return 0;if(button==SDL_BUTTON_RIGHT)return 1;if(button==SDL_BUTTON_MIDDLE)return 2;
        return button>=4 && button<=8?button-1:-1;
    }
    // ConsoleImage owns left/right scene picking with the cursor visible.
    // Its other buttons still call SetMouseTrekKey, independently of steering.
    static SDL_MouseButtonFlags pilotButtons(SDL_MouseButtonFlags buttons,bool originalPointer){
        return originalPointer?buttons&~(SDL_BUTTON_LMASK|SDL_BUTTON_RMASK):buttons;
    }
    Down pressed(Uint8 button)const{int i=index(button);return i<0 || (blocked_&SDL_BUTTON_MASK(button))?Down{}:bindings_[i];}
    void released(Uint8 button){if(index(button)>=0)blocked_&=~SDL_BUTTON_MASK(button);}
    Down wheel(float y)const{return std::isfinite(y) && y!=0?bindings_[y>0?9:8]:Down{};}
    int wheelTicks(float y){
        if(!std::isfinite(y))return 0;wheelRemainder_=std::clamp(wheelRemainder_+y,-64.f,64.f);
        int ticks=int(wheelRemainder_);wheelRemainder_-=ticks;return ticks;
    }
    Down sample(SDL_MouseButtonFlags buttons,bool active){return sample(buttons,active,active);}
    Down sample(SDL_MouseButtonFlags buttons,bool active,bool wheelActive){
        if(!wheelActive)wheelRemainder_=0;
        blocked_&=buttons;if(!active){blocked_|=buttons;return {};}
        Down result;buttons&=~blocked_;
        for(Uint8 i=1;i<=8;++i)if(buttons&SDL_BUTTON_MASK(i))result|=bindings_[index(i)];return result;
    }
    void suppress(SDL_MouseButtonFlags buttons){blocked_|=buttons;}
private:
    std::array<Down,10> bindings_;
    SDL_MouseButtonFlags blocked_{};
    float wheelRemainder_{};
};
}
