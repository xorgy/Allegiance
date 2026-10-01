#pragma once
#include "../frontend.hpp"
#include "../test_fixture.hpp"

namespace NativeTest {
inline void checkBrowseIdentity(Native::Assets& assets){
    using namespace Native;
    std::vector<Net::ServerListing> many;
    for(unsigned alias=0;alias<5;++alias)for(unsigned id=1;id<=1024;++id){
        Net::ServerListing row;row.info.instance={id,11,22,33};row.address="route-"+std::to_string(alias)+"/"+std::to_string(id);row.observedAt=alias;
        row.info.players=alias;many.push_back(std::move(row));
    }
    auto compact=Net::coalesceGames(std::move(many));require(compact.size()==1024,"alias coalescing loses or duplicates distinct missions");
    for(unsigned i=0;i<compact.size();++i)require(compact[i].info.instance[0]==i+1 && compact[i].info.players==4 && compact[i].address=="route-4/"+std::to_string(i+1),"alias coalescing loses stable order or newest metadata/route");
    Net::ServerListing anonymous;anonymous.address="anonymous";
    require(Net::coalesceGames({anonymous,anonymous}).size()==1,"anonymous local fixture address fallback");

    Net::ServerListing game;game.address="127.0.0.1:27500";game.info.instance={7,8,9,10};game.info.session=Net::session(SessionOptions{});
    game.info.flags=Net::ServerJoinable;Net::text(game.info.name,"Identity test");Net::text(game.info.core,"static_core");
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.open(Screen::Games);ui.gameListings({game});
    auto item=[&](MenuOp op,int value=0){auto i=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& row){return row.action.op==op && row.action.value==value;});require(i!=ui.view().items.end(),"identity test original UI action missing");return size_t(i-ui.view().items.begin());};
    auto enabled=[&](MenuOp op){return ui.view().items[item(op)].enabled;};
    auto count=[&](MenuOp op){return std::count_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& row){return row.action.op==op;});};
    ui.activate(item(MenuOp::GameSelect));auto alias=game;alias.address="[::1]:27500";alias.observedAt=1;
    ui.gameListings({game,alias});require(count(MenuOp::GameSelect)==1 && enabled(MenuOp::GameJoin),"Games aliases duplicate a row or lose its selected mission");
    require(ui.activate(item(MenuOp::GameJoin)).effect==FrontEffect::Connect && ui.serverAddress()==alias.address && ui.serverInstance()==game.info.instance,"Games selected mission retains a retired route or loses join identity");
    auto replacement=alias;replacement.info.instance[0]++;
    replacement.observedAt=2;auto reused=Net::coalesceGames({alias,replacement});require(reused.size()==1 && reused[0].info.instance==replacement.info.instance,"new mission reply retains the stale identity at a reused listener");
    ui.gameListings({replacement});require(!enabled(MenuOp::GameJoin),"same listener restarting a different mission inherits the old selection");
    ui.activate(item(MenuOp::GameSelect));ui.gameListings({});ui.gameListings({replacement});require(!enabled(MenuOp::GameJoin),"removed Games selection resurrects when the mission returns");

    ui.activate(item(MenuOp::FindPlayer));ui.text("Pilot");ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
    auto found=replacement;found.address="127.0.0.2:27500";found.observedAt=2;ui.foundPlayer(found);ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
    require(count(MenuOp::GameSelect)==1 && enabled(MenuOp::GameJoin) && ui.activate(item(MenuOp::GameJoin)).effect==FrontEffect::Connect && ui.serverAddress()==found.address,"Find Player duplicates an aliased mission or selects its old route");

    ui.open(Screen::Online);ui.gameListings({game});ui.activate(item(MenuOp::LanSelect));ui.gameListings({alias});
    require(enabled(MenuOp::LanJoin),"LAN refresh loses selected mission across routes");ui.activate(item(MenuOp::LanJoin));require(ui.serverAddress()==alias.address && ui.serverInstance()==game.info.instance,"LAN joins the retired mission route or loses callsign-dialog join identity");
    ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.open(Screen::Online);ui.gameListings({game});ui.activate(item(MenuOp::LanSelect));ui.gameListings({replacement});
    require(!enabled(MenuOp::LanJoin),"LAN listener reuse inherits selected mission");

    Net::AuthorityListing authority;authority.address=game.address;authority.info.instance={21,22,23,24};authority.info.count=1;authority.info.capacity=16;authority.cores={"static_core"};Net::text(authority.info.server,"Identity server");
    ui.open(Screen::Games);ui.gameAuthorities({authority});ui.activate(item(MenuOp::GameCreate));ui.activate(item(MenuOp::GameCreateServer,1));ui.activate(item(MenuOp::GameCreateCore));ui.text(" retained");
    auto name=ui.createName();require(enabled(MenuOp::GameCreateAccept),"selected authority/core fixture is not creatable");
    auto changed=authority;changed.address=alias.address;changed.observedAt=1;ui.gameAuthorities({authority,changed});
    require(count(MenuOp::GameCreateServer)==2 && enabled(MenuOp::GameCreateAccept) && ui.createAddress()==changed.address && ui.createName()==name,"Create Game aliases duplicate the server or lose draft/core/selection");
    changed.info.instance[0]++;ui.gameAuthorities({changed});require(!enabled(MenuOp::GameCreateAccept) && ui.createName()==name,"restarted authority inherits Create selection or loses draft");
    ui.activate(item(MenuOp::GameCreateServer,1));require(enabled(MenuOp::GameCreateAccept),"authority restart loses the independently selected core");
    ui.gameAuthorities({});ui.gameAuthorities({changed});require(!enabled(MenuOp::GameCreateAccept),"retired authority selection resurrects after discovery");
    puts("Original browser identity: 5,120 aliases, newest routes/metadata, stable ordering, Games/LAN/Create selection, restart/removal retirement and Find Player alias merging passed without rendering");
}
}
