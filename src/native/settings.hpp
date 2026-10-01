#pragma once
#include <SDL3/SDL.h>
#include "../Igc/TrekKeys.h"
#include "visual_preferences.hpp"
#include "../ClientCore/ChatPreferences.h"
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace Native {
enum class Binding { ThrottleUp,ThrottleDown,Left,Right,Up,Down,RollLeft,RollRight,Boost,Coast,Missile,Chaff,Deploy,Target,Autopilot,Cloak,Ripcord,Eject,Forward,Backward,Reload,NextCargo,DropCargo,Inventory,AcceptOrder,ExecuteOrder,RejectOrder,ClearOrder,TargetOrder,MatchSpeed,PilotOrders,TargetNearest,TargetPrev,TargetCenter,TargetHostile,TargetNothing,TargetSelf,TargetEnemy,TargetEnemyNearest,TargetEnemyPrev,TargetEnemyBomber,TargetEnemyBomberNearest,TargetEnemyBomberPrev,TargetEnemyFighter,TargetEnemyFighterNearest,TargetEnemyFighterPrev,TargetEnemyMiner,TargetEnemyMinerNearest,TargetEnemyMinerPrev,TargetEnemyConstructor,TargetEnemyConstructorNearest,TargetEnemyConstructorPrev,TargetEnemyBase,TargetEnemyBaseNearest,TargetEnemyBasePrev,TargetEnemyMajorBase,TargetEnemyMajorBaseNearest,TargetEnemyMajorBasePrev,TargetFriendlyBase,TargetFriendlyBaseNearest,TargetFriendlyBasePrev,TargetAlliedBase,TargetAlliedBaseNearest,TargetAlliedBasePrev,TargetFriendlyMajorBase,TargetFriendlyMajorBaseNearest,TargetFriendlyMajorBasePrev,TargetFriendly,TargetFriendlyNearest,TargetFriendlyPrev,TargetFriendlyBomber,TargetFriendlyBomberNearest,TargetFriendlyBomberPrev,TargetFriendlyFighter,TargetFriendlyFighterNearest,TargetFriendlyFighterPrev,TargetFriendlyMiner,TargetFriendlyMinerNearest,TargetFriendlyMinerPrev,TargetFriendlyConstructor,TargetFriendlyConstructorNearest,TargetFriendlyConstructorPrev,TargetFriendlyLifepod,TargetFriendlyLifepodNearest,TargetFriendlyLifepodPrev,TargetArtifact,TargetArtifactNearest,TargetArtifactPrev,TargetFlag,TargetFlagNearest,TargetFlagPrev,TargetWarp,TargetWarpNearest,TargetWarpPrev,NextWeapon,PreviousWeapon,Fire,Pause,ToggleView,RollModifier,YawLeft,YawRight,PitchUp,PitchDown,GroupFire,Weapon1,Weapon2,Weapon3,Weapon4,ThrottleZero,Throttle33,Throttle66,ThrottleFull,Help,CommandView,CommandTopDown,PaneInventory,PaneContacts,SectorMap,EventLog,Diagnostics,HudVisible,RadarDetail,Cursor,Intro,Research,UnitOrders,Chat,QuickChat,PauseTraining,CameraExternal,CameraTarget,CameraFlyBy,CameraOrbit,CameraStation,CameraMissile,CameraCockpit,CameraChase,CameraZoomIn,CameraZoomOut,LookFront,LookLeft,LookRight,LookRear,LookFrontLeft,LookFrontRight,LookRearLeft,LookRearRight,HudLarger,HudSmaller,BoardTarget,LeaveShip,TrackCommand,SwapWeapon1,SwapWeapon2,SwapWeapon3,SwapWeapon4,SwapMissile,SwapChaff,SwapShield,SwapCloak,SwapBooster,SwapDispenser,Teleport,ChatPageUp,ChatPageDown,ToggleGrid,Screenshot,SwapTurret1,SwapTurret2,SwapTurret3,SwapTurret4,PromoteTurret1,PromoteTurret2,PromoteTurret3,PromoteTurret4,GameState,CameraBase,VoteYes,VoteNo,Count };
struct ControllerAxisRoute {
    int device{-1},index{-1};bool hat{},invert{};
    int originalValue{-1}; // saved DirectInput value, retained for remapping
    bool operator==(const ControllerAxisRoute&)const=default;
};
struct ControllerButtonRoute {
    int device{},button{};Binding binding{Binding::Fire};
    bool operator==(const ControllerButtonRoute&)const=default;
};
inline constexpr std::array<const char*,6> controllerAxisNames{"Yaw","Pitch","Roll","Throttle","Look direction","Command hat"};
struct ControllerSettings {
    bool enabled{true},linear{},feedback{true},autoCenter{};
    float deadZone{.1f},feedbackGain{1};
    std::string device; // empty selects first device; otherwise its stable SDL GUID
    std::array<int,4> axes{0,1,2,-2}; // yaw, pitch, roll, throttle; -2 auto, -1 unbound
    std::array<bool,4> invert{true,false,true,true};
    int hat{0};bool triggers{true};
    std::array<int,64> buttons=[] {std::array<int,64> result;result.fill(-2);return result;}();
    bool mapped{};
    std::array<std::string,16> mapDevices{}; // empty follows the saved device ordinal
    std::array<ControllerAxisRoute,6> mapAxes{};
    std::vector<ControllerButtonRoute> mapButtons;
};
inline constexpr uint8_t KeyShift=1,KeyControl=2,KeyAlt=4,KeyAny=8;
struct KeyAlias {SDL_Scancode scan;uint8_t modifiers;Binding binding;TrekKey command{TK_NoKeyMapping};bool operator==(const KeyAlias&)const=default;};
struct MouseAlias {int button;Binding binding;bool operator==(const MouseAlias& other)const{return button==other.button && binding==other.binding;}};
inline constexpr std::array<const char*,10> mouseButtonNames{"Left button","Right button","Middle button","Side button 1","Side button 2","Button 6","Button 7","Button 8","Wheel down","Wheel up"};
struct Settings {
    ControllerSettings controller;
    VisualPreferences visual;
    Allegiance::ChatPreferences chat;
    int classicUiScale{}; // 0 follows the platform's per-window display scale.
    float uiScale{1},volume{0.7f},sensitivity{1},fieldOfView{65},effectsGain{-8},voiceGain{-13};
    float gamma{1.13f}; // WinTrek startup Gamma; display-encoded final-image ramp.
    int antialiasing{},maxTextureSize{}; // Samples; original 256 << level (0..3).
    bool fullscreen{true},invertY{},showTiming{},startupCredits{},startupIntro{},showGrid{},virtualJoystick{},joystickIndicator{true};
    bool showCommandGrid{true},preferChase{};
    bool showStartupTrainingSuggestion{true}; // Ui.ShowStartupTrainingSuggestion / absent HasTrained.
    int particles{1},throttleDelay{2},mouseAcceleration{};
    int hudStyle{}; // Original StyleHUD slots, including the authored Jello style.
    int chatLines{7},chatSize{2};
    int defaultWing{}; // Ui.DefaultWing, with the original absent-preference default.
    int cockpitRadar{},commandRadar{};
    std::array<int,10> mouseButtons{int(Binding::Fire),int(Binding::Missile),-1,-1,-1,-1,-1,-1,int(Binding::ThrottleDown),int(Binding::ThrottleUp)};
    std::vector<MouseAlias> mouseAliases;
    std::string pilot{"Cadet"};
    std::array<SDL_Scancode,size_t(Binding::Count)> keys=[] {std::array<SDL_Scancode,size_t(Binding::Count)> result{SDL_SCANCODE_W,SDL_SCANCODE_S,SDL_SCANCODE_A,SDL_SCANCODE_D,
        SDL_SCANCODE_SPACE,SDL_SCANCODE_C,SDL_SCANCODE_Q,SDL_SCANCODE_E,SDL_SCANCODE_LSHIFT,SDL_SCANCODE_LCTRL,
        SDL_SCANCODE_M,SDL_SCANCODE_X,SDL_SCANCODE_N,SDL_SCANCODE_TAB,SDL_SCANCODE_P,SDL_SCANCODE_K,SDL_SCANCODE_T,SDL_SCANCODE_BACKSPACE,SDL_SCANCODE_I,SDL_SCANCODE_J,SDL_SCANCODE_R,SDL_SCANCODE_G,SDL_SCANCODE_DELETE,SDL_SCANCODE_B,SDL_SCANCODE_INSERT,SDL_SCANCODE_HOME,SDL_SCANCODE_END,SDL_SCANCODE_F11,SDL_SCANCODE_U,SDL_SCANCODE_Y,SDL_SCANCODE_O,SDL_SCANCODE_COMMA,SDL_SCANCODE_PERIOD,SDL_SCANCODE_L,SDL_SCANCODE_F12};
        result[size_t(Binding::Pause)]=SDL_SCANCODE_ESCAPE;result[size_t(Binding::ToggleView)]=SDL_SCANCODE_V;
        result[size_t(Binding::RollModifier)]=SDL_SCANCODE_RCTRL;
        result[size_t(Binding::YawLeft)]=SDL_SCANCODE_LEFT;result[size_t(Binding::YawRight)]=SDL_SCANCODE_RIGHT;
        result[size_t(Binding::PitchUp)]=SDL_SCANCODE_UP;result[size_t(Binding::PitchDown)]=SDL_SCANCODE_DOWN;
        result[size_t(Binding::GroupFire)]=SDL_SCANCODE_0;
        result[size_t(Binding::Weapon1)]=SDL_SCANCODE_1;result[size_t(Binding::Weapon2)]=SDL_SCANCODE_2;
        result[size_t(Binding::Weapon3)]=SDL_SCANCODE_3;result[size_t(Binding::Weapon4)]=SDL_SCANCODE_4;
        result[size_t(Binding::ThrottleZero)]=SDL_SCANCODE_5;result[size_t(Binding::Throttle33)]=SDL_SCANCODE_6;
        result[size_t(Binding::Throttle66)]=SDL_SCANCODE_7;result[size_t(Binding::ThrottleFull)]=SDL_SCANCODE_8;
        result[size_t(Binding::TargetEnemy)]=SDL_SCANCODE_F;
        result[size_t(Binding::TargetFriendly)]=SDL_SCANCODE_F10;
        result[size_t(Binding::TargetFriendlyBase)]=SDL_SCANCODE_9;
        result[size_t(Binding::Help)]=SDL_SCANCODE_F1;
        result[size_t(Binding::CommandView)]=SDL_SCANCODE_F2;
        result[size_t(Binding::PaneInventory)]=SDL_SCANCODE_F4;
        result[size_t(Binding::PaneContacts)]=SDL_SCANCODE_F6;
        result[size_t(Binding::SectorMap)]=SDL_SCANCODE_F7;
        result[size_t(Binding::EventLog)]=SDL_SCANCODE_F8;
        result[size_t(Binding::Diagnostics)]=SDL_SCANCODE_F9;
        result[size_t(Binding::HudVisible)]=SDL_SCANCODE_H;
        result[size_t(Binding::RadarDetail)]=SDL_SCANCODE_Z;
        result[size_t(Binding::Cursor)]=SDL_SCANCODE_BACKSLASH;
        result[size_t(Binding::Intro)]=SDL_SCANCODE_F3;
        result[size_t(Binding::Research)]=SDL_SCANCODE_F5;
        result[size_t(Binding::Chat)]=SDL_SCANCODE_RETURN;
        result[size_t(Binding::QuickChat)]=SDL_SCANCODE_GRAVE;
        result[size_t(Binding::PauseTraining)]=SDL_SCANCODE_PAUSE;
        result[size_t(Binding::CameraZoomIn)]=SDL_SCANCODE_PAGEUP;
        result[size_t(Binding::CameraZoomOut)]=SDL_SCANCODE_PAGEDOWN;
        result[size_t(Binding::LookFront)]=SDL_SCANCODE_KP_8;
        result[size_t(Binding::LookLeft)]=SDL_SCANCODE_KP_4;
        result[size_t(Binding::LookRight)]=SDL_SCANCODE_KP_6;
        result[size_t(Binding::LookRear)]=SDL_SCANCODE_KP_2;
        result[size_t(Binding::LookFrontLeft)]=SDL_SCANCODE_KP_7;
        result[size_t(Binding::LookFrontRight)]=SDL_SCANCODE_KP_9;
        result[size_t(Binding::LookRearLeft)]=SDL_SCANCODE_KP_1;
        result[size_t(Binding::LookRearRight)]=SDL_SCANCODE_KP_3;
        result[size_t(Binding::HudLarger)]=SDL_SCANCODE_EQUALS;
        result[size_t(Binding::HudSmaller)]=SDL_SCANCODE_MINUS;
        for(int i=0;i<10;++i)result[size_t(Binding::SwapWeapon1)+i]=std::array<SDL_Scancode,10>{SDL_SCANCODE_1,SDL_SCANCODE_2,SDL_SCANCODE_3,SDL_SCANCODE_4,SDL_SCANCODE_5,SDL_SCANCODE_6,SDL_SCANCODE_7,SDL_SCANCODE_8,SDL_SCANCODE_9,SDL_SCANCODE_0}[i];
        result[size_t(Binding::Teleport)]=SDL_SCANCODE_F8;
        result[size_t(Binding::ChatPageUp)]=SDL_SCANCODE_PAGEUP;result[size_t(Binding::ChatPageDown)]=SDL_SCANCODE_PAGEDOWN;
        result[size_t(Binding::ToggleGrid)]=SDL_SCANCODE_PERIOD;result[size_t(Binding::Screenshot)]=SDL_SCANCODE_PRINTSCREEN;result[size_t(Binding::GameState)]=SDL_SCANCODE_SLASH;
        result[size_t(Binding::VoteYes)]=SDL_SCANCODE_Y;result[size_t(Binding::VoteNo)]=SDL_SCANCODE_N;
        return result;}();
    std::array<uint8_t,size_t(Binding::Count)> modifiers=[] {std::array<uint8_t,size_t(Binding::Count)> result;result.fill(KeyAny);for(size_t i=size_t(Binding::SwapWeapon1);i<=size_t(Binding::SwapDispenser);++i)result[i]=KeyControl;result[size_t(Binding::EventLog)]=KeyShift;result[size_t(Binding::Teleport)]=0;result[size_t(Binding::ChatPageUp)]=result[size_t(Binding::ChatPageDown)]=KeyControl;result[size_t(Binding::ToggleGrid)]=KeyControl;return result;}();
    std::vector<KeyAlias> keyAliases; // original map aliases; compiled once by KeyboardBindings
    std::array<TrekKey,size_t(Binding::Count)> keyCommands=[] {std::array<TrekKey,size_t(Binding::Count)> result;result.fill(TK_NoKeyMapping);return result;}();
    static std::filesystem::path defaultPath();
    static Settings load(const std::filesystem::path& path);
    static Settings load(const std::filesystem::path& path,const Settings& defaults);
    size_t importInputMap(const std::filesystem::path& path);
    size_t importOriginal(const std::filesystem::path& path); // supported WinTrek JSON preferences; does not write its source
    void save(const std::filesystem::path& path) const;
    void normalize();
};
inline constexpr std::array<const char*,size_t(Binding::Count)> bindingNames{"Increase throttle","Decrease throttle","Strafe left","Strafe right",
    "Strafe up","Strafe down","Roll left","Roll right","Afterburner","Vector lock","Fire missile","Deploy chaff","Deploy equipment",
    "Next target","Autopilot","Cloak","Ripcord / cancel","Eject / abandon pod","Forward thrust","Reverse thrust","Reload reserves","Next cargo stack","Drop cargo stack","Cargo / equipment","Accept command","Accept and execute","Reject command","Clear accepted command","Target accepted command","Match target speed","Pilot commands","Target nearest","Target prev","Target center","Target hostile","Target nothing","Target self","Target enemy","Target enemy nearest","Target enemy previous","Target enemy bomber","Target enemy bomber nearest","Target enemy bomber previous","Target enemy fighter","Target enemy fighter nearest","Target enemy fighter previous","Target enemy miner","Target enemy miner nearest","Target enemy miner previous","Target enemy constructor","Target enemy constructor nearest","Target enemy constructor previous","Target enemy base","Target enemy base nearest","Target enemy base previous","Target enemy major base","Target enemy major base nearest","Target enemy major base previous","Target friendly base","Target friendly base nearest","Target friendly base previous","Target allied base","Target allied base nearest","Target allied base previous","Target friendly major base","Target friendly major base nearest","Target friendly major base previous","Target friendly","Target friendly nearest","Target friendly previous","Target friendly bomber","Target friendly bomber nearest","Target friendly bomber previous","Target friendly fighter","Target friendly fighter nearest","Target friendly fighter previous","Target friendly miner","Target friendly miner nearest","Target friendly miner previous","Target friendly constructor","Target friendly constructor nearest","Target friendly constructor previous","Target friendly lifepod","Target friendly lifepod nearest","Target friendly lifepod previous","Target artifact","Target artifact nearest","Target artifact previous","Target flag","Target flag nearest","Target flag previous","Target warp","Target warp nearest","Target warp previous","Next weapon","Previous weapon","Fire guns","Pause / menu","Cockpit / chase","Roll modifier","Yaw left","Yaw right","Pitch up","Pitch down","Toggle group fire","Select gun 1","Select gun 2","Select gun 3","Select gun 4","Throttle zero","Throttle one-third","Throttle two-thirds","Throttle full","Help","Command view","Command perspective","Inventory pane","Contacts / team pane","Sector map","Event log","Timing diagnostics","Show HUD","Radar detail","Mouse cursor","Introduction movie","Research","Unit orders","Type chat","Quick chat","Pause training","External camera","Target camera","Flyby camera","Orbit camera","Station tracking camera","Missile camera","Cockpit camera","Chase camera","Camera zoom in","Camera zoom out","Look forward","Look left","Look right","Look rear","Look forward left","Look forward right","Look rear left","Look rear right","HUD scale larger","HUD scale smaller","Board / next turret","Disembark","Track command target","Swap gun 1 from cargo","Swap gun 2 from cargo","Swap gun 3 from cargo","Swap gun 4 from cargo","Swap missiles from cargo","Swap chaff from cargo","Swap shield from cargo","Swap cloak from cargo","Swap booster from cargo","Swap deployable from cargo","Teleport to a base","Message page up","Message page down","Show sector grid","Screenshot","Swap turret 1 from cargo","Swap turret 2 from cargo","Swap turret 3 from cargo","Swap turret 4 from cargo","Promote turret 1","Promote turret 2","Promote turret 3","Promote turret 4","Game state","View base","Vote yes","Vote no"};
static_assert([]{for(auto* name:bindingNames)if(!name)return false;return true;}(),"Every binding needs an input-mapping label");
struct TargetBinding {Binding binding;TrekKey key;};
inline constexpr std::array<TargetBinding,64> targetBindings{{
    {Binding::Target,TK_Target},
    {Binding::TargetNearest,TK_TargetNearest},
    {Binding::TargetPrev,TK_TargetPrev},
    {Binding::TargetCenter,TK_TargetCenter},
    {Binding::TargetHostile,TK_TargetHostile},
    {Binding::TargetNothing,TK_TargetNothing},
    {Binding::TargetSelf,TK_TargetSelf},
    {Binding::TargetEnemy,TK_TargetEnemy},
    {Binding::TargetEnemyNearest,TK_TargetEnemyNearest},
    {Binding::TargetEnemyPrev,TK_TargetEnemyPrev},
    {Binding::TargetEnemyBomber,TK_TargetEnemyBomber},
    {Binding::TargetEnemyBomberNearest,TK_TargetEnemyBomberNearest},
    {Binding::TargetEnemyBomberPrev,TK_TargetEnemyBomberPrev},
    {Binding::TargetEnemyFighter,TK_TargetEnemyFighter},
    {Binding::TargetEnemyFighterNearest,TK_TargetEnemyFighterNearest},
    {Binding::TargetEnemyFighterPrev,TK_TargetEnemyFighterPrev},
    {Binding::TargetEnemyMiner,TK_TargetEnemyMiner},
    {Binding::TargetEnemyMinerNearest,TK_TargetEnemyMinerNearest},
    {Binding::TargetEnemyMinerPrev,TK_TargetEnemyMinerPrev},
    {Binding::TargetEnemyConstructor,TK_TargetEnemyConstructor},
    {Binding::TargetEnemyConstructorNearest,TK_TargetEnemyConstructorNearest},
    {Binding::TargetEnemyConstructorPrev,TK_TargetEnemyConstructorPrev},
    {Binding::TargetEnemyBase,TK_TargetEnemyBase},
    {Binding::TargetEnemyBaseNearest,TK_TargetEnemyBaseNearest},
    {Binding::TargetEnemyBasePrev,TK_TargetEnemyBasePrev},
    {Binding::TargetEnemyMajorBase,TK_TargetEnemyMajorBase},
    {Binding::TargetEnemyMajorBaseNearest,TK_TargetEnemyMajorBaseNearest},
    {Binding::TargetEnemyMajorBasePrev,TK_TargetEnemyMajorBasePrev},
    {Binding::TargetFriendlyBase,TK_TargetFriendlyBase},
    {Binding::TargetFriendlyBaseNearest,TK_TargetFriendlyBaseNearest},
    {Binding::TargetFriendlyBasePrev,TK_TargetFriendlyBasePrev},
    {Binding::TargetAlliedBase,TK_TargetAlliedBase},
    {Binding::TargetAlliedBaseNearest,TK_TargetAlliedBaseNearest},
    {Binding::TargetAlliedBasePrev,TK_TargetAlliedBasePrev},
    {Binding::TargetFriendlyMajorBase,TK_TargetFriendlyMajorBase},
    {Binding::TargetFriendlyMajorBaseNearest,TK_TargetFriendlyMajorBaseNearest},
    {Binding::TargetFriendlyMajorBasePrev,TK_TargetFriendlyMajorBasePrev},
    {Binding::TargetFriendly,TK_TargetFriendly},
    {Binding::TargetFriendlyNearest,TK_TargetFriendlyNearest},
    {Binding::TargetFriendlyPrev,TK_TargetFriendlyPrev},
    {Binding::TargetFriendlyBomber,TK_TargetFriendlyBomber},
    {Binding::TargetFriendlyBomberNearest,TK_TargetFriendlyBomberNearest},
    {Binding::TargetFriendlyBomberPrev,TK_TargetFriendlyBomberPrev},
    {Binding::TargetFriendlyFighter,TK_TargetFriendlyFighter},
    {Binding::TargetFriendlyFighterNearest,TK_TargetFriendlyFighterNearest},
    {Binding::TargetFriendlyFighterPrev,TK_TargetFriendlyFighterPrev},
    {Binding::TargetFriendlyMiner,TK_TargetFriendlyMiner},
    {Binding::TargetFriendlyMinerNearest,TK_TargetFriendlyMinerNearest},
    {Binding::TargetFriendlyMinerPrev,TK_TargetFriendlyMinerPrev},
    {Binding::TargetFriendlyConstructor,TK_TargetFriendlyConstructor},
    {Binding::TargetFriendlyConstructorNearest,TK_TargetFriendlyConstructorNearest},
    {Binding::TargetFriendlyConstructorPrev,TK_TargetFriendlyConstructorPrev},
    {Binding::TargetFriendlyLifepod,TK_TargetFriendlyLifepod},
    {Binding::TargetFriendlyLifepodNearest,TK_TargetFriendlyLifepodNearest},
    {Binding::TargetFriendlyLifepodPrev,TK_TargetFriendlyLifepodPrev},
    {Binding::TargetArtifact,TK_TargetArtifact},
    {Binding::TargetArtifactNearest,TK_TargetArtifactNearest},
    {Binding::TargetArtifactPrev,TK_TargetArtifactPrev},
    {Binding::TargetFlag,TK_TargetFlag},
    {Binding::TargetFlagNearest,TK_TargetFlagNearest},
    {Binding::TargetFlagPrev,TK_TargetFlagPrev},
    {Binding::TargetWarp,TK_TargetWarp},
    {Binding::TargetWarpNearest,TK_TargetWarpNearest},
    {Binding::TargetWarpPrev,TK_TargetWarpPrev},
}};
}
