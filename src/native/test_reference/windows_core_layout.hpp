// Frozen static-core layout from original 39ef2c072b1eac0e9f97ab1428bc96572cbd5854.
// clang i686-pc-windows-msvc layout of the pointer-free original declarations.
// This certifies field layout, not a Windows executable or network protocol.
// Compiler-owned padding is excluded from payload comparison. Never exclude
// authored bytes or infer padding from the native export being tested.
#pragma once
#include "../../Igc/igc.h"
#include <cstddef>
#include <stdexcept>
#include <string>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wgnu-folding-constant"
namespace WindowsCoreLayout {
static_assert(sizeof(HardpointData)==30);
static_assert(offsetof(HardpointData,interiorSound)==0);
static_assert(offsetof(HardpointData,turnSound)==2);
static_assert(offsetof(HardpointData,frameName)==4);
static_assert(offsetof(HardpointData,locationAbreviation)==17);
static_assert(offsetof(HardpointData,partMask)==26);
static_assert(offsetof(HardpointData,bFixed)==28);
inline void paddingHardpointData(char* data) {data[29]=0;}
static_assert(sizeof(DataBuyableIGC)==364);
static_assert(offsetof(DataBuyableIGC,price)==0);
static_assert(offsetof(DataBuyableIGC,timeToBuild)==4);
static_assert(offsetof(DataBuyableIGC,modelName)==8);
static_assert(offsetof(DataBuyableIGC,iconName)==22);
static_assert(offsetof(DataBuyableIGC,name)==35);
static_assert(offsetof(DataBuyableIGC,description)==60);
static_assert(offsetof(DataBuyableIGC,groupID)==261);
static_assert(offsetof(DataBuyableIGC,ttbmRequired)==262);
static_assert(offsetof(DataBuyableIGC,ttbmEffects)==312);
inline void paddingDataBuyableIGC(char* data) {data[362]=0;data[363]=0;}
static_assert(sizeof(DataHullTypeIGC)==540);
static_assert(offsetof(DataHullTypeIGC,mass)==364);
static_assert(offsetof(DataHullTypeIGC,signature)==368);
static_assert(offsetof(DataHullTypeIGC,speed)==372);
static_assert(offsetof(DataHullTypeIGC,maxTurnRates)==376);
static_assert(offsetof(DataHullTypeIGC,turnTorques)==388);
static_assert(offsetof(DataHullTypeIGC,thrust)==400);
static_assert(offsetof(DataHullTypeIGC,sideMultiplier)==404);
static_assert(offsetof(DataHullTypeIGC,backMultiplier)==408);
static_assert(offsetof(DataHullTypeIGC,scannerRange)==412);
static_assert(offsetof(DataHullTypeIGC,maxFuel)==416);
static_assert(offsetof(DataHullTypeIGC,ecm)==420);
static_assert(offsetof(DataHullTypeIGC,length)==424);
static_assert(offsetof(DataHullTypeIGC,maxEnergy)==428);
static_assert(offsetof(DataHullTypeIGC,rechargeRate)==432);
static_assert(offsetof(DataHullTypeIGC,ripcordSpeed)==436);
static_assert(offsetof(DataHullTypeIGC,ripcordCost)==440);
static_assert(offsetof(DataHullTypeIGC,maxAmmo)==444);
static_assert(offsetof(DataHullTypeIGC,hullID)==446);
static_assert(offsetof(DataHullTypeIGC,successorHullID)==448);
static_assert(offsetof(DataHullTypeIGC,maxWeapons)==450);
static_assert(offsetof(DataHullTypeIGC,maxFixedWeapons)==451);
static_assert(offsetof(DataHullTypeIGC,hitPoints)==452);
static_assert(offsetof(DataHullTypeIGC,hardpointOffset)==456);
static_assert(offsetof(DataHullTypeIGC,defenseType)==458);
static_assert(offsetof(DataHullTypeIGC,capacityMagazine)==460);
static_assert(offsetof(DataHullTypeIGC,capacityDispenser)==462);
static_assert(offsetof(DataHullTypeIGC,capacityChaffLauncher)==464);
static_assert(offsetof(DataHullTypeIGC,preferredPartsTypes)==466);
static_assert(offsetof(DataHullTypeIGC,habmCapabilities)==494);
static_assert(offsetof(DataHullTypeIGC,textureName)==496);
static_assert(offsetof(DataHullTypeIGC,pmEquipment)==510);
static_assert(offsetof(DataHullTypeIGC,interiorSound)==526);
static_assert(offsetof(DataHullTypeIGC,exteriorSound)==528);
static_assert(offsetof(DataHullTypeIGC,mainThrusterInteriorSound)==530);
static_assert(offsetof(DataHullTypeIGC,mainThrusterExteriorSound)==532);
static_assert(offsetof(DataHullTypeIGC,manuveringThrusterInteriorSound)==534);
static_assert(offsetof(DataHullTypeIGC,manuveringThrusterExteriorSound)==536);
inline void paddingDataHullTypeIGC(char* data) {data[362]=0;data[363]=0;data[459]=0;data[509]=0;data[538]=0;data[539]=0;}
static_assert(sizeof(DataPartTypeIGC)==396);
static_assert(offsetof(DataPartTypeIGC,mass)==364);
static_assert(offsetof(DataPartTypeIGC,signature)==368);
static_assert(offsetof(DataPartTypeIGC,partID)==372);
static_assert(offsetof(DataPartTypeIGC,successorPartID)==374);
static_assert(offsetof(DataPartTypeIGC,equipmentType)==376);
static_assert(offsetof(DataPartTypeIGC,partMask)==378);
static_assert(offsetof(DataPartTypeIGC,inventoryLineMDL)==380);
inline void paddingDataPartTypeIGC(char* data) {data[362]=0;data[363]=0;data[393]=0;data[394]=0;data[395]=0;}
static_assert(sizeof(DataWeaponTypeIGC)==424);
static_assert(offsetof(DataWeaponTypeIGC,dtimeReady)==396);
static_assert(offsetof(DataWeaponTypeIGC,dtimeBurst)==400);
static_assert(offsetof(DataWeaponTypeIGC,energyPerShot)==404);
static_assert(offsetof(DataWeaponTypeIGC,dispersion)==408);
static_assert(offsetof(DataWeaponTypeIGC,cAmmoPerShot)==412);
static_assert(offsetof(DataWeaponTypeIGC,projectileTypeID)==414);
static_assert(offsetof(DataWeaponTypeIGC,activateSound)==416);
static_assert(offsetof(DataWeaponTypeIGC,singleShotSound)==418);
static_assert(offsetof(DataWeaponTypeIGC,burstSound)==420);
inline void paddingDataWeaponTypeIGC(char* data) {data[362]=0;data[363]=0;data[393]=0;data[394]=0;data[395]=0;data[422]=0;data[423]=0;}
static_assert(sizeof(DataShieldTypeIGC)==412);
static_assert(offsetof(DataShieldTypeIGC,rateRegen)==396);
static_assert(offsetof(DataShieldTypeIGC,maxStrength)==400);
static_assert(offsetof(DataShieldTypeIGC,defenseType)==404);
static_assert(offsetof(DataShieldTypeIGC,activateSound)==406);
static_assert(offsetof(DataShieldTypeIGC,deactivateSound)==408);
inline void paddingDataShieldTypeIGC(char* data) {data[362]=0;data[363]=0;data[393]=0;data[394]=0;data[395]=0;data[405]=0;data[410]=0;data[411]=0;}
static_assert(sizeof(DataCloakTypeIGC)==416);
static_assert(offsetof(DataCloakTypeIGC,energyConsumption)==396);
static_assert(offsetof(DataCloakTypeIGC,maxCloaking)==400);
static_assert(offsetof(DataCloakTypeIGC,onRate)==404);
static_assert(offsetof(DataCloakTypeIGC,offRate)==408);
static_assert(offsetof(DataCloakTypeIGC,engageSound)==412);
static_assert(offsetof(DataCloakTypeIGC,disengageSound)==414);
inline void paddingDataCloakTypeIGC(char* data) {data[362]=0;data[363]=0;data[393]=0;data[394]=0;data[395]=0;}
static_assert(sizeof(DataAfterburnerTypeIGC)==416);
static_assert(offsetof(DataAfterburnerTypeIGC,fuelConsumption)==396);
static_assert(offsetof(DataAfterburnerTypeIGC,maxThrust)==400);
static_assert(offsetof(DataAfterburnerTypeIGC,onRate)==404);
static_assert(offsetof(DataAfterburnerTypeIGC,offRate)==408);
static_assert(offsetof(DataAfterburnerTypeIGC,interiorSound)==412);
static_assert(offsetof(DataAfterburnerTypeIGC,exteriorSound)==414);
inline void paddingDataAfterburnerTypeIGC(char* data) {data[362]=0;data[363]=0;data[393]=0;data[394]=0;data[395]=0;}
static_assert(sizeof(DataPackTypeIGC)==400);
static_assert(offsetof(DataPackTypeIGC,packType)==396);
static_assert(offsetof(DataPackTypeIGC,amount)==398);
inline void paddingDataPackTypeIGC(char* data) {data[362]=0;data[363]=0;data[393]=0;data[394]=0;data[395]=0;data[397]=0;}
static_assert(sizeof(DataLauncherTypeIGC)==24);
static_assert(offsetof(DataLauncherTypeIGC,amount)==0);
static_assert(offsetof(DataLauncherTypeIGC,partID)==2);
static_assert(offsetof(DataLauncherTypeIGC,successorPartID)==4);
static_assert(offsetof(DataLauncherTypeIGC,launchCount)==6);
static_assert(offsetof(DataLauncherTypeIGC,expendabletypeID)==8);
static_assert(offsetof(DataLauncherTypeIGC,inventoryLineMDL)==10);
inline void paddingDataLauncherTypeIGC(char* data) {data[23]=0;}
static_assert(sizeof(DataObjectIGC)==52);
static_assert(offsetof(DataObjectIGC,color)==0);
static_assert(offsetof(DataObjectIGC,radius)==16);
static_assert(offsetof(DataObjectIGC,rotation)==20);
static_assert(offsetof(DataObjectIGC,modelName)==24);
static_assert(offsetof(DataObjectIGC,textureName)==37);
inline void paddingDataObjectIGC(char* data) {data[50]=0;data[51]=0;}
static_assert(sizeof(DataProjectileTypeIGC)==88);
static_assert(offsetof(DataProjectileTypeIGC,power)==52);
static_assert(offsetof(DataProjectileTypeIGC,blastPower)==56);
static_assert(offsetof(DataProjectileTypeIGC,blastRadius)==60);
static_assert(offsetof(DataProjectileTypeIGC,speed)==64);
static_assert(offsetof(DataProjectileTypeIGC,lifespan)==68);
static_assert(offsetof(DataProjectileTypeIGC,projectileTypeID)==72);
static_assert(offsetof(DataProjectileTypeIGC,damageType)==74);
static_assert(offsetof(DataProjectileTypeIGC,absoluteF)==75);
static_assert(offsetof(DataProjectileTypeIGC,bDirectional)==76);
static_assert(offsetof(DataProjectileTypeIGC,width)==80);
static_assert(offsetof(DataProjectileTypeIGC,ambientSound)==84);
inline void paddingDataProjectileTypeIGC(char* data) {data[50]=0;data[51]=0;data[77]=0;data[78]=0;data[79]=0;data[86]=0;data[87]=0;}
static_assert(sizeof(LauncherDef)==376);
static_assert(offsetof(LauncherDef,signature)==364);
static_assert(offsetof(LauncherDef,mass)==368);
static_assert(offsetof(LauncherDef,partMask)==372);
static_assert(offsetof(LauncherDef,expendableSize)==374);
inline void paddingLauncherDef(char* data) {data[362]=0;data[363]=0;}
static_assert(sizeof(DataExpendableTypeIGC)==464);
static_assert(offsetof(DataExpendableTypeIGC,loadTime)==52);
static_assert(offsetof(DataExpendableTypeIGC,lifespan)==56);
static_assert(offsetof(DataExpendableTypeIGC,signature)==60);
static_assert(offsetof(DataExpendableTypeIGC,launcherDef)==64);
static_assert(offsetof(DataExpendableTypeIGC,hitPoints)==440);
static_assert(offsetof(DataExpendableTypeIGC,defenseType)==444);
static_assert(offsetof(DataExpendableTypeIGC,expendabletypeID)==446);
static_assert(offsetof(DataExpendableTypeIGC,eabmCapabilities)==448);
static_assert(offsetof(DataExpendableTypeIGC,iconName)==450);
inline void paddingDataExpendableTypeIGC(char* data) {data[50]=0;data[51]=0;data[426]=0;data[427]=0;data[445]=0;data[463]=0;}
static_assert(sizeof(DataMissileTypeIGC)==524);
static_assert(offsetof(DataMissileTypeIGC,acceleration)==464);
static_assert(offsetof(DataMissileTypeIGC,turnRate)==468);
static_assert(offsetof(DataMissileTypeIGC,initialSpeed)==472);
static_assert(offsetof(DataMissileTypeIGC,lockTime)==476);
static_assert(offsetof(DataMissileTypeIGC,readyTime)==480);
static_assert(offsetof(DataMissileTypeIGC,maxLock)==484);
static_assert(offsetof(DataMissileTypeIGC,chaffResistance)==488);
static_assert(offsetof(DataMissileTypeIGC,dispersion)==492);
static_assert(offsetof(DataMissileTypeIGC,lockAngle)==496);
static_assert(offsetof(DataMissileTypeIGC,power)==500);
static_assert(offsetof(DataMissileTypeIGC,blastPower)==504);
static_assert(offsetof(DataMissileTypeIGC,blastRadius)==508);
static_assert(offsetof(DataMissileTypeIGC,width)==512);
static_assert(offsetof(DataMissileTypeIGC,damageType)==516);
static_assert(offsetof(DataMissileTypeIGC,bDirectional)==517);
static_assert(offsetof(DataMissileTypeIGC,launchSound)==518);
static_assert(offsetof(DataMissileTypeIGC,ambientSound)==520);
inline void paddingDataMissileTypeIGC(char* data) {data[50]=0;data[51]=0;data[426]=0;data[427]=0;data[445]=0;data[463]=0;data[522]=0;data[523]=0;}
static_assert(sizeof(DataMineTypeIGC)==480);
static_assert(offsetof(DataMineTypeIGC,radius)==464);
static_assert(offsetof(DataMineTypeIGC,power)==468);
static_assert(offsetof(DataMineTypeIGC,endurance)==472);
static_assert(offsetof(DataMineTypeIGC,damageType)==476);
inline void paddingDataMineTypeIGC(char* data) {data[50]=0;data[51]=0;data[426]=0;data[427]=0;data[445]=0;data[463]=0;data[477]=0;data[478]=0;data[479]=0;}
static_assert(sizeof(DataChaffTypeIGC)==468);
static_assert(offsetof(DataChaffTypeIGC,chaffStrength)==464);
inline void paddingDataChaffTypeIGC(char* data) {data[50]=0;data[51]=0;data[426]=0;data[427]=0;data[445]=0;data[463]=0;}
static_assert(sizeof(DataProbeTypeIGC)==492);
static_assert(offsetof(DataProbeTypeIGC,scannerRange)==464);
static_assert(offsetof(DataProbeTypeIGC,dtimeBurst)==468);
static_assert(offsetof(DataProbeTypeIGC,dispersion)==472);
static_assert(offsetof(DataProbeTypeIGC,accuracy)==476);
static_assert(offsetof(DataProbeTypeIGC,ammo)==480);
static_assert(offsetof(DataProbeTypeIGC,projectileTypeID)==482);
static_assert(offsetof(DataProbeTypeIGC,ambientSound)==484);
static_assert(offsetof(DataProbeTypeIGC,dtRipcord)==488);
inline void paddingDataProbeTypeIGC(char* data) {data[50]=0;data[51]=0;data[426]=0;data[427]=0;data[445]=0;data[463]=0;data[486]=0;data[487]=0;}
static_assert(sizeof(DataCivilizationIGC)==268);
static_assert(offsetof(DataCivilizationIGC,incomeMoney)==0);
static_assert(offsetof(DataCivilizationIGC,bonusMoney)==4);
static_assert(offsetof(DataCivilizationIGC,name)==8);
static_assert(offsetof(DataCivilizationIGC,iconName)==33);
static_assert(offsetof(DataCivilizationIGC,hudName)==46);
static_assert(offsetof(DataCivilizationIGC,ttbmBaseTechs)==59);
static_assert(offsetof(DataCivilizationIGC,ttbmNoDevTechs)==109);
static_assert(offsetof(DataCivilizationIGC,gasBaseAttributes)==160);
static_assert(offsetof(DataCivilizationIGC,lifepod)==260);
static_assert(offsetof(DataCivilizationIGC,civilizationID)==262);
static_assert(offsetof(DataCivilizationIGC,initialStationTypeID)==264);
inline void paddingDataCivilizationIGC(char* data) {data[159]=0;data[266]=0;data[267]=0;}
static_assert(sizeof(TreasureData)==4);
static_assert(offsetof(TreasureData,treasureID)==0);
static_assert(offsetof(TreasureData,treasureCode)==2);
static_assert(offsetof(TreasureData,chance)==3);
inline void paddingTreasureData(char* data) {}
static_assert(sizeof(DataTreasureSetIGC)==32);
static_assert(offsetof(DataTreasureSetIGC,name)==0);
static_assert(offsetof(DataTreasureSetIGC,treasureSetID)==26);
static_assert(offsetof(DataTreasureSetIGC,nTreasureData)==28);
static_assert(offsetof(DataTreasureSetIGC,bZoneOnly)==30);
inline void paddingDataTreasureSetIGC(char* data) {data[25]=0;data[31]=0;}
static_assert(sizeof(DataStationTypeIGC)==524);
static_assert(offsetof(DataStationTypeIGC,signature)==364);
static_assert(offsetof(DataStationTypeIGC,maxArmorHitPoints)==368);
static_assert(offsetof(DataStationTypeIGC,maxShieldHitPoints)==372);
static_assert(offsetof(DataStationTypeIGC,armorRegeneration)==376);
static_assert(offsetof(DataStationTypeIGC,shieldRegeneration)==380);
static_assert(offsetof(DataStationTypeIGC,scannerRange)==384);
static_assert(offsetof(DataStationTypeIGC,income)==388);
static_assert(offsetof(DataStationTypeIGC,radius)==392);
static_assert(offsetof(DataStationTypeIGC,ttbmLocal)==396);
static_assert(offsetof(DataStationTypeIGC,stationTypeID)==446);
static_assert(offsetof(DataStationTypeIGC,successorStationTypeID)==448);
static_assert(offsetof(DataStationTypeIGC,defenseTypeArmor)==450);
static_assert(offsetof(DataStationTypeIGC,defenseTypeShield)==451);
static_assert(offsetof(DataStationTypeIGC,sabmCapabilities)==452);
static_assert(offsetof(DataStationTypeIGC,aabmBuild)==454);
static_assert(offsetof(DataStationTypeIGC,classID)==456);
static_assert(offsetof(DataStationTypeIGC,constructionDroneTypeID)==458);
static_assert(offsetof(DataStationTypeIGC,constructorNeedRockSound)==460);
static_assert(offsetof(DataStationTypeIGC,constructorUnderAttackSound)==462);
static_assert(offsetof(DataStationTypeIGC,constructorDestroyedSound)==464);
static_assert(offsetof(DataStationTypeIGC,completionSound)==466);
static_assert(offsetof(DataStationTypeIGC,interiorSound)==468);
static_assert(offsetof(DataStationTypeIGC,exteriorSound)==470);
static_assert(offsetof(DataStationTypeIGC,interiorAlertSound)==472);
static_assert(offsetof(DataStationTypeIGC,underAttackSound)==474);
static_assert(offsetof(DataStationTypeIGC,criticalSound)==476);
static_assert(offsetof(DataStationTypeIGC,destroyedSound)==478);
static_assert(offsetof(DataStationTypeIGC,capturedSound)==480);
static_assert(offsetof(DataStationTypeIGC,enemyCapturedSound)==482);
static_assert(offsetof(DataStationTypeIGC,enemyDestroyedSound)==484);
static_assert(offsetof(DataStationTypeIGC,textureName)==486);
static_assert(offsetof(DataStationTypeIGC,builderName)==499);
inline void paddingDataStationTypeIGC(char* data) {data[362]=0;data[363]=0;data[457]=0;}
static_assert(sizeof(DataDevelopmentIGC)==468);
static_assert(offsetof(DataDevelopmentIGC,gas)==364);
static_assert(offsetof(DataDevelopmentIGC,developmentID)==464);
static_assert(offsetof(DataDevelopmentIGC,completionSound)==466);
inline void paddingDataDevelopmentIGC(char* data) {data[362]=0;data[363]=0;}
static_assert(sizeof(DataDroneTypeIGC)==384);
static_assert(offsetof(DataDroneTypeIGC,shootSkill)==364);
static_assert(offsetof(DataDroneTypeIGC,moveSkill)==368);
static_assert(offsetof(DataDroneTypeIGC,bravery)==372);
static_assert(offsetof(DataDroneTypeIGC,pilotType)==376);
static_assert(offsetof(DataDroneTypeIGC,hullTypeID)==378);
static_assert(offsetof(DataDroneTypeIGC,droneTypeID)==380);
static_assert(offsetof(DataDroneTypeIGC,etidLaid)==382);
inline void paddingDataDroneTypeIGC(char* data) {data[362]=0;data[363]=0;data[377]=0;}
inline void normalize(std::string& record) {
    constexpr size_t header=sizeof(ObjectType)+sizeof(int32_t);
    ObjectType type;memcpy(&type,record.data(),sizeof(type));char* data=record.data()+header;size_t length=record.size()-header;
    switch(type) {
    case OT_projectileType: if(length!=88)throw std::runtime_error("Windows DataProjectileTypeIGC record length differs");paddingDataProjectileTypeIGC(data);break;
    case OT_missileType: if(length!=524)throw std::runtime_error("Windows DataMissileTypeIGC record length differs");paddingDataMissileTypeIGC(data);break;
    case OT_mineType: if(length!=480)throw std::runtime_error("Windows DataMineTypeIGC record length differs");paddingDataMineTypeIGC(data);break;
    case OT_chaffType: if(length!=468)throw std::runtime_error("Windows DataChaffTypeIGC record length differs");paddingDataChaffTypeIGC(data);break;
    case OT_probeType: if(length!=492)throw std::runtime_error("Windows DataProbeTypeIGC record length differs");paddingDataProbeTypeIGC(data);break;
    case OT_civilization: if(length!=268)throw std::runtime_error("Windows DataCivilizationIGC record length differs");paddingDataCivilizationIGC(data);break;
    case OT_stationType: if(length!=524)throw std::runtime_error("Windows DataStationTypeIGC record length differs");paddingDataStationTypeIGC(data);break;
    case OT_development: if(length!=468)throw std::runtime_error("Windows DataDevelopmentIGC record length differs");paddingDataDevelopmentIGC(data);break;
    case OT_droneType: if(length!=384)throw std::runtime_error("Windows DataDroneTypeIGC record length differs");paddingDataDroneTypeIGC(data);break;
    case OT_partType:
        switch(length) {
        case 24:paddingDataLauncherTypeIGC(data);break;
        case 400:paddingDataPackTypeIGC(data);break;
        case 424:paddingDataWeaponTypeIGC(data);break;
        case 412:paddingDataShieldTypeIGC(data);break;
        case 416:paddingDataCloakTypeIGC(data);break;
        default:throw std::runtime_error("Unrecognized Windows part record");
        }break;
    case OT_hullType:
        if(length<540 || (length-540)%30)throw std::runtime_error("Invalid Windows hull hardpoints");
        paddingDataHullTypeIGC(data);for(size_t i=540;i<length;i+=30)paddingHardpointData(data+i);break;
    case OT_treasureSet:
        if(length<32 || (length-32)%4)throw std::runtime_error("Invalid Windows treasure array");
        paddingDataTreasureSetIGC(data);break;
    case OT_constants:break;
    default:throw std::runtime_error("Unexpected static-core object type");
    }
}
}
#pragma clang diagnostic pop
