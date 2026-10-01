// Explicit scale/performance diagnostic, separate from the routine CTest suite.
#include "game.hpp"
#include "smoke.hpp"
#include "test_fixture.hpp"
#include "../Igc/clusterIGC.h"
#include "network_codec.hpp"
#include "../Igc/projectileIGC.h"
#include "../Igc/missileigc.h"
#include <algorithm>
#include <bit>
#include <chrono>
#include <fstream>
#include <numeric>

using namespace Native;
using Clock=std::chrono::steady_clock;
namespace {
double milliseconds(Clock::time_point begin,Clock::time_point end){return std::chrono::duration<double,std::milli>(end-begin).count();}
double percentile(std::vector<double> values,double fraction){std::sort(values.begin(),values.end());return values[size_t(fraction*(values.size()-1))];}
uint64_t fingerprint(Game& game) {
    uint64_t result=1469598103934665603ull;auto word=[&](uint32_t x){result=(result^x)*1099511628211ull;};
    auto number=[&](float x){word(std::bit_cast<uint32_t>(x));};
    for(auto* l=game.core()->GetShips()->first();l;l=l->next()) {
        auto* s=l->data();word(uint16_t(s->GetObjectID()));word(s->GetStateM());
        for(const auto& v:{s->GetPosition(),s->GetVelocity(),s->GetOrientation().GetForward(),s->GetOrientation().GetUp()}){number(v.x);number(v.y);number(v.z);}
        number(s->GetFraction());number(s->GetFuel());number(s->GetEnergy());word(s->GetAmmo());word(s->GetKills());word(s->GetDeaths());
        for(Command c=0;c<c_cmdMax;++c){word(s->GetCommandID(c));word(s->GetCommandTarget(c)?uint16_t(s->GetCommandTarget(c)->GetObjectID()):0xffff);}
        for(float control:s->GetControls().jsValues)number(control);
        for(Axis axis=0;axis<3;++axis)number(s->GetCurrentTurnRate(axis));
        for(auto* part=s->GetParts()->first();part;part=part->next()) {
            word(uint16_t(part->data()->GetPartType()->GetObjectID()));word(uint16_t(part->data()->GetMountID()));
            word(part->data()->GetAmount());number(part->data()->GetMountedFraction());
        }
    }
    for(auto* c=game.core()->GetClusters()->first();c;c=c->next())
        for(auto* l=c->data()->GetModels()->first();l;l=l->next()) {
            auto* m=l->data();word(m->GetObjectType());word(uint16_t(m->GetObjectID()));
            for(const auto& v:{m->GetPosition(),m->GetVelocity(),m->GetOrientation().GetForward(),m->GetOrientation().GetUp()}){number(v.x);number(v.y);number(v.z);}
            if(m->GetObjectType()==OT_projectile) {
                auto* p=static_cast<CprojectileIGC*>(m);word(uint32_t(p->NativeShot()));word(uint32_t(p->NativeShot()>>32));word(p->NativeExpireTime().clock());
            }
            if(m->GetObjectType()==OT_missile) {
                auto* p=static_cast<CmissileIGC*>(m);word(uint32_t(p->NativeShot()));word(uint32_t(p->NativeShot()>>32));word(p->NativeExpireTime().clock());number(p->GetFraction());number(p->GetLock());
            }
        }
    word(game.simulationRandomState());
    return result;
}

}
int main(int argc,char** argv) {
    try {
        int ships=1024,ticks=300,warmup=50;float observerDistance=1500;bool spread=false,idle=false,presentation=false,observer=false,authority=false,publication=false;std::string csv;
        for(int i=1;i<argc;++i) {
            std::string arg=argv[i];auto value=[&]{if(++i>=argc)throw std::runtime_error("Missing argument");return std::string(argv[i]);};
            if(arg=="--ships")ships=std::stoi(value());else if(arg=="--ticks")ticks=std::stoi(value());else if(arg=="--warmup")warmup=std::stoi(value());
            else if(arg=="--observer-distance"){observerDistance=std::stof(value());if(!std::isfinite(observerDistance) || observerDistance<=0)throw std::runtime_error("Observer distance must be positive and finite");}
            else if(arg=="--authority")authority=true;else if(arg=="--publication")authority=publication=true;
            else if(arg=="--observe")observer=true;else if(arg=="--spread")spread=true;else if(arg=="--idle")idle=true;else if(arg=="--presentation")presentation=true;else if(arg=="--csv")csv=value();else throw std::runtime_error("Unknown option: "+arg);
        }
        NativeTest::require(ships>0 && ships<=(shipIdentityCapacity-1)/2 && ticks>warmup && warmup>=0,"invalid scale diagnostic counts");
        Assets assets(Assets::steamArtwork());std::vector<std::shared_ptr<ModelData>> owners;std::unordered_map<std::string,ModelHandle> models;auto load=[&](const std::string& name,const std::string&){if(auto it=models.find(name);it!=models.end())return it->second;auto model=assets.model(name);owners.push_back(model);return models.emplace(name,ModelHandle{1,model->radius,model->frames,model->lights}).first->second;};
        std::unordered_map<std::string,EffectTexture> effects;auto effect=[&](const std::string& name){
            if(auto it=effects.find(name);it!=effects.end())return it->second;
            auto image=assets.image(name);return effects.emplace(name,EffectTexture{1,image->width,image->height,int(std::max(1u,image->width/image->height))}).first->second;
        };
        SessionOptions options;options.scenario=Scenario::Skirmish;options.map=0;options.enemies=options.allies=ships;options.killLimit=0;options.seed=1;options.variedWingmen=true;
        if(authority)options.observerBattleDepth=1400;
        auto begin=Clock::now();Game game(assets.root(),load,0,0,options,presentation?EffectLoader(effect):EffectLoader{},authority?GameRole::Authority:GameRole::Offline);
        auto constructed=Clock::now();arrangeScaleBattle(game,spread,idle,observerDistance,authority);auto arranged=Clock::now();
        NativeTest::require(game.core()->GetShips()->n()==2*ships+1,"requested AI teams were silently capped");
        fprintf(stdout,"AI scale: %d per team, %d initial ships; %s %s; construction %.3f ms, arrangement %.3f ms\n",ships,game.core()->GetShips()->n(),spread?"spread":"dense",idle?"idle":"combat",milliseconds(begin,constructed),milliseconds(constructed,arranged));fflush(stdout);
        std::ofstream output;if(!csv.empty()){output.open(csv);output<<"tick,step_ms,presentation_ms,ships,models,contacts,draw_items,particles,fingerprint,camera_ms,scene_ms,particles_ms,audio_ms,spatial_builds,publication_ms,work_ms,publication_bytes\n";}
        std::vector<double> steps,views;steps.reserve(ticks);views.reserve(ticks);size_t peakModels=0,peakDraws=0,peakParticles=0;
        std::vector<double> work;std::vector<std::byte> baseline;uint64_t sequence=0;size_t wireBytes=0;Net::Frame::Effects acknowledged{};
        FlightInput input;input.active=observer;
        for(int tick=0;tick<ticks;++tick) {
            if(observer)maintainScaleObserver(game);
            auto a=Clock::now();if(authority)game.authorityStep({},1./Net::tickRate);else game.step(input,1./165);auto b=Clock::now();size_t draws=0,particles=0;
            double publish=0;size_t bytes=0;
            if(publication && (tick+1)%Net::snapshotInterval==0) {
                auto start=Clock::now();
                {
                    game.beginNetworkPublication();
                    auto frame=game.networkFrame(game.playerShip()->GetObjectID(),tick+1,0,false,{},true,acknowledged);frame.header.sequence=sequence+1;
                    // This local receiver acknowledges each complete publication,
                    // as the live server's test client does.
                    if(!frame.notices.empty())acknowledged.notices=frame.notices.back().number;
                    if(!frame.events.empty())acknowledged.events=frame.events.back().number;
                    auto full=Net::packet(frame);auto delta=Net::compressed(full,sequence,baseline);
                    bytes=delta.size();wireBytes+=bytes;baseline=std::move(full);++sequence;
                }
                // Destruction of temporary publication buffers is server work.
                publish=milliseconds(start,Clock::now());
            }
            auto cameraEnd=b,sceneEnd=b,particlesEnd=b,audioEnd=b;
            if(presentation){CameraInput camera;camera.viewportHeight=1440;game.camera(camera);cameraEnd=Clock::now();draws=game.scene(false).size();sceneEnd=Clock::now();particles=game.particles().size();particlesEnd=Clock::now();game.worldSounds(true);audioEnd=Clock::now();}
            auto c=Clock::now();double step=milliseconds(a,b),view=milliseconds(b,c);
            if(tick>=warmup){steps.push_back(step);views.push_back(view-publish);work.push_back(step+publish);}
            size_t models=0;for(auto* l=game.core()->GetClusters()->first();l;l=l->next())models+=l->data()->GetModels()->n();
            peakModels=std::max(peakModels,models);peakDraws=std::max(peakDraws,draws);peakParticles=std::max(peakParticles,particles);
            if(output){size_t builds=0;for(auto* l=game.core()->GetClusters()->first();l;l=l->next()){auto* cluster=static_cast<CclusterIGC*>(l->data());builds+=cluster->m_dodgeIndex.rebuilds()+cluster->m_shipDodgeIndex.rebuilds();}
                output<<tick<<','<<step<<','<<view-publish<<','<<game.core()->GetShips()->n()<<','<<models<<','<<game.contacts().size()<<','<<draws<<','<<particles<<','<<fingerprint(game)<<','<<milliseconds(b,cameraEnd)<<','<<milliseconds(cameraEnd,sceneEnd)<<','<<milliseconds(sceneEnd,particlesEnd)<<','<<milliseconds(particlesEnd,audioEnd)<<','<<builds<<','<<publish<<','<<step+publish<<','<<bytes<<'\n';}
            game.clearSounds();
            if(tick%50==49){fprintf(stdout,"tick %d/%d: %.3f ms step, %d ships, %zu models\n",tick+1,ticks,step,game.core()->GetShips()->n(),models);fflush(stdout);}
        }
        printf("step median/p95/p99 %.3f/%.3f/%.3f ms; presentation %.3f/%.3f/%.3f ms; peaks models/draw-items/particles %zu/%zu/%zu; fingerprint %llu\n",percentile(steps,.5),percentile(steps,.95),percentile(steps,.99),percentile(views,.5),percentile(views,.95),percentile(views,.99),peakModels,peakDraws,peakParticles,(unsigned long long)fingerprint(game));
        printf("authority work median/p95/p99/max %.3f/%.3f/%.3f/%.3f ms; over 60 Hz budget %zu/%zu; publication %.1f MiB\n",percentile(work,.5),percentile(work,.95),percentile(work,.99),*std::max_element(work.begin(),work.end()),size_t(std::count_if(work.begin(),work.end(),[](double ms){return ms>1000./Net::tickRate;})),work.size(),wireBytes/1048576.);
    }catch(const std::exception& e){fprintf(stderr,"scale: %s\n",e.what());return 1;}
}
