#pragma once
#include "../frontend.hpp"
#include "authored_hosts.hpp"
#include "../test_fixture.hpp"
#include "../../ClientCore/SavedWing.h"
#include <fstream>

namespace NativeTest {
inline void checkWingControls(Native::Assets& assets) {
    using namespace Native;
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.setPlaying(true);SessionOptions options;options.scenario=Scenario::Conquest;ui.networkSession(options);
    GameState state;state.networked=state.networkLobby=state.networkHost=state.commander=true;state.playerSide=0;state.shipId=40;
    state.networkPilots={{40,0,"Commander",true,true,true,true,false,noTeamRequest,4},{50,1,"Opponent",true,false,true,false,false,noTeamRequest,-1}};
    ui.open(Screen::RemoteLobby,&state);
    auto item=[&](MenuOp op,int value=-1){const auto& list=ui.view().items;auto found=std::find_if(list.begin(),list.end(),[&](const auto& c){return c.action.op==op && (value<0 || c.action.value==value);});require(found!=list.end(),"Original wing control or choice missing");return size_t(found-list.begin());};
    auto combo=authoredControl(ui.view().artwork,"wingComboPane");
    auto caption=[&](std::string_view wanted){
        bool image=combo.widget==OriginalControl::Widget::ImageCombo;
        auto bounds=image?combo.visibleBounds():glm::vec4(glm::vec2(combo.bounds),combo.faceSize);
        bool found=std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.kind==OriginalControl::Kind::Text && !c.text.empty() && (image || c.text==wanted) && c.font==combo.font && c.visibleBounds()==bounds;});
        require(found!=image,"Wing selector overwrites a bitmap label or loses an authored text face");
    };
    caption(Game::wingName(4));ui.pointerPress(item(MenuOp::LobbyWing));require(!ui.wantsText() && ui.keyboardPopup(),"Wing menu does not capture typing");
    for(int wing=0;wing<10;++wing)require(ui.view().items[item(MenuOp::LobbyWingChoice,wing)].label==Game::wingName(wing),"Wing menu omits or renames an original wing");
    ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);require(ui.wantsText(),"Wing menu Escape loses the lobby chat focus");caption(Game::wingName(4));
    ui.pointerPress(item(MenuOp::LobbyWing));auto result=ui.pointerRelease(item(MenuOp::LobbyWingChoice,7));
    require(result.effect==FrontEffect::Wing && result.value==40 && result.command==7,"Lobby combo fails drag/release or assigns the selected roster pilot instead of self");
    state.networkPilots[0].wing=7;ui.sync(state);caption(Game::wingName(7));
    ui.pointerPress(item(MenuOp::LobbyWing));state.networkPilots[0].team=noTeam;state.playerSide=noTeam;state.commander=false;ui.sync(state);
    require(std::none_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.name=="wingComboPane";}) && !ui.keyboardPopup(),"Leaving the faction retains a private wing control/menu");
    state.networkPilots[0].team=0;state.playerSide=0;ui.open(Screen::RemoteLobby,&state);
    const auto& controls=ui.view().items;auto other=std::find_if(controls.begin(),controls.end(),[](const auto& c){return c.control=="team1";});require(other!=controls.end(),"Opponent team tab missing");ui.activate(size_t(other-controls.begin()));
    require(std::none_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.name=="wingComboPane";}),"Opponent roster exposes own wing combo");
}
inline void checkWings(Native::Assets& assets) {
    using namespace Native;
    static_assert(c_widMax==Allegiance::WingCount);
    Allegiance::WingEntry entry;
    require(!entry.enter(10,noTeam,false,7),"No Team applies a private wing preference");
    require(entry.enter(10,0,false,7)==7,"Faction entry fails to restore the saved wing");
    require(!entry.enter(10,0,false,4),"A later report stomps a commander's wing assignment");
    require(!entry.enter(10,1,true,7),"New commander preference overrides Command wing");
    require(!entry.enter(10,1,false,7),"Losing command unsolicited replays a preference");
    require(entry.enter(11,1,false,-1)==1,"Explicit original NA preference fails to choose Attack");
    entry.reset();require(entry.enter(11,1,false,0)==0,"New match fails to restore the original default preference");
    Settings settings;require(settings.defaultWing==0,"Absent original wing preference defaults incorrectly");
    auto path=std::filesystem::temp_directory_path()/("allegiance-wing-settings-"+std::to_string(getpid()));
    for(int wing=-1;wing<10;++wing){settings.defaultWing=wing;settings.save(path);require(Settings::load(path).defaultWing==wing,"Wing preference does not survive saving");}
    auto original=path;original+=".json";
    for(auto [json,wanted]:std::initializer_list<std::pair<const char*,int>>{{"{}",0},{"{\"WingAssignment\":3}",3},{"{\"WingAssignment\":3,\"Ui.DefaultWing\":7}",7},{"{\"Ui.DefaultWing\":-1}",-1},{"{\"Ui.DefaultWing\":\"9\"}",9},{"{\"Ui.DefaultWing\":999}",0},{"{\"Ui.DefaultWing\":2.5}",0}}){
        {std::ofstream file(original);file<<json;}Settings imported;imported.importOriginal(original);require(imported.defaultWing==wanted,"Original wing migration precedence or invalid-value fallback differs");
    }
    std::filesystem::remove(path);std::filesystem::remove(original);
    checkWingControls(assets);
    AuthoredAssets fixture(assets.root());
    fixture.change("teamscreen.mdl",{{"ImageComboPane(ImportImage(\"btnchangewingbmp\", true), largeFont)","StringComboPane(Point(88, 19), Point(88, 19), smallFont, Color(.7, .8, .9))"},{"Point(695, 193)","Point(673, 183)"}});
    Assets alternate(fixture.root);checkWingControls(alternate);
    puts("Original wings: preference migration/persistence, one-time entry, commander policy, ten authored choices, bitmap/text faces, alternate MDL geometry/font, drag/release, Escape and No Team/opponent visibility passed");
}
}
