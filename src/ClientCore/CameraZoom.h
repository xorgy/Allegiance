// Original WinTrek held-key and wheel zoom equations. Bounds stay with the
// caller: native users may choose a wider unzoomed cockpit field of view.
#pragma once
namespace Allegiance {
inline float ZoomFieldOfView(float value,float dt,bool out,float minimum,float maximum){
    if(out){value+=dt;if(value>maximum)value=maximum;}
    else{value-=dt;if(value<minimum)value=minimum;}return value;
}
inline float ZoomCameraDistance(float value,float dt,bool out,float minimum,float maximum,float rate=1.f){
    const float delta=dt*value*rate;
    if(out){value+=delta;if(value>maximum)value=maximum;}
    else{value-=delta;if(value<minimum)value=minimum;}return value;
}
}
