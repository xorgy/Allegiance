#pragma once
#include "../frontend.hpp"
#include "../test_fixture.hpp"
#include "../classic_scrollbar.hpp"
#include <cstdio>

namespace NativeTest {
inline void checkListInput(Native::Assets& assets){
    using namespace Native;
    size_t cases=0;
    // Frozen ListPane NextItem/PreviousItem/PageUp/PageDown: replay each
    // individual step, checking the observable selection rather than relying
    // on the modern implementation's direct clamping formula.
    for(int count=0;count<=64;++count)for(int selected=-1;selected<count;++selected)for(int delta=-32;delta<=32;++delta){
        ScrollList list;list.configure(count,14,{0,0,100,168});list.selection=selected;
        int expected=selected;
        for(int n=0;n<std::abs(delta);++n){
            if(delta>0){if(expected<count-1)++expected;}
            else if(expected>0)--expected;
        }
        list.move(delta);require(list.selection==expected,"shared list navigation differs from original empty/end/page behavior");++cases;
    }
    ScrollList empty;empty.select(0);require(empty.selection==-1,"empty list creates a nonexistent selected row");
    {
        // Frozen TroughPane::UpdateLayout/SetMousePos, using its integer
        // division and ThumbPane's eight-pixel minimum independently.
        size_t geometry=0,drags=0;
        for(int count:{0,1,2,13,64,128})for(int row:{14,15,28})for(int page:{0,14,73,168,336})for(int pixels:{32,63,160,320}){
            int size=count*row,maximum=std::max(0,size-page),height=page>=size?pixels:page==0?14:pixels*page/size;height=std::max(8,height);
            for(int step=0;step<=8;++step){
                int pos=maximum*step/8,left=maximum?pos*(pixels-height)/maximum:0;
                ScrollList list;list.configure(count,row,{10,20,100,page});list.bar={110,9,14,pixels+22};list.upHeight=9;list.downHeight=13;list.thumbMinimum=8;list.position(pos);
                require(list.thumb()==glm::vec4(110,18+left,14,height),"scrollbar thumb differs from original integer layout/minimum");++geometry;
                for(int x=-8;x<=pixels+8;x+=7){
                    int expected=pixels!=height?x*maximum/(pixels-height):0;expected=std::clamp(expected,0,maximum);
                    list.drag(18+x+3.25f,3.25f);require(list.offset==expected,"scrollbar drag differs from original integer SetMousePos");++drags;
                }
            }
        }
        MenuView view;view.original=true;ScrollList list;list.configure(100,15,{10,20,300,168});list.bar={310,20,14,168};
        appendClassicScrollbar(view,assets,list,4);view.bindControlSounds();
        require(list.upHeight==assets.image("scrollupbmp",true)->height && list.downHeight==assets.image("scrolldownbmp",true)->height && list.thumbMinimum==8,"shared scrollbar divides original full-image dimensions");
        for(size_t i=0;i<view.items.size();++i){const auto& item=view.items[i];require(item.captureOnPress && item.clickSound<0 && item.hoverSound<0,"original scrollbar loses silent pointer ownership");
            if(item.action.op==MenuOp::ScrollLine){const auto& arrow=authoredControl(view.artwork,item.control);require(arrow.kind==OriginalControl::Kind::Button && !arrow.faces && arrow.downImage==assets.image(item.action.delta<0?"scrollupselbmp":"scrolldownselbmp",true),"scrollbar arrow still uses a four-face strip");}
        }
        printf("Original scrollbar geometry: %zu frozen integer/minimum comparisons and %zu drag positions; shared full-size arrow images and silent capture passed\n",geometry,drags);
    }
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);
    auto item=[&](MenuOp op,int value=0,int group=-999){auto& v=ui.view().items;auto i=std::find_if(v.begin(),v.end(),[&](const auto& c){return c.action.op==op && c.action.value==value && (group==-999 || c.action.delta==group);});require(i!=v.end(),"list input fixture action missing");return size_t(i-v.begin());};
    auto noSelection=[&](MenuOp op){return std::none_of(ui.view().items.begin(),ui.view().items.end(),[&](const auto& c){return c.action.op==op && c.selected;});};
    auto move=[&](SDL_Keycode key,SDL_Scancode scan){return ui.key(key,scan);};
    auto pointHit=[&](glm::vec2 local){auto layout=originalMenuLayout({800,600},ui.view().classicScale,ui.view().canvas);return menuHit(ui.view(),{800,600},1,layout.pixel(local));};
    auto blank=[&](MenuOp selection,int group=0,bool right=false){auto index=item(MenuOp::ListClear,int(selection),group);auto b=ui.view().items[index].bounds;auto hit=pointHit({b.x+3,b.y+b.w-3});require(hit && *hit==index,"authored blank viewport does not own its actual hit region");
        if(right)ui.pointerContext(hit);else ui.pointerPress(hit);
        require(!ui.pointerCaptured() && ui.pointerRelease({}).effect==FrontEffect::None,"blank ListPane click captures or acts again on release");};
    auto rowHit=[&](MenuOp op,int value,int group=-999){auto index=item(op,value,group);auto b=ui.view().items[index].bounds;auto hit=pointHit(glm::vec2(b)+glm::vec2(b.z,b.w)*.5f);require(hit && *hit==index,"blank viewport intercepts an occupied row");};

