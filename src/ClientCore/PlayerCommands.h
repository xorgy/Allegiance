// WinTrek player command transitions. Presentation, acknowledgements and
// transport stay in caller adapters; IGC owns the four command slots.
#pragma once
#include "../Igc/igc.h"
namespace Allegiance {
// WinTrek ChangeCluster: a sector buoy is an arrival order, unlike a waypoint
// or an object target. Clear every matching command slot, including queued.
inline void ClearArrivedSectorOrders(IshipIGC* ship,IclusterIGC* cluster) {
    for(Command slot=0;slot<c_cmdMax;++slot) {
        ImodelIGC* target=ship->GetCommandTarget(slot);
        if(target && target->GetObjectType()==OT_buoy &&
           static_cast<IbuoyIGC*>(target)->GetBuoyType()==c_buoyCluster &&
           target->GetCluster()==cluster)ship->SetCommand(slot,nullptr,c_cidNone);
    }
}
inline CommandID SetCurrentTarget(IshipIGC* ship,ImodelIGC* model,CommandID command=c_cidDefault) {
    if(command==c_cidDefault)command=model?ship->GetDefaultOrder(model):c_cidNone;
    ship->SetCommand(c_cmdCurrent,model,command);return command;
}
template<class Sound,class Join,class Rescue,class Select,class Accept,class Reachable,class Engage>
bool AcceptQueuedOrder(IshipIGC* ship,bool execute,Sound sound,Join join,Rescue rescue,Select select,Accept accept,Reachable reachable,Engage engage) {
    CommandID command=ship->GetCommandID(c_cmdQueued);
    if(command==c_cidNone)return false;
    ImodelIGC* model=ship->GetCommandTarget(c_cmdQueued);
    if(!model || !ship->LegalCommand(command,model))return false;
    sound();
    if(command==c_cidJoin) {
        if(model->GetObjectType()==OT_ship)join(static_cast<IshipIGC*>(model));
    } else {
        if(command==c_cidPickup && model->GetObjectType()==OT_ship && ship->GetBaseHullType()->HasCapability(c_habmRescue))rescue(static_cast<IshipIGC*>(model));
        select(model,command);accept(model,command);
        if(execute && !ship->GetStation() && !ship->GetParentShip() && reachable(model))engage();
    }
    ship->SetCommand(c_cmdQueued,nullptr,c_cidNone);return true;
}
inline float MatchTargetThrottle(IshipIGC* ship,ImodelIGC* model) {
    if(ship->GetParentShip())return 0.f;
    float speed=model->GetVelocity().Length(),maximum=ship->GetHullType()->GetMaxSpeed();
    return speed>=maximum?1.f:2.f*speed/maximum-1.f;
}
}
