// Boarding policy from FedSrv::BoardShip/LeaveShip. No transport or UI state.
#pragma once
#include "../Igc/igc.h"

namespace Allegiance {
// Observers are children too, but TeleportPane/TeamPane count only gunners.
// Walk the parent's crew rather than searching the entire faction roster.
inline int MannedTurrets(const IshipIGC* parent) {
    int count=0;
    for(auto* l=parent->GetChildShips()->first();l;l=l->next())
        if(l->data()->GetTurretID()!=NA)++count;
    return count;
}
// WinTrek's turret shortcuts count from the first non-fixed hardpoint.
inline Mount TurretShortcutMount(const IhullTypeIGC* hull,int index) {
    if(!hull || index<0 || index>=4)return NA;
    Mount mount=Mount(hull->GetMaxFixedWeapons()+index);
    return mount<hull->GetMaxWeapons()?mount:NA;
}
inline Mount NextTurret(IshipIGC* passenger,IshipIGC* parent) {
    const auto* hull=parent->GetBaseHullType();
    if(!hull)return NA;
    const Mount first=hull->GetMaxFixedWeapons(),last=hull->GetMaxWeapons();
    Mount mount=passenger->GetTurretID();
    if(mount<first || mount>=last)mount=last-1;
    // Bound the search even when transferring from a hull with different mounts.
    for(int n=0;n<last-first;++n) {
        if(++mount>=last)mount=first;
        bool occupied=false;
        for(auto* l=parent->GetChildShips()->first();l;l=l->next())
            if(l->data()->GetTurretID()==mount){occupied=true;break;}
        if(!occupied && parent->GetMountedPart(ET_Weapon,mount))return mount;
    }
    return NA;
}
// TeleportPane::DisembarkAndBoard releases the old seat before BoardShip.
// Preflight the resulting first-free search without detaching on rejection.
// Keep NextTurret's in-flight cycle and WinTrek's existing path unchanged.
inline Mount DockedBoardTurret(IshipIGC* passenger,IshipIGC* parent) {
    const auto* hull=parent->GetBaseHullType();
    if(!hull)return NA;
    for(Mount mount=hull->GetMaxFixedWeapons();mount<hull->GetMaxWeapons();++mount) {
        if(!parent->GetMountedPart(ET_Weapon,mount))continue;
        bool occupied=false;
        for(auto* l=parent->GetChildShips()->first();l;l=l->next())
            if(l->data()!=passenger && l->data()->GetTurretID()==mount){occupied=true;break;}
        if(!occupied)return mount;
    }
    return NA;
}
inline bool CanBoardShip(IshipIGC* passenger,IshipIGC* parent) {
    if(!passenger || !parent || passenger==parent || parent->GetParentShip() ||
       !parent->GetBaseHullType() || passenger->GetSide()!=parent->GetSide() ||
       passenger->GetPilotType()<c_ptPlayer || parent->GetPilotType()<c_ptPlayer ||
       passenger->IsGhost() || parent->IsGhost())return false;
    return passenger->GetParentShip()==parent ||
        (!passenger->GetCluster() && passenger->GetStation() && !parent->GetCluster() && parent->GetStation());
}
inline bool LeaveDockedShip(IshipIGC* passenger) {
    auto* parent=passenger->GetParentShip();
    if(!parent || parent->GetCluster() || !parent->GetStation())return false;
    passenger->SetParentShip(nullptr);
    passenger->SetBaseHullType(passenger->GetSide()->GetCivilization()->GetLifepod());
    return true;
}
// Original turret slew/FOV relationship; throttle controls zoom while gunning.
inline float TurretFieldOfView(float throttle) {
    return RadiansFromDegrees(5.f)+(RadiansFromDegrees(50.f)-RadiansFromDegrees(5.f))*.5f*(throttle+1.f);
}
}
