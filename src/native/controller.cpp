#include "controller.hpp"
#include "../ClientCore/Joystick.h"
#include "../ClientCore/JoystickHat.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace Native {
namespace {
float povValue(Uint8 position){
    constexpr std::array<float,16> values{-2,0,.5f,.25f,1,-2,.75f,-2,-.5f,-.25f,-2,-2,-.75f,-2,-2,-2};
    return position<values.size()?values[position]:-2.f;
}
}

Controller::Controller(const ControllerSettings& settings,const std::filesystem::path& artwork){
    constexpr const char* names[]={"fire.ffe","bounce.ffe","explode.ffe"};
    for(int i=0;i<3;++i){if(!artwork.empty())try{presets_[i]=loadForceFile(artwork/names[i]);}catch(const std::exception& e){fprintf(stderr,"Haptics: %s; using original fallback\n",e.what());}
        if(presets_[i].empty())presets_[i].push_back(defaultForceEffect(i));}
    configure(settings);
}
Controller::~Controller(){close();}
void Controller::close(){
    stopFeedback();if(haptic_){SDL_CloseHaptic(haptic_);haptic_=nullptr;}for(auto& bank:forces_)bank.clear();
    if(pad_)SDL_CloseGamepad(pad_);
    else if(joystick_ && std::find(mappedDevices_.begin(),mappedDevices_.end(),joystick_)==mappedDevices_.end())SDL_CloseJoystick(joystick_);
    for(auto*& device:mappedDevices_){if(device)SDL_CloseJoystick(device);device=nullptr;}
    mappedAxes_={};mappedButtons_.clear();for(auto& hat:mappedHat_)hat.reset();
    pad_=nullptr;joystick_=nullptr;id_=0;axisCount_=buttonCount_=hatCount_=0;name_.clear();reset();
}
void Controller::reset(){stopFeedback();initialized_=joyThrottle_=false;previous_.reset();suppressed_.reset();state_={};}
void Controller::configure(const ControllerSettings& settings){if(settings.mapped!=settings_.mapped)close();settings_=settings;inverseDeadZone_=1-settings_.deadZone;reset();refresh();}
int Controller::defaultButton(bool gamepad,int button) {
    if(gamepad) {
        switch(button) {
        case SDL_GAMEPAD_BUTTON_SOUTH:return int(Binding::Target);
        case SDL_GAMEPAD_BUTTON_EAST:return int(Binding::Coast);
        case SDL_GAMEPAD_BUTTON_WEST:return int(Binding::NextWeapon);
        case SDL_GAMEPAD_BUTTON_NORTH:return int(Binding::Boost);
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:return int(Binding::RollModifier);
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:return int(Binding::Chaff);
        case SDL_GAMEPAD_BUTTON_BACK:return int(Binding::PilotOrders);
        case SDL_GAMEPAD_BUTTON_START:return int(Binding::Pause);
        case SDL_GAMEPAD_BUTTON_LEFT_STICK:return int(Binding::Autopilot);
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK:return int(Binding::ToggleView);
        default:return -1;
        }
    }
    constexpr Binding defaults[]={Binding::Fire,Binding::Missile,Binding::Boost,Binding::NextWeapon,Binding::Coast,Binding::Target,Binding::Autopilot,Binding::Chaff};
    return button>=0 && button<int(std::size(defaults))?int(defaults[button]):-1;
}
void Controller::refresh() {
    devices_.clear();int count=0;auto* ids=SDL_GetJoysticks(&count);SDL_JoystickID selected=0;
    std::erase_if(deviceKeys_,[&](auto& key){return count==0 || std::find(ids,ids+count,key.first)==ids+count;});
    for(int i=0;i<count;++i) {
        char guid[33];SDL_GUIDToString(SDL_GetJoystickGUIDForID(ids[i]),guid,sizeof(guid));
        auto known=std::find_if(deviceKeys_.begin(),deviceKeys_.end(),[&](auto& key){return key.first==ids[i];});
        std::string key;
        if(known!=deviceKeys_.end())key=known->second;
        else {key=guid;int occurrence=1;
            while(std::any_of(deviceKeys_.begin(),deviceKeys_.end(),[&](auto& device){return device.second==key;}))key=std::string(guid)+"#"+std::to_string(++occurrence);
            deviceKeys_.push_back({ids[i],key});
        }
        auto* name=SDL_GetJoystickNameForID(ids[i]);std::string label=name?name:"Joystick";if(auto suffix=key.find('#');suffix!=key.npos)label+=" / device "+key.substr(suffix+1);devices_.push_back({key,std::move(label),SDL_IsGamepad(ids[i])});
        if(!selected && settings_.enabled && (settings_.device.empty() || settings_.device==key))selected=ids[i];
    }
    if(settings_.mapped) {
        close();
        std::bitset<16> used;used.set(0);
        for(auto& route:settings_.mapAxes)if(route.device>=0 && route.device<16 && route.index>=0)used.set(route.device);
        for(auto& route:settings_.mapButtons)if(route.device>=0 && route.device<16 && route.button>=0)used.set(route.device);
        if(settings_.enabled)for(size_t i=0;i<mappedDevices_.size();++i) {
            if(!used[i])continue;auto& key=settings_.mapDevices[i];int ordinal=int(i);
            if(!key.empty()){ordinal=-1;for(int j=0;j<count;++j)if(devices_[j].guid==key){ordinal=j;break;}}
            if(ordinal>=0 && ordinal<count)mappedDevices_[i]=SDL_OpenJoystick(ids[ordinal]);
        }
        for(auto* device:mappedDevices_)if(device){joystick_=device;break;}
        if(joystick_){id_=SDL_GetJoystickID(joystick_);name_=SDL_GetJoystickName(joystick_);axisCount_=SDL_GetNumJoystickAxes(joystick_);buttonCount_=SDL_GetNumJoystickButtons(joystick_);hatCount_=SDL_GetNumJoystickHats(joystick_);}
        compileMap();
    } else if(!(selected==id_ && joystick_ && SDL_JoystickConnected(joystick_))) {
        close();if(selected) {
            if(SDL_IsGamepad(selected)){pad_=SDL_OpenGamepad(selected);if(pad_)joystick_=SDL_GetGamepadJoystick(pad_);}
            if(!joystick_)joystick_=SDL_OpenJoystick(selected);
            if(joystick_){id_=selected;name_=SDL_GetJoystickName(joystick_);axisCount_=pad_?SDL_GAMEPAD_AXIS_COUNT:SDL_GetNumJoystickAxes(joystick_);buttonCount_=pad_?SDL_GAMEPAD_BUTTON_COUNT:SDL_GetNumJoystickButtons(joystick_);hatCount_=pad_?1:SDL_GetNumJoystickHats(joystick_);}
        }
    }
    // Capabilities are enumerated on configuration/hotplug, never in sample().
    for(int i=0;i<count;++i)if(auto* device=SDL_OpenJoystick(ids[i])){
        devices_[i].axes=SDL_GetNumJoystickAxes(device);devices_[i].buttons=SDL_GetNumJoystickButtons(device);devices_[i].hats=SDL_GetNumJoystickHats(device);SDL_CloseJoystick(device);
    }
    SDL_free(ids);openFeedback();
}
void Controller::compileMap() {
    mappedAxes_={};mappedButtons_.clear();mappedButtons_.reserve(settings_.mapButtons.size());for(auto& hat:mappedHat_)hat.reset();
    for(size_t i=0;i<mappedAxes_.size();++i){auto& route=settings_.mapAxes[i];
        if(route.device<0 || route.device>=int(mappedDevices_.size()) || route.index<0)continue;
        auto* device=mappedDevices_[route.device];if(!device || route.index>=(route.hat?SDL_GetNumJoystickHats(device):SDL_GetNumJoystickAxes(device)))continue;
        mappedAxes_[i]={device,route.index,route.hat,route.invert};
    }
    for(auto& route:settings_.mapButtons){
        if(route.binding<Binding{} || route.binding>=Binding::Count || route.device<0 || route.device>=int(mappedDevices_.size()))continue;
        if(route.button<0){if(route.button>=-4)mappedHat_[-route.button-1].set(size_t(route.binding));continue;}
        auto* device=mappedDevices_[route.device];if(!device || route.button>=SDL_GetNumJoystickButtons(device))continue;
        auto found=std::find_if(mappedButtons_.begin(),mappedButtons_.end(),[&](auto& input){return input.device==device && input.index==route.button;});
        if(found==mappedButtons_.end()){mappedButtons_.push_back({device,route.button,{}});found=std::prev(mappedButtons_.end());}
        found->actions.set(size_t(route.binding));
    }
}
float Controller::AxisInput::value()const {
    if(!device)return 0;
    float result;
    if(hat)result=povValue(SDL_GetJoystickHat(device,index)); else {Sint16 raw=SDL_GetJoystickAxis(device,index);result=float(raw)/(raw<0?32768.f:32767.f);}
    return invert && result>=-1 && result<=1?-result:result;
}
void Controller::sampleMap(float& throttle,bool& validThrottle) {
    for(int i=0;i<3;++i)state_.axes[i]=Allegiance::MapJoystick(mappedAxes_[i].value(),settings_.deadZone,inverseDeadZone_,settings_.linear);
    validThrottle=bool(mappedAxes_[3].device);throttle=mappedAxes_[3].value();
    float head=mappedAxes_[4].value();state_.hatActive=mappedAxes_[4].device && head>=-1 && head<=1;state_.headYaw=state_.hatActive?head*float(M_PI):0;
    int hat=mappedAxes_[5].device?Allegiance::JoystickHatButton(mappedAxes_[5].value()):-1;if(hat>=0)state_.down|=mappedHat_[hat];
    for(auto& input:mappedButtons_)if(SDL_GetJoystickButton(input.device,input.index))state_.down|=input.actions;
}

