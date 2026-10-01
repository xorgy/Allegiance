#include "settings.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace Native {
std::filesystem::path Settings::defaultPath() {
#ifdef _WIN32
    if(auto* p=std::getenv("LOCALAPPDATA");p && *p)return std::filesystem::path(p)/"FreeAllegiance/native.conf";
#elif defined(__APPLE__)
    if(auto* p=std::getenv("HOME");p && *p)return std::filesystem::path(p)/"Library/Application Support/FreeAllegiance/native.conf";
#endif
    if(auto* p=std::getenv("XDG_CONFIG_HOME");p && *p)return std::filesystem::path(p)/"freeallegiance/native.conf";
    if(auto* p=std::getenv("HOME");p && *p)return std::filesystem::path(p)/".config/freeallegiance/native.conf";
    return "allegiance-native.conf";
}
void Settings::normalize() {
    classicUiScale=std::clamp(classicUiScale,0,8);
    hudStyle=std::clamp(hudStyle,0,5);
    auto finite=[](float x,float fallback,float lo,float hi){return std::isfinite(x)?std::clamp(x,lo,hi):fallback;};
    uiScale=finite(uiScale,1,.5f,2.5f);volume=finite(volume,.7f,0,1);sensitivity=finite(sensitivity,1,.1f,3);
    fieldOfView=finite(fieldOfView,65,45,100);particles=std::clamp(particles,0,4);visual.debris=std::clamp(visual.debris,0,3);throttleDelay=std::clamp(throttleDelay,0,4);
    mouseAcceleration=std::clamp(mouseAcceleration,0,2);
    chatLines=std::clamp(chatLines,1,10);chatSize=std::clamp(chatSize,1,3);
    if(defaultWing< -1 || defaultWing>=10)defaultWing=0;
    if(chat.filterLobby<1 || chat.filterLobby>3)chat.filterLobby=3;
    cockpitRadar=std::clamp(cockpitRadar,0,3);commandRadar=std::clamp(commandRadar,0,3);
    std::erase_if(pilot,[](unsigned char c){return c<32 || c==127;});
    if(pilot.size()>23){size_t n=23;while(n && (uint8_t(pilot[n])&0xc0)==0x80)--n;pilot.resize(n);}
    if(pilot.empty())pilot="Cadet";
    controller.deadZone=finite(controller.deadZone,.1f,0.f,.95f);
    controller.feedbackGain=finite(controller.feedbackGain,1,0,1);
    effectsGain=finite(effectsGain,-8,-60,0);voiceGain=finite(voiceGain,-13,-60,0);
    gamma=finite(gamma,1.13f,1,2);
    if(antialiasing!=0 && antialiasing!=2 && antialiasing!=4 && antialiasing!=8 && antialiasing!=16)antialiasing=0;
    maxTextureSize=std::clamp(maxTextureSize,0,3);
    for(auto& axis:controller.axes)axis=std::clamp(axis,-2,63);
    controller.hat=std::clamp(controller.hat,-1,15);
    for(auto& button:controller.buttons)if(button< -2 || button>=int(Binding::Count))button=-1;
    for(auto& route:controller.mapAxes){
        if(route.device< -1 || route.device>=16 || route.index< -1 || route.index>=(route.hat?16:64))route={};
    }
    std::erase_if(controller.mapButtons,[](auto& route){return route.device<0 || route.device>=16 || route.button< -4 || route.button>=64 || int(route.binding)<0 || route.binding>=Binding::Count;});
    if(controller.mapButtons.size()>512)controller.mapButtons.resize(512);
    for(auto& button:mouseButtons)if(button< -1 || button>=int(Binding::Count))button=-1;
    std::erase_if(mouseAliases,[](const MouseAlias& a){return a.button<0 || a.button>=10 || int(a.binding)<0 || int(a.binding)>=int(Binding::Count);});
    if(mouseAliases.size()>512)mouseAliases.resize(512);
    const Settings defaults;
    for(size_t i=0;i<keys.size();++i)if(keys[i]<SDL_SCANCODE_UNKNOWN || keys[i]>=SDL_SCANCODE_COUNT)keys[i]=defaults.keys[i];
    for(auto& mod:modifiers)if(mod>KeyAny)mod=KeyAny;
    for(auto& command:keyCommands)if(command<TK_NoKeyMapping || command>=TK_Max)command=TK_NoKeyMapping;
    for(auto& alias:keyAliases)if(alias.command<TK_NoKeyMapping || alias.command>=TK_Max)alias.command=TK_NoKeyMapping;
    std::erase_if(keyAliases,[](const KeyAlias& a){return a.scan<=SDL_SCANCODE_UNKNOWN || a.scan>=SDL_SCANCODE_COUNT || a.modifiers>KeyAny || int(a.binding)<0 || int(a.binding)>=int(Binding::Count);});
    if(keyAliases.size()>512)keyAliases.resize(512);
}
Settings Settings::load(const std::filesystem::path& path) {
    return load(path,Settings{});
}
Settings Settings::load(const std::filesystem::path& path,const Settings& defaults) {
    Settings s=defaults;std::ifstream file(path);std::string line;
    // A persisted map supplies its own aliases rather than appending the
    // installed default map's aliases to it. Other preferences can be partial.
    bool keyboardMap=false,mouseMap=false;
    while(std::getline(file,line)) {
        std::istringstream in(line);std::string key;in>>key;
        if(key=="classic_ui_scale")in>>s.classicUiScale;
        else if(key=="ui_scale")in>>s.uiScale;else if(key=="volume")in>>s.volume;else if(key=="sensitivity")in>>s.sensitivity;
        else if(key=="fov")in>>s.fieldOfView;else if(key=="fullscreen")in>>s.fullscreen;else if(key=="invert_y")in>>s.invertY;
        else if(key=="timing")in>>s.showTiming;else if(key=="particles")in>>s.particles;else if(key=="pilot")in>>std::quoted(s.pilot);
        else if(key=="startup_movies")in>>s.startupCredits>>s.startupIntro;
        else if(key=="training_suggestion")in>>s.showStartupTrainingSuggestion;
        else if(key=="throttle_delay")in>>s.throttleDelay;
        else if(key=="mouse_mode")in>>s.virtualJoystick>>s.joystickIndicator>>s.mouseAcceleration;
        else if(key=="visual"){size_t i;bool value;if(in>>i>>value && i<visualFields.size())s.visual.*visualFields[i]=value;}
        else if(key=="debris")in>>s.visual.debris;
        else if(key=="hud_style")in>>s.hudStyle;
        else if(key=="chat_filters")in>>s.chat.filterAll>>s.chat.filterQuick>>s.chat.filterUnknown>>s.chat.censor;
        else if(key=="chat_layout")in>>s.chatLines>>s.chatSize;
        else if(key=="default_wing")in>>s.defaultWing;
        else if(key=="filter_lobby")in>>s.chat.filterLobby;
        else if(key=="show_grid")in>>s.showGrid;
        else if(key=="prefer_chase")in>>s.preferChase;
        else if(key=="show_command_grid")in>>s.showCommandGrid;
        else if(key=="radar_detail")in>>s.cockpitRadar>>s.commandRadar;
        else if(key=="controller"){in>>s.controller.enabled>>s.controller.linear>>s.controller.deadZone>>s.controller.hat>>s.controller.triggers>>std::quoted(s.controller.device);}
        else if(key=="controller_feedback")in>>s.controller.feedback;
        else if(key=="feedback_gain")in>>s.controller.feedbackGain;
        else if(key=="feedback_auto_center")in>>s.controller.autoCenter;
        else if(key=="gamma")in>>s.gamma;
        else if(key=="graphics_quality")in>>s.antialiasing>>s.maxTextureSize;
        else if(key=="audio_categories")in>>s.effectsGain>>s.voiceGain;
        else if(key=="controller_axis"){size_t i;int axis;bool invert;if(in>>i>>axis>>invert && i<4){s.controller.axes[i]=axis;s.controller.invert[i]=invert;}}
        else if(key=="controller_button"){size_t i;int binding;if(in>>i>>binding && i<s.controller.buttons.size())s.controller.buttons[i]=binding;}
        else if(key=="controller_mapped"){s.controller.mapButtons.clear();in>>s.controller.mapped;}
        else if(key=="controller_map_device"){size_t i;std::string guid;if(in>>i>>std::quoted(guid) && i<s.controller.mapDevices.size())s.controller.mapDevices[i]=std::move(guid);}
        else if(key=="controller_map_axis"){size_t i;ControllerAxisRoute route;if(in>>i>>route.device>>route.index>>route.hat>>route.invert>>route.originalValue && i<s.controller.mapAxes.size())s.controller.mapAxes[i]=route;}
        else if(key=="controller_map_button"){ControllerButtonRoute route;int binding;if(in>>route.device>>route.button>>binding && s.controller.mapButtons.size()<512){route.binding=Binding(binding);s.controller.mapButtons.push_back(route);}}
        else if(key=="mouse_button"){size_t i;int binding;if(in>>i>>binding && i<s.mouseButtons.size()){if(!mouseMap){s.mouseAliases.clear();mouseMap=true;}s.mouseButtons[i]=binding;}}
        else if(key=="mouse_alias"){int button,binding;if(in>>button>>binding && s.mouseAliases.size()<512){if(!mouseMap){s.mouseAliases.clear();mouseMap=true;}s.mouseAliases.push_back({button,Binding(binding)});}}
        else if(key=="binding"){size_t i;int code,mod,command;if(in>>i>>code && i<s.keys.size()){if(!keyboardMap){s.keyAliases.clear();keyboardMap=true;}s.keys[i]=SDL_Scancode(code);s.keyCommands[i]=TK_NoKeyMapping;s.modifiers[i]=KeyAny;if(in>>mod){s.modifiers[i]=uint8_t(mod);if(in>>command && command>=TK_NoKeyMapping && command<TK_Max)s.keyCommands[i]=TrekKey(command);}}}
        else if(key=="key_alias"){int scan,mod,binding,command=TK_NoKeyMapping;if(in>>scan>>mod>>binding && s.keyAliases.size()<512){if(!keyboardMap){s.keyAliases.clear();keyboardMap=true;}in>>command;s.keyAliases.push_back({SDL_Scancode(scan),uint8_t(mod),Binding(binding),command>=TK_NoKeyMapping && command<TK_Max?TrekKey(command):TK_NoKeyMapping});}}
    }
    s.normalize();return s;
}
void Settings::save(const std::filesystem::path& path) const {
    if(path.empty())return;
    if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
    auto temporary=path;temporary+=".tmp";
    {
        std::ofstream out(temporary,std::ios::trunc);out<<"# FreeAllegiance native settings\n"
            <<"pilot "<<std::quoted(pilot)<<"\nclassic_ui_scale "<<classicUiScale<<"\nui_scale "<<uiScale<<"\nvolume "<<volume<<"\nsensitivity "<<sensitivity
            <<"\nfov "<<fieldOfView<<"\nfullscreen "<<fullscreen<<"\ninvert_y "<<invertY<<"\ntiming "<<showTiming<<"\nparticles "<<particles<<'\n';
        out<<"mouse_mode "<<virtualJoystick<<' '<<joystickIndicator<<' '<<mouseAcceleration<<'\n';
        out<<"startup_movies "<<startupCredits<<' '<<startupIntro<<'\n';
        out<<"training_suggestion "<<showStartupTrainingSuggestion<<'\n';
        out<<"throttle_delay "<<throttleDelay<<'\n';
        for(size_t i=0;i<visualFields.size();++i)out<<"visual "<<i<<' '<<(visual.*visualFields[i])<<'\n';
        out<<"debris "<<visual.debris<<'\n';
        out<<"hud_style "<<hudStyle<<'\n';
        out<<"chat_filters "<<chat.filterAll<<' '<<chat.filterQuick<<' '<<chat.filterUnknown<<' '<<chat.censor<<'\n';
        out<<"chat_layout "<<chatLines<<' '<<chatSize<<'\n'<<"filter_lobby "<<chat.filterLobby<<'\n';
        out<<"default_wing "<<defaultWing<<'\n';
        out<<"prefer_chase "<<preferChase<<'\n';
        out<<"show_grid "<<showGrid<<'\n';
        out<<"show_command_grid "<<showCommandGrid<<'\n';
        out<<"radar_detail "<<cockpitRadar<<' '<<commandRadar<<'\n';
        out<<"controller "<<controller.enabled<<' '<<controller.linear<<' '<<controller.deadZone<<' '<<controller.hat<<' '<<controller.triggers<<' '<<std::quoted(controller.device)<<'\n';
        out<<"controller_feedback "<<controller.feedback<<'\n';
        out<<"feedback_gain "<<controller.feedbackGain<<"\naudio_categories "<<effectsGain<<' '<<voiceGain<<'\n';
        out<<"feedback_auto_center "<<controller.autoCenter<<"\ngamma "<<gamma<<'\n';
        out<<"graphics_quality "<<antialiasing<<' '<<maxTextureSize<<'\n';
        for(size_t i=0;i<controller.axes.size();++i)out<<"controller_axis "<<i<<' '<<controller.axes[i]<<' '<<controller.invert[i]<<'\n';
        for(size_t i=0;i<controller.buttons.size();++i)out<<"controller_button "<<i<<' '<<controller.buttons[i]<<'\n';
        out<<"controller_mapped "<<controller.mapped<<'\n';
        for(size_t i=0;i<controller.mapDevices.size();++i)if(!controller.mapDevices[i].empty())out<<"controller_map_device "<<i<<' '<<std::quoted(controller.mapDevices[i])<<'\n';
        for(size_t i=0;i<controller.mapAxes.size();++i){auto& route=controller.mapAxes[i];out<<"controller_map_axis "<<i<<' '<<route.device<<' '<<route.index<<' '<<route.hat<<' '<<route.invert<<' '<<route.originalValue<<'\n';}
        for(auto& route:controller.mapButtons)out<<"controller_map_button "<<route.device<<' '<<route.button<<' '<<int(route.binding)<<'\n';
        for(size_t i=0;i<mouseButtons.size();++i)out<<"mouse_button "<<i<<' '<<mouseButtons[i]<<'\n';
        for(auto& a:mouseAliases)out<<"mouse_alias "<<a.button<<' '<<int(a.binding)<<'\n';
        for(size_t i=0;i<keys.size();++i)out<<"binding "<<i<<' '<<int(keys[i])<<' '<<int(modifiers[i])<<' '<<keyCommands[i]<<'\n';
        for(auto& a:keyAliases)out<<"key_alias "<<int(a.scan)<<' '<<int(a.modifiers)<<' '<<int(a.binding)<<' '<<a.command<<'\n';
        out.flush();if(!out)throw std::runtime_error("Could not save settings to "+path.string());
    }
    std::filesystem::rename(temporary,path);
}
}
