// WinTrek indicator.cpp weapon/target policy, relay availability and aim colors.
#pragma once
#include "../Igc/igc.h"
#include "AimIndicator.h"
namespace Allegiance {
inline bool WeaponIndicatorTarget(IweaponIGC* weapon,ImodelIGC* target,IsideIGC* side,bool external) {
    return weapon && (external || (((target->GetSide()==side) || IsideIGC::AlliedSides(target->GetSide(),side))==(weapon->GetProjectileType()->GetPower()<0.f)));
}
inline bool HasLeadIndicator(IshipIGC* ship,ImodelIGC* target,bool turret) {
    if(turret || ship->GetBaseHullType()->HasCapability(c_habmLeadIndicator))return true;
    auto* cluster=ship->GetCluster();auto* side=ship->GetSide();
    for(auto* l=cluster->GetStations()->first();l;l=l->next()) {
        auto* station=l->data();
        if(station->GetSide()==side && station->GetBaseStationType()->HasCapability(c_sabmRemoteLeadIndicator) && station->InScannerRange(target))return true;
    }
    for(auto* l=cluster->GetShips()->first();l;l=l->next()) {
        auto* relay=l->data();
        if(relay->GetSide()==side && !relay->GetParentShip() && relay->GetBaseHullType()->HasCapability(c_habmRemoteLeadIndicator) && relay->InScannerRange(target))return true;
    }
    return false;
}
}
