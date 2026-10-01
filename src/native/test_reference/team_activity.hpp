#pragma once
#include "../frontend.hpp"
#include "../network_records.hpp"
#include "../../ClientCore/MissionLifecycle.h"
#include "../test_fixture.hpp"

namespace NativeTest {
inline void checkTeamActivity(Native::Assets& assets){
    using namespace Native;
    auto find=[](const Frontend& ui,int side){auto name="activeTeamButtonPane"+std::to_string(side+1);auto i=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& item){return item.control==name;});return i==ui.view().items.end()?std::optional<size_t>{}:std::optional(size_t(i-ui.view().items.begin()));};
    size_t cases=0;
    for(int teams=2;teams<=6;++teams)for(bool owner:{false,true})for(bool created:{false,true})for(auto stage:{STAGE_NOTSTARTED,STAGE_STARTING,STAGE_STARTED,STAGE_OVER}){
        Settings settings;Frontend ui(settings);ui.originalInterface(assets);SessionOptions options;options.scenario=Scenario::Conquest;options.rules.teams=teams;ui.networkSession(options);ui.setPlaying(true);
        GameState state;state.networked=state.networkLobby=true;state.networkHost=owner;state.playerSide=0;state.missionStage=stage;
        state.lobbyFlags=(created?Allegiance::LobbyObjectModelCreated:0)|(stage==STAGE_STARTED?4:0);state.teamScores.resize(teams);
        state.networkPilots={{7,0,"Owner",true,true,true,true,false},{8,1,"Ghost or reconnect seat",true,false,true,false,false}};
        state.teamScores.back().active=false;ui.open(Screen::RemoteLobby,&state);
        bool visible=owner && !created && stage==STAGE_NOTSTARTED;
        require(std::any_of(ui.view().items.begin(),ui.view().items.end(),[](const auto& item){return item.control=="settingsButtonPane";})==visible,"fixed-world/stage lobby exposes original editable Settings instead of Details");
        for(int side=0;side<6;++side){auto item=find(ui,side);require(bool(item)==(visible && side<teams),"original team activity visibility differs from owner/stage/fixed-world gate");
            if(item)require(ui.view().items[*item].enabled==(side>=2) && ui.view().items[*item].selected==state.teamScores[side].active,"team activity ignores occupied seats or reported checked state");}
        if(visible && teams>=3){auto index=*find(ui,2);auto before=ui.view().items[index].selected;auto mask=ui.session().inactiveTeams;ui.clearSounds();ui.pointerContext(index);ui.pointerRelease(index,false,SDL_BUTTON_RIGHT);
            require(ui.view().items[*find(ui,2)].selected==!before && ui.session().inactiveTeams==mask && ui.sounds().empty(),"right team checkbox sends a host callback or loses its silent toggle");
            ui.open(Screen::Home);ui.open(Screen::RemoteLobby,&state);index=*find(ui,2);ui.pointerPress(index);auto action=ui.pointerRelease(index);
            require(action.effect==FrontEffect::LobbyAction && action.command==int(LobbyAction::TeamActive) && action.targetId==2 && action.value==int(!before),"left team checkbox loses side identity or checked intent");}
        ++cases;
    }
    Settings settings;Frontend local(settings);local.originalInterface(assets);SessionOptions options;options.scenario=Scenario::Conquest;options.rules.teams=4;options.rules.miners=1;options.rules.maxImbalance=32767;options.startInHangar=true;local.networkSession(options);local.open(Screen::LobbyPilots);
    auto click=[&](int side){return local.activate(*find(local,side));};
    require(!local.view().items[*find(local,0)].enabled,"local occupied owner team can be disabled");
    click(2);require(local.session().inactiveTeams==4 && !local.session().requireTeamPlayers && !local.view().items[*find(local,2)].selected,"local empty team remains active");
    require(std::none_of(local.view().items.begin(),local.view().items.end(),[](const auto& item){return item.control=="team2";}),"inactive original team row is still selectable");
    click(2);require(!local.session().inactiveTeams && local.session().requireTeamPlayers,"all-active checkbox policy fails to restore the original minimum-player gate");
    click(2);click(3);options=local.session();
    auto raw=Net::session(options);auto restored=Net::options(raw,0,"Owner");require(raw.teamPolicy==12 && restored.inactiveTeams==12 && !restored.requireTeamPlayers && sizeof(raw)==484,"team activity bootstrap changes raw record size or loses its compact policy");
    Game game(assets.root(),[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
    for(SideID side=0;side<4;++side){auto* team=game.core()->GetSide(side);require(team->GetActiveF()==(side<2),"local world ignores inactive team setup");
        require((team->GetStations()->n()>0)==(side<2) && (team->GetShips()->n()>0)==(side<2),"local start retains inactive bases/drones or removes active teams");}
    require(!game.state().complete && game.state().docked && game.state().teamScores.size()==4,"inactive local setup prematurely ends play or discards original team seats");
    auto invalid=options;invalid.localPilots={{"Occupied inactive side",2,true}};require(!Game::missionError(invalid).empty(),"inactive team accepts a local pilot");
    printf("Original team activity: %zu owner/stage/fixed-world matrices, both-button intent, occupied-seat gates, original row removal/minimum policy, unchanged-size raw bootstrap and actual offline bases/drones passed\n",cases);
}
}
