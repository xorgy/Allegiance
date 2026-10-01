#include "player_pane.hpp"
#include "chat_console.hpp"
#include "game_state_pane.hpp"
#include "ui_pointer_capture.hpp"
#include "ui_console_panels.hpp"
#include "audio.hpp"
#include "test_reference/view_transitions.hpp"
#include "test_reference/roster_state.hpp"
#include "test_fixture.hpp"
#include "../zlib/tlist.h"
#include <thread>
#include <chrono>
#include <cctype>
using namespace Native;
using NativeTest::require;
namespace {
void hangarConsoleActions(const std::filesystem::path& art){
    Assets assets(art);SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.enemies=0;options.rules.miners=0;
    Game game(art,[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
    int peer=game.addLocalPilot("Hangar pane recipient",0);require(peer>=0,"hangar pane fixture");
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.setPlaying(true);
    ui.loadoutServices([&](const Loadout& l,bool defaults,int hull){return game.previewLoadout(l,defaults,hull);},[&](PartKind kind,int mount,int hull){return game.loadoutPartChoices(kind,mount,hull);});
    PlayerPane players(assets);GameStatePane standings(assets);
    constexpr uint32_t competing=ofTeam|ofExpandedTeam|ofGameState;
    struct Sink {const char* control;uint32_t dismissed;};
    // HangarScreen's event sinks dismiss competing overlays. Help, Quit and
    // pending Launch leave them resident; changing view mode clears them.
    constexpr Sink sinks[]={{"investButtonPane",competing},{"teleportButtonPane",competing},{"loadoutButtonPane",competing},{"teamButtonPane",ofGameState},{"commandButtonPane",competing},{"helpButtonPane",0},{"exitButtonPane",0},{"launchButtonPane",0}};
    auto click=[&](const char* name){
        const auto& v=ui.view();auto it=std::find_if(v.items.begin(),v.items.end(),[&](const auto& i){return i.control==name;});
        require(it!=v.items.end() && it->enabled,"original hangar sink missing or unexpectedly disabled");size_t index=size_t(it-v.items.begin());
        auto down=ui.pointerPress(index);auto up=ui.pointerRelease(index);
        return down.effect!=FrontEffect::None || down.dismissPanes?down:up;
    };
    int cases=0;
    for(bool remote:{false,true})for(int size=0;size<3;++size)for(bool scores:{false,true})for(const auto& sink:sinks){
        players.reset();standings.reset();if(ui.hasMessageBox())ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);auto state=game.state();state.networked=remote;ui.open(Screen::Hangar,&state);
        if(size)players.toggle(game,size==1);if(scores)standings.toggle(game);
        auto before=players.mode();auto action=click(sink.control);
        if(action.dismissPanes!=sink.dismissed)throw std::runtime_error(std::string("Original hangar ")+sink.control+" remote="+std::to_string(remote)+" roster="+std::to_string(size)+" scores="+std::to_string(scores)+" dismissal="+std::to_string(action.dismissPanes)+" expected="+std::to_string(sink.dismissed));
        dismissConsolePanes(action,players,standings);
        require(players.mode()==((sink.dismissed&(ofTeam|ofExpandedTeam))?Allegiance::PlayerPaneMode::Hidden:before) && standings.active()==(scores && !(sink.dismissed&ofGameState)),"hangar mouse action retains a competing pane or dismisses an unrelated pane");
        if(action.effect==FrontEffect::PlayerPane){players.toggle(game,action.value!=0);require(players.mode()==Allegiance::TogglePlayerPane(before,false) && ui.screen()==Screen::Hangar,"hangar Team replaces its parent or loses expanded/compact toggle behavior");}
        ++cases;
    }
    // A disabled/modal button must not dismiss a live console panel merely
    // because it carries the same MenuAction as an enabled hangar button.
    for(const auto& sink:sinks){
        players.reset();standings.reset();players.toggle(game);standings.toggle(game);ui.open(Screen::Hangar,&game.state());ui.error("Pending operation");
        auto button=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& i){return i.control==sink.control;});
        require(button!=ui.view().items.end(),"modal hangar parent missing");auto index=size_t(button-ui.view().items.begin());
        auto down=ui.pointerPress(index),up=ui.pointerRelease(index);dismissConsolePanes(down,players,standings);dismissConsolePanes(up,players,standings);
        require(players.active() && standings.active() && ui.hasMessageBox() && !down.dismissPanes && !up.dismissPanes,"blocked hangar event dismisses a resident pane or escapes lockdown");
        ++cases;
    }
    ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.open(Screen::Hangar,&game.state());players.reset();players.toggle(game);players.view({2560,1440},1);std::this_thread::sleep_for(std::chrono::milliseconds(550));
    auto point=[&](MenuOp op,int id){const auto& v=players.view({2560,1440},1);auto i=std::find_if(v.items.begin(),v.items.end(),[&](const auto& item){return item.action.op==op && item.action.value==id;});require(i!=v.items.end(),"held donation fixture control");return originalMenuLayout({2560,1440},1,v.canvas).pixel(glm::vec2(i->bounds)+glm::vec2(3)+menuSlideOffset(v,{2560,1440},uiTime()));};
    auto p=point(MenuOp::RosterSelect,peer);players.pointer({2560,1440},p,SDL_BUTTON_LEFT,true);players.pointer({2560,1440},p,SDL_BUTTON_LEFT,false);
    p=point(MenuOp::Donate,peer);auto donation=players.pointer({2560,1440},p,SDL_BUTTON_LEFT,true);require(donation && donation->effect==FrontEffect::Donate && players.pointerCaptured(),"held donation setup");
    require(game.donateMoney(donation->value,donation->command),"actual initial donation");auto money=game.playerPaneData().money;
    dismissConsolePanes(click("investButtonPane"),players,standings);
    require(!players.active() && !players.pointerCaptured() && !players.tick(5) && game.playerPaneData().money==money,"hangar investment leaves captured donation repeating after pane dismissal");
    SessionOptions training;training.trainingMission=2;training.enemies=0;
    Audio narration(assets,true);
    Game lesson(art,[](auto&,auto&){return ModelHandle{1,1};},0,0,training);
    lesson.startTraining(narration);
    lesson.setTrainingPane(ofGameState,true);standings.reset();standings.toggle(lesson);ui.open(Screen::Hangar,&lesson.state());
    auto help=click("helpButtonPane");dismissConsolePanes(help,players,standings,&lesson);
    require(standings.active() && (lesson.state().trainingPanes&ofGameState),"Help dismisses the training script's Game State overlay");
    auto team=click("teamButtonPane");dismissConsolePanes(team,players,standings,&lesson);
    require(!standings.active() && !(lesson.state().trainingPanes&ofGameState),"training synchronization reopens Game State after the hangar Team action");
    printf("Original hangar console: %d offline/network, compact/expanded, Game State and modal button paths plus captured donation retirement passed\n",cases);
}
void capturedControls(const std::filesystem::path& art){
    Assets assets(art);SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.enemies=0;options.rules.miners=0;
    Game game(art,[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
    int peer=game.addLocalPilot("Repeat recipient",0);require(peer>=0,"captured F6 recipient fixture");
    for(int i=0;i<40;++i)require(game.addLocalPilot("Scrollable recipient "+std::to_string(i),0)>=0,"captured F6 roster fixture");
    PlayerPane pane(assets);pane.toggle(game);glm::ivec2 size{2560,1440};pane.view(size,1);
    Settings settings;Frontend frontend(settings);frontend.originalInterface(assets);frontend.setPlaying(true);frontend.open(Screen::Hangar,&game.state());
    GameStatePane standings(assets);standings.toggle(game);standings.view(size,1);
    struct NoMap {
        bool pointerCaptured()const{return false;}void cancelPointer(){}bool mapPointerMove(glm::vec2){return false;}
        struct Result{std::optional<int> action;};Result mapPointerButton(glm::vec2,int,bool,bool){return {};}
    } noMap;
    UiPointerCapture capture(frontend,pane,standings,noMap);
    std::this_thread::sleep_for(std::chrono::milliseconds(520));
    auto item=[&](MenuOp op,int value=0,int delta=-999){const auto& view=pane.view(size,1);auto found=std::find_if(view.items.begin(),view.items.end(),[&](const auto& i){return i.action.op==op && i.action.value==value && (delta==-999 || i.action.delta==delta);});require(found!=view.items.end(),"captured F6 control missing");return *found;};
    auto pixel=[&](const MenuItem& i){const auto& view=pane.view(size,1);return originalMenuLayout(size,1,view.canvas).pixel(glm::vec2(i.bounds)+glm::vec2(i.bounds.z,i.bounds.w)*.5f+menuSlideOffset(view,size,uiTime()));};
    auto point=[&](MenuOp op,int value=0,int delta=-999){return pixel(item(op,value,delta));};
    auto apply=[&](const std::optional<FrontResult>& result,int amount){require(result && result->effect==FrontEffect::Donate && result->value==peer && result->command==amount,"F6 donation differs from original repeated-button event");
        int before=game.playerPaneData().money;require(game.donateMoney(result->value,result->command) && game.playerPaneData().money==before-amount,"F6 repeat does not transfer real IGC money exactly once");pane.sync(game,true);pane.view(size,1);};
    auto cue=[&]{require(pane.sounds().size()==1 && pane.sounds()[0]==positiveButtonClickSound,"F6 donation event loses or duplicates original sound");pane.clearSounds();};
    auto p=point(MenuOp::RosterSelect,peer);pane.pointer(size,p,SDL_BUTTON_LEFT,true);pane.pointer(size,p,SDL_BUTTON_LEFT,false);pane.view(size,1);p=point(MenuOp::Donate,peer);
    pane.hover(size,p);pane.clearSounds();
    // The same dispatcher as SDL must route capture before a newer modal or
    // Game State hit. Its release must not close the other panel or repeat.
    apply(pane.pointer(size,p,SDL_BUTTON_LEFT,true),100);cue();frontend.error("A newer modal above a held F6 control");
    require(capture.owner()==UiPointerOwner::Players && frontend.hasMessageBox(),"modal lifetime steals F6 pointer capture");
    const auto& close=standings.view(size,1).items.front();auto statePixel=originalMenuLayout(size,1,standings.view(size,1).canvas).pixel(glm::vec2(close.bounds)+glm::vec2(close.bounds.z,close.bounds.w)*.5f+menuSlideOffset(standings.view(size,1),size,uiTime()));
    int delivered=0;auto emit=[&](const FrontResult& r){if(r.effect!=FrontEffect::None){++delivered;apply(r,500);}};auto ignoreMap=[](int){};
    require(capture.move(size,{-1,-1}) && !pane.tick(.51),"higher modal blocks captured outside motion and donation keeps repeating");
    require(capture.button(size,statePixel,SDL_BUTTON_RIGHT,true,false,false,emit,ignoreMap) && capture.owner()==UiPointerOwner::Players && !standings.pointerCaptured(),"secondary press replaces the captured control with a higher panel");
    require(delivered==1,"captured right Donate press does not reach the original held button");cue();delivered=0;
    capture.move(size,p);apply(pane.tick(.11),500);cue();
    require(capture.button(size,statePixel,SDL_BUTTON_RIGHT,false,false,false,emit,ignoreMap) && !capture.active() && !pane.tick(1),"original right release does not retire ordinary button capture");
    apply(pane.pointer(size,p,SDL_BUTTON_LEFT,true),100);cue();
    require(capture.button(size,statePixel,SDL_BUTTON_LEFT,false,false,false,emit,ignoreMap) && !capture.active() && standings.active() && frontend.hasMessageBox() && !pane.tick(1) && !delivered,"release over a newer panel leaves capture/repeat active or executes its control");
    frontend.key(SDLK_RETURN,SDL_SCANCODE_RETURN);frontend.open(Screen::Hangar,&game.state());p=point(MenuOp::Donate,peer);pane.hover(size,p);pane.clearSounds();
    apply(pane.pointer(size,p,SDL_BUTTON_LEFT,true),100);cue();capture.cancel();
    require(!capture.active() && !pane.tick(1),"focus/session cancellation leaves a donation timer active");
    auto held=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& i){return i.control=="loadoutButtonPane";});require(held!=frontend.view().items.end(),"frontend routing button fixture");
    frontend.pointerPress(size_t(held-frontend.view().items.begin()));require(capture.owner()==UiPointerOwner::Frontend,"frontend button is not an exclusive capture owner");
    require(capture.button(size,statePixel,SDL_BUTTON_LEFT,false,false,false,emit,ignoreMap) && !capture.active() && standings.active() && frontend.screen()==Screen::Hangar && !delivered,"frontend release escapes to a console overlay");
    frontend.open(Screen::Settings);require(frontend.keyboardPopup() && frontend.pointerCaptured() && !capture.active(),"popup lifetime is confused with a held button");capture.cancel();require(frontend.keyboardPopup(),"focus cancellation dismisses an unrelated options popup");
    frontend.open(Screen::Hangar,&game.state());standings.close();
    // ButtonUIPane::SetRepeat(.1,.5): both buttons trigger on press, only
    // the left button installs a timer, and neither triggers on release.
    apply(pane.pointer(size,p,SDL_BUTTON_RIGHT,true),500);cue();
    require(pane.view(size,1).capturedButton() && !pane.tick(3),"right donation repeats or loses its face across the wallet report");
    auto release=pane.pointer(size,{-1,-1},SDL_BUTTON_RIGHT,false);require(release && release->effect==FrontEffect::None && !pane.pointerCaptured() && pane.sounds().empty(),"right donation duplicates its initial command on release");
    p=point(MenuOp::Donate,peer);apply(pane.pointer(size,p,SDL_BUTTON_LEFT,true),100);cue();
    require(!pane.tick(.49),"left Donate repeats before original half-second delay");apply(pane.tick(.02),100);cue();
    pane.hover(size,{-1,-1});require(!pane.tick(.11) && pane.sounds().empty(),"captured Donate repeats outside its actual bounds");
    p=point(MenuOp::Donate,peer);pane.hover(size,p);require(pane.sounds().empty(),"captured Donate reentry emits a hover cue");apply(pane.tick(.11),100);cue();
    release=pane.pointer(size,p,SDL_BUTTON_LEFT,false);require(release && release->effect==FrontEffect::None && !pane.pointerCaptured() && !pane.tick(1) && pane.sounds().empty(),"left Donate release repeats or leaves its timer active");
    apply(pane.pointer(size,p,SDL_BUTTON_LEFT,true),100);cue();
    require(game.donateMoney(peer,game.playerPaneData().money),"deplete real wallet during held donation");pane.sync(game,true);pane.view(size,1);
    require(!pane.tick(.6) && pane.sounds().empty(),"disabled captured Donate still repeats");pane.pointer(size,p,SDL_BUTTON_LEFT,false);

    auto firstVisible=[&]{const auto& view=pane.view(size,1);auto row=std::find_if(view.items.begin(),view.items.end(),[](const auto& i){return i.action.op==MenuOp::RosterSelect;});require(row!=view.items.end(),"captured scrollbar exposes no roster rows");return row->action.value;};
    int first=firstVisible();auto thumb=item(MenuOp::ScrollThumb);auto up=item(MenuOp::ScrollLine,0,-1),down=item(MenuOp::ScrollLine,0,1);
    require(up.bounds.w==assets.image("scrollupbmp",true)->height && down.bounds.w==assets.image("scrolldownbmp",true)->height,"F6 arrows are still quarter-sized strip faces");
    p=pixel(thumb);pane.pointer(size,p,SDL_BUTTON_LEFT,true);require(pane.view(size,1).capturedButton(),"F6 thumb loses pointer ownership");
    require(capture.button(size,p,SDL_BUTTON_RIGHT,false,false,false,emit,ignoreMap) && capture.active(),"right release retires a left-only scrollbar thumb");
    pane.hover(size,{p.x,float(size.y+50)});require(firstVisible()!=first && pane.pointerCaptured(),"captured F6 thumb does not drag outside its viewport");
    pane.hover(size,{p.x,-50});require(firstVisible()==first && pane.pointerCaptured(),"captured F6 thumb cannot return to the top");
    pane.pointer(size,{p.x,-50},SDL_BUTTON_LEFT,false);require(!pane.pointerCaptured(),"outside thumb release retains capture");
    p=point(MenuOp::ScrollLine,0,1);pane.pointer(size,p,SDL_BUTTON_LEFT,true);auto once=item(MenuOp::ScrollThumb).bounds;
    require(firstVisible()!=first,"F6 line arrow does not scroll on press");
    pane.hover(size,{-1,-1});require(!pane.tick(.51) && item(MenuOp::ScrollThumb).bounds==once,"F6 arrow keeps repeating after captured motion leaves it");
    pane.hover(size,p);pane.tick(.11);auto twice=item(MenuOp::ScrollThumb).bounds;require(twice.y>once.y,"F6 arrow does not resume its original repeat interval on reentry");
    pane.pointer(size,p,SDL_BUTTON_LEFT,false);require(item(MenuOp::ScrollThumb).bounds==twice && !pane.pointerCaptured() && pane.sounds().empty(),"arrow release scrolls twice or emits a Trek sound");
    p=point(MenuOp::ScrollThumb);pane.pointer(size,p,SDL_BUTTON_LEFT,true);pane.hover(size,{p.x,-50});pane.pointer(size,{p.x,-50},SDL_BUTTON_LEFT,false);
    auto page=item(MenuOp::ScrollPage,0,1);auto layout=originalMenuLayout(size,1,pane.view(size,1).canvas);p=layout.pixel({page.bounds.x+3,page.bounds.y+page.bounds.w-3});
    pane.pointer(size,p,SDL_BUTTON_LEFT,true);once=item(MenuOp::ScrollThumb).bounds;require(!pane.tick(.24) && item(MenuOp::ScrollThumb).bounds==once,"F6 page repeat fires early");
    pane.tick(.02);require(item(MenuOp::ScrollThumb).bounds.y>once.y,"F6 page repeat uses line delay instead of original quarter second");pane.pointer(size,p,SDL_BUTTON_LEFT,false);
    puts("F6 captured controls: cross-panel/modal routing, secondary edges, cancellation, real immediate/left-repeat/right-single donations, original sounds, outside/reentry/disabled timer gates, full arrow images, thumb drags beyond viewport and line/page timing passed");
}
void ordering(const std::filesystem::path& art) {
    // Exercise every TeamPane comparator against the original InsertSorted
    // semantics, including ties, humans/drones and its default LeaderCompare.
    for(unsigned trial=0;trial<64;++trial) {
        std::vector<PlayerPaneRow> rows(32);unsigned random=trial+1;
        auto next=[&]{random=random*1664525u+1013904223u;return random;};
        for(auto& row:rows) {
            row.human=next()&4;row.commander=next()&4;row.kills=int(next()%7);row.money=int(next()%5)-1;row.wing=int(next()%11)-1;
            row.name=std::string(next()&4?"Pilot ":"pilot ")+std::to_string(next()%9);
            row.ship=std::to_string(next()%4);row.sectorName=std::to_string(next()%4);
            row.location="aboard "+std::to_string(next()%11);row.command=int(next()%10);row.objective=std::to_string(next()%7);
        }
        auto nameAfter=[](std::string_view a,std::string_view b){return std::lexicographical_compare(b.begin(),b.end(),a.begin(),a.end(),[](unsigned char x,unsigned char y){return std::tolower(x)<std::tolower(y);});};
        for(int column=-1;column<8;++column) {
            auto after=[&](size_t x,size_t y){const auto& a=rows[x];const auto& b=rows[y];
                switch(column) {
                case 0:return a.human && b.human?nameAfter(a.name,b.name):!a.human && (b.human || nameAfter(a.name,b.name));
                case 1:return false;
                case 2:return a.kills>b.kills;
                case 3:return nameAfter(a.sectorName,b.sectorName);
                case 4:return nameAfter(a.ship,b.ship);
                case 5:return a.wing>b.wing;
                case 6:return a.money>b.money;
                case 7:return nameAfter(a.objective,b.objective);
                default:return b.human && !a.human;
                }
            };
            TList<size_t,DefaultEquals,decltype(after)> reference(DefaultEquals{},after);
            std::vector<size_t> actual;for(size_t i=0;i<rows.size();++i){reference.InsertSorted(i);actual.push_back(i);}
            std::stable_sort(actual.begin(),actual.end(),[&](size_t x,size_t y){return Allegiance::PlayerPaneBefore(rows[x],rows[y],column);});
            TList<size_t,DefaultEquals,decltype(after)>::Iterator expected(reference);
            for(auto i:actual){require(!expected.End() && expected.Value()==i,"F6 header order differs from original stable InsertSorted");expected.Next();}
        }
    }
    Assets assets(art);SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.enemies=0;options.rules.miners=0;
    Game game(art,[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
    int peer=game.addLocalPilot("Roster peer",0);auto* ship=game.core()->GetShip(peer);auto* station=game.playerShip()->GetStation();
    ship->SetCommand(c_cmdAccepted,station,c_cidGoto);require(ship->LegalCommand(c_cidGoto,station),"F6 objective projection fixture is legal");
    auto data=game.playerPaneData();auto row=std::find_if(data.rows.begin(),data.rows.end(),[&](const auto& r){return r.id==peer;});
    require(row!=data.rows.end() && row->objective==std::string(c_cdAllCommands[c_cidGoto].szVerb)+" "+station->GetName() && row->sectorName==station->GetCluster()->GetName(),"F6 objective/sector sort does not project actual IGC values");
    ship->SetCommand(c_cmdAccepted,nullptr,c_cidMine);data=game.playerPaneData();row=std::find_if(data.rows.begin(),data.rows.end(),[&](const auto& r){return r.id==peer;});
    require(row!=data.rows.end() && row->objective.empty(),"F6 objective sort exposes an illegal command");
    ship->SetParentShip(game.playerShip());ship->SetTurretID(NA);ship->SetStation(station);
    data=game.playerPaneData();row=std::find_if(data.rows.begin(),data.rows.end(),[&](const auto& r){return r.id==peer;});
    require(row!=data.rows.end() && row->location==std::string("aboard ")+game.playerShip()->GetName() && row->sectorName==station->GetCluster()->GetName(),"F6 passenger label replaces the physical sector sort key");
    PlayerPane pane(assets);pane.toggle(game);const glm::ivec2 size{2560,1440};pane.view(size,1);std::this_thread::sleep_for(std::chrono::milliseconds(550));
    auto point=[&](MenuOp op,int value){const auto& v=pane.view(size,1);auto i=std::find_if(v.items.begin(),v.items.end(),[&](const auto& c){return c.action.op==op && c.action.value==value && (op!=MenuOp::SelectTeam || c.action.delta==1);});if(i==v.items.end())throw std::runtime_error("F6 interaction fixture control "+std::to_string(int(op))+"/"+std::to_string(value));return originalMenuLayout(size,1,v.canvas).pixel(glm::vec2(i->bounds)+glm::vec2(3));};
    auto header=point(MenuOp::Order,2);pane.pointer(size,header,SDL_BUTTON_LEFT,true);require(pane.pointerCaptured(),"ordinary F6 header does not retain its original press capture");
    pane.sync(game,true);require(pane.pointerCaptured(),"unchanged forced roster refresh cancels an unselected header press");
    for(int i=0;i<11;++i)game.step({},.1);pane.sync(game,true);require(pane.pointerCaptured(),"changed dock-duration report cancels an unselected header press");
    auto headerResult=pane.pointer(size,header,SDL_BUTTON_LEFT,false);const auto& sorted=pane.view(size,1);
    require(headerResult && std::any_of(sorted.items.begin(),sorted.items.end(),[](const auto& i){return i.action.op==MenuOp::Order && i.action.value==2 && i.selected;}),"header release after roster refresh loses its original sort action");
    header=point(MenuOp::Order,7);pane.pointer(size,header,SDL_BUTTON_LEFT,true,true);require(!pane.pointerCaptured(),"unsubscribed double-header event captures the normal release action");pane.pointer(size,header,SDL_BUTTON_LEFT,false,true);
    require(std::none_of(pane.view(size,1).items.begin(),pane.view(size,1).items.end(),[](const auto& i){return i.action.op==MenuOp::Order && i.action.value==7 && i.selected;}),"double header click invents a second normal sort action");
    auto p=point(MenuOp::RosterSelect,peer);auto result=pane.pointer(size,p,SDL_BUTTON_LEFT,true,true);
    require(result && result->effect==FrontEffect::None && pane.active() && pane.selected()==peer && !game.state().commandView,"F6 double click invents a Take Me To action");pane.pointer(size,p,SDL_BUTTON_LEFT,false,true);
    result=pane.pointer(size,p,SDL_BUTTON_RIGHT,true,true);require(result && result->effect==FrontEffect::None && !pane.capturing() && pane.active(),"unsubscribed F6 double-right event opens a context menu");pane.pointer(size,p,SDL_BUTTON_RIGHT,false,true);
    require(!pane.key(SDLK_HOME) && !pane.key(SDLK_END) && pane.selected()==peer,"F6 captures Home/End absent from original ListPane");
    const auto& view=pane.view(size,1);float bottom=0;glm::vec2 blank{};for(const auto& i:view.items)if(i.action.op==MenuOp::RosterSelect){bottom=std::max(bottom,i.bounds.y+i.bounds.w);blank.x=i.bounds.x+3;}blank.y=bottom+4;
    auto blankPixel=originalMenuLayout(size,1,view.canvas).pixel(blank);
    for(int button:{SDL_BUTTON_LEFT,SDL_BUTTON_RIGHT}) {
        pane.pointer(size,p,SDL_BUTTON_LEFT,true);pane.pointer(size,p,SDL_BUTTON_LEFT,false);require(pane.selected()==peer,"F6 single down selection fixture");
        result=pane.pointer(size,blankPixel,button,true);require(result && result->effect==FrontEffect::None && pane.selected()<0 && !pane.capturing(),"F6 blank click retains pilot action selection");pane.pointer(size,blankPixel,button,false);
        const auto& cleared=pane.view(size,1);require(std::none_of(cleared.items.begin(),cleared.items.end(),[](const auto& i){return i.enabled && (i.action.op==MenuOp::Donate || i.action.op==MenuOp::TakeMeTo || i.action.op==MenuOp::ConfirmBoot);}),"blank F6 selection leaves actions enabled for the former pilot");
        pane.pointer(size,p,SDL_BUTTON_MIDDLE,true);require(pane.selected()<0,"middle click selects an F6 pilot absent from original ListPane");pane.pointer(size,p,SDL_BUTTON_MIDDLE,false);
        require(pane.key(SDLK_UP) && pane.selected()<0,"Up from an empty F6 selection invents a recipient");
    }
    pane.key(SDLK_DOWN);int first=pane.selected();require(first>=0,"Down from blank F6 does not select first pilot");pane.key(SDLK_UP);pane.key(SDLK_PAGEUP);require(pane.selected()==first,"F6 navigation moves before the first pilot");
    for(int button:{SDL_BUTTON_LEFT,SDL_BUTTON_RIGHT}) {
        auto teamPoint=point(MenuOp::SelectTeam,1);result=pane.pointer(size,teamPoint,button,true);
        require(result && pane.team()==1 && pane.selected()<0,"F6 team ListPane waits for release or ignores right down");result=pane.pointer(size,teamPoint,button,false);require(result && result->effect==FrontEffect::None,"F6 team release dispatches a second action");
        auto own=point(MenuOp::SelectTeam,0);pane.pointer(size,own,button,true);pane.pointer(size,own,button,false);require(pane.team()==0,"F6 original team selection cannot return to own side");
    }
    {
        const auto& v=pane.view(size,1);glm::vec2 emptyTeam{};
        for(const auto& i:v.items)if(i.action.op==MenuOp::SelectTeam && i.action.delta==1){emptyTeam.x=i.bounds.x+3;emptyTeam.y=std::max(emptyTeam.y,i.bounds.y+i.bounds.w+4);}
        auto pixel=originalMenuLayout(size,1,v.canvas).pixel(emptyTeam);
        result=pane.pointer(size,pixel,SDL_BUTTON_RIGHT,true);require(result && pane.team()==SIDE_TEAMLOBBY && pane.selected()<0,"F6 blank team row does not select original No Team fallback");pane.pointer(size,pixel,SDL_BUTTON_RIGHT,false);
        auto own=point(MenuOp::SelectTeam,0);pane.pointer(size,own,SDL_BUTTON_LEFT,true);pane.pointer(size,own,SDL_BUTTON_LEFT,false);
    }
    p=point(MenuOp::RosterSelect,peer);pane.pointer(size,p,SDL_BUTTON_LEFT,true);pane.pointer(size,p,SDL_BUTTON_LEFT,false);require(pane.selected()==peer,"F6 original Take Me To recipient selection");auto focus=point(MenuOp::TakeMeTo,peer);
    pane.pointer(size,focus,SDL_BUTTON_LEFT,true);result=pane.pointer(size,focus,SDL_BUTTON_LEFT,false);
    require(result && result->effect==FrontEffect::TakeMeTo && result->value==peer && !pane.active(),"removing invented double click disables the original Take Me To button");
    puts("F6 parity: 576 original sort comparisons, legal objective/passenger sector projection, refresh-safe header capture, blank/middle click policy, unsubscribed double clicks, original list keys/team-down selection and Take Me To button passed");
}
void check(const std::filesystem::path& art) {
    using Allegiance::PlayerPaneMode;
    require(Allegiance::TogglePlayerPane(PlayerPaneMode::Compact,false)==PlayerPaneMode::Expanded &&
        Allegiance::TogglePlayerPane(PlayerPaneMode::Expanded,false)==PlayerPaneMode::Hidden &&
        Allegiance::TogglePlayerPane(PlayerPaneMode::Expanded,true)==PlayerPaneMode::Compact,
        "original F6 and mini-team shortcuts select their own size and then hide");
    require(Allegiance::PlayerPaneDonation(73,false)==73 && Allegiance::PlayerPaneDonation(1200,false)==100 && Allegiance::PlayerPaneDonation(1200,true)==500,"original left/right donation amounts");
    require(Allegiance::PlayerPaneCompactAccount(true,false,true,73,500)=="Docked for 1min 13sec" &&
        Allegiance::PlayerPaneCompactAccount(true,false,true,9,500)=="Docked for 9sec" &&
        Allegiance::PlayerPaneCompactAccount(true,true,true,9,500)=="$: 500" &&
        Allegiance::PlayerPaneCompactAccount(false,false,true,9,500)=="$: 500","original commander compact pane shows dock duration for other pilots");
    Assets assets(art);QuickChatCatalog quick(assets);SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.rules.miners=0;
    Game game(art,[](auto&,auto&){return ModelHandle{1,1};},0,0,options);game.quickChats(quick);
    int peer=game.addLocalPilot("Duplicate pilot",0),other=game.addLocalPilot("Duplicate pilot",0),enemy=game.addLocalPilot("Unknown opponent",1);
    require(peer>=0 && other>=0 && enemy>=0,"player-pane real IGC fixtures");
    for(int i=0;i<13;++i)game.step({},.1);auto docked=game.playerPaneData();auto dockedPeer=std::find_if(docked.rows.begin(),docked.rows.end(),[&](auto& r){return r.id==peer;});
    require(dockedPeer!=docked.rows.end() && dockedPeer->dockedSeconds==1,"dock duration starts at the actual station transition, before the pane opens");
    auto* refitted=game.core()->GetShip(peer);auto* oldHull=refitted->GetBaseHullType();auto oldIdentity=dockedPeer->identity;
    for(auto* h=game.core()->GetHullTypes()->first();h;h=h->next())if(h->data()!=oldHull){refitted->SetBaseHullType(h->data());refitted->SetBaseHullType(oldHull);break;}
    docked=game.playerPaneData();dockedPeer=std::find_if(docked.rows.begin(),docked.rows.end(),[&](auto& r){return r.id==peer;});
    require(dockedPeer!=docked.rows.end() && dockedPeer->identity==oldIdentity && dockedPeer->dockedSeconds==1,"hull reload preserves ship identity and the actual dock transition");
    auto data=game.playerPaneData();auto unknown=std::find_if(data.rows.begin(),data.rows.end(),[&](auto& r){return r.id==enemy;});
    require(unknown!=data.rows.end() && !unknown->current && unknown->ship=="<none>" && unknown->location=="unknown" && unknown->money<0,"public human roster never reads hidden enemy hangar/hull/accounts");
    int originalTarget=game.state().targetId;
    require(game.takeMeTo(enemy) && !game.state().commandView && game.state().targetId==originalTarget,"unknown opponent is a console recipient, never a camera or weapon target");
    CameraInput camera;game.camera(camera);require(game.takeMeTo(other) && game.state().commandView && game.state().targetId==originalTarget,"Take Me To opens command view without changing weapon target");
    require(game.camera(camera).animating,"Take Me To retains the original command-camera interpolation");
    game.commandView(false);
    PlayerPane pane(assets);pane.toggle(game);require(pane.active() && pane.mode()==PlayerPaneMode::Expanded,"F6 opens original expanded overlay");
    const glm::ivec2 size{2560,1440};auto& first=pane.view(size,1);require(first.original && first.overlay && first.slide && first.slide->off.x==760,"player pane is independent, native-sized and slides from the right");
    require(std::any_of(first.artwork.begin(),first.artwork.end(),[](auto& c){return c.image && c.name.ends_with("expandedTeamPane");}),"expanded pane uses its authored background");
    std::this_thread::sleep_for(std::chrono::milliseconds(550));
    auto click=[&](MenuOp op,int value,int button=SDL_BUTTON_LEFT){
        const auto& view=pane.view(size,1);auto row=std::find_if(view.items.begin(),view.items.end(),[&](auto& item){return item.enabled && item.action.op==op && item.action.value==value;});require(row!=view.items.end(),"authored player control exists and is enabled");
        auto p=originalMenuLayout(size,view.classicScale,view.canvas).pixel(glm::vec2(row->bounds)+glm::vec2(row->bounds.z,row->bounds.w)*.5f);
        auto down=pane.pointer(size,p,button,true);auto up=pane.pointer(size,p,button,false);require(down && up,"player pane owns both pointer edges");return up->effect!=FrontEffect::None?*up:*down;
    };
    click(MenuOp::RosterSelect,peer);require(pane.selected()==peer,"selection stores a stable pilot ID");
    auto donation=click(MenuOp::Donate,peer);require(donation.effect==FrontEffect::Donate && donation.value==peer && donation.command==100,"original Donate dispatches actual selected pilot and 100 credits");
    donation=click(MenuOp::Donate,peer,SDL_BUTTON_RIGHT);require(donation.command==500,"right donation dispatches 500 credits");
    click(MenuOp::RosterSelect,other,SDL_BUTTON_RIGHT);require(pane.capturing() && pane.selected()==other,"right-click selects duplicate-name pilot before opening context menu");
    pane.view(size,1);auto chat=pane.key(SDLK_C);require(chat && chat->effect==FrontEffect::SelectUnit && chat->value==other && !pane.capturing(),"original flight Chat action picks a stable recipient, without a lobby-role menu");
    ChatConsole console(assets,quick);require(console.selectPeer(game,chat->value),"player-pane Chat chooses recipient");console.startChat(game);
    require(console.audience().channel==ChatChannel::Target && console.audience().recipient==other,"Enter composition preserves picked player recipient");
    console.cancel(game);require(console.selectPeer(game,enemy),"invisible public human can be selected for chat");console.startQuick(game);require(console.audience().recipient==enemy,"quick chat retains explicit enemy human recipient without ship visibility");
    pane.toggle(game,true);require(pane.mode()==PlayerPaneMode::Compact && pane.selected()==other,"mini-team switches size without losing selection");
    pane.view(size,2);require(pane.view(size,2).classicScale==2 && pane.view(size,2).canvas==glm::ivec2(1280,720),"integer scaling also transforms the player pane's pointer canvas");
    {
        const auto& compact=pane.view(size,1);auto label=std::find_if(compact.artwork.begin(),compact.artwork.end(),[](const auto& c){return c.name=="playerTeam";});
        auto combo=std::find_if(compact.items.begin(),compact.items.end(),[&](const auto& c){return label!=compact.artwork.end() && c.action.op==MenuOp::SelectTeam && c.bounds.x==label->bounds.x && c.bounds.y==label->bounds.y;});
        require(label!=compact.artwork.end() && combo!=compact.items.end() && label->bounds.z==109 && combo->bounds.z==125 && label->textMode==OriginalControl::TextMode::Pane,"compact combo retains separate authored face/hit dimensions and clipping");
        auto color=Allegiance::PlayerSideColor(pane.team());require(std::any_of(compact.artwork.begin(),compact.artwork.end(),[&](const auto& c){return c.kind==OriginalControl::Kind::Rectangle && c.bounds==label->bounds && c.color==glm::vec4(color[0],color[1],color[2],1);}),"compact team combo omits original faction-colored face");
        int selectedTeam=pane.team();auto point=originalMenuLayout(size,1,compact.canvas).pixel(glm::vec2(combo->bounds)+glm::vec2(3));
        require(pane.pointer(size,point,SDL_BUTTON_LEFT,true).has_value() && pane.capturing(),"compact team combo opens on mouse down");pane.pointer(size,point,SDL_BUTTON_LEFT,false);
        auto teams=pane.view(size,1);require(pane.capturing(),"compact team combo release does not select or dismiss its menu");
        auto selected=std::find_if(teams.artwork.begin(),teams.artwork.end(),[](const auto& c){return c.name=="playerPopup/0";});require(selected!=teams.artwork.end() && selected->font=="smallFont","team combo menu loses its authored font");
        auto popupSelected=[&](int row){const auto& v=pane.view(size,1);return std::any_of(v.items.begin(),v.items.end(),[&](const auto& i){return i.action.op==MenuOp::PopupItem && i.action.value==row && i.selected;});};
        auto popupPoint=[&](int row){const auto& v=pane.view(size,1);auto i=std::find_if(v.items.begin(),v.items.end(),[&](const auto& i){return i.action.op==MenuOp::PopupItem && i.action.value==row;});require(i!=v.items.end(),"combo remembered-row fixture missing");return originalMenuLayout(size,1,v.canvas).pixel(glm::vec2(i->bounds)+glm::vec2(3));};
        pane.hover(size,popupPoint(1));require(popupSelected(1),"team combo hover fails to select the second row");pane.key(SDLK_ESCAPE);
        pane.view(size,1);pane.pointer(size,point,SDL_BUTTON_LEFT,true);pane.view(size,1);require(popupSelected(1),"team combo cancellation loses its hovered highlight");require(pane.team()==selectedTeam,"team combo cancellation changes the selected team");
        auto ignoredWheel=pane.wheel(-1);require(ignoredWheel && popupSelected(1),"F6 combo acquires unauthored wheel navigation");
        pane.pointer(size,{0,0},SDL_BUTTON_RIGHT,true);require(!pane.capturing(),"outside right press fails to dismiss team combo");
        pane.toggle(game);require(pane.mode()==PlayerPaneMode::Expanded,"wing combo is authored in the expanded roster");
        require(game.setWing(game.state().shipId,5),"nondefault wing fixture");pane.sync(game,true);
        click(MenuOp::Wing,0);auto wings=pane.view(size,1);auto first=std::find_if(wings.artwork.begin(),wings.artwork.end(),[](const auto& c){return c.name=="playerPopup/0";});auto second=std::find_if(wings.artwork.begin(),wings.artwork.end(),[](const auto& c){return c.name=="playerPopup/1";});
        require(first!=wings.artwork.end() && second!=wings.artwork.end() && first->font=="largeFont" && second->bounds.y-first->bounds.y==assets.font("largeFont")->height+2,"F6 wing combo menu ignores its authored font or row height");
        require(popupSelected(0),"ComboPane SetSelection incorrectly changes IMenu's first highlight to the current wing");
        auto row=std::find_if(wings.items.begin(),wings.items.end(),[](const auto& item){return item.action.op==MenuOp::PopupItem && item.action.value==1;});require(row!=wings.items.end(),"F6 wing menu has no second row");
        point=originalMenuLayout(size,1,wings.canvas).pixel(glm::vec2(row->bounds)+glm::vec2(3));pane.hover(size,point);pane.view(size,1);
        auto ignored=pane.pointer(size,point,SDL_BUTTON_RIGHT,false);require(ignored && ignored->effect==FrontEffect::None && pane.capturing(),"right release executes a left-button menu command");auto wing=pane.pointer(size,point,SDL_BUTTON_LEFT,false);
        require(wing && wing->effect==FrontEffect::Wing && wing->command==1 && !pane.capturing(),"F6 wing menu cannot select by dragging from its face and releasing on a row");
        click(MenuOp::Wing,0);require(popupSelected(1),"accepted wing menu loses its independent remembered highlight");
        pane.hover(size,popupPoint(4));pane.key(SDLK_ESCAPE);click(MenuOp::Wing,0);require(popupSelected(4),"canceled wing menu loses its hovered highlight");
        const auto& reopened=pane.view(size,1);auto top=std::find_if(reopened.items.begin(),reopened.items.end(),[](const auto& i){return i.action.op==MenuOp::PopupItem && i.action.value==0;});
        require(top!=reopened.items.end(),"wing border fixture missing");auto border=originalMenuLayout(size,1,reopened.canvas).pixel(glm::vec2(top->bounds)-glm::vec2(1));
        for(int button:{SDL_BUTTON_LEFT,SDL_BUTTON_RIGHT,SDL_BUTTON_MIDDLE}){auto result=pane.pointer(size,border,button,true);require(result && result->effect==FrontEffect::None && pane.capturing(),"inside F6 popup border dismisses or executes the menu");pane.pointer(size,border,button,false);}
        for(int button:{SDL_BUTTON_LEFT,SDL_BUTTON_RIGHT,SDL_BUTTON_MIDDLE}){pane.pointer(size,{0,0},button,true);require(!pane.capturing() && pane.active(),"outside F6 popup press fails to dismiss only its menu");click(MenuOp::Wing,0);require(popupSelected(4),"outside dismissal resets remembered wing highlight");}
        pane.key(SDLK_ESCAPE);require(game.setWing(game.state().shipId,0),"restore wing fixture");pane.sync(game,true);
        pane.toggle(game,true);
    }
    pane.toggle(game,true);require(!pane.active() && pane.view(size,2).items.empty(),"closing pane immediately drops all input while artwork slides out");
    pane.toggle(game);pane.view(size,1);std::this_thread::sleep_for(std::chrono::milliseconds(550));click(MenuOp::RosterSelect,peer);
    require(game.removeLocalPilot(peer),"retire selected pilot fixture");pane.sync(game,true);require(pane.selected()<0 && !pane.capturing(),"departed recipient clears stable selection and context capture");
    for(int i=0;i<20;++i)require(game.addLocalPilot("Scrollable pilot "+std::to_string(i),0)>=0,"large roster fixture");
    pane.sync(game,true);auto last=pane.view(size,1);auto selection=std::find_if(last.items.begin(),last.items.end(),[](auto& item){return item.action.op==MenuOp::RosterSelect;});require(selection!=last.items.end(),"large roster rows are reachable");
    click(MenuOp::RosterSelect,selection->action.value);for(int i=0;i<4;++i)pane.key(SDLK_PAGEDOWN);auto& end=pane.view(size,1);
    require(std::any_of(end.items.begin(),end.items.end(),[&](auto& item){return item.action.op==MenuOp::RosterSelect && item.action.value==pane.selected();}),"Page Down reveals final row with original clipped viewport and scrollbar");
    auto originalPointer=end.artwork.data();pane.sync(game);require(pane.view(size,1).artwork.data()==originalPointer,"unchanged frames retain compiled layout and resident artwork");
    require(game.state().commander && game.setAutoDonate(other,-1),"commander can toggle another pilot's donation");
    auto* target=game.core()->GetShip(ShipID(other));require(!target->GetAutoDonate(),"Stop Donate turns an existing route off");
    require(game.setAutoDonate(other,-1) && target->GetAutoDonate()==game.playerShip(),"Stop Donate also restores donation to the commander");
    require(game.controlLocalPilot(other) && !game.setAutoDonate(game.playerPaneData().rows.front().id,-1),"ordinary pilots cannot toggle others' donation routes");
    puts("Original F6/mini-team panes, stable identity, original controls, donations, context chat, clipping, scrolling, integer scales and private visibility passed");
}
void boot(const std::filesystem::path& art) {
    Assets assets(art);SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.enemies=0;options.rules.miners=0;
    options.rules.allowDefections=false;
    Game game(art,[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
    int target=game.addLocalPilot("Duplicate pilot",0),other=game.addLocalPilot("Duplicate pilot",0);
    require(target>=0 && other>=0 && game.state().commander,"Boot fixtures have a real commander and teammates");
    PlayerPane pane(assets);pane.toggle(game);const glm::ivec2 size{1280,720};pane.view(size,1);
    std::this_thread::sleep_for(std::chrono::milliseconds(550));
    auto click=[&](MenuOp op,int value){
        auto& view=pane.view(size,1);auto item=std::find_if(view.items.begin(),view.items.end(),[&](auto& i){return i.enabled && i.action.op==op && i.action.value==value;});require(item!=view.items.end(),"original Boot control exists");
        auto pixel=originalMenuLayout(size,view.classicScale,view.canvas).pixel(glm::vec2(item->bounds)+glm::vec2(item->bounds.z,item->bounds.w)*.5f);
        pane.pointer(size,pixel,SDL_BUTTON_LEFT,true);auto result=pane.pointer(size,pixel,SDL_BUTTON_LEFT,false);require(bool(result),"Boot button owns pointer release");return *result;
    };
    click(MenuOp::RosterSelect,target);auto result=click(MenuOp::ConfirmBoot,target);
    require(result.effect==FrontEffect::ConfirmBoot && result.value==target && result.command==0 && game.core()->GetShip(target),"F6 asks for confirmation before sending or changing membership");
    Settings settings;Frontend frontend(settings);frontend.originalInterface(assets);frontend.setPlaying(true);frontend.open(Screen::Hangar,&game.state());
    auto ask=[&](bool resume=false){frontend.confirm("Boot Duplicate pilot?",{FrontEffect::BootPilot,result.value,result.command},resume);};
    ask();auto& dialog=frontend.messageView();auto background=assets.image("dialogbkgndbmp",true);auto origin=glm::floor((glm::vec2(800,600)-glm::vec2(background->width,background->height))*.5f);
    require(dialog.items.size()==2 && !dialog.slide && dialog.original && dialog.overlay,"original confirmation is a separate ordered modal with two buttons");
    require(glm::vec2(dialog.items[0].bounds)==origin+glm::vec2(110,170) && glm::vec2(dialog.items[1].bounds)==origin+glm::vec2(290,170),"confirmation OK/Abort use the original MessageBoxImpl offsets");
    require(std::none_of(frontend.sceneView().artwork.begin(),frontend.sceneView().artwork.end(),[](auto& c){return c.name=="messageBackground";}),"modal backdrop does not draw the dialog twice");
    require(std::all_of(frontend.sceneView().items.begin(),frontend.sceneView().items.end(),[](auto& i){return i.blocked;}),"confirmation blocks all parent controls");
    require(frontend.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE).effect==FrontEffect::None && !frontend.hasMessageBox() && game.core()->GetShip(target),"Escape cancels the confirmation without booting or leaving hangar");
    ask();auto cancel=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](auto& i){return i.control=="messageCancel";});
    auto cancelIndex=size_t(cancel-frontend.view().items.begin());frontend.pointerPress(cancelIndex);
    require(frontend.messageView().pressed==1,"cached dialog maps the captured Cancel button to its original pressed face");
    frontend.pointerRelease({});require(frontend.messageView().pressed==-1 && frontend.hasMessageBox(),"release outside clears the pressed face without executing Cancel");
    frontend.activate(cancelIndex);require(!frontend.hasMessageBox() && game.core()->GetShip(target),"authored Abort button cancels without mutation");
    ask();click(MenuOp::RosterSelect,other);auto accepted=frontend.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
    require(accepted.effect==FrontEffect::BootPilot && accepted.value==target && accepted.command==0 && !accepted.resumeAfter,"confirmation retains the original ID despite later row selection and duplicate names");
    require(game.bootPlayer(accepted.value,accepted.command) && !game.core()->GetShip(target) && game.core()->GetShip(other),"no-defections F6 removes only the confirmed pilot from the local match");
    pane.sync(game,true);require(!game.bootPlayer(target,0) && !game.bootPlayer(other,1),"departed pilot and changed confirmation side cannot redirect Boot");
    result.value=other;ask(true);require(game.makeTeamLeader(other),"leadership-change fixture");accepted=frontend.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
    require(accepted.resumeAfter && !game.bootPlayer(accepted.value,accepted.command) && game.core()->GetShip(other),"loss of leadership while confirming cannot boot a pilot");
    options.rules.allowDefections=true;Game defecting(art,[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
    int moved=defecting.addLocalPilot("Boot to lobby",0);require(defecting.bootPlayer(moved,0) && defecting.core()->GetShip(moved)->GetSide()->GetObjectID()==SIDE_TEAMLOBBY,"defections-enabled F6 retains the pilot on the original No Team side");
    options.rules.lockTeamSettings=true;Game locked(art,[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
    int protectedPilot=locked.addLocalPilot("Locked teammate",0);require(!locked.bootPlayer(protectedPilot,0) && locked.core()->GetShip(protectedPilot)->GetSide()->GetObjectID()==0,"confirmation cannot bypass a team-settings lock");
    puts("Original F6 Boot confirmation: OK/Abort/Escape, bitmap geometry, parent capture, stable target, late leadership/membership/lock validation and both defection policies passed");
}
void popupRefresh(const std::filesystem::path& art){
    Assets assets(art);SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.rules.miners=0;options.rules.maxImbalance=32767;
    Game authority(art,[](auto&,auto&){return ModelHandle{1,1};},0,0,options,{},GameRole::Authority);
    Game replica(art,[](auto&,auto&){return ModelHandle{1,1};},0,0,options,{},GameRole::Replica);
    int self=authority.joinNetworkPilot("Commander",0,true);require(self>=0 && authority.teamLeader(0)==self,"applicant recipient must be the actual commander");
    int peer=authority.addLocalPilot("Requesting pilot",0);require(peer>=0 && authority.moveNetworkPilotToTeam(peer,noTeam),"pending applicant fixture");
    auto frame=authority.networkFrame(self,1,0);
    // Server appends public membership/admission after the physical snapshot.
    Net::Member member;member.id=peer;member.side=noTeam;member.requested=0;Net::text(member.name,"Requesting pilot");frame.members.push_back(member);auto* applicant=&frame.members.back();
    replica.applyNetworkFrame(frame);auto data=replica.playerPaneData();require(data.commander && !data.closed && data.acceptJoiners,"replica applicant commander/admission fixture is not active");
    auto publicRow=std::find_if(data.rows.begin(),data.rows.end(),[&](const auto& p){return p.id==peer;});require(publicRow!=data.rows.end() && publicRow->human && publicRow->requesting && !publicRow->controlled,"public applicant fixture lost identity or request");
    PlayerPane pane(assets);pane.toggle(replica);const glm::ivec2 size{2560,1440};pane.view(size,1);std::this_thread::sleep_for(std::chrono::milliseconds(550));
    const auto& view=pane.view(size,1);auto row=std::find_if(view.items.begin(),view.items.end(),[&](const auto& i){return i.action.op==MenuOp::RosterSelect && i.action.value==peer;});require(row!=view.items.end(),"F6 pending applicant row missing");
    auto at=originalMenuLayout(size,1,view.canvas).pixel(glm::vec2(row->bounds)+glm::vec2(3));pane.pointer(size,at,SDL_BUTTON_RIGHT,true);require(pane.selected()==peer && pane.capturing(),"actual applicant right click fails to open its context");pane.view(size,1);pane.key(SDLK_DOWN);
    auto selected=[&]{const auto& v=pane.view(size,1);auto i=std::find_if(v.items.begin(),v.items.end(),[](const auto& i){return i.action.op==MenuOp::PopupItem && i.selected;});require(i!=v.items.end(),"F6 context selected command missing");auto text=std::find_if(v.artwork.begin(),v.artwork.end(),[&](const auto& c){return c.name=="playerPopup/"+std::to_string(i->action.value);});require(text!=v.artwork.end(),"F6 context selected text missing");return text->text;};
    auto command=selected();if(!command.starts_with("Accept"))throw std::runtime_error("F6 applicant Down selects "+command+" instead of Accept");
    ++frame.header.tick;++applicant->kills;replica.applyNetworkFrame(frame);pane.sync(replica,true);
    require(pane.capturing() && selected().starts_with("Accept"),"ordinary report resets the original context highlight");
    applicant->requested=noTeamRequest;++frame.header.tick;replica.applyNetworkFrame(frame);pane.sync(replica,true);
    require(!pane.capturing() && pane.selected()<0,"retired application leaves a stale context command");
    puts("F6 menus: replica applicant context retains its selected command across report refresh and retires with its application");
}
void history(const std::filesystem::path& art) {
    SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.rules.miners=0;
    Game authority(art,[](auto&,auto&){return ModelHandle{1,1};},0,0,options,{},GameRole::Authority);
    Game replica(art,[](auto&,auto&){return ModelHandle{1,1};},0,0,options,{},GameRole::Replica);
    int enemy=authority.addLocalPilot("Remembered opponent",1),self=authority.state().shipId;
    auto* ship=authority.core()->GetShip(enemy);auto* sector=authority.playerShip()->GetStation()->GetCluster();
    ship->SetStation(nullptr);ship->SetCluster(sector);ship->SetPosition(authority.playerShip()->GetStation()->GetPosition()+Vector(500,0,0));ship->SetVelocity(Vector::GetZero());
    DataShipIGC droneData{};droneData.shipID=authority.core()->GenerateNewShipID();droneData.hullID=ship->GetBaseHullType()->GetObjectID();droneData.sideID=1;droneData.pilotType=c_ptWingman;droneData.baseObjectID=NA;droneData.partsOffset=sizeof(droneData);strcpy(droneData.name,"Remembered drone");
    auto* created=static_cast<IshipIGC*>(authority.core()->CreateObject(authority.core()->GetLastUpdate(),OT_ship,&droneData,sizeof(droneData)));require(created,"observed drone fixture");TRef<IshipIGC> drone=created;created->Release();
    drone->SetCluster(sector);drone->SetPosition(ship->GetPosition()+Vector(200,0,0));drone->SetVelocity(Vector::GetZero());
    authority.authorityStep({},.05);authority.beginNetworkPublication();auto first=authority.networkFrame(self,3,0);
    Net::Member member;member.id=enemy;member.side=1;Net::text(member.name,ship->GetName());first.members.push_back(member);
    require(std::any_of(first.entities.begin(),first.entities.end(),[&](auto& e){return e.identity.type==OT_ship && e.identity.id==enemy && (e.flags&Net::Eye);}),"enemy enters real scanner sight before the pane opens");
    replica.applyNetworkFrame(first);
    auto hidden=first;hidden.header.tick+=60;hidden.entities.clear();hidden.payload.clear();
    for(auto& intel:hidden.shipIntel)if(intel.id==enemy || intel.id==droneData.shipID)Allegiance::HidePlayerShip(intel.status);
    size_t offset=0;for(const auto& e:first.entities){if(e.identity.type!=OT_ship || (e.identity.id!=enemy && e.identity.id!=droneData.shipID)){hidden.entities.push_back(e);hidden.payload.insert(hidden.payload.end(),first.payload.begin()+offset,first.payload.begin()+offset+e.payloadBytes);}offset+=e.payloadBytes;}
    std::erase_if(hidden.parts,[&](auto& p){return p.ship.id==enemy || p.ship.id==droneData.shipID;});replica.applyNetworkFrame(hidden);
    auto data=replica.playerPaneData();auto remembered=std::find_if(data.rows.begin(),data.rows.end(),[&](auto& row){return row.id==enemy;});
    require(!replica.core()->GetShip(enemy) && remembered!=data.rows.end() && !remembered->current && remembered->sector==sector->GetObjectID() && remembered->ship!="<none>","public opponent retains last received ship/sector through entity retirement without opening F6");
    auto own=std::find_if(data.rows.begin(),data.rows.end(),[&](auto& row){return row.id==self;});require(own!=data.rows.end() && own->dockedSeconds==1,"replica dock timer uses the authoritative timestamp, not report receipt");
    require(std::any_of(data.rows.begin(),data.rows.end(),[&](auto& row){return row.id==droneData.shipID && !row.current;}),"unseen drone is retained until an authorized death notification");
    authority.core()->GetIgcSite()->KillShipEvent(authority.core()->GetLastUpdate(),drone,nullptr,100,drone->GetPosition(),Vector::GetZero());authority.authorityStep({},.05);authority.beginNetworkPublication();hidden.events=authority.networkFrame(self,6,0).events;std::erase_if(hidden.shipIntel,[&](auto& intel){return intel.id==droneData.shipID;});replica.applyNetworkFrame(hidden);data=replica.playerPaneData();
    require(std::none_of(data.rows.begin(),data.rows.end(),[&](auto& row){return row.id==droneData.shipID;}),"authorized ship explosion clears retired drone status");
    hidden.members.clear();std::erase_if(hidden.shipIntel,[&](auto& intel){return intel.id==enemy;});replica.applyNetworkFrame(hidden);hidden.members.push_back(member);replica.applyNetworkFrame(hidden);data=replica.playerPaneData();
    remembered=std::find_if(data.rows.begin(),data.rows.end(),[&](auto& row){return row.id==enemy;});require(remembered!=data.rows.end() && remembered->ship=="<none>" && remembered->sector==NA,"departure followed by ID reuse cannot resurrect a previous closed-pane sighting");
    hidden.members.clear();replica.applyNetworkFrame(hidden);data=replica.playerPaneData();
    require(std::none_of(data.rows.begin(),data.rows.end(),[&](auto& row){return row.id==enemy;}),"public departure clears remembered pilot even without a live IGC entity");
    Game offline(art,[](auto&,auto&){return ModelHandle{1,1};},0,0,options);int observed=offline.addLocalPilot("Closed-pane opponent",1);
    auto* contact=offline.core()->GetShip(observed);auto* home=offline.playerShip()->GetStation();contact->SetStation(nullptr);contact->SetCluster(home->GetCluster());contact->SetPosition(home->GetPosition()+Vector(500,0,0));
    contact->SetVelocity(Vector::GetZero());contact->SetBB(offline.core()->GetLastUpdate(),offline.core()->GetLastUpdate(),0);
    for(int i=0;i<10;++i)offline.step({},.01);require(contact->GetCurrentEye(offline.playerShip()->GetSide()),"offline opponent enters real scanner sight");
    contact->SetPosition(home->GetPosition()+Vector(50000,0,0));contact->SetBB(offline.core()->GetLastUpdate(),offline.core()->GetLastUpdate(),0);
    for(int i=0;i<10;++i)offline.step({},.01);require(!contact->GetCurrentEye(offline.playerShip()->GetSide()),"offline opponent leaves sight without opening the pane");
    auto local=offline.playerPaneData();auto memory=std::find_if(local.rows.begin(),local.rows.end(),[&](auto& row){return row.id==observed;});
    require(memory!=local.rows.end() && !memory->current && memory->sector==home->GetCluster()->GetObjectID() && memory->ship!="<none>","closed-pane offline sighting survives loss of sight");
    puts("Player status: closed-pane observation, authoritative dock time, stale replica history, observed drone death, public departure and ID reuse passed");
}
void serverHistory(const std::filesystem::path& art) {
    using Status=Allegiance::PlayerShipStatus;
    Status policy;Allegiance::ObservePlayerShip(policy,Status::Flying,10,2,-1,-1,100);
    Allegiance::ObservePlayerShip(policy,Status::Flying,11,3,-1,-1,200);
    require(policy.stateTime==100 && policy.hull==11 && policy.sector==3,"warping and hull change preserve the state clock");
    Allegiance::HidePlayerShip(policy);require(!policy.current() && policy.hull==11 && policy.sector==3,"loss of sight preserves last observation");
    Allegiance::ObservePlayerShip(policy,Status::Docked,11,3,4,-1,300);require(policy.current() && policy.stateTime==300,"new docking observation restarts the state clock");
    SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.rules.miners=0;
    auto loader=[](auto&,auto&){return ModelHandle{1,1};};
    Game authority(art,loader,0,0,options,{},GameRole::Authority);
    int enemy=authority.addLocalPilot("Historical opponent",1),self=authority.state().shipId;
    auto* ship=authority.core()->GetShip(enemy);auto* enemyStation=ship->GetStation();auto* home=authority.playerShip()->GetStation();auto* sector=home->GetCluster();
    ship->SetStation(nullptr);ship->SetCluster(sector);ship->SetPosition(home->GetPosition()+Vector(2500,0,0));ship->SetVelocity(Vector::GetZero());
    DataShipIGC dd{};dd.shipID=authority.core()->GenerateNewShipID();dd.hullID=ship->GetBaseHullType()->GetObjectID();dd.sideID=1;dd.pilotType=c_ptWingman;dd.baseObjectID=NA;dd.partsOffset=sizeof(dd);strcpy(dd.name,"Historical drone");
    auto* created=static_cast<IshipIGC*>(authority.core()->CreateObject(authority.core()->GetLastUpdate(),OT_ship,&dd,sizeof(dd)));require(created,"server history drone fixture");TRef<IshipIGC> drone=created;created->Release();
    drone->SetCluster(sector);drone->SetPosition(ship->GetPosition()+Vector(200,0,0));drone->SetVelocity(Vector::GetZero());
    authority.authorityStep({},.05);auto first=authority.networkFrame(self,3,0);
    auto status=[&](const Net::Frame& f,int id)->const Net::ShipIntel&{auto i=std::find_if(f.shipIntel.begin(),f.shipIntel.end(),[&](auto& s){return s.id==id;});require(i!=f.shipIntel.end(),"server retains faction ship status");return *i;};
    require(status(first,enemy).status.current() && status(first,enemy).status.sector==sector->GetObjectID(),"authority records the real scanner encounter before any roster request");
    ship->SetCluster(enemyStation->GetCluster());ship->SetPosition(enemyStation->GetPosition()+Vector(2500,0,0));
    drone->SetCluster(enemyStation->GetCluster());drone->SetPosition(ship->GetPosition()+Vector(200,0,0));
    authority.authorityStep({},.05);auto hidden=authority.networkFrame(self,6,0);
    require(!status(hidden,enemy).status.current() && status(hidden,enemy).status.sector==sector->GetObjectID(),"sector departure freezes last-known sector instead of exposing the new sector");
    require(std::none_of(hidden.entities.begin(),hidden.entities.end(),[&](auto& e){return e.identity.type==OT_ship && (e.identity.id==enemy || e.identity.id==dd.shipID);}),"history never exports a hidden contact's physical ship");
    Net::Member member;member.id=enemy;member.side=1;Net::text(member.name,ship->GetName());hidden.members.push_back(member);
    auto copied=hidden;auto decoded=Net::frame(Net::packet(copied));require(decoded.shipIntel==hidden.shipIntel,"initialized faction ship records roundtrip on the raw wire");
    Game fresh(art,loader,0,0,options,{},GameRole::Replica);fresh.applyNetworkFrame(decoded);
    auto row=[&](Game& game,int id){auto data=game.playerPaneData();auto i=std::find_if(data.rows.begin(),data.rows.end(),[&](auto& r){return r.id==id;});require(i!=data.rows.end(),"historical row exists on a fresh replica");return *i;};
    auto remembered=row(fresh,enemy);require(!fresh.core()->GetShip(enemy) && !remembered.current && remembered.sector==sector->GetObjectID() && remembered.ship!="<none>","late join restores faction history without a hidden physical replica");
    require(!row(fresh,dd.shipID).current,"late join also restores observed drone identity");
    authority.core()->GetIgcSite()->KillShipEvent(authority.core()->GetLastUpdate(),drone,nullptr,100,drone->GetPosition(),Vector::GetZero());authority.authorityStep({},.05);
    auto dead=authority.networkFrame(self,9,0);dead.members.push_back(member);
    require(std::none_of(dead.events.begin(),dead.events.end(),[&](auto& e){return e.kind==Net::EventKind::Explosion && e.sector==enemyStation->GetCluster()->GetObjectID();}),"hidden-sector death supplies no physical explosion to the viewer");
    fresh.applyNetworkFrame(dead);auto roster=fresh.playerPaneData();require(std::none_of(roster.rows.begin(),roster.rows.end(),[&](auto& r){return r.id==dd.shipID;}),"authoritative retirement clears an unseen dead drone without an explosion event");
    authority.core()->GetIgcSite()->KillShipEvent(authority.core()->GetLastUpdate(),ship,nullptr,-1,ship->GetPosition(),Vector::GetZero());authority.authorityStep({},.05);
    auto restarted=authority.networkFrame(enemy,12,0);auto restart=status(restarted,enemy);
    require(restart.status.current() && restart.status.state==Status::Docked && restart.status.station==enemyStation->GetObjectID(),"own faction sees human death/restart docking status");
    auto hostile=authority.networkFrame(self,12,0);require(!status(hostile,enemy).status.current() && status(hostile,enemy).status.hull==status(hidden,enemy).status.hull && status(hostile,enemy).status.sector==sector->GetObjectID(),"unseen human restart does not reveal its new hull or sector to opponents");
    ship->SetStation(nullptr);ship->SetCluster(sector);ship->SetPosition(home->GetPosition()+Vector(2500,0,0));ship->SetVelocity(Vector::GetZero());authority.authorityStep({},.05);
    auto seenAgain=authority.networkFrame(self,15,0);auto beforeDeath=status(seenAgain,enemy).status;
    require(beforeDeath.current(),"human recovery is observed in flight before the next death");
    authority.core()->GetIgcSite()->KillShipEvent(authority.core()->GetLastUpdate(),ship,nullptr,-1,ship->GetPosition(),Vector::GetZero());authority.authorityStep({},.05);
    auto recovery=authority.networkFrame(self,18,0);require(!status(recovery,enemy).status.current() && status(recovery,enemy).status.hull==beforeDeath.hull && status(recovery,enemy).status.sector==sector->GetObjectID(),"observed death does not expose the unseen restart hull or destination");
    ship->SetStation(nullptr);ship->SetPosition(enemyStation->GetPosition()+Vector(2500,0,0));enemyStation->Terminate();
    authority.core()->GetIgcSite()->KillShipEvent(authority.core()->GetLastUpdate(),ship,nullptr,-1,ship->GetPosition(),Vector::GetZero());authority.authorityStep({},.05);
    auto noRestart=authority.networkFrame(enemy,21,0);auto gone=status(noRestart,enemy).status;
    require(gone.current() && gone.state==Status::Dead && gone.hull==NA && gone.sector==NA,"death with no restart clears own hull and location without retiring the human identity");
    puts("Server ship history: sighting/loss, raw records, fresh-join bootstrap, no hidden pose, unseen drone retirement and faction-scoped human restart passed");
}
void crew(const std::filesystem::path& art) {
    SessionOptions options;options.scenario=Scenario::FreeFlight;options.startInHangar=true;options.enemies=0;
    Game game(art,[](auto&,auto&){return ModelHandle{1,1};},0,0,options);int gunner=game.state().shipId,parent=game.addLocalPilot("Turret carrier",0,290);
    require(parent>=0 && game.boardShip(parent),"real IGC crew shortcut fixture");int mount=game.state().turret;
    require(!game.promoteTurret(0) && !game.swapTurret(0),"passenger cannot swap or promote a parent's turret");
    require(game.controlLocalPilot(parent),"control gunship pilot");auto* ship=game.playerShip();auto* hull=ship->GetBaseHullType();int index=mount-hull->GetMaxFixedWeapons();
    require(index>=0 && index<4 && !game.swapTurret(index),"turret swap is unavailable while docked");
    if(!ship->GetMountedPart(ET_Weapon,mount))for(auto* p=game.core()->GetPartTypes()->first();p;p=p->next())if(p->data()->GetEquipmentType()==ET_Weapon && hull->CanMount(p->data(),mount)){auto* w=ship->CreateAndAddPart(p->data(),mount,1);w->SetMountedFraction(1);break;}
    game.launch();require(!game.state().docked,"gunship launch for turret swap");
    auto* weapon=ship->GetMountedPart(ET_Weapon,mount);require(weapon && game.swapTurret(index) && ship->GetMountedPart(ET_Weapon,mount)!=weapon,"original turret shortcut swaps the non-fixed hardpoint with cargo");
    require(!game.promoteTurret(-1) && !game.promoteTurret(4) && game.promoteTurret(index) && game.playerShip()->GetParentShip() && game.playerShip()->GetParentShip()->GetObjectID()==gunner,"original promotion index transfers control to the gunner occupying that hardpoint");
    puts("Crew shortcuts: original turret indexing, cargo swap, dock/passenger gates and actual gunner promotion passed");
}
}
int main(int argc,char** argv){try{auto art=argc>1?std::filesystem::path(argv[1]):Assets::steamArtwork();{Assets assets(art);NativeTest::checkRosterStates(assets);NativeTest::checkConsoleViewTransitions(assets);}hangarConsoleActions(art);capturedControls(art);ordering(art);check(art);boot(art);popupRefresh(art);history(art);serverHistory(art);crew(art);return 0;}catch(const std::exception& e){fprintf(stderr,"Player pane check: %s\n",e.what());return 1;}}
