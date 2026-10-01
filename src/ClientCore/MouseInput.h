// WinTrek::OnTrekKey(TK_ToggleMouse), UpdateMouseEnabled and SenseJoystick.
#pragma once
#include "Overlay.h"
namespace Allegiance {
inline constexpr OverlayMask MouseDismissPanes=ofTeleportPane|ofGameState|ofTeam|ofExpandedTeam;
struct MouseToggleResult {bool enabled{},freshInvestment{};OverlayMask dismiss{};};
constexpr MouseToggleResult ToggleMouseInput(bool enabled,OverlayMask overlays,bool freshInvestment,bool overrideView=false){
    if(!overrideView && (overlays&MouseDismissPanes))return {true,freshInvestment,MouseDismissPanes};
    if(!overrideView && freshInvestment && (overlays&ofInvestment))return {enabled,false,0};
    return {!enabled,freshInvestment,0};
}
struct SteeringAxes {float yaw{},pitch{},roll{};};
constexpr SteeringAxes JoystickSteering(bool mouseEnabled,float mouseYaw,float mousePitch,
    float joystickYaw,float joystickPitch,float joystickRoll,bool rollModifier,bool flipY){
    SteeringAxes axes=mouseEnabled?SteeringAxes{mouseYaw,mousePitch,0}:
        SteeringAxes{rollModifier?0:joystickYaw,joystickPitch,rollModifier?joystickYaw:joystickRoll};
    if(flipY)axes.pitch=-axes.pitch;
    return axes;
}
}
