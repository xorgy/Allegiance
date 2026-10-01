#pragma once
#include "../frontend.hpp"
#include "../test_fixture.hpp"
#include <cctype>

namespace NativeTest {
inline void checkCoreIdentity(Native::Assets& assets){
    using namespace Native;
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.open(Screen::Games);
    auto action=[&](MenuOp op,int value=0){auto item=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& c){return c.action.op==op && c.action.value==value;});require(item!=ui.view().items.end(),"core identity authored action");return ui.activate(size_t(item-ui.view().items.begin()));};
    auto hasText=[&](std::string_view value){return std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.kind==OriginalControl::Kind::Text && c.text==value;});};
    std::vector<Net::ServerListing> games;
    for(auto name:{"wc_04","PCore014","zone_core","UnknownCore"}){
        Net::ServerInfo info{};SessionOptions options;options.scenario=Scenario::Conquest;info.session=Net::session(options);info.flags=Net::ServerWaiting|Net::ServerJoinable;
        Net::text(info.core,name);Net::text(info.name,name);games.push_back({std::string("core/")+name,info});
    }
    ui.gameListings(games);for(auto& game:games)require(hasText(assets.coreName(game.info.core)),"games list ignores the original configured core label");
    auto lower=[](std::string value){for(auto& c:value)c=char(std::tolower(static_cast<unsigned char>(c)));return value;};
    auto reference=games;std::stable_sort(reference.begin(),reference.end(),[&](const auto& a,const auto& b){return lower(assets.coreName(a.info.core))<lower(assets.coreName(b.info.core));});
    for(bool reverse:{false,true}){
        action(MenuOp::GameSort,5);std::vector<std::string> rows;
        for(const auto& item:ui.view().items)if(item.action.op==MenuOp::GameSelect && item.action.value>=0)rows.push_back(item.control);
        require(rows.size()==reference.size(),"core sorting drops game rows");
        for(size_t i=0;i<rows.size();++i)require(rows[i]=="game/"+reference[reverse?rows.size()-i-1:i].address,"core sort uses filenames instead of the displayed configured names");
    }
    action(MenuOp::GameCreate);require(hasText(assets.coreName(sessionCoreFile(Scenario::Conquest))),"Create Game advertises a different core from the mission it creates");
    action(MenuOp::GameCreateServer,0);action(MenuOp::GameCreateCore,0);
    action(MenuOp::GameCreateAccept);require(ui.session().scenario==Scenario::Conquest,"core identity changes the local creation mode");
    ui.open(Screen::LobbyPilots);
    require(hasText(assets.coreName(sessionCoreFile(ui.session().scenario))) && hasText("Local session"),"local original lobby labels the wrong core/server");
    puts("Original core identity: configured games-list labels and bidirectional sorting, actual creation core, local lobby and unchanged creation behavior passed");
}
}
