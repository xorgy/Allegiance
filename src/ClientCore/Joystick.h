// WinTrek's original dead zone and linear/quadratic joystick response.
#pragma once
#include <cmath>
namespace Allegiance {
inline float MapJoystick(float value,float deadZone,float inverseDeadZone,bool linear) {
    if(value>deadZone)value=(value-deadZone)/inverseDeadZone;
    else if(value< -deadZone)value=(value+deadZone)/inverseDeadZone;
    else value=0.0f;
    if(!linear)value*=fabsf(value);
    return value;
}
}
