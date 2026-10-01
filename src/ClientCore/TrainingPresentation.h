#pragma once
#include "Overlay.h"

namespace Allegiance {
// IndicatorImage's authored callouts, in its upward-growing view coordinates.
// Keep geometry and captions independent of either renderer.
struct TrainingCallout {
    float ax{},ay{},bx{},by{},dx{},dy{};
    const char* description="";
    const char* secondary="";
    float secondaryOffset{};
};
inline TrainingCallout TrainingCalloutFor(TrainingOverlay overlay,float left,float bottom,float right,float top) {
    float cx=(left+right)*.5f,cy=(bottom+top)*.5f;
    TrainingCallout c;
    auto anchor=[&](float x,float y,float dx,float dy,float length,const char* text){
        c={x,y,x+dx*length,y+dy*length,dx,dy,text,"",0};
    };
    switch(overlay) {
    case SpeedTrainingOverlay:anchor(cx-25,cy-105.5f,-1,-1,100,"SPEED (meters per second)");break;
    case MotionIndicatorTrainingOverlay:anchor(cx-10.5f,cy+10.5f,-1,1,150,"MOTION INDICATOR");break;
    case ReticuleTrainingOverlay:anchor(cx-10.5f,cy+10.5f,-1,1,150,"RETICULE");break;
    case ThrottleTrainingOverlay:anchor(cx-126.5f,cy+93.5f,-1,1,50,"THROTTLE GAUGE");c.secondary="SCROLL WHEEL ALSO CONTROLS THROTTLE";c.secondaryOffset=-140;break;
    case FuelTrainingOverlay:anchor(cx+126.5f,cy+93.5f,1,1,100,"FUEL GAUGE");c.secondary="PUSH R TO RELOAD";break;
    case EnergyTrainingOverlay:anchor(cx+126.5f,cy-112.5f,1,-1,50,"ENERGY GAUGE");break;
    case AmmoTrainingOverlay:anchor(cx-126.5f,cy-112.5f,-1,-1,50,"AMMUNITION GAUGE");break;
    case HullTrainingOverlay:anchor(cx-65.5f,cy+35.5f,-1,1,100,"ARMOR STRENGTH GAUGE");break;
    case ShieldTrainingOverlay:anchor(cx+65.5f,cy+35.5f,1,1,100,"SHIELD STRENGTH GAUGE");break;
    case ChatTrainingOverlay:anchor(cx-140,top-50,-1,-1,100,"CHAT PANE");break;
    case SectorTrainingOverlay:anchor(99,190,1,1,20,"SECTOR MAP");break;
    case InventoryTrainingOverlay:anchor(right-150,265,-1,1,50,"INVENTORY DISPLAY");break;
    case TargetTrainingOverlay:anchor(cx-70,120,-1,1,100,"TARGET HUD");break;
    case CommandTrainingOverlay:anchor(100,top-85,1,-1,50,"CURRENT COMMAND");break;
    case EyeballTrainingOverlay:anchor(cx+140.5f,cy-140.5f,1,-1,50,"DETECTION INDICATOR");break;
    case KillBonusTrainingOverlay:anchor(cx-15,cy-160.5f,-1,-.375f,70,"KILL BONUS");break;
    case RollTrainingOverlay:anchor(cx-110,cy-170,1,0,5,"PUSH Q AND E TO ROLL LEFT AND RIGHT");c.secondary="PUSH V TO TOGGLE MOUSE CONTROL";break;
    case BoosterTrainingOverlay:anchor(cx-70,cy-170,1,1,0,"SPACE TO BOOST");c.bx+=5;break;
    case TargetCenterTrainingOverlay:anchor(cx-144,cy-170,1,1,0,"CLICK RIGHT MOUSE BUTTON TO TARGET CENTER");c.bx+=5;break;
    case FireGunsTrainingOverlay:anchor(cx-75,cy-170,1,1,0,"LEFT CLICK TO FIRE GUNS");c.bx+=5;break;
    case TargetEnemyTrainingOverlay:anchor(cx-72,cy-170,1,1,0,"PUSH F TO TARGET ENEMY");c.bx+=5;break;
    case MissileTrainingOverlay:anchor(cx-140,cy-170,1,1,0,"PRESS 5 OR MIDDLE CLICK TO FIRE MISSILE");c.bx+=5;break;
    case CloakTrainingOverlay:anchor(cx-75,cy-170,1,1,0,"PUSH G TO TOGGLE CLOAK");c.bx+=5;break;
    default:break;
    }
    return c;
}
struct TrainingCalloutText {float cx{},cy{},x{},y{},secondaryX{},secondaryY{};};
inline TrainingCalloutText TrainingTextFor(const TrainingCallout& c,float width,float height) {
    float cx=c.bx+c.dx*width;
    float x=c.dx<0?cx:c.bx,y=c.dy<0?c.by+2:c.by-height-2;
    return {cx,c.by,x,y,x+c.secondaryOffset,y-height-4};
}
}
