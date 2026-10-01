#pragma once
#include "settings.hpp"
#include "controller.hpp"
#include "session.hpp"
#include "game.hpp"
#include "menu_layout.hpp"
#include "keyboard.hpp"
#include "original_ui.hpp"
#include "hud_program.hpp"
#include "scroll_list.hpp"
#include "server_browser.hpp"
#include "ui_clock.hpp"
#include "../ClientCore/ClientSoundEvents.h"
#include "../ClientCore/LobbyReady.h"
#include "../ClientCore/LobbyRosterPresentation.h"
#include "../ClientCore/CrewPresentation.h"
#include <glm/glm.hpp>
#include <optional>
#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include "../ClientCore/MissionDetails.h"
#include "../ClientCore/Overlay.h"
#include "../ClientCore/HelpMotion.h"
#include "mml_page.hpp"

namespace Native {
struct GameState;
enum class Screen { Home,Scenarios,Lobby,Settings,Controls,Online,Pause,Hangar,Parts,Results,ConfirmLeave,Credits,Cargo,Research,Orders,UnitOrders,OrderTargets,Crew,CargoMount,Loadouts,MissionRules,Scores,PilotOrders,Controller,ControllerAxes,ControllerButtons,ControllerAction,Training,Chat,QuickChat,Pilots,Account,Teams,LobbyPilots,Countdown,Movies,Audio,Interface,Teleport,Visuals,Mouse,MouseButtons,MouseAction,ControllerMap,ControllerMapAxis,ControllerMapDevices,ControllerMapButtons,ControllerMapButton,ControllerMapAction,ChatPreferences,RemoteLobby,ShipLoadout,Help,Games,GameDetails,Slideshow };
enum class MenuOp { NetworkAddress,NetworkTeam,NetworkReady,NetworkStart,NetworkEdit,NetworkConfigure,NetworkJoinTeam,NetworkNewRound,Connect,LanSelect,LanJoin,Navigate,Scenario,Map,Hull,Enemies,Allies,Difficulty,KillLimit,Ready,Start,Resume,Leave,Quit,Intro,
    LobbyAction,Setting,Apply,Defaults,Bind,Unbind,Pilot,HangarHull,EquipmentSlot,EquipPart,PartPage,ReturnToBase,Launch,EquipmentPage,ResearchPage,Invest,HullPage,Civilization,Unit,Order,OrderTarget,TargetPage,Crew,Board,Disembark,Promote,CrewPage,BindingPage,CargoSelect,CargoNext,CargoDrop,CargoMounts,CargoSwap,Reload,Loadouts,LoadoutSlot,SaveLoadout,UseLoadout,DeleteLoadout,MissionSetting,RulesPage,AcceptOrder,ExecuteOrder,RejectOrder,ClearOrder,TargetOrder,ControllerSetting,ControllerAxis,ControllerInvert,ControllerButton,ControllerAssign,ControllerPage,Training,TakeMeTo,ChatChannel,SendChat,QuickChatItem,QuickChatBack,QuickChatPage,JoinTeam,PilotPage,Account,MoneyAmount,Donate,AutoDonate,RequestMoney,MoneyResponse,Wing,HandOver,SelectTeam,TeamCivilization,TeamAlliance,AddLobbyPilot,RemoveLobbyPilot,LobbyPilotTeam,LobbyPilotReady,LobbyPilotName,LobbyPilotPage,CancelCountdown,ControlPilot,RemovePilot,Teleport,TeleportTo,TeleportPage,VisualSetting,MouseButton,MouseAssign,MousePage,MapMode,MapAxis,MapAxisDevice,MapAxisInput,MapAxisInvert,MapDevice,MapDevicePage,MapButton,MapButtonPage,MapButtonAdd,MapButtonDelete,MapButtonDevice,MapButtonInput,MapButtonAction,MapButtonAssign,ChatPreference,MutePilot,DroneStandby,TrainingSelect,TrainingStart,CommandView,Help,Web,ScenarioSelect,ScenarioOpen,ComposeChat,LobbyChatChannel,
    PreviewHull,PreviewDefault,PreviewSlot,PreviewPart,LoadoutPopup,CommitLoadout,
    MappingSelect,MappingAccept,MappingCancel,MappingRestore,ScrollLine,ScrollPage,ScrollThumb,ListClear,RosterSelect,RosterSort,PopupItem,PopupOption,MakeLeader,
    SetupCombo,SetupChoice,SetupAccept,SetupCancel,SetupMapDrag,TeleportSelect,TeleportActivate,MessageDismiss,ResearchTab,ResearchSelect,ResearchActivate,ResearchFilter,ResultsSort,ResultsSelect,ResultsBack,SaveStats,
    TeamSettingsOpen,TeamName,TeamFaction,TeamFactionChoice,TeamSettingsAccept,TeamSettingsCancel,
    GameSelect,GameJoin,GameSort,GameFilter,GameFilterCombo,GameFilterChoice,GameFilterName,GameFilterAccept,GameFilterCancel,
    GameCreate,GameCreateServer,GameCreateCore,GameCreateName,GameCreateAccept,GameCreateCancel,FindPlayer,FindPlayerName,FindPlayerAccept,FindPlayerCancel,LobbyWing,LobbyWingChoice,ConfirmBoot,DetailsClose,SecondaryClose,PlayerPings,ChangeAlliance,AllianceCancel,TrainingDontAskAgain,LogonName,LogonAccept,LogonCancel,TeleportHeader,TeamActive };
struct MenuAction { MenuOp op;int value{},delta{1}; };
struct MenuItem { glm::vec4 bounds;std::string label,value;MenuAction action;bool enabled{true},selected{};std::string control;int popupLevel{-1},popupRow{-1};bool blocked{};SoundID clickSound{-1},hoverSound{-1};bool detectWhenDisabled{},captureOnPress{},toggleOnRelease{}; };
struct MenuText { glm::vec2 position;std::string text;float width{650};bool heading{}; };
struct MenuScroll {glm::vec4 bounds{};std::shared_ptr<HudScalar> offset;double started{};std::vector<OriginalControl> artwork;};
struct MenuSlide {
    size_t artworkBegin{},itemsBegin{};glm::vec2 off{};double started{};bool closing{};
    bool sideLeft{};
    size_t artworkEnd{size_t(-1)},itemsEnd{size_t(-1)};bool sideRight{};
    float duration{.5f};std::optional<glm::vec2> screenAnchor;
    bool undetectable{};
    bool movesArtwork(size_t index)const{return index>=artworkBegin && index<artworkEnd;}
    bool movesItem(size_t index)const{return index>=itemsBegin && index<itemsEnd;}
    glm::vec2 displacement(double time)const{
        float phase=duration>0?float(std::clamp((time-started)/duration,0.,1.)):1;return glm::round(off*(closing?phase:1-phase));
    }
};
struct MenuView {
    std::string title,subtitle,notice;
    std::vector<MenuItem> items;std::vector<MenuText> text;
    int focus{};bool ingame{};
    bool original{},overlay{};int pressed{-1},classicScale{1};
    glm::ivec2 canvas{800,600};
    std::vector<OriginalControl> artwork;
    std::optional<ModelHandle> shipPreview;
    std::optional<MenuScroll> scroll;
    std::optional<MenuSlide> slide;
    std::optional<size_t> popupArtworkBegin;
    void blockInput(){for(auto& item:items)item.blocked=true;for(auto& control:artwork)control.hoverBlocked=true;}
    void bindControlSounds(){
        std::unordered_map<std::string_view,const OriginalControl*> sources;
        sources.reserve(items.size());
        // A captured parent and its popup may reuse a name. The topmost
        // definition wins even when it deliberately has no sound sinks.
        for(const auto& c:artwork)if((c.kind==OriginalControl::Kind::Button || c.kind==OriginalControl::Kind::Slot) && !c.name.empty())sources.insert_or_assign(c.name,&c);
        for(auto& item:items)if(auto found=sources.find(item.control);found!=sources.end()){
            item.clickSound=found->second->clickSound;item.hoverSound=found->second->hoverSound;
            const auto& c=*found->second;
            bool button=c.kind==OriginalControl::Kind::Button && c.widget!=OriginalControl::Widget::ImageCombo;
            item.detectWhenDisabled=button || c.widget==OriginalControl::Widget::StringCombo || c.widget==OriginalControl::Widget::ColorCombo;
            item.captureOnPress=button && !c.pressAction;
            item.toggleOnRelease=c.toggleOnRelease;
        }
    }
    const MenuItem* capturedButton()const{
        return original && pressed>=0 && size_t(pressed)<items.size() && items[pressed].popupLevel<0 && items[pressed].captureOnPress && !items[pressed].blocked?&items[pressed]:nullptr;
    }
};
// Original event sinks are discrete. Retained layouts must not restart hover
// audio on every refresh. The bounded queue never allocates on input events.
class UiSoundFeedback {
    std::array<SoundID,16> sounds_{};size_t count_{};std::string hovered_;int scope_{};bool enabled_{};
public:
    void emit(SoundID id){if(id>=0 && count_<sounds_.size())sounds_[count_++]=id;}
    void hover(const MenuItem* item,int scope=0){
        std::string_view name=item && !item->blocked && item->hoverSound>=0?std::string_view(item->control):std::string_view{};
        bool enabled=item && item->enabled && !item->blocked;
        if((name!=hovered_ || scope!=scope_ || (!enabled_ && enabled)) && enabled && !name.empty())emit(item->hoverSound);
        if(name!=hovered_)hovered_=name;
        scope_=scope;enabled_=enabled;
    }
    void capturedHover(const MenuItem& item,bool inside,int scope=0){
        // Pane::HitTest sends MouseMove while captured, not enter/leave events.
        // SetEnabled can still announce an enabled-enter while inside.
        std::string_view name=item.hoverSound>=0?std::string_view(item.control):std::string_view{};
        if(inside && item.enabled && !enabled_ && name==hovered_ && scope==scope_)emit(item.hoverSound);
        if(name!=hovered_)hovered_=name;
        scope_=scope;enabled_=item.enabled;
    }
    void click(const MenuItem& item){if(item.enabled && !item.blocked)emit(item.clickSound);}
    std::span<const SoundID> sounds()const{return {sounds_.data(),count_};}
    void clear(){count_=0;}
    void reset(){clear();hovered_.clear();scope_=0;enabled_=false;}
};
inline glm::vec2 menuSlideOffset(const MenuView& menu,glm::ivec2 size,double time) {
    if(!menu.slide)return {};
    auto offset=menu.slide->displacement(time);
    if(menu.slide->screenAnchor){auto layout=originalMenuLayout(size,menu.classicScale,menu.canvas);offset+=glm::floor((glm::vec2(size)/layout.scale-glm::vec2(menu.canvas))* *menu.slide->screenAnchor)-layout.origin;return offset;}
    if(menu.slide->sideLeft)offset.x-=originalMenuLayout(size,menu.classicScale,menu.canvas).origin.x;
    if(menu.slide->sideRight){auto layout=originalMenuLayout(size,menu.classicScale,menu.canvas);offset.x+=size.x/layout.scale-menu.canvas.x-layout.origin.x;}
    return offset;
}
inline MenuSlide overlaySlide(const HudOverlay& spec,double started,bool closing=false) {
    MenuSlide slide;slide.off=spec.off-spec.on;slide.started=started;slide.closing=closing;slide.duration=spec.duration;slide.screenAnchor=spec.anchor;slide.undetectable=spec.undetectable;return slide;
}
inline uint32_t originalOverlayModes(const GameState& g,uint32_t panes=0) {
    uint32_t flags=panes;
    if(!(panes&ofInLoadout)){if(g.commandView)flags|=g.docked?ofInStationCommand:ofInFlightCommand;else if(g.docked)flags|=ofInHangar;}
    if(!g.docked){flags|=ofInFlight;if(g.camera==CameraMode::Cockpit)flags|=ofInCockpit;if(g.camera==CameraMode::Chase)flags|=ofInChase;}
    if(g.cinematic)flags|=ofGloatCam;
    return flags;
}
inline const OriginalControl& authoredControl(std::span<const OriginalControl> controls,std::string_view name) {
    for(const auto& c:controls)if(c.name==name)return c;
    throw std::runtime_error("Original UI control missing: "+std::string(name));
}
inline glm::vec4 authoredControlBounds(std::span<const OriginalControl> controls,std::string_view name) {return authoredControl(controls,name).bounds;}
inline glm::vec4 authoredListBounds(std::span<const OriginalControl> controls,std::string_view name,float painterWidth) {
    auto bounds=authoredControlBounds(controls,name);bounds.z=std::max(bounds.z,painterWidth);return bounds;
}
inline glm::vec4 authoredScrollBarBounds(std::span<const OriginalControl> controls,std::string_view list) {
    return authoredControlBounds(controls,authoredControl(controls,list).scrollBar);
}
// Menus can change several times while draining one SDL event batch. Hit the
// current view directly; rendered glyphs and last frame's hit regions are not
// input state. Reverse order matches the renderer's overlapping-item order.
inline std::optional<size_t> menuHit(const MenuView& menu,glm::ivec2 size,float uiScale,glm::vec2 pixel) {
    if(pixel.x<0 || pixel.y<0 || pixel.x>=size.x || pixel.y>=size.y)return {};
    auto p=(menu.original?originalMenuLayout(size,menu.classicScale,menu.canvas):menuLayout(size,uiScale)).local(pixel);
    auto shift=menuSlideOffset(menu,size,uiTime());
    for(size_t i=menu.items.size();i-->0;) {
        const auto& item=menu.items[i];auto b=item.bounds;
        if(menu.slide && menu.slide->undetectable && menu.slide->movesItem(i))continue;
        if(menu.slide && menu.slide->movesItem(i)){b.x+=shift.x;b.y+=shift.y;}
        // Original buttons and MenuItemPane remain detectable when disabled.
        // Their command handlers refuse activation; hits must not fall through
        // to a parent control or the flight scene behind them.
        if((item.enabled || (menu.original && (item.popupLevel>=0 || item.detectWhenDisabled))) && !item.blocked && p.x>=b.x && p.y>=b.y && p.x<b.x+b.z && p.y<b.y+b.w)return i;
    }
    return {};
}
enum class FrontEffect { None,Connect,NetworkReady,NetworkStart,NetworkConfigure,NetworkJoinTeam,NetworkNewRound,Start,Resume,Leave,Quit,Intro,ApplySettings,ApplyOptions,ChangeHull,PartOptions,EquipPart,ReturnToBase,Launch,Invest,Order,OrderTargets,Crew,Board,Disembark,Promote,CargoSelect,CargoNext,CargoDrop,CargoMounts,CargoSwap,Reload,Loadouts,SaveLoadout,UseLoadout,DeleteLoadout,AcceptOrder,ExecuteOrder,RejectOrder,ClearOrder,TargetOrder,SelectUnit,TakeMeTo,SendChat,QuickChat,Donate,AutoDonate,RequestMoney,MoneyResponse,Wing,HandOver,ControlPilot,RemovePilot,Teleport,TeleportTo,MutePilot,DroneStandby,CommandView,Help,Web,CommitLoadout,MakeLeader,LobbyAction,PlayerPane,SaveStats,TeamSettings,LobbyVisit,ConfirmBoot,BootPilot,PlayerPings,ChangeAlliance,RememberLoadout,FindPlayer,CreateGame };
struct FrontResult { FrontEffect effect{};int value{},command{},targetType{-1},targetId{-1};bool voiceOnly{},resumeAfter{};uint32_t dismissPanes{}; };
class Frontend {
public:
    explicit Frontend(Settings& settings,Settings defaults={});
    void originalInterface(Assets& assets){creditsOffset_.reset();localCores_.clear();createCores_.clear();comboSelections_.clear();buttonChecks_.clear();setupPresetSelection_=-1;editTimes_.clear();customMaps_=Game::customMaps(assets.root());setupCombos_.clear();setupPreviewKey_.fill(-1);setupPreviewFile_.clear();original_=std::make_unique<OriginalUi>(assets);loadoutCalibration_.reset();overlays_=std::make_unique<UiOverlayLayout>(*original_,std::initializer_list<std::string_view>{"TeleportPaneImage","MergedInvestmentsPaneImage"},settings_.hudStyle);initializeHelp();rebuild();}
    bool classicInterface()const{return bool(original_);}
    void prepareSessionService(std::function<std::shared_ptr<const Allegiance::MissionBriefing>(SessionOptions&)> prepare,std::function<void()> discard){prepareSession_=std::move(prepare);discardPreparedSession_=std::move(discard);}
    void displayScale(float scale){displayScale_=scale;rebuild();}
    void missionDetailsService(std::function<Allegiance::MissionDetails()> service){detailsService_=std::move(service);}
    void connectionInfo(std::span<const Allegiance::PlayerConnectionInfo> rows,const GameState& state);
    void loadoutServices(std::function<LoadoutPreview(const Loadout&,bool,int)> preview,std::function<std::vector<PartChoice>(PartKind,int,int)> parts,std::function<std::optional<Loadout>(int,int)> cached={},std::function<std::optional<LoadoutSlotPreview>(int,int)> partInfo={}){previewLoadout_=std::move(preview);previewParts_=std::move(parts);cachedLoadout_=std::move(cached);previewPartInfo_=std::move(partInfo);}
    const LoadoutPreview& stagedLoadout()const{return loadoutPreview_;}
    void loadoutMemory(std::function<void(const Loadout&)> remember){rememberLoadout_=std::move(remember);}
    bool stageLoadout(const Loadout& loadout,bool defaults=false,int targetHull=-1);
    bool stageLoadoutPreset(int bank);
    void pointerHover(std::optional<size_t> hit);
    void pointerMove(glm::vec2 logical,glm::ivec2 drawable={800,600});
    FrontResult pointerOther();
    bool pointerWheel(float amount);
    bool pageChat(bool up);
    bool mappingMouse(int button);
    bool mappingController(const SDL_Event& event);
    void open(Screen screen,const GameState* state=nullptr);
    bool releaseDockedView(const GameState& state);
    void sync(const GameState& state);
    void controllers(std::vector<ControllerDevice> devices,std::string name,int axes,int buttons,int hats,bool gamepad) {controllerDevices_=std::move(devices);controllerName_=std::move(name);controllerAxes_=axes;controllerButtons_=buttons;controllerHats_=hats;controllerGamepad_=gamepad;rebuild();}
    void graphicsDeviceService(std::function<std::vector<std::string>()> service){graphicsDeviceService_=std::move(service);}
    void mouseInputService(std::function<bool()> get,std::function<void(bool)> set){mouseInputGet_=std::move(get);mouseInputSet_=std::move(set);}
    void refreshGraphicsDevice(){if(std::any_of(popups_.begin(),popups_.end(),[](const auto& popup){return popup.group==PopupGroup::Device;}))rebuild();}
    void graphicsSamples(std::vector<int> samples){graphicsSamples_=std::move(samples);}
    void refreshGraphicsOptions(){if(std::any_of(popups_.begin(),popups_.end(),[](const auto& popup){return popup.group==PopupGroup::Advanced;}))rebuild();}
    void civilizations(std::vector<CivilizationChoice> list,float winMoney=0,std::string_view core="static_core"){civilizationCore_=core;civilizations_=std::move(list);comboSelections_.erase("teamscreen/civComboPane");winMoney_=winMoney;rebuild();}
    FrontResult activate(size_t index);
    bool pointerCaptured()const{return bool(pointerAction_) || !popups_.empty();}
    bool contextButtonCaptured()const{return contextCapture_ && pointerAction_;}
    bool buttonCaptured()const{return bool(pointerAction_);}
    void cancelPointer(){if(pointerAction_){pointerAction_.reset();view_.pressed=-1;rebuild();}}
    bool previewCursor(glm::vec2 logical)const {
        return !hasMessageBox() && setupMapCanDrag_ && screen_==Screen::Lobby &&
            ((pointerAction_ && pointerAction_->op==MenuOp::SetupMapDrag) ||
             (logical.x>=setupMapBounds_.x && logical.y>=setupMapBounds_.y &&
              logical.x<setupMapBounds_.x+setupMapBounds_.z && logical.y<setupMapBounds_.y+setupMapBounds_.w));
    }
    FrontResult pointerPress(std::optional<size_t> hit,bool doubleClick=false);
    FrontResult pointerRelease(std::optional<size_t> hit,bool doubleClick=false,int button=SDL_BUTTON_LEFT);
    FrontResult pointerCapturedPress(int button);
    FrontResult pointerContext(std::optional<size_t> hit,bool doubleClick=false);
    FrontResult tick(double seconds,bool wallClock=false);
    std::span<const SoundID> sounds()const{return feedback_.sounds();}
    void clearSounds(){feedback_.clear();}
    FrontResult key(SDL_Keycode key,SDL_Scancode scan,bool shift=false,uint8_t modifiers=0);
    FrontResult keyboardEvent(const SDL_KeyboardEvent& event){return key(event.key,event.scancode,(event.mod&SDL_KMOD_SHIFT)!=0,keyModifiers(event.mod));}
    void keyReleased(SDL_Keycode key,SDL_Scancode scan);
    void text(std::string_view value);
    bool hasMessageBox()const{return !messageBox_.empty();}
    bool waitCursor()const{return original_ && hasMessageBox() && messageBoxPending_;}
    const MenuView& sceneView()const{
        if(original_ && hasMessageBox() && messageBoxResume_)return messageBackdrop_;
        if(original_ && screen_==Screen::GameDetails)return detailsBackdrop_;
        if(original_ && !menuPopupView_.artwork.empty())return menuPopupScene_;
        return original_ && hasMessageBox()?messageBackdrop_:view_;
    }
    const MenuView& menuPopupView()const{menuPopupView_.pressed=view_.pressed>=int(popupItemsBegin_)?view_.pressed-int(popupItemsBegin_):-1;return menuPopupView_;}
    const MenuView& detailsView()const{
        detailsView_.pressed=view_.pressed>=int(detailsItemsBegin_)?view_.pressed-int(detailsItemsBegin_):-1;
        return detailsView_;
    }
    const MenuView& messageView()const{
        messageView_.pressed=view_.pressed>=int(messageItemsBegin_)?view_.pressed-int(messageItemsBegin_):-1;
        return messageView_;
    }
    void confirm(std::string message,FrontResult accept,bool resume=false);
    void connecting();
    void trainingStarting();
    void trainingSlides(bool active);
    void startupTrainingSuggestion();
    void purchasing(bool launch,uint64_t action=0);
    bool pendingAction(uint64_t action)const{return !original_ || (action && messageBoxPending_ && messageBoxAction_==action);}
    bool actionRejected(uint64_t action,std::string message,bool resume=false);
    bool purchaseFinished(uint64_t action=0);
    void transferring(Allegiance::CrewTransfer transfer,std::string_view name,uint64_t action);
    void quittingTeam(uint64_t action);
    void cancelingRequest(uint64_t action);
    void transferFinished(uint64_t action){if(action && messageBoxAction_==action)connectionFinished();}
    void connectionFinished();
    void configuring();
    void closeConsoleToFlight();
    bool closingConsole()const{return consoleFlight_ && bool(consoleClosingSlide_);}
    bool freshInvestmentPane()const{return researchFresh_;}
    uint32_t investmentAsteroid()const;
    bool releaseInvestmentPointer(){if(!researchFresh_)return false;researchFresh_=false;researchHover_=-1;rebuild();return true;}
    bool capturingKey()const{return !hasMessageBox() && (binding_>=0 || mappingEditing_);}
    bool consoleBinding(Binding binding)const;
    FrontResult toggleHelp(){if(original_){showHelp(false);return {};}return screen_==Screen::Help?closeSecondary():FrontResult{FrontEffect::Help};}
    void showHelp(bool forceOn,double time=uiTime());
    bool helpOn()const{return original_ && helpMotion_.on();}
    bool helpVisible(double time=uiTime())const{return original_ && (helpMotion_.on() || helpMotion_.value(time)>0);}
    const MenuView& helpView(int scale=1,double time=uiTime());
    bool helpPointerInside(glm::ivec2 size,glm::vec2 pixel)const;
    bool helpPointerCaptured()const{return helpView_.pressed>=0;}
    void helpHover(std::optional<size_t> hit);
    FrontResult helpPress(std::optional<size_t> hit,int button=SDL_BUTTON_LEFT,bool doubleClick=false);
    FrontResult helpRelease(std::optional<size_t> hit,int button=SDL_BUTTON_LEFT);
    void cancelHelpPointer(){helpView_.pressed=-1;}
    bool mainMenuInput()const{
        // WinTrek's keyboard filter runs before the lobby/results EditPane.
        // Actual popups and key-mapping capture still consume their own keys.
        bool lobbyChat=view_.original && (screen_==Screen::RemoteLobby || screen_==Screen::Pilots || screen_==Screen::LobbyPilots || screen_==Screen::Results);
        return original_ && screen_!=Screen::GameDetails && !capturingKey() && !keyboardPopup() && (!wantsText() || lobbyChat);
    }
    bool keyboardPopup()const{return hasMessageBox() || logonOpen_ || missionBriefOpen_ || teamSettingsActive() || ((gameFilterOpen_ || gameCreateOpen_ || findPlayerOpen_) && screen_==Screen::Games) ||
        screen_==Screen::Online || screen_==Screen::ConfirmLeave || screen_==Screen::Help || screen_==Screen::Credits || screen_==Screen::Controls ||
        (!popups_.empty() && popups_.front().group!=PopupGroup::Commands);}
    bool wantsText()const{if(logonOpen_)return !hasMessageBox() && logonNameFocus_;return !hasMessageBox() && !(original_ && (screen_==Screen::GameDetails || screen_==Screen::Controls || screen_==Screen::Help || screen_==Screen::Credits)) && !missionBriefOpen_ && (popups_.empty() || popups_.front().group==PopupGroup::Commands) && (teamSettingsActive()?teamNameFocus_:findPlayerOpen_ || gameFilterEditing_ || editingAddress_ || editingWingmen_>=0 || editingPilot_ || editingLobbyPilot_>=0 || editingChat_ || screen_==Screen::Chat);}
    const std::string& teamSettingsName()const{return teamNameDraft_;}
    const std::string& chatText()const{return chat_;}
    bool lobbyQuickChat();
    void lobbyQuickChatSent(int command);
    void chatSent();
    std::string outgoingChatText()const;
    void statsSaved(std::string text){statsFeedback_=std::move(text);resultsChat_.dirty=true;rebuild();}
    const EndgameState& results()const{return endgame_;}
    const std::string& resultReason()const{return postgameReason_;}
    const std::string& serverAddress()const{return address_;}
    const Net::Instance& serverInstance()const{return joinInstance_;}
    void gameListings(std::vector<Net::ServerListing> listings);
    void gameAuthorities(std::vector<Net::AuthorityListing>);
    const std::string& createAddress()const{return createAuthority_;}
    const std::string& createName()const{return gameNameDraft_;}
    const char* createCore()const{return createCoreFile_.c_str();}
    void foundPlayer(std::optional<Net::ServerListing> game);
    const std::string& playerSearchName()const{return findPlayerName_;}
    int networkSide()const{return networkSide_;}
    const MenuView& view()const{return view_;}
    Screen screen()const{return screen_;}
    // Null means a popup over the live flight/command view.
    std::optional<Screen> consoleScreen()const;
    void networkSession(const SessionOptions& session,std::string_view serverName={}){session_=session;setupPresetSelection_=-1;buttonChecks_.clear();updateCoreCivilizations();networkServerName_=serverName;teamPage_=session.team;lastMissionPolicy_.reset();briefing_.reset();missionBriefStrings_.clear();if(original_ && screen_==Screen::Lobby && session_.scenario==Scenario::Conquest)setupSnapshot_=session;ready_=false;rebuild();}
    const SessionOptions& session()const{return session_;}
    void error(std::string message,bool resume=false);
    void quickChats(const QuickChatCatalog& catalog){quickChat_=&catalog;}
    void parts(PartKind kind,int mount,std::vector<PartChoice> choices);
    void orderTargets(int ship,int command,std::vector<OrderTarget> targets);
    void teleports(std::vector<TeleportChoice> choices,const GameState& state){teleports_=std::move(choices);open(Screen::Teleport,&state);}
    void researchService(std::function<std::vector<ResearchChoice>()> service){researchService_=std::move(service);}
    void teleportService(std::function<std::vector<TeleportChoice>()> service){teleportService_=std::move(service);}
    void crew(std::vector<CrewChoice> choices,const GameState& state){crew_=std::move(choices);open(Screen::Crew,&state);}
    void chatSector(int sector,const GameState& state);
    void cargoMounts(std::vector<PartChoice> choices,const GameState& state){choices_=std::move(choices);partPage_=0;open(Screen::CargoMount,&state);}
    void loadouts(std::vector<PartChoice> choices,const GameState& state){loadoutChoices_=std::move(choices);open(Screen::Loadouts,&state);}
    PartKind partKind()const{return partKind_;}int partMount()const{return partMount_;}
    void setPlaying(bool playing){playing_=playing;if(!playing){lastMissionPolicy_.reset();allianceRipDisabled_=false;if((screen_==Screen::ShipLoadout || loadoutSuspended_) && loadoutPreview_.valid && rememberLoadout_)rememberLoadout_(loadoutPreview_.loadout);loadoutPreview_={};loadoutSuspended_=false;complete_=false;inactive_=false;endgame_={};resultsDirty_=true;lastModalSerial_=0;
        networked_=networkLobby_=networkHost_=networkReady_=commander_=false;networkHostPilot_=-1;networkCountdown_=-1;networkPilots_.clear();teamScores_.clear();pilots_.clear();lobbyMessages_.clear();lobbyChat_={};resultsChat_={};lobbyUnread_={};lobbyChatSeen_=0;lobbyChatTab_=-1;lobbyChatActive_=false;lobbyFlags_=forceReady_=readySides_=0;autoAccept_=0x3f;}}
private:
    const CivilizationChoice* teamCivilization(int side)const;
    std::string teamName(int side)const;
    bool teamActive(int side)const;
    void retainTeamCivilizations();
    void assignKey(SDL_Scancode scan,uint8_t modifiers);
    enum class PopupGroup {Options,Main,Device,Graphics,Advanced,Sound,Game,Chat,Feedback,Mouse,LoadoutHulls,LoadoutParts,LoadoutOtherParts,Player,Commands,Combo,QuickChat,Alliance};
    enum class PopupOption {Chase,Linear,DeadZone,VirtualJoystick,FlipY,JoystickIndicator,MouseAcceleration,ThrottleDelay,SensitivityUp,SensitivityDown,Feedback,FeedbackUp,FeedbackDown,EffectsUp,EffectsDown,VoiceUp,VoiceDown,Particles,Censor,FilterAll,FilterQuick,FilterUnknown,Debris,FilterLobby,ChatLinesUp,ChatLinesDown,ChatSize,HudStyle,BrightnessUp,BrightnessDown,AutoCenter,Antialiasing,TextureSize};
    struct PopupRow {std::string label;char key{};MenuAction action{MenuOp::PopupItem};std::optional<PopupGroup> submenu;bool enabled{true};};
    struct PopupLevel {PopupGroup group;int selected{};glm::vec4 bounds{};std::vector<PopupRow> rows;ScrollList list;int described{-1};std::string combo;};
    void comboPopup(std::string source,std::vector<PopupRow> rows);
    std::unordered_map<std::string,int> comboSelections_;
    void originalEdit(const OriginalControl& face,std::string text,std::string_view script,bool focused,bool readOnly=false);
    void editTyped(std::string_view source){editTimes_[std::string(source)]=uiTime();}
    std::unordered_map<std::string,double> editTimes_;
    std::vector<PopupRow> popupRows(PopupGroup group)const;
    std::vector<PopupRow> quickPopupRows(int menu)const;
    void originalPopup(bool append=false);
    FrontResult dispatch(MenuAction action,size_t index=size_t(-1));
    void toggleButton(size_t index);
    void restoreButtonChecks(MenuView& view);
    void loadoutPartHover(const PopupRow& row);
    void closePopupChildren(int scrollId);
    bool popupWheel(float amount);
    void selectPopup(int level,int row,bool expand);
    FrontResult activatePopup(int level,int row);
    FrontResult popupKey(SDL_Keycode key);
    FrontResult closePopup();
    bool allianceMenuAvailable()const;
    bool syncMissionPolicy(const GameState& state);
    bool allianceRipDisabled_{};
    std::optional<Net::MissionPolicy> lastMissionPolicy_;
    void restorePopupParent();
    FrontResult closeSecondary();
    FrontResult closeMappings(bool accept);
    std::shared_ptr<HudScalar> creditsOffset_;
    void originalMappings();
    void originalRoster();
    FrontResult lobbyVisit(bool settings,bool cancelRequest);
    void clearLobbySelection(){rosterSelected_=-100000;rosterList_.selection=-1;lobbyRecipient_=-1;}
    void listBlank(glm::vec4 bounds,MenuOp selection,int group=0);
    void scrollbar(ScrollList& list,int id);
    ScrollList& scrollList(int id){return id==-10?lanList_:id==-9?createCoreList_:id==-8?createServerList_:id==-7?detailsList_:id==-6?lobbyChat_.list:id==-5?gamesList_:id==-4?resultsChat_.list:id==-3?resultsList_:id==-2?researchList_:id<0?teleportList_:id>=2?popups_.at(size_t(id-2)).list:id==0?mappingList_:rosterList_;}
    void editMapping();
    void finishMapping(){mappingEditing_=false;mappingPending_={};rebuild();}
    void addMappingKey(SDL_Scancode scan,uint8_t modifiers,bool overwrite=false);
    void addMappingButton(int device,int button,bool overwrite=false);
    void addMappingAxis(int device,int axis,bool hat,bool overwrite=false);
    void rebuild();
    void rebuildLayout();
    void originalMessageBox();
    void pendingLobbyMessage(const char* message,uint64_t action);
    void originalLayout();
    void prepareMissionDetails();
    void beginDetails();
    void prepareReport(std::string_view text,const UiStrings& strings);
    void originalMissionDetails();
    bool insideDetails()const{auto p=pointerPosition_-glm::vec2(detailsBounds_);return p.x>=0 && p.y>=0 && p.x<detailsBounds_.z && p.y<detailsBounds_.w;}
    std::function<Allegiance::MissionDetails()> detailsService_;
    MenuView detailsBackdrop_;Screen detailsReturn_{Screen::Hangar};bool detailsResume_{};
    ScrollList detailsList_;std::vector<OriginalControl> detailsControls_;MmlPage detailsPage_;
    glm::vec4 detailsBounds_{};mutable MenuView detailsView_;size_t detailsItemsBegin_{};
    bool originalSecondaryLayout();
    void originalGames();
    void gameFilterCombo(int field);
    bool gamesKey(SDL_Keycode key,FrontResult& result);
    void sortGames();
    std::vector<Net::AuthorityListing> authorities_;std::string createAuthority_;bool createServerSelected_{true},createCoreSelected_{true};
    std::string createCoreFile_{"static_core"},civilizationCore_{"static_core"};std::vector<std::string> localCores_,createCores_;
    void createCoreCatalog();
    void updateCoreCivilizations();
    bool canCreateGame()const;
    std::vector<Net::ServerListing> games_;std::vector<size_t> gamesRows_,lanRows_;ScrollList gamesList_,createServerList_,createCoreList_,lanList_;
    std::string gameSelected_,gameNameFilter_,gameNameDraft_,lanSelected_,networkServerName_;
    Net::Instance joinInstance_{};
    std::vector<std::string> gameCoreNames_;
    std::array<int,11> gameFilters_{2,2,2,2,2,2,2,2,2,2,2},gameFilterDraft_{};
    std::vector<std::pair<int,bool>> gameSorts_{{6,true}};
    bool gameFilterOpen_{},gameFilterEditing_{},gameCreateOpen_{},findPlayerOpen_{},findPlayerPending_{};
    std::string findPlayerName_;
    void originalTeleport();
    void originalResearch();
    void originalResults();
    FrontResult sendChatAction()const;
    EndgameState endgame_;int postgameSide_{-1};std::string postgameReason_,statsFeedback_;
    ScrollList resultsList_;std::vector<size_t> resultsPlayers_,resultsTeams_;
    int resultsPlayerSort_{5},resultsTeamSort_{6},resultsPlayerSelected_{-1},resultsTeamSelected_{-1};
    bool resultsPlayerReverse_{true},resultsTeamReverse_{},resultsDirty_{true};
    struct ResultChatLine {std::string text;glm::vec4 color;bool leader{};uint64_t serial{};unsigned line{};};
    struct ChatHistory {std::vector<ResultChatLine> lines;ScrollList list;bool dirty{true},censor{};double until{};int channel{-2},font{};float width{};};
    ChatHistory resultsChat_,lobbyChat_;
    std::array<bool,3> lobbyUnread_{};uint64_t lobbyChatSeen_{};
    void originalChatHistory(int id,int channel=-1);
    void chatScrolled(int id){auto& pane=id==-6?lobbyChat_:resultsChat_;pane.until=pane.list.offset<pane.list.maximum()?uiTime()+10:0;}
    bool researchVisible(const ResearchChoice& row)const;
    bool researchGroupEnabled(int group)const;
    void researchSelection(int id,bool reveal);
    void refreshResearchChoices();
    void originalCommandMenu();
    void originalLoadout();
    void initializeLoadoutCalibration();
    struct LoadoutCalibration {std::array<float,8> hull;UiNumbers part;};
    std::optional<LoadoutCalibration> loadoutCalibration_;
    void originalGameSettings();
    void originalTeamSettings();
    bool teamSettingsActive()const{return teamSettingsOpen_ && (screen_==Screen::RemoteLobby || screen_==Screen::Pilots || screen_==Screen::LobbyPilots || screen_==Screen::Countdown);}
    void originalMissionBriefing();
    void originalLogon();
    std::shared_ptr<const Allegiance::MissionBriefing> briefing_;
    std::function<std::shared_ptr<const Allegiance::MissionBriefing>(SessionOptions&)> prepareSession_;
    std::function<void()> discardPreparedSession_;
    bool preparedSession_{};double countdownClock_{};
    void originalMapPreview(bool interactive=true);
    bool teamSettingsOpen_{},teamNameFocus_{};int teamSettingsSide_{-1},teamCivDraft_{-1};std::string teamNameDraft_;
    void setupApply(int field,int choice,bool recognize=true);
    void readSetupChecks();
    void recognizeSetupPreset();
    int setupSelection(int field)const;
    enum class SetupField {GameType,Conquest,Territory,Prosperity,Artifacts,Flags,Kills,Time,Teams,Lives,Money,Helium,Resources,Map,Connectivity,MapSize,Treasures,Asteroids,Miners,Drones,Imbalance};
    struct SetupCombo {SetupField field;const char* control;std::vector<OriginalChoice> choices;};
    std::vector<SetupCombo> setupCombos_;
    int setupPresetSelection_{-1};
    std::vector<Allegiance::CustomMapInfo> customMaps_;
    SessionOptions setupSnapshot_;Screen setupReturn_{Screen::Scenarios};bool setupEditable_{},setupReadySnapshot_{};
    std::array<int,4> setupPreviewKey_{{-1,-1,-1,-1}};std::vector<OriginalControl> setupPreview_;
    std::string setupPreviewFile_;
    SetupMapPreview setupMap_;glm::vec2 setupMapDrag_{},setupMapExtent_{},setupMapSpan_{};glm::vec4 setupMapBounds_{};
    bool setupMapCanDrag_{},setupMapDirty_{};
    void sound(SoundID id){feedback_.emit(id);}
    bool lobbyReady()const;
    bool lobbyAvailable()const;
    Allegiance::LobbyStartState lobbyStartState()const;
    void button(glm::vec4 bounds,std::string label,MenuAction action,bool selected=false,bool enabled=true,std::string value={});
    void row(float y,std::string name,std::string value,MenuOp op,int id=0,bool enabled=true);
    void line(float x,float y,std::string text,float width=640,bool heading=false);
    const QuickChatCatalog* quickChat_{};std::vector<int> quickPath_;int quickPage_{},quickChannel_{1};
    std::string chat_;int chatChannel_{},chatSector_{-1};bool editingChat_{};std::deque<GameMessage> lobbyMessages_;
    std::string address_{"127.0.0.1:27500"};int networkSide_{};bool editingAddress_{};
    bool logonOpen_{},logonNameFocus_{};std::string logonNameDraft_;
    bool trainingCommand_{};
    Settings& settings_;Settings draft_,defaults_;SessionOptions session_;
    std::unique_ptr<OriginalUi> original_;
    std::unique_ptr<UiOverlayLayout> overlays_;
    uint32_t overlayModes_{};
    std::string messageBox_;bool messageBoxResume_{},messageBoxPending_{};uint64_t messageBoxAction_{};int messageFocus_{};
    enum class MessagePurpose {Ordinary,TrainingSuggestion,TrainingWelcome};
    MessagePurpose messagePurpose_{};bool trainingSuggestionVisited_{},messageDontAskAgain_{};
    void trainingDialog(bool suggestion);
    std::optional<FrontResult> messageBoxAccept_;
    MenuView messageBackdrop_;mutable MenuView messageView_;size_t messageItemsBegin_{};
    float displayScale_{1};
    std::function<LoadoutPreview(const Loadout&,bool,int)> previewLoadout_;
    std::function<std::vector<PartChoice>(PartKind,int,int)> previewParts_;
    std::function<std::optional<Loadout>(int,int)> cachedLoadout_;
    std::function<void(const Loadout&)> rememberLoadout_;
    std::function<std::optional<LoadoutSlotPreview>(int,int)> previewPartInfo_;
    LoadoutPreview loadoutPreview_;bool loadoutSuspended_{};int hoverSlot_{-1};glm::vec2 popupPosition_{10,10};
    std::optional<LoadoutSlotPreview> hoverPart_;bool loadoutMenuInfo_{};
    std::vector<PartChoice> loadoutPrimaryParts_,loadoutOtherParts_;
    int trainingSelection_{1},trainingNext_{1};
    int scenarioSelection_{5};
    double creditsStarted_{};Screen secondaryReturn_{Screen::Home};MenuView secondaryBackdrop_;bool secondaryResume_{},slideshowActive_{};
    Allegiance::HelpMotion helpMotion_;MenuView helpView_;
    void initializeHelp();
    std::optional<MenuAction> pointerAction_;std::string pointerControl_;Screen pointerScreen_{};
    Screen screen_{Screen::Home},settingsReturn_{Screen::Home};MenuView view_;
    Settings mappingSnapshot_;Screen mappingReturn_{Screen::Home};ScrollList mappingList_,rosterList_;
    MenuView mappingParentView_;std::vector<PopupLevel> mappingParentPopups_;
    std::vector<OriginalControl> mappingBackdrop_;
    bool mappingOverlay_{};
    MenuView popupBackdrop_,menuPopupScene_;mutable MenuView menuPopupView_;Screen popupReturn_{Screen::Home};bool popupFlight_{};
    std::optional<std::pair<size_t,size_t>> popupLayer_;size_t popupItemsBegin_{};
    void markPopupLayer(){if(!popupLayer_){auto begin=view_.popupArtworkBegin.value_or(view_.artwork.size());popupLayer_=std::pair(begin,view_.items.size());view_.popupArtworkBegin=begin;}}
    void cachePopupLayer();
    std::vector<PopupLevel> popups_;
    std::vector<PopupRow> commandRows_;Screen classicReturn_{Screen::Home};
    struct RosterRow {int id{};std::string name,status,wing;bool commander{},host{},controlled{},muted{},requesting{},ready{};int rank{},wingID{-1};};
    std::vector<RosterRow> roster_,rosterScratch_;std::vector<size_t> rosterLinks_;
    Allegiance::LobbyRosterSort rosterSort_{Allegiance::LobbyRosterSort::Leader};bool rosterReverse_{},lobbyChatActive_{};int rosterHeader_{-1},lobbyChatTab_{-1};
    int rosterSelected_{-100000},lobbyRecipient_{-1};bool rosterFocus_{},rosterFlash_{},hasRequests_{};
    int contextPilot_{-1};SDL_Keycode popupTextKey_{};
    bool mappingEditing_{};
    struct MappingPending {enum Type {None,Key,Button,Axis};Type type{};int device{},input{},conflict{-1};uint8_t modifiers{};bool hat{};} mappingPending_;
    std::vector<std::pair<uint64_t,float>> mappingAxes_;
    std::array<std::string,16> mappingDevices_;
    glm::vec2 pointerPosition_{};float scrollGrab_{};double scrollRepeat_{};
    double countdown_{-1};Screen countdownReturn_{Screen::Lobby};int lobbyPilotPage_{},editingLobbyPilot_{-1},nextLobbyPilot_{1};std::string lobbyPilotDraft_;
    int editingWingmen_{-1};std::string wingmenDraft_;
    Allegiance::CountdownSounds countdownSounds_;UiSoundFeedback feedback_;
    bool gameplayClosed()const{return complete_ || inactive_ || networkLobby_;}
    bool playing_{},ready_{},editingPilot_{},docked_{},complete_{},inactive_{};int binding_{-1},bindingPage_{},selectedCargo_{-1};
    std::string ship_,sector_,station_,equipment_,result_;int kills_{},deaths_{},stationId_{-1};
    bool ghost_{};bool passenger_{};bool cinematic_{};bool developments_{true};int turret_{-1};int parentId_{-1},crewPage_{};std::vector<CrewChoice> crew_;
    std::array<CommandStatus,3> commands_;
    std::vector<TeleportChoice> teleports_;int teleportPage_{},teleportSelected_{-1};bool teleportShipSelected_{};
    bool contextCapture_{};
    struct ButtonCheck {Screen scope;bool base{},checked{};};
    std::unordered_map<std::string,ButtonCheck> buttonChecks_;
    ScrollList teleportList_;double teleportEntered_{};std::vector<OriginalControl> consoleClosing_;std::optional<MenuSlide> consoleClosingSlide_;bool consoleFlight_{};
    std::function<std::vector<TeleportChoice>()> teleportService_;
    std::function<std::vector<ResearchChoice>()> researchService_;
    std::vector<ResearchChoice> researchChoices_;
    ScrollList researchList_;int researchTab_{},researchSelected_{-1},researchHover_{-1};bool researchHoverInvest_{};
    bool researchShowAll_{},researchShowComplete_{true},researchFresh_{true},researchSelectFirst_{true};double researchEntered_{};
    std::vector<ControllerDevice> controllerDevices_;std::string controllerName_;
    std::function<std::vector<std::string>()> graphicsDeviceService_;
    std::function<bool()> mouseInputGet_;std::function<void(bool)> mouseInputSet_;
    std::vector<int> graphicsSamples_{0,2,4,8,16};
    int controllerAxes_{},controllerButtons_{},controllerHats_{},controllerPage_{},controllerActionPage_{},controllerButton_{};bool controllerGamepad_{};
    int mouseButton_{},mouseActionPage_{};
    int mapAxis_{},mapDevicePage_{},mapButtonPage_{},mapButton_{};
    int pilotPage_{},accountId_{-1},moneyAmount_{1000};std::vector<PilotState> pilots_;MoneyRequest moneyRequest_;bool commander_{};
    int teamPage_{},rulesPage_{};std::vector<TeamScore> teamScores_;
    int loadoutSlot_{};std::vector<PartChoice> loadoutChoices_;
    uint32_t lobbyFlags_{},autoAccept_{0x3f},forceReady_{},readySides_{};
    bool networkLobby_{},networkHost_{},networkReady_{};int networkCountdown_{-1},networkHostPilot_{-1},missionStage_{};std::vector<LobbyPilot> networkPilots_;
    int lobbyTimerSeconds_{};uint64_t lastModalSerial_{};
    bool missionBriefOpen_{};UiStrings missionBriefStrings_;float winMoney_{};
    bool economy_{},networked_{},authoredLoadouts_{};int money_{},researchPage_{},hullPage_{},unitId_{},targetType_{-1},targetId_{-1};
    int orderCommand_{},targetPage_{};std::vector<OrderTarget> orderTargets_;
    uint64_t economyRevision_{};std::string target_;
    std::vector<ResearchState> research_;std::vector<OrderUnit> units_;std::vector<PartChoice> hulls_;std::vector<CivilizationChoice> civilizations_;
    std::vector<EquipmentState> equipmentRows_;std::vector<PartChoice> choices_;PartKind partKind_{};int partMount_{},partPage_{},equipmentPage_{};
};
}
