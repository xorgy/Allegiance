// Flag/artifact lifetime from FedSrv. Adapters provide RNG and end-game/UI events.
#pragma once
#include "../Igc/igc.h"
namespace Allegiance {
template<class RandomInt>
bool RespawnFlag(ImissionIGC* mission,Time now,SideID flag,RandomInt randomInt) {
    if(flag==NA)return true; // A picked-up treasure no longer owns its flag.
    if(flag==SIDE_TEAMLOBBY) {
        int count=0;for(auto* l=mission->GetClusters()->first();l;l=l->next())if(!l->data()->GetHomeSector())++count;
        if(!count)return false;int index=randomInt(0,count-1);
        for(auto* l=mission->GetClusters()->first();l;l=l->next())if(!l->data()->GetHomeSector() && index--==0) {
            mission->GenerateTreasure(now,l->data(),-2);return true;
        }
        return false;
    }
    auto* side=mission->GetSide(flag);if(!side)return false;
    auto available=[&](IstationIGC* station) {
        if(!station->GetStationType()->HasCapability(c_sabmPedestal))return false;
        for(auto* l=station->GetCluster()->GetTreasures()->first();l;l=l->next()) {
            auto* treasure=l->data();
            if(treasure->GetTreasureCode()==c_tcFlag && treasure->GetTreasureID()==flag &&
               (treasure->GetPosition()-station->GetPosition()).LengthSquared()<300.f*300.f)return false;
        }
        return true;
    };
    int count=0;for(auto* l=side->GetStations()->first();l;l=l->next())if(available(l->data()))++count;
    if(!count)return false;int index=randomInt(0,count-1);
    for(auto* l=side->GetStations()->first();l;l=l->next())if(available(l->data()) && index--==0) {
        auto* station=l->data();DataTreasureIGC data{};data.treasureCode=c_tcFlag;data.treasureID=flag;
        data.clusterID=station->GetCluster()->GetObjectID();data.lifespan=10.f*24.f*3600.f;data.createNow=false;
        data.objectID=mission->GenerateNewTreasureID();data.p0=station->GetPosition();data.p0.z+=station->GetRadius()+c_fFlagOffset;
        data.v0=Vector::GetZero();data.time0=now;
        auto* object=mission->CreateObject(now,OT_treasure,&data,sizeof(data));if(object)object->Release();return object!=nullptr;
    }
    return false;
}
// Returns false when the original universe-boundary rule requires a respawn.
template<class RandomDirection>
bool DropShipFlag(IshipIGC* ship,Time now,const Vector& impact,RandomDirection randomDirection) {
    SideID flag=ship->GetFlag();if(flag==NA)return true;ship->SetFlag(NA);
    auto* mission=ship->GetMission();const auto& p=ship->GetPosition();
    float lens=mission->GetFloatConstant(c_fcidLensMultiplier),radius=1.5f*mission->GetFloatConstant(c_fcidRadiusUniverse);
    if(p.x*p.x+p.y*p.y+p.z*p.z/(lens*lens)>radius*radius)return false;
    DataTreasureIGC data{};data.treasureCode=c_tcFlag;data.treasureID=flag;data.lifespan=10.f*24.f*3600.f;
    data.objectID=mission->GenerateNewTreasureID();data.clusterID=ship->GetCluster()->GetObjectID();data.time0=now;
    Vector direction=randomDirection();data.p0=impact+(ship->GetRadius()+10.f)*direction;data.v0=ship->GetVelocity()+direction*100.f;
    if(auto* object=mission->CreateObject(now,OT_treasure,&data,sizeof(data)))object->Release();return true;
}
inline bool SecureFlag(IshipIGC* ship) {
    auto flag=ship->GetFlag();if(flag==NA)return false;
    auto* side=ship->GetSide();ship->SetFlag(NA);const auto* params=ship->GetMission()->GetMissionParams();
    if(flag==SIDE_TEAMLOBBY){side->SetArtifacts(side->GetArtifacts()+1);return (params->IsArtifactsGame() || params->IsFlagsGame()) && side->GetArtifacts()>=params->nGoalArtifactsCount;}
    side->SetFlags(side->GetFlags()+1);return (params->IsArtifactsGame() || params->IsFlagsGame()) && side->GetFlags()>=params->nGoalFlagsCount;
}
}
