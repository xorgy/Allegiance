#pragma once
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace Native {
inline float interfaceScale(glm::ivec2 size,float userScale) {
    return std::max(.2f,std::min({size.y/900.f*userScale,size.x/1152.f,size.y/660.f}));
}
struct MenuLayout {
    float scale;
    glm::vec2 origin;
    glm::vec2 local(glm::vec2 pixel)const {return pixel/scale-origin;}
    glm::vec2 pixel(glm::vec2 local)const {return (local+origin)*scale;}
};
inline MenuLayout menuLayout(glm::ivec2 size,float userScale) {
    float scale=interfaceScale(size,userScale);
    return {scale,(glm::vec2(size)/scale-glm::vec2(1000,620))*.5f};
}
inline int integerDisplayScale(float requested) {
    // Round to the nearest integer, with midpoint ties going down: 150% is
    // still 1x. Invalid platform values mean ordinary pixel density.
    if(!std::isfinite(requested))return 1;
    return int(std::ceil(std::clamp(requested,1.f,8.f)-.5f));
}
inline int classicInterfaceScale(int overrideScale,float displayScale) {
    return overrideScale>0?std::clamp(overrideScale,1,8):integerDisplayScale(displayScale);
}
inline int originalInterfaceScale(glm::ivec2 size,int requested=1,glm::ivec2 canvas={800,600}) {
    // Authored pixels remain pixels. A larger display does not enlarge the UI;
    // a platform or explicit HiDPI scale falls back to the whole scale that fits.
    return std::clamp(requested,1,std::max(1,std::min(size.x/canvas.x,size.y/canvas.y)));
}
inline MenuLayout originalMenuLayout(glm::ivec2 size,int requested=1,glm::ivec2 canvas={800,600}) {
    int scale=originalInterfaceScale(size,requested,canvas);
    // Center on a physical pixel, including odd drawable dimensions. Subpixel
    // translation would defeat pixel replication even with nearest sampling.
    glm::ivec2 offset=(size-canvas*scale)/2;
    return {float(scale),glm::vec2(offset)/float(scale)};
}
}
