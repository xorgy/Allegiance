#pragma once
#include "../Igc/igc.h"
#include "SoundIds.h"

namespace Allegiance {
// BaseClient::FM_S_KILL_SHIP presentation, before the victim's hull/side changes.
template<class Post>
void ShipLossFeedback(IshipIGC* victim,IshipIGC* launcher,bool died,ShipID viewer,SideID side,Post post) {
    auto type=victim->GetPilotType();
    bool important=type==c_ptBuilder || type==c_ptMiner || type==c_ptCarrier;
    auto* owner=victim->GetSide();const char* name=victim->GetName();
    if(!launcher) {
        if(victim->GetObjectID()==viewer)post(true,died?"You were killed":"You ejected");
        else if(important)post(owner->GetObjectID()==side,START_COLOR_STRING "%s" END_COLOR_STRING " was killed",(PCC)ConvertColorToString(owner->GetColor()),name);
        else post(false,died?START_COLOR_STRING "%s" END_COLOR_STRING " was killed":START_COLOR_STRING "%s" END_COLOR_STRING " ejected",(PCC)ConvertColorToString(owner->GetColor()),name);
    }else if(victim->GetObjectID()==viewer)
        post(true,died?"You were killed by " START_COLOR_STRING "%s" END_COLOR_STRING:"You were forced to eject by " START_COLOR_STRING "%s" END_COLOR_STRING,(PCC)ConvertColorToString(launcher->GetSide()->GetColor()),launcher->GetName());
    else if(launcher->GetObjectID()==viewer)
        post(true,died?"You killed " START_COLOR_STRING "%s" END_COLOR_STRING:"You forced " START_COLOR_STRING "%s" END_COLOR_STRING " to eject",(PCC)ConvertColorToString(owner->GetColor()),name);
    else if(important && owner->GetObjectID()!=side)
        post(true,died?START_COLOR_STRING "%s %s" END_COLOR_STRING " was killed by %s":START_COLOR_STRING "%s %s" END_COLOR_STRING " was forced to eject by %s",(PCC)ConvertColorToString(owner->GetColor()),owner->GetName(),name,launcher->GetName());
    else post(important,died?"%s was killed by %s":"%s was forced to eject by %s",name,launcher->GetName());
}
inline SoundID DroneLossSound(IshipIGC* ship) {
    switch(ship->GetPilotType()) {
    case c_ptCarrier:return carrierDestroyedSound;
    case c_ptMiner:return minerDestroyedSound;
    case c_ptBuilder:return ship->GetBaseData() && ship->GetBaseData()->GetObjectType()==OT_stationType?static_cast<IstationTypeIGC*>(ship->GetBaseData())->GetConstructorDestroyedSound():NA;
    case c_ptLayer:return ship->GetBaseData() && ship->GetBaseData()->GetObjectType()==OT_mineType?minefieldLayerDestroyedSound:towerLayerDestroyedSound;
    default:return towerLayerDestroyedSound;
    }
}
template<class Post>
void StationLossFeedback(IstationIGC* station,IshipIGC* launcher,Post post) {
    auto* owner=station->GetSide();
    if(launcher)post(true,START_COLOR_STRING "%s" END_COLOR_STRING " destroyed " START_COLOR_STRING "%s's %s" END_COLOR_STRING " in %s.",
        (PCC)ConvertColorToString(launcher->GetSide()->GetColor()),launcher->GetName(),(PCC)ConvertColorToString(owner->GetColor()),owner->GetName(),station->GetName(),station->GetCluster()->GetName());
    else post(true,START_COLOR_STRING "%s's %s" END_COLOR_STRING " in %s was destroyed.",(PCC)ConvertColorToString(owner->GetColor()),owner->GetName(),station->GetName(),station->GetCluster()->GetName());
}
// BaseClient::FM_S_GAIN_FLAG. Keep its historical side-zero victim gate.
template<class Post,class Sound>
void FlagPickupFeedback(IshipIGC* ship,SideID flag,IsideIGC* viewer,Post post,Sound sound) {
    auto* owner=ship->GetSide();
    if(owner==viewer) {
        if(flag==SIDE_TEAMLOBBY){post(true,"%s has found an artifact.",ship->GetName());sound(artifactFoundSound);}
        else {auto* victim=ship->GetMission()->GetSide(flag);post(true,"%s has stolen " START_COLOR_STRING "%s's" END_COLOR_STRING " flag.",ship->GetName(),(PCC)ConvertColorToString(victim->GetColor()),victim->GetName());sound(enemyFlagLostSound);}
    }else if(flag>0 && flag==viewer->GetObjectID()) {
        ZString color=ConvertColorToString(owner->GetColor());
        post(true,START_COLOR_STRING "%s" END_COLOR_STRING " of " START_COLOR_STRING "%s" END_COLOR_STRING " has stolen our flag.",(PCC)color,ship->GetName(),(PCC)color,owner->GetName());sound(flagLostSound);
    }
}
}
