// Layout-only copy of pointer-free static records from original 39ef2c072b1eac0e9f97ab1428bc96572cbd5854.
// External scalar/storage definitions are reduced to their original members:
// TLargeBitMask<400>: 50 bytes, GlobalAttributeSet: 25 floats, COLORVALUE: four floats.
// Inspect with: clang++ -target i686-pc-windows-msvc -Xclang -fdump-record-layouts-complete -fsyntax-only windows_core_layout.cpp
typedef unsigned long DWORD; struct COLORVALUE {float r,g,b,a;}; struct TechTreeBitMask {unsigned char bits[50];}; struct GlobalAttributeSet {float values[25];};
typedef short ObjectID;
typedef ObjectID PartID;
typedef ObjectID TreasureSetID;
typedef ObjectID DevelopmentID;
typedef char PackType;
typedef float HitPoints;
typedef unsigned char PilotType;
typedef short AbilityBitMask;
typedef AbilityBitMask HullAbilityBitMask;
typedef char Mount;
typedef ObjectID DroneTypeID;
typedef char TreasureCode;
typedef unsigned char DamageTypeID;
typedef short EquipmentType;
typedef ObjectID ExpendableTypeID;
typedef AbilityBitMask StationAbilityBitMask;
typedef int Money;
typedef AbilityBitMask AsteroidAbilityBitMask;
typedef ObjectID CivID;
typedef unsigned char StationClassID;
typedef char BuyableGroupID;
typedef short PartMask;
typedef ObjectID ProjectileTypeID;
typedef ObjectID HullID;
typedef short SoundID;
typedef ObjectID StationTypeID;
typedef unsigned char DefenseTypeID;
struct HardpointData {
    SoundID interiorSound;
    SoundID turnSound;
    char frameName[13];
    char locationAbreviation[9];
    PartMask partMask;
    bool bFixed;
};

struct DataBuyableIGC {
    Money price;
    DWORD timeToBuild;
    char modelName[14];
    char iconName[13];
    char name[25];
    char description[201];
    BuyableGroupID groupID;
    TechTreeBitMask ttbmRequired;
    TechTreeBitMask ttbmEffects;
};

struct DataHullTypeIGC : public DataBuyableIGC {
    float mass;
    float signature;
    float speed;
    float maxTurnRates[3];
    float turnTorques[3];
    float thrust;
    float sideMultiplier;
    float backMultiplier;
    float scannerRange;
    float maxFuel;
    float ecm;
    float length;
    float maxEnergy;
    float rechargeRate;
    float ripcordSpeed;
    float ripcordCost;
    short maxAmmo;
    HullID hullID;
    HullID successorHullID;
    Mount maxWeapons;
    Mount maxFixedWeapons;
    HitPoints hitPoints;
    short hardpointOffset;
    DefenseTypeID defenseType;
    short capacityMagazine;
    short capacityDispenser;
    short capacityChaffLauncher;
    PartID preferredPartsTypes[14];
    HullAbilityBitMask habmCapabilities;
    char textureName[13];
    PartMask pmEquipment[8];
    SoundID interiorSound;
    SoundID exteriorSound;
    SoundID mainThrusterInteriorSound;
    SoundID mainThrusterExteriorSound;
    SoundID manuveringThrusterInteriorSound;
    SoundID manuveringThrusterExteriorSound;
};

struct DataPartTypeIGC : public DataBuyableIGC {
    float mass;
    float signature;
    PartID partID;
    PartID successorPartID;
    EquipmentType equipmentType;
    PartMask partMask;
    char inventoryLineMDL[13];
};

struct DataWeaponTypeIGC : public DataPartTypeIGC {
    float dtimeReady;
    float dtimeBurst;
    float energyPerShot;
    float dispersion;
    short cAmmoPerShot;
    ProjectileTypeID projectileTypeID;
    SoundID activateSound;
    SoundID singleShotSound;
    SoundID burstSound;
};

struct DataShieldTypeIGC : public DataPartTypeIGC {
    float rateRegen;
    float maxStrength;
    DefenseTypeID defenseType;
    SoundID activateSound;
    SoundID deactivateSound;
};

struct DataCloakTypeIGC : public DataPartTypeIGC {
    float energyConsumption;
    float maxCloaking;
    float onRate;
    float offRate;
    SoundID engageSound;
    SoundID disengageSound;
};

struct DataAfterburnerTypeIGC : public DataPartTypeIGC {
    float fuelConsumption;
    float maxThrust;
    float onRate;
    float offRate;
    SoundID interiorSound;
    SoundID exteriorSound;
};

