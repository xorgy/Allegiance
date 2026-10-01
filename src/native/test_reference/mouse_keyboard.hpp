// Frozen numeric policies from 39ef2c072b1eac0e9f97ab1428bc96572cbd5854:
// MouseInputStreamImpl, JoystickImageImpl and WinTrek::SenseJoystick.
#pragma once
#include <cmath>
namespace OriginalMouseKeyboard {
// WinTrek::OnTrekKey / OnTrekKeyFilter, TK_ToggleMouse. Keep the original
// statements distinct from the production result construction.
inline void toggle(bool& enabled,unsigned overlays,bool& fresh,unsigned& dismiss,bool overrideView){
    if(overrideView){enabled=!enabled;return;}
    unsigned banishable=ofTeleportPane|ofInvestment|ofGameState|ofTeam|ofExpandedTeam;
    if(overlays&(banishable&~ofInvestment)){dismiss=banishable&~ofInvestment;enabled=true;}
    else if(fresh && (overlays&ofInvestment))fresh=false;
    else enabled=!enabled;
}
// SenseJoystick after physical MapJoystick transformation, before keyboard
// steering. Cursor/menu ownership does not modify physical axis values.
inline void sense(float* values,bool mouse,float mouseYaw,float mousePitch,
    const float* joystick,bool modifier,bool flip){
    if(mouse){values[0]=mouseYaw;values[1]=mousePitch;values[2]=0;}
    else {
        if(modifier){values[0]=0;values[2]=joystick[0];}
        else {values[0]=joystick[0];values[2]=joystick[2];}
        values[1]=joystick[1];
    }
    if(flip)values[1]=-values[1];
}
inline float delta(int value,float sensitivity,int acceleration,int threshold1,int threshold2){
    if(std::abs(value)>threshold1 && acceleration>=1){if(std::abs(value)>threshold2 && acceleration>=2)return float(value)*4.f*sensitivity;else return float(value)*2.f*sensitivity;}
    return float(value)*sensitivity;
}
inline void move(float& x,float& y,float dx,float dy,float sensitivity){
    float px=x-dx*sensitivity,py=y-dy*sensitivity;
    float length=std::sqrt(px*px+py*py);if(length>1){px/=length;py/=length;}x=px;y=py;
}
template<class Key> void keys(float& yaw,float& pitch,float& roll,Key key){
    if(key(TK_RollRight))roll=-1.0f;else if(key(TK_RollLeft))roll=1.0f;
    if(key(TK_RollModifier)){if(key(TK_YawRight))roll=-1.0f;else if(key(TK_YawLeft))roll=1.0f;}
    else {if(key(TK_YawRight))yaw=-1.0f;else if(key(TK_YawLeft))yaw=1.0f;}
    if(key(TK_PitchDown))pitch=-1.0f;else if(key(TK_PitchUp))pitch=1.0f;
}
}
