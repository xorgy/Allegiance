// Original SenseJoystick throttle values, expressed in IGC's [-1,1] range.
#pragma once
namespace Allegiance {
inline float StepThrottle(float value,bool up) {
    return up?(value<.8f?value+.2f:1.f):(value>-.8f?value-.2f:-1.f);
}
inline float ThrottlePreset(int preset) {
    return preset==0?-1.f:preset==1?-.3f:preset==2?.3f:1.f;
}
inline float ThrottleDelay(int setting) {
    static const float delays[]={.066f,.033f,.022f,.016f,0.f};
    return delays[setting>=0 && setting<5?setting:2];
}
// The original repeat timer advances while its gate is closed, even after a
// key is released. One adjustment is allowed per input sample.
class ThrottleRepeat {
    bool ready_{true};float elapsed_{};
public:
    void Reset(){ready_=true;elapsed_=0;}
    void Advance(float dt,int delay) {
        if(!ready_ && (elapsed_+=dt)>ThrottleDelay(delay)){ready_=true;elapsed_=0;}
    }
    bool Consume(){if(!ready_)return false;ready_=false;return true;}
};
}
