#pragma once
#include "input_bindings.hpp"
#include "../ClientCore/CommandDescriptions.h"
#include <algorithm>

namespace Native {
inline TrekKey mappingCommand(Binding binding,TrekKey command=TK_NoKeyMapping){return command==TK_NoKeyMapping?trekKey(binding):command;}
inline void normalizeInputKeyModifiers(Settings& settings){
    // CommandMap::FillModifierMap / WriteKeyboardMapping: a plain key uses
    // Any only if no modified mapping occupies that physical key.
    std::array<bool,SDL_SCANCODE_COUNT> chords{};
    auto mark=[&](SDL_Scancode scan,uint8_t modifiers){if(scan>SDL_SCANCODE_UNKNOWN && scan<SDL_SCANCODE_COUNT && (modifiers&7))chords[scan]=true;};
    for(size_t i=0;i<settings.keys.size();++i)mark(settings.keys[i],settings.modifiers[i]);for(auto& a:settings.keyAliases)mark(a.scan,a.modifiers);
    auto normalize=[&](SDL_Scancode scan,uint8_t& modifiers){if(scan>SDL_SCANCODE_UNKNOWN && scan<SDL_SCANCODE_COUNT && !(modifiers&7))modifiers=chords[scan]?0:KeyAny;};
    for(size_t i=0;i<settings.keys.size();++i)normalize(settings.keys[i],settings.modifiers[i]);for(auto& a:settings.keyAliases)normalize(a.scan,a.modifiers);
}
inline void copyInputMappings(Settings& to,const Settings& from){
    to.keys=from.keys;to.modifiers=from.modifiers;to.keyCommands=from.keyCommands;to.keyAliases=from.keyAliases;
    to.mouseButtons=from.mouseButtons;to.mouseAliases=from.mouseAliases;
    to.controller.mapped=from.controller.mapped;to.controller.mapDevices=from.controller.mapDevices;
    to.controller.mapAxes=from.controller.mapAxes;to.controller.mapButtons=from.controller.mapButtons;
    to.controller.axes=from.controller.axes;to.controller.invert=from.controller.invert;to.controller.hat=from.controller.hat;to.controller.buttons=from.controller.buttons;
}
inline void deleteInputMappings(Settings& settings,TrekKey command){
    for(size_t i=0;i<settings.keys.size();++i)if(mappingCommand(Binding(i),settings.keyCommands[i])==command){settings.keys[i]=SDL_SCANCODE_UNKNOWN;settings.keyCommands[i]=TK_NoKeyMapping;}
    std::erase_if(settings.keyAliases,[&](auto& a){return mappingCommand(a.binding,a.command)==command;});
    for(auto& b:settings.mouseButtons)if(b>=0 && trekKey(Binding(b))==command)b=-1;
    std::erase_if(settings.mouseAliases,[&](auto& a){return trekKey(a.binding)==command;});
    std::erase_if(settings.controller.mapButtons,[&](auto& a){return trekKey(a.binding)==command;});
    for(auto& b:settings.controller.buttons)if(b>=0 && trekKey(Binding(b))==command)b=-1;
}
inline const char* commandDescription(TrekKey command){for(auto& entry:Allegiance::CommandDescriptions)if(entry.m_tk==command)return entry.m_pcc;return "";}
inline std::string inputMappingText(const Settings& settings,TrekKey command){
    std::string text;std::vector<std::pair<SDL_Scancode,uint8_t>> seen;
    auto add=[&](std::string value){if(!text.empty())text+=", ";text+=value;};
    auto key=[&](SDL_Scancode scan,uint8_t mods){
        if(scan==SDL_SCANCODE_UNKNOWN)return;
        // DirectInput's generic VK modifiers are expanded to both SDL sides.
        if(scan==SDL_SCANCODE_RSHIFT)scan=SDL_SCANCODE_LSHIFT;else if(scan==SDL_SCANCODE_RCTRL)scan=SDL_SCANCODE_LCTRL;else if(scan==SDL_SCANCODE_RALT)scan=SDL_SCANCODE_LALT;
        mods&=7;auto pair=std::pair{scan,mods};if(std::find(seen.begin(),seen.end(),pair)!=seen.end())return;seen.push_back(pair);
        std::string value;if(mods&KeyShift)value+="Shift+";if(mods&KeyControl)value+="Control+";if(mods&KeyAlt)value+="Alt+";value+=SDL_GetScancodeName(scan);add(std::move(value));
    };
    // New mappings are inserted at the front, as in CommandMap::AddKeyMapping.
    for(auto& a:settings.keyAliases)if(mappingCommand(a.binding,a.command)==command)key(a.scan,a.modifiers);
    for(size_t i=0;i<settings.keys.size();++i)if(mappingCommand(Binding(i),settings.keyCommands[i])==command)key(settings.keys[i],settings.modifiers[i]);
    auto mouse=[&](int b){add(b==8?"Wheel Down":b==9?"Wheel Up":"Mouse Btn "+std::to_string(b+1));};
    for(size_t b=0;b<settings.mouseButtons.size();++b)if(settings.mouseButtons[b]>=0 && trekKey(Binding(settings.mouseButtons[b]))==command)mouse(int(b));
    for(auto& a:settings.mouseAliases)if(trekKey(a.binding)==command)mouse(a.button);
    for(auto& a:settings.controller.mapButtons)if(trekKey(a.binding)==command){
        std::string value="Joy "+std::to_string(a.device);
        if(a.button<0 && a.button>=-4){constexpr const char* hats[]={" Hat Forward"," Hat Right"," Hat Backward"," Hat Left"};value+=hats[-a.button-1];}
        else value+=" Btn "+std::to_string(a.button+1);add(std::move(value));
    }
    return text;
}
}
