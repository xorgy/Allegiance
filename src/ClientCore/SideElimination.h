// FedSrv CFSMission::DeactivateSide ship cleanup. Adapters retain side flags,
// player status/transport and victory evaluation.
#pragma once
#include "../Igc/igc.h"
namespace Allegiance {
template<class Kill,class Exit>
inline void EliminateSideShips(IsideIGC* side,Kill kill,Exit exit) {
    const ShipListIGC* ships=side->GetShips();
    for(auto* link=ships->first();link;) {
        auto* ship=link->data();link=link->next();if(!ship)break;
        // Passengers own no hull, ammunition or fuel capacities.
        if(ship->GetBaseHullType()) {
            while(auto* part=ship->GetParts()->first())part->data()->Terminate();
            ship->SetAmmo(0);ship->SetFuel(0);
        }
        if(ship->GetPilotType()<c_ptPlayer)kill(ship,0.f);
        else {
            if(!ship->GetParentShip() && ship->GetCluster())kill(ship,-1.f);
            ship->Reset(false);exit(ship);
        }
    }
}
}
