#pragma once
#include <cmath>

namespace Allegiance {
// LightsGeo's authored flash envelope. Advance whole periods in constant time;
// the original loop could walk hours of cycles on first viewing a model.
inline float StrobeBrightness(float time,float period,float& phase,float rampUp,float hold,float rampDown) {
    if(!(period>0))return 0;
    float value=time-phase;
    if(value>=period){phase+=std::floor(value/period)*period;value=time-phase;}
    if(value<0)return 0;
    if(value<rampUp)return value/rampUp;
    value-=rampUp;
    if(value<hold)return 1;
    value-=hold;
    return value<rampDown?1-value/rampDown:0;
}
}
