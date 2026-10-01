// UI-only Vulkan checks for original pointer ownership and control faces.
#include "renderer.hpp"
#include "hud.hpp"
#include "frontend.hpp"
#include "test_fixture.hpp"
#include "classic_scrollbar.hpp"
#include "player_pane.hpp"
#include "game_state_pane.hpp"
#include "ui_pointer_capture.hpp"
#include "test_reference/authored_hosts.hpp"
#include "test_reference/compiled_overlays.hpp"
#include "test_reference/help_overlay.hpp"
#include "test_reference/full_lobby.hpp"
#include "test_reference/player_painter_clips.hpp"
#include "test_reference/research_cards.hpp"
#include "test_reference/loadout_metrics.hpp"
#include "test_reference/remote_teleport.hpp"
#include "test_reference/training_artwork.hpp"
#include "../ClientCore/AuthoredText.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <fstream>

using namespace Native;
using NativeTest::require;
namespace {
struct Capture {
    int width{},height{};std::vector<unsigned char> pixels;
    explicit Capture(const std::filesystem::path& path){
        std::ifstream in(path,std::ios::binary);std::string magic;int maximum;
        in>>magic>>width>>height>>maximum;in.get();
        require(magic=="P6" && width>0 && height>0 && maximum==255,"invalid UI screenshot");
        pixels.resize(size_t(width)*height*3);in.read(reinterpret_cast<char*>(pixels.data()),pixels.size());require(bool(in),"truncated UI screenshot");
    }
    size_t differences(const Capture& other,glm::ivec4 bounds)const{
        require(width==other.width && height==other.height,"UI screenshot dimensions changed");size_t count=0;
        for(int y=std::max(0,bounds.y);y<std::min(height,bounds.y+bounds.w);++y)
            for(int x=std::max(0,bounds.x);x<std::min(width,bounds.x+bounds.z);++x){auto at=(size_t(y)*width+x)*3;
                if(!std::equal(pixels.begin()+at,pixels.begin()+at+3,other.pixels.begin()+at))++count;}
        return count;
    }
};
}
int main(int argc,char** argv)try{
    require(argc>1,"ARTWORK argument required");require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
    struct Cleanup{~Cleanup(){SDL_Quit();}} cleanup;configureVulkanPlatform();
    auto window=std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)>(SDL_CreateWindow("Allegiance UI hover check",1000,700,SDL_WINDOW_VULKAN|SDL_WINDOW_BORDERLESS),SDL_DestroyWindow);
    require(bool(window),SDL_GetError());Renderer renderer(window.get(),true);Assets assets(argv[1]);Hud hud(renderer,assets);
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.setPlaying(true);
    GameState state;state.docked=state.economy=state.ghost=true;state.ship="Scout";state.station="Home";ui.open(Screen::Hangar,&state);
    const auto& launch=authoredControl(ui.view().artwork,"launchButtonPane");
    auto prompt=std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.kind==OriginalControl::Kind::Text && std::any_of(c.hoverConditions.begin(),c.hoverConditions.end(),[&](const auto& condition){return condition.site==launch.helpSite && condition.value==launch.helpID;});});
    require(prompt!=ui.view().artwork.end(),"original launch prompt missing");
    auto layout=originalMenuLayout(renderer.size(),ui.view().classicScale,ui.view().canvas);
    auto region=glm::ivec4(glm::round(glm::vec4(layout.pixel(glm::vec2(prompt->bounds)),glm::vec2(prompt->bounds.z,prompt->bounds.w)*layout.scale)));
    auto pointer=layout.pixel(glm::vec2(launch.bounds)+glm::vec2(launch.bounds.z,launch.bounds.w)*.5f);
    auto acquire=[&]{for(int retry=0;!renderer.beginFrame();++retry){SDL_Event event;while(SDL_PollEvent(&event)){}require(retry<500,"could not acquire UI check frame");SDL_Delay(2);}};
    auto drawView=[&](const MenuView& view,glm::vec2 position,std::string_view name){
        auto quads=hud.buildMenu(renderer.size(),view,position);renderer.flushUploads();
        auto path=std::filesystem::temp_directory_path()/("allegiance-ui-hover-"+std::string(name)+".ppm");acquire();renderer.draw(glm::mat4(1),{},quads,path.string());return Capture(path);
    };
    auto draw=[&](glm::vec2 position,std::string_view name){return drawView(ui.view(),position,name);};
    {
        Settings preferences;Frontend right(preferences);right.originalInterface(assets);right.open(Screen::Home);
        auto button=std::find_if(right.view().items.begin(),right.view().items.end(),[](const auto& item){return item.control=="optionsButtonPane";});require(button!=right.view().items.end(),"right button pixel fixture missing");auto index=size_t(button-right.view().items.begin());
        right.pointerContext(index);
        MenuView actual;actual.original=true;actual.artwork={authoredControl(right.view().artwork,"optionsButtonPane")};actual.items={right.view().items[index]};actual.pressed=0;
        auto position=originalMenuLayout(renderer.size(),1,actual.canvas).pixel(glm::vec2(actual.items.front().bounds)+glm::vec2(2));
        auto expected=actual;expected.items.clear();auto& face=expected.artwork.front();face.kind=OriginalControl::Kind::Image;face.faces=0;face.uv.y=.25f;face.uv.w=.5f;
        auto pressed=drawView(actual,position,"ordinary-right-down"),reference=drawView(expected,{-1,-1},"ordinary-right-down-reference");
        require(pressed.differences(reference,{0,0,renderer.size().x,renderer.size().y})==0,"ordinary right button does not draw original explicit down bitmap frame");
        right.pointerRelease(index,false,SDL_BUTTON_RIGHT);actual.pressed=-1;face.uv.y=0;face.uv.w=.25f;
        auto released=drawView(actual,{-1,-1},"ordinary-right-up"),upReference=drawView(expected,{-1,-1},"ordinary-right-up-reference");
        require(released.differences(upReference,{0,0,renderer.size().x,renderer.size().y})==0 && released.differences(pressed,{0,0,renderer.size().x,renderer.size().y})>0,"ordinary right button does not release to original distinct up bitmap frame");
        puts("Original secondary button pixels: independently sliced down/up bitmap frames passed without world rendering");
    }
    auto absent=draw({-1,-1},"absent");auto disabled=draw(pointer,"disabled");
    require(disabled.differences(absent,region)>20,"disabled launch does not render its authored hover prompt");
    auto hit=menuHit(ui.view(),renderer.size(),1,pointer);require(hit && ui.view().items[*hit].action.op==MenuOp::Launch && !ui.view().items[*hit].enabled,"disabled launch does not retain pointer occlusion");
    auto overlap=ui.view();auto underneath=overlap.items[*hit];underneath.enabled=true;underneath.control="underneath";underneath.action={MenuOp::Quit};overlap.items.insert(overlap.items.begin(),underneath);
    auto occluded=menuHit(overlap,renderer.size(),1,pointer);require(occluded && !overlap.items[*occluded].enabled && overlap.items[*occluded].action.op==MenuOp::Launch,"disabled button passes a click to an enabled control underneath");
    ui.pointerPress(hit);require(ui.pointerRelease(hit).effect==FrontEffect::None,"disabled launch activates");
    state.ghost=false;ui.sync(state);auto enabled=draw(pointer,"enabled");require(enabled.differences(disabled,region)==0,"disabled and enabled launch prompts differ");
    auto blocked=ui.view();blocked.blockInput();auto quads=hud.buildMenu(renderer.size(),blocked,pointer);renderer.flushUploads();
    auto path=std::filesystem::temp_directory_path()/"allegiance-ui-hover-blocked.ppm";acquire();renderer.draw(glm::mat4(1),{},quads,path.string());
    require(Capture(path).differences(absent,region)==0 && !menuHit(blocked,renderer.size(),1,pointer),"captured hangar retains hover or input");
    auto index=[&](std::string_view name){auto i=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& c){return c.control==name;});require(i!=ui.view().items.end(),"capture pixel control missing");return size_t(i-ui.view().items.begin());};
    auto point=[&](size_t i){auto b=ui.view().items[i].bounds;return layout.pixel(glm::vec2(b)+glm::vec2(b.z,b.w)*.5f);};
    auto buttonRegion=[&](std::string_view name){auto b=authoredControl(ui.view().artwork,name).bounds;return glm::ivec4(glm::round(glm::vec4(layout.pixel(glm::vec2(b)),glm::vec2(b.z,b.w)*layout.scale)));};
    auto loadout=index("loadoutButtonPane"),command=index("commandButtonPane");auto loadoutPoint=point(loadout),commandPoint=point(command);auto commandRegion=buttonRegion("commandButtonPane");
    auto normal=draw({-1,-1},"normal");auto commandHover=draw(commandPoint,"command-hover");auto loadoutHover=draw(loadoutPoint,"loadout-hover");
    ui.pointerHover(loadout);ui.clearSounds();ui.pointerPress(loadout);ui.pointerHover(command);
    require(ui.view().capturedButton() && ui.sounds().empty(),"button capture admits neighboring sound or loses ownership");
    ++state.money;++state.economyRevision;ui.sync(state);require(ui.view().capturedButton() && ui.view().capturedButton()->control=="loadoutButtonPane","refresh loses captured button");
    auto held=draw(commandPoint,"held-over-neighbor");
    require(held.differences(loadoutHover,region)==0,"captured hover replaces the original entered prompt");
    require(held.differences(normal,commandRegion)==0 && held.differences(commandHover,commandRegion)>20,"captured pointer highlights an uncaptured neighboring button");
    ui.pointerHover(loadout);require(ui.sounds().empty(),"captured button reentry emits a new hover sound");ui.pointerRelease(command);require(ui.screen()==Screen::Hangar && !ui.pointerCaptured(),"release over neighbor executes or retains capture");
    auto undetectable=ui.view();undetectable.slide=MenuSlide{};undetectable.slide->undetectable=true;
    auto hidden=drawView(undetectable,commandPoint,"undetectable");
    require(hidden.differences(normal,region)==0 && hidden.differences(normal,commandRegion)==0 && !menuHit(undetectable,renderer.size(),1,commandPoint),"undetectable pane still drives authored hover, button face or input");
    {
        MenuView header;header.original=true;OriginalUi original(assets);header.artwork=original.layout("gamescreen","serverListHeader");
        for(auto& c:header.artwork){c.translate({100,100});header.items.push_back({c.visibleBounds(),{},{},{MenuOp::Order},true,false,c.name});}header.bindControlSounds();
        auto i=std::find_if(header.items.begin(),header.items.end(),[](const auto& i){return i.control=="serverListHeader1";});require(i!=header.items.end(),"authored text header pixel fixture missing");auto at=size_t(i-header.items.begin());
        auto p=layout.pixel(glm::vec2(i->bounds)+glm::vec2(3));auto b=glm::ivec4(glm::round(glm::vec4(layout.pixel(glm::vec2(i->bounds)),glm::vec2(i->bounds.z,i->bounds.w))));
        auto up=drawView(header,{-1,-1},"header-up"),hover=drawView(header,p,"header-hover");require(hover.differences(up,b)==0,"PaneButtonFacePane text header incorrectly changes on hover");
        header.pressed=int(at);auto down=drawView(header,p,"header-down");require(down.differences(up,b)>20,"text header omits original yellow/bold down face");
        header.pressed=-1;header.items[at].selected=true;auto checked=drawView(header,{-1,-1},"header-checked");require(checked.differences(down,b)==0,"text header fails to retain its original checked face");
        header.pressed=int(at);auto checkedDown=drawView(header,p,"header-checked-down");require(checkedDown.differences(up,b)==0,"text header does not invert its checked face while pressed");
        header.pressed=-1;header.items[at].enabled=false;auto disabled=drawView(header,p,"header-disabled");require(disabled.differences(checked,b)==0,"text header invents a PaneButtonFacePane disabled face");
    }
    {
        MenuView menu;menu.original=true;ScrollList list;list.configure(60,15,{200,200,100,168});list.bar={300,200,14,168};appendClassicScrollbar(menu,assets,list,3);menu.bindControlSounds();
        auto i=std::find_if(menu.items.begin(),menu.items.end(),[](const auto& i){return i.action.op==MenuOp::ScrollLine && i.action.delta<0;});require(i!=menu.items.end(),"shared arrow pixel fixture missing");auto at=size_t(i-menu.items.begin());
        auto p=layout.pixel(glm::vec2(i->bounds)+glm::vec2(3));auto b=glm::ivec4(glm::round(glm::vec4(layout.pixel(glm::vec2(i->bounds)),glm::vec2(i->bounds.z,i->bounds.w))));
        auto up=drawView(menu,{-1,-1},"arrow-up"),hover=drawView(menu,p,"arrow-hover");require(hover.differences(up,b)==0,"scroll arrow invents a hover face");
        menu.pressed=int(at);auto down=drawView(menu,p,"arrow-down");
        auto expected=menu;expected.pressed=-1;auto arrow=std::find_if(expected.artwork.begin(),expected.artwork.end(),[&](const auto& c){return c.name==i->control;});require(arrow!=expected.artwork.end(),"selected arrow reference missing");arrow->image=assets.image("scrollupselbmp",true);arrow->downImage.reset();arrow->kind=OriginalControl::Kind::Image;
        auto selected=drawView(expected,{-1,-1},"arrow-reference");require(down.differences(selected,b)==0,"scroll arrow does not draw the original complete selected image");
        // This installed set has identical normal/selected arrow pixels. Also
        // exercise a distinguishable resident down image to prove selection.
        auto alternate=menu;auto alternateArrow=std::find_if(alternate.artwork.begin(),alternate.artwork.end(),[&](const auto& c){return c.name==i->control;});alternateArrow->downImage=assets.image("scrolldownbmp",true);
        auto distinct=drawView(alternate,p,"arrow-distinct-down");arrow->image=alternateArrow->downImage;
        auto distinctReference=drawView(expected,{-1,-1},"arrow-distinct-reference");require(distinct.differences(distinctReference,b)==0 && distinct.differences(up,b)>0,"PaneButtonFacePane does not select its separate down image");
        auto outside=drawView(menu,{-1,-1},"arrow-outside");require(outside.differences(up,b)==0,"captured scroll arrow keeps its down face outside");
    }
    {
        // Exercise the real map and production capture dispatcher with only
        // 2D quads. No Game, world geometry or model preview is constructed.
        Hud map(renderer,assets);map.configure(settings);GameState mapState;
        mapState.playerSide=0;mapState.sectors={{7,"Wasp",{-5,-5}},{9,"Aegis",{5,5}}};
        PlayerPane players(assets);GameStatePane standings(assets);UiPointerCapture capture(ui,players,standings,map);
        MenuView empty;empty.original=true;empty.canvas={800,600};auto size=renderer.size();
        auto buildMap=[&]{map.buildMenu(size,empty,{-1,-1});auto quads=map.buildSectorMap(size,mapState,{-1,-1},0);renderer.flushUploads();acquire();renderer.draw(glm::mat4(1),{},quads);};
        buildMap();SDL_Delay(510);buildMap();glm::vec2 origin{49,size.y-177};auto expand=origin+glm::vec2(183,9),blank=origin+glm::vec2(69,72);
        int activated=0;auto front=[&](const FrontResult& r){if(r.effect!=FrontEffect::None)++activated;};auto mapAction=[&](const HudAction& a){require(a.command==HudCommand::SectorChat && a.value==7,"captured map right press changes its original sector-chat action");++activated;};
        auto result=map.mapPointerButton(expand,SDL_BUTTON_LEFT,true);require(result.handled && !result.capture && !capture.active(),"map expand press invents capture absent from SectorInfoPane");
        require(!map.mapPointerButton({900,20},SDL_BUTTON_LEFT,false).handled,"map expand consumes an outside release");
        require(map.mapPointerButton(expand,SDL_BUTTON_LEFT,false).handled,"map expand ignores the original release event");buildMap();
        require(!map.mapPointerButton(expand,SDL_BUTTON_LEFT,true).handled,"map expand release does not select the collapsed layout");
        require(map.mapPointerButton(origin+glm::vec2(123,9),SDL_BUTTON_LEFT,false).handled,"map expand wrongly requires a matching down event");buildMap();
        auto node=origin+glm::vec2(53,76);require(map.mapCursor(node,true)=="goto","captured map sector fixture");
        result=map.mapPointerButton(blank,SDL_BUTTON_LEFT,true);
        require(result.handled && result.capture && capture.owner()==UiPointerOwner::Map,"map drag capture fixture");
        require(capture.button(size,blank,SDL_BUTTON_RIGHT,false,false,false,front,mapAction) && capture.active(),"right release incorrectly retires a left-only map drag");
        require(capture.button(size,node,SDL_BUTTON_RIGHT,true,false,false,front,mapAction) && activated==1 && capture.active(),"captured map suppresses the original right-button sector action");activated=0;
        ui.error("Popup during map drag");map.buildMenu(size,ui.messageView(),{-1,-1});
        require(capture.move(size,{900,20}) && capture.active(),"obscured map drag loses captured motion");
        require(capture.button(size,{900,20},SDL_BUTTON_LEFT,false,false,false,front,mapAction) && !capture.active() && !map.mapPointerMove(blank) && !activated,"obscured map drag cannot release outside the map");
        ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);ui.open(Screen::Hangar,&state);buildMap();require(map.mapPointerButton(blank,SDL_BUTTON_LEFT,true).capture,"map cancellation fixture lacks capture");capture.cancel();require(!capture.active() && !map.mapPointerMove(blank),"focus/session cancellation retains a map drag");
    }
    {
        // Compare real host painters to unbounded DrawString placement, clipped
        // at their original surfaces. Width zero bypasses wrapping/whole-char
        // clipping in the reference; it does not use surfaceText(). No Game,
        // model preview or world scene is constructed by these checks.
        NativeTest::AuthoredAssets fixture(assets.root());
        fixture.change("gameoverscreen.mdl",{{"[28, 119, 198, 251, 323, 371, 434, 482, 538, 570]","[28, 77, 81, 88, 95, 103, 111, 119, 127, 135]"}});
        Assets alternate(fixture.root);Settings prefs;Frontend painters(prefs);painters.originalInterface(alternate);painters.setPlaying(true);OriginalUi source(alternate);
        auto compare=[&](const MenuView& host,std::string_view name){
            MenuView actual;actual.original=true;actual.canvas=host.canvas;actual.classicScale=host.classicScale;
            for(const auto& c:host.artwork)if(c.kind==OriginalControl::Kind::Text && c.textMode==OriginalControl::TextMode::Native && !c.text.empty())actual.artwork.push_back(c);
            require(!actual.artwork.empty(),"surface painter pixel check contains no direct text");
            auto expected=actual;
            for(auto& c:expected.artwork){
                auto font=alternate.font(c.font.empty()?"smallFont":c.font,c.fontHeight,c.fontBold);
                float width=Allegiance::TextExtent(c.text,[&](unsigned char ch){return font->glyphs[ch].z;});
                if(c.justify==1)c.bounds.x=std::floor(c.bounds.x+(c.bounds.z-width)*.5f);
                else if(c.justify==2)c.bounds.x=std::floor(c.bounds.x+c.bounds.z-width);
                c.justify=0;c.bounds.z=0;c.singleLine=true;
            }
            auto original=drawView(expected,{-1,-1},std::string(name)+"-surface-reference"),ported=drawView(actual,{-1,-1},std::string(name)+"-surface-actual");
            require(original.differences(ported,{0,0,renderer.size().x,renderer.size().y})==0,"direct host text wraps/truncates instead of clipping original surface glyphs");
            printf("Original surface painter pixels: %s, %zu direct strings passed\n",std::string(name).c_str(),actual.artwork.size());
        };
        GameState lobby;lobby.networked=lobby.networkLobby=lobby.docked=lobby.networkHost=lobby.commander=true;lobby.playerSide=0;lobby.station="Home";
        lobby.networkPilots={{7,0,std::string(31,'W'),true,true,true,true,false},{8,0,std::string(31,'M'),true,false,false,false,false},{9,noTeam,std::string(31,'W'),false,false,false,false,false,0}};
        painters.open(Screen::RemoteLobby,&lobby);compare(painters.view(),"lobby-rows");
        painters.open(Screen::Controls);compare(painters.view(),"mapping-rows");
        painters.open(Screen::Online);Net::ServerInfo server;server.players=1024;server.capacity=1024;server.flags=Net::ServerJoinable;Net::text(server.name,std::string(63,'W'));
        painters.gameListings({{"127.0.0.1:29500",server}});compare(painters.view(),"lan-rows");
        GameState results=lobby;results.networkLobby=false;results.complete=true;results.endgame.frozen=true;results.endgame.winner=0;
        for(int i=0;i<12;++i){Net::ResultPlayer p;p.id=i;p.side=i%2;Net::text(p.name,std::string(31,i%2?'M':'W'));p.kills=p.deaths=p.ejections=p.assists=p.baseKills=p.baseCaptures=32767;p.score=1234567;p.played=1234567;results.endgame.players.push_back(p);}
        for(int i=0;i<2;++i){Net::ResultTeam t;t.id=i;Net::text(t.name,std::string(31,'W'));t.baseKills=t.baseCaptures=t.flags=t.artifacts=t.kills=t.deaths=32767;results.endgame.teams.push_back(t);}
        painters.open(Screen::Results,&results);auto columns=source.integers("gameoverscreen","statsColumns"),teamColumns=source.integers("gameoverscreen","statsSideColumns");
        auto playerBounds=authoredControlBounds(painters.view().artwork,"statsListPane"),teamBounds=authoredControlBounds(painters.view().artwork,"statsSideListPane");
        for(const auto& c:painters.view().artwork)if(c.name.starts_with("resultPlayer/") || c.name.starts_with("resultTeam/")){
            if(c.kind!=OriginalControl::Kind::Text)continue;bool player=c.name.starts_with("resultPlayer/");int offset=std::stoi(c.name.substr(c.name.find_last_of('/')+1));const auto& cols=player?columns:teamColumns;
            auto surface=player?playerBounds:teamBounds;
            bool restricted=offset==cols[0]+(player?4:2) || (player && offset==cols[1]+4);
            float expectedLeft=restricted?c.bounds.x:surface.x,expectedWidth=restricted?c.bounds.z:surface.z;
            require(c.clip && c.clip->x==expectedLeft && c.clip->z==expectedWidth && c.clip->w==14,"postgame numeric columns invent clips absent from original row surfaces");
        }
        compare(painters.view(),"postgame-rows");
        std::vector<TeleportChoice> destinations(2);for(int i=0;i<2;++i){destinations[i].id=10+i;destinations[i].name=std::string(31,'W');destinations[i].sector=std::string(24,'M');destinations[i].current=i==0;destinations[i].keepsShip=true;}
        painters.teleports(destinations,lobby);compare(painters.view(),"teleport-rows");
        std::vector<ResearchChoice> choices(2);for(int i=0;i<2;++i){auto& row=choices[i];row.id=i;row.type=OT_development;row.name=std::string(80,'W');row.model="missing-card";row.price=1234567;row.available=row.canBuy=row.predecessorReady=true;row.description=std::string(100,'W');}
        painters.researchService([&]{return choices;});lobby.money=1234567;painters.open(Screen::Research,&lobby);compare(painters.view(),"investment-rows");
        auto invest=std::find_if(painters.view().items.begin(),painters.view().items.end(),[](const auto& i){return i.action.op==MenuOp::ResearchSelect;});require(invest!=painters.view().items.end(),"investment info pixel fixture lacks a row");
        painters.pointerHover(size_t(invest-painters.view().items.begin()));compare(painters.view(),"investment-info");
        painters.open(Screen::Home);painters.open(Screen::Settings);compare(painters.view(),"options-menu");
    }
    auto comparePlayers=[&](const MenuView& actual,const MenuView& expected,std::string_view name){
        auto reference=drawView(expected,{-1,-1},std::string(name)+"-reference"),ported=drawView(actual,{-1,-1},std::string(name)+"-actual");
        require(reference.differences(ported,{0,0,renderer.size().x,renderer.size().y})==0,"full lobby/F6 pixels differ from original painter reference");
    };
    NativeTest::checkFullLobby(assets,comparePlayers);
    NativeTest::checkPlayerPainterClips(assets,renderer.size(),comparePlayers);
    NativeTest::checkResearchCards(assets,comparePlayers);
    NativeTest::checkTeleportHeaders(assets,[&](const MenuView& actual,const MenuView& expected,std::string_view name){
        const auto& button=actual.items.at(actual.pressed);auto placement=originalMenuLayout(renderer.size(),actual.classicScale,actual.canvas);
        auto pointer=placement.pixel(glm::vec2(button.bounds)+glm::vec2(button.bounds.z,button.bounds.w)*.5f);
        auto reference=drawView(expected,pointer,std::string(name)+"-reference"),ported=drawView(actual,pointer,std::string(name)+"-actual");
        require(reference.differences(ported,{0,0,renderer.size().x,renderer.size().y})==0,"teleport header pressed pixels differ from authored ButtonBarPane");
        auto normal=actual;normal.pressed=-1;auto up=drawView(normal,{-1,-1},std::string(name)+"-up");
        if(name.ends_with("-distinct"))require(up.differences(ported,{0,0,renderer.size().x,renderer.size().y})>0,"teleport header pixel check does not exercise a distinct pressed face");
    });
    NativeTest::checkLoadoutMetrics(assets,[&](const MenuView& actual,const MenuView& expected,std::string_view name){
        auto reference=drawView(expected,{-1,-1},std::string(name)+"-reference"),ported=drawView(actual,{-1,-1},std::string(name)+"-actual");
        require(reference.differences(ported,{0,0,renderer.size().x,renderer.size().y})==0,"loadout calibration pixels differ from original numeric reference");
    });
    NativeTest::checkCompiledOverlays(assets,[&](const MenuView& actual,const MenuView& expected,std::string_view name){
        auto original=drawView(expected,{-1,-1},std::string(name)+"-adapter-reference"),ported=drawView(actual,{-1,-1},std::string(name)+"-adapter-actual");
        require(original.differences(ported,{0,0,renderer.size().x,renderer.size().y})==0,"compiled crew/console adapter pixels differ from authored reference");
    });
    NativeTest::checkHelpOverlay(assets,renderer.size(),[&](const MenuView& actual,const MenuView& expected,std::string_view name){
        auto reference=drawView(expected,{-1,-1},std::string(name)+"-reference"),ported=drawView(actual,{-1,-1},std::string(name)+"-actual");
        require(reference.differences(ported,{0,0,renderer.size().x,renderer.size().y})==0,"independent Help pixels differ from authored translated root");
    });
    NativeTest::checkTrainingArtwork(assets,[&](const MenuView& actual,const MenuView& expected,std::string_view name){
        auto reference=drawView(expected,{-1,-1},std::string(name)+"-reference"),ported=drawView(actual,{-1,-1},std::string(name)+"-actual");
        require(reference.differences(ported,{0,0,renderer.size().x,renderer.size().y})==0,"training pixels differ from the independent original image/text evaluator");
    });
    puts("Original UI pixels/input: disabled prompts/occlusion, captured ownership/refresh/captions, undetectable overlays, authored text-header faces, separate full arrow images, actual map routing, eight direct surface-painter hosts, compiled crew/console overlays and independent Help passed with Vulkan validation enabled");return 0;
}catch(const std::exception& e){fprintf(stderr,"UI hover check: %s\n",e.what());return 1;}
