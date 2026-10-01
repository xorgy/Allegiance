// Original HUD/training identifiers; independent of rendering facilities.
#pragma once
enum OverlayFlag
{
    ofInHangar          = 0x000001,
    ofInFlightCommand   = 0x000002,
    ofInStationCommand  = 0x000004,
    ofInFlight          = 0x000008,
    ofInCockpit         = 0x000010,
    ofInChase           = 0x000020,
    ofInLoadout         = 0x000040,
    ofTeleportPane      = 0x000100,
    ofInvestment        = 0x000200,
    ofGameState         = 0x000400,
    ofTeam              = 0x000800,
    ofExpandedTeam      = 0x001000,
    ofInventory         = 0x002000,
    ofCommandPane       = 0x004000,
    ofSectorPane        = 0x008000,
    ofGloatCam          = 0x010000
};

typedef unsigned int OverlayMask;

enum TrainingOverlay
{
    NoTrainingOverlay,
    SpeedTrainingOverlay,
    MotionIndicatorTrainingOverlay,
    ReticuleTrainingOverlay,
    ThrottleTrainingOverlay,
    FuelTrainingOverlay,
    EnergyTrainingOverlay,
    AmmoTrainingOverlay,
    HullTrainingOverlay,
    ShieldTrainingOverlay,
    ChatTrainingOverlay,
    SectorTrainingOverlay,
    InventoryTrainingOverlay,
    TargetTrainingOverlay,
    CommandTrainingOverlay,
    EyeballTrainingOverlay,
    KillBonusTrainingOverlay,
    RollTrainingOverlay,
    BoosterTrainingOverlay,
    TargetCenterTrainingOverlay,
    FireGunsTrainingOverlay,
    TargetEnemyTrainingOverlay,
    MissileTrainingOverlay,
    CloakTrainingOverlay
};
