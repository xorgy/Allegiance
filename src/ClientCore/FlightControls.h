// WinTrek flight-input policy; device polling and training approval stay in the
// caller. These inline helpers retain the original branches and IGC writes.
#pragma once
#include "../Igc/igc.h"
namespace Allegiance {
inline bool AutopilotAxesChanged(const ControlData& controls,const float* baseline,float deadZone) {
    const float threshold=deadZone*2.0f;
    return (controls.jsValues[c_axisYaw]-baseline[c_axisYaw]<-threshold) ||
           (controls.jsValues[c_axisYaw]-baseline[c_axisYaw]> threshold) ||
           (controls.jsValues[c_axisPitch]-baseline[c_axisPitch]<-threshold) ||
           (controls.jsValues[c_axisPitch]-baseline[c_axisPitch]> threshold) ||
           (controls.jsValues[c_axisRoll]-baseline[c_axisRoll]<-threshold) ||
           (controls.jsValues[c_axisRoll]-baseline[c_axisRoll]> threshold);
}
inline float AutopilotReleaseThrottle(IshipIGC* ship) {
    return ship->GetEngineVector()*ship->GetOrientation().GetForward()>0?1.0f:-1.0f;
}
inline int ManualWeaponButtons(int buttons,Mount weapon,bool grouped,bool fire,bool missile,bool boost) {
    buttons|=weapon<<selectedWeaponShiftIGC;
    if(fire)buttons|=grouped?allWeaponsIGC:oneWeaponIGC;
    if(missile)buttons|=missileFireIGC;
    if(boost)buttons|=afterburnerButtonIGC;
    return buttons;
}
inline int ExpendableButtons(int buttons,bool mine,bool chaff) {
    if(mine)buttons|=mineFireIGC;else buttons&=~mineFireIGC;
    if(chaff)buttons|=chaffFireIGC;else buttons&=~chaffFireIGC;
    return buttons;
}
inline void LimitCockpitSlew(ControlData& controls,const IhullTypeIGC* hull,float fov,float maxFov) {
    static const float minimum=RadiansFromDegrees(7.5f),maximum=RadiansFromDegrees(75.0f);
    const float slew=minimum+(maximum-minimum)*fov/maxFov;
    const float pitch=hull->GetMaxTurnRate(c_axisPitch);
    if(pitch>slew)controls.jsValues[c_axisPitch]*=slew/pitch;
    const float yaw=hull->GetMaxTurnRate(c_axisYaw);
    if(yaw>slew)controls.jsValues[c_axisYaw]*=slew/yaw;
}
inline void ApplyClientControls(IshipIGC* ship,const ControlData& controls,int buttons) {
    ship->SetControls(controls);
    ship->SetStateBits(buttonsMaskIGC|weaponsMaskIGC|selectedWeaponMaskIGC|
                       missileFireIGC|mineFireIGC|chaffFireIGC,buttons);
}
}
