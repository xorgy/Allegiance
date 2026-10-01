// Frozen WinTrek.cpp at 39ef2c072b1eac0e9f97ab1428bc96572cbd5854.
// Only caller names/parameters are adapted; never update to match production.
#pragma once
namespace OriginalFlight {
inline bool axesChanged(const ControlData& controls,const float* baseline,float deadZone) {
    bool bControlsInUse=false;
                                const float c_fAutopilotDisengage = deadZone * 2.0f;
                                bControlsInUse = bControlsInUse ||
                                                 (controls.jsValues[c_axisYaw] - baseline[c_axisYaw] < -c_fAutopilotDisengage) ||
                                                 (controls.jsValues[c_axisYaw] - baseline[c_axisYaw] >  c_fAutopilotDisengage) ||
                                                 (controls.jsValues[c_axisPitch] - baseline[c_axisPitch] < -c_fAutopilotDisengage) ||
                                                 (controls.jsValues[c_axisPitch] - baseline[c_axisPitch] >  c_fAutopilotDisengage) ||
                                                 (controls.jsValues[c_axisRoll] - baseline[c_axisRoll] < -c_fAutopilotDisengage) ||
                                                 (controls.jsValues[c_axisRoll] - baseline[c_axisRoll] >  c_fAutopilotDisengage);
    return bControlsInUse;
}
inline void slew(ControlData& controls,const IhullTypeIGC* hull,float fov,float maxFov) {
static const float  c_minRate = RadiansFromDegrees(7.5f);
								static const float  c_maxRate = RadiansFromDegrees(75.0f);
								float   maxSlewRate = c_minRate +
									(c_maxRate - c_minRate) * fov / maxFov;

								const IhullTypeIGC* pht = hull;
								{
									float               pitch = pht->GetMaxTurnRate(c_axisPitch);

									if (pitch > maxSlewRate)
										controls.jsValues[c_axisPitch] *= maxSlewRate / pitch;
								}
								{
									float               yaw = pht->GetMaxTurnRate(c_axisYaw);

									if (yaw > maxSlewRate)
										controls.jsValues[c_axisYaw] *= maxSlewRate / yaw;
								}
}
inline int buttons(int buttonsM,int selectedWeapon,bool fGroupFire,bool fire,bool missile,bool boost,bool mine,bool chaff) {
    buttonsM |= (selectedWeapon << selectedWeaponShiftIGC);
    if (fire) buttonsM |= fGroupFire ? allWeaponsIGC : oneWeaponIGC;
    if (missile) buttonsM |= missileFireIGC;
    if (boost) buttonsM |= afterburnerButtonIGC;
    if (mine) buttonsM |= mineFireIGC; else buttonsM &= ~mineFireIGC;
    if (chaff) buttonsM |= chaffFireIGC; else buttonsM &= ~chaffFireIGC;
    return buttonsM;
}
}
