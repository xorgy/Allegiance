// FedSrv FM_C_DOCKED station transfer. Callers retain acknowledgement, money
// packets, player status and client-side loadout purchases.
#pragma once
#include "../Igc/igc.h"
namespace Allegiance {
inline bool CanTeleportToStation(IsideIGC* side,IstationIGC* station){
    return station && (station->GetSide()==side || IsideIGC::AlliedSides(side,station->GetSide())) &&
        station->GetStationType()->HasCapability(c_sabmRestart);
}
template<class LeaveChild,class Refund,class HullChanged>
inline bool ApplyStationTeleport(IshipIGC* ship,IstationIGC* station,bool newHull,LeaveChild leave,Refund refund,HullChanged changed){
    if(newHull){
        while(auto* child=ship->GetChildShips()->first())leave(child->data());
        Money value=ship->GetValue();assert(value>=0);if(value>0)refund(value);
        while(auto* part=ship->GetParts()->first())part->data()->Terminate();
        auto* pod=ship->GetSide()->GetCivilization()->GetLifepod();ship->SetBaseHullType(pod);changed(pod);
    }
    for(auto* child=ship->GetChildShips()->first();child;child=child->next())child->data()->SetStation(station);
    ship->SetStation(station);return newHull;
}
}
