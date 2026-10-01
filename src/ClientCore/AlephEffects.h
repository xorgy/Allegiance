#pragma once
#include <cmath>

namespace Allegiance {
// AlephGeo's authored funnel and animation. Native backends upload the static
// geometry once and add the time-dependent UV offset in the vertex shader.
struct AlephVertex {float x,y,z,brightness,u,v;};
inline AlephVertex AlephPoint(float radius,float section) {
    const float pi=3.14159265358979323846f;
    float angle=2*pi*section,depth=(1-radius)*(1-radius);
    return {radius*std::cos(angle),radius*std::sin(angle),-depth,
        2*radius*(1-radius),4*section+5*(1-depth),radius};
}
inline float AlephTextureOffset(float time) {return std::fmod(time/10,1.f);}
inline int AlephDetail(float projectedRadius) {
    return projectedRadius<16?3:projectedRadius<32?2:projectedRadius<64?1:0;
}
inline float AlephStarScale(float delta,float duration) {
    float scale=.1f;
    if(delta>-.25f && duration>0) {
        float progress=delta>=0?1-delta/duration:1+delta/.25f;
        float swell=progress*progress*5.9f;
        if(delta>=0) {
            float pulse=std::sin(3.14159265358979323846f*8.5f*progress);
            scale+=swell*pulse*pulse+swell*.8f;
        } else scale+=swell;
    }
    return scale;
}
}
