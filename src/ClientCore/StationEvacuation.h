// FedSrv CFSMission::VacateStation. The caller keeps transport/loadout updates.
#pragma once
#include "../Igc/igc.h"
namespace Allegiance {
template<class Launched>
inline void VacateStation(IstationIGC* station,Launched launched) {
    IsideIGC* ghostSide=nullptr;
    const ShipListIGC* ships=station->GetShips();
    auto* link=ships->first();
    while(link) {
        auto* ship=link->data();
        if(!ship->GetParentShip()) {
            if(ship->IsGhost()) {
                ghostSide=ship->GetSide();link=link->next();
            } else {
                for(auto* child=ship->GetChildShips()->first();child;child=child->next())
                    child->data()->SetStation(nullptr);
                ship->SetStation(nullptr);launched(ship);link=ships->first();
            }
        } else link=link->next();
    }
    if(ghostSide)for(auto* base=ghostSide->GetStations()->first();base;base=base->next()) {
        auto* destination=base->data();
        if(destination!=station && destination->GetBaseStationType()->HasCapability(c_sabmRestart)) {
            auto* ghost=ships->first();
            while(ghost) {
                auto* ship=ghost->data();ghost=ghost->next();
                assert(!ship->GetParentShip() && ship->IsGhost());
                ship->SetStation(destination);
            }
            break;
        }
    }
}
}
