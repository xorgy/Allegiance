// Original WinTrek external-camera geometry, independent of render backends.
#pragma once
#include "../zlib/orientation.h"
#include "../zlib/zmath.h"
#include <algorithm>
#include <cmath>
namespace Allegiance {
inline Orientation CommandViewOrientation(bool topDown,float roll) {
    Orientation o;
    if(!topDown) {
        static const Vector forward(0.0f,sqrt2/2.0f,-sqrt2/2.0f),up(0.0f,sqrt2/2.0f,sqrt2/2.0f);
        o.Set(forward,up);
    } else o.Reset();
    o.PostRoll(roll);return o;
}
// Original command-view policy; all inputs are rates integrated by the caller.
inline float CommandViewRoll(float roll,float delta) {
    roll+=delta;if(roll>=2*pi)roll-=2*pi;else if(roll<0)roll+=2*pi;return roll;
}
inline float CommandViewDistance(float distance,float delta) {
    return delta>=0?(std::min)(distance+delta,20000.f):(std::max)(distance+delta,50.f);
}
inline void PanCommandView(const Orientation& orientation,float right,float down,Vector& displacement,Vector& center) {
    if(right){Vector v=orientation.GetRight()*right;displacement+=v;center+=v;}
    if(down){Vector up=orientation.GetUp();up.z=0;float length=float(std::sqrt(up.x*up.x+up.y*up.y));
        if(length>0){Vector v=up*(down/length);displacement-=v;center-=v;}}
}
inline void ClampCommandView(Vector& center) {
    float length2=center.LengthSquared();if(length2>4.f*10000.f*10000.f)center*=20000.f/float(std::sqrt(length2));
}
inline void PositionCommandView(const Vector* focus,const Vector& fallback,float minimumDistance,
                               Vector& center,Vector& displacement,float& distance) {
    displacement=Vector::GetZero();
    if(focus){center=*focus;distance=10000.f;}
    else {center=fallback;distance=(std::max)(distance,minimumDistance);}
}
inline void TrackCamera(const Vector& from,const Vector& toward,float length,
                        Orientation& orientation,Vector& position) {
    Vector delta=toward-from;
    // A destroyed target can coincide with its missile/ship for one update.
    // Retain the current heading instead of normalizing a zero vector.
    Vector axis=delta.LengthSquared()>1e-12f?delta.Normalize():orientation.GetForward();
    Vector up=orientation.GetUp();
    if(std::fabs(up*axis)>.95f)up=orientation.GetRight();
    Vector side=CrossProduct(axis,up);
    position=from-axis*(length*2.f)+side*(length*.25f);
    orientation.TurnTo(axis);
}
// WinTrek's override views: an upright, frozen death frame, or a tracking
// launch frame. They contain no IGC ownership or renderer policy.
inline float OverrideCameraDuration(bool death){return death?5.f:3.f;}
inline Orientation OverrideCameraOrientation(){return Orientation(Vector(1,0,0),Vector(0,0,1));}
inline void OverrideCameraFrame(const Vector& from,const Vector& toward,float radius,bool death,
                                Orientation& orientation,Vector& position) {
    float offset=radius*(death?2.f:1.f);
    TrackCamera(from,toward,offset*3.f,orientation,position);
}
inline void DeathCameraFrame(const Vector& from,const Orientation& shipOrientation,float radius,const Vector* target,
                             Orientation& orientation,Vector& position) {
    orientation=OverrideCameraOrientation();
    OverrideCameraFrame(from,target?*target:from+shipOrientation.GetBackward()*(radius*2.f),radius,true,orientation,position);
}
inline void FrameFlyBy(const Vector& ship,const Vector& target,const Orientation& orientation,
                      Vector& center,float& offset) {
    center=.5f*(target+ship);
    auto extent=[&](const Vector& point) {
        Vector delta=point-center;float back=delta*orientation.GetBackward();
        // Preserve the original conservative framing (including the plus).
        float side=float(std::sqrt(delta.LengthSquared()+back*back));
        return side*3.f+back;
    };
    offset=1.25f*(std::max)(extent(target),extent(ship));
}
inline void OrbitCamera(Orientation& orientation,float dt) {
    orientation.Pitch(dt*.35f);orientation.Yaw(dt*.2f);orientation.Roll(dt*.5f);
}
}
