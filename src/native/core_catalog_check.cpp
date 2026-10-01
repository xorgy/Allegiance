#include "frontend.hpp"
#include "test_fixture.hpp"
#include "../ClientCore/CoreCatalog.h"
#include <sstream>
#include <fstream>
using NativeTest::require;
int main(int argc,char** argv){
    try{
        using namespace Native;
        std::istringstream sample("3\r\nac_20\r\n PCore016 \r\nzone_core\r\n");auto parsed=Allegiance::ReadCoreCatalog(sample);
        require(parsed==std::vector<std::string>{"ac_20","PCore016","zone_core"} && Allegiance::FindCore(parsed,"pcore016"),"catalog order/case identity differs");
        for(auto invalid:{"","\n","-1\n","33\n","1x\n","2\nac_20\n","2\nac_20\nAC_20\n","1\n../core\n","1\ncore.igc\n","1\n1234567890123\n","1\n\n","0\nextra\n"}){
            bool rejected=false;try{std::istringstream input(invalid);Allegiance::ReadCoreCatalog(input);}catch(const std::exception&){rejected=true;}require(rejected,"malformed core catalog accepted");
        }
        Assets assets(argc>1?argv[1]:Assets::steamArtwork());auto catalog=assets.coreCatalog();require(!catalog.empty(),"installed core catalog empty");
        std::ifstream input(assets.root()/"cores.txt");require(catalog==Allegiance::ReadCoreCatalog(input),"asset catalog substitutes arbitrary installed historical/map cores");
        auto cores=catalog;for(auto baseline:{"static_core","PCore014"})if(!Allegiance::FindCore(cores,baseline))cores.emplace_back(baseline);
        Settings settings;Frontend ui(settings);ui.originalInterface(assets);
        auto item=[&](std::string_view control){auto i=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& c){return c.control==control;});require(i!=ui.view().items.end(),"core authored control missing");return size_t(i-ui.view().items.begin());};
        auto click=[&](std::string_view control){return ui.activate(item(control));};
        auto hasText=[&](std::string_view value){return std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.kind==OriginalControl::Kind::Text && c.text==value;});};
        ModelLoader loader=[](const auto&,const auto&){return ModelHandle{1,1};};size_t definitions=0,factions=0;
        for(const auto& core:cores){
            float winMoney{};auto choices=Game::civilizations(assets.root(),&winMoney,core);require(!choices.empty(),"catalogued core has no factions");
            SessionOptions options;options.scenario=Scenario::Conquest;options.coreFile=core;options.startInHangar=true;options.rules.teams=6;options.rules.miners=0;options.enemies=0;
            Game game(assets.root(),loader,0,0,options);
            require(Allegiance::CoreFileEqual(game.core()->GetMissionParams()->szIGCStaticFile,core) && game.sessionOptions().coreFile==core,"actual IGC mission ignores selected core");
            require(game.core()->GetFloatConstant(c_fcidWinTheGameMoney)==winMoney,"briefing prosperity constant comes from another core");
            for(int side=0;side<6;++side){auto* civ=game.core()->GetSide(SideID(side))->GetCivilization();auto& choice=choices[size_t(side)%choices.size()];require(civ->GetObjectID()==choice.id && choice.name==civ->GetName() && choice.icon==civ->GetIconName(),"selected core faction catalog disagrees with actual sides");++factions;}
            auto* ownHull=game.core()->GetHullType(game.currentLoadout().hull);require(ownHull && game.playerShip()->GetStation() && game.playerShip()->GetStation()->CanBuy(ownHull),"selected core starts in an unavailable hull");
            for(const auto& hull:game.state().hullChoices){auto* type=game.core()->GetHullType(HullID(hull.id));require(type && hull.name==type->GetName(),"hangar hull choice comes from another core");++definitions;}
            for(const auto& part:game.currentLoadout().parts){auto* type=game.core()->GetPartType(PartID(part.id));require(type,"hangar part choice comes from another core");++definitions;}
            require(sessionLoadoutFile(game.sessionOptions())=="loadouts_"+core+".dat","saved equipment shares another core's bank");
            auto wire=Net::session(game.sessionOptions());auto restored=Net::options(wire,0,"Viewer");require(restored.coreFile==core,"session/replica drops the selected core");
            ui.networkSession(game.sessionOptions());ui.setPlaying(true);ui.open(Screen::Pilots,&game.state());require(hasText(assets.coreName(core)),"original lobby labels a scenario default instead of actual core");
            require(hasText(choices[0].name),"original faction labels use stale default-core names");
            ui.missionDetailsService([&]{return Allegiance::DescribeMission(*game.core()->GetMissionParams());});ui.open(Screen::GameDetails);
            require(std::any_of(ui.detailsView().artwork.begin(),ui.detailsView().artwork.end(),[&](const auto& c){return c.kind==OriginalControl::Kind::Text && c.text.find(assets.coreName(core))!=std::string::npos && (!assets.officialCore(core) || c.color==glm::vec4(0,1,0,1));}),"Game Details loses configured core name/color");
            ui.missionDetailsService({});
            ui.setPlaying(false);ui.open(Screen::Games);click("newGameButtonPane");

            printf("CORE UI %s: %zu factions, starter %d, %zu available hulls, isolated loadout bank\n",core.c_str(),choices.size(),game.currentLoadout().hull,game.state().hullChoices.size());
        }
        ui.setPlaying(false);ui.open(Screen::Games);click("newGameButtonPane");
        auto badge=assets.image("iconscorescountbmp",true);size_t expectedBadges=assets.officialCore("static_core");for(const auto& core:catalog)if(!Allegiance::CoreFileEqual(core,"static_core"))expectedBadges+=assets.officialCore(core);
        require(std::count_if(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.image==badge;})==int(expectedBadges),"original configured core badges are missing or invented");
        auto coreRows=std::count_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& c){return c.action.op==MenuOp::GameCreateCore;});
        require(coreRows>=int(catalog.size()),"creation core list omits installed catalog entries");
        auto pick=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& c){return c.action.op==MenuOp::GameCreateCore && c.action.value==2;});require(pick!=ui.view().items.end(),"creation alternate core absent");ui.activate(size_t(pick-ui.view().items.begin()));std::string chosen=ui.createCore();
        if(assets.officialCore(chosen)){auto fill=std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.kind==OriginalControl::Kind::Rectangle && c.color==glm::vec4(0,0,1,1);});require(fill!=ui.view().artwork.end(),"selected core fill missing");
            auto icon=std::find_if(fill,ui.view().artwork.end(),[&](const auto& c){return c.image==badge && std::abs(c.bounds.y+c.bounds.w/2-(fill->bounds.y+7))<1;});require(icon!=ui.view().artwork.end(),"selected core background covers official badge");}
        click("create/server/0");click("createOkButtonPane");
        require(ui.screen()==Screen::Lobby && ui.session().coreFile==chosen,"local Create Game drops selected core");ui.open(Screen::LobbyPilots);require(hasText(assets.coreName(chosen)),"local lobby labels another core");
        auto bad=ui.session();bad.coreFile="../bad";require(!Game::missionError(bad).empty() && !Game::mapPreview(bad,assets.root()).error.empty(),"unsafe core filename reaches IGC/preview");
        bad={};bad.coreFile="ac_20";require(!Game::missionError(bad).empty(),"practice core changes independently of authored world");
        printf("Core catalog: parser bounds, installed policy, %zu real cores, %zu faction selections, %zu actual hangar definitions, session identity, original labels/core selection and isolated equipment banks passed\n",cores.size(),factions,definitions);
        return 0;
    }catch(const std::exception& e){fprintf(stderr,"Core catalog: %s\n",e.what());return 1;}
}
