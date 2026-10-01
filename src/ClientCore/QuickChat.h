// Original console quick-chat targeting, independent of menus and transport.
#pragma once
#include "../Igc/igc.h"
namespace Allegiance {
inline ImodelIGC* QuickChatTarget(IshipIGC* ship,CommandID& command,int mask,AbilityBitMask abilities) {
    if(command==c_cidMyAccepted)command=ship->GetCommandID(c_cmdAccepted);
    if(command==c_cidNone)return nullptr;
    ImodelIGC* model=nullptr;
    switch(mask) {
    case c_ttMyAccepted:case c_ttMyQueued:case c_ttMyTarget:model=ship->GetCommandTarget(char(mask));break;
    case c_ttWorstEnemy:{auto* track=ship->GetDamageTrack();auto* first=track?track->GetDamageBuckets()->first():nullptr;model=first?first->data()->model():nullptr;break;}
    case c_ttMe:model=ship;break;
    default:
        model=ship->GetCommandTarget(c_cmdCurrent);
        if(!model || model==ship || !FindableModel(model,ship->GetSide(),mask,abilities) || !ship->CanSee(model))
            model=FindTarget(ship,mask,nullptr,nullptr,nullptr,nullptr,abilities);
        break;
    }
    if(!model)command=c_cidNone;
    return model;
}
}
