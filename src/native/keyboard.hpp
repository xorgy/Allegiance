#pragma once
#include "input_bindings.hpp"
#include <bitset>
#include <optional>
#include <span>

namespace Native {
inline uint8_t keyModifiers(SDL_Keymod mod){return ((mod&SDL_KMOD_SHIFT)?KeyShift:0)|((mod&SDL_KMOD_CTRL)?KeyControl:0)|((mod&SDL_KMOD_ALT)?KeyAlt:0);}
inline bool modifierKey(SDL_Scancode scan){return scan==SDL_SCANCODE_LSHIFT || scan==SDL_SCANCODE_RSHIFT || scan==SDL_SCANCODE_LCTRL || scan==SDL_SCANCODE_RCTRL || scan==SDL_SCANCODE_LALT || scan==SDL_SCANCODE_RALT;}
inline std::string keyName(SDL_Scancode scan,uint8_t modifiers){
    if(scan==SDL_SCANCODE_UNKNOWN)return "Unbound";
    std::string name;if(modifiers&KeyControl)name+="Ctrl+";if(modifiers&KeyShift)name+="Shift+";if(modifiers&KeyAlt)name+="Alt+";return name+SDL_GetScancodeName(scan);
}
inline std::string keyName(const Settings& settings,Binding binding){auto i=size_t(binding);return keyName(settings.keys[i],settings.modifiers[i]);}
class KeyboardBindings {
public:
    using Down=std::bitset<size_t(Binding::Count)>;
    explicit KeyboardBindings(const Settings& settings){configure(settings);}
    void configure(const Settings& settings){
        keys_.fill(false);
        for(auto& row:pressed_)row.fill({});for(auto& row:held_)for(auto& bits:row)bits.reset();
        auto add=[&](SDL_Scancode scan,uint8_t modifiers,Binding binding,TrekKey logicalKey,bool exact){
            if(scan<=SDL_SCANCODE_UNKNOWN || scan>=SDL_SCANCODE_COUNT || int(binding)<0 || int(binding)>=int(Binding::Count))return;
            if((modifiers!=KeyAny)!=exact)return;
            for(uint8_t m=0;m<8;++m)if(modifiers==KeyAny || modifiers==m)held_[m][scan].set(size_t(binding));
            // TrekInput::LoadMap puts Any in the unmodified press table only.
            // Its separate Boolean input ignores modifiers for held controls.
            pressed_[modifiers&7][scan]={int16_t(binding),logicalKey<0?trekKey(binding):logicalKey};
        };
        // Any and None occupy the same press slot. Valid original maps never
        // duplicate that slot; prefer an exact entry in older native profiles.
        for(bool exact:{false,true}){
            for(size_t i=0;i<settings.keys.size();++i)add(settings.keys[i],settings.modifiers[i],Binding(i),settings.keyCommands[i],exact);
            for(auto& alias:settings.keyAliases)add(alias.scan,alias.modifiers,alias.binding,alias.command,exact);
        }
    }
    std::optional<Binding> pressed(SDL_Scancode scan,SDL_Keymod mod)const{
        if(scan<=SDL_SCANCODE_UNKNOWN || scan>=SDL_SCANCODE_COUNT)return {};
        int i=pressed_[keyModifiers(mod)][scan].binding;return i<0?std::nullopt:std::optional(Binding(i));
    }
    TrekKey command(SDL_Scancode scan,SDL_Keymod mod)const{
        return scan<=SDL_SCANCODE_UNKNOWN || scan>=SDL_SCANCODE_COUNT?TK_NoKeyMapping:pressed_[keyModifiers(mod)][scan].command;
    }
    std::optional<Binding> keyDown(SDL_Scancode scan,SDL_Keymod mod){
        if(!focused_)return {};
        if(scan>SDL_SCANCODE_UNKNOWN && scan<SDL_SCANCODE_COUNT)keys_[scan]=true;
        return pressed(scan,mod);
    }
    void keyUp(SDL_Scancode scan){if(scan>SDL_SCANCODE_UNKNOWN && scan<SDL_SCANCODE_COUNT)keys_[scan]=false;}
    void setFocus(bool focused){if(!focused)keys_.fill(false);focused_=focused;}
    bool keyIsDown(SDL_Scancode scan)const{return scan>SDL_SCANCODE_UNKNOWN && scan<SDL_SCANCODE_COUNT && keys_[scan];}
    Down sample()const{
        SDL_Keymod mod=SDL_KMOD_NONE;
        if(keys_[SDL_SCANCODE_LSHIFT] || keys_[SDL_SCANCODE_RSHIFT])mod|=SDL_KMOD_SHIFT;
        if(keys_[SDL_SCANCODE_LCTRL] || keys_[SDL_SCANCODE_RCTRL])mod|=SDL_KMOD_CTRL;
        if(keys_[SDL_SCANCODE_LALT] || keys_[SDL_SCANCODE_RALT])mod|=SDL_KMOD_ALT;
        return sample(keys_,mod);
    }
    Down sample(std::span<const bool> keys,SDL_Keymod mod)const{
        Down down;const auto& row=held_[keyModifiers(mod)];
        for(size_t i=1;i<keys.size() && i<row.size();++i)if(keys[i])down|=row[i];return down;
    }
private:
    struct Press {int16_t binding{-1};TrekKey command{TK_NoKeyMapping};};
    bool focused_{true};
    std::array<bool,SDL_SCANCODE_COUNT> keys_{};
    std::array<std::array<Press,SDL_SCANCODE_COUNT>,8> pressed_;
    std::array<std::array<Down,SDL_SCANCODE_COUNT>,8> held_;
};
}
