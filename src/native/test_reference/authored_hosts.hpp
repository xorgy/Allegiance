#pragma once
#include "../crew_pane.hpp"
#include "../game_state_pane.hpp"
#include "../test_fixture.hpp"
#include <chrono>
#include <fstream>

namespace NativeTest {
// Real resources, independently changed MDL declarations. File symlinks avoid
// copying artwork; every override is unlinked before writing, never changing
// the installed game data.
struct AuthoredAssets {
    std::filesystem::path root;
    explicit AuthoredAssets(const std::filesystem::path& source) {
        root=std::filesystem::temp_directory_path()/("allegiance-hosts-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root);
        for(auto& entry:std::filesystem::recursive_directory_iterator(source))if(entry.is_regular_file()){
            auto target=root/std::filesystem::relative(entry.path(),source);std::filesystem::create_directories(target.parent_path());std::filesystem::create_symlink(entry.path(),target);
        }
    }
    ~AuthoredAssets(){std::error_code error;std::filesystem::remove_all(root,error);}
    void change(std::string_view file,std::initializer_list<std::pair<std::string_view,std::string_view>> substitutions) {
        auto path=root/file;std::ifstream input(path,std::ios::binary);std::string text((std::istreambuf_iterator<char>(input)),{});input.close();
        for(auto [before,after]:substitutions){auto at=text.find(before);require(at!=text.npos,"Alternate asset substitution missing");do{text.replace(at,before.size(),after);at=text.find(before,at+after.size());}while(at!=text.npos);}
        std::filesystem::remove(path);std::ofstream output(path,std::ios::binary);output<<text;require(bool(output),"Alternate MDL write failed");
    }
};
inline void checkAuthoredHosts(Native::Assets& installed) {
    using namespace Native;
    AuthoredAssets fixture(installed.root());
    fixture.change("inputmappane.mdl",{{"mappingOffset  = 196","mappingOffset  = 171"},{"Point(434, 182)","Point(374, 102)"},{"Point(12, 185)","Point(11, 105)"},{"Point( 36,  38)","Point( 46,  48)"},{"Point(492,  40)","Point(422,  50)"},{"textColor      = Color(1, 1, 1)","textColor      = Color(.2, .4, .6)"},{"selectionColor = Color(1, 0, 0)","selectionColor = Color(.4, .2, .1)"}});
    fixture.change("teamscreen.mdl",{{"ChatListPane(407, 123, 1, true)","ChatListPane(317, 99, 3, false)"},{"Point(270, 408)","Point(280, 410)"},{"[110, 138, 163, 249, 319, 398]","[90, 118, 143, 219, 289, 368]"},{"Point(398, 305)","Point(368, 205)"},{"Point(14, 303)","Point(13, 203)"},{"Point(268,  71)","Point(278,  81)"},{"Point(220, 308)","Point(220, 280)"},{"Point( 45,  85)","Point( 55,  95)"}});
    fixture.change("gamescreen.mdl",{{"[20,140,220,270,320]","[18,128,198,248,298]"},{"[20,110]","[18,98]"},{"Point(320, 110)","Point(298, 90)"},{"Point(110, 110)","Point(98, 90)"},{"serverScrollBar     =","alternateServerBar  ="},{"Point(298, 90), serverScrollBar","Point(298, 90), alternateServerBar"},{"(serverScrollBar ,","(alternateServerBar ,"}});
    fixture.change("gameoverscreen.mdl",{{"ChatListPane(591, 78, 1, true)","ChatListPane(491, 58, 2, true)"},{"Point(570, 168)","Point(560, 98)"},{"Point( 91,  83)","Point(101,  93)"},{"Point(16, 169)","Point(15, 99)"},{"[28, 119, 198, 251, 323, 371, 434, 482, 538, 570]","[28, 109, 188, 241, 313, 361, 424, 472, 528, 560]"},{"Point(571, 86)","Point(571, 42)"},{"Point( 91, 316)","Point(111, 326)"}});
    fixture.change("missionbrief.mdl",{{"Rect(0, 0, 73, 68)","Rect(0, 0, 63, 58)"},{"Point(8,160)","Point(18,150)"}});
    fixture.change("creditspane.mdl",{{"paneSizeY = 254","paneSizeY = 214"},{"paneSizeX = 475","paneSizeX = 425"},{"Multiply(10, creditsTime)","Multiply(17, creditsTime)"},{"Point( 20, 38)","Point( 30, 48)"}});
    fixture.change("dialog.mdl",{{"Point(-180, -75)","Point(-195, -85)"},{"Point(-362, 20)","Point(-342, 30)"},{"GameStateContainer(smallFont, smallFont, gameStateCloseButton)","GameStateContainer(Win32Font(\"Verdana\",18,true,0), largeFont, gameStateCloseButton)"},{"ChatListPane(355,","ChatListPane(325,"},{"CuttingRect                       = Rect(0, 0, 355, 125)","CuttingRect                       = Rect(0, 0, 315, 115)"}});
    Assets assets(fixture.root);OriginalUi definitions(assets);Settings settings;Frontend ui(settings);ui.originalInterface(assets);
    auto control=[&](std::string_view name)->const OriginalControl&{return authoredControl(ui.view().artwork,name);};
    auto action=[&](MenuOp op)->const MenuItem&{auto& items=ui.view().items;auto i=std::find_if(items.begin(),items.end(),[&](auto& item){return item.action.op==op;});require(i!=items.end(),"Alternate asset action missing");return *i;};
    ui.open(Screen::Controls);auto list=control("listPane"),bar=control("scrollBar");
    require(list.bounds.z==374 && list.bounds.w==102 && bar.bounds.z==11 && bar.bounds.w==105,"Mappings duplicate authored list/bar extents");
    require(action(MenuOp::MappingSelect).bounds.x==list.bounds.x && action(MenuOp::MappingSelect).bounds.y==list.bounds.y,"Mapping rows ignore authored position");
    auto text=std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[&](auto& c){return c.kind==OriginalControl::Kind::Text && c.bounds.x==list.bounds.x+171;});
    require(text!=ui.view().artwork.end() && text->font=="smallFont" && text->color==glm::vec4(.2,.4,.6,1),"Mapping split/colors or original caller-selected font differ");
    ui.activate(size_t(&action(MenuOp::MappingSelect)-ui.view().items.data()));
    require(control("mapping0").color==glm::vec4(.4,.2,.1,1),"Mapping selection ignores authored color");
    ui.open(Screen::Games);auto& create=action(MenuOp::GameCreate);ui.activate(size_t(&create-ui.view().items.data()));
    auto server=control("serverListPane"),core=control("coreListPane");require(server.scrollBar=="alternateServerBar" && server.bounds.z==298 && core.bounds.z==98,"Create hosts ignore authored sizes/scrollbar references");
    require(control("serverListHeader1").text=="NAME" && control("serverListHeader1").bounds.z==110 && control("coreListHeader1").bounds.z==80,"Create text headers duplicate authored labels/columns");
    auto local=std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[](auto& c){return c.text=="Local game";});require(local!=ui.view().artwork.end() && local->bounds.x==server.bounds.x+19 && local->bounds.y==server.bounds.y+1,"Create row duplicates authored origin/columns");
    ui.open(Screen::Home);GameState lobby;lobby.docked=lobby.networked=lobby.networkLobby=lobby.networkHost=lobby.commander=true;lobby.networkHostPilot=7;
    lobby.networkPilots={{7,0,"Commander",true,true,true,true,false},{8,0,"Peer",true,false,false,false,false}};
    ui.setPlaying(true);ui.open(Screen::RemoteLobby,&lobby);auto roster=control("playerListPane");
    ui.activate(size_t(&action(MenuOp::LobbyChatChannel)-ui.view().items.data()));
    require(action(MenuOp::RosterSelect).bounds.x==roster.bounds.x && action(MenuOp::RosterSelect).bounds.y==roster.bounds.y && roster.bounds.z==368,"Lobby rows duplicate authored geometry");
    auto chatHost=control("chatText");require(chatHost.bounds==glm::vec4(280,410,317,99) && chatHost.chatFont==3 && !chatHost.showScrollBar,"Lobby chat descriptor discards authored parameters");
    GameMessage chat;chat.chat=chat.fromPlayer=true;chat.serial=1;chat.chatTarget=CHAT_EVERYONE;chat.text="A message spanning many lines in the altered viewport.";lobby.messages.push_back(chat);ui.sync(lobby);
    require(control("lobbyChat/0").font=="hugeFont" && control("lobbyChat/0").bounds.x==283,"Lobby chat ignores authored font/position");
    require(std::none_of(ui.view().items.begin(),ui.view().items.end(),[](auto& item){return item.action.op==MenuOp::ScrollThumb && item.action.value==-6;}),"Hidden authored chat scrollbar still accepts input");
    require(action(MenuOp::SelectTeam).bounds.x==55 && action(MenuOp::SelectTeam).bounds.y==95 && action(MenuOp::SelectTeam).bounds.w==40,"Team seats duplicate authored origin/height");
    auto rules=ui.session();rules.scenario=Scenario::Conquest;ui.networkSession(rules);float winMoney{};auto civilizations=Game::civilizations(installed.root(),&winMoney);ui.civilizations(civilizations,winMoney);
    auto briefing=lobby;briefing.networkCountdown=5;briefing.missionStage=STAGE_STARTING;briefing.networkReady=true;briefing.teamScores.resize(2);for(int side=0;side<2;++side){briefing.teamScores[side].civilization=civilizations[side].id;briefing.teamScores[side].civilizationIcon=civilizations[side].icon;briefing.teamScores[side].name="Test team";}
    ui.open(Screen::RemoteLobby,&briefing);auto briefingBackdrop=authoredControlBounds(ui.view().artwork,"MissionBriefDialog");auto emblem=control("missionBriefingTeamImage");
    require(emblem.visibleBounds().x==briefingBackdrop.x+18 && emblem.visibleBounds().y==briefingBackdrop.y+150 && emblem.clip && emblem.clip->z==63 && emblem.clip->w==58,"Briefing icon duplicates authored position/clip");
    auto result=lobby;result.complete=true;result.endgame.frozen=true;result.endgame.winner=0;Net::ResultPlayer pilot{};pilot.id=7;pilot.side=0;Net::text(pilot.name,"Winner");result.endgame.players.push_back(pilot);Net::ResultTeam team{};team.id=0;Net::text(team.name,"Green");result.endgame.teams.push_back(team);
    ui.open(Screen::Results,&result);auto player=action(MenuOp::ResultsSelect);require(player.bounds==glm::vec4(101,93,560,14),"Results player rows duplicate authored position/width");
    require(control("resultPlayer/7/32").bounds.x==133 && control("resultPlayer/7/32").bounds.z==77,"Results columns remain native constants");
    require(control("resultTeam/0/55").bounds.x==166 && control("resultTeam/0/55").bounds.y==326,"Results team list ignores authored geometry");
    require(control("resultChat/0").font=="largeFont" && control("chatText").faceSize==glm::vec2(491,58),"Results chat ignores authored font/extent");
    GameState crew;crew.passenger=true;crew.turret=0;crew.sourcePilot="Parent pilot";CrewPane pane(assets);auto& flight=pane.view({1920,1080},1,crew,false,true);
    require(authoredControl(flight.artwork,"TurretHeader").bounds.x==1725 && authoredControl(flight.artwork,"TurretHeader").bounds.y==85,"Crew flight anchor duplicates the overlay table");
    auto& loadout=pane.view({1920,1080},1,crew,true,true);require(authoredControl(loadout.artwork,"BoardTextImage").bounds.x==618 && authoredControl(loadout.artwork,"BoardTextImage").bounds.y==510,"Crew loadout anchor duplicates the overlay table");
    ui.setPlaying(false);ui.open(Screen::Credits);auto scroll=*ui.view().scroll;auto backdrop=std::find_if(ui.view().artwork.rbegin(),ui.view().artwork.rend(),[](auto& c){return c.name=="screen";})->bounds;
    require(scroll.bounds==glm::vec4(backdrop.x+30,backdrop.y+48,425,214),"Credits duplicate authored viewport size/position");
    HudValues values;values[HudNumber::UiTime]=2;float distance=std::max(0.f,definitions.imageSize("creditstext").y-214);
    require(std::abs(scroll.offset->evaluate(values)-(std::fmod(34.f,distance)-distance))<.001f,"Credits motion duplicates authored animation expression");
    auto title=definitions.font("dialog","gameStateContainer",0),clock=definitions.font("dialog","gameStateContainer",1);
    require(title.font=="Verdana" && title.fontHeight==18 && title.fontBold && clock.font=="largeFont","Game State discards authored font arguments");
    SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.rules.miners=0;Game game(installed.root(),[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
    GameStatePane scores(assets);scores.toggle(game);auto& scoreView=scores.view({1920,1080},1);
    require(authoredControl(scoreView.artwork,"gameStateDescription/0").font=="Verdana" && authoredControl(scoreView.artwork,"gameStateDescription/0").fontHeight==18 && authoredControl(scoreView.artwork,"gameStateClock").font=="largeFont","Game State painter ignores its compiled font descriptors");
    puts("Alternate MDL hosts: mappings, Create headers/lists, lobby teams/roster/chat, results tables/chat, crew, credits animation and Game State fonts passed");
}
}
