#include "settings.hpp"
#include "effect_preferences.hpp"
#include "../Lib/nlohmann/json/include/json.hpp"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace Native {
size_t Settings::importOriginal(const std::filesystem::path& path) {
    // The repository's original configuration parser uses C++17. This small
    // startup-only adapter is compiled separately; native frame code stays C++20.
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file || file.tellg()<0 || file.tellg()>1024*1024)throw std::runtime_error("Original configuration is missing or exceeds its size limit.");
    file.seekg(0);nlohmann::json config;
    try{config=nlohmann::json::parse(file);}catch(...){throw std::runtime_error("Original configuration could not be parsed.");}
    if(!config.is_object())throw std::runtime_error("Original configuration must be a JSON object.");
    Settings next=*this;size_t count=0;
    auto boolean=[&](const char* key,bool& target) {
        auto it=config.find(key);if(it==config.end())return;
        if(it->is_boolean()){target=it->get<bool>();++count;}
        else if(it->is_number_integer()){target=it->get<int64_t>()!=0;++count;}
    };
    auto numeric=[&](const char* key,auto use) {
        auto it=config.find(key);if(it==config.end())return;
        double value;
        try {
            if(it->is_number())value=it->get<double>();
            else if(it->is_string()){auto s=it->get<std::string>();size_t end;value=std::stod(s,&end);if(end!=s.size())return;}
            else return;
        }catch(...){return;}
        if(std::isfinite(value)){use(value);++count;}
    };
    for(size_t i=0;i<visualFields.size();++i)boolean(originalVisualNames[i],next.visual.*visualFields[i]);
    // Preserve the effective startup behavior of WinTrek, whose first two
    // false preferences toggle constructor defaults from false to true.
    auto invertedBoolean=[&](const char* key,bool& target){size_t before=count;bool value{};boolean(key,value);if(count!=before)target=!value;};
    invertedBoolean("FilterChatsToAll",next.chat.filterAll);
    invertedBoolean("FilterQuickComms",next.chat.filterQuick);
    boolean("FilterUnknownChats",next.chat.filterUnknown);boolean("CensorChats",next.chat.censor);
    numeric("FilterLobbyChats",[&](double value){next.chat.filterLobby=value>=1 && value<=3?int(value):3;});
    numeric("ChatLines",[&](double value){next.chatLines=int(std::clamp(value,1.,10.));});
    numeric("ChatSize",[&](double value){next.chatSize=value<1?3:value>3?1:int(value);});
    auto wing=[&](double value){next.defaultWing=value>=-1 && value<10 && std::floor(value)==value?int(value):0;};
    numeric("WingAssignment",wing);numeric("Ui.DefaultWing",wing);
    numeric("WheelDelay",[&](double value){next.throttleDelay=int(std::clamp(value,0.,4.));});
    boolean("VirtualJoystick",next.virtualJoystick);boolean("ShowJoystickIndicator",next.joystickIndicator);
    numeric("MouseAcceleration",[&](double value){next.mouseAcceleration=int(std::clamp(value,0.,2.));});
    boolean("ShowGrid",next.showGrid);boolean("Graphics.Fullscreen",next.fullscreen);boolean("FlipY",next.invertY);
    boolean("EnableFeedback",next.controller.feedback);boolean("LinearControlResponse",next.controller.linear);
    boolean("FFAutoCenter",next.controller.autoCenter);
    numeric("Gamma",[&](double value){next.gamma=float(value);});
    auto aa=[&](double value){next.antialiasing=value>=16?16:value>=8?8:value>=4?4:value>0?2:0;};
    auto antialias=[&](const char* key){auto it=config.find(key);if(it!=config.end() && it->is_boolean()){aa(it->get<bool>()?2:0);++count;}else numeric(key,aa);};
    antialias("UseAntialiasing");antialias("Graphics.UseAntialiasing");
    auto textureSize=[&](double value){next.maxTextureSize=int(std::clamp(value,0.,3.));};
    numeric("MaxTextureSize",textureSize);numeric("Graphics.MaxTextureSizeLevel",textureSize);
    boolean("Ui.ShowStartupCreditsMovie",next.startupCredits);boolean("Ui.ShowStartupIntroMovie",next.startupIntro);
    auto beforeTrained=count;bool trained{};boolean("HasTrained",trained);
    if(count!=beforeTrained)next.showStartupTrainingSuggestion=!trained;
    boolean("Ui.ShowStartupTrainingSuggestion",next.showStartupTrainingSuggestion);
    numeric("MouseSensitivity",[&](double value){next.sensitivity=float(value);});
    numeric("Debris",[&](double value){next.visual.debris=value==.8?3:value==1?2:value==1.5?1:0;});
    numeric("StyleHUD",[&](double value){next.hudStyle=int(std::clamp(value,0.,5.));});
    numeric("SmokeEffects",[&](double value){
        for(size_t i=0;i<particlePreferences.size();++i)if(value==particlePreferences[i].original){next.particles=int(i);return;}
        next.particles=2; // WinTrek's remaining nonzero values use the high label.
    });
    numeric("DeadZone",[&](double value){next.controller.deadZone=value==0 || value==31 || value==4 || value==1?.04f:value==10 || value==5?.1f:.3f;});
    numeric("SFXGain",[&](double value){next.effectsGain=-float(value);});
    numeric("VoiceOverGain",[&](double value){next.voiceGain=-float(value);});
    numeric("FFGain",[&](double value){next.controller.feedbackGain=float(value/10000.);});
    numeric("RadarLOD",[&](double value){next.cockpitRadar=next.commandRadar=int(std::clamp(value,0.,3.));});
    auto pilot=config.find("Online.CharacterName");if(pilot!=config.end() && pilot->is_string()){next.pilot=pilot->get<std::string>();++count;}
    next.normalize();*this=std::move(next);return count;
}
}
