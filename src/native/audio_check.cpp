#include "sound_bank.hpp"
#include "audio_backend.hpp"
#include "audio_pcm.hpp"
#include "audio.hpp"
#include "game.hpp"
#include "../Igc/igc.h"
#include "../soundengine/soundutil.h"
#include <AL/al.h>
#include <set>
#include <stdexcept>
#include <cstdio>
#include <source_location>
using namespace Native;
using namespace SoundEngine;
namespace {
void require(bool b,const char* message){if(!b)throw std::runtime_error(message);}
void check(HRESULT h,std::source_location where=std::source_location::current()){if(FAILED(h))throw std::runtime_error("Original sound factory rejected parameters at line "+std::to_string(where.line())+": "+std::to_string(h));}
class CatalogPCM final:public ISoundPCMData {
    unsigned bytes_,channels_;
public:
    explicit CatalogPCM(unsigned bytes=48000*2*600,unsigned channels=1):bytes_(bytes),channels_(channels){}
    unsigned GetNumberOfChannels()override{return channels_;}
    unsigned GetBitsPerSample()override{return 16;}
    unsigned GetSampleRate()override{return 48000;}
    unsigned GetSize()override{return bytes_;}
    void GetData(void* p,unsigned,unsigned bytes)override{memset(p,0,bytes);}
};
class RecordingSource final:public ISoundBufferSource {
public:
    struct Instance final:public ISoundInstance,public ISoundTweakable3D {
        TRef<ISoundPCMData> pcm;TRef<EventSourceImpl> finish=new EventSourceImpl;
        bool playing{true},loop{},spatial{};unsigned start{},length{};
        float gain{},pitch{1},priority{},distance{},inner{},outer{},outside{};
        HRESULT Stop(bool=false)override{playing=false;return S_OK;}
        HRESULT IsPlaying()override{return playing?S_OK:S_FALSE;}
        IEventSource* GetFinishEventSource()override{return finish;}
        TRef<ISoundTweakable> GetISoundTweakable()override{return this;}
        TRef<ISoundTweakable3D> GetISoundTweakable3D()override{return this;}
        HRESULT SetGain(float f)override{gain=f;return S_OK;}
        HRESULT SetPitch(float f)override{pitch=f;return S_OK;}
        HRESULT SetPriority(float f)override{priority=f;return S_OK;}
        HRESULT Set3D(bool b)override{spatial=b;return S_OK;}
        HRESULT SetMinimumDistance(float f)override{distance=f;return S_OK;}
        HRESULT SetCone(float a,float b,float c)override{inner=a;outer=b;outside=c;return S_OK;}
    };
    TRef<IntegerEventSourceImpl> events=new IntegerEventSourceImpl;
    std::vector<TRef<Instance>> instances;
    HRESULT CreateStaticBuffer(TRef<ISoundInstance>& out,ISoundPCMData* pcm,bool loop,ISoundPositionSource*)override{
        TRef<Instance> i=new Instance;i->pcm=pcm;i->loop=loop;out=i;instances.push_back(i);return S_OK;
    }
    HRESULT CreateASRBuffer(TRef<ISoundInstance>& out,ISoundPCMData* pcm,unsigned start,unsigned length,ISoundPositionSource* p)override{
        check(CreateStaticBuffer(out,pcm,true,p));instances.back()->start=start;instances.back()->length=length;return S_OK;
    }
    IIntegerEventSource* GetUpdateEventSource()override{return events;}
    void clear(){events->Trigger(0);instances.clear();}
};
TRef<ISoundPCMData> tone(double duration=1,int channels=1,float amplitude=.1f) {
    DecodedAudio audio{{},channels,48000};audio.samples.resize(size_t(duration*48000)*channels);
    for(size_t i=0;i<audio.samples.size()/channels;++i)for(int c=0;c<channels;++c)
        audio.samples[i*channels+c]=int16_t(amplitude*32767*std::sin(double(i)*2*3.141592653589793*(c?880:440)/48000));
    return new AudioPCM(std::move(audio));
}
TRef<ISoundTemplate> wave(ISoundPCMData* pcm){TRef<ISoundTemplate> t;check(CreateWaveFileSoundTemplate(t,pcm));return t;}
TRef<ISoundInstance> play(ISoundTemplate* t,ISoundBufferSource* backend,ISoundPositionSource* p=nullptr){TRef<ISoundPositionSource> position=p?p:new GenericSoundSource;TRef<ISoundInstance> i;check(t->CreateSound(i,backend,position));require(i!=nullptr,"Sound factory returned no instance");return i;}
void catalog(Assets& assets,bool decode) {
    std::set<std::string> files;TRef<ISoundPCMData> pcm=new CatalogPCM;
    std::unordered_map<std::string,TRef<ISoundPCMData>> decoded;
    SoundBank bank(assets,[&](const auto& path){
        files.insert(path.string());if(!decode)return pcm;
        auto& data=decoded[path.string()];if(!data){auto audio=decodeAudio(path);data=new CatalogPCM(unsigned(audio.samples.size()*2),audio.channels);}
        return data;
    });
    TRef<RecordingSource> recorder=new RecordingSource;unsigned count=0,empty=0,failed=0,absent=0;
    const std::set<int> missingArtwork={634,635,636,637,638,639,647,648,649,650,651,652,653,654,655,656,657,658,659,661};
    for(int id:bank.ids()) {
        try{auto t=bank.sound(id);if(!t){++empty;continue;}auto i=play(t,recorder);i->Stop(true);bank.resetSpeech();recorder->clear();++count;}
        catch(const std::exception& e){if(missingArtwork.contains(id) && std::string(e.what()).starts_with("Asset not found: faoh"))++absent;
            else {fprintf(stderr,"Catalog sound %d: %s\n",id,e.what());++failed;}}
    }
    printf("Sound catalog: %u templates, %zu wave assets, %u unused IDs, %u definitions reference missing installed artwork, %u failures\n",count,files.size(),empty,absent,failed);
    require(count>500 && files.size()>500,"Sound catalog did not cover installed definitions");
    require(failed==0,"Installed sound definitions failed validation");
}
void composition() {
    TRef<RecordingSource> b=new RecordingSource;auto pcm=tone();auto base=wave(pcm);
    TRef<ISoundTemplate> gain,pitch,spatial,cone,pair;
    check(CreateGainSoundTemplate(gain,base,-6));check(CreatePitchSoundTemplate(pitch,gain,1.5f));
    check(Create3DSoundTemplate(spatial,pitch,75));check(CreateSoundConeTemplate(cone,spatial,30,90,-20));
    check(CreatePairedSoundTemplate(pair,cone,base));auto i=play(pair,b);
    require(b->instances.size()==2,"Paired sound lost a branch");
    auto leaf=b->instances[0];require(leaf->gain==-6 && leaf->pitch==1.5f && leaf->spatial && leaf->distance==75 && leaf->inner==30 && leaf->outer==90 && leaf->outside==-20,"Template nesting lost authored properties");
    i->GetISoundTweakable()->SetGain(-4);i->GetISoundTweakable()->SetPitch(2);
    require(leaf->gain==-10 && leaf->pitch==3 && b->instances[1]->gain==-4 && b->instances[1]->pitch==2,"Runtime changes did not compose with template gain/pitch");
    i->Stop(true);b->clear();
    // Check every sample against the sum of three offset shots. This catches
    // the original empty 16-bit initialization loop (and its uninitialized PCM).
    DecodedAudio constant{std::vector<int16_t>(14400,1000),1,48000};
    TRef<ISoundTemplate> repeating;check(CreateRepeatingFireSoundTemplate(repeating,wave(new AudioPCM(std::move(constant))),.05f));
    auto repeated=play(repeating,b);auto mixed=b->instances.back()->pcm;
    std::vector<int16_t> output(mixed->GetSize()/2);mixed->GetData(output.data(),0,mixed->GetSize());
    for(size_t sample=0;sample<output.size();++sample) {
        int expected=0;for(int shot=0;shot<3;++shot)if(sample>=size_t(shot*2400) && sample<size_t(shot*2400+14400))expected+=1000;
        require(output[sample]==expected,"Repeated fire PCM does not equal the authored overlapping shots");
    }
    repeated->Stop(true);b->clear();
    TRef<IRandomSoundTemplate> random;check(CreateRandomSoundTemplate(random));
    std::array<TRef<ISoundPCMData>,3> options{tone(),tone(),tone()};
    for(int n=0;n<3;++n)check(random->AddSoundTemplate(wave(options[n]),float(n+1)));
    ISoundPCMData* last=nullptr;std::set<ISoundPCMData*> selected;
    srand(912);int expected=rand();srand(912);
    for(int n=0;n<100;++n){auto r=play(random,b);auto p=b->instances.back()->pcm;require(p!=last,"Original random no-repeat rule lost");last=p;selected.insert(p);r->Stop(true);b->clear();}
    require(selected.size()==3,"Random sound omitted a weighted branch");require(rand()==expected,"Audio perturbed the simulation RNG");
    TRef<ISoundTemplate> intermittent;check(CreateIntermittentSoundTemplate(intermittent,base,.01f,false));
    auto r=play(intermittent,b);b->events->Trigger(10000);require(!b->instances.empty(),"Intermittent sound did not receive updates");r->Stop(true);b->clear();
}
void categories(Assets& assets) {
    TRef<RecordingSource> recorder=new RecordingSource;unsigned decoded=0;
    SoundBank bank(assets,[&](const auto&){++decoded;return TRef<ISoundPCMData>(new CatalogPCM);});
    auto effect=bank.sound("machineGun1ShotSound"),voice=bank.sound("tm_slide_1_01Sound");
    bank.categoryGains(-8,-13);TRef<ISoundTemplate> pair;check(CreatePairedSoundTemplate(pair,effect,voice));
    auto active=play(pair,recorder);require(recorder->instances.size()==2,"category fixture paired branches");
    auto gun=recorder->instances[0],speech=recorder->instances[1];
    require(gun->gain==-8 && speech->gain==-13,"original effects and voice defaults must reach their own leaves");
    active->GetISoundTweakable()->SetGain(-4);
    require(gun->gain==-12 && speech->gain==-17,"runtime attenuation must compose with both category gains");
    unsigned prepared=decoded;bank.categoryGains(-20,-30);
    require(gun->gain==-24 && speech->gain==-34 && decoded==prepared,"live category edits must update each active branch without decoding or losing runtime gain");
    auto future=play(voice,recorder);require(recorder->instances.back()->gain==-30,"new narration must use the edited voice gain");
    active->Stop(true);active=nullptr;future->Stop(true);future=nullptr;recorder->clear();
    bank.categoryGains(-6,-9);auto first=play(bank.sound("salNoAmmoSound"),recorder);
    auto queued=play(bank.sound("salNoFuelSound"),recorder);require(recorder->instances.size()==1,"original speech mutex must defer second cue");
    bank.categoryGains(-10,-25);first->Stop(true);recorder->instances.front()->finish->Trigger();
    require(recorder->instances.size()==2 && recorder->instances.back()->gain==-25,"queued original speech must pick up gain changes before its deferred start");
    first->Stop(true);queued->Stop(true);bank.resetSpeech();recorder->clear();
    puts("Original sound categories: paired leaves, composed runtime gain, live edits, future and queued speech, and unchanged decoding passed");
}
std::vector<float> capture(OpenALBufferSource& b,unsigned frames){std::vector<float> data(frames*2);b.render(data);return data;}
double rms(const std::vector<float>& v){double sum=0;for(float f:v)sum+=f*f;return std::sqrt(sum/v.size());}
void loopback() {
    TRef<OpenALBufferSource> b=new OpenALBufferSource(true);b->listener({0,0,0},{0,0,0},{0,0,-1},{0,1,0});
    auto pcm=tone(.3);auto base=wave(pcm);TRef<ISoundTemplate> asr;check(CreateASRSoundTemplate(asr,base,.05f,.1f));
    auto i=play(asr,b);b->update(0);require(rms(capture(*b,48000))>.01,"ASR sustain silent");b->update(1000);require(i->IsPlaying()==S_OK,"ASR sustain ended before stop");
    i->Stop(false);require(rms(capture(*b,4800))>.001,"Soft stop cut the release tail");capture(*b,24000);b->update(500);require(i->IsPlaying()==S_FALSE && b->active()==0,"ASR release never finished");
    TRef<ISoundTemplate> repeating;check(CreateRepeatingFireSoundTemplate(repeating,base,.05f));i=play(repeating,b);b->update(0);require(rms(capture(*b,48000))>.01,"Repeating weapon sound silent");i->Stop(false);capture(*b,48000);b->update(1000);require(i->IsPlaying()==S_FALSE,"Repeating weapon release did not finish");
    TRef<ISoundMutex> mutex;check(CreateSoundMutex(mutex));TRef<ISoundTemplate> serialA,serialB;
    auto shortWave=wave(tone(.05));check(CreateSerializedSoundTemplate(serialA,shortWave,mutex,10,0,true));check(CreateSerializedSoundTemplate(serialB,wave(tone(.1)),mutex,10,0,true));
    auto a=play(serialA,b),duplicate=play(serialA,b),queued=play(serialB,b);
    require(a==duplicate && b->active()==1 && queued->IsPlaying()==S_OK,"Serialized deduplication/queue lost");
    b->update(0);capture(*b,9600);b->update(200);require(a->IsPlaying()==S_FALSE && queued->IsPlaying()==S_OK && b->active()==1,"Queued speech did not start after predecessor");
    b->update(0);require(rms(capture(*b,4800))>.005,"Queued speech silent");capture(*b,4800);b->update(200);require(queued->IsPlaying()==S_FALSE,"Queued speech did not finish");mutex->Reset();
    a=play(serialA,b);queued=play(serialB,b);queued->Stop(true);
    require(queued->IsPlaying()==S_FALSE,"Cancelled speech remained pending");
    b->update(0);capture(*b,9600);b->update(200);require(b->active()==0,"Cancelled speech started after its predecessor");
    a=play(serialA,b);queued=play(serialB,b);mutex->Reset();
    require(queued->IsPlaying()==S_FALSE,"Reset left pending speech alive");b->stopAll();b->update(0);
    TRef<ISoundTemplate> spatial,cone;check(Create3DSoundTemplate(spatial,wave(tone(1,2)),1));check(CreateSoundConeTemplate(cone,spatial,30,60,-20));
    TRef<GenericSoundSource> emitter=new GenericSoundSource(Vector(0,0,-1),Vector::GetZero(),Vector(0,0,1),false);
    i=play(cone,b,emitter);b->update(0);capture(*b,2400);double front=rms(capture(*b,4800));emitter->SetOrientation(Vector(0,0,-1));b->update(0);capture(*b,2400);double back=rms(capture(*b,4800));
    require(front>.01 && back<front*.15 && back>front*.05,"OpenAL spatial cone or stereo-to-mono conversion wrong");i->Stop(true);b->update(0);
    // Exhaustion and repeated shutdown must leave no dangling finish callbacks.
    std::vector<TRef<ISoundInstance>> burst;for(int n=0;n<4096;++n)burst.push_back(play(base,b));b->update(0);require(b->active()==4096 && b->physical()==128,"Virtual sound pool lost voices or exceeded its physical limit");b->stopAll();b->update(0);require(b->active()==0 && alGetError()==AL_NO_ERROR,"OpenAL teardown failed");
    printf("OpenAL capture: ASR release, repeated fire, serialized speech, cone attenuation and source reuse passed\n");
}
void facade(Assets& assets) {
    Audio audio(assets,true);
    const int ids[]={120,myHullHitSound,salNoAmmoSound,missileToneSound,mouseoverSound,mouseclickSound,positiveButtonClickSound,negativeButtonClickSound,changeLoadoutSound};audio.preload(ids);
    auto uiDecoded=audio.decodedWaveCount();
    for(auto id:{mouseoverSound,mouseclickSound,positiveButtonClickSound,negativeButtonClickSound,changeLoadoutSound})require(audio.play(id,{},{},true)!=0,"original UI cue fails OpenAL playback");
    require(audio.decodedWaveCount()==uiDecoded,"preloaded UI cue decodes a wave on interaction");audio.stopAll();
    std::array<SoundLoop,1> loops{{{120,1,1,{0,0,-10},{},false,123,{0,0,1}}}};
    audio.synchronize(loops);audio.tick(.01);loops[0].position.x=10;audio.synchronize(loops);
    auto emitter=std::make_shared<SoundEmitter>();
    SoundEvent impact{myHullHitSound,{},{},false,{0,0,-1},emitter,{1,0,0}};
    require(audio.play(impact)!=0,"Attached hit sound failed to start");audio.tick(.01);
    emitter->position={10,10,10};emitter->relative=true;audio.tick(.01);
    emitter->active=false;audio.tick(.01);audio.synchronize({});
    audio.play(salNoAmmoSound);audio.play(salNoAmmoSound);audio.stopAll();audio.tick(.01);
    auto cue=audio.startCue(salNoAmmoSound);require(cue && cue->IsPlaying()==S_OK,"narration cue failed to start");
    audio.tick(.01);audio.paused(true);
    for(int i=0;i<100;++i)audio.tick(.1);
    require(cue->IsPlaying()==S_OK,"paused narration consumed its playback time");
    audio.paused(false);for(int i=0;i<100;++i)audio.tick(.1);
    require(cue->IsPlaying()==S_FALSE,"resumed narration did not finish");
    audio.stopAll();QuickChatCatalog catalog(assets);audio.preload(catalog);size_t decoded=audio.decodedWaveCount();
    for(auto& entry:catalog.commands){require(audio.play(entry.sound)!=0,"preloaded quick-chat voice/notification must start");audio.stopAll();require(catalog.voiceForChat(entry.sonic) && catalog.voiceForChat(entry.sonic)->sound==entry.sound,"audio preparation loses authored sonic identity");}
    for(auto& chat:catalog.sonicChats)for(auto& voice:chat.voices){require(audio.play(voice.sound)!=0,"all authored voice variants and non-menu entries must be preloaded");audio.stopAll();}
    require(audio.decodedWaveCount()==decoded,"quick-chat command decoded a wave during playback");
    require(alGetError()==AL_NO_ERROR,"Native audio facade left an AL error");
}
void fleetVoices(Assets& assets) {
    Audio audio(assets,true);const int ids[]={120,salNoAmmoSound};audio.preload(ids);auto decoded=audio.decodedWaveCount();
    std::vector<SoundLoop> loops;
    for(int i=0;i<2049;++i)loops.push_back({120,1,1,{float(i),0,-1000},{},false,uint64_t(i+1),{0,0,1}});
    audio.synchronize(loops);require(audio.logicalVoiceCount()==loops.size(),"large fleets silently lose logical ship voices");
    audio.tick(.01);require(audio.physicalVoiceCount()<=128,"fleet sound exceeds the physical mixer budget");
    audio.synchronize(loops);require(audio.logicalVoiceCount()==loops.size(),"unchanged fleet sound reallocates logical voices");
    auto cue=audio.startCue(salNoAmmoSound);require(cue && cue->IsPlaying()==S_OK,"large fleet starves player narration");
    for(int i=0;i<600;++i)require(audio.play(120)!=0,"large fleet starves one-shot effects");
    require(audio.logicalVoiceCount()==loops.size()+512,"one-shot budget leaks or steals fleet loops");
    auto stale=audio.play(120,{},{},true,true);audio.stopAll();require(audio.logicalVoiceCount()==0,"large fleet teardown retains logical voices");
    auto fresh=audio.play(120,{},{},true,true);require(fresh && fresh!=stale,"recycled sound identity aliases an old handle");
    audio.stop(stale);audio.tick(.01);require(audio.logicalVoiceCount()==1,"stale sound handle stops a recycled voice");
    require(audio.decodedWaveCount()==decoded,"fleet sound decoded additional copies of shared wave data");audio.stopAll();
    puts("2,049 logical fleet loops, 512 bounded one-shots, player narration, physical mixer budget and stale handles passed");
}
void ambientLifecycle(Assets& assets) {
    SessionOptions options;options.scenario=Scenario::FreeFlight;options.enemies=0;options.startInHangar=true;
    Game game(assets.root(),[&](const auto& name,const auto&){auto m=assets.model(name);return ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options);
    Audio audio(assets,true);audio.preload(game.soundIds());size_t decoded=audio.decodedWaveCount();
    auto* ship=game.playerShip();auto* station=ship->GetStation();auto loops=game.worldSounds(true);
    require(loops.size()==1 && loops[0].id==station->GetInteriorSound() && loops[0].relative,"docked hangar omits its original interior ambience");
    audio.synchronize(loops);audio.tick(.01);game.launch();for(int i=0;i<32;++i)game.step({},.1);
    std::vector<TRef<ImissileIGC>> missiles;
    for(float lock:{.25f,.9f}) {
        DataMissileIGC data{};data.pLauncher=ship;data.pTarget=ship;data.pmissiletype=static_cast<ImagazineIGC*>(ship->GetMountedPart(ET_Magazine,0))->GetMissileType();data.pCluster=ship->GetCluster();data.lock=lock;
        data.position=ship->GetPosition()+Vector(0,0,200);data.forward=Vector(0,0,-1);data.missileID=game.core()->GenerateNewMissileID();
        auto* missile=static_cast<ImissileIGC*>(game.core()->CreateObject(game.core()->GetLastUpdate(),OT_missile,&data,sizeof(data)));require(missile,"incoming missile fixture");
        missiles.emplace_back(missile);missile->Release();
    }
    loops=game.worldSounds(true);int warnings=0;
    for(auto& loop:loops)if(loop.id==missileLockSound){++warnings;require(loop.relative && std::abs(loop.pitch-1.2f)<1e-6f,"incoming warning pitch must follow the strongest missile lock");}
    require(warnings==1,"multiple incoming missiles must share one warning loop");audio.synchronize(loops);audio.tick(.01);
    for(auto& missile:missiles)missile->Terminate();missiles.clear();loops=game.worldSounds(false);
    require(std::none_of(loops.begin(),loops.end(),[](auto& loop){return loop.id==missileLockSound;}),"incoming warning persists after its missiles are gone");
    require(game.core()->GetIgcSite()->DockWithStationEvent(ship,station),"return to station for audio lifecycle");game.step({},.01);loops=game.worldSounds(true);
    require(loops.size()==1 && loops[0].id==station->GetInteriorSound() && loops[0].relative,"return to hangar must replace world loops with station ambience");
    audio.synchronize(loops);audio.tick(.01);require(audio.decodedWaveCount()==decoded,"hangar/incoming warning decoded waves during play");
    puts("Hangar/launch/dock ambience and actual incoming-missile warning selection, pitch, stop and preloading passed");
}
void stationEvents(Assets& assets) {
    SessionOptions options;options.scenario=Scenario::FreeFlight;options.map=3;options.enemies=0;options.startInHangar=true;
    Game game(assets.root(),[&](const auto& name,const auto&){auto m=assets.model(name);return ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options);
    auto* core=game.core();auto* player=game.playerShip();auto* own=player->GetStation();IstationIGC* enemy=nullptr;
    for(auto* l=core->GetStations()->first();l;l=l->next())if(l->data()->GetSide()!=player->GetSide()){enemy=l->data();break;}
    require(enemy,"station event fixture has opposing station");int initialStations=core->GetStations()->n();int id=game.addLocalPilot("Capture fixture",enemy->GetSide()->GetObjectID());
    require(id>=0,"station event fixture has opposing human pilot");auto* other=core->GetShip(id);game.launch();
    Audio audio(assets,true);audio.preload(game.soundIds());size_t decoded=audio.decodedWaveCount();
    auto check=[&](int expected) {
        int count=0;for(auto& sound:game.sounds()){if(sound.id==expected){require(sound.relative,"station announcement must be nonspatial");++count;}audio.play(sound);}
        require(count==1,"station callback must play original announcement exactly once");
        require(audio.decodedWaveCount()==decoded,"station event decoded a wave during gameplay");audio.stopAll();
    };
    game.clearSounds();core->GetIgcSite()->CaptureStationEvent(other,own);check(own->GetStationType()->GetCapturedSound());
    game.clearSounds();core->GetIgcSite()->CaptureStationEvent(player,enemy);check(enemy->GetStationType()->GetEnemyCapturedSound());
    TRef<IstationIGC> retainedEnemy=enemy,retainedOwn=own;
    auto lostSound=enemy->GetStationType()->GetDestroyedSound(),destroyedSound=own->GetStationType()->GetEnemyDestroyedSound();
    game.clearSounds();core->GetIgcSite()->KillStationEvent(enemy,other,1);check(lostSound);
    core->GetIgcSite()->KillStationEvent(enemy,other,1);
    require(std::count_if(game.sounds().begin(),game.sounds().end(),[&](auto& sound){return sound.id==lostSound;})==1,"duplicate station death repeats its announcement");
    game.clearSounds();core->GetIgcSite()->KillStationEvent(own,player,1);check(destroyedSound);
    game.step({},.01);
    require(core->GetStations()->n()==initialStations-2,"station announcement fixture must complete actual station destruction");
    puts("Original lost/captured and friendly/enemy destroyed station announcements: actual callbacks, nonspatial playback, duplicate suppression and preload passed");
}
void shipDiscovery(Assets& assets) {
    SessionOptions options;options.scenario=Scenario::FreeFlight;options.map=3;options.enemies=0;options.startInHangar=true;
    Game game(assets.root(),[&](const auto& name,const auto&){auto m=assets.model(name);return ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options);
    int id=game.addLocalPilot("Contact fixture",1);require(id>=0,"enemy contact fixture");auto* enemy=game.core()->GetShip(id);auto* ship=game.playerShip();game.launch();
    enemy->SetStation(nullptr);enemy->SetCluster(ship->GetCluster());enemy->SetPosition(ship->GetPosition()+Vector(0,0,-100000));
    auto update=[&]{enemy->GetThingSite()->UpdateSideVisibility(enemy,enemy->GetCluster());};update();
    require(!enemy->SeenBySide(ship->GetSide()),"enemy contact begins outside scanner range");
    Audio audio(assets,true);audio.preload(game.soundIds());size_t decoded=audio.decodedWaveCount();game.clearSounds();
    enemy->SetPosition(ship->GetPosition()+ship->GetOrientation().GetForward()*1000);update();
    require(enemy->SeenBySide(ship->GetSide()),"enemy acquired by actual ship scanner");
    require(std::count_if(game.sounds().begin(),game.sounds().end(),[](auto& event){return event.id==newShipSound;})==1,"first acquired enemy omits original contact sound");
    for(auto& sound:game.sounds())audio.play(sound);game.clearSounds();update();update();
    require(game.sounds().empty(),"retained scanner contact repeats acquisition sound every update");
    enemy->SetPosition(ship->GetPosition()+Vector(0,0,-100000));update();enemy->SetPosition(ship->GetPosition()+ship->GetOrientation().GetForward()*1000);update();
    require(std::count_if(game.sounds().begin(),game.sounds().end(),[](auto& event){return event.id==newShipSound;})==1,"lost and reacquired enemy does not announce its new contact");
    for(auto& sound:game.sounds())audio.play(sound);require(audio.decodedWaveCount()==decoded,"contact acquisition decoded a wave during play");
    puts("Actual scanner acquisition, retention and reacquisition sounds passed without extra scans or runtime waveform decoding");
}
void combatPreload(Assets& assets) {
    Game game(assets.root(),[&](const auto& name,const auto&){auto m=assets.model(name);return ModelHandle{1,m->radius,m->frames,m->lights};},0,0);
    Audio audio(assets,true);audio.preload(game.soundIds());size_t decoded=audio.decodedWaveCount();
    bool shieldsOffline=false,hullHit=false;
    for(int frame=0;frame<3000;++frame) {
        game.step({},1./165);const auto& state=game.state();
        audio.listener(state.position,state.velocity,state.forward,state.up);audio.synchronize(game.worldSounds(true));
        require(audio.decodedWaveCount()==decoded,"world loop decoded a wave after preloading");
        for(auto& sound:game.sounds()) {
            shieldsOffline|=sound.id==salShieldsOfflineSound;hullHit|=sound.id==myHullHitSound;audio.play(sound);
            if(audio.decodedWaveCount()!=decoded)fprintf(stderr,"Unexpected decode at frame %d, sound %d\n",frame,sound.id);
            require(audio.decodedWaveCount()==decoded,"combat event decoded a wave after preloading");
        }
        game.clearSounds();audio.tick(1./165);
        require(audio.decodedWaveCount()==decoded,"combat decoded a wave after preloading; this can miss a display deadline");
    }
    require(shieldsOffline && hullHit,"preload test must reach actual IGC shield loss and hull damage");
    printf("Actual IGC shield loss/hull combat: %zu waves preloaded, no runtime decoding.\n",decoded);
}

}
int main(int argc,char** argv){try{Assets assets(argc>1?argv[1]:Assets::steamArtwork());catalog(assets,argc>2);composition();categories(assets);loopback();facade(assets);fleetVoices(assets);ambientLifecycle(assets);stationEvents(assets);shipDiscovery(assets);combatPreload(assets);return 0;}catch(const std::exception& e){fprintf(stderr,"Audio check: %s\n",e.what());return 1;}}
