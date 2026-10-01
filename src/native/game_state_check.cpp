#include "game_state_pane.hpp"
#include "../ClientCore/GameStatePresentation.h"
#include "test_fixture.hpp"
#include <thread>
using namespace Native;
using NativeTest::require;
int main(int argc,char** argv)try {
    require(argc>1,"ARTWORK argument required");
    struct Score {int kills,deaths;};std::array<Score,4> scores{{{5,2},{5,1},{5,1},{1,0}}};
    auto greater=[](Score a,Score b){return Allegiance::GameStateKillsGreater(a.kills,a.deaths,b.kills,b.deaths);};
    auto ranks=Allegiance::GameStateRanks<Score>(scores,0,greater);
    require(ranks[0].index==1 && ranks[1].index==0 && ranks[1].rank==3,"kill ties use fewer deaths; equal leaders keep list order");
    ranks=Allegiance::GameStateRanks<Score>(scores,2,greater);
    require(ranks[0].index==2 && ranks[0].rank==1 && ranks[1].index==1 && ranks[1].rank==2,"self wins an exact top tie and best other is labelled second");
    require(Allegiance::GameStateClock(false,73)=="Time Elapsed: 1:13" && Allegiance::GameStateClock(true,60)=="Time Remaining: 1:00" &&
        Allegiance::GameStateClock(true,-1)=="Game Over" && Allegiance::GameStateClock(false,-1)=="Game Starting","original timer strings and truncation boundaries");
    Assets assets(argv[1]);SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.rules.miners=0;
    options.map=2;options.rules.mapSize=1;options.rules.conquest=80;options.rules.territory=50;options.rules.prosperity=1;options.rules.artifacts=3;options.rules.flags=4;options.rules.teamKills=200;options.rules.minutes=10;
    Game game(argv[1],[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
    auto data=game.gameStateData();require(data.tiles.size()==6 && data.tiles[0].description=="1st team to own 80% of the starbases wins" &&
        data.tiles[2].description=="First team to purchase reinforcements wins" && data.tiles[3].description=="First team to own 3 artifacts wins" &&
        data.tiles[4].description=="First team to capture 4 flags wins" && data.tiles[5].title=="wctitledeathmatchbmp" && data.clock=="Time Remaining: 10:00","all original win-condition tiles and clock use mission settings");
    int peer=game.addLocalPilot("Other pilot",1);require(peer>=0,"standing pilot fixture");auto* opponent=game.core()->GetShip(peer);opponent->SetKills(3);opponent->SetDeaths(2);
    data=game.gameStateData();require(data.tiles.back().lines[2].columns[1]=="Other pilot" && data.tiles.back().lines[3].columns[0]=="#2" && data.tiles.back().lines[2].columns[2]=="3 kills","human standings include an unseen pilot, exclude drones, and use original score text");
    GameStatePane pane(assets);pane.toggle(game);const glm::ivec2 size{2560,1440};auto& view=pane.view(size,1);
    require(view.original && view.overlay && view.slide && view.slide->off.x==-536 && view.canvas==size,"original pane slides from its left anchor at authored pixel size");
    auto header=std::find_if(view.artwork.begin(),view.artwork.end(),[](auto& c){return c.name=="gamestateheaderbmp";});require(header!=view.artwork.end() && header->bounds.x==36,"original left anchor is 36 pixels, independent of window width");
    require(view.items.size()==1 && view.items[0].bounds.y==header->bounds.y+4 && view.items[0].bounds.w==16,"Game State close button follows authored translation and nested justification");
    require(std::count_if(view.artwork.begin(),view.artwork.end(),[](auto& c){return c.name.starts_with("gameStateCell/");})==50,"original grids render each row once, with three/five dynamically sized columns");
    auto* unchanged=&view.artwork.front();pane.sync(game);require(&pane.view(size,1).artwork.front()==unchanged,"unchanged game state retains compiled artwork");
    auto& doubled=pane.view(size,2);require(doubled.classicScale==2 && doubled.canvas==glm::ivec2(1280,720),"integer UI scaling preserves authored pane size and anchors");
    std::this_thread::sleep_for(std::chrono::milliseconds(520));
    auto click=[&](bool down,glm::vec2 local){auto& v=pane.view(size,2);return pane.pointer(size,originalMenuLayout(size,2,v.canvas).pixel(local),SDL_BUTTON_LEFT,down);};
    auto close=glm::vec2(doubled.items[0].bounds)+glm::vec2(4);
    require(!click(true,{1200,650}),"uncovered scene retains pointer input");
    require(click(true,close) && pane.pointerCaptured() && click(false,{1200,650}) && pane.active(),"release outside the close button cancels dismissal and cannot leak into gameplay");
    require(!click(false,{1200,650}),"released Game State close retains stale capture for unrelated mouse edges");
    require(click(true,close),"Game State secondary-release capture fixture");
    auto outside=originalMenuLayout(size,2,pane.view(size,2).canvas).pixel({1200,650});
    require(pane.pointer(size,outside,SDL_BUTTON_RIGHT,false) && !pane.pointerCaptured() && pane.active(),"ordinary Game State close does not retire capture on original right release");
    require(!pane.pointer(size,outside,SDL_BUTTON_RIGHT,false),"right release leaves stale Game State capture");
    require(click(true,close) && click(false,close) && !pane.active() && pane.view(size,2).items.empty(),"close button dismisses on release; exit animation has no active hits");
    pane.toggle(game);require(pane.key(SDLK_ESCAPE) && !pane.active(),"Escape closes the original banishable console");
    puts("Original Game State rankings, mission tiles, timers, bitmap layout, integer scaling, caching and pointer capture passed");return 0;
}catch(const std::exception& e){fprintf(stderr,"Game State check: %s\n",e.what());return 1;}
