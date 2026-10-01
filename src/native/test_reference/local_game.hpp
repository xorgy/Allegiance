#pragma once
#include "../frontend.hpp"
#include "../test_fixture.hpp"

namespace NativeTest {
inline void checkOriginalLocalGame(Native::Assets& assets){
    using namespace Native;
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);
    auto civs=Game::civilizations(assets.root());require(civs.size()>=2,"local game needs two factions");ui.civilizations(civs);
    auto item=[&](std::string_view name){auto i=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& c){return c.control==name;});require(i!=ui.view().items.end(),"local game authored control missing");return size_t(i-ui.view().items.begin());};
    auto click=[&](std::string_view name){return ui.activate(item(name));};
    auto hasText=[&](std::string_view value){return std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.kind==OriginalControl::Kind::Text && c.text==value;});};
    auto prompt=[&](std::string_view value){return std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.name=="textPrompt" && c.text.find(value)!=c.text.npos;});};
    auto eraseEdit=[&](std::string_view name){for(;;){auto face=std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.name==name && c.widget==OriginalControl::Widget::Edit;});require(face!=ui.view().artwork.end(),"local game edit face missing");auto bounds=face->bounds;
        auto c=std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.bounds==bounds && c.textMode==OriginalControl::TextMode::Edit;});require(c!=ui.view().artwork.end(),"local game edit missing");if(c->text.empty())break;ui.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);}};

    // Small or noncontiguous faction sets must resolve indices, not core IDs,
    // even when the human seat begins above the number of civilizations.
    SessionOptions relative;relative.scenario=Scenario::Conquest;relative.rules.teams=6;relative.team=5;
    for(int count:{1,2,4})for(int side=0;side<6;++side){int index=teamCivilizationIndex(relative,side,count);require(index>=0 && index<count,"default faction produces a negative index");}
    auto distinct=civs;distinct.resize(2);distinct[0].id=120;distinct[1].id=240;
    ui.civilizations(distinct);ui.networkSession(relative);ui.open(Screen::LobbyPilots);
    require(hasText(distinct[0].name),"offline faction label mistakes its list index for a core identity");
    click("team0");click("joinButtonPane");
    require(ui.session().team==0 && ui.session().teamCivilizations[5]==0 && ui.session().teamCivilizations[0]==1 && hasText(distinct[1].name),"changing seats silently rotates the default factions");

    ui.civilizations(civs);ui.open(Screen::Games);ui.activate(item("newGameButtonPane"));click("create/server/0");click("create/core/0");eraseEdit("createEditPane");ui.text(std::string(64,'M'));click("createOkButtonPane");
    require(ui.screen()==Screen::Lobby && Game::missionError(ui.session()).empty(),"accepted 64-byte game name cannot pass actual mission validation");
    auto tooLong=ui.session();tooLong.name.push_back('M');require(!Game::missionError(tooLong).empty(),"mission accepts an unrepresentable game name");
    auto options=ui.session();options.startInHangar=true;options.rules.miners=0;options.localPilots={{"Friendly seat",0,false},{"Opposing seat",1,true}};ui.networkSession(options);
    click("createButtonPane");require(ui.screen()==Screen::LobbyPilots,"local setup bypasses the original team screen");
    require(hasText(civs[0].name) && ui.view().items[item("awayFromKeyboardButtonPane")].selected,"local lobby loses faction or personal AFK state");
    click("awayFromKeyboardButtonPane");require(!ui.view().items[item("awayFromKeyboardButtonPane")].selected && !ui.view().items[item("startButtonPane")].enabled,"personal readiness overrides an unready seat");
    require(click("startButtonPane").effect==FrontEffect::None && ui.screen()==Screen::LobbyPilots,"disabled local Start launches the game");
    click("awayFromKeyboardButtonPane");click("sideReadyButtonPane");
    require(ui.view().items[item("sideReadyButtonPane")].selected && !ui.view().items[item("awayFromKeyboardButtonPane")].selected && ui.view().items[item("startButtonPane")].enabled,"Team Ready fails to clear the commander's AFK state while overriding other seats");
    auto check=assets.image("lobbycheckbmp",true);
    require(std::count_if(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.image==check;})>=2,"local owner/commander role checks are absent");

    auto factions=ui.session().teamCivilizations;click("team1");click("joinButtonPane");
    require(ui.session().team==1 && ui.session().teamCivilizations==factions && !ui.view().items[item("awayFromKeyboardButtonPane")].selected && hasText(civs[1].name),"original team join loses readiness, faction identity or label");
    click("teamSettingsButtonPane");eraseEdit("teamNameEditPane");ui.text("Known complete crew");ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
    require(ui.session().teamNames[1]=="Known complete crew" && prompt("KNOWN COMPLETE CREW") && !ui.view().items[item("awayFromKeyboardButtonPane")].selected,"accepted local team name is missing from the prompt or makes its owner AFK");
    click("team0");click("joinButtonPane");require(ui.session().teamCivilizations==factions,"joining back changes configured factions");
    click("settingsButtonPane");click("friendlyFireCheckboxPane");click("cancelButtonPane");
    require(ui.screen()==Screen::LobbyPilots && !ui.session().rules.friendlyFire && ui.view().items[item("sideReadyButtonPane")].selected,"Cancel commits mission changes or loses its original team parent/readiness");

    click("startButtonPane");require(ui.screen()==Screen::Countdown && !ui.wantsText(),"local countdown fails to isolate original briefing");
    ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    require(ui.screen()==Screen::LobbyPilots && ui.wantsText() && ui.tick(60).effect==FrontEffect::None,"countdown Cancel returns to settings, loses chat focus or later launches");
    click("startButtonPane");require(ui.tick(60).effect==FrontEffect::Start && ui.tick(60).effect==FrontEffect::None,"local countdown emits no start or repeats it");
    options=ui.session();Game game(assets.root(),[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
    require(game.sessionOptions().name==std::string(64,'M') && game.state().networkPilots.size()==3 && game.state().pilots.size()==2 && game.state().docked,"full-name local game fails to create actual seats or mixes its public roster with private account rows");
    for(int side=0;side<2;++side)require(game.core()->GetSide(SideID(side))->GetCivilization()->GetObjectID()==civs[factions[side]].id,"lobby and actual mission disagree about factions");
    ui.setPlaying(true);ui.open(Screen::Pilots,&game.state());click("team1");
    require(hasText("Opposing seat") && hasText("Playing") && !ui.view().items[item("awayFromKeyboardButtonPane")].selected,"offline opponent public roster is absent or in-progress Away is checked");
    click("backButtonPane");
    require(ui.hasMessageBox() && ui.screen()==Screen::Pilots && hasText("Quit the Current Game?"),"local team Back skips original quit confirmation or replaces its parent");
    ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);require(!ui.hasMessageBox() && ui.screen()==Screen::Pilots,"quit Cancel loses local team screen");
    click("backButtonPane");auto yes=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& i){return i.action.op==MenuOp::MessageDismiss && i.action.delta;});require(yes!=ui.view().items.end(),"original quit confirmation has no accept action");
    require(ui.activate(size_t(yes-ui.view().items.begin())).effect==FrontEffect::Leave,"local quit acceptance does not leave");

    auto retired=game.state();retired.networked=retired.networkLobby=retired.networkHost=true;retired.networkPilots={{71,0,"Retired pilot",true,true,true,true,false}};
    for(auto& side:retired.teamScores){side.active=false;side.name="Retired team";}
    ui.open(Screen::RemoteLobby,&retired);ui.setPlaying(false);ui.open(Screen::Games);click("newGameButtonPane");click("create/server/0");click("create/core/0");click("createOkButtonPane");click("createButtonPane");
    require(ui.screen()==Screen::LobbyPilots && !hasText("RETIRED TEAM") && !prompt("RETIRED TEAM") && ui.view().items[item("sideReadyButtonPane")].enabled && ui.view().items[item("awayFromKeyboardButtonPane")].selected,"new local creation inherits retired team names, activity or roles");
    click("backButtonPane");require(ui.hasMessageBox() && ui.screen()==Screen::LobbyPilots,"pregame Back bypasses original confirmation");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);require(ui.wantsText(),"pregame quit Cancel loses chat focus");
    puts("Original local game: full-name mission start, noncontiguous faction IDs, stable seat changes, role checks, AFK/team readiness, team names, settings/countdown cancellation, actual IGC seats and quit/stale-session handling passed");
}
}
