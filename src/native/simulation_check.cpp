#include "game.hpp"
#include "test_fixture.hpp"
#include <bit>
using NativeTest::require;
namespace {
void randomReference() {
    unsigned cases=0;
    for(uint32_t seed:{0u,1u,912u,0x7fffffffu,0xffffffffu}) {
        uint32_t state=seed;uint64_t expected=seed;Allegiance::SimulationRandomScope scope(state);
        auto next=[&]{expected=(expected*214013+2531011)&0xffffffffULL;return int((expected>>16)&32767);};
        for(int i=0;i<10000;++i) {
            require(randomRaw()==next(),"Windows CRT raw simulation sequence differs");
            float reference=float(next())/32767.f*(13.f*(1-FLT_EPSILON)+2.f)-2.f;
            require(random(-2,13)==reference,"Windows CRT float scaling/draw order differs");
            require(randomInt(-7,37)==-7+next()%45,"Windows CRT integer scaling/draw order differs");
            ++cases;
        }
        uint32_t nested=1,saved=state;
        {Allegiance::SimulationRandomScope nestedScope(nested);require(randomRaw()==41 && randomRaw()==18467,"nested mission sequence");}
        require(state==saved && randomRaw()==next(),"nested RNG scope corrupted/restored the wrong mission");
    }
    printf("%u Windows CRT raw/float/integer triples and nested mission scopes passed.\n",cases);
}
uint64_t fingerprint(Native::Game& game) {
    uint64_t hash=14695981039346656037ULL;
    auto word=[&](uint32_t value){hash^=value;hash*=1099511628211ULL;};
    auto number=[&](float value){word(std::bit_cast<uint32_t>(value));};
    auto vector=[&](const Vector& v){number(v.x);number(v.y);number(v.z);};
    word(game.simulationRandomState());word(game.state().money);
    for(auto* sector=game.core()->GetClusters()->first();sector;sector=sector->next()) {
        word(sector->data()->GetObjectID());word(sector->data()->GetStarSeed());
        for(auto* model=sector->data()->GetModels()->first();model;model=model->next()) {
            auto* m=model->data();word(m->GetObjectType());word(m->GetObjectID());
            word(m->GetSide()?m->GetSide()->GetObjectID():NA);vector(m->GetPosition());vector(m->GetVelocity());
            vector(m->GetOrientation().GetForward());vector(m->GetOrientation().GetUp());
            if(m->GetObjectType()==OT_ship) {
                auto* s=static_cast<IshipIGC*>(m);number(s->GetFraction());number(s->GetFuel());number(s->GetEnergy());word(s->GetAmmo());
                word(s->GetCommandID(c_cmdAccepted));word(s->GetCommandID(c_cmdPlan));
            }
        }
    }
    return hash;
}
void missions(const std::filesystem::path& art) {
    Native::Assets assets(art);auto load=[&](const std::string& name,const std::string&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};};
    Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;options.startInHangar=true;options.seed=8317;
    auto effect=[](const std::string&){return Native::EffectTexture{1,64,64,1};};
    Native::Game a(art,load,0,0,options,effect),b(art,load,0,0,options,effect);
    require(fingerprint(a)==fingerprint(b),"same seed produces different authored maps/initial mission state");
    Native::FlightInput idle;idle.active=false;
    for(int tick=0;tick<600;++tick) {
        a.step(idle,.02);a.camera({});a.scene(true);a.particles();a.worldSounds(false);
        uint32_t unrelated=uint32_t(tick+1);
        {Allegiance::SimulationRandomScope other(unrelated);for(int j=0;j<17;++j)randomRaw();}
        b.step(idle,.02);
        require(fingerprint(a)==fingerprint(b),"rendering/audio/interleaved mission changed seeded IGC outcomes");
    }
    a.launch();b.launch();require(fingerprint(a)==fingerprint(b),"event-driven launch consumed the wrong mission stream");
    require(a.dropCargo()==b.dropCargo() && fingerprint(a)==fingerprint(b),"cargo scatter consumed the wrong mission stream");
    options.seed=8318;Native::Game c(art,load,0,0,options);
    require(c.core()->GetClusters()->first()->data()->GetStarSeed()!=a.core()->GetClusters()->first()->data()->GetStarSeed(),"different seed did not change authored map sequence");
    puts("Two independent real IGC missions: 600 interleaved state fingerprints, presentation isolation, launch/cargo events and changed seed passed.");
}
}
int main(int argc,char** argv) {
    try{randomReference();missions(argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork());return 0;}
    catch(const std::exception& e){fprintf(stderr,"simulation: %s\n",e.what());return 1;}
}
