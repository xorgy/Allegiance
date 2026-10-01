#pragma once
#include "../frontend.hpp"
#include "../player_pane.hpp"
#include "../game_state_pane.hpp"
#include "../ui_constants.hpp"
#include "../test_fixture.hpp"
#include "authored_hosts.hpp"
#include "../../ClientCore/CommunityLinks.h"
#include <chrono>
#include <thread>

namespace NativeTest {
inline void checkUiFeedback(Native::Assets& assets){
    using namespace Native;
    // Frozen event-sink choices from WinTrek/trekmdl.cpp and CreateMessageBox.
    OriginalUi compiler(assets);
    struct Primitive{const char* script;const char* root;SoundID click,hover,select;};
    for(auto p:std::array<Primitive,10>{{
        {"introscreen","trainBigButtonPane",positiveButtonClickSound,mouseoverSound,-1},
        {"gamescreen","backButtonPane",negativeButtonClickSound,mouseoverSound,-1},
        {"loadoutpane","defaultButtonPane",changeLoadoutSound,mouseoverSound,-1},
        {"newgamescreen","gameTypeComboPane",-1,mouseoverSound,mouseclickSound},
        {"logon","abortButtonPane",negativeButtonClickSound,mouseoverSound,-1},
        {"helppane","closeButton",negativeButtonClickSound,mouseoverSound,-1},
        {"dialog","gameStateCloseButton",negativeButtonClickSound,mouseoverSound,-1},
        {"mmlpane","closeButton",-1,-1,-1},
        {"teleportpane","teleportPaneDestinationListHeader0",mouseclickSound,mouseoverSound,-1},
        {"hangar","launchButtonPane",-1,-1,-1}}}){
        auto root=std::string(p.root);if(root.ends_with("Header0"))root.pop_back();
        auto layout=compiler.layout(p.script,root);auto c=std::find_if(layout.begin(),layout.end(),[&](const auto& c){return c.name==p.root;});
        require(c!=layout.end() && c->clickSound==p.click && c->hoverSound==p.hover && c->selectSound==p.select,"original MDL primitive loses its actual Trek sound sinks");
    }
    require(originalSoundId("negativeButtonClickSoundId")==negativeButtonClickSound && originalSoundId("changeLoadoutSoundId")==changeLoadoutSound && !originalSoundId("NotASoundId"),"authored sound namespace exports differ from IGC IDs");
    auto cue=[](auto& owner,SoundID expected,const char* message){require(owner.sounds().size()==1 && owner.sounds()[0]==expected,message);owner.clearSounds();};
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);
    auto item=[&](std::string_view name){auto i=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& c){return c.control==name;});if(i==ui.view().items.end())throw std::runtime_error("Original feedback control missing: "+std::string(name)+" on screen "+std::to_string(int(ui.screen())));return size_t(i-ui.view().items.begin());};
    auto hover=[&](std::string_view name){ui.pointerHover(item(name));};
    hover("zoneWebButtonPane");cue(ui,mouseoverSound,"main-menu enabled hover misses its original cue");
    ui.open(Screen::Home);hover("zoneWebButtonPane");require(ui.sounds().empty(),"retained layout repeats hover sound without pointer reentry");
    ui.pointerHover({});hover("zoneWebButtonPane");cue(ui,mouseoverSound,"button reentry omits hover cue");
    ui.pointerPress(item("zoneWebButtonPane"));require(ui.sounds().empty(),"release button plays click on press");
    auto web=ui.pointerRelease(item("zoneWebButtonPane"));cue(ui,positiveButtonClickSound,"normal button release loses positive click cue");
    require(web.effect==FrontEffect::Web && std::string_view(Allegiance::CommunityLink(web.value))=="https://www.freeallegiance.org/forums" && ui.screen()==Screen::Home,"original main-menu Web action opens the wrong destination or changes screens");
    ui.open(Screen::Games);ui.clearSounds();
    ui.pointerPress(item("backButtonPane"));ui.pointerRelease({});require(ui.sounds().empty() && ui.screen()==Screen::Games,"cancelled release sounds or activates a button");
    ui.pointerContext(item("backButtonPane"));require(ui.sounds().empty(),"ordinary right button invents a sound sink");
    ui.pointerPress(item("backButtonPane"),true);ui.pointerRelease(item("backButtonPane"),true);require(ui.sounds().empty() && ui.screen()==Screen::Games,"unsubscribed double click repeats a normal event sound");
    ui.pointerPress(item("backButtonPane"));ui.pointerRelease(item("backButtonPane"));cue(ui,negativeButtonClickSound,"SoundButtonPane ignores its explicit negative cue");
    ui.error("Feedback fixture");ui.clearSounds();hover("messageOK");cue(ui,mouseoverSound,"modal button hover is suppressed with its captured parent");
    ui.pointerPress(item("messageOK"));ui.pointerRelease(item("messageOK"));cue(ui,positiveButtonClickSound,"message acknowledgment loses original click sound");
    ui.confirm("Cancel fixture",{FrontEffect::Quit});ui.clearSounds();hover("messageOK");ui.clearSounds();ui.pointerPress(item("messageOK"));
    require(ui.messageView().capturedButton() && ui.messageView().capturedButton()->control=="messageOK","split modal loses captured-button event metadata");
    ui.displayScale(1);require(ui.messageView().capturedButton() && ui.messageView().capturedButton()->control=="messageOK","modal refresh restores capture before its active layer exists");
    hover("messageCancel");require(ui.sounds().empty(),"captured modal lets its other button emit hover");ui.pointerRelease(item("messageCancel"));require(ui.hasMessageBox(),"modal release over the other button activates it");
    ui.pointerPress(item("messageCancel"));ui.pointerRelease(item("messageCancel"));cue(ui,negativeButtonClickSound,"message cancellation loses original negative sound");
    SessionOptions options;options.scenario=Scenario::Conquest;options.rules.miners=0;options.startInHangar=true;
    ui.networkSession(options);ui.open(Screen::Lobby);ui.clearSounds();hover("gameTypeComboPane");cue(ui,mouseoverSound,"combo hover omits original cue");
    ui.pointerPress(item("gameTypeComboPane"));require(ui.sounds().empty(),"combo opening invents a click instead of waiting for MenuSelect");
    ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);cue(ui,mouseclickSound,"keyboard combo choice omits or duplicates MenuSelect sound");
    ui.pointerPress(item("gameTypeComboPane"));auto choice=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& c){return c.popupLevel>=0 && c.enabled;});require(choice!=ui.view().items.end(),"combo choice fixture");auto index=size_t(choice-ui.view().items.begin());
    ui.pointerPress(index);require(ui.sounds().empty(),"combo row press sounds before selection");ui.pointerRelease(index);cue(ui,mouseclickSound,"pointer combo choice omits or duplicates MenuSelect sound");
    ui.open(Screen::Home);ui.open(Screen::Help);ui.clearSounds();
    for(auto control:{"DiscordButton","WikiButton"}){const auto& pane=ui.helpView();auto found=std::find_if(pane.items.begin(),pane.items.end(),[&](const auto& i){return i.control==control;});require(found!=pane.items.end(),"authored Help link missing");size_t index=size_t(found-pane.items.begin());ui.helpPress(index);auto result=ui.helpRelease(index);require(result.effect==FrontEffect::Web && result.value==(std::string_view(control)=="WikiButton"?Allegiance::WikiLink:0) && ui.screen()==Screen::Home && ui.helpOn(),"Help link loses its original target or replaces its underlying screen");ui.clearSounds();}
    require(std::string_view(Allegiance::CommunityLink(Allegiance::WikiLink))=="http://www.freeallegiance.org/FAW/index.php/Quick_Crash_Course","Help Wiki maps to a generic website");
    ui.missionDetailsService([]{return Allegiance::MissionDetails{};});ui.open(Screen::GameDetails);ui.clearSounds();
    auto close=std::find_if(ui.view().items.rbegin(),ui.view().items.rend(),[](const auto& c){return c.action.op==MenuOp::DetailsClose;});require(close!=ui.view().items.rend(),"layered MML close fixture");
    auto closeIndex=ui.view().items.size()-1-size_t(close-ui.view().items.rbegin());
    ui.pointerMove(glm::vec2(ui.view().items[closeIndex].bounds)+glm::vec2(3));ui.pointerHover(closeIndex);ui.pointerPress(closeIndex);
    require(ui.screen()==Screen::GameDetails && ui.detailsView().capturedButton() && ui.detailsView().capturedButton()->control=="closeButton","split Details button loses capture or fires on press");
    ui.displayScale(1);require(ui.detailsView().capturedButton() && ui.detailsView().capturedButton()->control=="closeButton","Details refresh loses top-layer capture");ui.pointerRelease(closeIndex);
    require(ui.sounds().empty() && ui.screen()==Screen::Home && ui.helpOn(),"silent MML close changes independent Help or inherits another layer's sound sinks");
    ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    // The installed MML pane omits these optional buttons. Verify the actual
    // asset-defined variant instead of manufacturing controls in production.
    {
        AuthoredAssets variant(assets.root());variant.change("mmlpane.mdl",{
            {"mmlPane =","WikiButton = ButtonPane(ImportImageFromFile(\"menuhelp/btnwiki.png\", true), ButtonNormal, false);\nDiscordButton = ButtonPane(ImportImageFromFile(\"menuhelp/btndiscord.png\", true), ButtonNormal, false);\nmmlPane ="},
            {"(closeButton,       Point(470, 10))","(closeButton,       Point(470, 10)),\n(WikiButton, Point(20, 245)),\n(DiscordButton, Point(120, 245))"}});
        Assets alternate(variant.root);Frontend popup(settings);popup.originalInterface(alternate);popup.missionDetailsService([]{return Allegiance::MissionDetails{};});popup.open(Screen::GameDetails);
        for(auto control:{"DiscordButton","WikiButton"}){
            auto i=std::find_if(popup.view().items.begin(),popup.view().items.end(),[&](const auto& c){return c.control==control;});require(i!=popup.view().items.end(),"alternate MML pane omits its authored link");
            auto result=popup.activate(size_t(i-popup.view().items.begin()));require(result.effect==FrontEffect::Web && result.value==(std::string_view(control)=="WikiButton"?Allegiance::WikiLink:0) && popup.screen()==Screen::GameDetails,"MML popup link loses its original target or parent");
            cue(popup,positiveButtonClickSound,"alternate MML button loses its authored event sink");
        }
    }
    {
        UiSoundFeedback feedback;MenuItem button{{},"",{},{MenuOp::PopupItem}};button.control="retained";button.hoverSound=mouseoverSound;button.clickSound=positiveButtonClickSound;
        button.enabled=false;feedback.hover(&button);feedback.click(button);require(feedback.sounds().empty(),"disabled control emits feedback");
        button.enabled=true;button.blocked=true;feedback.hover(&button);feedback.click(button);require(feedback.sounds().empty(),"captured parent emits feedback");
        button.blocked=false;feedback.hover(&button);feedback.clear();feedback.capturedHover(button,false);feedback.capturedHover(button,true);require(feedback.sounds().empty(),"captured leave/reentry emits an original MouseEnter that does not occur");
        button.enabled=false;feedback.capturedHover(button,true);button.enabled=true;feedback.capturedHover(button,false);feedback.capturedHover(button,true);require(feedback.sounds().empty(),"enabling a captured button outside emits on reentry");
        button.enabled=false;feedback.capturedHover(button,true);button.enabled=true;feedback.capturedHover(button,true);require(feedback.sounds().size()==1 && feedback.sounds()[0]==mouseoverSound,"captured SetEnabled while inside loses its enabled-enter event");feedback.clear();
        button.blocked=false;for(int i=0;i<1000;++i)feedback.click(button);require(feedback.sounds().size()==16,"input feedback grows without bound");
    }
    {
        Frontend purchases(settings);purchases.originalInterface(assets);purchases.setPlaying(true);
        GameState docked;docked.docked=docked.economy=true;docked.money=1000;docked.ship="Scout";docked.station="Home";
        ResearchChoice row;row.id=11;row.type=OT_development;row.name="Research fixture";row.model="missing-card";row.price=100;row.available=row.canBuy=row.predecessorReady=true;
        purchases.researchService([&]{return std::vector{row};});purchases.open(Screen::Hangar,&docked);purchases.open(Screen::Research,&docked);
        auto find=[&](MenuOp op,int value=0){auto i=std::find_if(purchases.view().items.begin(),purchases.view().items.end(),[&](const auto& c){return c.action.op==op && c.action.value==value;});require(i!=purchases.view().items.end(),"hand-built purchase control fixture");return size_t(i-purchases.view().items.begin());};
        for(const auto& c:purchases.view().items)if(c.action.op==MenuOp::ResearchTab || c.action.op==MenuOp::ResearchFilter || c.action.op==MenuOp::ResearchActivate || c.control=="btnclosepanebmp")
            require(c.clickSound==(c.action.op==MenuOp::ResearchTab?mouseclickSound:positiveButtonClickSound) && c.hoverSound==mouseoverSound && c.detectWhenDisabled,"PurchasesPane loses its direct CreateTrekButton policy");
        purchases.pointerPress(find(MenuOp::ResearchTab));purchases.pointerRelease(find(MenuOp::ResearchTab));cue(purchases,mouseclickSound,"purchase tab loses explicit click cue");
        purchases.pointerPress(find(MenuOp::ResearchFilter));purchases.pointerRelease(find(MenuOp::ResearchFilter));cue(purchases,positiveButtonClickSound,"purchase checkbox substitutes the MDL factory cue for CreateTrekButton's default");
        auto both=[&]{require(purchases.sounds().size()==2 && purchases.sounds()[0]==mouseoverSound && purchases.sounds()[1]==mouseoverSound,"Invest enabled-enter and information-enter sinks are distinct");purchases.clearSounds();};
        purchases.pointerHover(find(MenuOp::ResearchActivate));both();purchases.pointerHover(find(MenuOp::ResearchActivate));require(purchases.sounds().empty(),"stationary Invest pointer repeats sound");
        purchases.pointerHover(find(MenuOp::ResearchSelect,11));cue(purchases,mouseoverSound,"return from Invest omits list information enter");purchases.pointerHover(find(MenuOp::ResearchActivate));both();
        std::this_thread::sleep_for(std::chrono::milliseconds(520));
        docked.money=0;++docked.economyRevision;purchases.sync(docked);purchases.pointerHover({});purchases.clearSounds();
        auto disabled=find(MenuOp::ResearchActivate);const auto& v=purchases.view();auto bounds=v.items[disabled].bounds;
        glm::ivec2 size{1280,720};auto pixel=originalMenuLayout(size,v.classicScale,v.canvas).pixel(glm::vec2(bounds)+glm::vec2(3)+menuSlideOffset(v,size,uiTime()));
        require(menuHit(v,size,1,pixel)==disabled,"disabled button's hover/hit falls through to its parent");
        purchases.pointerHover(menuHit(purchases.view(),size,1,pixel));cue(purchases,mouseoverSound,"disabled Invest loses its information-enter sink");
        require(std::any_of(purchases.view().artwork.begin(),purchases.view().artwork.end(),[](const auto& c){return c.name=="investmentInfoName" && c.text=="Research fixture";}),"disabled Invest does not display selected bucket information");
        purchases.pointerPress(disabled);require(purchases.pointerCaptured(),"disabled button loses original press capture");require(purchases.pointerRelease(disabled).effect==FrontEffect::None && !purchases.pointerCaptured() && purchases.sounds().empty(),"disabled Invest emits click or purchases");
        docked.money=1000;++docked.economyRevision;purchases.sync(docked);
        purchases.pointerHover(menuHit(purchases.view(),size,1,pixel));cue(purchases,mouseoverSound,"enabling a stationary hovered button misses enabled-enter feedback");
        purchases.sync(docked);purchases.pointerHover(menuHit(purchases.view(),size,1,pixel));require(purchases.sounds().empty(),"unchanged enabled button repeats feedback during refresh");
    }
    // Independent flight consoles use the same primitive metadata and queue.
    Game game(assets.root(),[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
    PlayerPane players(assets);GameStatePane state(assets);players.toggle(game);state.toggle(game);
    glm::ivec2 size{1280,720};players.view(size,1);state.view(size,1);
    std::this_thread::sleep_for(std::chrono::milliseconds(520));
    auto point=[&](const MenuView& v,const MenuItem& i){return originalMenuLayout(size,1,v.canvas).pixel(glm::vec2(i.bounds)+glm::vec2(3)+menuSlideOffset(v,size,uiTime()));};
    auto& roster=players.view(size,1);auto sort=std::find_if(roster.items.begin(),roster.items.end(),[](const auto& c){return c.action.op==MenuOp::Order;});require(sort!=roster.items.end(),"F6 sort feedback fixture");auto p=point(roster,*sort);
    players.hover(size,p);cue(players,mouseoverSound,"F6 header hover lacks original ButtonBar cue");players.sync(game,true);players.view(size,1);players.hover(size,p);require(players.sounds().empty(),"F6 report refresh repeats hover cue");
    players.pointer(size,p,SDL_BUTTON_RIGHT,true);require(players.pointerCaptured() && players.view(size,1).capturedButton(),"F6 ordinary right button does not capture");
    players.pointer(size,p,SDL_BUTTON_RIGHT,false);require(!players.pointerCaptured() && players.sounds().empty(),"F6 right header invokes its left sort/sound");
    players.pointer(size,p,SDL_BUTTON_LEFT,true);require(players.sounds().empty(),"F6 header sounds on press");players.pointer(size,p,SDL_BUTTON_LEFT,false);cue(players,mouseclickSound,"F6 header release lacks original ButtonBar cue");
    auto& stats=state.view(size,1);require(!stats.items.empty(),"Game State close feedback fixture");p=point(stats,stats.items.front());state.hover(size,p);cue(state,mouseoverSound,"Game State close hover lacks original cue");
    state.pointer(size,p,SDL_BUTTON_RIGHT,true);require(state.pointerCaptured() && state.view(size,1).capturedButton(),"Game State right close button does not capture");
    state.pointer(size,p,SDL_BUTTON_RIGHT,false);require(state.active() && !state.pointerCaptured() && state.sounds().empty(),"Game State right close invokes left callback/sound");
    state.pointer(size,p,SDL_BUTTON_LEFT,true);require(state.sounds().empty(),"Game State close sounds on press");state.pointer(size,p,SDL_BUTTON_LEFT,false);cue(state,negativeButtonClickSound,"Game State close loses explicit negative cue");
    int peer=game.addLocalPilot("Donation fixture",0);require(peer>=0,"F6 live-disable fixture");players.sync(game,true);auto& updated=players.view(size,1);
    auto peerItem=std::find_if(updated.items.begin(),updated.items.end(),[&](const auto& c){return c.action.op==MenuOp::RosterSelect && c.action.value==peer;});require(peerItem!=updated.items.end(),"F6 live-disable peer row");p=point(updated,*peerItem);
    players.pointer(size,p,SDL_BUTTON_LEFT,true);players.pointer(size,p,SDL_BUTTON_LEFT,false);const auto& selected=players.view(size,1);
    auto donate=std::find_if(selected.items.begin(),selected.items.end(),[&](const auto& c){return c.action.op==MenuOp::Donate && c.action.value==peer;});require(donate!=selected.items.end() && donate->enabled,"F6 live-disable Donate control");p=point(selected,*donate);
    auto down=players.pointer(size,p,SDL_BUTTON_RIGHT,true);require(down && down->effect==FrontEffect::Donate && down->command==500 && players.pointerCaptured(),"F6 repeat-configured right donation does not fire once on press");cue(players,positiveButtonClickSound,"F6 right donation loses its explicit handler sound");
    require(!players.tick(1),"right Donate installs a repeating timer absent from original ButtonUIPane");
    auto cancelled=players.pointer(size,{-1,-1},SDL_BUTTON_RIGHT,false);require(cancelled && cancelled->effect==FrontEffect::None && !players.pointerCaptured(),"F6 right donation emits a second command on release");
    down=players.pointer(size,p,SDL_BUTTON_LEFT,true);require(down && down->effect==FrontEffect::Donate && players.pointerCaptured(),"F6 repeat-configured left donation does not fire on press");cue(players,positiveButtonClickSound,"F6 initial left donation loses its ButtonPane sink");
    require(game.donateMoney(peer,game.playerPaneData().money),"F6 wallet depletion fixture");players.sync(game,true);players.view(size,1);auto release=players.pointer(size,p,SDL_BUTTON_LEFT,false);
    require(release && release->effect==FrontEffect::None && !players.pointerCaptured() && players.sounds().empty(),"F6 Donate still activates or sounds after becoming disabled during capture");
    players.pointer(size,p,SDL_BUTTON_LEFT,true);require(players.pointerCaptured(),"F6 disabled button does not capture its press");release=players.pointer(size,p,SDL_BUTTON_LEFT,false);
    require(release && release->effect==FrontEffect::None && !players.pointerCaptured() && players.sounds().empty(),"F6 disabled button emits a command");
    puts("Original UI feedback: authored primitive/export IDs, normal/explicit/header/combo sounds, direct PurchasesPane button policies and distinct information hover, disabled-button hover without activation, stationary enable/retained refresh, unused/cancelled/double-click silence, immediate left/right donation feedback, modal and F6/Game State events, bounded queues and original Web/Wiki popup targets passed");
}
}
