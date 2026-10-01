#pragma once
#include <cmath>

namespace Allegiance {
// ConsoleData's command selection and vertical waypoint geometry. Coordinates
// here are presentation-neutral; both renderers supply their own camera ray.
inline bool CommandGridVisible(bool preference,bool commandCamera,bool animating,bool previousCommandCamera) {
    return preference || commandCamera || (animating && previousCommandCamera);
}
inline void CommandGridSpan(int segment,int count,float radius,float& coordinate,float& halfLength) {
    coordinate=(float(segment)/count-.5f)*2*radius;halfLength=std::sqrt(std::fmax(0.f,radius*radius-coordinate*coordinate));
}
inline bool CommandSelectionRectangle(float dx,float dy) {
    return std::fabs(dx)>5.f || std::fabs(dy)>5.f;
}
inline bool CommandSelectionOverlap(float x,float y,float radius,float left,float top,float right,float bottom) {
    return x+radius>=left && x-radius<=right && y+radius>=top && y-radius<=bottom;
}
inline float CommandPickAltitude(float anchorX,float anchorY,float cameraX,float cameraY,float cameraZ,
    float backwardZ,float rayX,float rayY,float rayZ) {
    if(backwardZ>.9f || backwardZ<-.9f)return 0.f;
    float denominator=rayX*rayX+rayY*rayY;
    // The original formula is undefined for a vertical ray. Top-down cameras
    // normally exclude it above; retain a finite plane point at the boundary.
    if(denominator<1.e-12f)return 0.f;
    return ((anchorX-cameraX)*rayX*rayZ+(anchorY-cameraY)*rayY*rayZ)/denominator+cameraZ;
}
}
