#pragma once
#include "../frontend.hpp"
#include "../test_fixture.hpp"

namespace NativeTest {
inline void checkMissionSetupParity(Native::Assets& assets){
    using namespace Native;
    const std::string noGoals="You must choose one or more winning conditions.";
    for(int mask=0;mask<128;++mask){
        SessionOptions options;options.scenario=Scenario::Conquest;options.map=2;
        auto& r=options.rules;r.conquest=(mask&1)?100:0;r.teamKills=(mask&2)?10:0;r.minutes=(mask&4)?15:0;r.prosperity=(mask&8)?1:0;
        r.artifacts=(mask&16)?1:0;r.territory=(mask&32)?75:100;r.flags=(mask&64)?1:0;
        require((Game::missionError(options,{},true)==noGoals)==(mask==0),"client setup does not recognize all seven original winning conditions");
        if(!mask)require(Game::missionError(options).empty(),"server-authored no-winning-condition missions lost support");
    }
    for(bool noWin:{false,true})for(bool automatic:{false,true})for(bool clubObject:{false,true}){
        MissionParams original;original.iGoalConquestPercentage=noWin?0:100;original.bAutoRestart=automatic;original.bObjectModelCreated=clubObject;original.bClubGame=clubObject;
        auto error=Allegiance::MissionClientFlagsError(noWin,automatic,clubObject);auto legacy=original.Invalid(true);
        require((!error && !legacy) || (error && legacy && std::string(error)==legacy),"factored client flags change original validation precedence/messages");
    }
    for(int previous=2;previous<=6;++previous)for(int teams=2;teams<=6;++teams)for(unsigned active=0;active<(1u<<previous);++active){
        bool invalid=teams<previous && !(active&((1u<<teams)-1));
        require(bool(Allegiance::MissionTeamReductionError(previous,teams,[&](int side){return active&(1u<<side);}))==invalid,"team reduction permits all remaining factions to be inactive");
    }
    SessionOptions large;large.scenario=Scenario::Conquest;large.rules.miners=0;large.localPilots.resize(1024,{"Pilot",0,true});
    require(Game::missionError(large,{},true).empty(),"original validation reintroduces the old pilot-count cap");

    Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.civilizations(Game::civilizations(assets.root()));
    auto click=[&](std::string_view control){auto found=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& c){return c.control==control;});require(found!=ui.view().items.end(),"mission setup original control missing");return ui.activate(size_t(found-ui.view().items.begin()));};
    auto message=[&](std::string_view value){return std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.name=="messageText" && c.text==value;});};
    auto body=[&]()->const std::string&{auto found=std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.name=="missionBriefingTextBody";});require(found!=ui.view().artwork.end(),"original briefing body missing");return found->text;};
    SessionOptions options;options.scenario=Scenario::Conquest;options.map=2;options.rules.conquest=0;ui.networkSession(options);ui.open(Screen::Scenarios);ui.open(Screen::Lobby);
    click("createButtonPane");require(ui.screen()==Screen::Lobby && message(noGoals),"original setup accepts a mission with no goals or uses the wrong message");ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    options.rules.conquest=100;options.rules.teams=4;ui.networkSession(options);GameState lobby;lobby.networked=lobby.networkLobby=lobby.networkHost=true;lobby.teamScores.resize(4);lobby.teamScores[2].active=true;lobby.teamScores[3].active=true;
    ui.open(Screen::RemoteLobby,&lobby);ui.open(Screen::Lobby);click("teamCountComboPane");
    // ComboPaneImpl::SetSelection changes the face, not IMenu's independent
    // highlight. A fresh original menu starts on its first (two-team) row.
    ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
    click("createButtonPane");
    require(message("You can't reduce the number of teams if all remaining teams would be inactive."),"native host loses original inactive-team reduction guard");
    ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);require(ui.session().rules.teams==4,"rejected team reduction loses cancellation snapshot");

    options.rules.teams=2;options.rules.miners=0;options.rules.startCountdown=2;options.startInHangar=true;std::unique_ptr<Game> prepared;int preparations=0,discards=0;std::weak_ptr<const Allegiance::MissionBriefing> preparedBrief;
    ui.prepareSessionService([&](SessionOptions& session){++preparations;session.seed=47;prepared=std::make_unique<Game>(assets.root(),[](const auto&,const auto&){return ModelHandle{1,10};},0,0,session);auto briefing=prepared->missionBriefing(session.team);preparedBrief=briefing;return briefing;},[&]{++discards;prepared.reset();});
    ui.setPlaying(false);ui.networkSession(options);GameState offline;ui.open(Screen::LobbyPilots,&offline);click("sideReadyButtonPane");click("startButtonPane");
    require(prepared && preparations==1 && ui.screen()==Screen::Countdown && body().find(prepared->missionBriefing(0)->text)!=std::string::npos,"local briefing substitutes a map type for the actual prepared IGC home sector");
    auto frozen=prepared->state().time;ui.tick(.2);require(prepared->state().time==frozen && preparations==1,"countdown advances or recreates the prepared mission");
    ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);require(!prepared && discards==1 && preparedBrief.expired() && ui.screen()==Screen::LobbyPilots,"countdown cancellation retains its prepared mission or briefing");
    ui.prepareSessionService({},{});

    GameState storyState;storyState.networked=storyState.networkLobby=storyState.networkHost=storyState.networkReady=true;storyState.networkCountdown=9;storyState.missionStage=STAGE_STARTING;storyState.playerSide=0;storyState.teamScores.resize(2);for(auto& team:storyState.teamScores)team.active=true;
    auto text=std::string("Server story 100% literal: %s %n [Font=smallFont]\n")+std::string(4096,'S');
    storyState.briefing=std::make_shared<const Allegiance::MissionBriefing>(Allegiance::MissionBriefing{text,false});ui.networkSession(options);ui.open(Screen::RemoteLobby,&storyState);
    require(body()==text,"server-authored story is formatted, truncated or given extra generated objectives");
    storyState.briefing=std::make_shared<const Allegiance::MissionBriefing>(Allegiance::MissionBriefing{"Changed server story",false});ui.sync(storyState);require(body()=="Changed server story","late authoritative briefing does not update its already-open popup");
    storyState.networkCountdown=-1;storyState.missionStage=STAGE_NOTSTARTED;ui.sync(storyState);require(ui.wantsText() && std::none_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.name=="missionBriefingTextBody";}),"canceled remote countdown retains the briefing or loses lobby input");
    puts("Original mission setup: all seven goals, validation messages/precedence, uncapped seats, inactive-team reduction/cancel, actual frozen IGC home-sector preparation/release and literal/late server stories passed");
}
}
