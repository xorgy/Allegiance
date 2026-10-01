// Original WinTrek aim-indicator response curves, independent of render backend.
#pragma once
#include <cmath>
namespace Allegiance {
inline float IndicatorAngularRadius(float radius,float distance,double angleToPixels) {
    double sinAlpha=radius/distance;
    return sinAlpha<1.0?float(angleToPixels*asin(sinAlpha)):640.f;
}
inline float CockpitAimGreen(float distanceSquared,float radiusSquared) {
    if(radiusSquared<=0)return 0;
    float f=distanceSquared/radiusSquared;
    return f<2.f?(f<1.f?1.f-f*.75f:.5f-f/4.f):0.f;
}
inline float ExternalAimGreen(float time,float speed,float cosine,float radius) {
    // The original quotient is undefined at a perpendicular aim or zero radius.
    if(cosine==0 || radius<=0)return 0;
    float a=time*speed,h=a/cosine,ha2=h*h-a*a,r2=radius*radius;
    return ha2<=r2?1.f-ha2/r2:0.f;
}
}
