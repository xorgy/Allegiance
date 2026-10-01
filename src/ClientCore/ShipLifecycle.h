// Platform/transport-independent operations from BaseClient, WinTrek and FedSrv.
#pragma once
#include "../Igc/igc.h"

namespace Allegiance {
// BaseClient::CreateTreasureLocal. Returns the cluster-owned object.
template<class RandomDirection>
ItreasureIGC* CreatePartTreasure(Time now,IshipIGC* ship,short amount,
        IpartTypeIGC* part,const Vector& position,float speed,float lifespan,
        RandomDirection randomDirection) {
    DataTreasureIGC data{};
    data.treasureCode=c_tcPart;data.treasureID=part->GetObjectID();
    data.lifespan=lifespan;data.amount=amount;data.createNow=false;
    data.objectID=ship->GetMission()->GenerateNewTreasureID();
    Vector direction=randomDirection();
    data.p0=position+(ship->GetRadius()+10.f)*direction;
    data.v0=direction*speed+ship->GetVelocity();
    data.clusterID=ship->GetCluster()->GetObjectID();data.time0=now;
    auto* treasure=static_cast<ItreasureIGC*>(ship->GetMission()->CreateObject(now,OT_treasure,&data,sizeof(data)));
    if(treasure)treasure->Release();return treasure;
}
inline bool TreasureAmount(IpartIGC* part,short& amount) {
    auto type=part->GetObjectType();amount=0;
    if(type==OT_magazine || type==OT_dispenser || type==OT_pack){amount=part->GetAmount();return amount!=0;}
    return true;
}
// Original local/server drop probability and ordering. The adapter owns drops,
// score, flags, child ships and the subsequent pod/death transition.
template<class RandomInt,class Drop>
void DropShipInventory(IshipIGC* ship,RandomInt randomInt,Drop drop) {
    while(auto* link=ship->GetParts()->first()) {
        auto* part=link->data();
        if(randomInt(0,4)==0){short amount;if(TreasureAmount(part,amount))drop(amount,part->GetPartType(),60.f);}
        part->Terminate();
    }
    short ammo=ship->GetAmmo();
    if(ammo>0 && randomInt(0,4)==0)drop(ammo,ship->GetMission()->GetAmmoPack(),30.f);
    short fuel=short(ship->GetFuel());
    if(fuel>0 && randomInt(0,4)==0)drop(fuel,ship->GetMission()->GetFuelPack(),30.f);
}
// FedSrv's ejection kinematics, including its coincident-impact guards. Keep the
// same IGC ship/ID; a player ejection is a hull change, not an object replacement.
template<class RandomDirection>
void EjectIntoPod(Time now,IshipIGC* ship,ImodelIGC* credit,const Vector& hit,
        const Vector& impact,RandomDirection randomDirection) {
    TRef<IshipIGC> parent=ship->GetParentShip();auto* cluster=ship->GetCluster();
    Vector velocity=ship->GetSourceShip()->GetVelocity();
    if(cluster)for(auto* l=cluster->GetMissiles()->first();l;l=l->next())
        if(l->data()->GetTarget()==ship)l->data()->SetTarget(nullptr);
    if(parent)ship->SetParentShip(nullptr);
    auto* pod=ship->GetSide()->GetCivilization()->GetLifepod();ship->SetBaseHullType(pod);
    Vector direction=randomDirection(),position;Orientation orientation;
    if(parent) {
        orientation.Set(direction);
        position=hit-direction*(40.f+.5f*pod->GetLength()+parent->GetRadius());
    } else {
        ImodelIGC* launcher=credit && credit->GetObjectType()==OT_ship?static_cast<IshipIGC*>(credit)->GetSourceShip():credit;
        if(!launcher || launcher->GetCluster()==cluster) {
            Vector delta=(launcher?launcher->GetPosition():impact)-hit;float length2=delta.LengthSquared();
            if(length2>=1.f) {
                orientation.Set(delta);Vector biased=direction+delta*(8.f/std::sqrt(length2));length2=biased.LengthSquared();
                if(length2>.01f)direction=biased/std::sqrt(length2);
            } else orientation.Set(direction);
        } else orientation.Set(direction);
        position=hit;
    }
    ship->SetPosition(position);ship->SetVelocity(velocity-direction*100.f);ship->SetOrientation(orientation);
    ship->SetCurrentTurnRate(c_axisRoll,pi*2.f);ship->SetBB(now,now,0.f);
}
// Original offline wingman resupply decision. Preserve its accepted order while
// temporarily diverting the plan; IGC resumes it after repair/reload.
inline ImodelIGC* RequestAmmoResupply(IshipIGC* ship) {
    if(ship->GetPilotType()!=c_ptWingman || !ship->GetCluster())return nullptr;
    int mask=c_ttFriendly|c_ttStation|c_ttNearest|c_ttAnyCluster;
    if(ship->GetHullType()->HasCapability(c_habmLandOnCarrier))mask|=c_ttShip;
    auto* target=FindTarget(ship,mask,nullptr,nullptr,nullptr,nullptr,c_sabmReload);
    if(target){ship->SetCommand(c_cmdPlan,target,c_cidGoto);ship->SetGettingAmmo(true);}
    return target;
}
// Original station boarding authority, including friendly-fire allied capture.
inline bool ShouldBoardStation(IshipIGC* ship,IstationIGC* station) {
    auto* side=ship->GetSide();auto* owner=station->GetSide();auto* hull=ship->GetHullType();
    return side!=owner && hull && hull->HasCapability(c_habmBoard) &&
        (!IsideIGC::AlliedSides(side,owner) || ship->GetMission()->GetMissionParams()->bAllowFriendlyFire);
}
// FedSrv rescue: closest own restart, then first own restart. Allied pod docking
// uses the last own station instead; an allied or captured station cannot rescue.
inline IstationIGC* SelectRestartStation(IshipIGC* ship,StationID preferred=NA,bool nearest=true) {
    auto* mission=ship->GetMission();auto* side=ship->GetSide();
    if(preferred!=NA)if(auto* station=mission->GetStation(preferred))if(station->GetSide()==side)return station;
    if(nearest) {
        auto* station=static_cast<IstationIGC*>(FindTarget(ship,c_ttStation|c_ttFriendly|c_ttAnyCluster|c_ttNearest,
            nullptr,nullptr,nullptr,nullptr,c_sabmRestart));
        if(station && station->GetSide()==side)return station;
    }
    for(auto* l=side->GetStations()->first();l;l=l->next())if(l->data()->GetStationType()->HasCapability(c_sabmRestart))return l->data();
    return nullptr;
}
// FedSrv continuation rule (the local WinTrek stub unconditionally returned true).
inline bool CanContinueRipcord(IshipIGC* ship,ImodelIGC* target) {
    if(!target || !target->GetCluster() || ship->GetFlag()!=NA)return false;
    bool allied=ship->GetMission()->GetMissionParams()->bAllowAlliedRip;
    if(ship->GetSide()!=target->GetSide() && !(allied && IsideIGC::AlliedSides(ship->GetSide(),target->GetSide())))return false;
    if(target->GetObjectType()!=OT_ship)return true;
    auto* hull=static_cast<IshipIGC*>(target)->GetBaseHullType();auto* source=ship->GetBaseHullType();
    return hull && source && hull->HasCapability(source->HasCapability(c_habmCanLtRipcord)?
        (c_habmIsRipcordTarget|c_habmIsLtRipcordTarget):c_habmIsRipcordTarget);
}
// WinTrek/FedSrv non-station arrival. Energy authorization is the caller's job.
template<class RandomDirection>
void ApplyRipcordArrival(IshipIGC* ship,ImodelIGC* target,float exitSpeed,RandomDirection randomDirection) {
    if(target->GetObjectType()==OT_station){static_cast<IstationIGC*>(target)->Launch(ship);return;}
    Vector direction=randomDirection();Orientation orientation(direction);auto* cluster=target->GetCluster();Time now=cluster->GetLastUpdate();
    ship->SetPosition(target->GetPosition()+direction*(target->GetRadius()+ship->GetRadius()+25.f));
    ship->SetVelocity(direction*exitSpeed);ship->SetOrientation(orientation);
    ship->SetCurrentTurnRate(c_axisYaw,0);ship->SetCurrentTurnRate(c_axisPitch,0);ship->SetCurrentTurnRate(c_axisRoll,0);
    ship->SetLastUpdate(now);ship->SetBB(now,now,0);ship->SetCluster(cluster);
}
// WinTrek/FedSrv carrier launch geometry. Loadout restoration precedes this.
template<class Random>
void LaunchFromCarrier(IshipIGC* carrier,IshipIGC* ship,float exitSpeed,Random random) {
    const auto& orientation=carrier->GetOrientation();auto* hull=carrier->GetBaseHullType();
    Vector position=carrier->GetPosition(),velocity=carrier->GetVelocity();
    position.x+=random(-.5f,.5f);position.y+=random(-.5f,.5f);position.z+=random(-.5f,.5f);
    Orientation launch=orientation;
    if(hull->GetLaunchSlots()==0) {
        position-=orientation.GetBackward()*(carrier->GetRadius()+ship->GetRadius()+exitSpeed*.5f);
        velocity-=orientation.GetBackward()*exitSpeed;
    } else {
        short slot=carrier->GetLaunchSlot();position+=hull->GetLaunchPosition(slot)*orientation;
        Vector forward=hull->GetLaunchDirection(slot)*orientation;
        position+=forward*(ship->GetRadius()+exitSpeed*.5f);velocity+=forward*exitSpeed;launch.TurnTo(forward);
    }
    ship->SetPosition(position);ship->SetVelocity(velocity);ship->SetOrientation(launch);
}
inline void ResumeAfterCarrier(IshipIGC* carrier,IshipIGC* ship) {
    if(!ship->GetAutopilot())return;
    if(ship->GetCommandTarget(c_cmdAccepted)==carrier && ship->GetCommandID(c_cmdAccepted)==c_cidGoto) {
        ship->SetCommand(c_cmdAccepted,carrier,c_cidDefend);
        if(ship->GetPilotType()>=c_ptPlayer)ship->SetCommand(c_cmdPlan,carrier,c_cidDefend);
    } else ship->SetCommand(c_cmdPlan,nullptr,c_cidNone);
}
}
