#include "controller.hpp"
#include "../ClientCore/Joystick.h"
#include "../ClientCore/JoystickHat.h"
#include "../ClientCore/Throttle.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>
using namespace Native;
namespace {
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(std::string(text)+": "+SDL_GetError());}
float original(float value,float deadZone,float inverse,bool linear) {
    if(value>deadZone)value=(value-deadZone)/inverse;
    else if(value< -deadZone)value=(value+deadZone)/inverse;
    else value=0;
    if(!linear)value*=fabsf(value);
    return value;
}
void pump(Controller& controller){SDL_UpdateJoysticks();SDL_Event event;while(SDL_PollEvent(&event))controller.event(event);}
struct Virtual {
    SDL_JoystickID id;SDL_Joystick* joystick;int rumbleCalls{};Uint16 low{},high{};
    Virtual(bool pad) {
        SDL_VirtualJoystickDesc desc;SDL_INIT_INTERFACE(&desc);
        desc.type=pad?SDL_JOYSTICK_TYPE_GAMEPAD:SDL_JOYSTICK_TYPE_FLIGHT_STICK;
        desc.naxes=pad?6:4;desc.nbuttons=pad?SDL_GAMEPAD_BUTTON_COUNT:8;desc.nhats=pad?0:1;
        desc.userdata=this;desc.Rumble=[](void* data,Uint16 low,Uint16 high){auto& self=*static_cast<Virtual*>(data);++self.rumbleCalls;self.low=low;self.high=high;return true;};
        desc.name=pad?"Allegiance test gamepad":"Allegiance test flight stick";desc.vendor_id=0x1209;desc.product_id=pad?2:1;
        if(pad){desc.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;desc.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;}
        id=SDL_AttachVirtualJoystick(&desc);require(id,"attach virtual joystick");joystick=SDL_OpenJoystick(id);require(joystick,"open virtual joystick");
    }
    std::string guid(){char out[33];SDL_GUIDToString(SDL_GetJoystickGUID(joystick),out,sizeof(out));return out;}
    void axis(int index,Sint16 value){require(SDL_SetJoystickVirtualAxis(joystick,index,value),"set virtual axis");}
    void button(int index,bool value){require(SDL_SetJoystickVirtualButton(joystick,index,value),"set virtual button");}
    void detach(){if(id){require(SDL_DetachVirtualJoystick(id),"detach joystick");id=0;}}
    ~Virtual(){detach();SDL_CloseJoystick(joystick);}
};
void run() {
    int comparisons=0;
    for(float deadZone:{0.f,.05f,.1f,.3f,.95f})for(bool linear:{false,true})for(float polarity:{-1.f,1.f})for(int v=-32768;v<=32767;++v) {
        float axis=float(v)/(v<0?32768.f:32767.f);
        require(Allegiance::MapJoystick(axis,deadZone,polarity*(1-deadZone),linear)==original(axis,deadZone,polarity*(1-deadZone),linear),"frozen WinTrek joystick response");++comparisons;
    }
    int hatComparisons=0;
    auto frozenHat=[](float v){if(v>.75f)return 2;if(v>.25f)return 1;if(v>-.25f)return 0;if(v>-.75f)return 3;if(v>-1.25f)return 2;return -1;};
    for(int i=-4096;i<=4096;++i){float v=float(i)/2048;require(Allegiance::JoystickHatButton(v)==frozenHat(v),"frozen command hat thresholds");++hatComparisons;}
    printf("%d frozen original virtual hat comparisons passed\n",hatComparisons);
    int throttleCases=0;
    for(int setting=0;setting<5;++setting)for(float dt:{1.f/30,1.f/60,1.f/165,.1f}) {
        constexpr float delay[]={.066f,.033f,.022f,.016f,0};float elapsed=0,expected=-1,actual=-1;bool ready=true;
        Allegiance::ThrottleRepeat repeat;
        for(int i=0;i<1000;++i) {
            if(!ready && (elapsed+=dt)>delay[setting]){ready=true;elapsed=0;}
            repeat.Advance(dt,setting);bool up=(i/47)%2==0,held=i%17<12;
            if(held && ready){expected=up?(expected<.8f?expected+.2f:1.f):(expected>-.8f?expected-.2f:-1.f);ready=false;}
            if(held && repeat.Consume())actual=Allegiance::StepThrottle(actual,up);
            require(expected==actual,"original throttle repeat, release and clamp behavior");++throttleCases;
        }
    }
    require(Allegiance::ThrottlePreset(0)==-1 && Allegiance::ThrottlePreset(1)==-.3f && Allegiance::ThrottlePreset(2)==.3f && Allegiance::ThrottlePreset(3)==1,"original throttle preset values");
    printf("%d frozen throttle input sequence comparisons passed\n",throttleCases);
    ControllerSettings settings;settings.device="unconnected fixture";Controller controller(settings);
    Virtual stick(false);settings.device=stick.guid();controller.configure(settings);pump(controller);
    require(!controller.gamepad() && controller.axes()==4 && controller.buttons()==8 && controller.hats()==1,"raw device capabilities");
    stick.axis(3,32767);stick.button(0,true);pump(controller);auto first=controller.sample(true);
    require(!first.down.any() && !first.pressed.any() && !first.useThrottle,"initial held button and throttle must not activate");
    stick.button(0,false);pump(controller);controller.sample(true);
    stick.button(0,true);stick.axis(0,32767);stick.axis(1,-32768);stick.axis(2,-32768);stick.axis(3,-32768);
    require(SDL_SetJoystickVirtualHat(stick.joystick,0,SDL_HAT_RIGHTUP),"virtual hat");pump(controller);auto state=controller.sample(true);
    require(state.axes[0]==-1 && state.axes[1]==-1 && state.axes[2]==1,"axis polarity and endpoints");
    require(state.down[size_t(Binding::Fire)] && state.pressed[size_t(Binding::Fire)],"held/pressed controller gun mapping");
    require(state.useThrottle && state.throttleChanged && state.throttle==1,"throttle takes over after physical movement");
    require(state.hatActive && std::abs(state.headYaw-float(M_PI/4))<1e-6,"diagonal hat orientation");
    state=controller.sample(true);require(!state.pressed.any() && state.down[size_t(Binding::Fire)],"held buttons emit one action edge");
    controller.manualThrottle();state=controller.sample(true);require(!state.useThrottle,"keyboard throttle retains control until stick moves");
    require(controller.resumeThrottle(),"autopilot release resumes valid physical throttle");state=controller.sample(true);require(state.useThrottle && !state.throttleChanged,"resumed stationary throttle is not a new input change");
    controller.manualThrottle();stick.axis(3,0);pump(controller);state=controller.sample(true);require(state.useThrottle && state.throttle==.5f,"joystick throttle retakes control on movement");
    state=controller.sample(false);require(!state.down.any() && !state.pressed.any() && !state.useThrottle && state.axes[0]==0 && !state.hatActive,"focus loss suppresses all controls");
    state=controller.sample(true);require(!state.down.any() && !state.useThrottle,"focus regain requires release and renewed throttle movement");
    stick.button(0,false);pump(controller);controller.sample(true);stick.button(0,true);pump(controller);require(controller.sample(true).pressed[size_t(Binding::Fire)],"release rearms focused buttons");
    settings.buttons[0]=int(Binding::TargetEnemyNearest);settings.axes[0]=-1;settings.hat=-1;controller.configure(settings);controller.sample(true);
    stick.button(0,false);pump(controller);controller.sample(true);stick.button(0,true);pump(controller);state=controller.sample(true);
    require(state.pressed[size_t(Binding::TargetEnemyNearest)] && !state.down[size_t(Binding::Fire)] && state.axes[0]==0 && !state.hatActive,"custom target button and disabled axis/hat");
    stick.detach();pump(controller);require(!controller.resumeThrottle(),"disconnected throttle uses keyboard fallback");state=controller.sample(true);require(controller.name().empty() && !state.down.any() && state.axes[0]==0,"unplug clears stale input and device handle");
    {
        Virtual primary(false),secondary(false);
        settings=ControllerSettings{};settings.mapped=true;
        settings.mapAxes={ControllerAxisRoute{0,0,false,true},ControllerAxisRoute{1,1,false,true},ControllerAxisRoute{1,2,false,true},ControllerAxisRoute{1,3,false,true},ControllerAxisRoute{0,0,true,false},ControllerAxisRoute{1,0,true,false}};
        settings.mapButtons={{0,0,Binding::Fire},{0,0,Binding::Boost},{1,1,Binding::Missile},{0,-1,Binding::Target},{0,-2,Binding::NextWeapon},{0,-3,Binding::AcceptOrder},{0,-4,Binding::LookLeft}};
        controller.configure(settings);pump(controller);
        require(controller.devices().size()==2 && controller.devices()[0].guid!=controller.devices()[1].guid,"duplicate controller GUIDs must have distinct live identities");
        settings.mapDevices[0]=controller.devices()[0].guid;settings.mapDevices[1]=controller.devices()[1].guid;
        controller.configure(settings);controller.sample(true);
        primary.axis(0,32767);secondary.axis(1,-32768);secondary.axis(2,32767);secondary.axis(3,-32768);primary.button(0,true);secondary.button(1,true);
        require(SDL_SetJoystickVirtualHat(primary.joystick,0,SDL_HAT_RIGHTUP),"primary look hat");require(SDL_SetJoystickVirtualHat(secondary.joystick,0,SDL_HAT_RIGHT),"secondary command hat");
        pump(controller);state=controller.sample(true);
        require(state.axes[0]==-1 && state.axes[1]==1 && state.axes[2]==-1 && state.useThrottle && state.throttle==1,"independent device axis and throttle routes");
        require(state.down[size_t(Binding::Fire)] && state.down[size_t(Binding::Boost)] && state.down[size_t(Binding::Missile)] && state.pressed[size_t(Binding::NextWeapon)] && state.down.count()==4,"multi-device physical aliases and mapped command hat");
        require(state.hatActive && std::abs(state.headYaw-float(M_PI/4))<1e-6,"look hat is independent of the command hat");
        constexpr Uint8 positions[]={SDL_HAT_CENTERED,SDL_HAT_UP,SDL_HAT_RIGHTUP,SDL_HAT_RIGHT,SDL_HAT_RIGHTDOWN,SDL_HAT_DOWN,SDL_HAT_LEFTDOWN,SDL_HAT_LEFT,SDL_HAT_LEFTUP};
        constexpr int selected[]={-1,0,0,1,1,2,2,3,3};
        constexpr Binding actions[]={Binding::Target,Binding::NextWeapon,Binding::AcceptOrder,Binding::LookLeft};
        primary.button(0,false);secondary.button(1,false);
        for(size_t i=0;i<std::size(positions);++i){require(SDL_SetJoystickVirtualHat(secondary.joystick,0,positions[i]),"command hat direction");pump(controller);state=controller.sample(true);
            require(state.down.count()==size_t(selected[i]>=0) && (selected[i]<0 || state.down[size_t(actions[selected[i]])]),"original command hat cardinal and diagonal tie-breaks");
        }
        secondary.button(0,true);pump(controller);state=controller.sample(true);require(!state.down[size_t(Binding::Fire)],"mapped mode must not leak default joystick bindings");secondary.button(0,false);
        controller.sample(false);primary.button(0,true);secondary.button(1,true);pump(controller);controller.sample(false);
        state=controller.sample(true);require(state.down.none() && !state.useThrottle,"mapped held controls are suppressed across focus/menu resume");
        primary.button(0,false);secondary.button(1,false);require(SDL_SetJoystickVirtualHat(secondary.joystick,0,SDL_HAT_CENTERED),"release command hat");pump(controller);controller.sample(true);
        secondary.button(1,true);pump(controller);require(controller.sample(true).pressed[size_t(Binding::Missile)],"secondary mapped button release rearms action");
        primary.detach();pump(controller);controller.sample(true);secondary.button(1,false);pump(controller);controller.sample(true);secondary.button(1,true);secondary.axis(3,32767);pump(controller);state=controller.sample(true);
        require(state.axes[0]==0 && state.axes[1]==1 && state.pressed[size_t(Binding::Missile)] && state.useThrottle && state.throttle==0,"unplugging one duplicate device preserves other device routes and suppresses removed axes");
        secondary.detach();pump(controller);require(controller.sample(true).down.none() && !controller.resumeThrottle(),"all mapped device removal clears actions and throttle");
    }
    {
        Virtual pad(true);pad.axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER,-32768);pad.axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,-32768);
        settings=ControllerSettings{};settings.device=pad.guid();controller.configure(settings);pump(controller);controller.sample(true);
        require(controller.gamepad(),"SDL virtual gamepad standard mapping");
        pad.axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,32767);pad.button(SDL_GAMEPAD_BUTTON_SOUTH,true);pad.button(SDL_GAMEPAD_BUTTON_DPAD_LEFT,true);pump(controller);state=controller.sample(true);
        require(state.down[size_t(Binding::Fire)] && state.pressed[size_t(Binding::Target)] && state.hatActive && std::abs(state.headYaw+float(M_PI/2))<1e-6 && !state.useThrottle,"gamepad trigger, button, dpad and automatic throttle mapping");
        SDL_Event event{};event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;event.gbutton.which=pad.id;event.gbutton.button=SDL_GAMEPAD_BUTTON_SOUTH;require(controller.menuKey(event)==SDLK_RETURN,"gamepad menu activation");
        event.gbutton.button=SDL_GAMEPAD_BUTTON_EAST;require(controller.menuKey(event)==SDLK_ESCAPE,"gamepad menu back");
        Feedback feedback;feedback.add(0);controller.feedback(feedback,true);require(pad.rumbleCalls>0 && pad.low==18000 && pad.high==30000,"actual fire cue reaches virtual rumble backend");
        int calls=pad.rumbleCalls;controller.feedback({},true);require(pad.rumbleCalls==calls,"idle feedback performs no device update");
        feedback={};feedback.add(1,135);controller.feedback(feedback,true);require(pad.low==42000 && pad.high==50000,"impact cue replaces weak gun rumble");
        calls=pad.rumbleCalls;feedback={};feedback.add(0);controller.feedback(feedback,true);require(pad.rumbleCalls==calls,"gun cue cannot shorten an active impact");
        controller.feedback({},false);require(pad.low==0 && pad.high==0,"focus/menu suppression stops physical rumble");
        settings.mapped=true;settings.mapDevices[0]=pad.guid();settings.mapAxes[0]={0,0,false,true};settings.mapButtons={{0,0,Binding::Fire}};controller.configure(settings);pump(controller);controller.sample(true);
        require(!controller.gamepad(),"original map uses raw SDL button/axis indices on gamepads");
        settings.mapped=false;controller.configure(settings);pump(controller);controller.sample(true);require(controller.gamepad(),"return to standard gamepad mapping closes raw multi-device handles");
        settings.feedbackGain=.5f;controller.configure(settings);feedback={};feedback.add(0);controller.feedback(feedback,true);
        require(pad.low==9000 && pad.high==15000,"feedback strength must scale both physical rumble motors");
        settings.feedbackGain=0;controller.configure(settings);calls=pad.rumbleCalls;controller.feedback(feedback,true);require(pad.rumbleCalls==calls,"zero feedback gain must suppress device events");
        settings.feedbackGain=1;
        settings.feedback=false;controller.configure(settings);calls=pad.rumbleCalls;controller.feedback(feedback,true);require(pad.rumbleCalls==calls,"disabled force feedback emits no cue");
        settings.enabled=false;controller.configure(settings);state=controller.sample(true);require(!state.down.any() && controller.name().empty(),"disabled controller releases device");
    }
    printf("%d original joystick curve comparisons; SDL virtual stick/gamepad hotplug, focus, throttle, buttons, hats and menu routing passed\n",comparisons);
}
}
int main() {
    try{SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");require(SDL_Init(SDL_INIT_GAMEPAD),"initialize SDL joystick subsystem");{run();}SDL_Quit();return 0;}
    catch(const std::exception& e){fprintf(stderr,"controller: %s\n",e.what());SDL_Quit();return 1;}
}
