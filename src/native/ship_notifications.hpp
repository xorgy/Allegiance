#pragma once
#include "../ClientCore/ShipSound.h"

namespace Native {
// Invert WinTrek's per-player predicate through IGC's crew relationship.
// Visitors must not mutate that relationship while traversing it.
template<class Visitor>
void visitShipNotificationRecipients(ImodelIGC* model, Visitor visit) {
    if(!model || model->GetObjectType()!=OT_ship)return;
    auto* ship=static_cast<IshipIGC*>(model);
    visit(ship);
    for(auto* link=ship->GetChildShips()->first();link;link=link->next())
        if(Allegiance::ReceivesShipNotification(link->data(),model))visit(link->data());
}
}