struct DataPackTypeIGC : public DataPartTypeIGC {
    PackType packType;
    short amount;
};

struct DataLauncherTypeIGC {
    short amount;
    PartID partID;
    PartID successorPartID;
    short launchCount;
    ObjectID expendabletypeID;
    char inventoryLineMDL[13];
};

struct DataObjectIGC {
    COLORVALUE color;
    float radius;
    float rotation;
    char modelName[13];
    char textureName[13];
};

struct DataProjectileTypeIGC : public DataObjectIGC {
    float power;
    float blastPower;
    float blastRadius;
    float speed;
    float lifespan;
    ProjectileTypeID projectileTypeID;
    DamageTypeID damageType;
    bool absoluteF;
    bool bDirectional;
    float width;
    SoundID ambientSound;
};

struct LauncherDef : public DataBuyableIGC {
    float signature;
    float mass;
    PartMask partMask;
    short expendableSize;
};

struct DataExpendableTypeIGC : public DataObjectIGC {
    float loadTime;
    float lifespan;
    float signature;
    LauncherDef launcherDef;
    HitPoints hitPoints;
    DefenseTypeID defenseType;
    ExpendableTypeID expendabletypeID;
    AbilityBitMask eabmCapabilities;
    char iconName[13];
};

struct DataMissileTypeIGC : public DataExpendableTypeIGC {
    float acceleration;
    float turnRate;
    float initialSpeed;
    float lockTime;
    float readyTime;
    float maxLock;
    float chaffResistance;
    float dispersion;
    float lockAngle;
    float power;
    float blastPower;
    float blastRadius;
    float width;
    DamageTypeID damageType;
    bool bDirectional;
    SoundID launchSound;
    SoundID ambientSound;
};

struct DataMineTypeIGC : public DataExpendableTypeIGC {
    float radius;
    float power;
    float endurance;
    DamageTypeID damageType;
};

struct DataChaffTypeIGC : public DataExpendableTypeIGC {
    float chaffStrength;
};

struct DataProbeTypeIGC : public DataExpendableTypeIGC {
    float scannerRange;
    float dtimeBurst;
    float dispersion;
    float accuracy;
    short ammo;
    ProjectileTypeID projectileTypeID;
    SoundID ambientSound;
    float dtRipcord;
};

struct DataCivilizationIGC {
    float incomeMoney;
    float bonusMoney;
    char name[25];
    char iconName[13];
    char hudName[13];
    TechTreeBitMask ttbmBaseTechs;
    TechTreeBitMask ttbmNoDevTechs;
    GlobalAttributeSet gasBaseAttributes;
    HullID lifepod;
    CivID civilizationID;
    StationTypeID initialStationTypeID;
};

struct TreasureData {
    ObjectID treasureID;
    TreasureCode treasureCode;
    unsigned char chance;
};

struct DataTreasureSetIGC {
    char name[25];
    TreasureSetID treasureSetID;
    short nTreasureData;
    bool bZoneOnly;
};

struct DataStationTypeIGC : public DataBuyableIGC {
    float signature;
    float maxArmorHitPoints;
    float maxShieldHitPoints;
    float armorRegeneration;
    float shieldRegeneration;
    float scannerRange;
    Money income;
    float radius;
    TechTreeBitMask ttbmLocal;
    StationTypeID stationTypeID;
    StationTypeID successorStationTypeID;
    DefenseTypeID defenseTypeArmor;
    DefenseTypeID defenseTypeShield;
    StationAbilityBitMask sabmCapabilities;
    AsteroidAbilityBitMask aabmBuild;
    StationClassID classID;
    DroneTypeID constructionDroneTypeID;
    SoundID constructorNeedRockSound;
    SoundID constructorUnderAttackSound;
    SoundID constructorDestroyedSound;
    SoundID completionSound;
    SoundID interiorSound;
    SoundID exteriorSound;
    SoundID interiorAlertSound;
    SoundID underAttackSound;
    SoundID criticalSound;
    SoundID destroyedSound;
    SoundID capturedSound;
    SoundID enemyCapturedSound;
    SoundID enemyDestroyedSound;
    char textureName[13];
    char builderName[25];
};

struct DataDevelopmentIGC : public DataBuyableIGC {
    GlobalAttributeSet gas;
    DevelopmentID developmentID;
    SoundID completionSound;
};

struct DataDroneTypeIGC : public DataBuyableIGC {
    float shootSkill;
    float moveSkill;
    float bravery;
    PilotType pilotType;
    HullID hullTypeID;
    DroneTypeID droneTypeID;
    ExpendableTypeID etidLaid;
};