    std::vector<Net::ServerListing> listings;
    for(int i=0;i<2;++i){Net::ServerInfo info;info.flags=Net::ServerWaiting|Net::ServerJoinable;info.players=i;Net::text(info.name,"Game "+std::to_string(i));listings.push_back({"127.0.0.1:"+std::to_string(28000+i),info});}
    ui.open(Screen::Games);ui.gameListings(listings);
    move(SDLK_UP,SDL_SCANCODE_UP);move(SDLK_PAGEUP,SDL_SCANCODE_PAGEUP);require(noSelection(MenuOp::GameSelect),"Games Up/PageUp invents an initial selection");
    rowHit(MenuOp::GameSelect,1);ui.pointerContext(item(MenuOp::GameSelect,1),true);require(ui.view().items[item(MenuOp::GameSelect,1)].selected,"Games double-right click fails to select without joining");
    require(move(SDLK_HOME,SDL_SCANCODE_HOME).effect==FrontEffect::None && move(SDLK_END,SDL_SCANCODE_END).effect==FrontEffect::None && move(SDLK_RETURN,SDL_SCANCODE_RETURN).effect==FrontEffect::None && ui.view().items[item(MenuOp::GameSelect,1)].selected,"Games adds original-absent Home/End/Enter behavior");
    blank(MenuOp::GameSelect);require(noSelection(MenuOp::GameSelect) && !ui.view().items[item(MenuOp::GameJoin)].enabled,"blank Games click retains a join target");
    move(SDLK_DOWN,SDL_SCANCODE_DOWN);require(ui.view().items[item(MenuOp::GameSelect,0)].selected,"Games Down from no selection does not select first row");blank(MenuOp::GameSelect,0,true);
    ui.pointerMove(glm::vec2(ui.view().items[item(MenuOp::ListClear,int(MenuOp::GameSelect))].bounds)+glm::vec2(3));require(ui.pointerWheel(1) && noSelection(MenuOp::GameSelect),"Games upward wheel invents a blank selection");
    ui.pointerContext(item(MenuOp::GameSelect,1));auto returning=listings;ui.gameListings({listings.back()});require(noSelection(MenuOp::GameSelect),"departed game keeps its selected row");ui.gameListings(returning);require(noSelection(MenuOp::GameSelect) && !ui.view().items[item(MenuOp::GameJoin)].enabled,"returning game recreates a retired selection");

    ui.open(Screen::Online);ui.gameListings(listings);move(SDLK_UP,SDL_SCANCODE_UP);require(noSelection(MenuOp::LanSelect),"LAN Up invents an initial selection");
    ui.pointerContext(item(MenuOp::LanSelect,1),true);require(ui.view().items[item(MenuOp::LanSelect,1)].selected && ui.screen()==Screen::Online && std::none_of(ui.view().items.begin(),ui.view().items.end(),[](const auto& c){return c.action.op==MenuOp::LogonAccept;}),"LAN double-right click opens Logon instead of selecting");
    ui.pointerContext(item(MenuOp::LanSelect,-1));require(!ui.view().items[item(MenuOp::LanJoin)].enabled,"LAN blank right-click retains a join target");

