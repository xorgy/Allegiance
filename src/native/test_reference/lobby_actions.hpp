#pragma once
#include "../frontend.hpp"
#include "../lobby_network_actions.hpp"
#include "../../ClientCore/CommunityLinks.h"
#include "../../ClientCore/LobbyActions.h"
#include "../../ClientCore/LobbyAdmission.h"
#include "../../ClientCore/MissionLifecycle.h"
namespace NativeTest {
inline void checkLobbyActions(Native::Assets& assets) {
    using namespace Native;
    auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);
    SessionOptions options;options.scenario=Scenario::Conquest;ui.networkSession(options);
    GameState state;state.networked=state.networkLobby=true;state.playerSide=0;state.endgame.frozen=true;
    ui.open(Screen::RemoteLobby,&state);
    require(std::none_of(ui.view().items.begin(),ui.view().items.end(),[](const auto& item){return item.control=="gameOverButtonPane";}),"Previous Scores is visible without any original result records");
    Net::ResultPlayer resultPilot{};resultPilot.id=10;resultPilot.side=0;Net::text(resultPilot.name,"Self");state.endgame.players.push_back(resultPilot);
    state.networkPilots={{10,0,"Self",true,false,true,true,false},{20,0,"Successor",true,false,false,false,false},{30,noTeam,"Applicant",true,false,false,false,false,0}};
    auto control=[&](std::string_view name){auto& items=ui.view().items;auto i=std::find_if(items.begin(),items.end(),[&](auto& p){return p.control==name;});if(i==items.end())throw std::runtime_error("lobby action control is absent: "+std::string(name));return size_t(i-items.begin());};
    auto click=[&](std::string_view name){return ui.activate(control(name));};
    auto select=[&](int id){auto& items=ui.view().items;auto i=std::find_if(items.begin(),items.end(),[&](auto& p){return p.action.op==MenuOp::RosterSelect && p.action.delta==id;});require(i!=items.end(),"lobby action pilot is absent");if(!i->selected)ui.activate(size_t(i-items.begin()));};
    auto message=[&]{return ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);};
    auto channelRestored=[&]{auto m=message();return m.effect==FrontEffect::SendChat && m.value==int(ChatChannel::Team) && m.targetId==-1 && ui.chatText()=="Unsent draft" &&
        std::none_of(ui.view().items.begin(),ui.view().items.end(),[](auto& p){return p.action.op==MenuOp::RosterSelect && p.selected;});};
    state.networkHost=state.commander=state.networkReady=true;state.networkPilots[0].host=true;
    ui.open(Screen::RemoteLobby,&state);ui.text("Unsent draft");select(30);require(message().targetId==30,"applicant selection did not address private chat");
    auto accept=click("acceptButtonPane");require(accept.effect==FrontEffect::LobbyAction && accept.command==int(LobbyAction::Accept) && accept.targetId==30 && channelRestored(),"Accept loses its target or retains a private chat recipient/selection");
    select(20);auto leader=click("makeLeaderButtonPane");require(leader.effect==FrontEffect::MakeLeader && leader.value==20 && channelRestored(),"Make Leader loses its target or retains a private chat recipient/selection");
    select(30);auto reject=click("rejectButtonPane");require(reject.effect==FrontEffect::LobbyAction && reject.targetId==30 && message().targetId==30,"Reject incorrectly adopts Accept's selection-clearing behavior");
    for(bool popup:{false,true}){
        ui.open(Screen::RemoteLobby,&state);select(30);
        FrontResult result;
        if(popup){ui.pointerContext(control("roster30"));result=ui.key(SDLK_A,SDL_SCANCODE_A);ui.keyReleased(SDLK_A,SDL_SCANCODE_A);}
        else result=click("acceptButtonPane");
        require(result.effect==FrontEffect::LobbyAction && result.command==int(LobbyAction::Accept) && result.targetId==30,"context-menu Accept does not dispatch the selected applicant");
        if(!channelRestored()){auto sent=message();std::fprintf(stderr,"Accept fixture popup=%d effect=%d channel=%d recipient=%d draft=%s\n",popup,int(sent.effect),sent.value,sent.targetId,ui.chatText().c_str());throw std::runtime_error("context-menu Accept retains its private recipient or consumes the unsent draft");}
    }
    size_t cases=0;
    for(bool owner:{false,true})for(bool commander:{false,true})for(bool ready:{false,true})for(bool request:{false,true}){
        state.networkHost=owner;state.commander=commander;state.networkReady=ready;state.forceReady=1;
        state.networkPilots[0].host=owner;state.networkPilots[0].commander=commander;state.networkPilots[0].ready=ready;state.networkPilots[0].requested=request?1:noTeamRequest;
        ui.open(Screen::RemoteLobby,&state);auto visit=click(owner?"settingsButtonPane":"detailsButtonPane");
        bool withdraw=commander && !owner && ready;
        require(ui.screen()==Screen::Lobby && visit.effect==((withdraw || request)?FrontEffect::LobbyVisit:FrontEffect::None) &&
            visit.value==int(withdraw) && visit.command==int(request),"mission details readiness/request policy differs from WinTrek");
        std::vector<Net::Action> records;emitLobbyNetworkActions(visit,true,ready,[&](const auto& r){records.push_back(r);});
        require(records.size()==size_t(withdraw?2:0)+size_t(request),"lobby visit sends redundant or missing actions");
        if(withdraw)require(records[0].op==Net::Operation::Lobby && records[0].args[0]==int(LobbyAction::ForceReady) && records[0].args[1]==0 && records[1].op==Net::Operation::Ready && records[1].args[0]==0,"settings entry does not clear force-ready before personal AFK");
        if(request)require(records.back().op==Net::Operation::Lobby && records.back().args[0]==int(LobbyAction::CancelRequest),"details entry does not cancel the outstanding request");
        ui.activate(size_t(std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& item){return item.action.op==MenuOp::SetupCancel;})-ui.view().items.begin()));require(ui.screen()==Screen::RemoteLobby,"mission details Back loses the lobby");
        require(ui.view().items[control("awayFromKeyboardButtonPane")].selected==(!ready || withdraw),"details cancellation silently restores personal readiness");
        ui.open(Screen::RemoteLobby,&state);auto scores=click("gameOverButtonPane");
        require(ui.screen()==Screen::Results && scores.effect==(request?FrontEffect::LobbyVisit:FrontEffect::None) && !scores.value && scores.command==int(request),"previous scores changes readiness or retains a pending join request");
        ++cases;
    }
    for(bool classic:{false,true})for(bool ready:{false,true})for(auto kind:{LobbyAction::ForceReady,LobbyAction::CancelRequest,LobbyAction::AutoAccept}){
        std::vector<Net::Action> records;emitLobbyNetworkActions({FrontEffect::LobbyAction,0,int(kind),-1,30},classic,ready,[&](const auto& r){records.push_back(r);});
        bool wake=classic && !ready && kind!=LobbyAction::AutoAccept;
        require(records.size()==size_t(wake?2:1) && records.back().op==Net::Operation::Lobby && records.back().args[0]==int(kind) && records.back().args[2]==30,"original JOIN/CANCEL/Team Ready action ordering or target changed");
        if(wake)require(records.front().op==Net::Operation::Ready && records.front().args[0]==1,"JOIN/CANCEL while AFK does not restore personal readiness first");
    }
    size_t joinCases=0;
    for(bool pending:{false,true})for(bool ownNoTeam:{false,true})for(bool viewedNoTeam:{false,true})for(bool ownTeam:{false,true})for(bool available:{false,true})for(bool active:{false,true}){
        bool original=pending || ((ownNoTeam && !viewedNoTeam) || (viewedNoTeam || available && active) && !ownTeam);
        require(Allegiance::LobbyJoinEnabled(pending,ownNoTeam,viewedNoTeam,ownTeam,available,active)==original,"JOIN gate differs from TeamScreen::UpdateButtons");
    }
    options.rules.teams=3;state.teamScores.resize(3);for(auto& team:state.teamScores)team.active=true;
    for(auto stage:{STAGE_NOTSTARTED,STAGE_STARTING,STAGE_STARTED,STAGE_OVER})for(bool locked:{false,true})for(bool defections:{false,true})for(bool pending:{false,true})for(int own:{noTeam,0,1})for(int viewed:{noTeam,0,1}){
        options.rules.allowDefections=defections;options.team=std::max(0,own);ui.networkSession(options);
        state.missionStage=stage;state.lobbyFlags=(locked?2:0)|(stage==STAGE_STARTED?4:0);state.playerSide=own;
        state.networkPilots={{10,own,"Self",true,false,false,true,false,pending?1:noTeamRequest}};
        ui.open(Screen::Home);ui.open(Screen::RemoteLobby,&state);click(viewed==noTeam?"noTeam":viewed?"team1":"team0");
        bool enabled=pending || viewed!=own;
        require(ui.view().items[control("joinButtonPane")].enabled==enabled,"JOIN adds a side-lock/defection filter or enables the current side");
        auto result=click("joinButtonPane");
        require(result.effect==(!enabled?FrontEffect::None:pending?FrontEffect::LobbyAction:FrontEffect::NetworkJoinTeam),"JOIN/Cancel dispatch differs from the displayed original gate");
        if(enabled)require(pending?result.command==int(LobbyAction::CancelRequest):result.value==viewed,"JOIN/Cancel loses its requested side or cancellation intent");
        ++joinCases;
    }
    state.missionStage=STAGE_STARTED;state.lobbyFlags=4;state.playerSide=0;state.teamScores[2].active=false;
    state.networkPilots={{10,0,"Self",true,false,false,true,false},{21,2,"Eliminated pilot",true,false,false,false,false}};
    ui.networkSession(options);ui.open(Screen::Home);ui.open(Screen::RemoteLobby,&state);click("team2");
    require(!ui.view().items[control("joinButtonPane")].enabled,"inactive populated faction remains joinable from another faction");
    state.playerSide=state.networkPilots[0].team=noTeam;state.networkReady=true;ui.open(Screen::Home);ui.open(Screen::RemoteLobby,&state);click("team2");
    require(ui.view().items[control("joinButtonPane")].enabled && click("joinButtonPane").effect==FrontEffect::None,"No Team JOIN ignores the original handler's separate inactive-side gate");
    state.networkReady=false;ui.sync(state);require(click("joinButtonPane").effect==FrontEffect::NetworkReady,"inactive-side JOIN loses the original AFK-only wake-up");
    state.playerSide=state.networkPilots[0].team=0;state.networkReady=true;ui.sync(state);click("team2");
    state.networkPilots.pop_back();ui.sync(state);
    require(ui.view().items[control("noTeam")].action.delta==0 && ui.view().items[control("joinButtonPane")].enabled,"empty inactive selection does not refresh to No Team");
    ui.quittingTeam(71);
    require(ui.pendingAction(71) && ui.messageView().items.empty() && std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.name=="messageText" && c.text=="Quitting current team...";}),"No Team departure lacks its original non-dismissible pending dialog");
    ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);ui.transferFinished(70);
    require(ui.pendingAction(71) && !ui.actionRejected(70,"Old failure"),"pending departure dismisses on a key or stale receipt");
    ui.transferFinished(71);require(!ui.hasMessageBox(),"canonical departure leaves the original pending dialog open");
    ui.quittingTeam(72);require(ui.actionRejected(72,"Departure failed") && ui.hasMessageBox() && !ui.pendingAction(72),"matching departure rejection does not replace its pending dialog");ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
    ui.quittingTeam(73);ui.quittingTeam(74);ui.transferFinished(73);require(ui.pendingAction(74),"old team receipt dismisses a newer departure");ui.transferFinished(74);
    ui.cancelingRequest(75);require(ui.pendingAction(75) && ui.messageView().items.empty() && std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.name=="messageText" && c.text=="Canceling request...";}),"request cancellation lacks its original non-dismissible caption");
    ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.transferFinished(74);require(ui.pendingAction(75),"request cancellation dismisses on Escape or an older completion");ui.transferFinished(75);
    size_t controlCases=0;
    for(auto stage:{STAGE_NOTSTARTED,STAGE_STARTING,STAGE_STARTED,STAGE_OVER})for(bool owner:{false,true})for(bool commander:{false,true})for(bool lockedOpen:{false,true})for(bool scores:{false,true}){
        state.missionStage=stage;state.complete=stage==STAGE_OVER;state.networkHost=owner;state.commander=commander;state.playerSide=0;
        state.lobbyFlags=(stage==STAGE_STARTED?4:0)|(lockedOpen?Allegiance::LobbyGameOpenLocked:0);
        state.networkPilots={{10,0,"Self",true,owner,commander,true,false}};state.forceReady=0;state.teamScores[2].active=true;
        state.endgame.players.clear();if(scores)state.endgame.players.push_back(resultPilot);
        ui.networkSession(options);ui.open(Screen::Home);ui.open(Screen::RemoteLobby,&state);
        auto present=[&](std::string_view name){return std::any_of(ui.view().items.begin(),ui.view().items.end(),[&](const auto& item){return item.control==name;});};
        require(ui.view().items[control("lockLobbyButtonPane")].enabled==(owner && !lockedOpen),"Lock Lobby ignores the original game-open policy");
        require(ui.view().items[control("lockSidesButtonPane")].enabled==owner,"game-open policy incorrectly disables independent side locking");
        require(present("autoAcceptButtonPane")==commander,"Auto Accept ignores original commander visibility");
        if(commander)require(ui.view().items[control("autoAcceptButtonPane")].enabled==(stage!=STAGE_STARTED && !lockedOpen),"Auto Accept ignores stage or game-open policy");
        require(present("randomizeButtonPane")==owner && (!owner || ui.view().items[control("randomizeButtonPane")].enabled),"Randomize adds a stage filter missing from the original UI");
        require(present("gameOverButtonPane")==scores && (!scores || ui.view().items[control("gameOverButtonPane")].enabled),"Previous Scores differs from the original result-record presence gate");
        ++controlCases;
    }
    size_t balances=0;
    struct AdmissionCase{bool setup;int imbalance,count,minimum;bool allowed;};
    // Original-policy boundaries, including large rosters: two rank-zero
    // newcomers, lowest-rank ties only, unrestricted setup and balance off.
    constexpr AdmissionCase admissionCases[]={
        {true,0,1024,0,true},{true,32766,1024,0,true},
        {false,0,0,0,true},{false,0,1,0,true},{false,0,2,0,false},
        {false,1,2,0,true},{false,1,3,0,false},
        {false,2,4,1,true},{false,2,5,1,false},
        {false,0,1023,1022,true},{false,0,1024,1022,false},
        {false,32766,512,512,true},{false,32766,513,512,false},
        {false,32767,1024,0,true}};
    for(auto c:admissionCases){
        require(Allegiance::UnrankedLobbyPositionAvailable(c.setup,c.imbalance,c.count,c.minimum)==c.allowed,"unranked position admission differs from original count/rank-one boundaries");++balances;
    }
    require(std::string_view(Allegiance::LobbyJoinFailureMessage(Allegiance::LobbyJoinFailure::SidesLocked))=="The sides are currently locked." &&
        std::string_view(Allegiance::LobbyJoinFailureMessage(Allegiance::LobbyJoinFailure::TeamBalance))=="You can't join this team - it would unbalance the teams." &&
        std::string_view(Allegiance::LobbyJoinFailureMessage(Allegiance::LobbyJoinFailure(999)))=="You were not able to join the team.","original admission caption or unknown-result fallback changed");
    size_t readinessCases=0;options.rules.teams=6;state.teamScores.resize(6);
    for(int side=0;side<6;++side){state.teamScores[side].name="Team "+std::to_string(side+1);state.teamScores[side].active=true;}
    for(bool autoStart:{false,true})for(unsigned reconnecting=0;reconnecting<64;++reconnecting){
        state.complete=false;state.networkHost=state.commander=state.networkReady=true;state.playerSide=0;state.missionStage=STAGE_NOTSTARTED;
        state.lobbyFlags=(autoStart?Allegiance::LobbyAutoStart:0)|(reconnecting?Allegiance::LobbyReconnecting|(reconnecting<<Allegiance::LobbyReconnectingShift):0);
        state.networkPilots.clear();
        for(int side=0;side<6;++side)if(!(reconnecting&(1u<<side)))state.networkPilots.push_back({10+side,side,"Pilot",true,side==0,side==0,side==0,false});
        ui.networkSession(options);ui.open(Screen::Home);ui.open(Screen::RemoteLobby,&state);
        auto text=[&](std::string_view name){auto i=std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.name==name;});require(i!=ui.view().artwork.end(),"lobby readiness text is absent");return i->text;};
        require(ui.view().items[control("startButtonPane")].enabled==(!autoStart && !reconnecting),"manual Start ignores automatic-start policy or faction reconnect grace");
        if(!reconnecting)require(text("textStatus")=="READY TO START" && text("textStatus2").empty(),"automatic-start policy changes original ready status");
        else if(!(reconnecting&(reconnecting-1))){int side=0;while(!(reconnecting&(1u<<side)))++side;require(text("textStatus")=="TEAM "+std::to_string(side+1) && text("textStatus2")=="IS NOT READY","one dormant faction falsely reports every team unready");}
        else require(text("textStatus")=="ONE OR MORE TEAMS" && text("textStatus2")=="ARE NOT READY","multiple dormant factions lose original collective readiness status");
        ++readinessCases;
    }
    state.complete=false;state.networkPilots={{10,0,"Self",true,true,true,true,false}};
    state.lobbyFlags=0;state.missionStage=STAGE_NOTSTARTED;state.teamScores[2].active=true;
    constexpr const char* teamLinks[]={"https://discord.gg/qv3cEa7","https://discord.gg/6JWrRdZ","https://discord.gg/wytSK2Q","https://discord.gg/5QWGrSa","https://discord.gg/WrHj7AS","https://discord.gg/eUrSxek","https://discord.gg/43jn6Ab"};
    options.rules.teams=6;
    for(int side=0;side<7;++side){int id=side==6?noTeam:side;options.team=std::max(0,id);ui.networkSession(options);state.networkPilots[0].team=state.playerSide=id;state.networkPilots[0].requested=noTeamRequest;ui.open(Screen::Home);ui.open(Screen::RemoteLobby,&state);
        auto web=click("discordButtonPane");require(web.effect==FrontEffect::Web && std::string_view(Allegiance::CommunityLink(web.value))==teamLinks[side],"team voice button uses the general community invitation or wrong faction");}
    require(!Allegiance::CommunityLink(-1) && !Allegiance::CommunityLink(11),"invalid community link resolves to an unrelated destination");
    std::printf("Original lobby actions: preserved targets/drafts, Accept/Make Leader selection clearing, Reject retention, %zu detail/score cases, %zu JOIN cases, %zu owner/commander/stage/open-policy/result control cases, %zu count/rank-one admission cases, %zu automatic-start/faction-reconnect readiness cases, inactive selection, causal departure/cancel lockdown, original rejection captions, ordered AFK/cancel records and seven team voice destinations passed\n",cases,joinCases,controlCases,balances,readinessCases);
}
}
