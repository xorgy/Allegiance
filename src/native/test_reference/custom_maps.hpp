#pragma once
#include "../frontend.hpp"
#include "../test_fixture.hpp"
#include <sstream>
#include <chrono>
#include <fstream>
namespace NativeTest {
inline void checkMalformedMapPreviews(const std::filesystem::path& art){
    using namespace Native;
    auto root=std::filesystem::temp_directory_path()/("allegiance-map-preview-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(root);
    struct Cleanup {std::filesystem::path root;~Cleanup(){std::error_code error;std::filesystem::remove_all(root,error);}} cleanup{root};
    SessionOptions options;options.scenario=Scenario::Conquest;options.customMap="pv";
    std::vector<char> bytes;
    auto put=[&](ObjectType type,const auto& record){int size=sizeof(record);auto append=[&](const auto& value){auto data=reinterpret_cast<const char*>(&value);bytes.insert(bytes.end(),data,data+sizeof(value));};append(type);append(size);append(record);};
    auto preview=[&](int adjustment=0){std::ofstream file(root/"PV.IGC",std::ios::binary);int size=int(bytes.size())+adjustment;file.write(reinterpret_cast<const char*>(&size),sizeof(size));file.write(bytes.data(),bytes.size());file.close();return Game::mapPreview(options,root);};
    DataClusterIGC cluster{};cluster.clusterID=77;cluster.screenX=.25f;cluster.screenY=.75f;put(OT_cluster,cluster);
    auto valid=preview();require(valid.error.empty() && valid.sectors.size()==1 && valid.sectors[0].id==77 && valid.links.empty(),"file preview loses arbitrary sector IDs or rejects a single-sector/no-aleph map");
    require(!preview(1).error.empty(),"truncated IGC envelope accepted");bytes.pop_back();require(!preview().error.empty(),"truncated IGC record accepted");
    bytes.clear();put(OT_cluster,cluster);put(OT_cluster,cluster);require(!preview().error.empty(),"duplicate custom sector IDs accepted");
    bytes.clear();put(OT_cluster,cluster);DataStationIGC station{};station.clusterID=88;put(OT_station,station);require(!preview().error.empty(),"custom station with unknown sector accepted");
    bytes.clear();put(OT_cluster,cluster);DataWarpIGC warp{};warp.clusterID=77;warp.warpDef.warpID=10;warp.warpDef.destinationID=11;put(OT_warp,warp);require(!preview().error.empty(),"custom aleph with no destination accepted");
    Game::mapPreview(SessionOptions{},art); // Restore the original utility resource root.
}
inline void checkCustomMapUi(Native::Assets& assets){
    using namespace Native;
    checkMalformedMapPreviews(assets.root());
    std::istringstream source("2\r\n2,Example,MAP_A\r\n4,Four teams,map_b");auto parsed=Allegiance::ReadCustomMaps(source);
    require(parsed.size()==2 && Allegiance::FindCustomMap(parsed,"map_a")==&parsed[0],"original map catalog loses order/CRLF/names/case-insensitive asset identity");
    for(const char* bad:{"","-1\n","1\n","1\n2,Name,../core\n","1\n2,Name,abcdefghijklm\n","1\n7,Name,good\n","2\n2,A,same\n2,B,SAME\n","0\n2,Extra,file\n"}){
        bool rejected=false;try{std::istringstream input(bad);Allegiance::ReadCustomMaps(input);}catch(const std::exception&){rejected=true;}require(rejected,"invalid original map catalog metadata accepted");
    }
    auto maps=Game::customMaps(assets.root());require(!maps.empty(),"custom-map UI fixture requires the installed catalog");
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.civilizations(Game::civilizations(assets.root()));
    SessionOptions session;session.scenario=Scenario::Conquest;session.rules.startCountdown=1;ui.networkSession(session);ui.open(Screen::Scenarios);ui.open(Screen::Lobby);
    auto click=[&](std::string_view name){auto item=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& c){return c.control==name;});require(item!=ui.view().items.end(),"custom-map authored control missing");return ui.activate(size_t(item-ui.view().items.begin()));};
    {auto item=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& c){return c.control=="mapTypeComboPane";});require(item!=ui.view().items.end(),"authored map combo absent");auto at=glm::vec2(item->bounds)+glm::vec2(item->bounds.z,item->bounds.w)*.5f;glm::ivec2 size{2560,1440};auto pixel=originalMenuLayout(size,ui.view().classicScale,ui.view().canvas).pixel(at);ui.pointerMove(at,size);ui.pointerPress(menuHit(ui.view(),size,1,pixel));ui.pointerRelease(menuHit(ui.view(),size,1,pixel));require(ui.keyboardPopup() && ui.session().customMap.empty(),"opening a long map combo immediately selects an entry under its opening release");}
    ui.key(SDLK_END,SDL_SCANCODE_END);
    require(std::any_of(ui.view().items.begin(),ui.view().items.end(),[](const auto& item){return item.action.op==MenuOp::ScrollLine && item.action.value==2;}),"full original map catalog cannot scroll");
    ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);require(ui.session().customMap==maps.back().file && ui.session().map==0,"last catalog entry is unreachable or loses its filename");
    auto original=ui.session();ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);require(ui.session().customMap.empty(),"setup cancellation retains a custom-map draft");
    ui.open(Screen::Lobby);click("mapTypeComboPane");ui.key(SDLK_END,SDL_SCANCODE_END);ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
    auto select=[&](std::string_view name,int row){click(name);auto selected=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& item){return item.popupLevel>=0 && item.selected && item.action.op==MenuOp::SetupChoice;});require(selected!=ui.view().items.end(),"setup combo has no current row");int current=selected->action.delta;for(int i=0;i<current;++i)ui.key(SDLK_UP,SDL_SCANCODE_UP);for(int i=0;i<row;++i)ui.key(SDLK_DOWN,SDL_SCANCODE_DOWN);ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);};
    select("teamCountComboPane",maps.back().teams==2?1:0);click("createButtonPane");require(ui.screen()==Screen::Lobby && ui.hasMessageBox() && std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.name=="messageText" && c.text.find("can only be played with "+std::to_string(maps.back().teams)+" teams")!=std::string::npos;}),"custom-map team mismatch does not show original rejection and retain its draft");
    ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);select("teamCountComboPane",maps.back().teams-2);click("createButtonPane");
    require(ui.screen()==Screen::LobbyPilots && ui.session().customMap==maps.back().file,"valid custom map does not reach original team lobby");
    click("sideReadyButtonPane");click("startButtonPane");require(ui.screen()==Screen::Countdown && std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.name=="missionBriefingMapStr" && c.text==maps.back().name;}),"custom briefing substitutes a generated map name");
    auto preview=Game::mapPreview(ui.session(),assets.root());auto home=std::find_if(preview.sectors.begin(),preview.sectors.end(),[](const auto& s){return s.stations[0]>0;});require(home!=preview.sectors.end() && !home->name.empty() && std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.name=="missionBriefingTextBody" && c.text.find(home->name)!=std::string::npos;}),"custom briefing substitutes a generated map name for the original home sector");
    ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.open(Screen::Lobby);select("mapTypeComboPane",0);require(ui.session().customMap.empty(),"generated map selection retains the file-backed map");
    ui.networkSession(original);ui.open(Screen::Lobby);select("gameTypeComboPane",0);require(ui.session().customMap.empty(),"original game preset retains a custom-map override");
    ui.configuring();require(ui.hasMessageBox() && std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.name=="messageText" && c.text=="Changing mission parameters...";}),"remote setup does not use the original pending message");
    ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);require(ui.hasMessageBox(),"pending configuration accepts dismissal before the authoritative reply");
    ui.error("Rejected configuration");ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);require(!ui.hasMessageBox() && ui.screen()==Screen::Lobby,"configuration rejection fails to restore the setup parent");
    ui.configuring();ui.connectionFinished();require(!ui.hasMessageBox() && ui.screen()==Screen::Lobby,"successful configuration retains the pending modal");
    puts("Original custom-map UI: strict catalog/order, full menu scrolling, file selection, cancellation, original team-count rejection, lobby/briefing and generated/preset restoration passed");
}
}
