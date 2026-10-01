// Frozen 39ef2c072b1eac0e9f97ab1428bc96572cbd5854 Orientation::Set policy.
// Original imports normalize/reorthogonalize axes; source file bytes can change
// by a few ULPs even in the original client. Keep this independent of Set().
#pragma once
#include "../../Igc/igc.h"
namespace OriginalDynamicImport {
inline void axes(Vector& forward,Vector& up) {
    auto cross=[](const Vector& a,const Vector& b){return Vector(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);};
    Vector right=cross(forward,up);float lRight=right.Length(),lForward=forward.Length();up=cross(right,forward);float lUp=up.Length();
    if(lRight!=0 && lForward!=0 && lUp!=0){forward/=lForward;up/=lUp;}
    else {forward=Vector(0,0,-1);up=Vector(0,1,0);}
}
inline void normalize(ObjectType type,char* payload) {
    size_t f=0,u=0;
    if(type==OT_asteroid){f=offsetof(DataAsteroidIGC,forward);u=offsetof(DataAsteroidIGC,up);}
    else if(type==OT_station){f=offsetof(DataStationIGC,forward);u=offsetof(DataStationIGC,up);}
    if(f){Vector forward,up;memcpy(&forward,payload+f,12);memcpy(&up,payload+u,12);axes(forward,up);memcpy(payload+f,&forward,12);memcpy(payload+u,&up,12);}
    if(type==OT_warp){Vector forward;memcpy(&forward,payload+offsetof(DataWarpIGC,forward),12);forward/=forward.Length();memcpy(payload+offsetof(DataWarpIGC,forward),&forward,12);}
    if(type==OT_cluster)payload[offsetof(DataClusterIGC,activeF)]=0; // original Export deliberately makes clusters inactive
    if(type==OT_asteroid){
        // Original BytePercentage converts through a float with divisor 240.
        // Its assignment clamps out-of-range authored bytes (241..255) to 240.
        auto& byte=payload[offsetof(DataAsteroidIGC,fraction)];
        float fraction=float(static_cast<unsigned char>(byte))/240.0f;
        float rounded=fraction*240.0f+0.5f;
        byte=static_cast<char>(rounded<0.0f?0:rounded>=241.0f?240:static_cast<unsigned char>(rounded));
    }
}
}
