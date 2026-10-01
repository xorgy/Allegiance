#pragma once
#include "../frontend.hpp"
#include "../test_fixture.hpp"
#include "../ui_constants.hpp"
#include <chrono>

namespace NativeTest {
inline void checkConnectionReports(Native::Assets& assets){
    using namespace Native;OriginalUi mdl(assets);
    for(auto color:{"white","black","red","green","blue","yellow","cyan"})require(mdl.color("mmlpane",color)==*originalColor(color),"original effect color export resolves as white");
    auto small=assets.font("smallFont"),large=assets.font("medBoldVerdana");
    auto page=compileMmlPage(mdl,"<Font|medBoldVerdana>A<Font|smallFont>B<p><p>C\tD",450);
    require(page.text.size()==4 && page.text[0].bounds.y+page.text[0].bounds.w==page.text[1].bounds.y+page.text[1].bounds.w,"MML font changes do not bottom-align mixed-height words");
    require(page.text[2].bounds.y==std::max(small->height,large->height)+small->height && page.text[3].bounds.x==16,"MML paragraph blank line or 16-pixel tab semantics differ from PagePane");
    UiStrings literal{{"name","<Color|red>Literal pilot"}};page=compileMmlPage(mdl,"<Color|cyan><String|name>",450,literal);
    require(page.text.size()==1 && page.text.front().text==literal["name"]+" " && page.text.front().color==glm::vec4(0,1,1,1),"dynamic MML String content is interpreted as markup or loses color");
    auto width=small->glyphs['W'].z*2+small->glyphs[' '].z;
    page=compileMmlPage(mdl,"WW WW WW",width);require(page.lines.size()==3 && page.height==3*small->height,"MML whole-word wrapping differs from original PagePane");
    float wide=8*small->glyphs['W'].z+small->glyphs[' '].z;page=compileMmlPage(mdl,"WWWWWWWW\tB",wide);
    require(page.text.size()==2 && page.text[1].bounds.x==(std::floor(wide/16)+1)*16-wide,"MML wrapped tab recomputes the original PagePane advance");
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.setPlaying(true);
    GameState state;state.docked=true;state.networked=true;state.shipId=20;state.networkPilots={{20,0,"Self",true,true,true,true},{42,1,"<Color|red>Literal pilot"}};
    ui.open(Screen::Hangar,&state);ui.open(Screen::Pause,&state);auto query=ui.key(SDLK_P,SDL_SCANCODE_P);
    require(query.effect==FrontEffect::PlayerPings && !query.value && ui.screen()==Screen::Hangar && !ui.keyboardPopup(),"original Player Pings accelerator does not close its menu and request the report");
    std::array<Allegiance::PlayerConnectionInfo,3> rows{{{20,0,0},{42,300,15},{99,150,5}}};ui.connectionInfo(rows,state);
    auto text=[&](std::string_view value,glm::vec4 color){return std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.name=="detailsText" && c.text.find(value)!=std::string::npos && c.color==color;});};
    require(ui.screen()==Screen::GameDetails && !ui.wantsText() && text("Connection Info of Players",{1,1,0,1}) && text("Your Connection Info",{0,1,1,1}),"connection report loses original MML heading/self section/input capture");
    require(text("10ms",{0,1,0,1}) && text("300ms",{1,0,0,1}) && text("15%",{1,0,0,1}) && text("155ms",{1,1,0,1}),"connection report loses original thresholds, minimum ping or human averages");
    require(text("<Color|red>Literal pilot",{0,1,1,1}),"connection report treats a public pilot name as markup");
    require(ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE).effect==FrontEffect::None && ui.screen()==Screen::Hangar,"docked connection popup does not restore its parent");
    state.docked=false;ui.open(Screen::Pause,&state);query=ui.key(SDLK_P,SDL_SCANCODE_P);require(query.effect==FrontEffect::PlayerPings && query.value,"flight Player Pings does not resume while awaiting its response");ui.connectionInfo(rows,state);
    require(ui.view().overlay && ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect==FrontEffect::Resume,"connection popup replaces the flight scene or fails to resume it");
    for(int count:{1024,2048}){
        state.networkPilots.clear();std::vector<Allegiance::PlayerConnectionInfo> fleet;fleet.reserve(count);
        for(int i=0;i<count;++i){state.networkPilots.push_back({i,i%4,"Pilot "+std::to_string(i)});fleet.push_back({i,uint16_t(10+i%500),uint8_t(i%25)});}state.shipId=0;
        auto start=std::chrono::steady_clock::now();ui.connectionInfo(fleet,state);auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        auto visible=std::count_if(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.name=="detailsText";});require(visible<150,"large connection report submits offscreen pilot rows");
        for(int i=0;i<count/10+2;++i){auto item=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& c){return c.action.op==MenuOp::ScrollPage && c.action.delta>0;});if(item==ui.view().items.end()){require(i>0,"large connection report loses scrollbar");break;}ui.activate(size_t(item-ui.view().items.begin()));}
        require(text("Pilot "+std::to_string(count-1),{0,1,1,1}) && text("Average Ping",glm::vec4(1)),"connection report cannot scroll to its final pilot/averages");
        printf("Connection report %d pilots: snapshot/layout %.3f ms, %td visible text spans\n",count,elapsed,visible);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    }
    puts("Original connection reports: effect colors, PagePane fonts/paragraphs/tabs/wrapping/literal values, original P action, thresholds/self/averages, flight/docked parents and bounded full-roster scrolling passed");
}
}