void Controller::stopFeedback() {
    if(feedbackPlaying_ && haptic_)SDL_StopHapticEffects(haptic_);
    if(rumbleUntil_ && joystick_)SDL_RumbleJoystick(joystick_,0,0,0);
    feedbackPlaying_=false;rumbleUntil_=0;rumblePriority_=-1;
}
void Controller::openFeedback() {
    stopFeedback();if(haptic_){SDL_CloseHaptic(haptic_);haptic_=nullptr;}for(auto& bank:forces_)bank.clear();
    if(!joystick_ || !(SDL_WasInit(SDL_INIT_HAPTIC)&SDL_INIT_HAPTIC) || !SDL_IsJoystickHaptic(joystick_))return;
    haptic_=SDL_OpenHapticFromJoystick(joystick_);if(!haptic_)return;
    auto features=SDL_GetHapticFeatures(haptic_);
    if(features&SDL_HAPTIC_AUTOCENTER)SDL_SetHapticAutocenter(haptic_,settings_.autoCenter?100:0);
    if(!settings_.feedback)return; // Centering is independent of impact/fire cues.
    if(features&SDL_HAPTIC_GAIN)SDL_SetHapticGain(haptic_,int(std::clamp(settings_.feedbackGain,0.f,1.f)*100));
    for(size_t i=0;i<forces_.size();++i) {
        for(auto& preset:presets_[i])if(features&preset.effect.type){auto id=SDL_CreateHapticEffect(haptic_,&preset.effect);if(id>=0)forces_[i].push_back({preset,id});}
        if(forces_[i].empty()){auto fallback=defaultForceEffect(int(i));if(features&fallback.effect.type){auto id=SDL_CreateHapticEffect(haptic_,&fallback.effect);if(id>=0)forces_[i].push_back({fallback,id});}}
    }
}
void Controller::feedback(const Feedback& events,bool active) {
    if(!active || !settings_.feedback || settings_.feedbackGain<=0){stopFeedback();return;}
    if(!joystick_ || !events.mask)return;
    // The original bounce switch also starts its explosion bank. Preserve that
    // authored overlap on force-feedback devices; rumble uses one strong cue.
    unsigned mask=events.mask;if(mask&2)mask|=4;
    int fallback=-1;
    for(int i=0;i<3;++i)if(mask&(1u<<i)) {
        bool played=false;
        for(auto& force:forces_[i]) {
            if(force.preset.directional){int direction=((events.direction[i]%360+360)%360)*100;
                if(force.preset.effect.constant.direction.dir[0]!=direction){force.preset.effect.constant.direction.dir[0]=direction;SDL_UpdateHapticEffect(haptic_,force.id,&force.preset.effect);}}
            played|=SDL_RunHapticEffect(haptic_,force.id,1);
        }
        feedbackPlaying_|=played;if(!played && (events.mask&(1u<<i)))fallback=i;
    }
    if(fallback<0)return;
    uint64_t now=SDL_GetTicks();if(now<rumbleUntil_ && fallback<rumblePriority_)return;
    constexpr Uint16 low[]={18000,42000,56000},high[]={30000,50000,60000};constexpr Uint32 length[]={20,200,1000};
    auto gain=std::clamp(settings_.feedbackGain,0.f,1.f);
    if(SDL_RumbleJoystick(joystick_,Uint16(low[fallback]*gain),Uint16(high[fallback]*gain),length[fallback])){rumbleUntil_=now+length[fallback];rumblePriority_=fallback;}
}

