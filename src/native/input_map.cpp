#include "input_bindings.hpp"
#include "mdl_text.hpp"
#include <fstream>
#include <cmath>
#include <optional>
#include <limits>
#include <bitset>

namespace Native {
namespace {
SDL_Scancode windowsKey(int vk){
    if(vk>=65 && vk<=90)return SDL_Scancode(SDL_SCANCODE_A+vk-65);
    if(vk>=49 && vk<=57)return SDL_Scancode(SDL_SCANCODE_1+vk-49);
    if(vk==48)return SDL_SCANCODE_0;
    if(vk>=112 && vk<=123)return SDL_Scancode(SDL_SCANCODE_F1+vk-112);
    if(vk>=124 && vk<=135)return SDL_Scancode(SDL_SCANCODE_F13+vk-124);
    if(vk>=97 && vk<=105)return SDL_Scancode(SDL_SCANCODE_KP_1+vk-97);
    switch(vk){
    case 8:return SDL_SCANCODE_BACKSPACE;case 9:return SDL_SCANCODE_TAB;case 13:return SDL_SCANCODE_RETURN;
    case 16:return SDL_SCANCODE_LSHIFT;case 17:return SDL_SCANCODE_LCTRL;case 18:return SDL_SCANCODE_LALT;
    case 19:return SDL_SCANCODE_PAUSE;case 20:return SDL_SCANCODE_CAPSLOCK;case 27:return SDL_SCANCODE_ESCAPE;
    case 32:return SDL_SCANCODE_SPACE;case 33:return SDL_SCANCODE_PAGEUP;case 34:return SDL_SCANCODE_PAGEDOWN;
    case 35:return SDL_SCANCODE_END;case 36:return SDL_SCANCODE_HOME;case 37:return SDL_SCANCODE_LEFT;
    case 38:return SDL_SCANCODE_UP;case 39:return SDL_SCANCODE_RIGHT;case 40:return SDL_SCANCODE_DOWN;
    case 44:return SDL_SCANCODE_PRINTSCREEN;case 45:return SDL_SCANCODE_INSERT;case 46:return SDL_SCANCODE_DELETE;
    case 96:return SDL_SCANCODE_KP_0;case 106:return SDL_SCANCODE_KP_MULTIPLY;case 107:return SDL_SCANCODE_KP_PLUS;case 108:return SDL_SCANCODE_KP_ENTER;
    case 109:return SDL_SCANCODE_KP_MINUS;case 110:return SDL_SCANCODE_KP_PERIOD;case 111:return SDL_SCANCODE_KP_DIVIDE;
    case 144:return SDL_SCANCODE_NUMLOCKCLEAR;case 145:return SDL_SCANCODE_SCROLLLOCK;
    case 160:return SDL_SCANCODE_LSHIFT;case 161:return SDL_SCANCODE_RSHIFT;case 162:return SDL_SCANCODE_LCTRL;
    case 163:return SDL_SCANCODE_RCTRL;case 164:return SDL_SCANCODE_LALT;case 165:return SDL_SCANCODE_RALT;
    case 186:return SDL_SCANCODE_SEMICOLON;case 187:return SDL_SCANCODE_EQUALS;case 188:return SDL_SCANCODE_COMMA;
    case 189:return SDL_SCANCODE_MINUS;case 190:return SDL_SCANCODE_PERIOD;case 191:return SDL_SCANCODE_SLASH;
    case 192:return SDL_SCANCODE_GRAVE;case 219:return SDL_SCANCODE_LEFTBRACKET;case 220:return SDL_SCANCODE_BACKSLASH;
    case 221:return SDL_SCANCODE_RIGHTBRACKET;case 222:return SDL_SCANCODE_APOSTROPHE;case 226:return SDL_SCANCODE_NONUSBACKSLASH;
    default:return SDL_SCANCODE_UNKNOWN;
    }
}

}
size_t Settings::importInputMap(const std::filesystem::path& path){
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file || file.tellg()<0 || file.tellg()>1024*1024)throw std::runtime_error("Original keyboard map is missing or exceeds its size limit.");
    file.seekg(0);std::string text{std::istreambuf_iterator<char>(file),{}};Mdl::Parser parser(text);
    std::optional<Mdl::Expr> map,mouse,buttons,values,oldValues;
    while(!parser.end()){
        auto symbol=parser.word();if(symbol=="use"){parser.expression();parser.need(';');continue;}
        parser.need('=');auto value=parser.expression();parser.need(';');if(symbol=="keyCommandMap")map=std::move(value);else if(symbol=="mouseButtonCommandMap")mouse=std::move(value);else if(symbol=="buttonCommandMap")buttons=std::move(value);else if(symbol=="numericValuesNew")values=std::move(value);else if(symbol=="numericValues")oldValues=std::move(value);
    }
    if(!map || map->kind!=Mdl::Expr::List || map->args.size()>512)throw std::runtime_error("Original keyboard map has no valid keyCommandMap.");
    Settings next=*this;next.keyAliases.clear();
    for(size_t i=0;i<next.keys.size();++i)if(trekKey(Binding(i))>=0){next.keys[i]=SDL_SCANCODE_UNKNOWN;next.modifiers[i]=KeyAny;next.keyCommands[i]=TK_NoKeyMapping;}
    std::array<std::bitset<256>,8> occupied;
    std::array<std::bitset<SDL_SCANCODE_COUNT>,8> reserved;
    size_t count=0;auto number=[](const Mdl::Expr& e){if(e.kind!=Mdl::Expr::Number || !std::isfinite(e.number) || e.number!=std::floor(e.number) || e.number<0 || e.number>65535)throw std::runtime_error("Expected integer in saved original keyboard map.");return int(e.number);};
    for(auto& row:map->args){
        if(row.kind!=Mdl::Expr::List || row.args.size()!=3)throw std::runtime_error("Invalid original keyboard mapping row.");
        int vk=number(row.args[0]),mod=number(row.args[1]),command=number(row.args[2]);auto binding=bindingFor(command);
        if(mod>KeyAny || vk>=256 || command>=TK_Max)throw std::runtime_error("Invalid original keyboard key, modifier or command.");
        if(occupied[mod&7].test(vk))throw std::runtime_error("Duplicate original keyboard press slot.");occupied[mod&7].set(vk);
        auto scan=windowsKey(vk);
        if(scan!=SDL_SCANCODE_UNKNOWN)reserved[mod&7].set(scan);
        if(!binding || scan==SDL_SCANCODE_UNKNOWN)continue;auto i=size_t(*binding);
        if(next.keys[i]==SDL_SCANCODE_UNKNOWN){next.keys[i]=scan;next.modifiers[i]=uint8_t(mod);next.keyCommands[i]=TrekKey(command);}
        else next.keyAliases.push_back({scan,uint8_t(mod),*binding,TrekKey(command)});
        if(vk>=16 && vk<=18)next.keyAliases.push_back({vk==16?SDL_SCANCODE_RSHIFT:vk==17?SDL_SCANCODE_RCTRL:SDL_SCANCODE_RALT,uint8_t(mod),*binding,TrekKey(command)});
        ++count;
    }
    // Native-only convenience defaults must not replace authored commands.
    // For example Equals is original throttle-up, not native HUD scaling;
    // H is miner targeting, not a HUD toggle, and F3 is command view, not FMV.
    for(size_t i=0;i<next.keys.size();++i)if(trekKey(Binding(i))<0 && next.keys[i]!=SDL_SCANCODE_UNKNOWN && reserved[next.modifiers[i]&7].test(next.keys[i]))next.keys[i]=SDL_SCANCODE_UNKNOWN;
    if(mouse){
        if(mouse->kind!=Mdl::Expr::List || mouse->args.size()>512)throw std::runtime_error("Invalid original mouse button map.");
        next.mouseButtons.fill(-1);next.mouseAliases.clear();
        for(auto& row:mouse->args){
            if(row.kind!=Mdl::Expr::List || row.args.size()!=2)throw std::runtime_error("Invalid original mouse mapping row.");
            int button=number(row.args[0]);auto binding=bindingFor(number(row.args[1]));
            if(button>=10)throw std::runtime_error("Invalid original mouse button index.");
            if(!binding)continue;
            if(next.mouseButtons[button]<0)next.mouseButtons[button]=int(*binding);else next.mouseAliases.push_back({button,*binding});
            ++count;
        }
    }
    auto signedNumber=[](const Mdl::Expr& e){if(e.kind!=Mdl::Expr::Number || !std::isfinite(e.number) || e.number!=std::floor(e.number) || e.number<std::numeric_limits<int>::min() || e.number>std::numeric_limits<int>::max())throw std::runtime_error("Expected integer in saved original controller map.");return int(e.number);};
    if(buttons){
        if(buttons->kind!=Mdl::Expr::List || buttons->args.size()>512)throw std::runtime_error("Invalid original controller button map.");
        next.controller.mapButtons.clear();next.controller.mapped=true;
        for(auto& row:buttons->args){
            if(row.kind!=Mdl::Expr::List || row.args.size()!=3)throw std::runtime_error("Invalid original controller button row.");
            int device=signedNumber(row.args[0]),button=signedNumber(row.args[1]);auto binding=bindingFor(number(row.args[2]));
            if(device>=16 || button< -4 || button>=64)throw std::runtime_error("Original controller button exceeds native device/button limits.");
            if(device<0 || !binding)continue;
            next.controller.mapButtons.push_back({device,button,*binding});++count;
        }
    }
    bool old=!values && oldValues.has_value();if(old)values=std::move(oldValues);
    if(values){
        if(values->kind!=Mdl::Expr::List || values->args.size()>6)throw std::runtime_error("Invalid original controller axis map.");
        next.controller.mapAxes={};next.controller.mapped=true;
        for(size_t i=0;i<values->args.size();++i){
            auto& row=values->args[i];if(row.kind!=Mdl::Expr::List || row.args.size()!=2)throw std::runtime_error("Invalid original controller axis row.");
            int device=signedNumber(row.args[0]),value=signedNumber(row.args[1]);if(old){if(value==2)value=3;else if(value==3)value=2;}
            // The installed map has a debug-fill negative sixth row. Like
            // TrekInput's absent stream/value, every negative source is inert.
            if(device<0 || value<0)continue;
            if(device>=16 || value>=64)throw std::runtime_error("Original controller axis exceeds native device/value limits.");
            // DirectInput labels X/Y/Z/Rz/POV as 0..4. This is a reviewable
            // initial translation; raw SDL axis order varies with hardware.
            next.controller.mapAxes[i]={device,value==4?0:value,value==4,i<4,value};++count;
        }
    }
    next.normalize();*this=std::move(next);return count;
}
}
