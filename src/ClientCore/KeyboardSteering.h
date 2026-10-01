// WinTrek::SenseJoystick keyboard overrides. Query in original short-circuit
// order; device axes and virtual mouse steering are supplied by the caller.
#pragma once
#include "../Igc/TrekKeys.h"
namespace Allegiance {
template<class Key> inline void KeyboardSteering(float& yaw,float& pitch,float& roll,Key key){
    if(key(TK_RollRight))roll=-1;else if(key(TK_RollLeft))roll=1;
    if(key(TK_RollModifier)){
        if(key(TK_YawRight))roll=-1;else if(key(TK_YawLeft))roll=1;
    }else{
        if(key(TK_YawRight))yaw=-1;else if(key(TK_YawLeft))yaw=1;
    }
    if(key(TK_PitchDown))pitch=-1;else if(key(TK_PitchUp))pitch=1;
}
}
