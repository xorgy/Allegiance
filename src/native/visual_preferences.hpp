#pragma once
#include <array>
namespace Native {
struct VisualPreferences {
    int debris{2}; // independent off/low/medium/high camera-motion debris
    bool environment{true},posters{true},stars{true},strobes{true},trails{true},lensFlare{true},bidirectional{true},centerHud{true},targetHud{true},messages{true};
    bool operator==(const VisualPreferences& other)const;
    bool operator!=(const VisualPreferences& other)const{return !(*this==other);}
};
inline constexpr std::array<bool VisualPreferences::*,10> visualFields{&VisualPreferences::environment,&VisualPreferences::posters,&VisualPreferences::stars,&VisualPreferences::strobes,&VisualPreferences::trails,&VisualPreferences::lensFlare,&VisualPreferences::bidirectional,&VisualPreferences::centerHud,&VisualPreferences::targetHud,&VisualPreferences::messages};
inline constexpr std::array<const char*,10> visualNames{"Sector environment","Planets and posters","Stars","Ship lights","Ship trails","Lens flare","Bidirectional lighting","Center instruments","Target instruments","Message history"};
inline constexpr std::array<const char*,10> originalVisualNames{"Environment","Posters","Stars","Strobes","Trails","Lens Flare","BidirectionalLighting","CenterHUD","TargetHUD","ChatHistory"};
inline bool VisualPreferences::operator==(const VisualPreferences& other)const{if(debris!=other.debris)return false;for(auto field:visualFields)if(this->*field!=other.*field)return false;return true;}
}
