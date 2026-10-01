// Original MouseInputStreamImpl acceleration and JoystickImageImpl accumulation.
// Screen/platform coordinate conversion belongs to each caller.
#pragma once
#include <cmath>
namespace Allegiance {
inline float MouseDelta(float delta,float sensitivity,int acceleration,float threshold1,float threshold2){
    if(std::fabs(delta)>threshold1 && acceleration>=1)
        return delta*(std::fabs(delta)>threshold2 && acceleration>=2?4.f:2.f)*sensitivity;
    return delta*sensitivity;
}
inline void AccumulateMouseJoystick(float& x,float& y,float dx,float dy,float sensitivity){
    x-=dx*sensitivity;y-=dy*sensitivity;float lengthSquared=x*x+y*y;
    if(lengthSquared>1){float length=std::sqrt(lengthSquared);x/=length;y/=length;}
}
struct MouseJoystick {
    float x{},y{};bool enabled{},justEnabled{};
    void enable(bool value){if(enabled!=value){enabled=value;justEnabled=value;if(!value)x=y=0;}}
    void move(float dx,float dy,float sensitivity){if(!enabled)return;if(justEnabled)justEnabled=false;else AccumulateMouseJoystick(x,y,dx,dy,sensitivity);}
};
}
