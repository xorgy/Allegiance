#include "test_reference/local_game.hpp"
#include "test_reference/training_menu.hpp"
#include "test_reference/training_artwork.hpp"
#include "test_reference/classic_keyboard.hpp"
#include "test_reference/list_input.hpp"
#include "test_reference/docked_workflows.hpp"
#include "test_reference/loadout_editor.hpp"
#include "test_reference/loadout_metrics.hpp"
#include "test_reference/remote_teleport.hpp"
#include "test_reference/button_input.hpp"
#include "test_reference/team_activity.hpp"
#include "test_reference/browse_identity.hpp"
#include "test_reference/compiled_overlays.hpp"
#include "test_reference/help_overlay.hpp"
#include "test_reference/ui_feedback.hpp"
#include "test_reference/full_lobby.hpp"
#include "test_reference/player_painter_clips.hpp"
#include "test_reference/research_cards.hpp"
#include "player_pane.hpp"
#include "game_state_pane.hpp"
#include "ui_console_panels.hpp"
#include "scene_pointer.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
using NativeTest::require;
namespace {
void mouseControl(Native::Assets& assets){
    using namespace Native;
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);
    SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.enemies=0;options.rules.miners=0;
    Game game(assets.root(),[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
    PlayerPane players(assets);GameStatePane standings(assets);ui.setPlaying(true);
    int cases=0;
    for(auto host:{Screen::Hangar,Screen::Research,Screen::Teleport})for(bool initial:{false,true})for(int mask=0;mask<4;++mask){
        players.reset();standings.reset();ui.open(Screen::Hangar,&game.state());
        if(host==Screen::Teleport)ui.teleports(game.teleportDestinations(),game.state());else ui.open(host,&game.state());
        if(mask&1)players.toggle(game);if(mask&2)standings.toggle(game);
        require(players.active()==bool(mask&1) && standings.active()==bool(mask&2),"mouse-toggle pane fixture is inactive");
        bool enabled=initial,wasFresh=ui.freshInvestmentPane();auto result=toggleConsoleMouse(ui,enabled,players,standings,game,true);
        require(!players.active() && !standings.active() && !players.pointerCaptured() && !standings.pointerCaptured(),"mouse toggle retains a banishable pane or its capture");
        if(mask || host==Screen::Teleport){
            require(enabled && result.dismissPanes==Allegiance::MouseDismissPanes && ui.freshInvestmentPane()==wasFresh,"pane dismissal fails to enable mouse input or incorrectly consumes fresh investment state");
            require(result.effect==(host==Screen::Teleport?FrontEffect::Resume:FrontEffect::None),"mouse toggle closes research/hangar or fails to return from teleport");
        }else if(host==Screen::Research){
            require(wasFresh && !ui.freshInvestmentPane() && enabled==initial && !result.dismissPanes,"fresh investment changes mouse preference instead of releasing only pointer ownership");
            toggleConsoleMouse(ui,enabled,players,standings,game,true);require(enabled!=initial,"second investment mouse toggle fails to switch mouse steering");
        }else require(enabled!=initial && !result.dismissPanes,"ordinary mouse toggle fails to change input mode");
        ++cases;
    }
    bool runtime=true;settings.virtualJoystick=false;int applied=0;
    ui.mouseInputService([&]{return runtime;},[&](bool value){runtime=value;++applied;});ui.open(Screen::Hangar,&game.state());ui.open(Screen::Pause);ui.key(SDLK_G,SDL_SCANCODE_G);
    auto label=[&](bool on){return std::any_of(ui.view().items.begin(),ui.view().items.end(),[&](const auto& item){return item.label==(on?"Use Mouse As Virtual Joystick: On ":"Use Mouse As Virtual Joystick: Off ");});};
    require(label(true),"mouse option displays the saved preference instead of current mapped-key mode");
    auto result=ui.key(SDLK_J,SDL_SCANCODE_J);require(result.effect==FrontEffect::ApplyOptions && result.value==128 && !runtime && !settings.virtualJoystick && applied==1 && label(false),"mouse option does not toggle current runtime state or unnecessarily reconfigure controllers");
    result=ui.key(SDLK_J,SDL_SCANCODE_J);require(result.value==128 && runtime && settings.virtualJoystick && applied==2 && label(true),"mouse option does not refresh/persist its current mode");
    ui.key(SDLK_L,SDL_SCANCODE_L);require(runtime && applied==2,"unrelated input option resets transient mouse mode");
    printf("Original mouse UI: %d hangar/investment/teleport/pane states, capture retirement, two-stage investment toggle, live options and isolated controller changes passed without rendering\n",cases);

    QuickChatCatalog quick(assets);game.quickChats(quick);ChatConsole chat(assets,quick);
    int self=game.state().shipId,other=game.addLocalPilot("Pointer recipient",0);
    require(other>=0 && game.controlLocalPilot(other),"scene pointer second recipient fixture");game.launch();
    require(!game.state().docked && game.controlLocalPilot(self),"scene pointer launched recipient fixture");game.launch();
    require(!game.state().docked,"scene pointer flying pilot fixture");
    // Retire the launch transition without advancing collision/combat state.
    require(game.controlLocalPilot(self) && !game.state().cinematic,"scene pointer normal pilot input fixture");
    game.playerShip()->SetPosition(Vector(0,0,0));game.core()->GetShip(ShipID(other))->SetPosition(Vector(100,0,0));
    CameraState camera;camera.position={0,-600,600};camera.forward=glm::normalize(-camera.position);
    auto projection=glm::perspectiveRH_ZO(glm::radians(60.f),4.f/3,1.f,10000.f);projection[1][1]*=-1;
    projection*=glm::lookAtRH(camera.position,glm::vec3(0),glm::vec3(0,0,1));
    glm::ivec2 size{800,600};CommandGesture gesture;
    auto pixel=[&](glm::vec3 position){auto clip=projection*glm::vec4(position,1);return (glm::vec2(clip)/clip.w+1.f)*.5f*glm::vec2(size);};
    auto button=[&](glm::vec2 point,int which,bool down,std::optional<ScenePointerTarget> target={},bool shift=false){
        require(scenePointerEnabled(game.state(),true,true),"original free pilot/command cursor does not accept scene input");
        require(scenePointerButton(gesture,game,chat,point,size,projection,camera,which,down,shift,true,target),"original scene button not consumed");
    };
    for(auto mode:{CameraMode::Cockpit,CameraMode::Command}){
        if(mode==CameraMode::Command)game.commandView(true);else require(game.viewCamera(mode),"scene pointer view fixture");
        require(game.state().commandView==(mode==CameraMode::Command),"scene pointer view did not change");chat.reset();game.pickCommandUnits({});
        require(scenePointerEnabled(game.state(),true,false)==game.state().commandView && scenePointerEnabled(game.state(),false,true)==game.state().commandView,"pilot steering or optional modern cursor leaks scene gestures");
        auto targetBefore=game.state().targetId;auto typeBefore=game.state().targetObjectType;
        auto point=pixel({100,0,0});
        auto* picked=commandHover(game.contacts(),point,size,projection,[&](const auto& c){return !game.commandCursor(c.type,c.id,{},false,0,-1,0).empty();});
        require(picked && picked->type==ContactShip && picked->id==other,"pilot perspective does not pick the original projected friendly ship");
        ScenePointerTarget target{picked->type,picked->id};
        button(point,SDL_BUTTON_LEFT,true,target);button(point,SDL_BUTTON_LEFT,false,target);
        require(chat.mode()==Allegiance::ComposeState::Mouse && chat.audience().channel==ChatChannel::Target && chat.audience().recipient==other && game.state().targetId==targetBefore && game.state().targetObjectType==typeBefore,"scene selection loses recipient or overwrites the weapon target");
        button(pixel({0,0,0}),SDL_BUTTON_LEFT,true,ScenePointerTarget{ContactShip,self},true);button(pixel({0,0,0}),SDL_BUTTON_LEFT,false,ScenePointerTarget{ContactShip,self},true);
        require(chat.audience().channel==ChatChannel::Group && game.commandUnits().size()==2 && game.commandUnits().front()==self,"scene Shift-click loses group/self ordering");
        button(point,SDL_BUTTON_LEFT,true,target,true);button(point,SDL_BUTTON_LEFT,false,target,true);
        require(game.commandUnits().size()==1 && game.commandUnits().front()==self,"scene Shift-click does not remove a selected recipient");
        button({20,20},SDL_BUTTON_LEFT,true);button({20,20},SDL_BUTTON_LEFT,false);
        require(!chat.active() && game.commandUnits().size()==1 && game.commandUnits().front()==self,"empty point click clears retained original recipients");
        // Compose Go-to and finish it with the left button. The old native
        // path initialized the ground anchor only for the right button.
        game.pickCommandUnit(self);chat.selectUnits(game);chat.verb(game,c_cidGoto);
        auto destination=pixel({200,100,0});button(destination,SDL_BUTTON_LEFT,true);button(destination,SDL_BUTTON_LEFT,false);
        auto* waypoint=game.playerShip()->GetCommandTarget(c_cmdAccepted);
        constexpr float waypointTolerance=.1f; // Subpixel float projection/ray error, in meters.
        if(chat.active() || game.playerShip()->GetCommandID(c_cmdAccepted)!=c_cidGoto || !waypoint || waypoint->GetObjectType()!=OT_buoy || (waypoint->GetPosition()-Vector(200,100,0)).Length()>=waypointTolerance){
            auto position=waypoint?waypoint->GetPosition():Vector(0,0,0);
            fprintf(stderr,"Scene waypoint: view %d, compose %d, accepted %d, object %d, position %.6f %.6f %.6f\n",int(mode),int(chat.mode()),int(game.playerShip()->GetCommandID(c_cmdAccepted)),waypoint?int(waypoint->GetObjectType()):-1,position.x,position.y,position.z);
        }
        require(!chat.active() && game.playerShip()->GetCommandID(c_cmdAccepted)==c_cidGoto && waypoint && waypoint->GetObjectType()==OT_buoy && (waypoint->GetPosition()-Vector(200,100,0)).Length()<waypointTolerance,"left-button composed waypoint is absent or uses a stale anchor");
        auto* home=game.core()->GetSide(0)->GetStations()->first()->data();
        button({400,300},SDL_BUTTON_RIGHT,true,ScenePointerTarget{ContactStation,home->GetObjectID()});button({400,300},SDL_BUTTON_RIGHT,false,ScenePointerTarget{ContactStation,home->GetObjectID()});
        require(!chat.active() && game.playerShip()->GetCommandTarget(c_cmdAccepted)==home,"pilot/command right-click does not dispatch the original object order");
        game.pickCommandUnit(self);chat.selectUnits(game);
        button({10,10},SDL_BUTTON_LEFT,true);button({30,30},SDL_BUTTON_LEFT,false);
        require(!chat.active() && game.commandUnits().empty() && !gesture.selectionStart && !gesture.orderStart && !gesture.anchor,"empty rectangle retains recipients or pointer capture");
    }
    GameState blocked=game.state();blocked.commandView=false;blocked.docked=true;require(!scenePointerEnabled(blocked,true,true),"hangar accepts scene gestures");
    blocked.docked=false;blocked.dead=true;require(!scenePointerEnabled(blocked,true,true),"dead pilot accepts scene gestures");
    blocked.commandView=true;blocked.networkLobby=true;require(!scenePointerEnabled(blocked,true,true),"lobby accepts stale command gestures");
    blocked.networkLobby=false;blocked.complete=true;require(!scenePointerEnabled(blocked,true,true),"completed game accepts stale command gestures");
    puts("Original free-cursor pilot/command input: perspective picking, recipient/weapon-target separation, Shift groups, retained recipients, left-button waypoint, right-button object orders, empty rectangles and inactive screens passed without rendering");
}
void slideshowMenus(Native::Assets& assets){
    using namespace Native;
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);
    auto mission=[&]{return std::any_of(ui.view().items.begin(),ui.view().items.end(),[](const auto& item){return item.label=="Mission";});};
    for(bool post:{false,true}){
        // Debriefing may still have the completed training Game in the host.
        // Slideshow menu availability depends on the screen, not that object.
        ui.setPlaying(post);ui.trainingSlides(true);
        require(ui.screen()==Screen::Slideshow && ui.view().original && ui.view().overlay && ui.view().artwork.empty() && ui.view().items.empty() && ui.mainMenuInput(),"slideshow invents a new menu or blocks the global input filter");
        for(auto popup:{Screen::Pause,Screen::Settings,Screen::Help,Screen::Controls}){
            ui.open(popup);
            require(ui.keyboardPopup()==(popup!=Screen::Help) && ui.mainMenuInput()==(popup==Screen::Help),"slideshow popup ownership or independent Help filter differs");
            if(popup==Screen::Pause){
                require(mission() && std::none_of(ui.view().items.begin(),ui.view().items.end(),[](const auto& item){return item.label=="Game Details" || item.label=="Player Pings";}),"slideshow menu loses Mission or invents live-mission information");
            }
            auto close=popup==Screen::Help?ui.toggleHelp():ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            require(close.effect==(popup==Screen::Help?FrontEffect::None:FrontEffect::Resume) && ui.screen()==Screen::Slideshow && !ui.keyboardPopup() && ui.mainMenuInput() && ui.view().artwork.empty(),"slideshow popup return opens lesson selection, retains a stale menu or hides the live image");
        }
        ui.open(Screen::Pause);ui.key(SDLK_Q,SDL_SCANCODE_Q);
        require(ui.hasMessageBox() && std::any_of(ui.messageView().artwork.begin(),ui.messageView().artwork.end(),[](const auto& control){return control.text=="Quit the Current Training Mission?";}),"slideshow Mission does not use the original training quit confirmation");
        require(ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE).effect==FrontEffect::Resume && ui.screen()==Screen::Slideshow && ui.mainMenuInput(),"cancel training quit fails to resume the slideshow");
        ui.open(Screen::Pause);ui.key(SDLK_Q,SDL_SCANCODE_Q);
        require(ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect==FrontEffect::Leave,"confirmed slideshow quit does not leave training");
        ui.trainingSlides(false);ui.open(Screen::Training);ui.open(Screen::Pause);
        require(!mission(),"completed/aborted slideshow leaks Mission into lesson selection");
        ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    }
    puts("Original slideshow menus: introduction/debrief, mapped-menu eligibility, options/help/mapping capture and return, training quit/cancel and retired Mission entry passed without a game");
}
void creation(Native::Assets& assets){
    using namespace Native;
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);
    auto item=[&](MenuOp op,int value=0){auto i=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& c){return c.action.op==op && c.action.value==value;});if(i==ui.view().items.end()){fprintf(stderr,"Missing original dialog action %d value %d on screen %d\n",int(op),value,int(ui.screen()));throw std::runtime_error("original dialog action missing");}return size_t(i-ui.view().items.begin());};
    auto selected=[&](MenuOp op){return std::count_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& c){return c.action.op==op && c.selected;});};
    auto enabled=[&]{return ui.view().items[item(MenuOp::GameCreateAccept)].enabled;};
    auto blank=[&](MenuOp op,bool right){auto index=item(MenuOp::ListClear,int(op));auto b=ui.view().items[index].bounds;auto layout=originalMenuLayout({800,600},ui.view().classicScale);auto hit=menuHit(ui.view(),{800,600},1,layout.pixel({b.x+2,b.y+b.w-2}));require(hit && *hit==index,"creation blank viewport does not own its hit region");if(right)ui.pointerContext(hit);else ui.pointerPress(hit);require(!ui.pointerCaptured() && ui.pointerRelease({}).effect==FrontEffect::None,"creation blank captures or repeats selection");};
    ui.open(Screen::Games);
    // CreateGameDialog's fresh ListPanes have NULL selections. Its OnChar
    // forwards Return to an EditPane with no string-event subscriber.
    for(bool right:{false,true})for(bool doubleClick:{false,true}){
        ui.activate(item(MenuOp::GameCreate));auto name=ui.createName();
        require(!enabled() && !selected(MenuOp::GameCreateServer) && !selected(MenuOp::GameCreateCore),"Create Game invents initial list selections");
        require(authoredControl(ui.view().artwork,"createOkButtonPane").visibleBounds().z==0,"unselected Create Game draws its hidden OK");
        for(auto key:{SDLK_RETURN,SDLK_SPACE,SDLK_TAB,SDLK_UP,SDLK_DOWN})require(ui.key(key,SDL_GetScancodeFromKey(key,nullptr)).effect==FrontEffect::None && ui.screen()==Screen::Games && ui.createName()==name && !enabled(),"Create Game adds a button/list key handler");
        auto press=[&](MenuOp op){auto index=item(op);if(right)ui.pointerContext(index,doubleClick);else ui.pointerPress(index,doubleClick);require(!ui.pointerCaptured() && ui.pointerRelease({}).effect==FrontEffect::None,"creation list selects on release or captures the mouse");};
        press(MenuOp::GameCreateServer);require(!enabled() && selected(MenuOp::GameCreateServer)==1,"server selection invents a core");
        press(MenuOp::GameCreateCore);require(enabled() && selected(MenuOp::GameCreateCore)==1,"creation list mouse-down selection is missing");
        require(ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect==FrontEffect::None && enabled() && ui.screen()==Screen::Games,"Create Game Return creates a mission");
        ui.text(" retained");require(ui.createName()==name+" retained" && ui.wantsText(),"creation list click loses original edit focus");
        blank(MenuOp::GameCreateServer,right);require(!enabled() && !selected(MenuOp::GameCreateServer) && selected(MenuOp::GameCreateCore)==1,"server blank clears both or neither selection");
        press(MenuOp::GameCreateServer);blank(MenuOp::GameCreateCore,right);require(!enabled() && !selected(MenuOp::GameCreateCore) && selected(MenuOp::GameCreateServer)==1,"core blank clears both or neither selection");
        ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);require(!ui.keyboardPopup() && !ui.wantsText() && ui.screen()==Screen::Games,"Create Game Escape loses its Games parent");
    }
    std::vector<Net::AuthorityListing> authorities;
    for(int i=0;i<16;++i){Net::AuthorityListing a;a.address="127.0.0.1:"+std::to_string(29000+i);Net::text(a.info.server,"Server "+std::to_string(i));a.info.capacity=16;a.cores=assets.coreCatalog();if(!Allegiance::FindCore(a.cores,"static_core"))a.cores.insert(a.cores.begin(),"static_core");for(int c=0;c<8;++c)a.cores.push_back("foreign"+std::to_string(c));authorities.push_back(std::move(a));}
    ui.gameAuthorities(authorities);ui.activate(item(MenuOp::GameCreate));auto name=ui.createName();
    auto wheel=[&](MenuOp op,float amount){auto b=ui.view().items[item(MenuOp::ListClear,int(op))].bounds;ui.pointerMove(glm::vec2(b)+glm::vec2(4,4));require(ui.pointerWheel(amount),"creation wheel is not consumed by its popup");};
    wheel(MenuOp::GameCreateServer,1);wheel(MenuOp::GameCreateCore,1);require(!selected(MenuOp::GameCreateServer) && !selected(MenuOp::GameCreateCore),"upward creation wheel invents an initial selection");
    wheel(MenuOp::GameCreateServer,-1);wheel(MenuOp::GameCreateCore,-1);require(enabled() && ui.createAddress().empty() && std::string_view(ui.createCore())=="static_core" && ui.createName()==name,"creation wheel skips first row, changes name or fails compatibility gates");
    for(int i=0;i<14;++i)wheel(MenuOp::GameCreateServer,-1);
    require(!ui.createAddress().empty() && enabled(),"creation wheel cannot reach overflowing server rows");
    for(int i=0;i<14;++i)wheel(MenuOp::GameCreateCore,-1);
    require(std::string_view(ui.createCore()).starts_with("foreign") && enabled(),"creation wheel cannot reach overflowing supported-core rows");
    for(int i=0;i<14;++i)wheel(MenuOp::GameCreateServer,1);
    require(!enabled() && !selected(MenuOp::GameCreateCore),"local selection retains an unsupported remote-only core");
    wheel(MenuOp::GameCreateCore,-1);require(enabled(),"cleared core wheel fails to return to first supported core");
    for(int i=0;i<14;++i)wheel(MenuOp::GameCreateCore,-1);
    require(!enabled() && !selected(MenuOp::GameCreateServer),"remote-only core retains an incompatible local server");
    ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.activate(item(MenuOp::GameCreate));
    require(!enabled() && ui.createName()==name && !selected(MenuOp::GameCreateCore) && !selected(MenuOp::GameCreateServer),"reopened creation retains rejected selections or name draft");
    auto server=ui.view().items[item(MenuOp::GameCreateServer)].bounds,core=ui.view().items[item(MenuOp::GameCreateCore)].bounds;
    require(server.y==authoredControlBounds(ui.view().artwork,"serverListPane").y && core.y==authoredControlBounds(ui.view().artwork,"coreListPane").y,"reopened creation keeps scrolled list offsets");
    ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    ui.activate(item(MenuOp::GameFilter));ui.text("No matching game");ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);require(!ui.keyboardPopup() && ui.screen()==Screen::Games,"FilterDialog loses its explicit Return acceptance");
    ui.activate(item(MenuOp::FindPlayer));ui.text("Pilot");require(ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect==FrontEffect::FindPlayer && ui.hasMessageBox(),"FindDialog loses its explicit Return lookup");ui.foundPlayer({});ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
    puts("Original creation/dialog input: empty initial selections, four mouse-button/double-click combinations, edit focus, Return distinction, blank selection, overflowing wheel navigation, local/remote compatibility and fresh reopening passed");
}
void officialBadges(Native::Assets& installed){
    using namespace Native;
    NativeTest::AuthoredAssets fixture(installed.root());std::filesystem::remove(fixture.root/"allegiance.txt");
    {std::ofstream out(fixture.root/"allegiance.txt");out<<"[OfficialServers]\nOfficial host=127.0.0.1\nIPv6 host=2001:db8::1\n[OfficialCores]\nstatic_core=true\n";}
    Assets assets(fixture.root);auto badge=assets.image("iconscorescountbmp",true);Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.open(Screen::Games);
    std::vector<Net::ServerListing> listings;
    for(int i=0;i<4;++i){Net::ServerInfo info;info.flags=Net::ServerWaiting|Net::ServerJoinable;Net::text(info.name,"Game "+std::to_string(i));Net::text(info.core,i==2?"zone_core":"static_core");Net::text(info.server,i==1?"IPv6 host":"Official host");info.session.rules.developments=false;
        listings.push_back({i==1?"[2001:db8::1]:29502":i==3?"127.0.0.2:29500":"127.0.0.1:"+std::to_string(29500+i),info});}
    ui.gameListings(listings);require(std::count_if(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.image==badge;})==2,"Games List official marker ignores server host/core conjunction");
    std::vector<Net::AuthorityListing> authorities;
    for(const auto& game:listings){Net::AuthorityListing a;a.address=game.address;Net::text(a.info.server,game.info.server);a.info.capacity=16;a.cores={"static_core"};authorities.push_back(std::move(a));}
    ui.gameAuthorities(authorities);auto open=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& i){return i.action.op==MenuOp::GameCreate;});ui.activate(size_t(open-ui.view().items.begin()));
    auto bounds=authoredControlBounds(ui.view().artwork,"serverListPane");
    require(std::count_if(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.image==badge && c.bounds.x==bounds.x;})==3,"Create Game official-server markers are absent, unpinned or invented");
    auto pick=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& i){return i.action.op==MenuOp::GameCreateServer && i.action.value==1;});ui.pointerPress(size_t(pick-ui.view().items.begin()));
    auto fill=std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.kind==OriginalControl::Kind::Rectangle && c.color==glm::vec4(0,0,.7,1);});require(fill!=ui.view().artwork.end(),"selected server fill missing");
    require(std::any_of(fill,ui.view().artwork.end(),[&](const auto& c){return c.image==badge && c.bounds.x==bounds.x && c.bounds.y+c.bounds.w/2==fill->bounds.y+7;}),"server selection covers official badge or moves its original center");
    puts("Original official markers: configured server name/address, IPv4/IPv6 native endpoints, core conjunction, per-server creation badges and selected-row painter order passed");
}
}
int main(int argc,char** argv){
    try{
        Native::Assets assets(argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork());
        NativeTest::checkCompiledOverlays(assets);
        NativeTest::checkHelpOverlay(assets);
        NativeTest::checkUiFeedback(assets);
        creation(assets);officialBadges(assets);slideshowMenus(assets);mouseControl(assets);NativeTest::checkBrowseIdentity(assets);
        NativeTest::checkOriginalLocalGame(assets);NativeTest::checkTrainingMenu(assets);NativeTest::checkTrainingArtwork(assets);
        NativeTest::checkClassicKeyboard(assets);NativeTest::checkListInput(assets);
        NativeTest::checkFullLobby(assets);
        NativeTest::checkPlayerPainterClips(assets);
        NativeTest::checkResearchCards(assets);
        NativeTest::checkLoadoutMetrics(assets);
        NativeTest::checkRemoteTeleport(assets);
        NativeTest::checkTeleportHeaders(assets);
        NativeTest::checkButtonInput(assets);
        NativeTest::checkTeamActivity(assets);
        NativeTest::checkLoadoutEditor(assets);NativeTest::checkLoadoutPresets(assets);NativeTest::checkDockedWorkflows(assets);
        return 0;
    }catch(const std::exception& e){fprintf(stderr,"Original dialogs: %s\n",e.what());return 1;}
}
