#pragma once
#include "../zlib/vector.h"
#include <algorithm>
#include <cmath>

namespace Allegiance {
inline int MineParticleCount(float strength,float radius) {
    return 1+int(std::max(0.f,strength)*radius*radius/100.f);
}
inline float MineBrightness(float distance){return distance>100?100/distance:1;}
struct HullImpactFrame {Vector forward,right;};
inline HullImpactFrame HullImpactAxes(const Vector& normal) {
    // The original +Z special case omitted -Z (and a coincident impact),
    // allowing a zero cross product to normalize to NaNs.
    Vector forward=CrossProduct(normal,Vector(0,0,1));
    if(forward.LengthSquared()<1e-8f)forward=CrossProduct(normal,Vector(0,1,0));
    if(forward.LengthSquared()<1e-8f)return {Vector(1,0,0),Vector(0,1,0)};
    forward=forward.Normalize();return {forward,CrossProduct(forward,normal)};
}
inline float HullImpactScale(float age){return std::min(2.f,.1f+10*age);}
}
