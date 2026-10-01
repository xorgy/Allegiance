#include "game.hpp"
#include "test_fixture.hpp"
#include "../ClientCore/MissionEconomy.h"
#include "test_reference/investments.hpp"
#include <cstdio>
using NativeTest::require;
int main(int argc,char** argv) {
    try {
        Native::Assets assets(argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork());
        NativeTest::checkPredictedInvestments(assets);
        Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;options.enemies=0;options.startInHangar=true;
        Native::Game game(assets.root(),[&](auto& name,auto&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options,[](const std::string&){return Native::EffectTexture{1,64,64,1};});
        const auto& state=game.state();
        printf("Conquest: %s, %s, money %d, %zu sectors, %zu buckets, %zu units, %zu hulls\n",state.civilization.c_str(),state.ship.c_str(),state.money,state.sectors.size(),state.research.size(),state.units.size(),state.hullChoices.size());
        require(state.economy && state.docked && state.units.size()>=2 && !state.hullChoices.empty(),"generated mission, initial miner and starter ship");
        require(!game.invest(-100) && !game.buyHull(-100),"invalid purchase rejected");
        int before=state.money;
        require(!game.invest(state.research.front().id,-2) && state.money==before,"negative funding rejected without minting money");
        auto miner=std::find_if(state.research.begin(),state.research.end(),[](auto& b){return b.type==OT_droneType && b.available && b.name.find("Miner")!=std::string::npos;});
        require(miner!=state.research.end(),"available authored miner bucket");int bucket=miner->id,cost=miner->price;size_t units=state.units.size();
        require(game.invest(bucket,1) && state.money==before-1,"partial bucket investment debited");
        require(game.invest(bucket) && state.money==before-cost,"remaining bucket cost debited exactly");
        for(int i=0;i<12000 && state.units.size()==units;++i)game.step({},.1);
        require(state.units.size()==units+1,"IGC bucket completion creates and launches a real miner");
        printf("Funded miner completed: %zu units, money %d\n",state.units.size(),state.money);
        bool sawMining=false;
        for(int i=0;i<18000 && !state.minedCredits;++i) {
            game.step({},.1);
            if(i%10==0)for(auto& p:game.particles())sawMining|=(p.texture&Native::particleLine)!=0;
        }
        require(sawMining,"IGC mining state emits the original lightning bolt");
        require(state.minedCredits>0,"real miner mine/unload/relaunch generates income");
        printf("He3 delivered: %d credits\n",state.minedCredits);
        require(!game.order(state.shipId,OT_invalid,NA,32767),"undefined command rejected before IGC");
        auto project=std::find_if(state.research.begin(),state.research.end(),[](auto& b){return b.type==OT_stationType && b.available && b.name=="Light Outpost";});
        require(project!=state.research.end(),"available authored constructor bucket");
        int projectId=project->id;require(game.invest(projectId),"fund constructor");
        int builderId=-1;
        for(int i=0;i<18000 && builderId<0;++i){game.step({},.1);for(auto& unit:state.units)if(unit.type==c_ptBuilder)builderId=unit.id;}
        require(builderId>=0,"IGC production creates a constructor");
        auto rocks=game.orderTargets(builderId,c_cidBuild);
        require(!rocks.empty(),"constructor has discovered compatible build sites");
        const auto rock=rocks.front();
        require(game.order(builderId,rock.type,rock.id,c_cidBuild),"accept legal construction order");
        auto stationCount=[&](){int n=0;for(auto& s:state.sectors)n+=s.stations;return n;};
        int stations=stationCount();
        bool sawInner=false,sawOuter=false;
        for(int i=0;i<24000 && stationCount()==stations;++i) {
            game.step({},.1);
            if(i%10==0)for(auto& draw:game.scene(true)) {
                sawInner|=draw.blend==Native::ModelBlend::Alpha;
                sawOuter|=draw.blend==Native::ModelBlend::Additive;
                require(std::isfinite(draw.transform[0][0]) && std::isfinite(draw.color.a),"finite construction shell");
            }
        }
        require(sawInner && sawOuter,"IGC construction emits animated alpha and additive shells");
        require(stationCount()>stations,"constructor travels, drills and completes original building effect");
        for(int i=0;i<500;++i)game.step({},.1);
        require(std::none_of(state.units.begin(),state.units.end(),[&](auto& u){return u.id==builderId;}),"construction consumes builder");
        printf("Completed Light Outpost at %s / %s\n",rock.name.c_str(),rock.sector.c_str());
        auto research=std::find_if(state.research.begin(),state.research.end(),[](auto& b){return b.type==OT_development && b.available && b.name=="Carbon Mine";});
        require(research!=state.research.end(),"available civilization research");int researchId=research->id;
        auto* development=static_cast<IdevelopmentIGC*>(game.core()->GetSide(SideID(state.playerSide))->GetBucket(BucketID(researchId))->GetBuyable());
        auto completionSound=development->GetCompletionSound();game.clearMessages();game.clearSounds();
        require(game.invest(researchId),"fund technology research");
        bool complete=false;
        for(int i=0;i<18000 && !complete;++i){game.step({},.1);for(auto& b:state.research)if(b.id==researchId)complete=b.complete;}
        require(complete,"IGC development completes and applies civilization technology");
        require(std::count_if(state.messages.begin(),state.messages.end(),[](const auto& m){return m.tip && m.warning && !m.chat && m.text=="Carbon Mine research completed";})==1,"research completion lacks its original critical console tip");
        require(std::count_if(game.sounds().begin(),game.sounds().end(),[&](const auto& s){return s.id==completionSound;})==1,"research completion loses or duplicates its authored sound");
        game.respawn();require(state.docked,"practice respawn cannot bypass economy purchases");
        game.launch();game.step({},.1);require(!state.docked,"economy launch");
        require(!game.buyHull(state.hullChoices.empty()?0:state.hullChoices.front().id),"in-flight hull purchase rejected");
        auto civs=Native::Game::civilizations(assets.root());require(civs.size()==5,"installed civilization catalog");
        for(size_t c=0;c<civs.size();++c)for(size_t m=0;m<Native::conquestMapNames.size();++m) {
            if(!Native::Game::validMap(int(m),options.rules.teams))continue;
            options.civilization=int(c);options.map=int(m);
            Native::Game variant(assets.root(),[&](auto& name,auto&){auto model=assets.model(name);return Native::ModelHandle{1,model->radius,model->frames};},0,0,options);
            require(variant.state().civilization==civs[c].name && !variant.state().pod && variant.state().docked,"civilization/map produces valid starter and base");
            variant.launch();for(int i=0;i<10;++i)variant.step({},.1);
            require(!variant.state().dead && !variant.state().docked,"civilization starter launches");
        }
        printf("All five civilizations and their compatible generated maps initialize and launch.\n");
        return 0;
    } catch(const std::exception& e){fprintf(stderr,"economy: %s\n",e.what());return 1;}
}
