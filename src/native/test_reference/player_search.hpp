#pragma once
#include "../frontend.hpp"
#include "../test_fixture.hpp"

namespace NativeTest {
inline void checkPlayerSearch(Native::Assets& assets){
    using namespace Native;
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.open(Screen::Games);
    std::vector<Net::ServerListing> games;
    for(int i=0;i<30;++i){Net::ServerListing g;g.address="server-"+std::to_string(i);g.info.session=Net::session(SessionOptions{});g.info.flags=Net::ServerJoinable;g.info.players=unsigned(30-i);Net::text(g.info.name,"Game "+std::to_string(i));games.push_back(g);}
    ui.gameListings(games);
    auto item=[&](MenuOp op,int value=0){for(size_t i=0;i<ui.view().items.size();++i)if(ui.view().items[i].action.op==op && ui.view().items[i].action.value==value)return i;throw std::runtime_error("Find Player control absent");};
    ui.activate(item(MenuOp::FindPlayer));require(ui.keyboardPopup() && ui.wantsText(),"original Find dialog fails to capture input and focus its edit pane");
    auto controls=OriginalUi(assets).layout("gamescreen","FindDialog");
    for(const auto& c:controls)if(c.name=="findPlayerEditPane" || c.name=="findOkButtonPane" || c.name=="findCancelButtonPane"){
        auto actual=std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& v){return v.name==c.name;});
        require(actual!=ui.view().artwork.end() && actual->bounds.z==c.bounds.z && actual->bounds.w==c.bounds.w && actual->font==c.font,"Find Player ignores authored geometry/font");
    }
    require(ui.view().items[item(MenuOp::GameJoin)].blocked,"Find Player leaves its game list actions active");
    ui.text("Alphé");ui.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);require(ui.playerSearchName()=="Alph","Find Player Backspace splits a UTF-8 character");ui.text("a");
    ui.pointerMove({0,0});ui.pointerPress({});require(ui.keyboardPopup(),"outside click dismisses the original protected Find dialog");
    ui.activate(item(MenuOp::FindPlayerCancel));require(!ui.keyboardPopup() && !ui.wantsText(),"Find cancel retains its edit capture");
    ui.activate(item(MenuOp::FindPlayer));require(ui.playerSearchName()=="Alpha","reopening the original Find dialog loses its retained name");
    auto submit=ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);require(submit.effect==FrontEffect::FindPlayer && ui.hasMessageBox() && !ui.wantsText(),"Find Enter fails to send the name and show a pending lookup");
    require(ui.waitCursor(),"pending lookup does not request the original wait cursor");
    require(std::any_of(ui.messageView().artwork.begin(),ui.messageView().artwork.end(),[](const auto& c){return c.text=="Asking server to find player....";}),"Find lookup loses the original pending caption");
    for(auto key:{SDLK_ESCAPE,SDLK_RETURN,SDLK_TAB})require(ui.key(key,SDL_GetScancodeFromKey(key,nullptr)).effect==FrontEffect::None && ui.hasMessageBox(),"pending lookup allows dismissal or list navigation");
    ui.gameListings(games);ui.foundPlayer(games.back());require(ui.hasMessageBox() && !ui.waitCursor(),"found lookup lacks its original acknowledgment or retains the wait cursor");
    require(std::any_of(ui.messageView().artwork.begin(),ui.messageView().artwork.end(),[](const auto& c){return c.text=="Player found!";}),"found lookup caption differs from WinTrek");
    ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);require(ui.activate(item(MenuOp::GameJoin)).effect==FrontEffect::Connect && ui.serverAddress()==games.back().address,"found lookup does not reveal/select the matching game for ordinary joining");
    ui.open(Screen::Games);ui.activate(item(MenuOp::GameFilter));ui.text("Unrelated filter");ui.activate(item(MenuOp::GameFilterAccept));
    ui.activate(item(MenuOp::FindPlayer));ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);ui.foundPlayer(games.back());ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
    require(ui.activate(item(MenuOp::GameJoin)).effect==FrontEffect::Connect && ui.serverAddress()==games.back().address,"found lookup fails to reset the original game filters");
    ui.open(Screen::Games);ui.activate(item(MenuOp::FindPlayer));ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);ui.foundPlayer({});
    require(std::any_of(ui.messageView().artwork.begin(),ui.messageView().artwork.end(),[](const auto& c){return c.text=="The player was not found.";}),"failed lookup loses its original message");ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
    require(ui.activate(item(MenuOp::GameJoin)).effect==FrontEffect::Connect && ui.serverAddress()==games.back().address,"failed lookup discards the previous selected game");
    ui.open(Screen::Games);ui.activate(item(MenuOp::FindPlayer));ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);ui.open(Screen::Home);ui.foundPlayer(games.back());
    require(!ui.hasMessageBox() && ui.screen()==Screen::Home,"late lookup result reopens a retired Games screen");
    puts("Original Find Player: authored dialog, protected input/focus, retained name, UTF-8 edit, pending captions/capture, acknowledgment, filter reset, selected/revealed join target, failed-search retention and retired-screen immunity passed.");
}
}
