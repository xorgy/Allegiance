#include "game.hpp"
#include "test_fixture.hpp"
#include <limits>
using NativeTest::require;
int main(int argc,char** argv) {
    try {
        auto art=argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork();
        Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;options.startInHangar=true;options.rules.miners=0;
        auto loader=[](auto&,auto&){return Native::ModelHandle{1,10};};
        int worlds=0;
        for(int teams=2;teams<=6;++teams)for(int map=0;map<int(Native::conquestMapNames.size());++map) {
            options.rules.teams=teams;options.team=teams-1;options.map=map;
            if(!Native::Game::validMap(map,teams)){require(!Native::Game::missionError(options).empty(),"unsupported map/team combination accepted");continue;}
            require(Native::Game::missionError(options).empty(),"original valid map rejected");
            Native::Game game(art,loader,0,0,options);auto* core=game.core();auto* pilot=game.playerShip();
            require(core->GetSides()->n()==teams && core->GetMissionParams()->mmMapType==Native::conquestMapIds[size_t(map)] && pilot->GetSide()->GetObjectID()==teams-1,"generated mission discarded selected teams, map or seat");
            require(pilot->GetStation() && pilot->GetStation()->GetSide()==pilot->GetSide() && !game.state().pod,"selected team receives its actual restart base and starter");
            for(auto* link=core->GetSides()->first();link;link=link->next())require(link->data()->GetStations()->n()>0,"generated team missing its base");
            game.launch();for(int i=0;i<35;++i)game.step({},.1);
            require(!game.state().dead && !game.state().docked && pilot->GetCluster(),"map/team starter cannot launch");++worlds;
        }
        options={};options.scenario=Native::Scenario::Conquest;options.startInHangar=true;options.map=2;
        options.rules.teams=4;options.team=2;auto& r=options.rules;
        r.mapSize=1;r.maxDrones=6;r.miners=3;r.randomEncounters=2;r.showMap=r.showHome=true;r.alliedSight=r.alliedRipcord=true;
        r.homeAsteroids=7;r.neutralAsteroids=11;r.homeMineable=3;r.neutralMineable=5;r.homeSpecial=2;r.neutralSpecial=3;
        r.homeTreasures=2;r.neutralTreasures=3;r.homeTreasureRate=.1f;r.neutralTreasureRate=.4f;r.shipyard=false;r.expansion=false;r.resourceAmounts=false;r.ejectPods=false;
        {Native::Game game(art,loader,0,0,options);auto* p=game.core()->GetMissionParams();
            require(p->nTeams==4 && p->iMapSize==1 && p->nMaxDronesPerTeam==6 && p->nInitialMinersPerTeam==3 && p->iRandomEncounters==2,"mission topology/economy settings lost");
            require(p->bShowMap && p->bShowHomeSector && p->bAllowAlliedViz && p->bAllowAlliedRip && !p->bAllowShipyardPath && !p->bAllowExpansionPath && !p->bResourceAmountsVisible && !p->bEjectPods,"mission capability flags lost");
            require(p->nPlayerSectorAsteroids==7 && p->nNeutralSectorAsteroids==11 && p->nPlayerSectorMineableAsteroids==3 && p->nNeutralSectorMineableAsteroids==5 && p->nPlayerSectorSpecialAsteroids==2 && p->nNeutralSectorSpecialAsteroids==3,"authored resource population settings lost");
            require(p->nPlayerSectorTreasures==2 && p->nNeutralSectorTreasures==3 && p->nPlayerSectorTreasureRate==.1f/60 && p->nNeutralSectorTreasureRate==.4f/60,"treasure population/rate units lost");
            game.launch();game.eject();game.step({},1./60);require(game.state().docked && !game.state().pod && game.state().deaths==1,"no-pod death must complete the original deferred dock/refit lifecycle");
        }
        r={};options.team=0;r.developments=false;
        {Native::Game game(art,loader,0,0,options);require(game.state().research.empty() && game.state().units.size()==1,"no-developments mission creates research or initial miners");
            int money=game.state().money;for(int i=0;i<1000;++i)game.step({},.1);
            require(game.state().money==money,"no-developments mission still pays development income");
            const auto& attributes=game.playerShip()->GetSide()->GetGlobalAttributeSet();GlobalAttributeSet defaults;defaults.Initialize();
            require(!std::memcmp(&attributes,&defaults,sizeof(defaults)),"no-developments mission retains civilization attribute bonuses");
        }
        r={};r.helium=std::numeric_limits<float>::quiet_NaN();require(!Native::Game::missionError(options).empty(),"NaN mission accepted");
        r={};r.invulnerableStations=true;require(!Native::Game::missionError(options).empty(),"invulnerable conquest accepted");
        r={};r.prosperity=1;r.developments=false;require(!Native::Game::missionError(options).empty(),"prosperity without developments accepted");
        r={};r.teams=2;options.map=0;r.territory=50;require(!Native::Game::missionError(options).empty(),"original invalid small territory map accepted");
        auto maps=Native::Game::customMaps(art);require(!maps.empty(),"installed custom-map catalog absent");
        int customWorlds=0,unavailable=0;
        for(const auto& map:maps){
            options={};options.scenario=Native::Scenario::Conquest;options.startInHangar=true;options.customMap=map.file;options.rules.teams=map.teams;options.rules.miners=0;
            require(Native::Game::missionError(options,maps).empty(),"catalogued map/team combination rejected");
            auto preview=Native::Game::mapPreview(options,art);
            if(preview.error=="no preview available"){
                bool rejected=false;try{Native::Game game(art,loader,0,0,options);}catch(const std::exception&){rejected=true;}
                require(rejected,"unavailable catalog map silently generates a different world");++unavailable;
                printf("Unavailable catalog map %s: original file absent; creation rejected\n",map.file.c_str());continue;
            }
            require(preview.error.empty() && !preview.sectors.empty(),"original custom map preview missing/invalid");
            printf("Custom map %s: %zu sectors, %zu aleph links\n",map.file.c_str(),preview.sectors.size(),preview.links.size());fflush(stdout);
            Native::Game game(art,loader,0,0,options);
            require(std::string(game.core()->GetMissionParams()->szCustomMapFile)==map.file && game.core()->GetClusters()->n()==int(preview.sectors.size()),"custom map falls back to generated topology");
            require(game.core()->GetWarps()->n()==int(preview.links.size()*2),"custom map preview and actual aleph topology disagree");
            for(const auto& sector:preview.sectors){auto* actual=game.core()->GetCluster(sector.id);require(actual,"custom preview loses authored sector identity");std::array<int,6> stations{};
                require(sector.name==actual->GetName(),"custom preview loses original sector names used by the briefing");
                for(auto* station=actual->GetStations()->first();station;station=station->next())++stations.at(station->data()->GetSide()->GetObjectID());
                require(stations==sector.stations,"custom preview station ownership differs from IGC");}
            for(auto* team=game.core()->GetSides()->first();team;team=team->next())require(team->data()->GetStations()->n()>0,"custom map loses a faction's base");
            game.launch();for(int i=0;i<35;++i)game.step({},.1);require(!game.state().dead && !game.state().docked && game.playerShip()->GetCluster(),"custom-map starting ship cannot launch");
            ++customWorlds;
        }
        options.rules.teams=3;require(!Native::Game::missionError(options,maps).empty(),"wrong custom-map faction count accepted");
        options.customMap="../core";require(!Native::Game::missionError(options,maps).empty(),"custom map accepts a path instead of an original basename");
        options.customMap="no_map";require(!Native::Game::missionError(options,maps).empty(),"uncatalogued custom map accepted");
        printf("%d generated and %d file-backed original worlds initialize and launch; %d absent catalog files reject without fallback. Every available custom preview matches IGC sector/aleph/station ownership, with map/team validation passed.\n",worlds,customWorlds,unavailable);return 0;
    } catch(const std::exception& e){fprintf(stderr,"setup: %s\n",e.what());return 1;}
}
