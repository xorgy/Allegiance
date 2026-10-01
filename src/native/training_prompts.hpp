// Authored lesson text adapted to the current native input configuration.
// Runs only when a lesson posts a message; narration assets remain original.
#pragma once
#include "input_bindings.hpp"
#include "keyboard.hpp"
namespace Native {
inline std::string acceptKeyName(const Settings& settings,const KeyboardBindings* keyboard=nullptr) {
    // Original maps commonly list '[' first and Insert as an alias. Preserve
    // the authored instruction whenever unmodified Insert still accepts.
    if(keyboard && keyboard->command(SDL_SCANCODE_INSERT,SDL_KMOD_NONE)==TK_AcceptCommand)return "Insert";
    return keyName(settings,Binding::AcceptOrder);
}
inline std::string interfacePrompt(std::string_view text,const Settings& settings,const KeyboardBindings* keyboard=nullptr) {
    auto at=text.find("Press [insert]");if(at!=text.npos)at+=6;
    else {at=text.find("Press the [Insert] key");if(at!=text.npos)at+=10;}
    auto key=acceptKeyName(settings,keyboard);
    if(at==text.npos || key=="Insert")return std::string(text);
    auto label=key.find_first_of("[]")==key.npos?"["+key+"]":"'"+key+"'";
    return std::string(text.substr(0,at))+label+std::string(text.substr(at+8));
}
inline std::string trainingPrompt(std::string_view text,const Settings& settings,const KeyboardBindings* keyboard=nullptr) {
    auto key=[&](Binding binding)->std::string {
        if(binding==Binding::Fire)return "Left mouse button";
        if(binding==Binding::Missile)return "Right mouse button";
        if(binding==Binding::AcceptOrder)return acceptKeyName(settings,keyboard);
        auto scan=settings.keys[size_t(binding)];
        if(scan!=SDL_SCANCODE_UNKNOWN)return keyName(settings,binding);
        return std::string("the configured ")+bindingNames[size_t(binding)]+" control";
    };
    auto has=[&](std::string_view part){return text.find(part)!=text.npos;};
    if(has("SPACEBAR") || has("SPACE key")) {
        auto at=text.find(" when ");
        if(at!=text.npos)return "Press "+key(Binding::Boost)+std::string(text.substr(at));
        if(has("fire the booster"))return "Hold "+key(Binding::Boost)+" to fire the booster.";
    }
    if(has("button #3") && has("booster"))return "Hold "+key(Binding::Boost)+" or your configured booster button to fire the booster.";
    if(has("BACKSLASH"))return "Press "+key(Binding::ThrottleFull)+" or move your joystick throttle to maximum for full thrust.";
    if(has("center of your display"))return "Press "+key(Binding::TargetCenter)+" to target the objects in the center of your display.";
    if(has("main weapon"))return "Fire your main weapon with "+key(Binding::Fire)+" or your configured trigger.";
    if(has("secondary weapon") || has("#2 button"))return "Fire your secondary weapon with "+key(Binding::Missile)+" or your configured missile button.";
    if(has("Target an enemy using"))return "Target an enemy with "+key(Binding::TargetEnemy)+". Target a friendly with "+key(Binding::TargetFriendly)+".";
    if(has("Target an enemy miner"))return "Target an enemy miner with "+key(Binding::TargetEnemy)+", and destroy it if you can.";
    if(has("target the outpost"))return "Press "+key(Binding::TargetFriendlyBase)+" to target the outpost.";
    if(has("activate your cloak"))return "Press "+key(Binding::Cloak)+" to activate your cloak.";
    if(has("accept the command"))return key(Binding::AcceptOrder)=="Insert"?std::string(text):"Press "+key(Binding::AcceptOrder)+" to accept the command.";
    if(has("[insert]"))return interfacePrompt(text,settings,keyboard);
    if(has("Click the hangar door"))return "Choose Launch in the hangar to return to space. Select a scout to mount the Nanite Repair System.";
    if(has("F4 toggles"))return key(Binding::PaneInventory)+" toggles the inventory. You will want it to be open.";
    if(has("Press F1"))return "Hint: Press "+key(Binding::Help)+" to see an overview of your ship's controls.";
    if(has("Ctrl+1"))return "Swap the Nanite Repair System into slot 1 with "+key(Binding::SwapWeapon1)+".";
    return std::string(text);
}
}
