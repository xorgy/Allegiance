#pragma once
#include "../zlib/orientation.h"

namespace Allegiance {
// ThingGeo::AddFlare. The hit is a world-space offset from the hull center.
inline Orientation ShieldFlareOrientation(const Orientation& hull,const Vector& hit,
        float radius,const Vector* ellipse) {
    Vector local=hull.TimesInverse(hit);Orientation result;
    if(ellipse && radius>0 && ellipse->x>0 && ellipse->y>0 && ellipse->z>0) {
        local.x/=ellipse->x;local.y/=ellipse->y;local.z/=ellipse->z;
        result.Set(local);result.Scale(*ellipse/radius);
    } else result.Set(local);
    return result;
}
}
