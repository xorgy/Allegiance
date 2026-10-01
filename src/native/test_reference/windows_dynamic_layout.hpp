// Frozen original dynamic record layout: clang i686-pc-windows-msvc.
// Verifies pointer-free fields, not runtime pointers or a transport protocol.
#pragma once
#include "../../Igc/igc.h"
#include <cstddef>
#include <cstring>
#include <stdexcept>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
namespace WindowsDynamicLayout {
static_assert(sizeof(AsteroidDef)==60);
static_assert(offsetof(AsteroidDef,ore)==0);
static_assert(offsetof(AsteroidDef,oreMax)==4);
static_assert(offsetof(AsteroidDef,aabmCapabilities)==8);
static_assert(offsetof(AsteroidDef,asteroidID)==10);
static_assert(offsetof(AsteroidDef,hitpoints)==12);
static_assert(offsetof(AsteroidDef,radius)==16);
static_assert(offsetof(AsteroidDef,modelName)==18);
static_assert(offsetof(AsteroidDef,textureName)==31);
static_assert(offsetof(AsteroidDef,iconName)==44);
inline void cleanAsteroidDef(char* p) {
    {bool end=false;for(int i=18;i<31;++i){if(end)p[i]=0;else end=p[i]==0;}}
    {bool end=false;for(int i=31;i<44;++i){if(end)p[i]=0;else end=p[i]==0;}}
    {bool end=false;for(int i=44;i<57;++i){if(end)p[i]=0;else end=p[i]==0;}}
    p[57]=0;
    p[58]=0;
    p[59]=0;
}
static_assert(sizeof(WarpDef)==32);
static_assert(offsetof(WarpDef,warpID)==0);
static_assert(offsetof(WarpDef,destinationID)==2);
static_assert(offsetof(WarpDef,radius)==4);
static_assert(offsetof(WarpDef,textureName)==6);
static_assert(offsetof(WarpDef,iconName)==19);
inline void cleanWarpDef(char* p) {
    {bool end=false;for(int i=6;i<19;++i){if(end)p[i]=0;else end=p[i]==0;}}
    {bool end=false;for(int i=19;i<32;++i){if(end)p[i]=0;else end=p[i]==0;}}
}
static_assert(sizeof(PartData)==5);
static_assert(offsetof(PartData,partID)==0);
static_assert(offsetof(PartData,amount)==2);
static_assert(offsetof(PartData,mountID)==4);
inline void cleanPartData(char* p) {
}
static_assert(sizeof(DataProjectileIGC)==32);
static_assert(offsetof(DataProjectileIGC,velocity)==0);
static_assert(offsetof(DataProjectileIGC,forward)==12);
static_assert(offsetof(DataProjectileIGC,lifespan)==24);
static_assert(offsetof(DataProjectileIGC,projectileTypeID)==28);
inline void cleanDataProjectileIGC(char* p) {
    p[30]=0;
    p[31]=0;
}
static_assert(sizeof(DataBuoyIGC)==20);
static_assert(offsetof(DataBuoyIGC,position)==0);
static_assert(offsetof(DataBuoyIGC,clusterID)==12);
static_assert(offsetof(DataBuoyIGC,type)==14);
static_assert(offsetof(DataBuoyIGC,buoyID)==16);
static_assert(offsetof(DataBuoyIGC,visible)==18);
inline void cleanDataBuoyIGC(char* p) {
    p[15]=0;
    p[19]=0;
}
static_assert(sizeof(DataClusterIGC)==92);
static_assert(offsetof(DataClusterIGC,starSeed)==0);
static_assert(offsetof(DataClusterIGC,lightDirection)==4);
static_assert(offsetof(DataClusterIGC,lightColor)==16);
static_assert(offsetof(DataClusterIGC,screenX)==20);
static_assert(offsetof(DataClusterIGC,screenY)==24);
static_assert(offsetof(DataClusterIGC,clusterID)==28);
static_assert(offsetof(DataClusterIGC,nDebris)==30);
static_assert(offsetof(DataClusterIGC,nStars)==32);
static_assert(offsetof(DataClusterIGC,name)==34);
static_assert(offsetof(DataClusterIGC,posterName)==59);
static_assert(offsetof(DataClusterIGC,planetName)==72);
static_assert(offsetof(DataClusterIGC,planetSinLatitude)==85);
static_assert(offsetof(DataClusterIGC,planetLongitude)==86);
static_assert(offsetof(DataClusterIGC,planetRadius)==87);
static_assert(offsetof(DataClusterIGC,activeF)==88);
static_assert(offsetof(DataClusterIGC,bHomeSector)==89);
inline void cleanDataClusterIGC(char* p) {
    {bool end=false;for(int i=34;i<59;++i){if(end)p[i]=0;else end=p[i]==0;}}
    {bool end=false;for(int i=59;i<72;++i){if(end)p[i]=0;else end=p[i]==0;}}
    {bool end=false;for(int i=72;i<85;++i){if(end)p[i]=0;else end=p[i]==0;}}
    p[90]=0;
    p[91]=0;
}
static_assert(sizeof(DataAsteroidIGC)==144);
static_assert(offsetof(DataAsteroidIGC,signature)==0);
static_assert(offsetof(DataAsteroidIGC,position)==4);
static_assert(offsetof(DataAsteroidIGC,up)==16);
static_assert(offsetof(DataAsteroidIGC,forward)==28);
static_assert(offsetof(DataAsteroidIGC,rotation)==40);
static_assert(offsetof(DataAsteroidIGC,asteroidDef)==56);
static_assert(offsetof(DataAsteroidIGC,clusterID)==116);
static_assert(offsetof(DataAsteroidIGC,name)==118);
static_assert(offsetof(DataAsteroidIGC,fraction)==143);
inline void cleanDataAsteroidIGC(char* p) {
    cleanAsteroidDef(p+56);
    {bool end=false;for(int i=118+(p[118]==0);i<143;++i){if(end)p[i]=0;else end=p[i]==0;}}
}
static_assert(sizeof(DataSideIGC)==276);
static_assert(offsetof(DataSideIGC,name)==0);
static_assert(offsetof(DataSideIGC,civilizationID)==26);
static_assert(offsetof(DataSideIGC,sideID)==28);
static_assert(offsetof(DataSideIGC,ttbmInitialTechs)==30);
static_assert(offsetof(DataSideIGC,ttbmDevelopmentTechs)==80);
static_assert(offsetof(DataSideIGC,gasAttributes)==132);
static_assert(offsetof(DataSideIGC,color)==232);
static_assert(offsetof(DataSideIGC,fTimeEndured)==248);
static_assert(offsetof(DataSideIGC,nFlags)==252);
static_assert(offsetof(DataSideIGC,nArtifacts)==254);
static_assert(offsetof(DataSideIGC,nKills)==256);
static_assert(offsetof(DataSideIGC,nEjections)==258);
static_assert(offsetof(DataSideIGC,nDeaths)==260);
static_assert(offsetof(DataSideIGC,nBaseKills)==262);
static_assert(offsetof(DataSideIGC,nBaseCaptures)==264);
static_assert(offsetof(DataSideIGC,squadID)==268);
static_assert(offsetof(DataSideIGC,conquest)==272);
static_assert(offsetof(DataSideIGC,territory)==273);
static_assert(offsetof(DataSideIGC,allies)==274);
inline void cleanDataSideIGC(char* p) {
    {bool end=false;for(int i=0;i<25;++i){if(end)p[i]=0;else end=p[i]==0;}}
    p[25]=0;
    p[130]=0;
    p[131]=0;
    p[266]=0;
    p[267]=0;
    p[275]=0;
}
static_assert(sizeof(DataShipIGC)==52);
static_assert(offsetof(DataShipIGC,dwPrivate)==0);
static_assert(offsetof(DataShipIGC,partsOffset)==4);
static_assert(offsetof(DataShipIGC,hullID)==6);
static_assert(offsetof(DataShipIGC,shipID)==8);
static_assert(offsetof(DataShipIGC,name)==10);
static_assert(offsetof(DataShipIGC,nParts)==35);
static_assert(offsetof(DataShipIGC,sideID)==36);
static_assert(offsetof(DataShipIGC,pilotType)==38);
static_assert(offsetof(DataShipIGC,abmOrders)==40);
static_assert(offsetof(DataShipIGC,baseObjectID)==42);
static_assert(offsetof(DataShipIGC,nDeaths)==44);
static_assert(offsetof(DataShipIGC,nEjections)==46);
static_assert(offsetof(DataShipIGC,nKills)==48);
inline void cleanDataShipIGC(char* p) {
    {bool end=false;for(int i=10;i<35;++i){if(end)p[i]=0;else end=p[i]==0;}}
    p[39]=0;
    p[50]=0;
    p[51]=0;
}
static_assert(sizeof(DataWarpIGC)==108);
static_assert(offsetof(DataWarpIGC,warpDef)==0);
static_assert(offsetof(DataWarpIGC,name)==32);
static_assert(offsetof(DataWarpIGC,position)==60);
static_assert(offsetof(DataWarpIGC,forward)==72);
static_assert(offsetof(DataWarpIGC,rotation)==84);
static_assert(offsetof(DataWarpIGC,signature)==100);
static_assert(offsetof(DataWarpIGC,clusterID)==104);
inline void cleanDataWarpIGC(char* p) {
    cleanWarpDef(p+0);
    {bool end=false;for(int i=32;i<57;++i){if(end)p[i]=0;else end=p[i]==0;}}
    p[57]=0;
    p[58]=0;
    p[59]=0;
    p[106]=0;
    p[107]=0;
}
static_assert(sizeof(DataTreasureIGC)==44);
static_assert(offsetof(DataTreasureIGC,p0)==0);
static_assert(offsetof(DataTreasureIGC,v0)==12);
static_assert(offsetof(DataTreasureIGC,lifespan)==24);
static_assert(offsetof(DataTreasureIGC,time0)==28);
static_assert(offsetof(DataTreasureIGC,objectID)==32);
static_assert(offsetof(DataTreasureIGC,treasureID)==34);
static_assert(offsetof(DataTreasureIGC,clusterID)==36);
static_assert(offsetof(DataTreasureIGC,amount)==38);
static_assert(offsetof(DataTreasureIGC,treasureCode)==40);
static_assert(offsetof(DataTreasureIGC,createNow)==41);
inline void cleanDataTreasureIGC(char* p) {
    p[42]=0;
    p[43]=0;
}
static_assert(sizeof(DataMunitionExport)==4);
static_assert(offsetof(DataMunitionExport,lifespan)==0);
static_assert(offsetof(DataMunitionExport,dx)==1);
static_assert(offsetof(DataMunitionExport,dy)==2);
static_assert(offsetof(DataMunitionExport,dz)==3);
inline void cleanDataMunitionExport(char* p) {
}
static_assert(sizeof(DataMineBase)==20);
static_assert(offsetof(DataMineBase,p0)==0);
static_assert(offsetof(DataMineBase,time0)==12);
static_assert(offsetof(DataMineBase,mineID)==16);
static_assert(offsetof(DataMineBase,exportF)==18);
inline void cleanDataMineBase(char* p) {
    p[19]=0;
}
static_assert(sizeof(DataMineExport)==32);
static_assert(offsetof(DataMineExport,clusterID)==20);
static_assert(offsetof(DataMineExport,minetypeID)==22);
static_assert(offsetof(DataMineExport,launcherID)==24);
static_assert(offsetof(DataMineExport,sideID)==26);
static_assert(offsetof(DataMineExport,fraction)==28);
static_assert(offsetof(DataMineExport,createNow)==29);
inline void cleanDataMineExport(char* p) {
    cleanDataMineBase(p);
    p[30]=0;
    p[31]=0;
}
static_assert(sizeof(DataBuildingEffectBase)==36);
static_assert(offsetof(DataBuildingEffectBase,timeStart)==0);
static_assert(offsetof(DataBuildingEffectBase,radiusAsteroid)==4);
static_assert(offsetof(DataBuildingEffectBase,radiusStation)==8);
static_assert(offsetof(DataBuildingEffectBase,positionStart)==12);
static_assert(offsetof(DataBuildingEffectBase,positionStop)==24);
inline void cleanDataBuildingEffectBase(char* p) {
}
static_assert(sizeof(DataBuildingEffectExport)==56);
static_assert(offsetof(DataBuildingEffectExport,clusterID)==36);
static_assert(offsetof(DataBuildingEffectExport,asteroidID)==38);
static_assert(offsetof(DataBuildingEffectExport,color)==40);
inline void cleanDataBuildingEffectExport(char* p) {
    cleanDataBuildingEffectBase(p);
}
static_assert(sizeof(DataProbeBase)==20);
static_assert(offsetof(DataProbeBase,p0)==0);
static_assert(offsetof(DataProbeBase,time0)==12);
static_assert(offsetof(DataProbeBase,probeID)==16);
static_assert(offsetof(DataProbeBase,exportF)==18);
inline void cleanDataProbeBase(char* p) {
    p[19]=0;
}
static_assert(sizeof(DataProbeExport)==36);
static_assert(offsetof(DataProbeExport,probetypeID)==20);
static_assert(offsetof(DataProbeExport,sideID)==22);
static_assert(offsetof(DataProbeExport,clusterID)==24);
static_assert(offsetof(DataProbeExport,shipID)==26);
static_assert(offsetof(DataProbeExport,otTarget)==28);
static_assert(offsetof(DataProbeExport,oidTarget)==30);
static_assert(offsetof(DataProbeExport,createNow)==32);
inline void cleanDataProbeExport(char* p) {
    cleanDataProbeBase(p);
    p[33]=0;
    p[34]=0;
    p[35]=0;
}
static_assert(sizeof(DataStationIGC)==88);
static_assert(offsetof(DataStationIGC,position)==0);
static_assert(offsetof(DataStationIGC,up)==12);
static_assert(offsetof(DataStationIGC,forward)==24);
static_assert(offsetof(DataStationIGC,rotation)==36);
static_assert(offsetof(DataStationIGC,clusterID)==52);
static_assert(offsetof(DataStationIGC,sideID)==54);
static_assert(offsetof(DataStationIGC,stationID)==56);
static_assert(offsetof(DataStationIGC,stationTypeID)==58);
static_assert(offsetof(DataStationIGC,bpHull)==60);
static_assert(offsetof(DataStationIGC,bpShield)==61);
static_assert(offsetof(DataStationIGC,name)==62);
inline void cleanDataStationIGC(char* p) {
    {bool end=false;for(int i=62;i<87;++i){if(end)p[i]=0;else end=p[i]==0;}}
    p[87]=0;
}
inline void clean(ObjectType type,char* p,size_t size){
    switch(type) {
    case OT_cluster:if(size!=sizeof(DataClusterIGC))throw std::runtime_error("Windows dynamic record size differs");cleanDataClusterIGC(p);break;
    case OT_warp:if(size!=sizeof(DataWarpIGC))throw std::runtime_error("Windows dynamic record size differs");cleanDataWarpIGC(p);break;
    case OT_asteroid:if(size!=sizeof(DataAsteroidIGC))throw std::runtime_error("Windows dynamic record size differs");cleanDataAsteroidIGC(p);break;
    case OT_station:if(size!=sizeof(DataStationIGC))throw std::runtime_error("Windows dynamic record size differs");cleanDataStationIGC(p);break;
    default:throw std::runtime_error("Unexpected installed map record type");
    }
}
inline constexpr int recordCount=19;
inline constexpr int fieldCount=144;
}
#pragma clang diagnostic pop