bool Controller::event(const SDL_Event& event) {
    if(event.type==SDL_EVENT_JOYSTICK_ADDED || event.type==SDL_EVENT_JOYSTICK_REMOVED || event.type==SDL_EVENT_GAMEPAD_REMAPPED) {refresh();return true;}
    return false;
}
SDL_Keycode Controller::menuKey(const SDL_Event& event)const {
    if(!settings_.enabled)return 0;
    if(pad_ && event.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN && event.gbutton.which==id_) {
        switch(event.gbutton.button) {
        case SDL_GAMEPAD_BUTTON_SOUTH:return SDLK_RETURN;
        case SDL_GAMEPAD_BUTTON_EAST:case SDL_GAMEPAD_BUTTON_START:return SDLK_ESCAPE;
        case SDL_GAMEPAD_BUTTON_DPAD_UP:return SDLK_UP;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:return SDLK_DOWN;
        default:return 0;
        }
    }
    if(!pad_ && event.type==SDL_EVENT_JOYSTICK_BUTTON_DOWN && event.jbutton.which==id_)
        return event.jbutton.button==0?SDLK_RETURN:event.jbutton.button==1?SDLK_ESCAPE:0;
    if(!pad_ && event.type==SDL_EVENT_JOYSTICK_HAT_MOTION && event.jhat.which==id_)
        return event.jhat.value&SDL_HAT_UP?SDLK_UP:event.jhat.value&SDL_HAT_DOWN?SDLK_DOWN:0;
    return 0;
}
float Controller::axis(int index)const {
    if(!joystick_ || index<0 || index>=axisCount_)return 0;
    Sint16 value=pad_?SDL_GetGamepadAxis(pad_,SDL_GamepadAxis(index)):SDL_GetJoystickAxis(joystick_,index);
    return float(value)/(value<0?32768.f:32767.f);
}
bool Controller::resumeThrottle() {
    if(settings_.mapped){if(!initialized_ || !mappedAxes_[3].device)return false;joyThrottle_=true;return true;}
    int index=settings_.axes[3];if(index==-2)index=pad_?-1:3;
    if(!joystick_ || !initialized_ || index<0 || index>=axisCount_)return false;
    joyThrottle_=true;return true;
}
const ControllerState& Controller::sample(bool active) {
    state_={};if(!joystick_)return state_;
    float value=0;bool validThrottle=false;
    if(settings_.mapped)sampleMap(value,validThrottle);
    else {
    for(int i=0;i<3;++i) {
        int index=settings_.axes[i];if(index==-2)index=i;
        float value=axis(index)*(settings_.invert[i]?-1.f:1.f);
        state_.axes[i]=Allegiance::MapJoystick(value,settings_.deadZone,inverseDeadZone_,settings_.linear);
    }
    int throttle=settings_.axes[3];if(throttle==-2)throttle=pad_?-1:3;
    validThrottle=throttle>=0 && throttle<axisCount_;
    value=axis(throttle)*(settings_.invert[3]?-1.f:1.f);
    for(int i=0;i<std::min(64,buttonCount_);++i) {
        bool pressed=pad_?SDL_GetGamepadButton(pad_,SDL_GamepadButton(i)):SDL_GetJoystickButton(joystick_,i);
        if(!pressed)continue;
        int binding=settings_.buttons[i];if(binding==-2)binding=defaultButton(pad_!=nullptr,i);
        if(binding>=0 && binding<int(Binding::Count))state_.down.set(binding);
    }
    if(pad_ && settings_.triggers) {
        if(axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)>.2f)state_.down.set(size_t(Binding::Fire));
        if(axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER)>.2f)state_.down.set(size_t(Binding::Missile));
    }
    Uint8 hat=0;
    if(settings_.hat>=0 && settings_.hat<hatCount_) {
        if(pad_) {
            if(SDL_GetGamepadButton(pad_,SDL_GAMEPAD_BUTTON_DPAD_UP))hat|=SDL_HAT_UP;
            if(SDL_GetGamepadButton(pad_,SDL_GAMEPAD_BUTTON_DPAD_DOWN))hat|=SDL_HAT_DOWN;
            if(SDL_GetGamepadButton(pad_,SDL_GAMEPAD_BUTTON_DPAD_LEFT))hat|=SDL_HAT_LEFT;
            if(SDL_GetGamepadButton(pad_,SDL_GAMEPAD_BUTTON_DPAD_RIGHT))hat|=SDL_HAT_RIGHT;
        } else hat=SDL_GetJoystickHat(joystick_,settings_.hat);
    }
    float head=povValue(hat);state_.hatActive=head>=-1;state_.headYaw=state_.hatActive?head*float(M_PI):0;
    }
    if(!active || !initialized_) {
        initialized_=active;oldThrottle_=value;joyThrottle_=false;previous_=state_.down;suppressed_=state_.down;
    }
    suppressed_&=state_.down;
    auto physical=state_.down;state_.down&=~suppressed_;
    if(active) {
        state_.pressed=state_.down&~previous_;
        state_.throttleChanged=validThrottle && std::abs(value-oldThrottle_)>settings_.deadZone;
        if(state_.throttleChanged)joyThrottle_=true;
        if(joyThrottle_)oldThrottle_=value;
        state_.useThrottle=validThrottle && joyThrottle_;state_.throttle=(value+1)*.5f;
    } else {state_.axes={};state_.hatActive=false;state_.headYaw=0;state_.down.reset();}
    previous_=physical;return state_;
}
}
