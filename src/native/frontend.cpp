#include "frontend.hpp"
#include "../ClientCore/LobbyAlliances.h"
#include "effect_preferences.hpp"
#include "chat_censor.hpp"
#include "game.hpp"
#include "ui_clock.hpp"
#include "input_mapping.hpp"
#include "../ClientCore/LobbyReady.h"
#include "../ClientCore/GameTypes.h"
#include "../ClientCore/InvestmentPresentation.h"
#include "../ClientCore/MissionLifecycle.h"
#include "../ClientCore/TrainingMenuPresentation.h"
#include "../ClientCore/LogonPresentation.h"
#include "../ClientCore/LobbyActions.h"
#include "../ClientCore/ChatPresentation.h"
#include "../ClientCore/AuthoredText.h"
#include "../ClientCore/GameListPresentation.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace Native {
namespace {
std::string number(float value,int decimals=0){char s[48];snprintf(s,sizeof(s),"%.*f",decimals,value);return s;}
int cycle(int n,int count){return (n%count+count)%count;}
bool teamScreen(Screen screen){return screen==Screen::RemoteLobby || screen==Screen::Pilots || screen==Screen::LobbyPilots || screen==Screen::Countdown;}
}
const CivilizationChoice* Frontend::teamCivilization(int side)const{
    if(side<0 || side>=session_.rules.teams)return nullptr;
    if((networked_ || playing_) && side<int(teamScores_.size())){
        auto choice=std::find_if(civilizations_.begin(),civilizations_.end(),[&](const auto& c){return c.id==teamScores_[side].civilization;});
        if(choice!=civilizations_.end())return &*choice;
    }
    int index=teamCivilizationIndex(session_,side,int(civilizations_.size()));
    return index<0?nullptr:&civilizations_[index];
}
std::string Frontend::teamName(int side)const{
    if(side==noTeam)return "No Team";
    if(side<0 || side>=session_.rules.teams)return {};
    if((networked_ || playing_) && side<int(teamScores_.size()))return teamScores_[side].name;
    return session_.teamNames[side].empty()?"Team "+std::to_string(side+1):session_.teamNames[side];
}
bool Frontend::teamActive(int side)const{
    if(side<0 || side>=session_.rules.teams)return false;
    if((networked_ || playing_) && side<int(teamScores_.size()))return teamScores_[side].active;
    return !(session_.inactiveTeams&(1u<<side));
}
void Frontend::retainTeamCivilizations(){
    if(civilizations_.empty())return;
    for(int side=0;side<int(session_.teamCivilizations.size());++side)if(session_.teamCivilizations[side]<0)
        session_.teamCivilizations[side]=teamCivilizationIndex(session_,side,int(civilizations_.size()));
}
Allegiance::LobbyStartState Frontend::lobbyStartState()const {
    std::array<Allegiance::LobbyTeamState,6> teams{};
    for(int side=0;side<session_.rules.teams;++side)teams[side].active=teamActive(side);
    if(networked_){
        for(const auto& pilot:networkPilots_)if(pilot.team>=0 && pilot.team<session_.rules.teams){
            ++teams[pilot.team].players;
            if(!pilot.ready && !(forceReady_&(1u<<pilot.team)))teams[pilot.team].ready=false;
        }
        auto reconnecting=Allegiance::LobbyReconnectingSides(lobbyFlags_);
        for(int side=0;side<session_.rules.teams;++side)if(reconnecting&(1u<<side))teams[side].ready=false;
    }else{
        ++teams[session_.team].players;
        teams[session_.team].ready=ready_ || (forceReady_&(1u<<session_.team));
        for(const auto& pilot:session_.localPilots)if(pilot.team>=0 && pilot.team<session_.rules.teams){
            ++teams[pilot.team].players;
            if(!pilot.ready && !(forceReady_&(1u<<pilot.team)))teams[pilot.team].ready=false;
        }
    }
    bool allowEmpty=networked_?!(lobbyFlags_&Allegiance::LobbyRequirePlayers):!session_.requireTeamPlayers;
    return Allegiance::LobbyStart(session_.rules.teams,allowEmpty?0:networked_?Allegiance::LobbyMinimumPlayers(lobbyFlags_):1,shipIdentityCapacity,session_.rules.maxImbalance,allowEmpty,
        [&](int side){return teams[side];});
}
bool Frontend::lobbyReady()const {
    return lobbyStartState().CanStart();
}
bool Frontend::lobbyAvailable()const {
    int count=0;for(const auto& p:networkPilots_)if(p.team==session_.team)++count;
    return count<shipIdentityCapacity;
}
bool Frontend::pageChat(bool up){
    auto host=screen_;
    if(hasMessageBox() || !original_ || (host!=Screen::Results && host!=Screen::Pilots && host!=Screen::RemoteLobby && host!=Screen::LobbyPilots))return false;
    int id=host==Screen::Results?-4:-6;auto& list=scrollList(id);list.position(list.offset+(up?-1:1)*list.page);chatScrolled(id);rebuild();return true;
}
bool Frontend::consoleBinding(Binding binding)const {
    if(!original_ || !playing_ || gameplayClosed() || keyboardPopup() || capturingKey() || wantsText())return false;
    if(screen_!=Screen::Hangar && screen_!=Screen::ShipLoadout && screen_!=Screen::Research && screen_!=Screen::Teleport)return false;
    return binding==Binding::Cursor || binding==Binding::PaneInventory || binding==Binding::Research || binding==Binding::Teleport ||
        binding==Binding::CommandView || binding==Binding::UnitOrders || binding==Binding::PilotOrders || binding==Binding::ToggleView ||
        binding==Binding::CameraBase || (binding>=Binding::CameraExternal && binding<=Binding::CameraChase);
}
FrontResult Frontend::tick(double seconds,bool wallClock) {
    for(auto* pane:{&resultsChat_,&lobbyChat_})if(pane->until && uiTime()>=pane->until){pane->until=0;pane->list.position(pane->list.maximum());rebuild();}
    if(consoleClosingSlide_ && uiTime()-consoleClosingSlide_->started>=consoleClosingSlide_->duration){consoleClosing_.clear();consoleClosingSlide_.reset();rebuild();}
    if(hasRequests_ || std::any_of(lobbyUnread_.begin(),lobbyUnread_.end(),[](bool unread){return unread;})){
        bool flash=Allegiance::LobbyRosterFlash(uiTime());
        if(flash!=rosterFlash_){rosterFlash_=flash;for(auto& c:view_.artwork){
            if(c.name.starts_with("request/"))c.color=glm::vec4(flash?.75f:1.f,flash?.75f:1.f,flash?.75f:1.f,1);
            else if(c.name.starts_with("chatHeader")){int column=c.name.back()-'0';if(column>=0 && column<3)c.checkedState=lobbyUnread_[column] && flash?2:0;}
        }}
    }
    if(pointerAction_ && (pointerAction_->op==MenuOp::ScrollLine || pointerAction_->op==MenuOp::ScrollPage)){
        scrollRepeat_-=std::max(0.,seconds);
        if(scrollRepeat_<=0){
            auto action=*pointerAction_;auto& list=scrollList(action.value);auto t=list.thumb();
            bool inside=pointerPosition_.x>=list.bar.x && pointerPosition_.x<list.bar.x+list.bar.z;
            if(action.op==MenuOp::ScrollPage)inside=inside && (action.delta<0?pointerPosition_.y>=list.track().y && pointerPosition_.y<t.y:pointerPosition_.y>=t.y+t.w && pointerPosition_.y<list.bar.y+list.bar.w-list.downHeight);
            else inside=inside && (action.delta<0?pointerPosition_.y>=list.bar.y && pointerPosition_.y<list.bar.y+list.upHeight:pointerPosition_.y>=list.bar.y+list.bar.w-list.downHeight && pointerPosition_.y<list.bar.y+list.bar.w);
            if(inside){list.position(list.offset+action.delta*(action.op==MenuOp::ScrollLine?(action.value==-7?8:list.row):list.page));if(action.value==-4 || action.value==-6)chatScrolled(action.value);rebuild();}
            scrollRepeat_=action.op==MenuOp::ScrollLine?.1:.25;
        }
    }
    if(countdown_<0)return {};
    if(!lobbyReady()){open(countdownReturn_);return {};}
    if(auto invalid=Game::missionError(session_,customMaps_);!invalid.empty()){open(countdownReturn_);error(std::move(invalid));return {};}
    if(wallClock){auto current=uiTime();seconds=std::max(0.,current-countdownClock_);countdownClock_=current;}
    int previous=int(std::ceil(countdown_));countdown_-=std::max(0.,seconds);sound(countdownSounds_.Observe(countdown_));
    if(countdown_<=0){countdown_=-1;return {FrontEffect::Start};}
    if(int(std::ceil(countdown_))!=previous)rebuild();return {};
}
Frontend::Frontend(Settings& settings,Settings defaults):settings_(settings),draft_(settings),defaults_(std::move(defaults)){session_.pilot=settings.pilot;rebuild();}
void Frontend::button(glm::vec4 b,std::string s,MenuAction a,bool selected,bool enabled,std::string value){view_.items.push_back({b,std::move(s),std::move(value),a,enabled,selected});}
void Frontend::line(float x,float y,std::string s,float w,bool heading){view_.text.push_back({{x,y},std::move(s),w,heading});}
void Frontend::row(float y,std::string name,std::string value,MenuOp op,int id,bool enabled) {
    line(270,y+8,std::move(name),270);
    button({565,y,40,32},"<",{op,id,-1},false,enabled);
    if(op==MenuOp::Enemies || op==MenuOp::Allies) {
        bool editing=editingWingmen_==(op==MenuOp::Enemies?0:1);
        button({616,y,280,32},editing?wingmenDraft_+"_":std::move(value),{op,id,0},editing,enabled);
    } else line(616,y+8,std::move(value),265);
    button({914,y,40,32},">",{op,id,1},false,enabled);
}
void Frontend::open(Screen screen,const GameState* state) {
    if(original_ && screen==Screen::Help){if(state)sync(*state);showHelp(true);return;}
    if(original_ && screen==Screen::Online && screen_!=screen){if(state)sync(*state);secondaryBackdrop_=view_;}
    if(screen!=screen_)logonOpen_=logonNameFocus_=false;
    if(screen_==Screen::Countdown && screen!=Screen::Countdown && preparedSession_){
        preparedSession_=false;if(discardPreparedSession_)discardPreparedSession_();briefing_.reset();missionBriefStrings_.clear();
    }
    if(original_ && screen==Screen::ConfirmLeave){
        if(state)sync(*state);
        bool resume=false;
        if(!popups_.empty() && (screen_==Screen::Settings || screen_==Screen::Pause)){resume=popupFlight_;restorePopupParent();}
        confirm(authoredLoadouts_ || slideshowActive_?"Quit the Current Training Mission?":"Quit the Current Game?",{FrontEffect::Leave},resume);
        return;
    }
    if(screen_!=screen && hasMessageBox()){
        messageBox_.clear();messageBoxAccept_.reset();messageBoxResume_=messageBoxPending_=false;messageBoxAction_=0;messagePurpose_=MessagePurpose::Ordinary;pointerAction_.reset();view_.focus=messageFocus_;
    }
    if(original_ && (screen==Screen::Settings || screen==Screen::Pause) && screen_!=screen){
        if(state)sync(*state);
        popupPosition_={10,10};popupReturn_=screen_;popupBackdrop_=view_;draft_=settings_;
        popupFlight_=slideshowActive_ || (playing_ && (!docked_ || trainingCommand_) && !networkLobby_ && !complete_);
        if(popupFlight_){popupBackdrop_={};popupBackdrop_.original=popupBackdrop_.overlay=true;}
        popups_={{screen==Screen::Settings?PopupGroup::Options:PopupGroup::Main}};
        if(screen_==Screen::ShipLoadout)loadoutSuspended_=true;
        screen_=screen;pointerAction_.reset();rebuild();return;
    }
    if(original_ && screen==Screen::Controls && screen_!=screen){
        if(state)sync(*state);
        mappingReturn_=screen_;mappingParentView_=view_;mappingParentPopups_=popups_;
        if(screen_!=Screen::Settings)draft_=settings_;
        mappingOverlay_=view_.overlay;mappingSnapshot_=draft_;mappingBackdrop_=view_.artwork;
        mappingList_={};mappingEditing_=false;mappingPending_={};popups_.clear();pointerAction_.reset();
        if(screen_==Screen::ShipLoadout)loadoutSuspended_=true;
        screen_=screen;rebuild();return;
    }
    if(original_ && (screen==Screen::Credits || screen==Screen::GameDetails) && screen_!=screen){
        if(state)sync(*state);
        bool resume=slideshowActive_ || (playing_ && !networkLobby_ && !complete_ && (!docked_ || trainingCommand_) && screen_!=Screen::Research && screen_!=Screen::Teleport);
        if(screen==Screen::GameDetails){
            beginDetails();prepareMissionDetails();
        }else {secondaryReturn_=screen_;secondaryBackdrop_=view_;secondaryResume_=resume;if(screen==Screen::Credits)creditsStarted_=uiTime();}
        pointerAction_.reset();view_.pressed=-1;screen_=screen;rebuild();return;
    }
    // WinTrek creates a new TrainingScreen on entry. Popup restoration keeps
    // the existing screen and selection; genuine navigation selects progression.
    if(original_ && screen==Screen::Training && screen_!=screen)trainingSelection_=trainingNext_;
    bool enteringTeam=original_ && teamScreen(screen) && !teamScreen(screen_);
    if(original_ && lobbyChatActive_ && teamScreen(screen_) && !teamScreen(screen))
        lobbyChatTab_=chatChannel_==int(ChatChannel::Team)?1:chatChannel_==int(ChatChannel::Leaders)?2:0;
    if(enteringTeam && screen_!=Screen::Settings && screen_!=Screen::Pause && screen_!=Screen::Controls && screen_!=Screen::Account){
        rosterSort_=Allegiance::LobbyRosterSort::Leader;rosterReverse_=false;rosterHeader_=-1;
    }
    bool loadoutPopup=screen_==Screen::ShipLoadout && (screen==Screen::Settings || screen==Screen::Pause || screen==Screen::Controls);
    if(loadoutPopup)loadoutSuspended_=true;
    if(screen_==Screen::ShipLoadout && screen!=screen_ && !loadoutPopup && loadoutPreview_.valid && rememberLoadout_)rememberLoadout_(loadoutPreview_.loadout);
    if(screen!=screen_){gameFilterOpen_=gameCreateOpen_=gameFilterEditing_=findPlayerOpen_=false;findPlayerPending_=false;}
    bool flightConsole=consoleFlight_;consoleFlight_=false;
    if(original_ && (screen_==Screen::Teleport || screen_==Screen::Research) && screen!=screen_ && view_.slide){
        consoleClosing_.assign(view_.artwork.begin()+view_.slide->artworkBegin,view_.artwork.begin()+std::min(view_.artwork.size(),view_.slide->artworkEnd));
        consoleClosingSlide_=view_.slide;consoleClosingSlide_->started=uiTime();consoleClosingSlide_->closing=true;
    }
    if(screen!=Screen::Hangar && screen!=Screen::ShipLoadout && screen!=Screen::Pause && screen!=Screen::Controls && screen!=Screen::Research && screen!=Screen::Teleport){
        consoleClosing_.clear();consoleClosingSlide_.reset();
    }
    if(screen==Screen::Teleport && screen_!=screen){teleportList_={};teleportSelected_=-1;teleportShipSelected_=false;teleportEntered_=uiTime();consoleClosing_.clear();consoleClosingSlide_.reset();}
    if(original_ && screen==Screen::Research && (screen_!=screen || flightConsole)){
        researchEntered_=uiTime();researchHover_=-1;researchFresh_=true;consoleClosing_.clear();consoleClosingSlide_.reset();
        refreshResearchChoices();
    }
    bool enteringSetup=original_ && screen==Screen::Lobby && session_.scenario==Scenario::Conquest && screen_!=screen;
    if(enteringSetup){setupSnapshot_=session_;setupReturn_=screen_;setupReadySnapshot_=ready_;setupPresetSelection_=-1;}
    if(screen!=screen_ && screen!=Screen::Settings && screen!=Screen::Pause && screen!=Screen::Controls && screen!=Screen::GameDetails && screen!=Screen::Credits && screen!=Screen::Help &&
       screen_!=Screen::Settings && screen_!=Screen::Pause && screen_!=Screen::Controls && screen_!=Screen::GameDetails && screen_!=Screen::Credits && screen_!=Screen::Help)buttonChecks_.clear();
    if(screen!=screen_)classicReturn_=screen_;
    if(screen==Screen::Results && screen_!=screen){resultsPlayerSort_=5;resultsPlayerReverse_=true;resultsTeamSort_=6;resultsTeamReverse_=false;resultsList_={};resultsChat_={};resultsPlayerSelected_=resultsTeamSelected_=-1;resultsDirty_=true;statsFeedback_.clear();lobbyRecipient_=-1;chatChannel_=int(ChatChannel::Everyone);}
    chatSector_=-1;
    teamSettingsOpen_=false;
    pointerAction_.reset();
    if(screen!=Screen::Settings && screen!=Screen::Pause)popups_.clear();
    if(screen!=Screen::Pilots && screen!=Screen::RemoteLobby && screen!=Screen::LobbyPilots)lobbyRecipient_=-1;
    if((screen==Screen::Pilots || screen==Screen::RemoteLobby || screen==Screen::LobbyPilots) && screen_!=Screen::Account && screen_!=screen){rosterList_={};rosterSelected_=-100000;lobbyRecipient_=-1;rosterFocus_=false;}
    if(screen==Screen::ShipLoadout && screen_!=screen && previewLoadout_){loadoutPreview_=previewLoadout_(loadoutSuspended_ && loadoutPreview_.valid?loadoutPreview_.loadout:Loadout{},false,-1);loadoutSuspended_=false;hoverSlot_=-1;hoverPart_.reset();loadoutMenuInfo_=false;}
    if(screen==Screen::Hangar || screen==Screen::Results || screen==Screen::Home)loadoutSuspended_=false;
    if(screen==Screen::Settings && screen_!=Screen::ControllerMap && screen_!=Screen::ControllerMapAxis && screen_!=Screen::ControllerMapDevices && screen_!=Screen::ControllerMapButtons && screen_!=Screen::ControllerMapButton && screen_!=Screen::ControllerMapAction && screen_!=Screen::Mouse && screen_!=Screen::MouseButtons && screen_!=Screen::MouseAction && screen_!=Screen::ChatPreferences && screen_!=Screen::Visuals && screen_!=Screen::Interface && screen_!=Screen::Audio && screen_!=Screen::Movies && screen_!=Screen::Controls && screen_!=Screen::Controller && screen_!=Screen::ControllerAxes && screen_!=Screen::ControllerButtons && screen_!=Screen::ControllerAction && screen_!=Screen::Settings){settingsReturn_=screen_;draft_=settings_;}
    if(screen==Screen::Chat && screen_!=Screen::Chat)chat_.clear();
    if(screen==Screen::QuickChat && screen_!=Screen::QuickChat){quickPath_={0};quickPage_=0;}
    if(screen!=Screen::Countdown)countdown_=-1;
    screen_=screen;editingLobbyPilot_=-1;editingWingmen_=-1;editingPilot_=false;binding_=-1;editingChat_=original_ && (screen==Screen::RemoteLobby || screen==Screen::LobbyPilots || screen==Screen::Pilots || screen==Screen::Results);view_.notice.clear();view_.focus=0;
    if(state && state->endgame.frozen){if(endgame_!=state->endgame)resultsDirty_=true;endgame_=state->endgame;postgameSide_=state->playerSide;postgameReason_=state->result;}
    if(state){briefing_=state->briefing;overlayModes_=originalOverlayModes(*state);complete_=state->complete;inactive_=!state->sideActive;ship_=state->ship;sector_=state->sector;station_=state->station;stationId_=state->stationId;docked_=state->docked;selectedCargo_=state->selectedCargo;ghost_=state->ghost;passenger_=state->passenger;cinematic_=state->cinematic;turret_=state->turret;parentId_=state->parentId;kills_=state->kills;deaths_=state->deaths;equipment_.clear();
        lobbyTimerSeconds_=std::max(0,int(session_.rules.minutes?state->timeRemaining:state->time));
        for(auto& e:state->equipment){if(!equipment_.empty())equipment_+="\n";equipment_+=e.name;}
        pilots_=state->pilots;moneyRequest_=state->moneyRequest;commander_=state->commander;commands_=state->commands;result_=state->result;teamScores_=state->teamScores;equipmentRows_=state->equipment;networkLobby_=state->networkLobby;networkHost_=state->networkHost;networkReady_=state->networkReady;networkCountdown_=state->networkCountdown;networkPilots_=state->networkPilots;lobbyFlags_=state->lobbyFlags;autoAccept_=state->autoAccept;forceReady_=state->forceReady;readySides_=state->readySides;networked_=state->networked;economy_=state->economy;authoredLoadouts_=state->trainingMission!=0;trainingCommand_=state->commandView;money_=state->money;hulls_=state->hullChoices;
        missionStage_=state->missionStage;
        developments_=state->developments;economyRevision_=state->economyRevision;research_=state->research;units_=state->units;target_=state->target;targetId_=state->targetId;targetType_=state->targetObjectType;lobbyMessages_=state->messages;lobbyChat_.dirty=resultsChat_.dirty=true;networkHostPilot_=state->networkHostPilot;
    }
    if(screen==Screen::Pilots || screen==Screen::RemoteLobby || screen==Screen::LobbyPilots){lobbyChatSeen_=lobbyMessages_.empty()?0:lobbyMessages_.back().serial;lobbyUnread_={};}
    if(state)syncMissionPolicy(*state);
    if(state && state->networked)session_.requireTeamPlayers=(state->lobbyFlags&Allegiance::LobbyRequirePlayers)!=0;
    if(enteringTeam && state)for(const auto& p:networkPilots_)if(p.controlled)teamPage_=p.team;
    if(enteringTeam){
        allianceRipDisabled_=std::any_of(session_.alliances.begin(),session_.alliances.end(),[](char group){return group>=0;}) && !session_.rules.alliedRipcord;
        lobbyChatActive_=true;
        int side=session_.team;for(const auto& p:networkPilots_)if(p.controlled)side=p.team;
        int tab=lobbyChatTab_;
        if(tab<0 || (tab==2 && !(networked_?commander_:!playing_ || commander_)))tab=side==noTeam?0:1;
        chatChannel_=int(tab==0?ChatChannel::Everyone:tab==1?ChatChannel::Team:ChatChannel::Leaders);
    }
    if(enteringSetup)setupEditable_=!networked_ || (networkHost_ && networkLobby_ && missionStage_==STAGE_NOTSTARTED && !(lobbyFlags_&Allegiance::LobbyObjectModelCreated));
    rebuild();
}
std::optional<Screen> Frontend::consoleScreen()const {
    auto parent=screen_;
    for(int depth=0;depth<8;++depth){
        switch(parent){
        case Screen::Pause:case Screen::Settings:
            if(popupFlight_)return {};
            parent=popupReturn_;continue;
        case Screen::Controls:
            if(mappingOverlay_)return {};
            parent=mappingReturn_;continue;
        case Screen::Help:case Screen::Credits:
            if(secondaryResume_)return {};
            parent=secondaryReturn_;continue;
        case Screen::GameDetails:
            if(detailsResume_)return {};
            parent=detailsReturn_;continue;
        default:return parent;
        }
    }
    return parent;
}
bool Frontend::releaseDockedView(const GameState& state) {
    // SetViewMode retires the old console on docking, command/combat changes
    // and completion of an override, as well as forced launch/ghost recovery.
    // A fresh explicitly opened destination already holds the new state.
    if(!playing_ || state.complete || state.networkLobby ||
       !(docked_!=state.docked || trainingCommand_!=state.commandView ||
         (cinematic_ && !state.cinematic) || (!ghost_ && state.ghost)))return false;
    auto parent=consoleScreen();
    if(parent && *parent!=Screen::Hangar && *parent!=Screen::ShipLoadout && *parent!=Screen::Teleport && *parent!=Screen::Research)return false;
    // LoadoutImpl's destructor remembers the draft even through a popup.
    if((loadoutSuspended_ || (parent==Screen::ShipLoadout && screen_!=*parent)) && loadoutPreview_.valid && rememberLoadout_)rememberLoadout_(loadoutPreview_.loadout);
    messageBox_.clear();messageBoxAccept_.reset();messageBoxResume_=messageBoxPending_=false;
    messageBoxAction_=0;messagePurpose_=MessagePurpose::Ordinary;
    open(Screen::Hangar,&state);
    mappingEditing_=false;mappingPending_={};
    consoleClosing_.clear();consoleClosingSlide_.reset();consoleFlight_=false;
    return true;
}
void Frontend::sync(const GameState& state) {
    auto modes=originalOverlayModes(state);bool overlayChanged=overlayModes_!=modes;overlayModes_=modes;
    bool policyChanged=syncMissionPolicy(state);
    if(sector_!=state.sector)sector_=state.sector;
    if(briefing_!=state.briefing){briefing_=state.briefing;missionBriefStrings_.clear();if(missionBriefOpen_)rebuild();}
    bool stageChanged=missionStage_!=state.missionStage;missionStage_=state.missionStage;
    if(screen_==Screen::Results && state.networked && !state.complete && state.playerSide>=0 && (state.missionStage==STAGE_STARTING || state.missionStage==STAGE_STARTED)){open(Screen::RemoteLobby,&state);return;}
    if(state.endgame.frozen && (!endgame_.frozen || !complete_ && state.complete)){endgame_=state.endgame;postgameSide_=state.playerSide;postgameReason_=state.result;resultsDirty_=true;if(screen_==Screen::Results)rebuild();}
    bool crewChanged=passenger_!=state.passenger || parentId_!=state.parentId || turret_!=state.turret;
    bool loadoutContextChanged=screen_==Screen::ShipLoadout && (crewChanged || docked_!=state.docked || ghost_!=state.ghost || stationId_!=state.stationId);
    bool chatChanged=original_ && (screen_==Screen::RemoteLobby || screen_==Screen::Pilots || screen_==Screen::LobbyPilots || screen_==Screen::Results) &&
        (lobbyMessages_.size()!=state.messages.size() || (!state.messages.empty() && (lobbyMessages_.empty() || lobbyMessages_.back().serial!=state.messages.back().serial)));
    if(chatChanged){
        if(screen_!=Screen::Results)for(const auto& m:state.messages)if(m.serial>lobbyChatSeen_ && m.fromPlayer){
            int column=m.chatTarget==CHAT_EVERYONE?0:m.chatTarget==CHAT_TEAM?1:m.chatTarget==CHAT_LEADERS?2:-1;
            int current=chatChannel_==int(ChatChannel::Everyone)?0:chatChannel_==int(ChatChannel::Team)?1:chatChannel_==int(ChatChannel::Leaders)?2:-1;
            if(column>=0 && column!=current)lobbyUnread_[column]=true;
        }
        lobbyChatSeen_=state.messages.empty()?0:state.messages.back().serial;
        lobbyMessages_=state.messages;lobbyChat_.dirty=resultsChat_.dirty=true;
    }
    if(screen_==Screen::Results && (networkCountdown_!=state.networkCountdown || lobbyFlags_!=state.lobbyFlags || stageChanged)){networkCountdown_=state.networkCountdown;lobbyFlags_=state.lobbyFlags;rebuild();}
    if(original_ && screen_==Screen::Teleport && teleportService_){
        bool changed=docked_!=state.docked || crewChanged || complete_!=state.complete;
        if(changed || economyRevision_!=state.economyRevision){
            auto choices=teleportService_();if(changed || choices!=teleports_){teleports_=std::move(choices);docked_=state.docked;passenger_=state.passenger;complete_=state.complete;rebuild();}
        }
    }
    passenger_=state.passenger;parentId_=state.parentId;turret_=state.turret;
    bool hostChanged=networkHost_!=state.networkHost || networkHostPilot_!=state.networkHostPilot;networkHost_=state.networkHost;networkHostPilot_=state.networkHostPilot;networkLobby_=state.networkLobby;networked_=state.networked;
    bool terminalChanged=complete_!=state.complete || inactive_!=!state.sideActive || developments_!=state.developments || ghost_!=state.ghost;
    developments_=state.developments;
    complete_=state.complete;inactive_=!state.sideActive;ghost_=state.ghost;cinematic_=state.cinematic;trainingCommand_=state.commandView;docked_=state.docked;stationId_=state.stationId;
    if(overlayChanged && original_ && (screen_==Screen::Teleport || screen_==Screen::Research))rebuild();
    if(hostChanged && screen_==Screen::Results)rebuild();
    if(original_ && screen_==Screen::Lobby && session_.scenario==Scenario::Conquest){
        bool editable=!networked_ || (networkHost_ && networkLobby_ && missionStage_==STAGE_NOTSTARTED && !(state.lobbyFlags&Allegiance::LobbyObjectModelCreated));
        if(editable!=setupEditable_ || policyChanged){setupEditable_=editable;setupPresetSelection_=-1;buttonChecks_.clear();popups_.clear();pointerAction_.reset();rebuild();}
    }
    if(screen_==Screen::RemoteLobby || (networked_ && screen_==Screen::Pilots)){
        int previousSide=session_.team;for(const auto& pilot:networkPilots_)if(pilot.controlled)previousSide=pilot.team;
        for(const auto& pilot:state.networkPilots)if(pilot.controlled){
            if(pilot.team!=previousSide){
                // TeamScreen::OnAddPlayer follows the accepted side unless an
                // outstanding request still targets another roster, and resets
                // the channel to All after any membership change.
                if(pilot.requested<0 || pilot.requested==pilot.team)teamPage_=pilot.team;
                chatChannel_=int(ChatChannel::Everyone);lobbyChat_={};lobbyRecipient_=-1;rosterList_={};rosterSelected_=-100000;
            }
            if(pilot.team>=0)session_.team=pilot.team;
        }
        bool teamsChanged=teamScores_!=state.teamScores;
        if(teamSettingsOpen_ && (session_.rules.lockTeamSettings || !state.commander || state.playerSide!=teamSettingsSide_ || state.missionStage==STAGE_STARTING)){
            teamSettingsOpen_=false;popups_.clear();pointerAction_.reset();teamsChanged=true;
        }
        if(teamSettingsOpen_ && teamSettingsSide_<int(state.teamScores.size()) && teamSettingsSide_<int(teamScores_.size()) &&
           state.teamScores[teamSettingsSide_].name!=teamScores_[teamSettingsSide_].name)teamNameDraft_=state.teamScores[teamSettingsSide_].name;
        teamScores_=state.teamScores;
        session_.inactiveTeams=0;
        session_.requireTeamPlayers=(state.lobbyFlags&Allegiance::LobbyRequirePlayers)!=0;
        for(size_t side=0;side<teamScores_.size() && side<session_.teamNames.size();++side){
            if(!teamScores_[side].active)session_.inactiveTeams|=1u<<side;
            session_.teamNames[side]=teamScores_[side].name;
            auto civ=std::find_if(civilizations_.begin(),civilizations_.end(),[&](const auto& c){return c.id==teamScores_[side].civilization;});
            if(civ!=civilizations_.end())session_.teamCivilizations[side]=int(civ-civilizations_.begin());
        }
        int timer=std::max(0,int(session_.rules.minutes?state.timeRemaining:state.time));
        bool changed=stageChanged || policyChanged || teamsChanged || hostChanged || lobbyTimerSeconds_!=timer || commander_!=state.commander || lobbyFlags_!=state.lobbyFlags || autoAccept_!=state.autoAccept || forceReady_!=state.forceReady || readySides_!=state.readySides || networkReady_!=state.networkReady || networkCountdown_!=state.networkCountdown || networkPilots_.size()!=state.networkPilots.size() ||
            networkPilots_!=state.networkPilots;
        lobbyTimerSeconds_=timer;
        lobbyFlags_=state.lobbyFlags;autoAccept_=state.autoAccept;forceReady_=state.forceReady;readySides_=state.readySides;networkReady_=state.networkReady;networkCountdown_=state.networkCountdown;
        if(changed){networkPilots_=state.networkPilots;commander_=state.commander;rebuild();}
    }
    // QSR_FlushSides / QSR_BalanceSides open the original message box once,
    // even when a notice is repeated in subsequent snapshots. Suppress an
    // additional randomize warning while a modal is already being displayed.
    if(!state.messages.empty() && state.messages.back().serial>lastModalSerial_){
        for(const auto& message:state.messages)if(message.modal && message.serial>lastModalSerial_ && messageBox_.empty())
            error(message.displayText.empty()?message.text:message.displayText);
        lastModalSerial_=state.messages.back().serial;
    }
    bool requestChanged=screen_==Screen::PilotOrders && (terminalChanged ||
        !std::equal(commands_.begin(),commands_.end(),state.commands.begin(),[](const auto& a,const auto& b){return a.command==b.command && a.targetType==b.targetType && a.targetId==b.targetId && a.text==b.text;}) ||
        moneyRequest_.sender!=state.moneyRequest.sender || moneyRequest_.amount!=state.moneyRequest.amount || moneyRequest_.hull!=state.moneyRequest.hull || moneyRequest_.prompt!=state.moneyRequest.prompt);
    if(requestChanged){commands_=state.commands;moneyRequest_=state.moneyRequest;}
    if(screen_==Screen::Scores){teamScores_=state.teamScores;rebuild();}
    if(screen_==Screen::Cargo || screen_==Screen::CargoMount){equipmentRows_=state.equipment;selectedCargo_=state.selectedCargo;docked_=state.docked;passenger_=state.passenger;rebuild();}
    if(state.economyRevision==economyRevision_ && state.money==money_){
        if(loadoutContextChanged && previewLoadout_)loadoutPreview_=previewLoadout_(loadoutPreview_.loadout,false,-1);
        if(requestChanged || terminalChanged || chatChanged || loadoutContextChanged || (crewChanged && screen_==Screen::Hangar) || (hostChanged && screen_==Screen::Pilots))rebuild();return;
    }
    economyRevision_=state.economyRevision;money_=state.money;pilots_=state.pilots;moneyRequest_=state.moneyRequest;commander_=state.commander;research_=state.research;units_=state.units;hulls_=state.hullChoices;
    if(!networked_){networkPilots_=state.networkPilots;teamScores_=state.teamScores;}
    if(original_ && screen_==Screen::Research)refreshResearchChoices();
    if(screen_==Screen::ShipLoadout && previewLoadout_){loadoutPreview_=previewLoadout_(loadoutPreview_.loadout,false,-1);rebuild();}
    if(terminalChanged || screen_==Screen::Research || screen_==Screen::Orders || screen_==Screen::Hangar || screen_==Screen::Pilots || screen_==Screen::Account || screen_==Screen::PilotOrders)rebuild();
}
void Frontend::error(std::string message,bool resume){
    if(!original_){view_.notice=std::move(message);return;}
    messagePurpose_=MessagePurpose::Ordinary;
    if(!hasMessageBox())messageFocus_=view_.focus;
    messageBoxAccept_.reset();
    messageBox_=std::move(message);messageBoxResume_=resume;messageBoxPending_=false;messageBoxAction_=0;view_.notice.clear();pointerAction_.reset();rebuild();
}
void Frontend::confirm(std::string message,FrontResult accept,bool resume){
    if(!original_){error(std::move(message),resume);return;}
    messagePurpose_=MessagePurpose::Ordinary;
    if(!hasMessageBox())messageFocus_=view_.focus;
    messageBox_=std::move(message);messageBoxResume_=resume;messageBoxPending_=false;messageBoxAction_=0;messageBoxAccept_=accept;
    view_.notice.clear();pointerAction_.reset();rebuild();
}
void Frontend::connecting(){
    if(!original_)return;
    messagePurpose_=MessagePurpose::Ordinary;
    if(!hasMessageBox())messageFocus_=view_.focus;
    messageBoxAccept_.reset();
    messageBox_="Connecting...";messageBoxResume_=false;messageBoxPending_=true;messageBoxAction_=0;pointerAction_.reset();rebuild();
}
void Frontend::purchasing(bool launch,uint64_t action){
    if(!original_)return;
    if((screen_==Screen::Settings || screen_==Screen::Pause) && !popups_.empty())restorePopupParent();
    messagePurpose_=MessagePurpose::Ordinary;
    if(!hasMessageBox())messageFocus_=view_.focus;
    messageBoxAccept_.reset();
    messageBox_=launch?"Preparing ship for launch....":"Loading out ship....";
    messageBoxResume_=false;messageBoxPending_=true;messageBoxAction_=action;pointerAction_.reset();popups_.clear();rebuild();
}
bool Frontend::actionRejected(uint64_t action,std::string message,bool resume){
    if(action && !pendingAction(action))return false;
    if(message.empty()){if(action)transferFinished(action);return false;}
    error(std::move(message),resume);return true;
}
bool Frontend::purchaseFinished(uint64_t action){
    // A receipt can outlive its screen or be followed by a new docking refit.
    // Only the action owning the current lockdown may release or replace it.
    if(action && !pendingAction(action))return false;
    connectionFinished();return true;
}
void Frontend::transferring(Allegiance::CrewTransfer transfer,std::string_view name,uint64_t action){
    if(!original_ || !action)return;
    if((screen_==Screen::Settings || screen_==Screen::Pause) && !popups_.empty())restorePopupParent();
    messagePurpose_=MessagePurpose::Ordinary;
    if(!hasMessageBox())messageFocus_=view_.focus;
    messageBoxAccept_.reset();messageBox_=Allegiance::CrewTransferMessage(transfer,name);
    messageBoxResume_=false;messageBoxPending_=true;messageBoxAction_=action;pointerAction_.reset();popups_.clear();rebuild();
}
void Frontend::pendingLobbyMessage(const char* message,uint64_t action){
    if(!original_ || !action)return;
    messagePurpose_=MessagePurpose::Ordinary;
    if(!hasMessageBox())messageFocus_=view_.focus;
    messageBoxAccept_.reset();messageBox_=message;
    messageBoxResume_=false;messageBoxPending_=true;messageBoxAction_=action;pointerAction_.reset();popups_.clear();rebuild();
}
void Frontend::quittingTeam(uint64_t action){pendingLobbyMessage("Quitting current team...",action);}
void Frontend::cancelingRequest(uint64_t action){
    pendingLobbyMessage("Canceling request...",action);
}
void Frontend::trainingSlides(bool active){
    slideshowActive_=active;
    if(active){setPlaying(false);open(Screen::Slideshow);}
}
void Frontend::trainingStarting(){
    if(!original_)return;
    messagePurpose_=MessagePurpose::Ordinary;
    if(!hasMessageBox())messageFocus_=view_.focus;
    messageBoxAccept_.reset();messageBox_="Training simulation initiated...";
    messageBoxResume_=false;messageBoxPending_=true;messageBoxAction_=0;pointerAction_.reset();rebuild();
}
void Frontend::connectionFinished(){if(messageBoxPending_){messageBox_.clear();messageBoxPending_=false;messageBoxAction_=0;messagePurpose_=MessagePurpose::Ordinary;pointerAction_.reset();view_.focus=messageFocus_;rebuild();}}
void Frontend::trainingDialog(bool suggestion){
    if(!original_){open(Screen::Training);return;}
    if(!hasMessageBox())messageFocus_=view_.focus;
    messageBox_=suggestion?Allegiance::TrainingSuggestionMessage:Allegiance::TrainingWelcomeMessage;
    messageBoxResume_=messageBoxPending_=false;messageBoxAction_=0;messageDontAskAgain_=false;
    messageBoxAccept_=suggestion?std::optional<FrontResult>{FrontResult{}}:std::nullopt;
    messagePurpose_=suggestion?MessagePurpose::TrainingSuggestion:MessagePurpose::TrainingWelcome;
    pointerAction_.reset();view_.notice.clear();rebuild();
}
void Frontend::startupTrainingSuggestion(){
    if(!original_ || screen_!=Screen::Home || trainingSuggestionVisited_)return;
    trainingSuggestionVisited_=true;
    if(settings_.showStartupTrainingSuggestion)trainingDialog(true);
}
void Frontend::configuring(){
    if(!original_)return;
    if((screen_==Screen::Settings || screen_==Screen::Pause) && !popups_.empty())restorePopupParent();
    messagePurpose_=MessagePurpose::Ordinary;
    if(!hasMessageBox())messageFocus_=view_.focus;
    messageBoxAccept_.reset();messageBox_="Changing mission parameters...";
    messageBoxResume_=false;messageBoxPending_=true;messageBoxAction_=0;pointerAction_.reset();popups_.clear();rebuild();
}
void Frontend::closeConsoleToFlight(){
    if(!original_ || screen_!=Screen::Research || !view_.slide)return;
    consoleClosing_.assign(view_.artwork.begin()+view_.slide->artworkBegin,view_.artwork.begin()+std::min(view_.artwork.size(),view_.slide->artworkEnd));
    consoleClosingSlide_=view_.slide;consoleClosingSlide_->started=uiTime();consoleClosingSlide_->closing=true;
    consoleFlight_=true;pointerAction_.reset();popups_.clear();researchHover_=-1;rebuild();
}
void Frontend::chatSector(int sector,const GameState& state){open(Screen::Chat,&state);chatChannel_=int(ChatChannel::FriendlySector);chatSector_=sector;editingChat_=true;rebuild();}
void Frontend::parts(PartKind kind,int mount,std::vector<PartChoice> choices){partKind_=kind;partMount_=mount;choices_=std::move(choices);partPage_=0;open(Screen::Parts);}
void Frontend::orderTargets(int ship,int command,std::vector<OrderTarget> targets){unitId_=ship;orderCommand_=command;orderTargets_=std::move(targets);targetPage_=0;open(Screen::OrderTargets);}
void Frontend::rebuild() {
    menuPopupView_={};menuPopupScene_={};popupLayer_.reset();popupItemsBegin_=0;
    missionBriefOpen_=false;
    rebuildLayout();
    if(original_ && teamSettingsActive())originalTeamSettings();
    if(original_ && logonOpen_)originalLogon();
    if(original_ && (screen_==Screen::Countdown || (screen_==Screen::RemoteLobby || screen_==Screen::Pilots) && missionStage_==STAGE_STARTING && networkCountdown_>0))originalMissionBriefing();
    if(original_ && !popups_.empty() && popups_.front().group==PopupGroup::QuickChat)originalPopup(true);
    if(original_ && popupLayer_)cachePopupLayer();
    if(original_ && hasMessageBox())originalMessageBox();
    // Restore capture after the final host layer adds its controls. Modals and
    // Details are not present when rebuildLayout constructs the parent screen.
    if(pointerAction_ && screen_==pointerScreen_)for(size_t i=0;i<view_.items.size();++i){
        const auto& item=view_.items[i];
        if(!item.blocked && item.control==pointerControl_ && item.action.op==pointerAction_->op && item.action.value==pointerAction_->value && item.action.delta==pointerAction_->delta){view_.pressed=int(i);break;}
    }
    if(original_){
        if(!hasMessageBox())buttonChecks_.erase("trainingDontAskAgain");
        for(auto* view:{&view_,&menuPopupView_,&messageView_,&detailsView_}){view->bindControlSounds();restoreButtonChecks(*view);}
    }
    if(!missionBriefOpen_)missionBriefStrings_.clear();
}
void Frontend::rebuildLayout() {
    view_.popupArtworkBegin.reset();
    view_.classicScale=classicInterfaceScale(settings_.classicUiScale,displayScale_);
    view_.items.clear();view_.text.clear();view_.artwork.clear();view_.shipPreview.reset();view_.scroll.reset();view_.slide.reset();view_.original=false;view_.overlay=false;view_.pressed=-1;view_.ingame=playing_;view_.canvas={800,600};
    view_.title="ALLEGIANCE";view_.subtitle="";
    if(original_ && consoleFlight_){
        view_.original=true;view_.overlay=true;
        if(consoleClosingSlide_){view_.slide=consoleClosingSlide_;view_.slide->artworkBegin=view_.slide->itemsBegin=0;view_.artwork=consoleClosing_;view_.slide->artworkEnd=view_.artwork.size();view_.slide->itemsEnd=0;}
        return;
    }
    if(original_ && !popups_.empty() && (screen_==Screen::Settings || screen_==Screen::Pause)){originalPopup();return;}
    // The slideshow image is painted by its host. Popups use the same native
    // primitives as every other original screen, over that live image.
    if(screen_==Screen::Slideshow){view_.original=bool(original_);view_.overlay=true;return;}
    if(complete_) {
        button({24,100,196,38},"Session results",{MenuOp::Navigate,int(Screen::Results)},screen_==Screen::Results);
        button({24,148,196,38},"Team scores",{MenuOp::Navigate,int(Screen::Scores)},screen_==Screen::Scores);
    } else {
    button({24,100,196,38},playing_?(inactive_?"Team eliminated":trainingCommand_?"Resume command view":docked_?"Launch":"Resume flight"):"Home",{playing_?(docked_ && !trainingCommand_ && !inactive_?MenuOp::Launch:MenuOp::Resume):MenuOp::Navigate,int(Screen::Home)},screen_==Screen::Home,!networkLobby_ && (!playing_ || inactive_ || !docked_ || !passenger_));
    if(playing_)button({24,148,196,38},inactive_?"Team scores":docked_?"Hangar / loadout":"Return to base",{inactive_ || docked_?MenuOp::Navigate:MenuOp::ReturnToBase,int(inactive_?Screen::Scores:Screen::Hangar)},screen_==(inactive_?Screen::Scores:Screen::Hangar),!networkLobby_);
    else button({24,148,196,38},"Scenarios",{MenuOp::Navigate,int(Screen::Scenarios)},screen_==Screen::Scenarios || screen_==Screen::Lobby);
    }
    button({24,196,196,38},playing_?"Team pilots":"Multiplayer",{MenuOp::Navigate,int(playing_?(networkLobby_?Screen::RemoteLobby:Screen::Pilots):Screen::Online)},screen_==(playing_?(networkLobby_?Screen::RemoteLobby:Screen::Pilots):Screen::Online));
    button({24,244,196,38},"Configuration",{MenuOp::Navigate,int(Screen::Settings)},screen_==Screen::Settings || screen_==Screen::Controls);
    button({24,292,196,38},"Introduction",{MenuOp::Intro});
    button({24,340,196,38},"Credits",{MenuOp::Navigate,int(Screen::Credits)},screen_==Screen::Credits);
    if(playing_ && !gameplayClosed() && (economy_ || trainingCommand_)) {
        if(economy_)button({24,388,196,38},"Research / production",{MenuOp::Navigate,int(Screen::Research)},screen_==Screen::Research);
        button({24,436,196,38},"Team orders",{MenuOp::Navigate,int(Screen::Orders)},screen_==Screen::Orders);
        line(24,529,"Credits: "+std::to_string(money_),196);
    }
    if(playing_ && !gameplayClosed())button({24,484,196,32},"Crew / turrets",{MenuOp::Crew},screen_==Screen::Crew);
    button({24,552,196,38},playing_?"Leave session":"Quit",{playing_?(complete_?MenuOp::Leave:MenuOp::Navigate):MenuOp::Quit,int(Screen::ConfirmLeave)});
    switch(screen_) {
    case Screen::Chat: {
        bool shell=!chat_.empty() && chat_[0]=='!';view_.subtitle=shell?"Local command":"Chat";
        if(!shell)row(135,"Send to",chatChannelNames[chatChannel_],MenuOp::ChatChannel);
        line(270,210,chat_.empty()?"Type a message...":chat_,684);
        line(270,410,shell?"!mute pilot / !unmute pilot. Enter runs locally. Escape returns to the game.":"Tab changes recipient. Enter sends. Escape returns to the game.",684);
        button({270,485,330,40},shell?"Run":"Send",{MenuOp::SendChat},false,!chat_.empty());
        button({624,485,330,40},"Cancel",{MenuOp::Resume});break;
    }
    case Screen::Home:
        view_.subtitle="Welcome, "+settings_.pilot;
        line(270,105,"Choose your next flight",650,true);
        line(270,147,"Fly the original ships and sectors in a local session. Start a guided exercise, explore freely, or set up a battle with AI wingmen.");
        button({270,242,315,70},"Choose a scenario",{MenuOp::Navigate,int(Screen::Scenarios)});
        button({615,242,339,70},"Create a local skirmish",{MenuOp::Scenario,int(Scenario::Skirmish)});
        button({270,320,684,32},"Original narrated training",{MenuOp::Navigate,int(Screen::Training)});
        line(270,359,"Pilot profile",650,true);
        button({270,400,684,40},editingPilot_?"Callsign (Enter to finish)":"Callsign",{MenuOp::Pilot},editingPilot_,true,editingPilot_?draft_.pilot+"_":settings_.pilot);
        line(270,473,"F1 shows flight controls. Mouse and keyboard navigation are available throughout the menus.");
        break;
    case Screen::Training:
        view_.subtitle="Original narrated training";
        for(size_t i=0;i<originalTraining.size();++i){auto& t=originalTraining[i];float y=96+i*66;
            button({270,y,684,32},t.name,{MenuOp::Training,t.id});line(280,y+34,t.description,650);
        }break;
    case Screen::Scenarios:
        view_.subtitle="Scenarios";
        for(size_t i=0;i<scenarios.size();++i){float y=94+i*78;
            button({270,y,684,34},scenarios[i].name,{MenuOp::Scenario,int(i)});
            line(280,y+38,scenarios[i].description,650);
        }break;
    case Screen::Lobby:
        view_.subtitle=std::string(networked_?"Multiplayer setup / ":"Local lobby / ")+scenarios[size_t(session_.scenario)].name;
        row(100,"Map",session_.scenario==Scenario::Conquest?conquestMapNames[session_.map]:mapNames[session_.map],MenuOp::Map);
        if(session_.scenario==Scenario::Conquest) {
            int civ=session_.teamCivilizations[session_.team];if(civ<0)civ=session_.civilization;
            row(143,"Civilization",civilizations_.empty()?"Default civilization":civilizations_[civ].name,MenuOp::Civilization);
        }
        else row(143,"Starting ship",hullNames[session_.hull],MenuOp::Hull);
        if(session_.scenario==Scenario::Practice || session_.scenario==Scenario::Skirmish) {
            row(186,"Enemy wingmen",std::to_string(session_.enemies),MenuOp::Enemies);
            row(229,"Allied wingmen",std::to_string(session_.allies),MenuOp::Allies);
            row(272,"AI skill",std::array<const char*,3>{"Cadet","Regular","Veteran"}[session_.difficulty],MenuOp::Difficulty);
            row(315,"Team kill limit",session_.killLimit?std::to_string(session_.killLimit):"Unlimited",MenuOp::KillLimit);
        } else {
            line(270,197,scenarios[size_t(session_.scenario)].description,650);
            if(session_.scenario==Scenario::Conquest) {
                button({270,238,684,35},"Teams, civilizations and alliances",{MenuOp::Navigate,int(Screen::Teams)});
                button({270,280,684,42},"Mission goals and rules",{MenuOp::Navigate,int(Screen::MissionRules)});
            }
        }
        if(session_.scenario==Scenario::Conquest)row(334,"Your team",std::to_string(session_.team+1)+" / "+std::to_string(session_.rules.teams),MenuOp::JoinTeam);
        line(270,374,"YOUR TEAM",300,true);line(627,374,"OPPOSING TEAMS",300,true);
        line(270,406,settings_.pilot+"  /  "+(ready_?"Ready":"Not ready"),330);
        line(270,434,std::to_string(session_.scenario==Scenario::Conquest?session_.rules.miners:session_.allies)+(session_.scenario==Scenario::Conquest?" initial miners":" AI wingmen"),330);line(627,406,std::to_string(session_.scenario==Scenario::Conquest?session_.rules.miners:session_.enemies)+(session_.scenario==Scenario::Conquest?" initial miners":" AI wingmen"),300);
        if(networked_){button({270,501,325,42},"Apply mission settings",{MenuOp::NetworkConfigure},false,networkHost_ && Game::missionError(session_,customMaps_).empty());button({627,501,327,42},"Back to lobby",{MenuOp::Navigate,int(Screen::RemoteLobby)});}
        else {
        button({270,501,325,42},ready_?"Ready":"Mark ready",{MenuOp::Ready},ready_);
        button({627,501,327,42},"Start session",{MenuOp::Start},false,lobbyReady() && Game::missionError(session_,customMaps_).empty());
        if(session_.scenario==Scenario::Conquest)button({270,451,684,32},"Pilot seats / "+std::to_string(session_.localPilots.size()+1)+" local pilots",{MenuOp::Navigate,int(Screen::LobbyPilots)});
        line(270,560,"Local session. Use Multiplayer to join a server.",650);
        }
        break;
    case Screen::Countdown: {
        view_.subtitle="Local session / mission briefing";
        line(270,112,scenarios[size_t(session_.scenario)].name,684,true);
        line(270,158,scenarios[size_t(session_.scenario)].description,684);
        line(270,260,"Starting in "+std::to_string(int(std::ceil(std::max(0.,countdown_))))+" seconds",684,true);
        line(270,318,"Pilot: "+settings_.pilot+" / Team "+std::to_string(session_.team+1),684);
        button({270,470,684,42},"Cancel start",{MenuOp::CancelCountdown});break;
    }
    case Screen::LobbyPilots: {
        view_.subtitle="Local lobby / pilot seats";
        line(270,100,settings_.pilot+" / Team "+std::to_string(session_.team+1)+" / "+(ready_?"Ready":"Not ready"),684,true);
        lobbyPilotPage_=std::clamp(lobbyPilotPage_,0,std::max(0,(int(session_.localPilots.size())-1)/5));
        for(int row=0;row<5;++row) {
            int index=lobbyPilotPage_*5+row;if(index>=int(session_.localPilots.size()))break;auto& pilot=session_.localPilots[index];float y=145+row*66;
            button({270,y,274,29},editingLobbyPilot_==index?lobbyPilotDraft_+"_":censorChat(pilot.name,settings_.chat.censor),{MenuOp::LobbyPilotName,index});
            button({552,y,154,29},"Team "+std::to_string(pilot.team+1),{MenuOp::LobbyPilotTeam,index});
            button({714,y,134,29},pilot.ready?"Ready":"Not ready",{MenuOp::LobbyPilotReady,index},pilot.ready);
            button({856,y,98,29},"Remove",{MenuOp::RemoveLobbyPilot,index});
        }
        button({270,483,215,34},"Add local pilot",{MenuOp::AddLobbyPilot},false,initialShipCount(session_)<shipIdentityCapacity);
        button({503,483,215,34},"Previous",{MenuOp::LobbyPilotPage,0,-1},false,lobbyPilotPage_>0);
        button({739,483,215,34},"Next",{MenuOp::LobbyPilotPage,0,1},false,(lobbyPilotPage_+1)*5<int(session_.localPilots.size()));
        line(270,529,"Each seat is a real pilot account. Switch control through the in-game pilot roster.",684);
        button({270,568,684,32},"Back to lobby",{MenuOp::Navigate,int(networked_?Screen::RemoteLobby:Screen::Lobby)});break;
    }
    case Screen::Teams: {
        view_.subtitle="Local lobby / team configuration";
        if(teamPage_!=noTeam)teamPage_=std::clamp(teamPage_,0,session_.rules.teams-1);
        row(100,"Edit team",std::to_string(teamPage_+1),MenuOp::SelectTeam);
        int count=std::max(1,int(civilizations_.size()));
        int civ=session_.teamCivilizations[teamPage_];if(civ<0)civ=(session_.civilization+teamPage_-session_.team+count)%count;
        row(143,"Civilization",civilizations_.empty()?"Default":civilizations_[civ].name,MenuOp::TeamCivilization,teamPage_);
        constexpr const char* groups[]={"Independent","Alliance A","Alliance B","Alliance C"};
        row(186,"Alliance",groups[session_.alliances[teamPage_]+1],MenuOp::TeamAlliance,teamPage_);
        auto effective=Game::effectiveAlliances(session_);
        for(int team=0;team<session_.rules.teams;++team) {
            int index=session_.teamCivilizations[team];if(index<0)index=(session_.civilization+team-session_.team+count)%count;
            std::string label="Team "+std::to_string(team+1)+(team==session_.team?" / "+settings_.pilot:"");
            line(270,250+team*43,label+" / "+(civilizations_.empty()?"Default":civilizations_[index].name)+" / "+groups[effective[team]+1],684,team==session_.team);
        }
        line(270,515,"An alliance needs at least two teams and a separate opponent. Invalid groups remain independent.",684);
        button({270,565,684,35},"Back to lobby",{MenuOp::Navigate,int(networked_?Screen::RemoteLobby:Screen::Lobby)});break;
    }
    case Screen::MissionRules: {
        view_.subtitle=networked_?"Multiplayer / mission rules":"Local lobby / mission rules";auto& r=session_.rules;
        if(rulesPage_==0) {
            row(100,"Conquest goal",r.conquest?std::to_string(r.conquest)+"%":"Disabled",MenuOp::MissionSetting,0);
            row(143,"Territory goal",r.territory<100?std::to_string(r.territory)+"%":"Disabled",MenuOp::MissionSetting,1);
            row(186,"Enemy flags",r.flags?std::to_string(r.flags):"Disabled",MenuOp::MissionSetting,2);
            row(229,"Neutral artifacts",r.artifacts?std::to_string(r.artifacts):"Disabled",MenuOp::MissionSetting,3);
            row(272,"Team kills",r.teamKills?std::to_string(r.teamKills):"Disabled",MenuOp::MissionSetting,4);
            row(315,"Prosperity cost",r.prosperity?number(r.prosperity,1)+"x":"Disabled",MenuOp::MissionSetting,5);
            row(358,"Time limit",r.minutes?std::to_string(r.minutes)+" minutes":"Unlimited",MenuOp::MissionSetting,6);
            line(270,416,"Any enabled goal can win. At timeout, the original objective and kill/death tie-break rules apply.",684);
        } else if(rulesPage_==1) {
            row(100,"Starting credits",number(r.startingMoney,1)+"x",MenuOp::MissionSetting,7);
            row(143,"He3 density",number(r.helium,1)+"x",MenuOp::MissionSetting,8);
            row(186,"Initial miners",std::to_string(r.miners),MenuOp::MissionSetting,9);
            row(229,"Friendly fire",r.friendlyFire?"Enabled":"Disabled",MenuOp::MissionSetting,10);
            row(272,"Deaths allowed",r.lives==32767?"Unlimited":std::to_string(r.lives),MenuOp::MissionSetting,11);
            row(315,"Maximum drones",std::to_string(r.maxDrones),MenuOp::MissionSetting,12);
            row(358,"Developments",r.developments?"Enabled":"Disabled",MenuOp::MissionSetting,13);
        } else if(rulesPage_==2) {
            row(100,"Teams",std::to_string(r.teams),MenuOp::MissionSetting,14);
            row(143,"Starting garrisons",r.mapSize?"Two":"One",MenuOp::MissionSetting,15);
            row(186,"Random encounters",std::to_string(r.randomEncounters),MenuOp::MissionSetting,16);
            row(229,"Show full map",r.showMap?"Yes":"No",MenuOp::MissionSetting,17);
            row(272,"Reveal home sector",r.showHome?"Yes":"No",MenuOp::MissionSetting,18);
            row(315,"Invulnerable stations",r.invulnerableStations?"Yes":"No",MenuOp::MissionSetting,19);
            row(358,"Eject lifepods",r.ejectPods?"Yes":"No",MenuOp::MissionSetting,20);
        } else if(rulesPage_==3) {
            row(100,"Home generic asteroids",std::to_string(r.homeAsteroids),MenuOp::MissionSetting,21);
            row(143,"Neutral generic asteroids",std::to_string(r.neutralAsteroids),MenuOp::MissionSetting,22);
            row(186,"Home He3 asteroids",std::to_string(r.homeMineable),MenuOp::MissionSetting,23);
            row(229,"Neutral He3 asteroids",std::to_string(r.neutralMineable),MenuOp::MissionSetting,24);
            row(272,"Home special asteroids",std::to_string(r.homeSpecial),MenuOp::MissionSetting,25);
            row(315,"Neutral special asteroids",std::to_string(r.neutralSpecial),MenuOp::MissionSetting,26);
            row(358,"Visible resource amounts",r.resourceAmounts?"Yes":"No",MenuOp::MissionSetting,27);
        } else if(rulesPage_==4) {
            row(100,"Home starting treasures",std::to_string(r.homeTreasures),MenuOp::MissionSetting,28);
            row(143,"Neutral starting treasures",std::to_string(r.neutralTreasures),MenuOp::MissionSetting,29);
            row(186,"Home treasures / minute",number(r.homeTreasureRate,1),MenuOp::MissionSetting,30);
            row(229,"Neutral treasures / minute",number(r.neutralTreasureRate,1),MenuOp::MissionSetting,31);
            row(315,"Allied sight",r.alliedSight?"Shared":"Separate",MenuOp::MissionSetting,33);
            row(358,"Allied ripcord",r.alliedRipcord?"Enabled":"Disabled",MenuOp::MissionSetting,34);
        } else {
            row(100,"Shipyard path",r.shipyard?"Enabled":"Disabled",MenuOp::MissionSetting,35);
            row(143,"Tactical path",r.tactical?"Enabled":"Disabled",MenuOp::MissionSetting,36);
            row(186,"Supremacy path",r.supremacy?"Enabled":"Disabled",MenuOp::MissionSetting,37);
            row(229,"Expansion path",r.expansion?"Enabled":"Disabled",MenuOp::MissionSetting,38);
            row(272,"Random wormholes",r.randomWormholes?"Enabled":"Disabled",MenuOp::MissionSetting,39);
            row(315,"Start countdown",std::to_string(r.startCountdown)+" seconds",MenuOp::MissionSetting,40);
        }
        if(auto error=Game::missionError(session_,customMaps_);!error.empty())line(270,417,error,684);
        button({270,500,325,38},"Previous settings",{MenuOp::RulesPage,rulesPage_-1},false,rulesPage_>0);
        button({629,500,325,38},"Next settings",{MenuOp::RulesPage,rulesPage_+1},false,rulesPage_<5);
        button({270,550,684,38},"Back to lobby",{MenuOp::Navigate,int(networked_?Screen::RemoteLobby:Screen::Lobby)});break;
    }
    case Screen::Scores:
        view_.subtitle="Team scores";line(270,100,"Mission standings",650,true);
        for(size_t i=0;i<teamScores_.size();++i) {
            auto& score=teamScores_[i];float y=150+i*57;
            line(270,y,score.name+(score.active?"":" / eliminated"),684,true);
            line(270,y+25,"Kills "+std::to_string(score.kills)+"   Deaths "+std::to_string(score.deaths)+"   Flags "+std::to_string(score.flags)+"   Artifacts "+std::to_string(score.artifacts)+"   Sectors "+std::to_string(score.sectors)+"   Prosperity "+std::to_string(score.prosperity)+"%",684);
        }
        button({270,550,684,38},complete_?"Back to results":"Back to session",{MenuOp::Navigate,int(complete_?Screen::Results:Screen::Pause)});break;
    case Screen::Settings:
        view_.subtitle="Configuration";
        row(100,"Display mode",draft_.fullscreen?"Borderless fullscreen":"Borderless window",MenuOp::Setting,0);
        row(143,"HUD size",number(draft_.uiScale*100)+"%",MenuOp::Setting,1);
        row(186,"Field of view",number(draft_.fieldOfView)+" degrees",MenuOp::Setting,2);
        row(229,"Master volume",number(draft_.volume*100)+"%",MenuOp::Setting,3);
        row(272,"Mouse sensitivity",number(draft_.sensitivity,2),MenuOp::Setting,4);
        row(315,"Invert mouse Y",draft_.invertY?"On":"Off",MenuOp::Setting,5);
        row(358,"Particle detail",particlePreference(draft_.particles).label,MenuOp::Setting,6);
        row(401,"Frame timing display",draft_.showTiming?"On":"Off",MenuOp::Setting,7);
        button({270,457,220,36},"Keyboard bindings",{MenuOp::Navigate,int(Screen::Controls)});
        button({502,457,220,36},"Joystick / gamepad",{MenuOp::Navigate,int(Screen::Controller)});
        button({734,457,220,36},"Audio / feedback",{MenuOp::Navigate,int(Screen::Audio)});
        button({270,500,220,32},"Startup movies",{MenuOp::Navigate,int(Screen::Movies)});
        button({502,500,220,32},"Interface preferences",{MenuOp::Navigate,int(Screen::Interface)});
        button({734,500,220,32},"Restore defaults",{MenuOp::Defaults});
        button({270,543,320,42},"Cancel",{MenuOp::Navigate,int(settingsReturn_)});
        button({620,543,334,42},"Apply and save",{MenuOp::Apply});break;
    case Screen::Interface: {
        view_.subtitle="Interface preferences";constexpr const char* modes[]={"All","Default","Target","Minimal"};
        row(110,"Cockpit and chase",modes[draft_.cockpitRadar],MenuOp::Setting,14);
        row(175,"Command view",modes[draft_.commandRadar],MenuOp::Setting,15);
        row(240,"Sector grid in flight",draft_.showGrid?"Enabled":"Disabled",MenuOp::Setting,16);
        row(305,"Grid and height lines in command",draft_.showCommandGrid?"Enabled":"Disabled",MenuOp::Setting,20);
        line(270,390,"Each view remembers its radar detail and grid setting. Control + period toggles the grid in the active view. Original lessons control their own radar and panes.",684);
        button({270,482,330,38},"Appearance preferences",{MenuOp::Navigate,int(Screen::Visuals)});
        button({620,482,334,38},"Mouse steering",{MenuOp::Navigate,int(Screen::Mouse)});
        button({270,527,684,32},"Chat preferences",{MenuOp::Navigate,int(Screen::ChatPreferences)});
        button({270,568,684,32},"Back to configuration",{MenuOp::Navigate,int(Screen::Settings)});break;
    }
    case Screen::ChatPreferences:
        view_.subtitle="Chat preferences";
        row(110,"All pilots channel",draft_.chat.filterAll?"Filtered":"Shown",MenuOp::ChatPreference,0);
        row(175,"Human quick comms voice",draft_.chat.filterQuick?"Muted":"Played",MenuOp::ChatPreference,1);
        row(240,"Unknown human quick comms",draft_.chat.filterUnknown?"Filtered":"Shown",MenuOp::ChatPreference,2);
        row(305,"Display text censor",draft_.chat.censor?"Enabled":"Disabled",MenuOp::ChatPreference,3);
        line(270,390,"Muting quick comms keeps their text and orders. Muting a pilot in the roster discards their text, voice and orders. Headquarters messages are retained.",684);
        button({270,550,684,38},"Back to interface preferences",{MenuOp::Navigate,int(Screen::Interface)});break;
    case Screen::Mouse:
        view_.subtitle="Mouse steering";
        row(110,"Steering mode",draft_.virtualJoystick?"Virtual joystick":"Direct movement",MenuOp::Setting,17);
        row(170,"Steering indicator",draft_.joystickIndicator?"Enabled":"Disabled",MenuOp::Setting,18);
        row(230,"Mouse acceleration",std::array<const char*,3>{"Off","Low","High"}[draft_.mouseAcceleration],MenuOp::Setting,19);
        row(290,"Mouse sensitivity",number(draft_.sensitivity,2),MenuOp::Setting,4);
        row(350,"Invert mouse Y",draft_.invertY?"Enabled":"Disabled",MenuOp::Setting,5);
        line(270,430,"Virtual joystick retains the turn rate until you move the mouse back. Direct movement turns while the mouse moves. Leaving flight resets steering.",684);
        button({270,497,684,34},"Button and wheel assignments",{MenuOp::Navigate,int(Screen::MouseButtons)});
        button({270,550,684,38},"Back to interface preferences",{MenuOp::Navigate,int(Screen::Interface)});break;
    case Screen::MouseButtons:
        view_.subtitle="Mouse buttons and wheel";
        for(int i=0;i<10;++i){
            int binding=draft_.mouseButtons[i];std::string value=binding<0?"Unbound":bindingNames[binding];
            if(std::any_of(draft_.mouseAliases.begin(),draft_.mouseAliases.end(),[&](auto& a){return a.button==i;}))value+=" (+ aliases)";
            button({i<5?270.f:625.f,110.f+(i%5)*70,329,52},mouseButtonNames[i],{MenuOp::MouseButton,i},false,true,value);
        }
        line(270,480,"Assignments apply in flight. Menu clicks and command-view selection keep their normal controls.",684);
        button({270,550,684,38},"Back to mouse steering",{MenuOp::Navigate,int(Screen::Mouse)});break;
    case Screen::MouseAction:
        view_.subtitle=std::string("Assign ")+mouseButtonNames[mouseButton_];
        button({270,96,684,32},"Unbound",{MenuOp::MouseAssign,-1});
        for(int row=0;row<20;++row){
            int binding=mouseActionPage_*20+row;if(binding>=int(Binding::Count))break;
            button({row<10?270.f:625.f,144.f+(row%10)*36,329,32},bindingNames[binding],{MenuOp::MouseAssign,binding});
        }
        button({270,525,155,36},"Previous",{MenuOp::MousePage,0,-1},false,mouseActionPage_>0);
        button({440,525,155,36},"Next",{MenuOp::MousePage,0,1},false,(mouseActionPage_+1)*20<int(Binding::Count));
        button({625,525,329,36},"Back to buttons",{MenuOp::Navigate,int(Screen::MouseButtons)});break;
    case Screen::Visuals:
        view_.subtitle="Appearance preferences";
        for(int i=0;i<int(visualFields.size());++i)row(98+i*40,visualNames[i],draft_.visual.*visualFields[i]?"Enabled":"Disabled",MenuOp::VisualSetting,i);
        button({270,550,684,38},"Back to interface preferences",{MenuOp::Navigate,int(Screen::Interface)});break;
    case Screen::Audio:
        view_.subtitle="Audio and feedback";
        row(110,"Master volume",number(draft_.volume*100)+"%",MenuOp::Setting,3);
        row(170,"Sound effects",number(draft_.effectsGain)+" dB",MenuOp::Setting,11);
        row(230,"Voice and narration",number(draft_.voiceGain)+" dB",MenuOp::Setting,12);
        row(290,"Force feedback strength",number(draft_.controller.feedbackGain*100)+"%",MenuOp::Setting,13);
        line(270,375,"Effects and voice levels follow the original sound data. Master volume also controls movies. Feedback strength applies to forces and rumble.",684);
        button({270,550,684,38},"Back to configuration",{MenuOp::Navigate,int(Screen::Settings)});break;
    case Screen::Movies:
        view_.subtitle="Startup movies";
        row(110,"Startup credits",draft_.startupCredits?"Enabled":"Disabled",MenuOp::Setting,9);
        row(165,"Intro on next start",draft_.startupIntro?"Enabled":"Disabled",MenuOp::Setting,10);
        line(270,250,"Credits play before the introduction. The introduction option clears after the next ordinary startup, as in the original client.",684);
        line(270,328,"Space or Escape skips a movie. Playback pauses when the window loses focus.",684);
        button({270,550,684,38},"Back to configuration",{MenuOp::Navigate,int(Screen::Settings)});break;
    case Screen::Controls:
        view_.subtitle="Keyboard bindings";
        for(int row=0;row<20;++row){size_t i=size_t(bindingPage_*20+row);if(i>=bindingNames.size())break;
            float x=row<10?270:625,y=100+(row%10)*40;
            button({x,y,289,34},bindingNames[i],{MenuOp::Bind,int(i)},binding_==int(i),true,binding_==int(i)?"Press a key or chord":keyName(draft_,Binding(i)));
            button({x+293,y,36,34},"X",{MenuOp::Unbind,int(i)},false,draft_.keys[i]!=SDL_SCANCODE_UNKNOWN);
        }
        button({270,555,160,36},"Previous page",{MenuOp::BindingPage,0,-1},false,bindingPage_>0);
        button({440,555,160,36},"Next page",{MenuOp::BindingPage,1,1},false,(bindingPage_+1)*20<int(bindingNames.size()));
        row(506,"Throttle repeat",std::array<const char*,5>{"66 ms","33 ms","22 ms","16 ms","Every frame"}[draft_.throttleDelay],MenuOp::Setting,8);
        button({620,555,334,36},"Back to configuration",{MenuOp::Navigate,int(Screen::Settings)});break;
    case Screen::Controller: {
        view_.subtitle="Joystick and gamepad";
        line(270,102,controllerName_.empty()?"No controller connected":controllerName_,684,true);
        auto& c=draft_.controller;std::string device=c.device.empty()?"Automatic":"Disconnected device";
        for(auto& d:controllerDevices_)if(d.guid==c.device)device=d.name;
        row(150,"Controller input",c.enabled?"Enabled":"Disabled",MenuOp::ControllerSetting,0);
        row(198,"Device",c.mapped?"Mapped devices":device,MenuOp::ControllerSetting,1,!c.mapped);
        row(246,"Response",c.linear?"Linear":"Original quadratic",MenuOp::ControllerSetting,2);
        row(294,"Dead zone",number(c.deadZone*100)+"%",MenuOp::ControllerSetting,3);
        row(342,"Look hat",c.hat<0?"Disabled":"Hat "+std::to_string(c.hat+1),MenuOp::ControllerSetting,4,!c.mapped);
        row(390,"Gamepad triggers",c.triggers?"Guns / missile":"Disabled",MenuOp::ControllerSetting,5,!c.mapped);
        row(434,"Force feedback / rumble",c.feedback?"Enabled":"Disabled",MenuOp::ControllerSetting,6);
        button({270,478,325,38},"Axes and inversion",{MenuOp::Navigate,int(c.mapped?Screen::ControllerMap:Screen::ControllerAxes)});
        button({629,478,325,38},"Button assignments",{MenuOp::Navigate,int(c.mapped?Screen::ControllerMapButtons:Screen::ControllerButtons)});
        button({270,520,684,28},"Multiple controllers / original input maps",{MenuOp::Navigate,int(Screen::ControllerMap)});
        button({270,552,684,36},"Back to configuration",{MenuOp::Navigate,int(Screen::Settings)});break;
    }
    case Screen::ControllerMap: {
        auto& c=draft_.controller;view_.subtitle="Multiple controller routes";
        row(100,"Input mapping",c.mapped?"Multiple devices":"Single controller",MenuOp::MapMode);
        for(int i=0;i<6;++i){auto& route=c.mapAxes[i];std::string source=route.device<0 || route.index<0?"Unbound":"Device "+std::to_string(route.device+1)+" / "+(route.hat?"hat ":"axis ")+std::to_string(route.index+1);
            button({i<3?270.f:625.f,154.f+(i%3)*73,329,58},controllerAxisNames[i],{MenuOp::MapAxis,i},false,true,source);
        }
        button({270,391,325,38},"Device assignments",{MenuOp::Navigate,int(Screen::ControllerMapDevices)});
        button({629,391,325,38},"Button and hat routes",{MenuOp::Navigate,int(Screen::ControllerMapButtons)});
        line(270,460,"Review devices and axis directions after importing a Windows map. Device order and physical axis order can differ on Linux. Changes apply when you save.",684);
        button({270,550,684,38},"Back to controller",{MenuOp::Navigate,int(Screen::Controller)});break;
    }
    case Screen::ControllerMapAxis: {
        auto& route=draft_.controller.mapAxes[mapAxis_];view_.subtitle=std::string("Route / ")+controllerAxisNames[mapAxis_];
        row(120,"Device slot",route.device<0?"Unbound":std::to_string(route.device+1),MenuOp::MapAxisDevice);
        row(195,"Physical input",route.index<0?"Unbound":std::string(route.hat?"Hat ":"Axis ")+std::to_string(route.index+1),MenuOp::MapAxisInput);
        row(270,"Direction",route.invert?"Reversed":"Normal",MenuOp::MapAxisInvert);
        if(route.originalValue>=0)line(270,354,"Imported Windows value: "+std::to_string(route.originalValue)+". Select the physical Linux axis or hat that performs this action.",684);
        if(mapAxis_==5)line(270,437,"Command hat selects one direction at a time, matching the original. Assign its directions in button routes.",684);
        button({270,550,684,38},"Back to routes",{MenuOp::Navigate,int(Screen::ControllerMap)});break;
    }
    case Screen::ControllerMapDevices: {
        view_.subtitle="Controller device assignments";
        for(int i=0;i<8;++i){int slot=mapDevicePage_*8+i;auto& guid=draft_.controller.mapDevices[slot];std::string name=guid.empty()?"Automatic device "+std::to_string(slot+1):"Disconnected device";
            for(auto& device:controllerDevices_)if(device.guid==guid)name=device.name;
            row(104.f+i*47,"Device slot "+std::to_string(slot+1),name,MenuOp::MapDevice,slot);
        }
        button({270,506,155,32},"Previous",{MenuOp::MapDevicePage,0,-1},false,mapDevicePage_>0);
        button({445,506,155,32},"Next",{MenuOp::MapDevicePage,0,1},false,mapDevicePage_<1);
        button({625,550,329,38},"Back to routes",{MenuOp::Navigate,int(Screen::ControllerMap)});break;
    }
    case Screen::ControllerMapButtons: {
        auto& routes=draft_.controller.mapButtons;view_.subtitle="Button and command-hat routes";
        mapButtonPage_=std::clamp(mapButtonPage_,0,std::max(0,(int(routes.size())-1)/8));
        constexpr const char* hats[]={"Hat up","Hat right","Hat down","Hat left"};
        for(int i=0;i<8 && mapButtonPage_*8+i<int(routes.size());++i){int index=mapButtonPage_*8+i;auto& route=routes[index];std::string input=route.button<0?hats[-route.button-1]:"Device "+std::to_string(route.device+1)+" / button "+std::to_string(route.button+1);
            button({270,104.f+i*47,684,39},input,{MenuOp::MapButton,index},false,true,bindingNames[size_t(route.binding)]);
        }
        button({270,506,155,32},"Previous",{MenuOp::MapButtonPage,0,-1},false,mapButtonPage_>0);
        button({445,506,155,32},"Next",{MenuOp::MapButtonPage,0,1},false,(mapButtonPage_+1)*8<int(routes.size()));
        button({625,506,329,32},"Add button route",{MenuOp::MapButtonAdd},false,routes.size()<512);
        button({625,550,329,38},"Back to routes",{MenuOp::Navigate,int(Screen::ControllerMap)});break;
    }
    case Screen::ControllerMapButton: {
        auto& route=draft_.controller.mapButtons.at(mapButton_);view_.subtitle="Edit button route";
        constexpr const char* hats[]={"Hat up","Hat right","Hat down","Hat left"};
        row(120,"Device slot",std::to_string(route.device+1),MenuOp::MapButtonDevice,0,route.button>=0);
        row(195,"Physical input",route.button<0?hats[-route.button-1]:"Button "+std::to_string(route.button+1),MenuOp::MapButtonInput);
        button({270,285,684,48},"Action",{MenuOp::MapButtonAction},false,true,bindingNames[size_t(route.binding)]);
        line(270,385,"Several routes may share a button. Command-hat directions use the command-hat axis route, independent of this device slot.",684);
        button({270,480,325,38},"Delete route",{MenuOp::MapButtonDelete});
        button({629,550,325,38},"Back to buttons",{MenuOp::Navigate,int(Screen::ControllerMapButtons)});break;
    }
    case Screen::ControllerMapAction: {
        view_.subtitle="Assign routed button";
        for(int row=0;row<20;++row){int binding=controllerActionPage_*20+row;if(binding>=int(Binding::Count))break;
            button({row<10?270.f:625.f,114.f+(row%10)*38,329,34},bindingNames[binding],{MenuOp::MapButtonAssign,binding});
        }
        button({270,525,155,36},"Previous",{MenuOp::ControllerPage,1,-1},false,controllerActionPage_>0);
        button({440,525,155,36},"Next",{MenuOp::ControllerPage,1,1},false,(controllerActionPage_+1)*20<int(Binding::Count));
        button({625,525,329,36},"Back to route",{MenuOp::Navigate,int(Screen::ControllerMapButton)});break;
    }
    case Screen::ControllerAxes: {
        view_.subtitle="Controller axes";constexpr const char* names[]={"Yaw","Pitch","Roll","Throttle"};
        for(int i=0;i<4;++i) {
            int axis=draft_.controller.axes[i];float y=110+i*94;
            row(y,names[i],axis==-2?"Automatic":axis==-1?"Disabled":"Axis "+std::to_string(axis+1),MenuOp::ControllerAxis,i);
            button({615,y+38,339,30},draft_.controller.invert[i]?"Direction: reversed":"Direction: normal",{MenuOp::ControllerInvert,i});
        }
        line(270,500,"Gamepads: axes 1/2 are the left stick, 3/4 the right stick. Automatic throttle uses axis 4 on a joystick and is disabled on gamepads.",684);
        button({270,558,684,34},"Back to controller",{MenuOp::Navigate,int(Screen::Controller)});break;
    }
    case Screen::ControllerButtons: {
        view_.subtitle="Controller buttons";
        for(int row=0;row<20;++row) {
            int button=controllerPage_*20+row;if(button>=64)break;
            int binding=draft_.controller.buttons[button];int actual=binding==-2?Controller::defaultButton(controllerGamepad_,button):binding;
            std::string label="Button "+std::to_string(button+1),value=actual<0?"Unbound":bindingNames[actual];if(binding==-2)value="Auto: "+value;
            this->button({row<10?270.f:625.f,100.f+(row%10)*40,329,34},label,{MenuOp::ControllerButton,button},false,true,value);
        }
        button({270,525,155,36},"Previous",{MenuOp::ControllerPage,0,-1},false,controllerPage_>0);
        button({440,525,155,36},"Next",{MenuOp::ControllerPage,0,1},false,controllerPage_<3);
        button({625,525,329,36},"Back to controller",{MenuOp::Navigate,int(Screen::Controller)});break;
    }
    case Screen::ControllerAction: {
        view_.subtitle="Assign button "+std::to_string(controllerButton_+1);
        button({270,96,325,32},"Automatic assignment",{MenuOp::ControllerAssign,-2});
        button({629,96,325,32},"Unbound",{MenuOp::ControllerAssign,-1});
        for(int row=0;row<20;++row) {
            int binding=controllerActionPage_*20+row;if(binding>=int(Binding::Count))break;
            button({row<10?270.f:625.f,144.f+(row%10)*36,329,32},bindingNames[binding],{MenuOp::ControllerAssign,binding});
        }
        button({270,525,155,36},"Previous",{MenuOp::ControllerPage,1,-1},false,controllerActionPage_>0);
        button({440,525,155,36},"Next",{MenuOp::ControllerPage,1,1},false,(controllerActionPage_+1)*20<int(Binding::Count));
        button({625,525,329,36},"Back to buttons",{MenuOp::Navigate,int(Screen::ControllerButtons)});break;
    }
    case Screen::QuickChat: {
        view_.subtitle="Quick communication";
        row(96,"Recipient",chatChannelNames[quickChannel_],MenuOp::ChatChannel);
        if(quickChat_ && !quickPath_.empty()) {
            const auto& entries=quickChat_->menus.at(quickPath_.back()).items;
            for(int row=0;row<20;++row) {
                int index=quickPage_*20+row;if(index>=int(entries.size()))break;auto& entry=entries[index];
                button({row<10?270.f:620.f,148.f+(row%10)*36,334,32},entry.label,{MenuOp::QuickChatItem,index},false,true,std::string(1,entry.key));
            }
            button({270,520,155,32},"Previous page",{MenuOp::QuickChatPage,0,-1},false,quickPage_>0);
            button({440,520,155,32},"Next page",{MenuOp::QuickChatPage,0,1},false,(quickPage_+1)*20<int(entries.size()));
            button({620,520,334,32},quickPath_.size()>1?"Back":"Return to flight",{MenuOp::QuickChatBack});
        }
        line(270,567,"Type a message shortcut. Tab changes recipient. Escape goes back.",684);break;
    }
    case Screen::RemoteLobby:
        view_.subtitle="Multiplayer lobby";line(270,110,networkCountdown_>=0?"Match starts in "+std::to_string(networkCountdown_)+" seconds":"Waiting for pilots",650,true);
        row(125,"Your team",std::to_string(session_.team+1),MenuOp::NetworkJoinTeam,0,networkLobby_);
        pilotPage_=std::clamp(pilotPage_,0,std::max(0,(int(networkPilots_.size())-1)/10));
        for(int i=0;i<10 && pilotPage_*10+i<int(networkPilots_.size());++i){auto& p=networkPilots_[pilotPage_*10+i];line(270,180+i*25,p.name+" / Team "+std::to_string(p.team+1)+" / "+(p.ready?"Ready":"Not ready"),650);}
        button({270,445,130,32},"Previous",{MenuOp::PilotPage,0,-1},false,pilotPage_>0);
        button({414,445,390,32},"Mission setup",{MenuOp::NetworkEdit},false,networkLobby_ && networkHost_);
        button({824,445,130,32},"Next",{MenuOp::PilotPage,0,1},false,(pilotPage_+1)*10<int(networkPilots_.size()));
        button({270,501,325,42},networkReady_?"Not ready":"Mark ready",{MenuOp::NetworkReady},networkReady_,networkLobby_);
        button({627,501,327,42},networkHost_?"Start match":"Waiting for host",{MenuOp::NetworkStart},false,networkLobby_ && networkHost_ && networkReady_ && std::all_of(networkPilots_.begin(),networkPilots_.end(),[](auto& p){return p.ready;}));
        break;
    case Screen::Online:
        view_.subtitle="Multiplayer";line(270,110,"Join a native Allegiance server",650,true);
        button({270,175,684,40},editingAddress_?"Server address (Enter to finish)":"Server address",{MenuOp::NetworkAddress},editingAddress_,!playing_,address_+(editingAddress_?"_":""));
        row(230,"Team",std::to_string(networkSide_+1),MenuOp::NetworkTeam,0,!playing_);
        button({270,310,684,48},"Connect",{MenuOp::Connect},false,!playing_ && !address_.empty());
        line(270,395,"Use an IP address and port, for example 127.0.0.1:27500. Your callsign is configured on the home screen.",650);
        break;
    case Screen::Pause:
        view_.subtitle=inactive_?"Team eliminated":"Session paused";line(270,110,inactive_?"No restart station remains":ship_+" / "+sector_,650,true);
        if(inactive_)line(270,198,"Your team has been eliminated. The remaining teams are still playing.",684);
        line(270,162,"Kills: "+std::to_string(kills_)+"    Deaths: "+std::to_string(deaths_));
        button({270,247,684,48},inactive_?"Return to game":"Resume flight",{MenuOp::Resume});
        button({270,315,684,48},docked_?"Hangar and ship selection":"Return to base (autopilot)",{docked_?MenuOp::Navigate:MenuOp::ReturnToBase,int(Screen::Hangar)},false,!gameplayClosed());
        button({270,383,684,48},"Configuration",{MenuOp::Navigate,int(Screen::Settings)});
        if(!docked_)button({270,443,684,38},"Cargo and equipment",{MenuOp::Navigate,int(Screen::Cargo)},false,!gameplayClosed() && !passenger_);
        button({270,491,684,38},"Team scores",{MenuOp::Navigate,int(Screen::Scores)});
        button({270,537,684,38},"Pilot commands",{MenuOp::Navigate,int(Screen::PilotOrders)});break;
    case Screen::PilotOrders: {
        view_.subtitle="Pilot commands";
        constexpr const char* labels[]={"Queued command","Accepted command","Current target"};
        for(int i=0;i<3;++i){line(270,110+i*75,labels[i],650,true);line(270,138+i*75,i==0 && moneyRequest_.sender>=0?censorChat(moneyRequest_.prompt,settings_.chat.censor):commands_[i].command<0?"None":commands_[i].text,684);}
        bool queued=!gameplayClosed() && (commands_[0].command>=0 || moneyRequest_.sender>=0),accepted=!gameplayClosed() && commands_[1].command>=0;
        button({270,357,216,40},"Accept",{MenuOp::AcceptOrder},false,queued);
        button({504,357,216,40},"Accept and execute",{MenuOp::ExecuteOrder},false,queued);
        button({738,357,216,40},"Reject",{MenuOp::RejectOrder},false,queued);
        button({270,415,325,40},"Target accepted order",{MenuOp::TargetOrder},false,accepted);
        button({629,415,325,40},"Clear accepted order",{MenuOp::ClearOrder},false,accepted);
        line(270,480,"Selecting a target keeps your accepted command. Accept and execute engages autopilot when the destination is known.",684);
        button({270,550,684,38},"Back to session",{MenuOp::Navigate,int(Screen::Pause)});break;
    }
    case Screen::ShipLoadout:
    case Screen::Hangar: {
        if(ghost_){line(270,110,"No lives remain",650,true);line(270,165,"Continue observing and commanding your team.");button({270,260,684,42},"Resume command view",{MenuOp::Resume});button({270,320,684,42},"Teleport to a base",{MenuOp::Teleport},false,!authoredLoadouts_);break;}
        view_.subtitle="Hangar / "+station_;
        if(passenger_) {
            line(270,110,"Aboard "+ship_,650,true);
            line(270,165,"The pilot controls launch and loadout. You can change turret or disembark while docked.");
            button({270,260,325,42},"Crew and turret positions",{MenuOp::Crew});
            button({270,320,325,42},"Disembark",{MenuOp::Disembark});
            button({629,260,325,42},"Teleport to a base",{MenuOp::Teleport},false,!authoredLoadouts_);break;
        }
        line(270,100,"Choose a ship",650,true);
        if(economy_ || authoredLoadouts_) {
            hullPage_=std::clamp(hullPage_,0,std::max(0,(int(hulls_.size())-1)/6));
            for(int i=0;i<6 && hullPage_*6+i<int(hulls_.size());++i){auto& h=hulls_[hullPage_*6+i];button({270,139+float(i)*41,310,34},h.name,{MenuOp::HangarHull,h.id},false,true,economy_?std::to_string(h.price):"");}
            button({270,389,145,30},"Previous ships",{MenuOp::HullPage,0,-1},false,hullPage_>0);
            button({430,389,150,30},"Next ships",{MenuOp::HullPage,0,1},false,(hullPage_+1)*6<int(hulls_.size()));
        } else for(size_t i=0;i<hullNames.size();++i)button({270,139+float(i)*46,310,36},hullNames[i],{MenuOp::HangarHull,int(i)});
        line(620,100,ship_,334,true);
        int count=int(std::count_if(equipmentRows_.begin(),equipmentRows_.end(),[](auto& e){return e.mount>=0;}));
        equipmentPage_=std::clamp(equipmentPage_,0,std::max(0,(count-1)/10));
        int row=0,seen=0;
        for(size_t i=0;i<equipmentRows_.size();++i)if(equipmentRows_[i].mount>=0) {
            if(seen++<equipmentPage_*10)continue;
            if(row==10)break;
            button({620,139+float(row++)*30,334,25},equipmentRows_[i].name,{MenuOp::EquipmentSlot,int(i)});
        }
        if(count>10) {
            button({620,449,160,30},"Previous slots",{MenuOp::EquipmentPage,0,-1},false,equipmentPage_>0);
            button({794,449,160,30},"Next slots",{MenuOp::EquipmentPage,0,1},false,(equipmentPage_+1)*10<count);
        }
        button({270,486,220,32},"Cargo reserves",{MenuOp::Navigate,int(Screen::Cargo)});
        button({502,486,220,32},"Saved loadouts",{MenuOp::Loadouts});
        button({734,486,220,32},"Teleport to a base",{MenuOp::Teleport},false,!authoredLoadouts_);
        line(270,525,authoredLoadouts_?"Training supplies equipment allowed by this base and mission.":economy_?"Purchases use your credits and this base's technology.":"Practice provides equipment freely.",684);
        button({270,546,684,42},"Launch",{MenuOp::Launch});break;
    }
    case Screen::Teleport:
        view_.subtitle="Teleport from / "+station_;
        for(int row=0;row<8;++row){int i=teleportPage_*8+row;if(i>=int(teleports_.size()))break;auto& destination=teleports_[i];
            button({270,104.f+row*46,684,38},destination.name+" / "+destination.sector,{MenuOp::TeleportTo,destination.id},destination.current,!destination.current || passenger_,destination.current?"Current base":destination.keepsShip?"Keep ship":"Replace loadout");
        }
        button({270,484,155,32},"Previous",{MenuOp::TeleportPage,0,-1},false,teleportPage_>0);
        button({445,484,155,32},"Next",{MenuOp::TeleportPage,0,1},false,(teleportPage_+1)*8<int(teleports_.size()));
        line(270,532,"A base must belong to your team or an ally and support restarting. Unavailable hulls are refunded and refitted using that base's equipment.",684);
        button({620,484,334,32},"Back to hangar",{MenuOp::Navigate,int(Screen::Hangar)});break;
    case Screen::Loadouts: {
        view_.subtitle="Hangar / saved loadouts";
        line(270,100,"Ten loadout banks",650,true);
        for(int slot=0;slot<10;++slot) {
            auto item=std::find_if(loadoutChoices_.begin(),loadoutChoices_.end(),[&](auto& c){return c.id==slot;});
            button({270,138.f+slot*31,684,27},std::to_string(slot+1)+" / "+(item==loadoutChoices_.end()?"Empty":item->name),{MenuOp::LoadoutSlot,slot},loadoutSlot_==slot);
        }
        bool present=std::any_of(loadoutChoices_.begin(),loadoutChoices_.end(),[&](auto& c){return c.id==loadoutSlot_;});
        line(270,457,"Each bank remembers one setup per hull. Fitting uses available station equipment and credits.",684);
        button({270,500,216,38},"Save current ship",{MenuOp::SaveLoadout,loadoutSlot_},false,docked_ && !passenger_);
        button({504,500,216,38},"Fit saved ship",{MenuOp::UseLoadout,loadoutSlot_},false,present && docked_ && !passenger_);
        button({738,500,216,38},"Clear bank",{MenuOp::DeleteLoadout,loadoutSlot_},false,present);
        button({270,550,684,38},"Back to hangar",{MenuOp::Navigate,int(Screen::Hangar)});break;
    }
    case Screen::Cargo:
        view_.subtitle=docked_?"Hangar / cargo reserves":"Flight / cargo and equipment";
        line(270,100,"Cargo slots",650,true);
        for(int slot=1;slot<=5;++slot) {
            auto item=std::find_if(equipmentRows_.begin(),equipmentRows_.end(),[&](auto& e){return e.mount==-slot;});
            std::string label="Slot "+std::to_string(slot)+" / ";
            label+=item==equipmentRows_.end()?"Empty":item->name+" ("+std::to_string(item->amount)+")";
            button({270,142.f+(slot-1)*55,684,43},std::move(label),{docked_?MenuOp::EquipmentSlot:MenuOp::CargoSelect,-slot},!docked_ && selectedCargo_==-slot,!passenger_);
        }
        if(docked_) {
            line(270,440,"Ammo, fuel and launcher reserves reload automatically when depleted. Reloading takes time. Choose a slot to stock or clear it.",684);
            button({270,535,325,42},"Back to hangar",{MenuOp::Navigate,int(Screen::Hangar)});
            button({629,535,325,42},"Launch",{MenuOp::Launch});
        } else {
            button({270,435,210,38},"Next cargo stack",{MenuOp::CargoNext},false,!passenger_);
            button({507,435,210,38},"Mount / swap",{MenuOp::CargoMounts},false,!passenger_);
            button({744,435,210,38},"Drop one item",{MenuOp::CargoDrop},false,!passenger_);
            line(270,490,"Select a cargo stack, then a compatible equipment slot. Empty cargo slots can receive unmounted equipment. Ammo and fuel packs use Reload.",684);
            button({270,555,325,38},"Reload reserves",{MenuOp::Reload},false,!passenger_);
            button({629,555,325,38},"Resume flight",{MenuOp::Resume});
        }
        break;
    case Screen::CargoMount:
        view_.subtitle="Flight / mount equipment";
        line(270,100,"Choose the destination slot",650,true);
        for(int i=0;i<9 && partPage_*9+i<int(choices_.size());++i){auto& c=choices_[partPage_*9+i];button({270,142.f+i*39,684,33},c.name,{MenuOp::CargoSwap,c.id});}
        if(choices_.empty())line(270,162,"No compatible equipment slot. Ammo and fuel packs use Reload.");
        button({270,513,160,33},"Previous",{MenuOp::PartPage,0,-1},false,partPage_>0);
        button({460,513,160,33},"Next",{MenuOp::PartPage,0,1},false,(partPage_+1)*9<int(choices_.size()));
        button({650,513,304,33},"Back to cargo",{MenuOp::Navigate,int(Screen::Cargo)});break;
    case Screen::Parts:
        view_.subtitle=partMount_<0?"Hangar / cargo slot "+std::to_string(-partMount_):"Hangar / compatible equipment";
        line(270,100,partMount_<0?"Choose cargo":"Choose replacement equipment",650,true);
        for(int i=0;i<9 && partPage_*9+i<int(choices_.size());++i){auto& c=choices_[partPage_*9+i];button({270,142.f+i*39,684,33},c.name,{MenuOp::EquipPart,c.id},false,true,economy_ && c.id>=0?std::to_string(c.price):"");}
        button({270,513,160,33},"Previous",{MenuOp::PartPage,0,-1},false,partPage_>0);
        button({460,513,160,33},"Next",{MenuOp::PartPage,0,1},false,(partPage_+1)*9<int(choices_.size()));
        button({650,513,304,33},partMount_<0?"Back to cargo":"Back to hangar",{MenuOp::Navigate,int(partMount_<0?Screen::Cargo:Screen::Hangar)});
        line(270,565,"Page "+std::to_string(partPage_+1)+" / "+std::to_string(std::max(1,(int(choices_.size())+8)/9)));break;
    case Screen::Pilots: {
        view_.subtitle="Team pilots / credits and wings";
        pilotPage_=std::clamp(pilotPage_,0,std::max(0,(int(pilots_.size())-1)/6));
        for(int row=0;row<6 && pilotPage_*6+row<int(pilots_.size());++row) {
            auto& pilot=pilots_[pilotPage_*6+row];float y=100+row*60;
            button({270,y,684,31},censorChat(pilot.name,settings_.chat.censor)+(pilot.allied?" / Ally":"")+(pilot.commander?" / Commander":"")+(pilot.controlled?" / You":""),{MenuOp::Account,pilot.id});
            line(280,y+35,pilot.ship+" / "+pilot.location+" / Wing "+std::to_string(pilot.wing+1)+(economy_ && !pilot.allied?" / "+std::to_string(pilot.money)+" credits":""),650);
        }
        button({270,485,160,33},"Previous",{MenuOp::PilotPage,0,-1},false,pilotPage_>0);
        button({460,485,160,33},"Next",{MenuOp::PilotPage,0,1},false,(pilotPage_+1)*6<int(pilots_.size()));
        button({650,485,304,33},"Crew / turrets",{MenuOp::Crew},false,!gameplayClosed());
        if(moneyRequest_.sender>=0) {
            line(270,535,censorChat(moneyRequest_.name,settings_.chat.censor)+" requests "+std::to_string(moneyRequest_.amount)+" credits.",410);
            button({715,531,113,32},"Accept",{MenuOp::MoneyResponse,1},false,!gameplayClosed());
            button({840,531,114,32},"Decline",{MenuOp::MoneyResponse,0},false,!gameplayClosed());
        }break;
    }
    case Screen::Account: {
        auto it=std::find_if(pilots_.begin(),pilots_.end(),[&](auto& pilot){return pilot.id==accountId_;});
        if(it==pilots_.end()){view_.subtitle="Pilot unavailable";break;}
        auto& pilot=*it;view_.subtitle=censorChat(pilot.name,settings_.chat.censor)+(pilot.allied?" / Ally":"")+(pilot.commander?" / Commander":"");
        line(270,100,pilot.ship+" / "+pilot.location,650,true);
        if(economy_) {
            line(270,145,pilot.allied?"Allied pilot / account private":"Account: "+std::to_string(pilot.money)+" credits",650);
            auto donation=std::find_if(pilots_.begin(),pilots_.end(),[&](auto& p){return p.id==pilot.autoDonate;});
            line(270,180,"Automatic donation: "+(donation==pilots_.end()?std::string("Disabled"):censorChat(donation->name,settings_.chat.censor)),650);
            row(225,"Amount",std::to_string(moneyAmount_),MenuOp::MoneyAmount,0,!gameplayClosed());
            if(pilot.controlled) {
                button({270,280,330,37},"Request credits",{MenuOp::RequestMoney},false,!gameplayClosed() && pilots_.size()>1);
                button({624,280,330,37},"Keep future income",{MenuOp::AutoDonate,-1},false,!gameplayClosed() && pilot.autoDonate>=0);
            } else {
                button({270,280,330,37},"Donate credits",{MenuOp::Donate,pilot.id},false,!gameplayClosed() && money_>=moneyAmount_);
                button({624,280,330,37},"Auto-donate future income",{MenuOp::AutoDonate,pilot.id},false,!gameplayClosed() && !pilot.allied);
                button({270,335,684,37},"Hand over command and credits",{MenuOp::HandOver,pilot.id},false,!gameplayClosed() && commander_ && !pilot.allied);
            }
        }
        row(410,"Wing",std::to_string(pilot.wing+1),MenuOp::Wing,pilot.id,!gameplayClosed() && !pilot.allied && (pilot.controlled || commander_));
        if(pilot.allied || (!pilot.controlled && !commander_))line(270,448,pilot.allied?"Wings and automatic donations belong to each team.":"Only the commander can assign another pilot's wing.",650);
        if(economy_ && !networked_ && !pilot.controlled && !pilot.allied) {
            button({270,478,330,30},"Control this local pilot",{MenuOp::ControlPilot,pilot.id},false,!gameplayClosed() && !pilot.ghost);
            button({624,478,330,30},"Remove local pilot",{MenuOp::RemovePilot,pilot.id},false,!gameplayClosed() && commander_);
        }
        button({270,515,330,38},pilot.muted?"Unmute pilot":"Mute pilot",{MenuOp::MutePilot,pilot.id,pilot.muted?0:1},false,!pilot.controlled || pilot.muted);
        button({624,515,330,38},"Back to team pilots",{MenuOp::Navigate,int(Screen::Pilots)});break;
    }
    case Screen::Crew: {
        view_.subtitle="Crew / turret positions";
        line(270,99,passenger_?"Aboard "+ship_:"Team pilots",650,true);
        crewPage_=std::clamp(crewPage_,0,std::max(0,(int(crew_.size())-1)/6));
        int row=0;
        for(int i=crewPage_*6;i<int(crew_.size()) && row<6;++i,++row) {
            auto& p=crew_[i];float y=140.f+row*50.f;
            line(270,y,censorChat(p.name,settings_.chat.censor)+(p.turret>=0?" / turret "+std::to_string(p.turret+1):""),300);
            line(270,y+21,p.ship+" / "+p.location,365);
            if(p.promotable)button({705,y,249,35},"Promote to pilot",{MenuOp::Promote,p.id});
            else button({705,y,249,35},p.aboard && passenger_?"Next turret":"Board turret",{MenuOp::Board,p.id},p.aboard,p.boardable);
        }
        button({270,452,155,33},"Previous",{MenuOp::CrewPage,0,-1},false,crewPage_>0);
        button({445,452,155,33},"Next",{MenuOp::CrewPage,0,1},false,(crewPage_+1)*6<int(crew_.size()));
        if(passenger_)button({650,452,304,33},"Disembark",{MenuOp::Disembark},false,docked_);
        line(270,507,"Board a friendly pilot while both ships are docked. In flight, change turrets aboard your current ship. The pilot can promote a gunner.",684);
        break;
    }
    case Screen::Research: {
        view_.subtitle="Research and production";
        line(270,100,"Credits: "+std::to_string(money_)+" / fund remaining cost",650,true);
        std::vector<const ResearchState*> rows;for(auto& r:research_)if(r.available || r.invested || r.complete || (r.limit>=0 && r.count>0 && r.count>=r.limit))rows.push_back(&r);
        researchPage_=std::clamp(researchPage_,0,std::max(0,(int(rows.size())-1)/9));
        for(int i=0;i<9 && researchPage_*9+i<int(rows.size());++i){auto& r=*rows[researchPage_*9+i];
            std::string value=r.complete?"Complete":std::to_string(r.invested)+" / "+std::to_string(r.price)+"    "+std::to_string(r.progress)+"%";
            auto name=r.name;if(r.limit>=0)name+=" ("+std::to_string(r.count)+"/"+std::to_string(r.limit)+")";
            if(r.limit>=0 && r.count>=r.limit)value="At maximum";
            button({270,139.f+i*39,684,33},name,{MenuOp::Invest,r.id},false,r.available&&!r.complete&&money_>0&&r.invested<r.price,value);
        }
        button({270,513,220,33},"Previous",{MenuOp::ResearchPage,0,-1},false,researchPage_>0);
        button({520,513,220,33},"Next",{MenuOp::ResearchPage,0,1},false,(researchPage_+1)*9<int(rows.size()));
        line(270,562,"Construction and mining continue while this screen is open.",684);break;
    }
    case Screen::Orders:
        view_.subtitle="Team orders";line(270,100,"Selected target: "+(target_.empty()?std::string("None"):target_),650,true);
        researchPage_=std::clamp(researchPage_,0,std::max(0,(int(units_.size())-1)/9));
        for(int i=0;i<9 && researchPage_*9+i<int(units_.size());++i){auto& u=units_[researchPage_*9+i];button({270,145.f+i*38,684,32},u.name,{MenuOp::Unit,u.id},false,true,u.order);}
        button({270,513,220,33},"Previous",{MenuOp::ResearchPage,0,-1},false,researchPage_>0);
        button({520,513,220,33},"Next",{MenuOp::ResearchPage,0,1},false,(researchPage_+1)*9<int(units_.size()));
        line(270,562,"Choose a friendly ship, its order, then a target in any discovered sector.",684);break;
    case Screen::UnitOrders: {
        view_.subtitle="Issue order";
        constexpr const char* names[]={"Default order","Attack","Capture","Defend","Pick up","Go to","Repair","Join","Mine","Build"};
        for(int i=0;i<10;++i)button({270+float(i/5)*354,139.f+(i%5)*62,330,48},names[i],{MenuOp::Order,unitId_,i});
        button({270,480,330,40},"Stop / hold position",{MenuOp::Order,unitId_,-3});
        button({624,480,330,40},"Take me to",{MenuOp::TakeMeTo,unitId_});
        auto unit=std::find_if(units_.begin(),units_.end(),[&](auto& u){return u.id==unitId_;});
        if(unit!=units_.end() && unit->type<playerPilot) {
            bool resume=unit->docked && unit->stayDocked;
            button({270,535,684,40},resume?"Resume duties":unit->docked?"Stay docked":"Return to base and stay docked",{MenuOp::DroneStandby,unitId_,int(!resume)});
        }
        break;
    }
    case Screen::OrderTargets:
        view_.subtitle="Choose an order target";
        line(270,100,orderTargets_.empty()?"No known target supports this order.":"Known targets / sector",650,true);
        for(int i=0;i<9 && targetPage_*9+i<int(orderTargets_.size());++i){auto& t=orderTargets_[targetPage_*9+i];
            button({270,139.f+i*39,684,33},t.name,{MenuOp::OrderTarget,targetPage_*9+i},false,true,t.sector);
        }
        button({270,513,220,33},"Previous",{MenuOp::TargetPage,0,-1},false,targetPage_>0);
        button({520,513,220,33},"Next",{MenuOp::TargetPage,0,1},false,(targetPage_+1)*9<int(orderTargets_.size()));
        button({270,562,684,33},"Back to orders",{MenuOp::Navigate,int(Screen::UnitOrders)});break;
    case Screen::Results:
        view_.subtitle="Session results";line(270,120,result_.empty()?"Session complete":result_,650,true);
        line(270,178,"Kills: "+std::to_string(kills_)+"    Deaths: "+std::to_string(deaths_));
        button({270,290,684,48},authoredLoadouts_?"Return to training":"Return to scenarios",{MenuOp::Leave});
        button({270,355,684,48},"Team scores",{MenuOp::Navigate,int(Screen::Scores)});if(networked_ && networkHost_)button({270,430,684,42},"New multiplayer round",{MenuOp::NetworkNewRound});break;
    case Screen::ConfirmLeave:
        view_.subtitle="Leave session";line(270,145,networked_?"Disconnect from this match?":"End this local session?",650,true);
        line(270,195,networked_?"The server match continues for the remaining pilots.":"Your current flight and session score will be discarded.");
        button({270,310,320,48},"Keep playing",{MenuOp::Resume});button({620,310,334,48},"Leave session",{MenuOp::Leave});break;
    case Screen::Credits:
        view_.subtitle="Credits";line(270,105,"Microsoft Allegiance / FreeAllegiance",650,true);
        line(270,157,"Original game by Microsoft Research. Maintained and extended by the FreeAllegiance community. Original artwork, music, and game data remain credited to their respective creators.");
        line(270,281,"Native client",650,true);
        line(270,324,"Vulkan rendering, SDL windowing and input, OpenAL Soft audio, FFmpeg media decoding, FreeType and Fontconfig text, GLM mathematics. See the repository and installed game data for license notices.");break;
    case Screen::Help:case Screen::GameDetails:break;
    }
    if(original_)originalLayout();
    if(original_ && screen_!=Screen::Teleport && screen_!=Screen::Research && consoleClosingSlide_){
        view_.slide=consoleClosingSlide_;view_.slide->artworkBegin=view_.artwork.size();view_.slide->itemsBegin=view_.items.size();
        view_.artwork.insert(view_.artwork.end(),consoleClosing_.begin(),consoleClosing_.end());
        view_.slide->artworkEnd=view_.artwork.size();view_.slide->itemsEnd=view_.items.size();
    }
    view_.focus=std::clamp(view_.focus,0,std::max(0,int(view_.items.size())-1));
}
void Frontend::originalLayout() {
    if(screen_==Screen::GameDetails){originalMissionDetails();return;}
    if(screen_==Screen::Games){originalGames();return;}
    if(screen_==Screen::Teleport){originalTeleport();return;}
    if(screen_==Screen::Research){originalResearch();return;}
    if(originalSecondaryLayout())return;
    if(screen_==Screen::Controls){originalMappings();return;}
    if(screen_==Screen::Credits){
        auto scale=view_.classicScale;view_=secondaryBackdrop_;view_.classicScale=scale;view_.original=true;view_.text.clear();
        view_.blockInput();
        markPopupLayer();
        auto popup=original_->layout("creditspane","screen",{}, {},false,{{"messagePane",original_->paneSize("creditspane","messagePane")}});glm::vec2 size{512,300};
        if(!popup.empty())size={popup.front().bounds.z,popup.front().bounds.w};auto origin=glm::floor((glm::vec2(view_.canvas)-size)*.5f);
        for(auto& c:popup){c.translate(origin);
            if(c.name=="closeButtonPane")view_.items.push_back({c.visibleBounds(),"Close credits.",{},{MenuOp::SecondaryClose},true,false,c.name});view_.artwork.push_back(std::move(c));}
        if(!creditsOffset_)creditsOffset_=std::make_shared<HudScalar>(*original_,"creditspane","y");
        MenuScroll scroll;scroll.bounds=authoredControl(view_.artwork,"messagePane").visibleBounds();scroll.started=creditsStarted_;scroll.offset=creditsOffset_;scroll.artwork=original_->imageLayout("creditstext");view_.scroll=std::move(scroll);return;
    }
    if(screen_==Screen::Results){originalResults();return;}
    if(screen_==Screen::ShipLoadout && loadoutPreview_.valid){originalLoadout();return;}
    if(screen_==Screen::Lobby && session_.scenario==Scenario::Conquest){originalGameSettings();return;}
    std::string script;
    UiNumbers values{{"NoMissionSelected",0},{"hoverNone",0},{"CurrentHover",0}};
    for(int i=1;i<=8;++i)values["missionNumber"+std::to_string(i)]=i;
    values["missionNumber"]=trainingSelection_;values["BoardState"]=passenger_?(turret_<0?1:2):0;
    if(screen_==Screen::Home)script="introscreen";
    else if(screen_==Screen::Training)script="trainingscreen";
    else if(screen_==Screen::Hangar)script="hangar";
    else if(screen_==Screen::RemoteLobby || screen_==Screen::LobbyPilots || screen_==Screen::Pilots || screen_==Screen::Countdown)script="teamscreen";
    else {originalCommandMenu();return;}
    // These are original host events, rather than replacement captions. MDL
    // owns the text, branch styles and geometry for each hover value.
    std::vector<std::pair<const char*,const char*>> hoverEvents;
    if(script=="hangar")hoverEvents={{"launchButtonPane","hoverLaunch"},{"commandButtonPane","hoverCommand"},{"loadoutButtonPane","hoverLoadout"},{"investButtonPane","hoverInvest"},{"helpButtonPane","hoverHelp"},{"teleportButtonPane","hoverTeleport"},{"teamButtonPane","hoverTeam"},{"exitButtonPane","hoverExit"}};
    if(script=="introscreen")hoverEvents={{"playLanButtonPane","hoverPlayLan"},{"playIntButtonPane","hoverPlayInt"},{"trainBigButtonPane","hoverTrain"},{"zoneWebButtonPane","hoverZoneWeb"},{"optionsButtonPane","hoverOptions"},{"introButtonPane","hoverIntro"},{"creditsButtonPane","hoverCredits"},{"quickstartButtonPane","hoverQuickstart"},{"exitButtonPane","hoverExit"},{"helpButtonPane","hoverHelp"},{"discordButtonPane","hoverDiscord"},{"steamLeaderboardButtonPane","hoverSteamLeaderboard"}};
    for(size_t i=0;i<hoverEvents.size();++i)values[hoverEvents[i].second]=int(i)+1;
    UiStrings strings;
    if(script=="teamscreen"){
        if(teamPage_!=noTeam)teamPage_=std::clamp(teamPage_,0,session_.rules.teams-1);
        bool populated=teamPage_>=0 && (networked_?std::any_of(networkPilots_.begin(),networkPilots_.end(),[&](const auto& p){return p.team==teamPage_;}):
            teamPage_==session_.team || std::any_of(session_.localPilots.begin(),session_.localPilots.end(),[&](const auto& p){return p.team==teamPage_;}));
        // OnSelTeam redirects an empty inactive side to No Team. Local
        // pregame setup has a mandatory pilot seat, so retain that seat.
        if(teamPage_>=0 && !teamActive(teamPage_) && !populated){
            teamPage_=networked_?noTeam:session_.team;
            rosterList_={};rosterSelected_=-100000;lobbyRecipient_=-1;
            populated=!networked_;
        }
        std::string image="defaultwatermarkbmp";glm::vec4 color{0,0,0,.5f};
        if(teamPage_>=0 && teamPage_<int(teamScores_.size())){
            color=teamScores_[teamPage_].color*.5f;color.a=.5f;
            if(populated && !teamScores_[teamPage_].civilizationIcon.empty()){
                image=teamScores_[teamPage_].civilizationIcon;std::transform(image.begin(),image.end(),image.begin(),[](unsigned char c){return std::tolower(c);});image+="lobbybmp";
            }
        }
        else if(populated){
            if(auto civ=teamCivilization(teamPage_);civ && !civ->icon.empty()){
                image=civ->icon;std::transform(image.begin(),image.end(),image.begin(),[](unsigned char c){return std::tolower(c);});image+="lobbybmp";
            }
        }
        strings["civImage"]=image;values["civColor.r"]=color.r;values["civColor.g"]=color.g;values["civColor.b"]=color.b;values["civColor.a"]=color.a;
    }
    view_.artwork=original_->layout(script,"screen",values,strings);view_.original=true;view_.items.clear();view_.text.clear();
    if(script=="hangar" && !developments_)std::erase_if(view_.artwork,[](const auto& c){return c.name=="investButtonPane";});
    for(auto& c:view_.artwork)for(auto [control,event]:hoverEvents)if(c.name==control){c.helpSite="CurrentHover";c.helpID=int(values.at(event));}
    auto text=[&](glm::vec4 b,std::string value,const char* font="promptFont",glm::vec4 color=glm::vec4(1)) {
        OriginalControl c;c.kind=OriginalControl::Kind::Text;c.bounds=b;c.text=std::move(value);c.font=font;c.color=color;c.surfaceText();view_.artwork.push_back(std::move(c));
    };
    auto image=[&](glm::vec2 p,const std::string& name,glm::vec4 color=glm::vec4(1)) {
        auto im=original_->assets().image(name,true);OriginalControl c;c.kind=OriginalControl::Kind::Image;c.bounds={p,im->width,im->height};c.image=im;c.color=color;view_.artwork.push_back(std::move(c));
    };
    auto bind=[&](const OriginalControl& c,std::string label,MenuAction action,bool enabled=true,bool selected=false){
        view_.items.push_back({c.visibleBounds(),std::move(label),{},action,enabled,selected,c.name});
    };
    int ownSide=session_.team,ownRequest=noTeamRequest;std::string ownName=session_.pilot;
    for(const auto& p:networkPilots_)if(p.controlled){ownSide=p.team;ownRequest=p.requested;ownName=p.name;}
    bool inProgress=networked_?!complete_ && (lobbyFlags_&4)!=0:playing_;
    bool starting=networked_?(missionStage_==STAGE_STARTING || missionStage_==STAGE_NOTSTARTED && (lobbyFlags_&Allegiance::LobbyAutoRestart)):countdown_>=0;
    bool canceled=networked_ && missionStage_==STAGE_OVER;
    bool ownLeader=(networked_?commander_:!playing_ || commander_) && ownSide>=0 && teamPage_==ownSide;
    bool canSettings=networked_?networkHost_ && networkLobby_ && missionStage_==STAGE_NOTSTARTED && !(lobbyFlags_&Allegiance::LobbyObjectModelCreated):!playing_;
    bool canChangeActive=canSettings;
    std::array<int,7> counts{};
    auto count=[&](int side){int index=side==noTeam?6:side;if(index>=0 && index<7)++counts[index];};
    if(script=="teamscreen"){
        if(networked_ || playing_ && !networkPilots_.empty())for(auto& pilot:networkPilots_)count(pilot.team);
        else if(playing_)for(auto& pilot:pilots_)count(pilot.side);
        else {count(session_.team);for(auto& pilot:session_.localPilots)count(pilot.team);}
    }
    auto startState=lobbyStartState();
    auto selected=std::find_if(networkPilots_.begin(),networkPilots_.end(),[&](const auto& p){return p.id==rosterSelected_;});
    bool selectable=selected!=networkPilots_.end() && !selected->controlled;
    bool requesting=selectable && selected->requested==ownSide;
    bool teammate=selectable && selected->team==ownSide;
    auto playerActions=Allegiance::LobbyPlayerMenu(ownLeader,!selectable,teammate,requesting,lobbyAvailable(),session_.rules.lockTeamSettings);
    if(script=="teamscreen")std::erase_if(view_.artwork,[&](const auto& c){
        const auto& n=c.name;
        if(n.starts_with("activeTeamButtonPane"))return !canChangeActive || n.back()-'1'>=session_.rules.teams;
        if(n=="settingsButtonPane")return !canSettings;
        if(n=="detailsButtonPane")return canSettings;
        if(n=="gameOverButtonPane")return endgame_.players.empty();
        if(n=="randomizeButtonPane")return networked_?!networkHost_:playing_;
        if(n=="startButtonPane")return networked_?!networkHost_:playing_;
        if(n=="discordButtonPane")return teamPage_!=ownSide;
        if(n=="wingComboPane")return ownSide<0 || teamPage_!=ownSide;
        if(n=="chatHeader2")return networked_?!commander_:playing_ && !commander_;
        if(n=="sideReadyButtonPane" || n=="autoAcceptButtonPane" || n=="acceptButtonPane" || n=="rejectButtonPane" || n=="makeLeaderButtonPane" || n=="teamSettingsButtonPane")return !ownLeader;
        return false;
    });
    for(auto& c:view_.artwork)if(c.kind==OriginalControl::Kind::Button) {
        const auto& n=c.name;
        if(script=="teamscreen" && n=="wingComboPane")continue;
        if(screen_==Screen::Home) {
            if(n=="trainBigButtonPane")bind(c,"Play a training mission.",{MenuOp::Navigate,int(Screen::Training)});
            else if(n=="introButtonPane")bind(c,"Play the Allegiance introductory video.",{MenuOp::Intro});
            else if(n=="playIntButtonPane")bind(c,"Play a game on a public server.",{MenuOp::Navigate,int(Screen::Games)});
            else if(n=="playLanButtonPane")bind(c,"Play a game on a private server located on a LAN.",{MenuOp::Navigate,int(Screen::Online)});
            else if(n=="optionsButtonPane")bind(c,"Customize the graphics, sound, and controls in Allegiance.",{MenuOp::Navigate,int(Screen::Settings)});
            else if(n=="creditsButtonPane")bind(c,"View the names of those who created Allegiance.",{MenuOp::Navigate,int(Screen::Credits)});
            else if(n=="exitButtonPane")bind(c,"Quit Allegiance.",{MenuOp::Quit});
            else if(n=="helpButtonPane")bind(c,"View the in-game Help display.",{MenuOp::Help});
            else if(n=="discordButtonPane")bind(c,"Join the community on Discord - opens in a webbrowser!",{MenuOp::Web,0});
            else if(n=="steamLeaderboardButtonPane")bind(c,"Check out the leaderboards on Steam!",{MenuOp::Web,1});
            else if(n=="zoneWebButtonPane")bind(c,"Connect to the Allegiance Web page.",{MenuOp::Web,2});
        } else if(screen_==Screen::Training) {
            if(n=="backButtonPane")bind(c,"Back to main menu",{MenuOp::Navigate,int(Screen::Home)});
            else if(n=="trainButtonPane")bind(c,"Begin training",{MenuOp::TrainingStart,trainingSelection_},trainingSelection_!=0,true);
            else if(n.starts_with("trainMission") && n.size()>12) {
                int id=n[12]-'0';bind(c,"Select training lesson",{MenuOp::TrainingSelect,id},true,id==trainingSelection_);
            }
        } else if(screen_==Screen::Hangar) {
            if(n=="launchButtonPane")bind(c,"Launch ship",{MenuOp::Launch},!ghost_ && !inactive_);
            else if(n=="commandButtonPane")bind(c,"Command screen",{MenuOp::CommandView},!inactive_);
            else if(n=="loadoutButtonPane")bind(c,"Ship's loadout",{MenuOp::Navigate,int(Screen::ShipLoadout)},!ghost_);
            else if(n=="investButtonPane")bind(c,"Team investments",{MenuOp::Navigate,int(Screen::Research)},economy_);
            else if(n=="helpButtonPane")bind(c,"Help",{MenuOp::Help});
            else if(n=="teleportButtonPane")bind(c,"Teleport to another station or Board another ship",{MenuOp::Teleport},!authoredLoadouts_);
            else if(n=="teamButtonPane")bind(c,"Information about the teams playing",{MenuOp::Navigate,int(Screen::Pilots)});
            else if(n=="exitButtonPane")bind(c,"Exit the game.",{MenuOp::Navigate,int(Screen::ConfirmLeave)});
        } else {
            bool remote=networked_;
            if(n=="backButtonPane")bind(c,"Quit the current game.",{MenuOp::Navigate,int(Screen::ConfirmLeave)});
            else if(n.starts_with("activeTeamButtonPane")){int side=n.back()-'1';if(side>=0 && side<session_.rules.teams)bind(c,"Enable or disable this empty team.",{MenuOp::TeamActive,side},!counts[side],teamActive(side));}
            else if(n=="awayFromKeyboardButtonPane")bind(c,"Indicate that you are away from the keyboard.",{remote?MenuOp::NetworkReady:MenuOp::Ready},!inProgress || ownSide==noTeam,(!inProgress || ownSide==noTeam) && !(remote?networkReady_:ready_));
            else if(n=="sideReadyButtonPane")bind(c,"Indicate that your team is ready.",{MenuOp::LobbyAction,int(LobbyAction::ForceReady),int(!(forceReady_&(1u<<ownSide)))},!inProgress,inProgress || (forceReady_&(1u<<ownSide)));
            else if(n=="autoAcceptButtonPane")bind(c,"Automatically accept requests to join your team.",{MenuOp::LobbyAction,int(LobbyAction::AutoAccept),int(!(autoAccept_&(1u<<ownSide)))},!inProgress && !(lobbyFlags_&Allegiance::LobbyGameOpenLocked),(autoAccept_&(1u<<ownSide))!=0);
            else if(n=="lockLobbyButtonPane")bind(c,"Lock the game lobby.",{MenuOp::LobbyAction,int(LobbyAction::LockLobby),int(!(lobbyFlags_&1))},(remote?networkHost_:!playing_) && !(lobbyFlags_&Allegiance::LobbyGameOpenLocked),lobbyFlags_&1);
            else if(n=="lockSidesButtonPane")bind(c,"Lock team membership.",{MenuOp::LobbyAction,int(LobbyAction::LockSides),int(!(lobbyFlags_&2))},remote?networkHost_:!playing_,lobbyFlags_&2);
            else if(n=="acceptButtonPane")bind(c,"Accept this player's request to join your team.",{MenuOp::LobbyAction,int(LobbyAction::Accept)},remote && playerActions.accept);
            else if(n=="rejectButtonPane")bind(c,teammate?"Boot this player from your team.":"Reject this player's request.",{MenuOp::LobbyAction,int(LobbyAction::Reject)},remote && playerActions.reject);
            else if(n=="makeLeaderButtonPane")bind(c,"Make this player team leader.",{MenuOp::MakeLeader,rosterSelected_},playerActions.makeLeader);
            else if(n=="startButtonPane")bind(c,"Start the game.",{remote?MenuOp::NetworkStart:MenuOp::Start},!canceled && !inProgress && !starting && !(lobbyFlags_&Allegiance::LobbyAutoStart) && startState.CanStart() && (remote?networkHost_:!playing_));
            else if(n=="randomizeButtonPane")bind(c,"Assign all players to random teams, except for the team leaders.",{MenuOp::LobbyAction,int(LobbyAction::Randomize)},remote && networkHost_);
            else if(n=="joinButtonPane")bind(c,ownRequest>=0?"Cancel your request to join a team.":"Join the selected team.",
                {remote && ownRequest>=0?MenuOp::LobbyAction:remote?MenuOp::NetworkJoinTeam:MenuOp::JoinTeam,
                    remote && ownRequest>=0?int(LobbyAction::CancelRequest):remote?teamPage_:0,remote && ownRequest>=0?0:teamPage_-session_.team},
                (remote || !playing_) && Allegiance::LobbyJoinEnabled(remote && ownRequest>=0,remote && ownSide==noTeam,
                    remote && teamPage_==noTeam,teamPage_==ownSide,true,teamActive(teamPage_)));
            else if(n=="settingsButtonPane")bind(c,"Change game settings.",{remote?MenuOp::NetworkEdit:MenuOp::Navigate,int(Screen::Lobby)},!remote || networkHost_);
            else if(n=="detailsButtonPane")bind(c,"View game settings.",{MenuOp::Navigate,int(Screen::Lobby)});
            else if(n=="teamSettingsButtonPane")bind(c,"Change the team's name and faction.",{MenuOp::TeamSettingsOpen},!session_.rules.lockTeamSettings);
            else if(n=="discordButtonPane")bind(c,"Join your team Discord Voice Channel.",{MenuOp::Web,teamPage_>=0?3+teamPage_:9});
            else if(n=="gameOverButtonPane")bind(c,"View the previous game's statistics.",{MenuOp::Navigate,int(Screen::Results)});
            else if(n=="sendButtonPane")bind(c,"Send your message.",{MenuOp::SendChat},!chat_.empty());
            else if(n.starts_with("playerListHeader")){
                int column=n.back()-'0';c.checkedState=column==rosterHeader_?(rosterReverse_?2:1):0;
                bind(c,c.help.empty()?"Sort the players.":c.help,{MenuOp::RosterSort,column});
            }
            else if(n.starts_with("chatHeader")) {int column=n.back()-'0';if(column>=0 && column<3){c.checkedState=lobbyUnread_[column] && rosterFlash_?2:0;bind(c,"Choose the chat channel.",{MenuOp::LobbyChatChannel,column},true,chatChannel_==int(column==0?ChatChannel::Everyone:column==1?ChatChannel::Team:ChatChannel::Leaders));}}
            else bind(c,"Team control.",{MenuOp::Navigate,int(screen_)},false);
        }
    }
    if(script=="teamscreen") {
        auto wing=std::find_if(view_.artwork.begin(),view_.artwork.end(),[](const auto& c){return c.name=="wingComboPane";});
        if(wing!=view_.artwork.end()) {
            auto face=*wing;int assignment=settings_.defaultWing;
            for(const auto& pilot:networkPilots_)if(pilot.controlled)assignment=pilot.wing;
            if(!networked_)for(const auto& pilot:pilots_)if(pilot.controlled)assignment=pilot.wing;
            if(popups_.empty() || popups_.front().combo!="teamscreen/wingComboPane")comboSelections_["teamscreen/wingComboPane"]=std::max(0,assignment);
            bind(face,face.help.empty()?"Change your wing assignment.":face.help,{MenuOp::LobbyWing},!complete_);
            if(face.widget!=OriginalControl::Widget::ImageCombo)view_.artwork.push_back(widgetText(face,std::string(Game::wingName(assignment))));
        }
        auto edit=std::find_if(view_.artwork.begin(),view_.artwork.end(),[](auto& c){return c.name=="chatEditPane";});
        if(edit!=view_.artwork.end()){auto box=*edit;bind(box,"Enter a chat message.",{MenuOp::ComposeChat});originalEdit(box,chat_,"teamscreen",editingChat_);}
        originalChatHistory(-6,chatChannel_==int(ChatChannel::Team)?CHAT_TEAM:chatChannel_==int(ChatChannel::Leaders)?CHAT_LEADERS:CHAT_EVERYONE);
        if(teamPage_!=noTeam)teamPage_=std::clamp(teamPage_,0,session_.rules.teams-1);
        auto upper=[](std::string text){std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return std::toupper(c);});return text;};
        std::string status,status2,timerType,timer;
        if(inProgress)status="GAME IN PROGRESS";
        else if(starting)status="COUNTDOWN IN PROGRESS";
        else if(complete_ || canceled)status="GAME CANCELED";
        else if(startState.block==Allegiance::LobbyBlock::Unbalanced)status="TEAMS ARE UNBALANCED";
        else if(startState.CanStart())status="READY TO START";
        else{
            int side=startState.side;
            status=startState.multiple?"ONE OR MORE TEAMS":upper(censorChat(side<int(teamScores_.size())?teamScores_[side].name:
                !session_.teamNames[side].empty()?session_.teamNames[side]:"Team "+std::to_string(side+1),settings_.chat.censor));
            status2=std::string(startState.multiple?"ARE ":"IS ")+Allegiance::LobbyBlockText(startState.block);
        }
        if(starting && ownSide==noTeam){timerType="STARTING IN:";timer=Allegiance::LobbyTimer(networkCountdown_,[](int n){return std::to_string(n);});}
        else if(inProgress){timerType=session_.rules.minutes?"TIME LEFT:":"TIME ELAPSED:";timer=Allegiance::LobbyTimer(lobbyTimerSeconds_,[](int n){return std::to_string(n);});}
        for(auto& c:view_.artwork)if(c.kind==OriginalControl::Kind::Text) {
            if(c.name=="textTitle")c.text=sessionName(session_);
            else if(c.name=="textPrompt"){
                if(ownRequest>=0)c.text=upper(ownName)+", YOU ARE REQUESTING TO JOIN A TEAM ";
                else if(ownSide>=0)c.text=upper(ownName)+", YOU ARE ON TEAM "+upper(teamName(ownSide));
                else c.text=upper(ownName)+" -- you need to pick a team";
            }
            else if(c.name=="textStatus")c.text=status;
            else if(c.name=="textStatus2")c.text=status2;
            else if(c.name=="textTimer")c.text=timer;
            else if(c.name=="textTimerType")c.text=timerType;
            else if(c.name=="textServername")c.text=networked_?(networkServerName_.empty()?address_:networkServerName_):"Local session";
            else if(c.name=="textCorename")c.text=original_->assets().coreName(sessionCoreFile(session_));
        }
        auto alliances=Game::effectiveAlliances(session_);
        auto teamColumns=original_->integers("teamscreen","teamColumns");
        if(teamColumns.size()!=5 || !std::is_sorted(teamColumns.begin(),teamColumns.end()))throw std::runtime_error("Invalid original lobby team columns");
        auto viewport=authoredListBounds(view_.artwork,"teamListPane",teamColumns.back());
        // TeamPainter divides the MDL list into six faction seats plus No Team.
        float rowHeight=std::floor(viewport.w/7);
        constexpr const char* tabs[]={"teamyelbmp","teambluebmp","teampurplebmp","teamgreenbmp","teamredbmp","teamwhitebmp","teamgreybmp"};
        constexpr const char* selectedTabs[]={"teamselyelbmp","teamselbluebmp","teamselpurplebmp","teamselgreenbmp","teamselredbmp","teamselwhitebmp","teamselgreybmp"};
        auto paintTeam=[&](int side,int row){
            float y=viewport.y+row*rowHeight;if(rowHeight<=0 || y>=viewport.y+viewport.w)return;
            // TeamPainter leaves inactive faction seats blank before play.
            if(side>=0 && !teamActive(side) && !inProgress)return;
            int slot=side==noTeam?6:side;auto begin=view_.artwork.size();
            image({viewport.x,y},teamPage_==side?selectedTabs[slot]:tabs[slot]);
            if(side>=0){auto group=alliances[side];if(group>=0 && group<3){constexpr const char* edges[]={"green_edgebmp","orange_edgebmp","pink_edgebmp"};constexpr const char* selectedEdges[]={"greensel_edgebmp","orangesel_edgebmp","pinksel_edgebmp"};image({viewport.x,y},teamPage_==side?selectedEdges[group]:edges[group]);}}
            if(side==ownRequest)image({viewport.x-2,y-4},"teamscreenarrowbmp");
            OriginalControl c;c.bounds={viewport.x,y,viewport.z,std::min(rowHeight,viewport.y+viewport.w-y)};c.name=side==noTeam?"noTeam":"team"+std::to_string(side);bind(c,"View the team's roster.",{MenuOp::SelectTeam,0,side-teamPage_});
            float mid=rowHeight/2-8;
            if(side>=0 && !counts[slot])text({viewport.x+teamColumns[1]-15,y+mid,viewport.z-teamColumns[1]+15,14},!teamActive(side)?"destroyed":"available","smallFont");
            else {
                auto name=teamName(side);
                text({viewport.x+teamColumns[0],y+mid+(side>=0?-4:0),teamColumns[1]-teamColumns[0]-4,14},upper(censorChat(name,settings_.chat.censor)),"smallFont");
                auto titleClip=glm::vec4(viewport.x+teamColumns[0],y,teamColumns[1]-teamColumns[0]-4,rowHeight);view_.artwork.back().clip=titleClip;
                if(side>=0){auto civ=teamCivilization(side);
                    text({viewport.x+teamColumns[0]+4,y+rowHeight/2-2,teamColumns[1]-teamColumns[0]-8,14},civ?civ->name:"","smallFont");view_.artwork.back().clip=titleClip;
                }
                text({viewport.x+teamColumns[1],y+mid,viewport.z-teamColumns[1],14},"("+std::to_string(counts[slot])+")","smallFont");
            }
            for(size_t i=begin;i<view_.artwork.size();++i)view_.artwork[i].intersectClip(c.bounds);
        };
        for(int side=0;side<session_.rules.teams;++side)paintTeam(side,side);
        if(networked_)paintTeam(noTeam,session_.rules.teams);
        originalRoster();
        if(!popups_.empty() && popups_.front().group==PopupGroup::Combo && popups_.front().combo=="teamscreen/wingComboPane"){
            if(ownSide<0 || teamPage_!=ownSide || complete_){popups_.clear();pointerAction_.reset();}
            else originalPopup(true);
        }
        if(!popups_.empty() && popups_.front().group==PopupGroup::Player){
            if(std::none_of(roster_.begin(),roster_.end(),[&](const auto& pilot){return pilot.id==contextPilot_;})){popups_.clear();pointerAction_.reset();}
            else {originalPopup(true);}
        }
        if(!popups_.empty() && popups_.front().group==PopupGroup::Alliance){
            if(!allianceMenuAvailable()){popups_.clear();pointerAction_.reset();}
            else originalPopup(true);
        }
    }
}
bool Frontend::stageLoadout(const Loadout& loadout,bool defaults,int targetHull) {
    if(!previewLoadout_)return false;
    auto preview=previewLoadout_(loadout,defaults,targetHull);if(!preview.valid){error("This loadout is unavailable here.");return false;}
    loadoutPreview_=std::move(preview);popups_.clear();pointerAction_.reset();hoverSlot_=-1;hoverPart_.reset();loadoutMenuInfo_=false;rebuild();
    return true;
}
bool Frontend::stageLoadoutPreset(int bank){
    if(screen_!=Screen::ShipLoadout || !loadoutPreview_.valid || bank<0 || bank>=int(Loadouts::count))return false;
    // LoadoutImpl::RefreshLoadout looks up this hull in the selected bank.
    // A fitting for another hull does not replace this hull's defaults.
    int hull=loadoutPreview_.loadout.hull;
    auto cached=cachedLoadout_?cachedLoadout_(bank,hull):std::optional<Loadout>{};
    if(cached && cached->hull==hull)return stageLoadout(*cached,false,hull);
    return stageLoadout({int16_t(hull),{}},true);
}
void Frontend::pointerHover(std::optional<size_t> hit) {
    if(auto captured=view_.capturedButton()){
        feedback_.capturedHover(*captured,hit && *hit==size_t(view_.pressed),int(screen_)+(hasMessageBox()?256:0));return;
    }
    if(original_)feedback_.hover(hit && *hit<view_.items.size()?&view_.items[*hit]:nullptr,int(screen_)+(hasMessageBox()?256:0));
    if(hasMessageBox())return;
    if(!popups_.empty()){
        if(hit && *hit<view_.items.size()){
            const auto& item=view_.items[*hit];if(item.popupLevel>=0)selectPopup(item.popupLevel,item.popupRow,true);
        }return;
    }
    if(original_ && screen_==Screen::Research){
        int id=-1;bool invest=false;if(hit && *hit<view_.items.size()){
            auto action=view_.items[*hit].action;
            if(action.op==MenuOp::ResearchSelect)id=action.value;
            else if(action.op==MenuOp::ResearchActivate){id=researchSelected_;invest=true;}
        }
        // The Invest button's generic enabled-enter sink and OnListHover are
        // separate in PurchasesPane. Its information hover also runs disabled.
        if(id!=researchHover_ || (id>=0 && invest!=researchHoverInvest_)){researchHover_=id;researchHoverInvest_=invest;if(id>=0)sound(mouseoverSound);rebuild();}return;
    }
    if(screen_!=Screen::ShipLoadout || !loadoutPreview_.valid)return;
    int slot=-1;if(hit && *hit<view_.items.size() && view_.items[*hit].action.op==MenuOp::PreviewSlot)slot=view_.items[*hit].action.value;
    if(slot!=hoverSlot_){hoverSlot_=slot;hoverPart_.reset();loadoutMenuInfo_=false;if(slot>=0)sound(mouseoverSound);rebuild();}
}
void Frontend::initializeLoadoutCalibration(){
    if(loadoutCalibration_)return;
    LoadoutCalibration calibration;
    for(size_t i=0;i<calibration.hull.size();++i)calibration.hull[i]=float(original_->number("hullinfo",std::string("maxHull")+Allegiance::LoadoutHullMetrics[i]));
    for(const auto& binding:Allegiance::LoadoutMetricBindings)if(binding.maximum)calibration.part[binding.maximum]=original_->number("partinfo",binding.maximum);
    for(auto name:Allegiance::LoadoutDerivedMaxima)calibration.part[name]=original_->number("partinfo",name);
    loadoutCalibration_=std::move(calibration);
}
void Frontend::originalLoadout() {
    initializeLoadoutCalibration();
    const auto& p=loadoutPreview_;
    std::string civ=p.civilizationIcon;std::transform(civ.begin(),civ.end(),civ.begin(),[](unsigned char c){return std::tolower(c);});
    std::string title=p.name;std::transform(title.begin(),title.end(),title.begin(),[](unsigned char c){return std::toupper(c);});
    UiStrings strings{{"civImage",civ+"loadoutpanebmp"},{"civHullImage",civ+"hullinfobmp"},{"titleString",title},{"hullDescriptionString",p.description},{"partImage","ldefaultbmp"}};
    UiNumbers values{{"equipmentType",-3},{"partID",0},{"hullDefenseClassNumber",p.defense},{"baseHullDefenseClassNumber",p.defense},
        {"money",economy_?p.money:0},{"oldShipCost",economy_?p.oldValue:0},{"newShipCost",economy_?p.value:0},
        {"loadoutShipState",p.equivalent?0:p.value<=0?1:p.affordable?2:3},
        {"equipmentTypeWeapon",1},{"equipmentTypeShield",4},{"equipmentTypeMagazine",2},{"equipmentTypeDispenser",3},{"equipmentTypeCloak",5},{"equipmentTypeAfterburner",7},{"equipmentTypeChaff",0},{"equipmentTypeMine",-4}};
    for(size_t i=0;i<loadoutCalibration_->hull.size();++i)for(bool base:{false,true}){
        auto value=(base?p.baseStats:p.stats)[i];std::string key=std::string(base?"baseHull":"hull")+Allegiance::LoadoutHullMetrics[i];
        auto maximum=loadoutCalibration_->hull[i];values[key]=maximum!=0?(i==6?(value>0?1/value:0):value)/maximum:0;values[key+"Number"]=i==6?value*100:value;
    }
    if(hoverPart_ || (hoverSlot_>=0 && hoverSlot_<int(p.slots.size()))){
        auto& s=hoverPart_?*hoverPart_:p.slots[hoverSlot_];values["partID"]=s.id<0?(loadoutMenuInfo_?0:s.mount<0?-1:-(10+int(s.kind))):s.id;
        strings["partNameString"]=s.name;strings["partDescriptionString"]=s.description;
        if(s.id>=0){strings["partImage"]="l"+s.model+"bmp";Allegiance::PresentLoadoutPart(s.metrics,[&](const char* name){return float(loadoutCalibration_->part.at(name));},[&](const std::string& name,float value){values[name]=value;});}
    }
    view_.original=true;view_.items.clear();view_.text.clear();view_.shipPreview=p.model;
    view_.artwork=original_->layout("loadoutpane","loadoutPane",values,strings);
    OriginalControl preview;preview.kind=OriginalControl::Kind::Slot;preview.name="loadoutGeoPane";
    preview.bounds={original_->point("loadoutpane","geoPosition"),original_->point("loadoutpane","geoSize")};
    preview.intersectClip(view_.artwork.front().bounds);
    view_.artwork.insert(view_.artwork.begin()+1,std::move(preview));
    // WinTrek displays one of these overlapping buttons, depending on trade-in.
    bool confirm=p.value==0;
    std::erase_if(view_.artwork,[&](auto& c){return c.name==(confirm?"buyButtonPane":"confirmButtonPane");});
    auto bind=[&](const OriginalControl& c,std::string label,MenuAction a,bool enabled=true){view_.items.push_back({c.visibleBounds(),std::move(label),{},a,enabled,false,c.name});};
    for(auto& c:view_.artwork)if(c.kind==OriginalControl::Kind::Button){
        if(c.name=="backButtonPane")bind(c,"Return to the hangar.",{MenuOp::Navigate,int(Screen::Hangar)});
        else if(c.name=="defaultButtonPane")bind(c,"Buy default loadout.",{MenuOp::PreviewDefault});
        else if(c.name=="previousButtonPane" || c.name=="nextButtonPane")bind(c,c.name=="nextButtonPane"?"Go to next ship.":"Go to previous ship.",{MenuOp::PreviewHull,-1,c.name=="nextButtonPane"?1:-1},p.hulls.size()>1);
        else if(c.name=="hullListButtonPane")bind(c,"Choose from a list of ships.",{MenuOp::LoadoutPopup,1});
        else if(c.name=="launchButtonPane")bind(c,"Launch with this loadout.",{MenuOp::CommitLoadout,1},(p.equivalent || p.affordable) && !gameplayClosed());
        else if(c.name=="buyButtonPane" || c.name=="confirmButtonPane")bind(c,p.affordable?"Confirm this loadout.":"Request money for this loadout.",{MenuOp::CommitLoadout,0},!p.equivalent && (c.name=="confirmButtonPane" || p.canBuy) && !gameplayClosed());
    }
    auto overlays=original_->loadoutOverlay(p.loadout.hull);view_.artwork.insert(view_.artwork.begin()+2,overlays.begin(),overlays.end());
    for(auto& authored:original_->loadoutSlots(p.loadout.hull)){
        int mount=authored.type==-1?authored.mount-5:authored.type==-2?authored.mount+p.fixedWeapons:authored.mount;
        int type=authored.type==-2?1:authored.type;
        if((authored.type==1 && mount>=p.fixedWeapons) || (authored.type==-2 && mount>=p.weapons))continue;
        auto it=std::find_if(p.slots.begin(),p.slots.end(),[&](auto& s){return s.mount==mount && (mount<0 || int(s.kind)==type);});
        if(it==p.slots.end())continue;int index=int(it-p.slots.begin());auto im=authored.empty;
        if(it->id>=0)im=original_->loadoutImage(authored,it->model);
        if(!im)continue;glm::vec4 slotBounds{authored.position,133,37};
        // Slot::UpdateLayout fixes both its surface and input area at 133x37;
        // alternate card images keep their native size inside that surface.
        OriginalControl c;c.kind=OriginalControl::Kind::Image;c.name="loadoutSlot"+std::to_string(index);c.bounds={authored.position,im->width,im->height};c.image=im;c.intersectClip(slotBounds);view_.artwork.push_back(c);
        c.bounds=slotBounds;bind(c,it->id<0?"Select equipment for this slot.":it->name,{MenuOp::PreviewSlot,index});
        if(it->id>=0 && (it->kind==PartKind::Missile || it->kind==PartKind::Dispenser || it->kind==PartKind::Chaff || it->kind==PartKind::Pack)){
            OriginalControl count;count.kind=OriginalControl::Kind::Text;count.bounds={authored.position+glm::vec2(3,24),127,13};count.font="smallFont";count.text=std::to_string(it->amount);count.justify=2;count.surfaceText();count.intersectClip(slotBounds);view_.artwork.push_back(std::move(count));}
        if(authored.type==-2 && it->id>=0){OriginalControl gunner;gunner.kind=OriginalControl::Kind::Text;gunner.bounds={authored.position+glm::vec2(3,24),127,13};gunner.font="smallFont";gunner.text=it->gunner.empty()?"<open>":it->gunner;gunner.surfaceText();gunner.intersectClip(slotBounds);view_.artwork.push_back(std::move(gunner));}
    }
    if(!popups_.empty()){
        originalPopup(true);
    }
}
FrontResult Frontend::pointerPress(std::optional<size_t> hit,bool doubleClick) {
    contextCapture_=false;
    if(!hasMessageBox() && original_ && screen_==Screen::GameDetails && !insideDetails())return dispatch({MenuOp::DetailsClose});
    if(hasMessageBox() && !hit){pointerAction_.reset();view_.pressed=-1;return {};}
    if(!popups_.empty() && !hit){
        pointerAction_.reset();view_.pressed=-1;
        bool border=std::any_of(popups_.begin(),popups_.end(),[&](const auto& popup){auto b=popup.bounds;return pointerPosition_.x>=b.x && pointerPosition_.x<b.x+b.z && pointerPosition_.y>=b.y && pointerPosition_.y<b.y+b.w;});
        return border?FrontResult{}:closePopup();
    }
    pointerAction_.reset();view_.pressed=-1;
    if(hit && *hit<view_.items.size()){
        const auto& item=view_.items[*hit];
        if(view_.original && !item.enabled && !item.blocked && item.captureOnPress){
            if(!doubleClick){pointerAction_=item.action;pointerControl_=item.control;pointerScreen_=screen_;view_.pressed=int(*hit);}
            return {};
        }
    }
    if(hit && *hit<view_.items.size() && view_.items[*hit].enabled && !view_.items[*hit].blocked) {
        // WinTrek Slot::Button opens the part menu on either button's down event.
        if(view_.items[*hit].action.op==MenuOp::PreviewSlot || view_.items[*hit].action.op==MenuOp::SetupCombo || view_.items[*hit].action.op==MenuOp::GameFilterCombo || view_.items[*hit].action.op==MenuOp::TeamFaction || view_.items[*hit].action.op==MenuOp::TeamName || view_.items[*hit].action.op==MenuOp::LobbyWing)return activate(*hit);
        if(view_.original && view_.items[*hit].popupLevel<0){
            auto action=view_.items[*hit].action;
            auto control=std::find_if(view_.artwork.begin(),view_.artwork.end(),[&](const auto& c){return c.name==view_.items[*hit].control;});
            if(control!=view_.artwork.end() && control->kind==OriginalControl::Kind::Button){
                if(control->pressAction)return activate(*hit);
                // ButtonPane emits a separate double-click event on down;
                // it does not capture or repeat its normal release action.
                if(doubleClick){
                    if(action.op==MenuOp::TrainingSelect)return dispatch({MenuOp::TrainingStart,trainingSelection_});
                    return {};
                }
            }
            // ListPane selects on down and has no mouse capture. Its single
            // and double-click events are distinct, notably for roster toggles.
            switch(action.op){
            case MenuOp::ListClear:return activate(*hit);
            case MenuOp::MappingSelect:activate(*hit);if(doubleClick)editMapping();return {};
            case MenuOp::TeleportSelect:activate(*hit);return doubleClick?dispatch({MenuOp::TeleportActivate}):FrontResult{};
            case MenuOp::ResearchSelect:activate(*hit);return doubleClick?dispatch({MenuOp::ResearchActivate}):FrontResult{};
            case MenuOp::GameSelect:activate(*hit);return doubleClick?dispatch({MenuOp::GameJoin}):FrontResult{};
            case MenuOp::LanSelect:activate(*hit);return doubleClick?dispatch({MenuOp::LanJoin}):FrontResult{};
            case MenuOp::RosterSelect:if(!doubleClick || rosterSelected_!=action.delta)return activate(*hit);return {};
            case MenuOp::ResultsSelect:return activate(*hit);
            case MenuOp::GameCreateServer:case MenuOp::GameCreateCore:return activate(*hit);
            case MenuOp::SelectTeam:
                activate(*hit);
                if(doubleClick)for(size_t i=0;i<view_.items.size();++i)if(view_.items[i].control=="joinButtonPane")return activate(i);
                return {};
            default:break;
            }
        }
        const auto& item=view_.items[*hit];pointerAction_=item.action;pointerControl_=item.control;pointerScreen_=screen_;view_.pressed=int(*hit);
        if(item.action.op==MenuOp::ScrollThumb){scrollGrab_=pointerPosition_.y-scrollList(item.action.value).thumb().y;return {};}
        if(item.action.op==MenuOp::ScrollLine || item.action.op==MenuOp::ScrollPage){scrollRepeat_=item.action.op==MenuOp::ScrollLine?.5:.25;return activate(*hit);}
        if(item.action.op==MenuOp::MappingSelect)return activate(*hit);
        if(item.action.op==MenuOp::TeleportSelect){activate(*hit);return doubleClick?dispatch({MenuOp::TeleportActivate}):FrontResult{};}
        if(item.action.op==MenuOp::ResearchSelect){activate(*hit);return doubleClick?dispatch({MenuOp::ResearchActivate}):FrontResult{};}
    }
    return {};
}
FrontResult Frontend::pointerCapturedPress(int button) {
    if(!pointerAction_ || (pointerAction_->op!=MenuOp::ScrollLine && pointerAction_->op!=MenuOp::ScrollPage))return {};
    if(button==SDL_BUTTON_RIGHT){scrollRepeat_=std::numeric_limits<double>::infinity();return {};}
    if(button!=SDL_BUTTON_LEFT)return {};
    scrollRepeat_=pointerAction_->op==MenuOp::ScrollLine?.5:.25;
    return dispatch(*pointerAction_);
}
FrontResult Frontend::pointerRelease(std::optional<size_t> hit,bool doubleClick,int button) {
    if(button!=SDL_BUTTON_LEFT && button!=SDL_BUTTON_RIGHT)return {};
    if(button==SDL_BUTTON_RIGHT && pointerAction_ && (pointerAction_->op==MenuOp::ScrollThumb || pointerAction_->op==MenuOp::SetupMapDrag))return {};
    contextCapture_=false;
    auto pressed=pointerAction_;pointerAction_.reset();view_.pressed=-1;
    if(pressed && (pressed->op==MenuOp::ScrollLine || pressed->op==MenuOp::ScrollPage || pressed->op==MenuOp::ScrollThumb)){rebuild();return {};}
    // Original MenuItemPane executes on release: dragging from a combo or
    // another row does not require a matching press on the released row.
    if(button==SDL_BUTTON_LEFT && !popups_.empty() && hit && *hit<view_.items.size() && view_.items[*hit].popupLevel>=0)return activate(*hit);
    if(!pressed || !hit || screen_!=pointerScreen_ || *hit>=view_.items.size())return {};
    const auto& item=view_.items[*hit];
    if(item.control!=pointerControl_ || item.action.op!=pressed->op || item.action.value!=pressed->value || item.action.delta!=pressed->delta)return {};
    auto selected=item.action;
    if(selected.op==MenuOp::ScrollLine || selected.op==MenuOp::ScrollPage || selected.op==MenuOp::ScrollThumb)return {};
    if(selected.op==MenuOp::TeleportSelect || selected.op==MenuOp::ResearchSelect)return {};
    // Ordinary buttons have separate left/right sinks. The authored toggle is
    // internal to the widget and occurs on either release; right has no host
    // callback or click sound on these screens.
    if(button==SDL_BUTTON_RIGHT){if(item.enabled && !item.blocked)toggleButton(*hit);return {};}
    auto result=activate(*hit);
    if(!view_.original && doubleClick && selected.op==MenuOp::MappingSelect){editMapping();return {};}
    if(!view_.original && doubleClick && (selected.op==MenuOp::TrainingSelect || selected.op==MenuOp::ScenarioSelect || selected.op==MenuOp::GameSelect)) {
        auto launch=selected.op==MenuOp::TrainingSelect?MenuOp::TrainingStart:selected.op==MenuOp::GameSelect?MenuOp::GameJoin:MenuOp::ScenarioOpen;
        for(size_t i=0;i<view_.items.size();++i)if(view_.items[i].action.op==launch)return activate(i);
    }
    return result;
}
FrontResult Frontend::activate(size_t index) {
    if(index>=view_.items.size() || !view_.items[index].enabled || view_.items[index].blocked)return {};
    const auto& item=view_.items[index];auto a=item.action;int depth=item.popupLevel,row=item.popupRow;
    if(original_ && depth<0){toggleButton(index);feedback_.click(item);}
    if(original_ && item.captureOnPress && a.op==MenuOp::LobbyAction &&
       (LobbyAction(a.value)==LobbyAction::ForceReady || LobbyAction(a.value)==LobbyAction::AutoAccept ||
        LobbyAction(a.value)==LobbyAction::LockLobby || LobbyAction(a.value)==LobbyAction::LockSides))a.delta=item.selected;
    view_.focus=int(index);view_.notice.clear();
    if(depth>=0)return activatePopup(depth,row);
    return dispatch(a,index);
}
void Frontend::toggleButton(size_t index){
    if(index>=view_.items.size())return;
    auto& item=view_.items[index];if(!item.toggleOnRelease || !item.enabled || item.blocked || item.control.empty())return;
    auto found=buttonChecks_.try_emplace(item.control,ButtonCheck{screen_,item.selected,item.selected}).first;
    if(found->second.scope!=screen_)found->second={screen_,item.selected,item.selected};
    found->second.checked=item.selected=!item.selected;
    for(auto* layer:{&menuPopupView_,&messageView_,&detailsView_})for(auto& copy:layer->items)
        if(copy.control==item.control && !copy.blocked)copy.selected=item.selected;
}
void Frontend::restoreButtonChecks(MenuView& view){
    if(buttonChecks_.empty())return;
    for(auto& item:view.items)if(item.toggleOnRelease && !item.blocked){
        auto found=buttonChecks_.find(item.control);if(found==buttonChecks_.end() || found->second.scope!=screen_)continue;
        // A reported SetChecked retires local widget state. An unrelated
        // retained-layout refresh must not discard a silent right toggle.
        if(item.selected!=found->second.base)buttonChecks_.erase(found);
        else item.selected=found->second.checked;
    }
}
FrontResult Frontend::lobbyVisit(bool settings,bool cancelRequest) {
    if(!original_ || !networked_)return {};
    bool withdraw=settings && networkLobby_ && commander_ && !networkHost_ && networkReady_;
    bool cancel=cancelRequest && std::any_of(networkPilots_.begin(),networkPilots_.end(),[](const auto& p){return p.controlled && p.requested>=0;});
    if(withdraw){networkReady_=false;forceReady_&=~(1u<<session_.team);readySides_&=~(1u<<session_.team);}
    return withdraw || cancel?FrontResult{FrontEffect::LobbyVisit,int(withdraw),int(cancel)}:FrontResult{};
}
FrontResult Frontend::dispatch(MenuAction a,size_t index){
    auto buttonChecked=[&](bool fallback){return original_ && index<view_.items.size() && view_.items[index].captureOnPress?view_.items[index].selected:fallback;};
    switch(a.op) {
    case MenuOp::TeamActive:{
        if(a.value<0 || a.value>=session_.rules.teams)return {};
        bool active=buttonChecked(!teamActive(a.value));
        if(networked_)return {FrontEffect::LobbyAction,int(active),int(LobbyAction::TeamActive),-1,a.value};
        if(playing_ || session_.team==a.value || std::any_of(session_.localPilots.begin(),session_.localPilots.end(),[&](const auto& p){return p.team==a.value;}))return {};
        if(active)session_.inactiveTeams&=~(1u<<a.value);else session_.inactiveTeams|=1u<<a.value;
        session_.requireTeamPlayers=(session_.inactiveTeams&((1u<<session_.rules.teams)-1))==0;
        auto current=session_.alliances;
        Allegiance::UpdateAlliances(current.data(),SideID(session_.rules.teams),SideID(a.value),NA,[&](SideID side){return teamActive(side);},session_.alliances.data());
        if(current!=session_.alliances){auto rules=Allegiance::LobbyAllianceRules(std::any_of(session_.alliances.begin(),session_.alliances.end(),[](char group){return group!=NA;}),allianceRipDisabled_);session_.rules.allowDefections=rules.defections;session_.rules.alliedSight=rules.sight;session_.rules.alliedRipcord=rules.ripcord;session_.rules.maxImbalance=rules.imbalance;}
        break;}
    case MenuOp::AllianceCancel:popups_.clear();pointerAction_.reset();break;
    case MenuOp::ChangeAlliance:{
        if(!allianceMenuAvailable() || a.value!=teamPage_ || (a.delta!=NA && (a.delta<0 || a.delta>=session_.rules.teams || a.delta==a.value ||
           !teamActive(a.delta))))return {};
        if(networked_)return {FrontEffect::ChangeAlliance,a.value,int(allianceRipDisabled_),-1,a.delta};
        auto current=Game::effectiveAlliances(session_);
        Allegiance::UpdateAlliances(current.data(),SideID(session_.rules.teams),SideID(a.value),SideID(a.delta),
            [&](SideID side){return teamActive(side);},session_.alliances.data());
        auto rules=Allegiance::LobbyAllianceRules(std::any_of(session_.alliances.begin(),session_.alliances.end(),[](char group){return group>=0;}),allianceRipDisabled_);
        session_.rules.allowDefections=rules.defections;session_.rules.alliedSight=rules.sight;session_.rules.alliedRipcord=rules.ripcord;session_.rules.maxImbalance=rules.imbalance;
        ready_=false;break;}
    case MenuOp::DetailsClose:{
        if(screen_!=Screen::GameDetails)return {};
        bool resume=detailsResume_;screen_=detailsReturn_;view_=detailsBackdrop_;pointerAction_.reset();view_.pressed=-1;rebuild();
        return resume?FrontResult{FrontEffect::Resume}:FrontResult{};
    }
    case MenuOp::TeamSettingsOpen:{
        if(!original_ || session_.rules.lockTeamSettings || teamPage_<0 || teamPage_>=session_.rules.teams ||
           (networked_ && (!commander_ || teamPage_!=session_.team)))return {};
        teamSettingsSide_=teamPage_;teamNameDraft_=teamName(teamSettingsSide_);
        auto civ=teamCivilization(teamSettingsSide_);teamCivDraft_=civ?civ->id:-1;
        auto visit=lobbyVisit(true,false);
        teamSettingsOpen_=teamNameFocus_=true;popups_.clear();pointerAction_.reset();rebuild();
        return visit;}
    case MenuOp::TeamName:teamNameFocus_=true;break;
    case MenuOp::TeamFaction:{
        if(!teamSettingsOpen_)return {};
        auto found=std::find_if(view_.items.begin(),view_.items.end(),[](const auto& i){return i.action.op==MenuOp::TeamFaction;});
        if(found==view_.items.end())return {};
        std::vector<PopupRow> rows;for(const auto& civ:civilizations_)if(civ.id<300)rows.push_back({civ.name,0,{MenuOp::TeamFactionChoice,civ.id}});
        comboPopup("teamscreen/civComboPane",std::move(rows));break;}
    case MenuOp::TeamFactionChoice:
        if(teamSettingsOpen_ && std::any_of(civilizations_.begin(),civilizations_.end(),[&](const auto& c){return c.id==a.value && c.id<300;}))teamCivDraft_=a.value;
        teamNameFocus_=true;
        popups_.clear();break;
    case MenuOp::TeamSettingsCancel:teamSettingsOpen_=false;popups_.clear();pointerAction_.reset();break;
    case MenuOp::TeamSettingsAccept:{
        if(!teamSettingsOpen_)return {};
        auto civ=std::find_if(civilizations_.begin(),civilizations_.end(),[&](const auto& c){return c.id==teamCivDraft_ && c.id<300;});
        if(civ==civilizations_.end())return {};
        teamSettingsOpen_=false;popups_.clear();pointerAction_.reset();
        if(!networked_){session_.teamNames[teamSettingsSide_]=teamNameDraft_;session_.teamCivilizations[teamSettingsSide_]=int(civ-civilizations_.begin());}
        rebuild();return networked_?FrontResult{FrontEffect::TeamSettings,teamCivDraft_}:FrontResult{};}
    case MenuOp::MessageDismiss:{
        if(!hasMessageBox() || messageBoxPending_)return {};
        if(messagePurpose_==MessagePurpose::TrainingSuggestion)for(const auto& item:view_.items)
            if(item.action.op==MenuOp::TrainingDontAskAgain && !item.blocked)messageDontAskAgain_=item.selected;
        bool resume=messageBoxResume_;auto accept=std::exchange(messageBoxAccept_,{});
        auto purpose=std::exchange(messagePurpose_,MessagePurpose::Ordinary);bool dontAsk=std::exchange(messageDontAskAgain_,false);
        messageBox_.clear();messageBoxResume_=false;pointerAction_.reset();view_.pressed=-1;view_.focus=messageFocus_;rebuild();
        if(purpose==MessagePurpose::TrainingWelcome){open(Screen::Training);return {};}
        if(purpose==MessagePurpose::TrainingSuggestion){
            if(dontAsk)settings_.showStartupTrainingSuggestion=false;
            if(a.delta)trainingDialog(false);
            return dontAsk?FrontResult{FrontEffect::ApplyOptions}:FrontResult{};
        }
        if(accept && a.delta){accept->resumeAfter=resume;return *accept;}
        return {resume?FrontEffect::Resume:FrontEffect::None};}
    case MenuOp::TrainingDontAskAgain:
        if(messagePurpose_==MessagePurpose::TrainingSuggestion)messageDontAskAgain_=buttonChecked(!messageDontAskAgain_);
        break;
    case MenuOp::LogonName:logonNameFocus_=true;break;
    case MenuOp::LogonCancel:logonOpen_=logonNameFocus_=false;break;
    case MenuOp::LogonAccept:
        if(!logonOpen_ || !Allegiance::LogonNameAcceptable(logonNameDraft_))return {};
        settings_.pilot=session_.pilot=logonNameDraft_;logonOpen_=logonNameFocus_=false;rebuild();return {FrontEffect::Connect};
    case MenuOp::SecondaryClose:return closeSecondary();
    case MenuOp::PopupItem:return activatePopup(a.value,a.delta);
    case MenuOp::PopupOption:return {};
    case MenuOp::SetupCombo:{
        if(!setupEditable_ || a.value<0 || a.value>=int(setupCombos_.size()))return {};
        std::vector<PopupRow> rows;const auto& combo=setupCombos_[a.value];
        for(size_t i=0;i<combo.choices.size();++i)rows.push_back({combo.choices[i].label,0,{MenuOp::SetupChoice,a.value,int(i)}, {},combo.field!=SetupField::GameType || i<Allegiance::GameTypes.size()});
        comboPopup(std::string("newgamescreen/")+combo.control,std::move(rows));break;
    }
    case MenuOp::SetupChoice:{
        if(!setupEditable_ || a.value<0 || a.value>=int(setupCombos_.size()))return {};
        int choice=a.delta;
        if(choice<0)choice=cycle(setupSelection(a.value)+(choice==-2?1:-1),int(setupCombos_[a.value].choices.size()));
        setupApply(a.value,choice);popups_.clear();pointerAction_.reset();break;
    }
    case MenuOp::SetupCancel:session_=setupSnapshot_;ready_=setupReadySnapshot_;open(setupReturn_);return {};
    case MenuOp::SetupAccept:
        if(!setupEditable_)return {};
        // ReadControls takes the displayed entries, including nearest choices
        // for values supplied by a custom server; recognition is not Apply.
        readSetupChecks();
        for(size_t field=1;field<setupCombos_.size();++field)setupApply(int(field),setupSelection(int(field)),false);
        session_.rules.experimental=setupSelection(0)==5; // Original ReadControls uses the selected Experimental preset.
        if(auto invalid=Game::missionError(session_,customMaps_,true);!invalid.empty()){error(std::move(invalid));return {};}
        if(auto invalid=Allegiance::MissionTeamReductionError(setupSnapshot_.rules.teams,session_.rules.teams,[&](int side){return (networked_ || playing_) && side<int(teamScores_.size())?teamScores_[side].active:!(setupSnapshot_.inactiveTeams&(1u<<side));})){error(invalid);return {};}
        if(!session_.customMap.empty() && !setupMap_.error.empty()){error(setupMap_.error);return {};}
        if(networked_)return {FrontEffect::NetworkConfigure};
        open(Screen::LobbyPilots);return {};
    case MenuOp::SetupMapDrag:return {};
    case MenuOp::MappingSelect:mappingList_.select(a.value);mappingEditing_=false;mappingPending_={};break;
    case MenuOp::ListClear:
        switch(MenuOp(a.value)){
        case MenuOp::MappingSelect:mappingList_.selection=-1;mappingEditing_=false;mappingPending_={};break;
        case MenuOp::GameSelect:gamesList_.selection=-1;gameSelected_.clear();break;
        case MenuOp::GameCreateServer:createServerSelected_=false;createAuthority_.clear();createServerList_.selection=-1;break;
        case MenuOp::GameCreateCore:createCoreSelected_=false;createCoreList_.selection=-1;break;
        case MenuOp::TeleportSelect:teleportList_.selection=-1;teleportSelected_=-1;teleportShipSelected_=false;break;
        case MenuOp::RosterSelect:clearLobbySelection();rosterFocus_=true;break;
        case MenuOp::ResultsSelect:if(a.delta)resultsTeamSelected_=-1;else {resultsPlayerSelected_=-1;resultsList_.selection=-1;}break;
        default:return {};
        }break;
    case MenuOp::MappingAccept:return closeMappings(true);
    case MenuOp::MappingCancel:return closeMappings(false);
    case MenuOp::MappingRestore:copyInputMappings(draft_,defaults_);mappingEditing_=false;mappingPending_={};break;
    case MenuOp::ScrollLine:{auto& list=scrollList(a.value);list.position(list.offset+a.delta*(a.value==-7?8:list.row));if(a.value==-4 || a.value==-6)chatScrolled(a.value);closePopupChildren(a.value);break;}
    case MenuOp::ScrollPage:{auto& list=scrollList(a.value);list.position(list.offset+a.delta*list.page);if(a.value==-4 || a.value==-6)chatScrolled(a.value);closePopupChildren(a.value);break;}
    case MenuOp::ScrollThumb:return {};
    case MenuOp::PreviewHull:{
        if(!loadoutPreview_.valid)return {};int id=a.value;
        if(id<0){auto& hulls=loadoutPreview_.hulls;auto it=std::find_if(hulls.begin(),hulls.end(),[&](auto& h){return h.id==loadoutPreview_.loadout.hull;});if(it==hulls.end())return {};id=hulls[cycle(int(it-hulls.begin())+a.delta,int(hulls.size()))].id;}
        if(id==loadoutPreview_.loadout.hull){popups_.clear();pointerAction_.reset();rebuild();return {};}
        if(rememberLoadout_)rememberLoadout_(loadoutPreview_.loadout);
        if(auto cached=cachedLoadout_?cachedLoadout_(0,id):std::optional<Loadout>{};cached && cached->hull==id)stageLoadout(*cached,false,id);
        else stageLoadout({int16_t(id),{}},true);
        sound(mountSound);return {};
    }
    case MenuOp::PreviewDefault:stageLoadout({loadoutPreview_.loadout.hull,{}},true);return {FrontEffect::RememberLoadout,0};
    case MenuOp::LoadoutPopup:
        if(a.value!=1 || !loadoutPreview_.valid)return {};
        popupPosition_=pointerPosition_;popups_={{PopupGroup::LoadoutHulls}};
        hoverSlot_=-1;hoverPart_.reset();loadoutMenuInfo_=false;break;
    case MenuOp::PreviewSlot:{
        if(!previewParts_ || a.value<0 || a.value>=int(loadoutPreview_.slots.size()))return {};
        const auto& slot=loadoutPreview_.slots[a.value];partKind_=slot.kind;partMount_=slot.mount;hoverSlot_=a.value;
        loadoutPrimaryParts_.clear();loadoutOtherParts_.clear();
        for(auto& p:previewParts_(slot.kind,slot.mount,loadoutPreview_.loadout.hull))(p.otherShip?loadoutOtherParts_:loadoutPrimaryParts_).push_back(std::move(p));
        if(slot.mount>=0 && std::none_of(loadoutPrimaryParts_.begin(),loadoutPrimaryParts_.end(),[](auto& p){return p.id>=0;}))return {};
        popupPosition_=pointerPosition_;popups_={{PopupGroup::LoadoutParts}};hoverPart_.reset();loadoutMenuInfo_=false;break;
    }
    case MenuOp::PreviewPart:{
        auto loadout=loadoutPreview_.loadout;
        // Part IDs can share mount zero across equipment types. The selected
        // slot identifies the old record; cargo mounts are unique across types.
        auto old=std::find_if(loadoutPreview_.slots.begin(),loadoutPreview_.slots.end(),[&](auto& s){return s.mount==partMount_ && (s.mount<0 || s.kind==partKind_);});
        if(old!=loadoutPreview_.slots.end() && old->id>=0)std::erase_if(loadout.parts,[&](auto& p){return p.mount==partMount_ && p.id==old->id;});
        if(a.value>=0)loadout.parts.push_back({int16_t(a.value),int8_t(partMount_)});stageLoadout(loadout);sound(a.value<0?unmountSound:mountSound);return {};
    }
    case MenuOp::CommitLoadout:if(!loadoutPreview_.affordable)return {FrontEffect::RequestMoney,std::max(0,loadoutPreview_.value-loadoutPreview_.money-loadoutPreview_.oldValue),loadoutPreview_.loadout.hull};return {FrontEffect::CommitLoadout,a.value};
    case MenuOp::Teleport:{
        FrontResult result{original_ && screen_==Screen::Teleport?FrontEffect::Resume:FrontEffect::Teleport};
        if(original_)result.dismissPanes=ofTeam|ofExpandedTeam|ofGameState;
        return result;
    }
    case MenuOp::TeleportTo:return {FrontEffect::TeleportTo,a.value};
    case MenuOp::TeleportPage:teleportPage_=std::clamp(teleportPage_+a.delta,0,std::max(0,(int(teleports_.size())-1)/8));break;
    case MenuOp::TeleportSelect:
        teleportSelected_=a.value;teleportShipSelected_=a.delta!=0;
        for(size_t i=0;i<teleports_.size();++i)if(teleports_[i].id==a.value && teleports_[i].boarding==teleportShipSelected_){teleportList_.select(int(i),false);break;}
        break;
    case MenuOp::TeleportHeader:
        // TeleportPane::OnButtonBar is empty. Its ButtonBarPane is neither
        // a tab bar nor a collection of toggling buttons, despite checked art.
        return {};
    case MenuOp::TeleportActivate:
        if(!docked_ || complete_)return {};
        for(const auto& choice:teleports_)if(choice.id==teleportSelected_ && choice.boarding==teleportShipSelected_){
            if(choice.boarding){sound(personalJumpSound);return {FrontEffect::Board,choice.id};}
            if(choice.current)return {passenger_?FrontEffect::Disembark:FrontEffect::Resume};
            return {FrontEffect::TeleportTo,choice.id};
        }
        return {};
    case MenuOp::Crew:return {FrontEffect::Crew};
    case MenuOp::Board:return {FrontEffect::Board,a.value};
    case MenuOp::Disembark:return {FrontEffect::Disembark};
    case MenuOp::Promote:return {FrontEffect::Promote,a.value};
    case MenuOp::CrewPage:crewPage_=std::max(0,crewPage_+a.delta);break;
    case MenuOp::Navigate:{
        auto destination=Screen(a.value);
        if(original_ && screen_==Screen::Home && destination==Screen::Training){trainingDialog(false);return {};}
        if(original_ && playing_ && !networkLobby_ && destination==Screen::Pilots){FrontResult result{FrontEffect::PlayerPane};result.dismissPanes=ofGameState;return result;}
        if(original_ && destination==Screen::Research && screen_==Screen::Research){FrontResult result{FrontEffect::Resume};result.dismissPanes=ofTeam|ofExpandedTeam|ofGameState;return result;}
        FrontResult visit;
        if(teamScreen(screen_) && (destination==Screen::Lobby || destination==Screen::Results))visit=lobbyVisit(destination==Screen::Lobby,true);
        if(original_ && playing_ && !networkLobby_ && (destination==Screen::Research || (destination==Screen::ShipLoadout && screen_!=destination)))visit.dismissPanes=ofTeam|ofExpandedTeam|ofGameState;
        open(networked_ && networkLobby_ && destination==Screen::Hangar?Screen::RemoteLobby:destination);return visit;}
    case MenuOp::GameSelect:if(a.value>=0 && a.value<int(gamesRows_.size())){gamesList_.select(a.value);gameSelected_=games_[gamesRows_[a.value]].address;}break;
    case MenuOp::GameJoin:{
        if(gameFilterOpen_ || gameCreateOpen_ || findPlayerOpen_ || gamesList_.selection<0)return {};
        auto selected=std::find_if(games_.begin(),games_.end(),[&](const auto& g){return g.address==gameSelected_;});
        if(selected==games_.end() || !(selected->info.flags&Net::ServerJoinable))return {};
        address_=selected->address;joinInstance_=selected->info.instance;editingAddress_=false;return {FrontEffect::Connect};
    }
    case MenuOp::GameSort:{if(a.value<0 || a.value>7)return {};bool reverse=gameSorts_.back().first==a.value?!gameSorts_.back().second:false;std::erase_if(gameSorts_,[&](auto s){return s.first==a.value;});gameSorts_.push_back({a.value,reverse});sortGames();break;}
    case MenuOp::GameFilter:gameFilterDraft_=gameFilters_;gameNameDraft_=gameNameFilter_;gameFilterOpen_=gameFilterEditing_=true;break;
    case MenuOp::GameFilterName:case MenuOp::GameCreateName:gameFilterEditing_=true;break;
    case MenuOp::GameFilterCombo:gameFilterCombo(a.value);break;
    case MenuOp::GameFilterChoice:if(a.value>=0 && a.value<11 && a.delta>=0 && a.delta<3)gameFilterDraft_[a.value]=a.delta;break;
    case MenuOp::GameFilterAccept:gameFilters_=gameFilterDraft_;gameNameFilter_=gameNameDraft_;gameFilterOpen_=gameFilterEditing_=false;sortGames();break;
    case MenuOp::GameFilterCancel:case MenuOp::GameCreateCancel:gameFilterOpen_=gameCreateOpen_=gameFilterEditing_=false;break;
    case MenuOp::GameCreate:gameCreateOpen_=gameFilterEditing_=true;gameNameDraft_=Allegiance::DefaultCreatedGameName(settings_.pilot);createAuthority_.clear();createCoreFile_="static_core";createServerSelected_=createCoreSelected_=!original_;createServerList_={};createCoreList_={};createCoreCatalog();break;
    case MenuOp::GameCreateServer:
        if(a.value<0 || a.value>int(authorities_.size()))return {};
        createAuthority_=a.value?authorities_[size_t(a.value-1)].address:"";createServerSelected_=true;
        if(createCoreSelected_ && !(a.value?authorities_[size_t(a.value-1)].supports(createCore()):Allegiance::FindCore(localCores_,createCore())!=nullptr))createCoreSelected_=false;
        break;
    case MenuOp::GameCreateCore:
        if(a.value<0 || a.value>=int(createCores_.size()))return {};createCoreFile_=createCores_[size_t(a.value)];createCoreSelected_=true;
        if(createServerSelected_){auto server=std::find_if(authorities_.begin(),authorities_.end(),[&](const auto& s){return s.address==createAuthority_;});
            if(!(createAuthority_.empty()?Allegiance::FindCore(localCores_,createCore())!=nullptr:server!=authorities_.end() && server->supports(createCore()))){createAuthority_.clear();createServerSelected_=false;}}
        break;
    case MenuOp::GameCreateAccept:if(!canCreateGame())return {};if(auto invalid=Allegiance::CreatedGameNameError(gameNameDraft_);!invalid.empty()){error(std::string(invalid));return {};}if(!createAuthority_.empty()){gameCreateOpen_=gameFilterEditing_=false;rebuild();return {FrontEffect::CreateGame};}
        session_=SessionOptions{};session_.scenario=Scenario::Conquest;session_.coreFile=createCore();updateCoreCivilizations();session_.pilot=settings_.pilot;session_.name=gameNameDraft_;teamPage_=session_.team;gameCreateOpen_=gameFilterEditing_=false;ready_=false;open(Screen::Lobby);return {};
    case MenuOp::FindPlayer:findPlayerOpen_=true;break;
    case MenuOp::FindPlayerName:break;
    case MenuOp::FindPlayerCancel:findPlayerOpen_=false;break;
    case MenuOp::FindPlayerAccept:
        if(!findPlayerOpen_)return {};
        findPlayerOpen_=false;findPlayerPending_=true;messagePurpose_=MessagePurpose::Ordinary;
        messageFocus_=view_.focus;messageBoxAccept_.reset();messageBox_="Asking server to find player....";
        messageBoxResume_=false;messageBoxPending_=true;messageBoxAction_=0;pointerAction_.reset();rebuild();
        return {FrontEffect::FindPlayer};
    case MenuOp::ScenarioSelect:scenarioSelection_=a.value;rebuild();return {};
    case MenuOp::ScenarioOpen:{for(size_t i=0;i<view_.items.size();++i)if(view_.items[i].action.op==MenuOp::Scenario && view_.items[i].action.value==scenarioSelection_)return activate(i);session_.scenario=Scenario(scenarioSelection_);session_.customMap.clear();auto& s=scenarios[scenarioSelection_];session_.map=s.map;session_.enemies=s.enemies;session_.allies=s.allies;session_.killLimit=s.killLimit;ready_=false;open(Screen::Lobby);return {};}
    case MenuOp::CommandView:{FrontResult result{FrontEffect::CommandView};if(original_)result.dismissPanes=ofTeam|ofExpandedTeam|ofGameState;return result;}
    case MenuOp::Help:return {FrontEffect::Help};
    case MenuOp::Web:return {FrontEffect::Web,a.value};
    case MenuOp::TrainingSelect:trainingSelection_=a.value;rebuild();return {};
    case MenuOp::TrainingStart:{if(trainingSelection_<1)return {};settings_.showStartupTrainingSuggestion=false;if(a.value==trainingNext_)trainingNext_=trainingNext_>=6?8:trainingNext_+1;trainingSelection_=trainingNext_;session_=SessionOptions{};session_.trainingMission=a.value;session_.pilot=settings_.pilot;return {FrontEffect::Start};}
    case MenuOp::Training:settings_.showStartupTrainingSuggestion=false;session_=SessionOptions{};session_.trainingMission=a.value;session_.pilot=settings_.pilot;return {FrontEffect::Start};
    case MenuOp::Scenario:{session_.trainingMission=0;session_.scenario=Scenario(a.value);session_.coreFile.clear();updateCoreCivilizations();session_.customMap.clear();if(session_.scenario!=Scenario::Conquest)session_.localPilots.clear();auto& s=scenarios[a.value];session_.map=s.map;session_.enemies=s.enemies;session_.allies=s.allies;session_.killLimit=s.killLimit;ready_=false;open(Screen::Lobby);return {};}
    case MenuOp::Map:session_.customMap.clear();session_.map=cycle(session_.map+a.delta,int(session_.scenario==Scenario::Conquest?conquestMapNames.size():mapFiles.size()));ready_=false;break;
    case MenuOp::JoinTeam:if(original_ && !teamActive(cycle(session_.team+a.delta,session_.rules.teams)))return {};retainTeamCivilizations();session_.team=cycle(session_.team+a.delta,session_.rules.teams);ready_=bool(original_);break;
    case MenuOp::Civilization:{int& civ=session_.teamCivilizations[session_.team];if(civ<0)civ=session_.civilization;civ=cycle(civ+a.delta,std::max(1,int(civilizations_.size())));ready_=false;break;}
    case MenuOp::SelectTeam:
        teamPage_=original_ && networked_ && teamPage_+a.delta==noTeam?noTeam:cycle(teamPage_+a.delta,session_.rules.teams);
        if(original_ && networked_ && teamPage_>=0 && teamPage_<int(teamScores_.size()) && !teamScores_[teamPage_].active &&
           std::none_of(networkPilots_.begin(),networkPilots_.end(),[&](const auto& p){return p.team==teamPage_;}))teamPage_=noTeam;
        rosterList_={};rosterSelected_=-100000;lobbyRecipient_=-1;break;
    case MenuOp::RosterSort:{
        if(a.value<0 || a.value>5)return {};
        auto mode=Allegiance::LobbyRosterColumn(rosterSort_,a.value);
        rosterReverse_=mode==rosterSort_?!rosterReverse_:false;rosterSort_=mode;rosterHeader_=a.value;break;
    }
    case MenuOp::RosterSelect:{
        if(a.value<0 || a.value>=int(roster_.size()))return {};auto& pilot=roster_[a.value];rosterFocus_=true;
        if(rosterSelected_==pilot.id){rosterSelected_=-100000;rosterList_.selection=-1;lobbyRecipient_=-1;}
        else {rosterSelected_=pilot.id;rosterList_.select(a.value);lobbyRecipient_=pilot.controlled?-1:pilot.id;}break;
    }
    case MenuOp::TeamCivilization:{int count=std::max(1,int(civilizations_.size()));int& civ=session_.teamCivilizations[a.value];if(civ<0)civ=(session_.civilization+a.value-session_.team+count)%count;civ=cycle(civ+a.delta,count);ready_=false;break;}
    case MenuOp::TeamAlliance:session_.alliances[a.value]=char(cycle(session_.alliances[a.value]+1+a.delta,4)-1);ready_=false;break;
    case MenuOp::ResearchPage:researchPage_=std::max(0,researchPage_+a.delta);break;
    case MenuOp::HullPage:hullPage_=std::max(0,hullPage_+a.delta);break;
    case MenuOp::SaveStats:return {FrontEffect::SaveStats};
    case MenuOp::ResultsBack:if(!networked_ || missionStage_==STAGE_OVER || complete_ && !networkLobby_)return {FrontEffect::Leave};open(Screen::RemoteLobby);return {};
    case MenuOp::ResultsSort:{auto& col=a.delta?resultsTeamSort_:resultsPlayerSort_;auto& reverse=a.delta?resultsTeamReverse_:resultsPlayerReverse_;int next=a.value?a.value:col;if(next==col)reverse=!reverse;else {col=next;reverse=false;}resultsDirty_=true;break;}
    case MenuOp::ResultsSelect:if(a.delta)resultsTeamSelected_=a.value;else {resultsPlayerSelected_=a.value;auto it=std::find_if(resultsPlayers_.begin(),resultsPlayers_.end(),[&](size_t i){return endgame_.players[i].id==a.value;});if(it!=resultsPlayers_.end())resultsList_.select(int(it-resultsPlayers_.begin()),false);}if(!original_)editingChat_=false;break;
    case MenuOp::ResearchTab:
        if(!researchGroupEnabled(a.value))return {};
        researchTab_=a.value;researchSelected_=-1;researchHover_=-1;researchList_={};researchSelectFirst_=true;break;
    case MenuOp::ResearchSelect:researchSelection(a.value,false);break;
    case MenuOp::ResearchFilter:
        (a.value==0?researchShowAll_:researchShowComplete_)=buttonChecked(!(a.value==0?researchShowAll_:researchShowComplete_));
        // PurchasesPane::OnShowAll rebuilds the current tab and selects its
        // first item, falling back to Purchases when a technology path closes.
        if(researchTab_>=2 && !researchGroupEnabled(researchTab_))researchTab_=0;
        researchSelected_=-1;researchHover_=-1;researchList_={};researchSelectFirst_=true;break;
    case MenuOp::ResearchActivate:
        for(const auto& row:researchChoices_)if(row.id==researchSelected_ && researchVisible(row) && row.available && !gameplayClosed()){
            int amount=Allegiance::InvestmentAmount(row.price,row.invested,money_,a.value!=0);
            if(amount>0)return {FrontEffect::Invest,row.id,amount};
        }
        return {};
    case MenuOp::Invest:return {FrontEffect::Invest,a.value};
    case MenuOp::MutePilot:return {FrontEffect::MutePilot,a.value,a.delta};
    case MenuOp::MakeLeader:if(original_){clearLobbySelection();rebuild();}return {FrontEffect::MakeLeader,a.value};
    case MenuOp::ChatPreference:
        if(a.value==0)draft_.chat.filterAll=!draft_.chat.filterAll;
        else if(a.value==1)draft_.chat.filterQuick=!draft_.chat.filterQuick;
        else if(a.value==2)draft_.chat.filterUnknown=!draft_.chat.filterUnknown;
        else if(a.value==3)draft_.chat.censor=!draft_.chat.censor;
        break;
    case MenuOp::ChatChannel:chatSector_=-1;if(screen_==Screen::QuickChat)quickChannel_=cycle(quickChannel_+a.delta,int(ChatChannel::Leaders)+1);else chatChannel_=cycle(chatChannel_+a.delta,int(ChatChannel::Leaders)+1);break;
    case MenuOp::ComposeChat:editingChat_=true;rosterFocus_=false;break;
    case MenuOp::LobbyChatChannel:chatChannel_=int(a.value==0?ChatChannel::Everyone:a.value==1?ChatChannel::Team:ChatChannel::Leaders);lobbyChat_={};if(a.value>=0 && a.value<3)lobbyUnread_[a.value]=false;lobbyRecipient_=-1;rosterSelected_=-100000;rosterList_.selection=-1;editingChat_=true;rosterFocus_=false;break;
    case MenuOp::SendChat:return sendChatAction();
    case MenuOp::Unit:unitId_=a.value;open(Screen::UnitOrders);return {FrontEffect::SelectUnit,unitId_};
    case MenuOp::TakeMeTo:return {FrontEffect::TakeMeTo,a.value};
    case MenuOp::Order:return {a.delta==-3?FrontEffect::Order:FrontEffect::OrderTargets,a.value,a.delta};
    case MenuOp::DroneStandby:return {FrontEffect::DroneStandby,a.value,a.delta};
    case MenuOp::OrderTarget:{auto& t=orderTargets_.at(a.value);return {FrontEffect::Order,unitId_,orderCommand_,t.type,t.id};}
    case MenuOp::TargetPage:targetPage_=std::max(0,targetPage_+a.delta);break;
    case MenuOp::Hull:session_.hull=cycle(session_.hull+a.delta,int(hullNames.size()));ready_=false;break;
    case MenuOp::Enemies:case MenuOp::Allies: {
        int& count=a.op==MenuOp::Enemies?session_.enemies:session_.allies;
        if(a.delta==0){editingWingmen_=a.op==MenuOp::Enemies?0:1;wingmenDraft_.clear();}
        else count=int(std::clamp<int64_t>(int64_t(count)+a.delta,0,shipIdentityCapacity-1));
        ready_=false;break;
    }
    case MenuOp::Difficulty:session_.difficulty=cycle(session_.difficulty+a.delta,3);ready_=false;break;
    case MenuOp::KillLimit:session_.killLimit=std::clamp(session_.killLimit+a.delta*5,0,50);ready_=false;break;
    case MenuOp::Ready:ready_=original_ && index<view_.items.size() && view_.items[index].captureOnPress?!view_.items[index].selected:!ready_;break;
    case MenuOp::PilotPage:pilotPage_=std::max(0,pilotPage_+a.delta);break;
    case MenuOp::Account:accountId_=a.value;open(Screen::Account);return {};
    case MenuOp::MoneyAmount:moneyAmount_=std::clamp(moneyAmount_+a.delta*1000,1000,100000000);break;
    case MenuOp::Donate:return {FrontEffect::Donate,a.value,moneyAmount_};
    case MenuOp::AutoDonate:return {FrontEffect::AutoDonate,a.value};
    case MenuOp::RequestMoney:return {FrontEffect::RequestMoney,moneyAmount_};
    case MenuOp::MoneyResponse:return {FrontEffect::MoneyResponse,a.value};
    case MenuOp::HandOver:return {FrontEffect::HandOver,a.value};
    case MenuOp::LobbyWing:{
        if(complete_)return {};
        std::vector<PopupRow> rows;for(int wing=0;wing<10;++wing)rows.push_back({std::string(Game::wingName(wing)),0,{MenuOp::LobbyWingChoice,wing}});
        comboPopup("teamscreen/wingComboPane",std::move(rows));break;
    }
    case MenuOp::LobbyWingChoice:{
        if(complete_ || a.value<0 || a.value>=10)return {};
        for(const auto& pilot:networkPilots_)if(pilot.controlled && pilot.team>=0 && pilot.team==teamPage_)return {FrontEffect::Wing,pilot.id,a.value};
        if(!networked_){for(const auto& pilot:pilots_)if(pilot.controlled)return {FrontEffect::Wing,pilot.id,a.value};return {FrontEffect::Wing,-1,a.value};}
        break;
    }
    case MenuOp::Wing:{auto i=std::find_if(pilots_.begin(),pilots_.end(),[&](auto& pilot){return pilot.id==a.value;});
        if(i!=pilots_.end() && !i->allied && (i->controlled || commander_))return {FrontEffect::Wing,a.value,(i->wing+a.delta+10)%10};return {};}

    case MenuOp::Start:session_.pilot=settings_.pilot;if(!lobbyReady())return {};if(auto invalid=Game::missionError(session_,customMaps_);!invalid.empty()){error(std::move(invalid));return {};}if(!session_.rules.startCountdown)return {FrontEffect::Start};if(prepareSession_){try{briefing_=prepareSession_(session_);preparedSession_=true;}catch(const std::exception& e){error(e.what());return {};}}countdownReturn_=screen_;open(Screen::Countdown);countdown_=session_.rules.startCountdown;countdownSounds_.Reset();sound(countdownStartingSound);rebuild();countdownClock_=uiTime();return {};
    case MenuOp::CancelCountdown:open(countdownReturn_);return {};
    case MenuOp::AddLobbyPilot:if(initialShipCount(session_)<shipIdentityCapacity){session_.localPilots.push_back({"Pilot "+std::to_string(nextLobbyPilot_++),session_.team,false});lobbyPilotPage_=(session_.localPilots.size()-1)/5;ready_=false;}break;
    case MenuOp::RemoveLobbyPilot:if(a.value>=0 && a.value<int(session_.localPilots.size())){session_.localPilots.erase(session_.localPilots.begin()+a.value);ready_=false;}break;
    case MenuOp::LobbyPilotTeam:if(a.value>=0 && a.value<int(session_.localPilots.size())){auto& pilot=session_.localPilots[a.value];pilot.team=cycle(pilot.team+a.delta,session_.rules.teams);pilot.ready=false;ready_=false;}break;
    case MenuOp::LobbyPilotReady:if(a.value>=0 && a.value<int(session_.localPilots.size()))session_.localPilots[a.value].ready=!session_.localPilots[a.value].ready;break;
    case MenuOp::LobbyPilotName:if(a.value>=0 && a.value<int(session_.localPilots.size())){editingLobbyPilot_=a.value;lobbyPilotDraft_=session_.localPilots[a.value].name;}break;
    case MenuOp::LobbyPilotPage:lobbyPilotPage_+=a.delta;break;
    case MenuOp::ControlPilot:return {FrontEffect::ControlPilot,a.value};
    case MenuOp::RemovePilot:return {FrontEffect::RemovePilot,a.value};
    case MenuOp::Resume:return {FrontEffect::Resume};
    case MenuOp::ReturnToBase:return {FrontEffect::ReturnToBase};
    case MenuOp::Launch:return {FrontEffect::Launch};
    case MenuOp::Leave:return {FrontEffect::Leave};
    case MenuOp::Quit:return {FrontEffect::Quit};
    case MenuOp::Intro:return {FrontEffect::Intro};
    case MenuOp::HangarHull:return {FrontEffect::ChangeHull,a.value};
    case MenuOp::EquipmentSlot:return {FrontEffect::PartOptions,a.value};
    case MenuOp::EquipPart:return {FrontEffect::EquipPart,a.value};
    case MenuOp::PartPage:partPage_=std::clamp(partPage_+a.delta,0,std::max(0,(int(choices_.size())-1)/9));break;
    case MenuOp::EquipmentPage:equipmentPage_+=a.delta;break;
    case MenuOp::Defaults:draft_=defaults_;break;
    case MenuOp::NetworkEdit:{auto visit=lobbyVisit(true,true);open(Screen::Lobby);return visit;}
    case MenuOp::NetworkConfigure:return {FrontEffect::NetworkConfigure};
    case MenuOp::NetworkJoinTeam:
        // UpdateButtons can enable JOIN for a No Team pilot viewing an
        // occupied eliminated side; OnButtonJoin still checks SideActive.
        if(original_ && a.value>=0 && !teamActive(a.value))return networkReady_?FrontResult{}:FrontResult{FrontEffect::NetworkReady,1};
        return {FrontEffect::NetworkJoinTeam,original_?a.value:cycle(session_.team+a.delta,session_.rules.teams)};
    case MenuOp::NetworkNewRound:return {FrontEffect::NetworkNewRound};
    case MenuOp::LobbyAction:
        if(networked_){int target=rosterSelected_;if(original_ && LobbyAction(a.value)==LobbyAction::Accept){clearLobbySelection();rebuild();}return {FrontEffect::LobbyAction,a.delta,a.value,-1,target};}
        if(LobbyAction(a.value)==LobbyAction::ForceReady){if(a.delta)forceReady_|=1u<<session_.team;else forceReady_&=~(1u<<session_.team);if(original_)ready_=true;}
        else if(LobbyAction(a.value)==LobbyAction::AutoAccept){if(a.delta)autoAccept_|=1u<<session_.team;else autoAccept_&=~(1u<<session_.team);}
        else if(LobbyAction(a.value)==LobbyAction::LockLobby)lobbyFlags_=(lobbyFlags_&~1u)|(a.delta?1u:0u);
        else if(LobbyAction(a.value)==LobbyAction::LockSides)lobbyFlags_=(lobbyFlags_&~2u)|(a.delta?2u:0u);
        break;
    case MenuOp::NetworkReady:return {FrontEffect::NetworkReady,int(!buttonChecked(networkReady_))};
    case MenuOp::NetworkStart:return {FrontEffect::NetworkStart};
    case MenuOp::NetworkAddress:editingAddress_=!editingAddress_;break;
    case MenuOp::LanSelect:if(a.value>=0 && a.value<int(lanRows_.size())){lanList_.select(a.value);lanSelected_=games_[lanRows_[a.value]].address;}else{lanList_.selection=-1;lanSelected_.clear();}break;
    case MenuOp::LanJoin:{
        if(playing_ || lanList_.selection<0 || lanList_.selection>=int(lanRows_.size()))return {};
        auto& selected=games_[lanRows_[lanList_.selection]];if(selected.address!=lanSelected_ || !(selected.info.flags&Net::ServerJoinable))return {};
        address_=selected.address;joinInstance_=selected.info.instance;editingAddress_=false;
        if(original_){open(Screen::Home);logonNameDraft_.clear();Allegiance::AppendEditInput(logonNameDraft_,settings_.pilot,23);logonNameFocus_=logonNameDraft_.empty();logonOpen_=true;rebuild();return {};}
        return {FrontEffect::Connect};}
    case MenuOp::NetworkTeam:networkSide_=cycle(networkSide_+a.delta,6);break;
    case MenuOp::Connect:joinInstance_={};editingAddress_=false;return {FrontEffect::Connect};
    case MenuOp::Pilot:editingPilot_=true;draft_.pilot=settings_.pilot;break;
    case MenuOp::Bind:binding_=a.value;break;
    case MenuOp::Unbind:draft_.keys[a.value]=SDL_SCANCODE_UNKNOWN;draft_.keyCommands[a.value]=TK_NoKeyMapping;std::erase_if(draft_.keyAliases,[&](auto& key){return key.binding==Binding(a.value);});break;
    case MenuOp::BindingPage:bindingPage_=std::clamp(bindingPage_+a.delta,0,int((bindingNames.size()-1)/(original_?13:20)));break;
    case MenuOp::CargoSelect:return {FrontEffect::CargoSelect,a.value};
    case MenuOp::CargoNext:return {FrontEffect::CargoNext};
    case MenuOp::CargoDrop:return {FrontEffect::CargoDrop};
    case MenuOp::CargoMounts:return {FrontEffect::CargoMounts};
    case MenuOp::CargoSwap:return {FrontEffect::CargoSwap,a.value};
    case MenuOp::RulesPage:rulesPage_=std::clamp(a.value,0,5);break;
    case MenuOp::MissionSetting: {
        auto& r=session_.rules;ready_=false;
        if(original_ && screen_==Screen::Lobby && session_.scenario==Scenario::Conquest){
            readSetupChecks();if(a.value==13 || a.value==19)recognizeSetupPreset();break;
        }
        switch(a.value) {
        case 0:r.conquest=std::clamp(r.conquest+a.delta*10,0,100);break;
        case 1:r.territory=r.territory==100?(a.delta>0?10:90):r.territory+a.delta*10<=0 || r.territory+a.delta*10>=100?100:r.territory+a.delta*10;break;
        case 2:r.flags=std::clamp(r.flags+a.delta,0,100);break;
        case 3:r.artifacts=std::clamp(r.artifacts+a.delta,0,100);break;
        case 4:r.teamKills=std::clamp(r.teamKills+a.delta*5,0,1000);break;
        case 5:r.prosperity=std::clamp(r.prosperity+a.delta*.5f,0.f,9.f);break;
        case 6:r.minutes=std::clamp(r.minutes+a.delta*5,0,1440);break;
        case 7:r.startingMoney=std::clamp(r.startingMoney+a.delta*.5f,0.f,9.f);break;
        case 8:r.helium=std::clamp(r.helium+a.delta*.1f,0.f,9.f);break;
        case 9:r.miners=std::clamp(r.miners+a.delta,0,r.maxDrones);break;
        case 10:r.friendlyFire=!r.friendlyFire;break;
        case 11:r.lives=r.lives==32767?0:r.lives+a.delta<0?32767:r.lives+a.delta>10?32767:r.lives+a.delta;break;
        case 12:r.maxDrones=std::clamp(r.maxDrones+a.delta,0,10);r.miners=std::min(r.miners,r.maxDrones);break;
        case 13:r.developments=!r.developments;break;
        case 14:r.teams=std::clamp(r.teams+a.delta,2,6);session_.team=std::min(session_.team,r.teams-1);for(auto& pilot:session_.localPilots)if(pilot.team>=r.teams){pilot.team=r.teams-1;pilot.ready=false;}break;
        case 15:r.mapSize=!r.mapSize;break;
        case 16:r.randomEncounters=std::clamp(r.randomEncounters+a.delta,0,10);break;
        case 17:r.showMap=!r.showMap;break;case 18:r.showHome=!r.showHome;break;
        case 19:r.invulnerableStations=!r.invulnerableStations;break;case 20:r.ejectPods=!r.ejectPods;break;
        case 21:r.homeAsteroids=std::clamp(r.homeAsteroids+a.delta,0,40);break;case 22:r.neutralAsteroids=std::clamp(r.neutralAsteroids+a.delta,0,40);break;
        case 23:r.homeMineable=std::clamp(r.homeMineable+a.delta,0,9);break;case 24:r.neutralMineable=std::clamp(r.neutralMineable+a.delta,0,9);break;
        case 25:r.homeSpecial=std::clamp(r.homeSpecial+a.delta,0,9);break;case 26:r.neutralSpecial=std::clamp(r.neutralSpecial+a.delta,0,9);break;
        case 27:r.resourceAmounts=!r.resourceAmounts;break;
        case 28:r.homeTreasures=std::clamp(r.homeTreasures+a.delta,0,20);break;case 29:r.neutralTreasures=std::clamp(r.neutralTreasures+a.delta,0,20);break;
        case 30:r.homeTreasureRate=std::clamp(r.homeTreasureRate+a.delta*.1f,0.f,.6f);break;case 31:r.neutralTreasureRate=std::clamp(r.neutralTreasureRate+a.delta*.1f,0.f,.6f);break;
        case 32:r.powerUps=!r.powerUps;break;case 33:r.alliedSight=!r.alliedSight;break;case 34:r.alliedRipcord=!r.alliedRipcord;break;
        case 35:r.shipyard=!r.shipyard;break;case 36:r.tactical=!r.tactical;break;case 37:r.supremacy=!r.supremacy;break;case 38:r.expansion=!r.expansion;break;case 41:r.allowDefections=!r.allowDefections;break;case 42:r.allowJoiners=!r.allowJoiners;break;
        case 39:r.randomWormholes=!r.randomWormholes;break;
        case 40:r.startCountdown=std::clamp(r.startCountdown+a.delta*5,0,60);break;
        }break;
    }
    case MenuOp::ControllerSetting: {
        auto& c=draft_.controller;
        switch(a.value) {
        case 0:c.enabled=!c.enabled;break;
        case 1:{int current=-1;for(size_t i=0;i<controllerDevices_.size();++i)if(controllerDevices_[i].guid==c.device)current=int(i);
            int next=cycle(current+1+a.delta,int(controllerDevices_.size())+1)-1;c.device=next<0?"":controllerDevices_[next].guid;break;}
        case 2:c.linear=!c.linear;break;
        case 3:c.deadZone=std::clamp(c.deadZone+a.delta*.025f,0.f,.95f);break;
        case 4:c.hat=cycle(c.hat+1+a.delta,std::max(1,controllerHats_)+1)-1;break;
        case 5:c.triggers=!c.triggers;break;
        case 6:c.feedback=!c.feedback;break;
        }break;
    }
    case MenuOp::MapMode:draft_.controller.mapped=!draft_.controller.mapped;break;
    case MenuOp::MapAxis:mapAxis_=a.value;open(Screen::ControllerMapAxis);return {};
    case MenuOp::MapAxisDevice:{auto& route=draft_.controller.mapAxes[mapAxis_];route.device=cycle(route.device+1+a.delta,17)-1;break;}
    case MenuOp::MapAxisInput:{auto& route=draft_.controller.mapAxes[mapAxis_];int axes=8,hats=1;int ordinal=route.device;
        if(ordinal>=0 && ordinal<16){auto& guid=draft_.controller.mapDevices[ordinal];if(!guid.empty()){ordinal=-1;for(size_t i=0;i<controllerDevices_.size();++i)if(controllerDevices_[i].guid==guid)ordinal=int(i);}if(ordinal>=0 && ordinal<int(controllerDevices_.size())){axes=std::min(64,controllerDevices_[ordinal].axes);hats=std::min(16,controllerDevices_[ordinal].hats);}}
        int current=route.index<0?-1:route.index+(route.hat?axes:0);int input=cycle(current+1+a.delta,axes+hats+1)-1;
        route.hat=input>=axes && input>=0;route.index=route.hat?input-axes:input;break;}
    case MenuOp::MapAxisInvert:{auto& route=draft_.controller.mapAxes[mapAxis_];route.invert=!route.invert;break;}
    case MenuOp::MapDevice:{auto& guid=draft_.controller.mapDevices[a.value];int current=-1;for(size_t i=0;i<controllerDevices_.size();++i)if(controllerDevices_[i].guid==guid)current=int(i);int next=cycle(current+1+a.delta,int(controllerDevices_.size())+1)-1;guid=next<0?"":controllerDevices_[next].guid;break;}
    case MenuOp::MapDevicePage:mapDevicePage_=std::clamp(mapDevicePage_+a.delta,0,1);break;
    case MenuOp::MapButtonPage:mapButtonPage_+=a.delta;break;
    case MenuOp::MapButton:mapButton_=a.value;open(Screen::ControllerMapButton);return {};
    case MenuOp::MapButtonAdd:if(draft_.controller.mapButtons.size()<512){mapButton_=int(draft_.controller.mapButtons.size());draft_.controller.mapButtons.push_back({});open(Screen::ControllerMapButton);}return {};
    case MenuOp::MapButtonDelete:draft_.controller.mapButtons.erase(draft_.controller.mapButtons.begin()+mapButton_);open(Screen::ControllerMapButtons);return {};
    case MenuOp::MapButtonDevice:{auto& route=draft_.controller.mapButtons[mapButton_];route.device=cycle(route.device+a.delta,16);break;}
    case MenuOp::MapButtonInput:{auto& route=draft_.controller.mapButtons[mapButton_];route.button=cycle(route.button+4+a.delta,68)-4;break;}
    case MenuOp::MapButtonAction:controllerActionPage_=int(draft_.controller.mapButtons[mapButton_].binding)/20;open(Screen::ControllerMapAction);return {};
    case MenuOp::MapButtonAssign:draft_.controller.mapButtons[mapButton_].binding=Binding(a.value);open(Screen::ControllerMapButton);return {};
    case MenuOp::ControllerAxis:draft_.controller.axes[a.value]=cycle(draft_.controller.axes[a.value]+2+a.delta,std::max(8,controllerAxes_)+2)-2;break;
    case MenuOp::ControllerInvert:draft_.controller.invert[a.value]=!draft_.controller.invert[a.value];break;
    case MenuOp::ControllerButton:controllerButton_=a.value;controllerActionPage_=0;open(Screen::ControllerAction);return {};
    case MenuOp::ControllerAssign:draft_.controller.buttons[controllerButton_]=a.value;open(Screen::ControllerButtons);return {};
    case MenuOp::MouseButton:mouseButton_=a.value;mouseActionPage_=0;open(Screen::MouseAction);return {};
    case MenuOp::MouseAssign:draft_.mouseButtons[mouseButton_]=a.value;std::erase_if(draft_.mouseAliases,[&](auto& alias){return alias.button==mouseButton_;});open(Screen::MouseButtons);return {};
    case MenuOp::MousePage:mouseActionPage_=std::clamp(mouseActionPage_+a.delta,0,int((bindingNames.size()-1)/20));break;
    case MenuOp::ControllerPage:if(a.value==0)controllerPage_=std::clamp(controllerPage_+a.delta,0,3);else controllerActionPage_=std::clamp(controllerActionPage_+a.delta,0,int((bindingNames.size()-1)/20));break;
    case MenuOp::AcceptOrder:return {FrontEffect::AcceptOrder};
    case MenuOp::ExecuteOrder:return {FrontEffect::ExecuteOrder};
    case MenuOp::RejectOrder:return {FrontEffect::RejectOrder};
    case MenuOp::ClearOrder:return {FrontEffect::ClearOrder};
    case MenuOp::TargetOrder:return {FrontEffect::TargetOrder};
    case MenuOp::Reload:return {FrontEffect::Reload};
    case MenuOp::Loadouts:return {FrontEffect::Loadouts};
    case MenuOp::LoadoutSlot:loadoutSlot_=a.value;break;
    case MenuOp::SaveLoadout:return {FrontEffect::SaveLoadout,a.value};
    case MenuOp::UseLoadout:return {FrontEffect::UseLoadout,a.value};
    case MenuOp::DeleteLoadout:return {FrontEffect::DeleteLoadout,a.value};
    case MenuOp::QuickChatItem: {
        if(!quickChat_ || quickPath_.empty())return {};
        const auto& entries=quickChat_->menus.at(quickPath_.back()).items;
        if(a.value<0 || a.value>=int(entries.size()))return {};auto& entry=entries[a.value];
        if(entry.menu>=0){quickPath_.push_back(entry.menu);quickPage_=0;view_.focus=0;}
        else return {FrontEffect::QuickChat,entry.command,quickChannel_,targetType_,targetId_};
        break;
    }
    case MenuOp::QuickChatBack:if(quickPath_.size()>1){quickPath_.pop_back();quickPage_=0;view_.focus=0;}else return {FrontEffect::Resume};break;
    case MenuOp::QuickChatPage:quickPage_+=a.delta;view_.focus=0;break;
    case MenuOp::Apply:draft_.normalize();settings_=draft_;open(settingsReturn_);return {FrontEffect::ApplySettings};
    case MenuOp::VisualSetting:if(a.value>=0 && size_t(a.value)<visualFields.size())draft_.visual.*visualFields[a.value]=!(draft_.visual.*visualFields[a.value]);break;
    case MenuOp::Setting:
        switch(a.value) {
        case 0:draft_.fullscreen=!draft_.fullscreen;break;case 1:draft_.uiScale+=a.delta*.125f;break;
        case 2:draft_.fieldOfView+=a.delta*5;break;case 3:draft_.volume+=a.delta*.05f;break;
        case 4:draft_.sensitivity+=a.delta*.1f;break;case 5:draft_.invertY=!draft_.invertY;break;
        case 6:draft_.particles=cycle(draft_.particles+a.delta,5);break;case 7:draft_.showTiming=!draft_.showTiming;break;case 8:draft_.throttleDelay=cycle(draft_.throttleDelay+a.delta,5);break;case 9:draft_.startupCredits=!draft_.startupCredits;break;case 10:draft_.startupIntro=!draft_.startupIntro;break;
        case 11:draft_.effectsGain+=a.delta;break;case 12:draft_.voiceGain+=a.delta;break;case 13:draft_.controller.feedbackGain+=a.delta*.05f;break;
        case 14:draft_.cockpitRadar=cycle(draft_.cockpitRadar+a.delta,4);break;case 15:draft_.commandRadar=cycle(draft_.commandRadar+a.delta,4);break;case 16:draft_.showGrid=!draft_.showGrid;break;case 17:draft_.virtualJoystick=!draft_.virtualJoystick;break;case 18:draft_.joystickIndicator=!draft_.joystickIndicator;break;case 19:draft_.mouseAcceleration=cycle(draft_.mouseAcceleration+a.delta,3);break;
        case 20:draft_.showCommandGrid=!draft_.showCommandGrid;break;
        }draft_.normalize();break;
    }
    rebuild();return {};
}
void Frontend::assignKey(SDL_Scancode scan,uint8_t modifiers){
    auto conflict=[&](uint8_t other){return other==modifiers || (modifiers==0 && other==KeyAny) || (modifiers==KeyAny && other==0);};
    for(size_t i=0;i<draft_.keys.size();++i)if(int(i)!=binding_ && draft_.keys[i]==scan && conflict(draft_.modifiers[i])){
        draft_.keys[i]=draft_.keys[binding_];draft_.modifiers[i]=draft_.modifiers[binding_];draft_.keyCommands[i]=TK_NoKeyMapping;break;
    }
    std::erase_if(draft_.keyAliases,[&](auto& a){return int(a.binding)==binding_ || (a.scan==scan && conflict(a.modifiers));});
    draft_.keys[binding_]=scan;draft_.modifiers[binding_]=modifiers;draft_.keyCommands[binding_]=TK_NoKeyMapping;binding_=-1;rebuild();
}
void Frontend::keyReleased(SDL_Keycode key,SDL_Scancode scan){
    if(hasMessageBox())return;
    if(key==popupTextKey_)popupTextKey_=0;
    if(original_ && screen_==Screen::Controls)return;
    if(binding_>=0 && modifierKey(scan)) {
        uint8_t mod=scan==SDL_SCANCODE_LCTRL || scan==SDL_SCANCODE_RCTRL?KeyControl:
            scan==SDL_SCANCODE_LSHIFT || scan==SDL_SCANCODE_RSHIFT?KeyShift:KeyAlt;
        assignKey(scan,continuousBinding(Binding(binding_))?KeyAny:mod);
    }
}
FrontResult Frontend::key(SDL_Keycode key,SDL_Scancode scan,bool shift,uint8_t modifiers) {
    if(hasMessageBox())return !messageBoxPending_ && (key==SDLK_RETURN || key==SDLK_ESCAPE)?dispatch({MenuOp::MessageDismiss,0,key==SDLK_ESCAPE && messageBoxAccept_?0:1}):FrontResult{};
    if(logonOpen_){
        if(key==SDLK_ESCAPE)return dispatch({MenuOp::LogonCancel});
        if(key==SDLK_RETURN)return dispatch({logonNameDraft_.empty()?MenuOp::LogonCancel:MenuOp::LogonAccept});
        if(key==SDLK_TAB)logonNameFocus_=true;
        if(logonNameFocus_ && key==SDLK_BACKSPACE && !logonNameDraft_.empty()){
            size_t n=logonNameDraft_.size()-1;while(n && (uint8_t(logonNameDraft_[n])&0xc0)==0x80)--n;logonNameDraft_.resize(n);editTyped("logon/nameEditPane");
        }
        rebuild();return {};
    }
    if(missionBriefOpen_){if(!networked_ && key==SDLK_ESCAPE)return dispatch({MenuOp::CancelCountdown});return {};}
    if(original_ && screen_==Screen::GameDetails)return key==SDLK_ESCAPE || key==SDLK_RETURN?dispatch({MenuOp::DetailsClose}):FrontResult{};
    if(key!=popupTextKey_)popupTextKey_=0;
    if(!popups_.empty() && (popups_.front().group!=PopupGroup::Commands ||
        (screen_!=Screen::Chat && screen_!=Screen::QuickChat && editingWingmen_<0 && editingLobbyPilot_<0 && !editingPilot_)))return popupKey(key);
    {FrontResult result;if(gamesKey(key,result))return result;}
    if(original_ && screen_==Screen::Online){
        if(key==SDLK_ESCAPE){open(Screen::Home);return {};}
        int move=key==SDLK_UP?-1:key==SDLK_DOWN?1:key==SDLK_PAGEUP?-lanList_.pageRows():key==SDLK_PAGEDOWN?lanList_.pageRows():0;
        if(move && !lanRows_.empty() && lanList_.move(move)){lanSelected_=games_[lanRows_[lanList_.selection]].address;rebuild();}
        return {};
    }
    if(teamSettingsActive()){
        if(key==SDLK_ESCAPE)return dispatch({MenuOp::TeamSettingsCancel});
        if(key==SDLK_RETURN)return dispatch({MenuOp::TeamSettingsAccept});
        if(teamNameFocus_ && key==SDLK_BACKSPACE && !teamNameDraft_.empty()){
            size_t n=teamNameDraft_.size()-1;while(n && (uint8_t(teamNameDraft_[n])&0xc0)==0x80)--n;teamNameDraft_.resize(n);rebuild();
        }
        return {};
    }
    if(original_ && screen_==Screen::Lobby && session_.scenario==Scenario::Conquest && key==SDLK_ESCAPE)return dispatch({MenuOp::SetupCancel});
    if(original_ && screen_==Screen::Teleport){
        if(key==SDLK_ESCAPE)return {FrontEffect::Resume};
        int delta=key==SDLK_DOWN?1:key==SDLK_UP?-1:key==SDLK_PAGEDOWN?teleportList_.pageRows():key==SDLK_PAGEUP?-teleportList_.pageRows():0;
        if(delta && !teleports_.empty() && teleportList_.move(delta)){
            auto& choice=teleports_[teleportList_.selection];teleportSelected_=choice.id;teleportShipSelected_=choice.boarding;rebuild();
        }
        return {};
    }
    if(original_ && screen_==Screen::Research){
        if(key==SDLK_ESCAPE)return {FrontEffect::Resume};
        return {}; // PurchasesPane's ListPaneOld has no keyboard navigation handler.
    }
    if(original_ && screen_==Screen::Controls){
        if(mappingEditing_){
            if(key==SDLK_ESCAPE){finishMapping();return {};}
            auto pending=mappingPending_;
            if(pending.type!=MappingPending::None){
                if(key==SDLK_RETURN){
                    if(pending.type==MappingPending::Key)addMappingKey(SDL_Scancode(pending.input),pending.modifiers,true);
                    else if(pending.type==MappingPending::Button)addMappingButton(pending.device,pending.input,true);
                    else addMappingAxis(pending.device,pending.input,pending.hat,true);
                }return {};
            }
            if(mappingList_.selection<int(std::size(Allegiance::CommandDescriptions)) && !modifierKey(scan) && scan!=SDL_SCANCODE_UNKNOWN)addMappingKey(scan,modifiers);
            return {};
        }
        if(key==SDLK_ESCAPE)return closeMappings(false);
        if(key==SDLK_SPACE)return closeMappings(true);
        if(key==SDLK_RETURN){editMapping();return {};}
        int selected=mappingList_.selection;
        if(key==SDLK_DELETE && selected>=0){
            int count=int(std::size(Allegiance::CommandDescriptions));
            if(selected>=count)draft_.controller.mapAxes[selected-count]={};
            else if(auto command=Allegiance::CommandDescriptions[selected].m_tk;command!=TK_NoKeyMapping)deleteInputMappings(draft_,command);
            rebuild();return {};
        }
        int delta=key==SDLK_DOWN?1:key==SDLK_UP?-1:key==SDLK_PAGEDOWN?mappingList_.pageRows():key==SDLK_PAGEUP?-mappingList_.pageRows():0;
        if(delta){mappingList_.move(delta);rebuild();}return {};
    }
    if(original_ && rosterFocus_ && (screen_==Screen::Pilots || screen_==Screen::RemoteLobby || screen_==Screen::LobbyPilots)){
        int delta=key==SDLK_DOWN?1:key==SDLK_UP?-1:key==SDLK_PAGEDOWN?rosterList_.pageRows():key==SDLK_PAGEUP?-rosterList_.pageRows():0;
        if(delta){if(!roster_.empty() && rosterList_.move(delta)){auto& p=roster_[rosterList_.selection];rosterSelected_=p.id;lobbyRecipient_=p.controlled?-1:p.id;rebuild();}return {};}
    }
    if(original_ && screen_==Screen::Credits && key==SDLK_ESCAPE)return closeSecondary();
    if(screen_==Screen::ShipLoadout && loadoutPreview_.valid && !passenger_){
        if(key>=SDLK_0 && key<=SDLK_9){
            if(modifiers&KeyControl)return {FrontEffect::SaveLoadout,int(key-SDLK_0)};
            if(!(modifiers&(KeyShift|KeyAlt)))return {FrontEffect::UseLoadout,int(key-SDLK_0)};
        }
    }
    if(screen_==Screen::QuickChat && quickChat_ && !quickPath_.empty()) {
        if(key==SDLK_ESCAPE) {
            if(quickPath_.size()==1)return {FrontEffect::Resume};
            quickPath_.pop_back();quickPage_=0;rebuild();return {};
        }
        if(key==SDLK_TAB){quickChannel_=cycle(quickChannel_+(shift?-1:1),int(ChatChannel::Count));rebuild();return {};}
        const auto& entries=quickChat_->menus.at(quickPath_.back()).items;
        for(size_t i=0;i<entries.size();++i)if(key==SDL_Keycode(std::tolower(uint8_t(entries[i].key)))) {
            if(entries[i].menu>=0){quickPath_.push_back(entries[i].menu);quickPage_=0;rebuild();return {};}
            return {FrontEffect::QuickChat,entries[i].command,quickChannel_,targetType_,targetId_};
        }
    }
    if(original_ && screen_==Screen::Results && !editingChat_){
        int move=key==SDLK_UP?-1:key==SDLK_DOWN?1:key==SDLK_PAGEUP?-resultsList_.pageRows():key==SDLK_PAGEDOWN?resultsList_.pageRows():0;
        if(move){if(!resultsPlayers_.empty() && resultsList_.move(move)){resultsPlayerSelected_=endgame_.players[resultsPlayers_[resultsList_.selection]].id;rebuild();}return {};}
    }
    if((screen_==Screen::Chat || editingChat_) && wantsText()) {
        if(key==SDLK_ESCAPE){if(screen_==Screen::Chat)return {FrontEffect::Resume};editingChat_=false;rebuild();return {};}
        if(key==SDLK_RETURN)return sendChatAction();
        if(key==SDLK_TAB){if((!original_ || screen_==Screen::Chat) && screen_!=Screen::Results && (chat_.empty() || chat_[0]!='!')){chatSector_=-1;chatChannel_=cycle(chatChannel_+(shift?-1:1),int(ChatChannel::Count));rebuild();}return {};}
        if(key==SDLK_BACKSPACE && !chat_.empty()){size_t n=chat_.size()-1;while(n && (uint8_t(chat_[n])&0xc0)==0x80)--n;chat_.resize(n);rebuild();}return {};
    }

    if(editingAddress_){if(key==SDLK_RETURN || key==SDLK_ESCAPE)editingAddress_=false;else if(key==SDLK_BACKSPACE && !address_.empty())address_.pop_back();rebuild();return {};}
    if(binding_>=0) {
        if(modifierKey(scan))return {}; // wait for a chord, or release to bind the modifier alone
        if(key!=SDLK_ESCAPE && scan!=SDL_SCANCODE_UNKNOWN) {
            assignKey(scan,modifiers);return {};
        }
        binding_=-1;rebuild();return {};
    }
    if(editingWingmen_>=0) {
        if(key==SDLK_RETURN && !wingmenDraft_.empty()) {
            int value=std::stoi(wingmenDraft_);
            if(value<shipIdentityCapacity){(editingWingmen_==0?session_.enemies:session_.allies)=value;editingWingmen_=-1;ready_=false;}
        } else if(key==SDLK_ESCAPE)editingWingmen_=-1;
        else if(key==SDLK_BACKSPACE && !wingmenDraft_.empty())wingmenDraft_.pop_back();
        rebuild();return {};
    }
    if(editingLobbyPilot_>=0) {
        if(key==SDLK_RETURN && !lobbyPilotDraft_.empty()){auto& pilot=session_.localPilots[editingLobbyPilot_];pilot.name=lobbyPilotDraft_;pilot.ready=false;ready_=false;editingLobbyPilot_=-1;}
        else if(key==SDLK_ESCAPE)editingLobbyPilot_=-1;
        else if(key==SDLK_BACKSPACE && !lobbyPilotDraft_.empty()){size_t n=lobbyPilotDraft_.size()-1;while(n && (uint8_t(lobbyPilotDraft_[n])&0xc0)==0x80)--n;lobbyPilotDraft_.resize(n);}
        rebuild();return {};
    }
    if(editingPilot_) {
        if(key==SDLK_RETURN){editingPilot_=false;draft_.normalize();settings_.pilot=draft_.pilot;rebuild();return {FrontEffect::ApplySettings};}
        if(key==SDLK_ESCAPE){editingPilot_=false;draft_.pilot=settings_.pilot;}
        if(key==SDLK_BACKSPACE && !draft_.pilot.empty()){size_t n=draft_.pilot.size()-1;while(n && (uint8_t(draft_.pilot[n])&0xc0)==0x80)--n;draft_.pilot.resize(n);}
        rebuild();return {};
    }
    if(view_.original && (key==SDLK_RETURN || key==SDLK_SPACE || key==SDLK_TAB || key==SDLK_UP || key==SDLK_DOWN))return {}; // Original ButtonPane has no keyboard handler; explicit pane handlers above retain their keys.
    if(key==SDLK_ESCAPE){if(screen_==Screen::Countdown || screen_==Screen::LobbyPilots)open(Screen::Lobby);else if(screen_==Screen::Account)open(Screen::Pilots);else if(screen_==Screen::MissionRules || screen_==Screen::Teams)open(networked_?Screen::RemoteLobby:Screen::Lobby);else if(screen_==Screen::PilotOrders)open(Screen::Pause);else if(screen_==Screen::MouseAction)open(Screen::MouseButtons);else if(screen_==Screen::MouseButtons)open(Screen::Mouse);else if(screen_==Screen::ControllerMapAction)open(Screen::ControllerMapButton);else if(screen_==Screen::ControllerMapButton)open(Screen::ControllerMapButtons);else if(screen_==Screen::ControllerMapAxis || screen_==Screen::ControllerMapDevices || screen_==Screen::ControllerMapButtons)open(Screen::ControllerMap);else if(screen_==Screen::ControllerMap)open(Screen::Controller);else if(screen_==Screen::ControllerAction)open(Screen::ControllerButtons);else if(screen_==Screen::ControllerAxes || screen_==Screen::ControllerButtons)open(Screen::Controller);else if(screen_==Screen::ChatPreferences || screen_==Screen::Mouse || screen_==Screen::Visuals)open(Screen::Interface);else if(screen_==Screen::Interface || screen_==Screen::Audio || screen_==Screen::Movies || screen_==Screen::Controls || screen_==Screen::Controller)open(Screen::Settings);else if(screen_==Screen::Settings)open(settingsReturn_);else if(screen_==Screen::CargoMount || (screen_==Screen::Parts && partMount_<0))open(Screen::Cargo);else if(playing_ && complete_)open(Screen::Results);else if(playing_ && docked_ && !trainingCommand_)open(networkLobby_?Screen::RemoteLobby:Screen::Hangar);else if(playing_)return {FrontEffect::Resume};else open(Screen::Home);return {};}
    if(key==SDLK_TAB || key==SDLK_DOWN || key==SDLK_UP) {
        int d=key==SDLK_UP || (key==SDLK_TAB && shift)?-1:1;
        do{view_.focus=cycle(view_.focus+d,int(view_.items.size()));}while(!view_.items[view_.focus].enabled);if(screen_==Screen::ShipLoadout)pointerHover(size_t(view_.focus));return {};
    }
    if(key==SDLK_RETURN || key==SDLK_SPACE)return activate(view_.focus);
    return {};
}
void Frontend::text(std::string_view value){
    if(!wantsText())return;
    if(logonOpen_){editTyped("logon/nameEditPane");Allegiance::AppendEditInput(logonNameDraft_,value,23);rebuild();return;}
    if(missionBriefOpen_)return;
    if(hasMessageBox())return;
    if(original_ && screen_==Screen::GameDetails)return;
    if((!popups_.empty() && popups_.front().group!=PopupGroup::Commands) || popupTextKey_)return;
    if(findPlayerOpen_){editTyped("gamescreen/findPlayerEditPane");Allegiance::AppendEditInput(findPlayerName_,value,findPlayerName_.max_size());rebuild();return;}
    if(gameFilterEditing_){editTyped(gameCreateOpen_?"gamescreen/createEditPane":"gamescreen/gameNameEditPane");Allegiance::AppendEditInput(gameNameDraft_,value,gameNameDraft_.max_size());rebuild();return;}
    if(teamSettingsOpen_){
        if(teamNameFocus_)editTyped("teamscreen/teamNameEditPane");
        if(teamNameFocus_)Allegiance::AppendEditInput(teamNameDraft_,value,24);
        rebuild();return;
    }
    if(editingAddress_){Allegiance::AppendEditInput(address_,value,128,[](unsigned char c){return c>32 && c<127;});rebuild();return;}
    if(editingWingmen_>=0){Allegiance::AppendEditInput(wingmenDraft_,value,5,[](unsigned char c){return c>='0' && c<='9';});rebuild();return;}
    if(editingLobbyPilot_>=0){Allegiance::AppendEditInput(lobbyPilotDraft_,value,23);rebuild();return;}
    if(screen_==Screen::Chat || editingChat_){editTyped(screen_==Screen::Results?"gameoverscreen/chatEditPane":"teamscreen/chatEditPane");Allegiance::AppendEditInput(chat_,value,255);rebuild();return;}
if(editingPilot_){Allegiance::AppendEditInput(draft_.pilot,value,23);rebuild();}}
void Frontend::chatSent(){if(!playing_){GameMessage m{};m.text=settings_.pilot+": "+chat_;m.color=glm::vec4(1);m.chat=m.fromPlayer=true;m.serial=lobbyMessages_.empty()?1:lobbyMessages_.back().serial+1;m.chatTarget=chatChannel_==int(ChatChannel::Team)?CHAT_TEAM:chatChannel_==int(ChatChannel::Leaders)?CHAT_LEADERS:CHAT_EVERYONE;lobbyMessages_.push_back(std::move(m));if(lobbyMessages_.size()>Allegiance::ChatHistoryCapacity)lobbyMessages_.pop_front();lobbyChat_.dirty=resultsChat_.dirty=true;}chat_.clear();rebuild();}
void Frontend::lobbyQuickChatSent(int command){
    if(!playing_ && quickChat_ && command>=0 && command<int(quickChat_->commands.size())){
        GameMessage message{};message.text=settings_.pilot+": "+quickChat_->commands[command].text;message.color=glm::vec4(1);message.chat=message.fromPlayer=true;message.serial=lobbyMessages_.empty()?1:lobbyMessages_.back().serial+1;
        message.chatTarget=chatChannel_==int(ChatChannel::Team)?CHAT_TEAM:chatChannel_==int(ChatChannel::Leaders)?CHAT_LEADERS:CHAT_EVERYONE;
        lobbyMessages_.push_back(std::move(message));if(lobbyMessages_.size()>Allegiance::ChatHistoryCapacity)lobbyMessages_.pop_front();lobbyChat_.dirty=resultsChat_.dirty=true;rebuild();
    }
}
}
