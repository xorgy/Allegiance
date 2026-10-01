// Pointer-free dynamic records frozen from original 39ef2c072b1eac0e9f97ab1428bc96572cbd5854.
// Layout-only external classes retain their original scalar storage.
// clang++ -target i686-pc-windows-msvc -Xclang -fdump-record-layouts-complete -fsyntax-only windows_dynamic_layout.cpp
const int NA=-1;
typedef unsigned long DWORD; typedef unsigned long D3DCOLOR;
typedef short ObjectID; typedef short ObjectType;
typedef ObjectID SectorID; typedef ObjectID HullID; typedef ObjectID ShipID; typedef ObjectID WarpID;
typedef ObjectID StationID; typedef ObjectID StationTypeID; typedef ObjectID ExpendableTypeID;
typedef ObjectID MineID; typedef ObjectID ProbeID; typedef ObjectID TreasureID; typedef ObjectID CivID;
typedef ObjectID ProjectileTypeID; typedef ObjectID PartID; typedef ObjectID AsteroidID;
typedef short AbilityBitMask; typedef short AsteroidAbilityBitMask; typedef float HitPoints;
typedef char Mount; typedef ObjectID SideID; typedef unsigned char PilotType; typedef char TreasureCode;
typedef int SquadID; typedef char BuoyType; typedef ObjectID BuoyID;
struct Vector {float x,y,z;}; struct Rotation {Vector axis;float angle;};
struct Time {unsigned long clock;}; struct BytePercentage {unsigned char value;};
struct Color {float r,g,b,a;}; struct TechTreeBitMask {unsigned char bits[50];}; struct GlobalAttributeSet {float values[25];};
struct AsteroidDef
{
    float                   ore;
    float                   oreMax;	
    AsteroidAbilityBitMask  aabmCapabilities;
    AsteroidID              asteroidID;
    HitPoints               hitpoints;
    short                   radius;
    char                    modelName[13];
    char                    textureName[13];
    char                    iconName[13];
};

struct  WarpDef
{
    WarpID              warpID;
    WarpID              destinationID;
    short               radius;
    char                textureName[13];
    char                iconName[13];
};

#pragma pack(push, 1)
struct  PartData
{
    PartID          partID;
    short           amount;
    Mount           mountID;
};
#pragma pack(pop)

struct  DataProjectileIGC
{
    Vector              velocity;
    Vector              forward;
    float               lifespan;
    ProjectileTypeID    projectileTypeID;
};

struct  DataBuoyIGC
{
    Vector              position;
    SectorID            clusterID;
    BuoyType            type;
    BuoyID              buoyID;
    bool                visible;

    DataBuoyIGC(void) { buoyID = NA; visible = true; }
};

struct  DataClusterIGC
{
    unsigned int        starSeed;
    Vector              lightDirection;
    D3DCOLOR            lightColor;
    float               screenX;
    float               screenY;
    SectorID            clusterID;
    short               nDebris;
    short               nStars;
    char                name[25];
    char                posterName[13];
    char                planetName[13];
    BytePercentage      planetSinLatitude;
    BytePercentage      planetLongitude;
    unsigned char       planetRadius;
    bool                activeF;
    bool                bHomeSector;
};

struct  DataAsteroidIGC
{
    float               signature;
    Vector              position;
    Vector              up;
    Vector              forward;
    Rotation            rotation;
    AsteroidDef         asteroidDef;
    SectorID            clusterID;
    char                name[25];
    BytePercentage      fraction;
};

struct  DataSideIGC
{
    char                name[25];
    CivID               civilizationID;
    SideID              sideID;
    TechTreeBitMask     ttbmInitialTechs;
    TechTreeBitMask     ttbmDevelopmentTechs;
    GlobalAttributeSet  gasAttributes;
    Color               color;
    float               fTimeEndured;
    short               nFlags;
    short               nArtifacts;
    short               nKills;
    short               nEjections;
    short               nDeaths;
    short               nBaseKills;
    short               nBaseCaptures;
    SquadID             squadID;

    unsigned char       conquest;
    unsigned char       territory;

	char				allies; 
};

struct  DataShipIGC
{
    DWORD           dwPrivate;
    short           partsOffset;
    HullID          hullID;
    ShipID          shipID;
    char            name[25];
    Mount           nParts;
    SideID          sideID;
    
    PilotType       pilotType;
    AbilityBitMask  abmOrders;
    ObjectID        baseObjectID;
    short           nDeaths;
    short           nEjections;
    short           nKills;
};

struct  DataWarpIGC
{
    WarpDef             warpDef;
    char                name[25];
    Vector              position;
    Vector              forward;
    Rotation            rotation;
    float               signature;
    SectorID            clusterID;
};

struct  DataTreasureIGC
{
    Vector              p0;
    Vector              v0;
    float               lifespan;
    Time                time0;
    ObjectID            objectID;
    TreasureID          treasureID;
    SectorID            clusterID;
    short               amount;
    TreasureCode        treasureCode;
    bool                createNow;
};

struct DataMunitionExport
{
    unsigned char       lifespan;
    char                dx;
    char                dy;
    char                dz;
};

struct  DataMineBase
{
    Vector              p0;
    Time                time0;
    MineID              mineID;
    bool                exportF;
};

struct DataMineExport : public DataMineBase
{
    SectorID            clusterID;
    ExpendableTypeID    minetypeID;
    ShipID              launcherID;
    SideID              sideID;
    BytePercentage      fraction;
    bool                createNow;
};

struct DataBuildingEffectBase
{
    Time                timeStart;
    float               radiusAsteroid;
    float               radiusStation;
    Vector              positionStart;
    Vector              positionStop;
};

struct DataBuildingEffectExport : DataBuildingEffectBase
{
    SectorID            clusterID;
    AsteroidID          asteroidID;
    Color               color;
};

struct  DataProbeBase
{
    Vector              p0;
    Time                time0;
    ProbeID             probeID;
    bool                exportF;
};

struct DataProbeExport : public DataProbeBase
{
    ExpendableTypeID    probetypeID;
    SideID              sideID;
    SectorID            clusterID;
    ShipID              shipID;
    ObjectType          otTarget;
    ObjectID            oidTarget;
    bool                createNow;
};

struct  DataStationIGC
{
    Vector              position;
    Vector              up;
    Vector              forward;
    Rotation            rotation;
    SectorID            clusterID;
    SideID              sideID;
    StationID           stationID;
    StationTypeID       stationTypeID;
    BytePercentage      bpHull;
    BytePercentage      bpShield;
    char                name[25];
};
