#pragma once
#include "../frontend.hpp"
#include "../lobby_network_actions.hpp"
#include <cstdio>
namespace NativeTest {
inline void checkLobbyAllianceUi(Native::Assets& assets) {
    using namespace Native;
    auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);
    SessionOptions options;options.scenario=Scenario::Conquest;options.rules.teams=4;
    GameState state;state.networked=state.networkLobby=state.networkHost=state.commander=state.networkReady=true;
    state.networkPilots={{10,0,"Owner",true,true,true,true,false},{20,0,"Wing pilot",true,false,false,false,false}};
    state.teamScores.resize(4);for(auto& team:state.teamScores)team.active=true;
    auto enter=[&]{ui.networkSession(options);ui.open(Screen::Home);ui.open(Screen::RemoteLobby,&state);};
    auto control=[&](std::string_view name){auto& items=ui.view().items;auto i=std::find_if(items.begin(),items.end(),[&](const auto& p){return p.control==name;});require(i!=items.end(),"alliance fixture control absent");return size_t(i-items.begin());};
    auto context=[&](int side){ui.pointerMove({250,200});return ui.pointerContext(control(side==noTeam?"noTeam":"team"+std::to_string(side)));};
    auto rows=[&]{std::vector<std::pair<std::string,int>> result;for(auto& item:ui.view().items)if(item.popupLevel>=0)result.emplace_back(item.label,item.action.op==MenuOp::AllianceCancel?NA:item.action.delta);return result;};
    enter();ui.text("Retained draft");ui.activate(control("roster20"));require(ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).targetId==20,"private recipient fixture failed");
    context(0);
    require(rows()==std::vector<std::pair<std::string,int>>{{"Ally with Team 2",1},{"Ally with Team 3",2},{"Ally with Team 4",3},{"Cancel",NA}},"original solo-side menu order/labels/IDs differ");
    auto popup=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& p){return p.popupLevel>=0;});
    require(popup!=ui.view().items.end() && popup->bounds.y<200 && popup->bounds.x>=250,"alliance popup is not anchored above the right-click point");
    auto intent=ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
    require(intent.effect==FrontEffect::ChangeAlliance && intent.value==0 && intent.targetId==1 && !intent.command,"alliance menu loses selected side/ally");
    require(ui.session().alliances==options.alliances,"remote alliance menu mutates state before authority acceptance");
    auto chat=ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
    require(chat.effect==FrontEffect::SendChat && chat.targetId==-1 && ui.chatText()=="Retained draft","team right click retains a private recipient or consumes its unsent draft");
    std::vector<Net::Action> records;emitLobbyNetworkActions(intent,true,false,[&](auto record){records.push_back(record);});
    require(records.size()==1 && records[0].op==Net::Operation::Lobby && records[0].args[0]==int(LobbyAction::Alliance) && records[0].args[1]==0 && records[0].args[2]==1 && !records[0].args[3],"alliance sends excess actions or loses intent fields");
    options.alliances={0,0,-1,-1,-1,-1};options.rules.alliedRipcord=false;enter();context(2);
    require(rows()==std::vector<std::pair<std::string,int>>{{"Ally with Team 4",3},{"Ally with Teams 1,2",1},{"Cancel",NA}},"original alliance-group menu order/representative differs");
    ui.key(SDLK_DOWN,SDL_SCANCODE_DOWN);intent=ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
    require(intent.effect==FrontEffect::ChangeAlliance && intent.value==2 && intent.targetId==1 && intent.command==1,"group keyboard selection loses the deliberate ripcord opt-out");
    context(0);require(rows()==std::vector<std::pair<std::string,int>>{{"Remove allies",NA},{"Cancel",NA}},"allied team's original menu is not Remove/Cancel");
    ui.key(SDLK_DOWN,SDL_SCANCODE_DOWN);require(ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect==FrontEffect::None && ui.screen()==Screen::RemoteLobby && ui.session().alliances==options.alliances,"alliance Cancel changes groups or navigates away");
    context(0);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);require(rows().empty() && ui.screen()==Screen::RemoteLobby,"alliance Escape loses its team screen");
    size_t gates=0;
    for(int gate=0;gate<10;++gate){
        auto saved=options;auto previous=state;
        options.alliances.fill(NA);
        if(gate==0)state.networkHost=false;
        if(gate==1)state.networkCountdown=1;
        if(gate==2){state.networkLobby=false;state.lobbyFlags=4;}
        if(gate==3)options.rules.teams=2;
        if(gate==4){state.teamScores[2].active=state.teamScores[3].active=false;}
        if(gate==5)options.rules.artifacts=1;
        if(gate==6)options.rules.teamKills=1;
        if(gate==7)options.rules.flags=1;
        if(gate==8)options.rules.prosperity=1;
        if(gate==9)options.rules.territory=50;
        enter();context(0);require(rows().empty(),"alliance menu ignores owner/stage/active-team/objective gate");
        options=saved;state=previous;++gates;
    }
    enter();context(noTeam);require(rows().empty(),"No Team offers an actionable alliance");
    options.alliances.fill(NA);enter();context(0);
    state.networkHost=false;ui.sync(state);require(rows().empty(),"owner departure leaves a stale alliance popup actionable");
    // Reliable policy must reach the team painter and read-only details. An
    // unchanged report must not overwrite a locally staged settings choice.
    state.networkHost=true;state.maxImbalance=32767;state.alliances={0,0,-1,-1,-1,-1};state.alliedSight=state.alliedRipcord=true;
    ui.sync(state);require(ui.session().alliances==state.alliances && ui.session().rules.alliedSight && ui.session().rules.alliedRipcord,"authoritative live policy is missing from frontend state");
    ui.activate(control("settingsButtonPane"));
    state.alliances.fill(NA);state.maxImbalance=32766;state.allowDefections=state.alliedSight=state.alliedRipcord=false;ui.sync(state);
    require(ui.session().alliances==state.alliances && !ui.session().rules.allowDefections,"open details retains obsolete alliance policy");
    auto combo=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& item){return item.control=="maxImbalanceComboPane";});
    require(combo!=ui.view().items.end() && combo->enabled,"automatic-balance fixture combo absent");
    int field=combo->action.value;
    ui.activate(size_t(combo-ui.view().items.begin()));
    auto choice=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& item){return item.action.op==MenuOp::SetupChoice && item.action.value==field && item.action.delta==1;});
    require(choice!=ui.view().items.end(),"staged balance fixture choice absent");ui.activate(size_t(choice-ui.view().items.begin()));
    int staged=ui.session().rules.maxImbalance;require(staged!=32766,"staged balance fixture did not change its value");ui.sync(state);require(ui.session().rules.maxImbalance==staged,"unchanged authority report resets staged balance editing");
    auto back=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& item){return item.action.op==MenuOp::SetupCancel;});ui.activate(size_t(back-ui.view().items.begin()));
    require(ui.session().alliances==state.alliances && ui.session().rules.maxImbalance==32766,"Back restores an obsolete live alliance policy");
    ui.setPlaying(false);options.alliances.fill(NA);ui.networkSession(options);ui.open(Screen::LobbyPilots);context(0);ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
    require(ui.session().alliances[0]==0 && ui.session().alliances[1]==0 && ui.session().rules.allowDefections && ui.session().rules.alliedSight && ui.session().rules.alliedRipcord && ui.session().rules.maxImbalance==32767,"local original team menu does not form a valid alliance with original rules");
    context(0);ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);require(std::all_of(ui.session().alliances.begin(),ui.session().alliances.end(),[](char group){return group==NA;}) && !ui.session().rules.allowDefections && !ui.session().rules.alliedSight && !ui.session().rules.alliedRipcord && ui.session().rules.maxImbalance==32766,"last local alliance removal does not restore original rule state");
    options.inactiveTeams=8;ui.networkSession(options);ui.open(Screen::Home);ui.open(Screen::LobbyPilots);context(0);
    require(rows()==std::vector<std::pair<std::string,int>>{{"Ally with Team 2",1},{"Ally with Team 3",2},{"Cancel",NA}},"local alliance menu offers an inactive faction from stale or absent reports");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    ui.activate(control("team2"));ui.activate(control("activeTeamButtonPane3"));
    require(ui.view().items[control("team0")].action.delta==0 && !ui.view().items[control("joinButtonPane")].enabled,"local inactive selection retains an unusable JOIN action instead of its pilot seat");
    std::printf("Original alliance UI: solo/group/Remove/Cancel menus, mouse anchor, keyboard, draft/recipient, ordered intent, ripcord opt-out, %zu role/stage/active/objective gates, stale-owner retirement, live/staged details and local formation/removal passed\n",gates);
}
}
