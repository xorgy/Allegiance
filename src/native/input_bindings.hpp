#pragma once
#include "settings.hpp"
#include <optional>
namespace Native {
// Original logical identifiers also reach authored GetKeyCondition objects.
// Platform bindings translate separately from the domain's logical keys.
inline TrekKey trekKey(Binding binding) {
    for(auto target:targetBindings)if(target.binding==binding)return target.key;
    switch(binding) {
    case Binding::ThrottleUp:return TK_ThrottleUp;case Binding::ThrottleDown:return TK_ThrottleDown;
    case Binding::Teleport:return TK_ConModeTeleport;
    case Binding::ChatPageUp:return TK_ChatPageUp;case Binding::ChatPageDown:return TK_ChatPageDown;
    case Binding::ToggleGrid:return TK_ToggleGrid;case Binding::Screenshot:return TK_ScrnShot;
    case Binding::Pause:return TK_MainMenu;
    case Binding::CommandView:return TK_ViewCommand;
    case Binding::RadarDetail:return TK_CycleRadar;
    case Binding::ThrottleZero:return TK_ThrottleZero;case Binding::Throttle33:return TK_Throttle33;
    case Binding::Throttle66:return TK_Throttle66;case Binding::ThrottleFull:return TK_ThrottleFull;
    case Binding::Left:return TK_ThrustLeft;case Binding::Right:return TK_ThrustRight;
    case Binding::Up:return TK_ThrustUp;case Binding::Down:return TK_ThrustDown;
    case Binding::Forward:return TK_ThrustForward;case Binding::Backward:return TK_ThrustBackward;
    case Binding::RollLeft:return TK_RollLeft;case Binding::RollRight:return TK_RollRight;
    case Binding::YawLeft:return TK_YawLeft;case Binding::YawRight:return TK_YawRight;
    case Binding::PitchUp:return TK_PitchUp;case Binding::PitchDown:return TK_PitchDown;
    case Binding::RollModifier:return TK_RollModifier;case Binding::Boost:return TK_FireBooster;
    case Binding::Coast:return TK_LockVector;case Binding::Fire:return TK_FireWeapon;
    case Binding::Missile:return TK_FireMissile;case Binding::Chaff:return TK_DropChaff;
    case Binding::Deploy:return TK_DropMine;case Binding::Cloak:return TK_ToggleCloak;
    case Binding::Autopilot:return TK_ToggleAutoPilot;case Binding::Ripcord:return TK_Ripcord;
    case Binding::Eject:return TK_Suicide;case Binding::Inventory:return TK_ConModeInventory;
    case Binding::NextCargo:return TK_NextCargo;case Binding::DropCargo:return TK_DropCargo;
    case Binding::Reload:return TK_Reload;case Binding::NextWeapon:return TK_NextWeapon;
    case Binding::PreviousWeapon:return TK_PrevWeapon;case Binding::GroupFire:return TK_ToggleGroupFire;
    case Binding::Weapon1:return TK_ToggleWeapon1;case Binding::Weapon2:return TK_ToggleWeapon2;
    case Binding::Weapon3:return TK_ToggleWeapon3;case Binding::Weapon4:return TK_ToggleWeapon4;
    case Binding::AcceptOrder:return TK_AcceptCommand;case Binding::ExecuteOrder:return TK_ExecuteCommand;
    case Binding::RejectOrder:return TK_RejectCommand;case Binding::ClearOrder:return TK_ClearCommand;
    case Binding::TargetOrder:return TK_TargetCommand;case Binding::MatchSpeed:return TK_MatchSpeed;
    case Binding::ToggleView:return TK_ViewChase;case Binding::PilotOrders:return TK_ConModeCommand;
    case Binding::Help:return TK_Help;
    case Binding::CommandTopDown:return TK_ToggleCommand;
    case Binding::PaneInventory:return TK_ConModeInventory;
    case Binding::PaneContacts:return TK_ConModeTeam;
    case Binding::GameState:return TK_ConModeGameState;
    case Binding::SectorMap:return TK_ViewSector;
    case Binding::Research:return TK_ConModeInvest;
    case Binding::UnitOrders:return TK_ConModeCommand;
    case Binding::Chat:return TK_StartChat;
    case Binding::QuickChat:return TK_QuickChatMenu;
    case Binding::PauseTraining:return TK_PauseTM;
    case Binding::CameraExternal:return TK_ViewExternal;
    case Binding::CameraTarget:return TK_ViewTarget;
    case Binding::CameraFlyBy:return TK_ViewFlyby;
    case Binding::CameraOrbit:return TK_ViewExternalOrbit;
    case Binding::CameraStation:return TK_ViewExternalStation;
    case Binding::CameraBase:return TK_ViewBase;
    case Binding::VoteYes:return TK_VoteYes;
    case Binding::VoteNo:return TK_VoteNo;
    case Binding::CameraMissile:return TK_ViewMissile;
    case Binding::CameraCockpit:return TK_ConModeCombat;
    case Binding::CameraChase:return TK_ViewChase;
    case Binding::CameraZoomIn:return TK_ZoomIn;
    case Binding::CameraZoomOut:return TK_ZoomOut;
    case Binding::LookLeft:return TK_ViewLeft;
    case Binding::LookRight:return TK_ViewRight;
    case Binding::LookRear:return TK_ViewRear;
    case Binding::LookFrontLeft:return TK_ViewFrontLeft;
    case Binding::LookFrontRight:return TK_ViewFrontRight;
    case Binding::LookRearLeft:return TK_ViewRearLeft;
    case Binding::LookRearRight:return TK_ViewRearRight;
    case Binding::Cursor:return TK_ToggleMouse;
    case Binding::BoardTarget:return TK_OccupyNextTurret;
    case Binding::TrackCommand:return TK_TrackCommandView;
    case Binding::SwapWeapon1:return TK_SwapWeapon1;case Binding::SwapWeapon2:return TK_SwapWeapon2;
    case Binding::SwapWeapon3:return TK_SwapWeapon3;case Binding::SwapWeapon4:return TK_SwapWeapon4;
    case Binding::SwapMissile:return TK_SwapMissile;case Binding::SwapChaff:return TK_SwapChaff;
    case Binding::SwapShield:return TK_SwapShield;case Binding::SwapCloak:return TK_SwapCloak;
    case Binding::SwapBooster:return TK_SwapAfterburner;case Binding::SwapDispenser:return TK_SwapMine;
    case Binding::SwapTurret1:return TK_SwapTurret1;case Binding::SwapTurret2:return TK_SwapTurret2;
    case Binding::SwapTurret3:return TK_SwapTurret3;case Binding::SwapTurret4:return TK_SwapTurret4;
    case Binding::PromoteTurret1:return TK_PromoteTurret1;case Binding::PromoteTurret2:return TK_PromoteTurret2;
    case Binding::PromoteTurret3:return TK_PromoteTurret3;case Binding::PromoteTurret4:return TK_PromoteTurret4;
    default:return -1;
    }
}
inline std::optional<Binding> bindingFor(TrekKey key){
    // Pane mode aliases have more specific native equivalents than Inventory's
    // full equipment screen and PilotOrders' command response screen.
    switch(key){case TK_ConModeInventory:return Binding::PaneInventory;case TK_ConModeCommand:return Binding::CommandView;
    case TK_ConModeNav:return Binding::SectorMap;case TK_ConModeMiniTeam:return Binding::PaneContacts;default:break;}
    for(int i=0;i<int(Binding::Count);++i)if(trekKey(Binding(i))==key)return Binding(i);return {};
}
// The native editor may bind a physical modifier alone. Continuous controls
// need Any; a press action needs the modifier's exact table to be reachable.
inline bool continuousBinding(Binding binding) {
    if((binding>=Binding::Left && binding<=Binding::Boost) ||
       (binding>=Binding::YawLeft && binding<=Binding::PitchDown) ||
       (binding>=Binding::ThrottleZero && binding<=Binding::ThrottleFull) ||
       (binding>=Binding::LookFront && binding<=Binding::LookRearRight))return true;
    switch(binding) {
    case Binding::ThrottleUp:case Binding::ThrottleDown:case Binding::Forward:case Binding::Backward:
    case Binding::Fire:case Binding::Missile:case Binding::Chaff:case Binding::Deploy:
    case Binding::MatchSpeed:case Binding::RollModifier:case Binding::CameraZoomIn:case Binding::CameraZoomOut:return true;
    default:return false;
    }
}
}
