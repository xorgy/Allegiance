// Native services consumed by the original authored training actions. This is
// an adapter to Game/IGC, Audio and native presentation state, not a Win32 UI.
#pragma once
#include "legacy.hpp"
#include "igc.h"
#include "../soundengine/soundengine.h"
#include "../ClientCore/Overlay.h"
#include "../ClientCore/RadarPresentation.h"
#include <list>
#include <algorithm>
#include <cstdarg>
#include <climits>
#include <optional>
#include <functional>
#include <string_view>
#ifndef MAXCHAR
#define MAXCHAR CHAR_MAX
#endif
#define bit_and &
#define bit_or |
using namespace SoundEngine;
namespace Native {
class Game;class Audio;
class TrainingHost {
public:
    enum CameraMode {cmCockpit,cmExternal,cmExternalChase,cmExternalTarget,cmExternalBase,cmExternalMissile,
        cmExternalFlyBy,cmExternalFlyByTarget,cmExternalOrbit,cmExternalStation,cmExternalOverride,cmExternalTurret,
        cmExternalCommandView34,cmExternalCommandViewTD};
    enum ViewMode {vmHangar,vmLoadout,vmCommand,vmCombat,vmOverride,vmUI,c_cViewModes};
    TrainingHost(Game& game,Audio& audio,int joystickCount);
    ~TrainingHost();
    std::function<std::string(std::string_view)> promptFormatter;
    Game& game;Audio& audio;bool& fGroupFire;float m_serverOffset{};
    int joystickCount{};float delta{};bool paused{},finished{},skipSlideshow{};std::optional<bool> detected;
    ViewMode view{vmCombat};CameraMode camera{cmCockpit};int radar{Allegiance::c_rlAll};
    OverlayMask overlays{ofInventory|ofSectorPane|ofGameState};TrainingOverlay overlay{NoTrainingOverlay};float hudStyle{};
    bool displacement{true};TRef<IclusterIGC> viewCluster;
    ImissionIGC* GetCore()const;IshipIGC* GetShip()const;ShipID GetShipID()const;IsideIGC* GetSide()const;
    ImissionIGC* ResetStaticData();void SetMissionParameters(const char* core,int version);
    void RegisterPilot(IshipIGC* ship,SideID side,ShipID id);
    void ClearLoadout();void SaveLoadout(IshipIGC* ship);void ClearMessages();void OnEnterGame();
    void BuyDefaultLoadout(IshipIGC* ship,IstationIGC* station,IhullTypeIGC* hull,Money* money);
    void OverrideThrottle(float value);void SetSelectedWeapon(Mount mount);void ToggleWeapon(Mount mount);
    void SwapPart(EquipmentType type,Mount mount);bool flyingF()const;
    void SetTarget(ImodelIGC* target,CommandID command);void SetQueuedCommand(IshipIGC* sender,CommandID command,ImodelIGC* target);
    void PostText(bool critical,const char* format,...);
    void ReceiveChat(IshipIGC* sender,ChatTarget recipient,ObjectID id,SoundID sound,const char* text,
        CommandID command,ObjectType type,ObjectID target,ImodelIGC* model,bool objectModel);
    TRef<ISoundInstance> StartSound(SoundID sound);
    void EjectPlayer(ImodelIGC* launcher);void FinishMission(bool skip);void ShowEyeball(bool value){detected=value;}
    void SetViewCluster(IclusterIGC* cluster){viewCluster=cluster;}IclusterIGC* GetViewCluster()const{return viewCluster;}
    float GetHUDStyle()const{return hudStyle;}void SetHUDStyle(float style){hudStyle=style;}
    ViewMode GetViewMode()const{return view;}void SetViewMode(ViewMode mode){view=mode;}
    CameraMode GetCameraMode()const{return camera;}void SetCameraMode(CameraMode mode){camera=mode;}
    int GetRadarMode()const{return radar;}void SetRadarMode(int mode){radar=mode;}
    OverlayMask GetOverlayFlags()const{return overlays;}
    void TurnOffOverlayFlags(OverlayMask flags){overlays&=~flags;}void TurnOnOverlayFlags(OverlayMask flags){overlays|=flags;}
    void EnableDisplacementCommandView(bool value){displacement=value;}
    void PositionCommandView(const Vector* position,float dt);
    float GetDeltaTime()const{return delta;}
    TrainingHost* GetInputEngine(){return this;}int GetJoystickCount()const{return joystickCount;}
    TrainingHost* GetConsoleImage(){return this;}TrainingHost* GetConsoleData(){return this;}IbaseIGC* GetPickedObject()const;
    static bool CommandCamera(CameraMode mode){return mode==cmExternalCommandView34 || mode==cmExternalCommandViewTD;}
};
extern TrainingHost* trainingHost;
}
using TrekWindow=Native::TrainingHost;
struct RadarImage {
    using RadarLOD=Allegiance::RadarDetailMode;
    static constexpr RadarLOD c_rlNone=Allegiance::c_rlNone,c_rlAll=Allegiance::c_rlAll,c_rlDefault=Allegiance::c_rlDefault,c_rlTarget=Allegiance::c_rlTarget,c_rlMinimal=Allegiance::c_rlMinimal;
};
inline TrekWindow* GetWindow(){return Native::trainingHost;}
inline void SetTrainingOverlay(TrainingOverlay overlay){Native::trainingHost->overlay=overlay;}
inline void ClearTrainingOverlay(){SetTrainingOverlay(NoTrainingOverlay);}
// Preserve the authored source's existing client expression. Only this native
// translation unit contract uses the alias; game/renderer code does not.
#define trekClient (*Native::trainingHost)
