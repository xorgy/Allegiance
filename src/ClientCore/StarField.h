#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cfloat>

namespace Allegiance {
// Private Windows CRT sequence: authored star seeds must not reseed simulation RNG.
class StarRandom {
    uint32_t state;
public:
    explicit StarRandom(uint32_t seed):state(seed){}
    float operator()(float low,float high) {
        state=state*214013u+2531011u;
        return float((state>>16)&32767u)/32767.f*(high*(1-FLT_EPSILON)-low)+low;
    }
};
struct StarPoint {float x,y;};
inline StarPoint ProjectStar(float x,float y,float z,float focus,float centerX,float centerY) {
    if(z>0){x=-x;y=-y;}
    return {float(int(focus*x+centerX)),float(int(centerY-focus*y))};
}
// Native sky directions use the same perspective as world geometry. Positive
// depth is along camera forward. Cull before dividing; never fold the rear
// hemisphere onto the visible sky. Keep ProjectStar's legacy behavior separate.
inline bool ProjectPerspectiveStar(float x,float y,float depth,float focus,float centerX,float centerY,StarPoint& point) {
    if(!(depth>0))return false;
    float sx=focus*x,sy=focus*y;
    if(sx < -centerX*depth || sx >= centerX*depth || sy > centerY*depth || sy <= -centerY*depth)return false;
    float inverseDepth=1/depth;
    point={float(int(centerX+sx*inverseDepth)),float(int(centerY-sy*inverseDepth))};
    return point.x>=0 && point.y>=0 && point.x<2*centerX && point.y<2*centerY;
}
struct StarTrail {StarPoint end;float brightness;bool point;};
inline StarTrail StarMotion(StarPoint point,StarPoint old) {
    float length=std::abs(point.x-old.x)+std::abs(point.y-old.y);
    if(length<=1)return {point,1,true};
    if(length>16){float scale=16/length;old={point.x+(old.x-point.x)*scale,point.y+(old.y-point.y)*scale};}
    return {old,std::max(.25f,1-.1f*length),false};
}
}