    ui.open(Screen::Controls);ui.pointerContext(item(MenuOp::MappingSelect,3),true);require(ui.view().items[item(MenuOp::MappingSelect,3)].selected && !ui.capturingKey(),"mapping double-right click edits instead of selecting");
    ui.activate(item(MenuOp::ListClear,int(MenuOp::MappingSelect)));move(SDLK_UP,SDL_SCANCODE_UP);move(SDLK_RETURN,SDL_SCANCODE_RETURN);require(noSelection(MenuOp::MappingSelect) && !ui.capturingKey(),"cleared mapping selection is recreated or Enter starts an invalid edit");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);

    SessionOptions options;options.scenario=Scenario::Conquest;options.rules.teams=4;ui.networkSession(options);ui.setPlaying(true);
    GameState lobby;lobby.networked=lobby.networkLobby=lobby.networkHost=lobby.commander=true;lobby.networkPilots={{10,0,"Owner",true,true,true,true,false},{20,0,"Peer",true,false,false,false,false}};lobby.teamScores.resize(4);for(auto& t:lobby.teamScores)t.active=true;
    ui.open(Screen::RemoteLobby,&lobby);ui.text("Preserved draft");
    auto peer=[&]{auto& v=ui.view().items;auto i=std::find_if(v.begin(),v.end(),[](const auto& c){return c.action.op==MenuOp::RosterSelect && c.action.delta==20;});require(i!=v.end(),"peer row missing");return size_t(i-v.begin());};
    ui.pointerContext(peer(),true);require(ui.view().items[peer()].selected && !ui.keyboardPopup() && move(SDLK_RETURN,SDL_SCANCODE_RETURN).targetId==20,"lobby double-right click opens a popup or loses private selection");
    blank(MenuOp::RosterSelect);require(noSelection(MenuOp::RosterSelect) && move(SDLK_RETURN,SDL_SCANCODE_RETURN).targetId==-1 && ui.chatText()=="Preserved draft","lobby blank click retains private recipient or loses draft");
    move(SDLK_UP,SDL_SCANCODE_UP);require(noSelection(MenuOp::RosterSelect),"lobby Up recreates a cleared selection");
    ui.pointerContext(peer());require(ui.keyboardPopup(),"single-right lobby context menu is missing");move(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);blank(MenuOp::RosterSelect,0,true);require(noSelection(MenuOp::RosterSelect),"lobby blank right-click retains private selection");
    auto team=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& c){return c.control=="team0";});require(team!=ui.view().items.end(),"team context fixture missing");ui.pointerContext(size_t(team-ui.view().items.begin()),true);require(!ui.keyboardPopup(),"double-right team click opens original single-right alliance menu");

    ui.setPlaying(false);ui.setPlaying(true);GameState dock;dock.docked=true;dock.ship="Scout";dock.station="Home";
    std::vector<TeleportChoice> destinations{{7,"Home","Wasp",true,true},{8,"Outpost","Aegis",false,true}};ui.teleports(destinations,dock);
    move(SDLK_UP,SDL_SCANCODE_UP);require(noSelection(MenuOp::TeleportSelect),"Teleport Up invents an initial selection");move(SDLK_DOWN,SDL_SCANCODE_DOWN);require(ui.view().items[item(MenuOp::TeleportSelect,7)].selected,"Teleport Down does not select first destination");
    ui.pointerContext(item(MenuOp::TeleportSelect,-1));move(SDLK_PAGEUP,SDL_SCANCODE_PAGEUP);require(noSelection(MenuOp::TeleportSelect),"Teleport paging recreates blank selection");

    GameState result;result.complete=true;result.endgame.frozen=true;
    for(int i=0;i<2;++i){Net::ResultPlayer p;p.id=100+i;p.side=i;p.score=i;Net::text(p.name,"Player "+std::to_string(i));result.endgame.players.push_back(p);Net::ResultTeam t;t.id=i;Net::text(t.name,"Team "+std::to_string(i));result.endgame.teams.push_back(t);}
    ui.open(Screen::Results,&result);rowHit(MenuOp::ResultsSelect,100,0);ui.pointerContext(item(MenuOp::ResultsSelect,100,0),true);require(ui.view().items[item(MenuOp::ResultsSelect,100,0)].selected,"Results double-right click does not select its player");
    blank(MenuOp::ResultsSelect);require(noSelection(MenuOp::ResultsSelect),"Results blank player click retains selection");ui.pointerContext(item(MenuOp::ResultsSelect,1,1));blank(MenuOp::ResultsSelect,1,true);require(noSelection(MenuOp::ResultsSelect),"Results blank team right-click retains selection");
    printf("Original ListPane input: %zu frozen navigation cases; Games/LAN/mapping/lobby/teleport/results right clicks, blank hits, row precedence, private draft and original key limits passed\n",cases);
}
}
