#pragma once
#include "../frontend.hpp"
#include "../game.hpp"
#include "../test_fixture.hpp"
#include <set>

namespace NativeTest {
// PartListItem::Construct and DrawItem in WinTrek/loadout.cpp. Model cards
// retain their authored alpha; only the fallback, highlight and brackets key
// black. Price/status are painted before the title on the shared row surface.
inline void checkResearchCards(Native::Assets& assets,
    const std::function<void(const Native::MenuView&,const Native::MenuView&,std::string_view)>& comparePixels={}){
    using namespace Native;
    std::set<std::string> models;
    for(const char* core:{"static_core","PCore014"}){
        SessionOptions options;options.scenario=Scenario::Conquest;options.coreFile=core;options.enemies=0;options.rules.miners=0;options.startInHangar=true;
        Game game(assets.root(),[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
        auto collect=[&](auto* list){for(auto* link=list->first();link;link=link->next()){
            std::string model=link->data()->GetModelName(),name="i"+model+"bmp";
            std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return std::tolower(c);});
            if(assets.hasImage(name))models.insert(std::move(model));
        }};
        collect(game.core()->GetHullTypes());collect(game.core()->GetPartTypes());collect(game.core()->GetDroneTypes());collect(game.core()->GetStationTypes());collect(game.core()->GetDevelopments());
    }
    require(!models.empty(),"original research card fixtures are absent");
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.setPlaying(true);
    GameState state;state.docked=state.economy=true;state.money=1000;state.ship="Scout";state.station="Home";
    ResearchChoice row;row.id=11;row.group=0;row.type=OT_development;row.name=std::string(80,'W');row.price=100;row.canBuy=row.available=row.predecessorReady=true;
    ui.researchService([&]{return std::vector{row};});size_t cases=0,blackCards=0,pixelCases=0;
    for(const auto& model:models){
        row.model=model;ui.open(Screen::Hangar,&state);ui.open(Screen::Research,&state);
        auto name="i"+model+"bmp";std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return std::tolower(c);});
        auto authored=assets.image(name,false),keyed=assets.image(name,true);
        const auto& art=ui.view().artwork;auto card=std::find_if(art.begin(),art.end(),[&](const auto& c){return c.name==name;});
        require(card!=art.end() && card->image==authored && card->clip,"research card overrides the original LoadImage(false) alpha policy");
        if(authored->pixels!=keyed->pixels)++blackCards;
        auto price=std::find_if(art.begin(),art.end(),[](const auto& c){return c.name=="investment/11/text" && c.text=="$ 100";});
        auto title=std::find_if(art.begin(),art.end(),[&](const auto& c){return c.name=="investment/11/text" && c.text==row.name;});
        require(price!=art.end() && title!=art.end() && price<title,"research title/price overlap differs from original surface painter order");
        require(card->bounds.x+137==price->bounds.x && card->bounds.x+1==title->bounds.x && card->bounds.y-1==title->bounds.y,"research text and artwork leave their original row anchors");
        if(comparePixels && authored->pixels!=keyed->pixels && pixelCases<4){
            auto actual=ui.view();actual.slide.reset(); // Compare the same settled pane, not two times in its entrance animation.
            auto expected=actual;auto at=size_t(card-art.begin());expected.artwork[at].image=authored;
            // Reorder independently from the native insertion sequence. A
            // long label makes the original title-over-price rule observable.
            auto p=size_t(price-art.begin()),t=size_t(title-art.begin());
            if(p>t){std::swap(expected.artwork[p],expected.artwork[t]);std::swap(expected.artwork[p-1],expected.artwork[t-1]);}
            comparePixels(actual,expected,"research-card-"+std::to_string(pixelCases++));
        }
        ++cases;
    }
    require(blackCards>0,"research alpha regression does not exercise any authored opaque black pixels");
    row.model="missing-card";ui.open(Screen::Hangar,&state);ui.open(Screen::Research,&state);
    require(std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.name=="conloadoutitembmp" && c.image==assets.image("conloadoutitembmp",true);}),"missing research card loses original keyed fallback");
    printf("Original research cards: %zu models across two cores, %zu with distinct black-key pixels, surface text order/anchors and missing-card fallback passed\n",cases,blackCards);
}
}
