// FedSrv's accepted player orders, independent of chat/packet transport.
#pragma once
#include "../Igc/igc.h"
namespace Allegiance {
inline CommandID ResolveDefaultOrder(IshipIGC* ship,ImodelIGC* target,CommandID command) {
    return command==c_cidDefault && target?ship->GetDefaultOrder(target):command;
}
inline void RememberMinerOrder(IshipIGC* ship,ImodelIGC* target,Time now,IclusterIGC* miningCluster=nullptr) {
    if(ship->GetPilotType()!=c_ptMiner)return;
    if(target && target->GetObjectType()!=OT_station && target->GetObjectType()!=OT_ship) {
        DataBuoyIGC data{};data.position=Vector::GetZero();data.type=c_buoyWaypoint;
        data.clusterID=(miningCluster?miningCluster:target->GetCluster())->GetObjectID();
        auto* mission=ship->GetMission();data.buoyID=mission->GenerateNewBuoyID();
        auto* buoy=static_cast<IbuoyIGC*>(mission->CreateObject(now,OT_buoy,&data,sizeof(data)));
        if(buoy){ship->SetCommand(c_cmdQueued,buoy,miningCluster?c_cidJoin:c_cidGoto);buoy->Release();}
    } else ship->SetCommand(c_cmdQueued,nullptr,c_cidGoto);
}
// Caller validates ownership, discovery and LegalCommand before applying.
inline void ApplyAcceptedOrder(IshipIGC* ship,ImodelIGC* target,CommandID command) {
    ship->SetRunawayCheckCooldown(120.f);ship->SetStayDocked(false);
    if(command==c_cidJoin && target && target->GetObjectType()==OT_ship)
        ship->SetWingID(static_cast<IshipIGC*>(target)->GetWingID());
    else ship->SetCommand(c_cmdAccepted,target,command);
}
// FedSrv FM_CS_ORDER_CHANGE's drone return/stay-docked action is distinct
// from a normal accepted move/mine/build command or a hold in space.
inline bool SetDroneStandby(IshipIGC* ship,bool stay) {
    if(!ship || ship->GetPilotType()>=c_ptPlayer || ship->GetParentShip())return false;
    if(auto* station=ship->GetStation()) {
        ship->SetStayDocked(stay);
        if(!stay)ship->PickDefaultOrder(station->GetCluster(),station->GetPosition(),true);
        return true;
    }
    if(!stay || !ship->GetCluster())return false;
    ImodelIGC* station=nullptr;
    if(ship->GetPilotType()==c_ptCarrier)
        station=FindTarget(ship,c_ttFriendly|c_ttStation|c_ttNearest|c_ttAnyCluster,nullptr,nullptr,nullptr,nullptr,c_sabmCapLand);
    if(!station)station=FindTarget(ship,c_ttFriendly|c_ttStation|c_ttNearest|c_ttAnyCluster,nullptr,nullptr,nullptr,nullptr,c_sabmRepair);
    if(!station)return false;
    ship->SetStayDocked(true);ship->SetCommand(c_cmdQueued,nullptr,c_cidNone);
    ship->SetCommand(c_cmdAccepted,station,c_cidGoto);return true;
}
}
