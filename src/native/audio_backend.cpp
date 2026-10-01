#include "audio_backend.hpp"
#include "../soundengine/soundtemplates.h"
#include "../soundengine/soundutil.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <array>
#include <map>
#include <tuple>
#include <stdexcept>

namespace Native {
using namespace SoundEngine;
struct OpenALBufferSource::Impl {
    ALCdevice* device{};ALCcontext* context{};bool offline{},paused{};
    LPALCRENDERSAMPLESSOFT renderSamples{};
    TRef<IntegerEventSourceImpl> updates=new IntegerEventSourceImpl;
    struct Buffer {TRef<ISoundPCMData> data;ALuint id{};};
    using Key=std::tuple<ISoundPCMData*,unsigned,unsigned,bool>;
    std::map<Key,Buffer> buffers;
    struct Instance final : ISoundInstance,ISoundTweakable3D {
        Impl& backend;ALuint source;TRef<ISoundPCMData> data;TRef<ISoundPositionSource> positionSource;
        TRef<EventSourceImpl> finished=new EventSourceImpl;
        unsigned loopStart{},loopLength{};bool loop{},spatial{},pending{true},playing{true},notified{};
        double playhead{};float score{};bool selected{};
        float gain{},pitch{1},priority{},distance{100},inner{360},outer{360},outside{};
        Vector position,velocity,direction{0,0,-1};bool relative{true};
        Instance(Impl& owner,ALuint id,ISoundPCMData* pcm,bool repeat,unsigned start,unsigned length,ISoundPositionSource* emitter)
            :backend(owner),source(id),data(pcm),positionSource(emitter),loopStart(start),loopLength(length),loop(repeat){}
        HRESULT Stop(bool force=false)override {
            if(!playing)return S_OK;
            loop=false;
            if(force || pending){playing=false;if(source)alSourceStop(source);}
            else if(source)alSourcei(source,AL_LOOPING,AL_FALSE);
            return S_OK;
        }
        HRESULT IsPlaying()override{return playing?S_OK:S_FALSE;}
        IEventSource* GetFinishEventSource()override{return finished;}
        TRef<ISoundTweakable> GetISoundTweakable()override{return this;}
        TRef<ISoundTweakable3D> GetISoundTweakable3D()override{return this;}
        HRESULT SetGain(float value)override{if(!std::isfinite(value))return E_INVALIDARG;value=std::clamp(value,-100.f,0.f);if(value!=gain){gain=value;if(source && playing)alSourcef(source,AL_GAIN,std::pow(10.f,gain/20.f));}return S_OK;}
        HRESULT SetPitch(float value)override{if(!std::isfinite(value) || value<=0)return E_INVALIDARG;value=std::clamp(value,.01f,16.f);if(value!=pitch){pitch=value;if(source && playing)alSourcef(source,AL_PITCH,pitch);}return S_OK;}
        HRESULT SetPriority(float value)override{priority=value;return S_OK;}
        HRESULT Set3D(bool value)override{
            if(value!=spatial && source && playing)return E_NOTIMPL;
            spatial=value;return S_OK;
        }
        HRESULT SetMinimumDistance(float value)override{if(value<=0)return E_INVALIDARG;distance=value;if(source && playing)alSourcef(source,AL_REFERENCE_DISTANCE,value);return S_OK;}
        HRESULT SetCone(float a,float b,float c)override{inner=a;outer=b;outside=c;if(source && playing)cone();return S_OK;}
        void cone(){alSourcef(source,AL_CONE_INNER_ANGLE,inner);alSourcef(source,AL_CONE_OUTER_ANGLE,outer);alSourcef(source,AL_CONE_OUTER_GAIN,std::pow(10.f,outside/20.f));}
        void location(bool force=false) {
            Vector p=Vector::GetZero(),v=Vector::GetZero(),d(0,0,-1);bool r=true;
            if(spatial && positionSource) {
                positionSource->GetPosition(p);positionSource->GetVelocity(v);positionSource->GetOrientation(d);r=positionSource->IsListenerRelative()==S_OK;
            }
            if(force || position!=p){position=p;if(source)alSource3f(source,AL_POSITION,p.x,p.y,p.z);}
            if(force || velocity!=v){velocity=v;if(source)alSource3f(source,AL_VELOCITY,v.x,v.y,v.z);}
            if(force || direction!=d){direction=d;if(source)alSource3f(source,AL_DIRECTION,d.x,d.y,d.z);}
            if(force || relative!=r){relative=r;if(source)alSourcei(source,AL_SOURCE_RELATIVE,r);}
        }
        void releaseSource() {
            if(!source)return;
            ALfloat offset=0;alGetSourcef(source,AL_SEC_OFFSET,&offset);playhead=offset;
            alSourceStop(source);alSourcei(source,AL_BUFFER,0);source=0;
        }
        void attach(ALuint id) {
            source=id;
            alSourcei(source,AL_BUFFER,backend.buffer(data,loopStart,loopLength,spatial));alSourcei(source,AL_LOOPING,loop);
            alSourcef(source,AL_REFERENCE_DISTANCE,distance);alSourcef(source,AL_MAX_DISTANCE,distance*4000);
            alSourcef(source,AL_ROLLOFF_FACTOR,spatial?1:0);alSourcef(source,AL_GAIN,std::pow(10.f,gain/20.f));alSourcef(source,AL_PITCH,pitch);
            cone();location(true);alSourcef(source,AL_SEC_OFFSET,float(playhead));alSourcePlay(source);if(backend.paused)alSourcePause(source);
        }
        void update(unsigned ms) {
            selected=false;if(!playing)return;
            if(positionSource && positionSource->IsPlaying()!=S_OK){Stop(true);return;}
            if(source) {
                ALint status;alGetSourcei(source,AL_SOURCE_STATE,&status);
                if(status!=AL_PLAYING && status!=AL_PAUSED){playing=false;return;}
            } else if(!pending) {
                playhead+=ms*.001*pitch;
                double bytes=data->GetBytesPerSec(),duration=data->GetSize()/bytes;
                double start=loopStart/bytes,end=loopLength?(loopStart+loopLength)/bytes:duration;
                if(loop && playhead>=end)playhead=start+std::fmod(playhead-start,end-start);
                else if(!loop && playhead>=duration){playing=false;return;}
            }
            pending=false;location();score=gain+priority;
            if(spatial) {
                Vector toListener=(relative?Vector::GetZero():backend.listenerPosition)-position;
                float range=toListener.Length();score-=20*std::log10((std::max)(range,distance)/distance);
                if(range>0 && inner<360) {
                    float cosine=(toListener*direction)/(range*(std::max)(direction.Length(),.00001f));
                    float angle=2*DegreesFromRadians(std::acos(std::clamp(cosine,-1.f,1.f)));
                    float fraction=outer>inner?std::clamp((angle-inner)/(outer-inner),0.f,1.f):float(angle>inner);
                    float coneGain=1-fraction+fraction*std::pow(10.f,outside/20);
                    score+=20*std::log10((std::max)(coneGain,.00001f));
                }
            }
            // A small preference avoids churning sources at the audibility cutoff.
            if(source)score+=.5f;
        }

    };
    static constexpr size_t capacity=128;
    struct Slot {ALuint source{};TRef<Instance> instance;};
    std::array<Slot,capacity> slots;
    std::vector<TRef<Instance>> instances,retired,preparingInstances;bool preparing{};
    std::vector<Instance*> audible;Vector listenerPosition;
    explicit Impl(bool capture):offline(capture) {
        try {
            if(capture) {
                auto open=reinterpret_cast<LPALCLOOPBACKOPENDEVICESOFT>(alcGetProcAddress(nullptr,"alcLoopbackOpenDeviceSOFT"));
                renderSamples=reinterpret_cast<LPALCRENDERSAMPLESSOFT>(alcGetProcAddress(nullptr,"alcRenderSamplesSOFT"));
                if(!open || !renderSamples)throw std::runtime_error("OpenAL loopback unavailable");device=open(nullptr);
            } else device=alcOpenDevice(nullptr);
            if(!device)throw std::runtime_error("OpenAL cannot open audio device");
            ALCint config[]={ALC_FORMAT_CHANNELS_SOFT,ALC_STEREO_SOFT,ALC_FORMAT_TYPE_SOFT,ALC_FLOAT_SOFT,ALC_FREQUENCY,48000,0};
            context=alcCreateContext(device,capture?config:nullptr);if(!context || !alcMakeContextCurrent(context))throw std::runtime_error("OpenAL cannot create context");
            if(!alIsExtensionPresent("AL_SOFT_loop_points"))throw std::runtime_error("OpenAL loop points are required for Allegiance ASR sounds");
            alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);alDopplerFactor(1);alSpeedOfSound(343.3f);
            for(auto& slot:slots)alGenSources(1,&slot.source);
            if(alGetError()!=AL_NO_ERROR)throw std::runtime_error("OpenAL source pool allocation failed");
            instances.reserve(512);audible.reserve(512);retired.reserve(512);fprintf(stderr,"OpenAL: %s; original sound templates\n",alGetString(AL_RENDERER));
        }catch(...){cleanup();throw;}
    }
    void cleanup(){if(context){alcMakeContextCurrent(context);for(auto& slot:slots)if(slot.source)alDeleteSources(1,&slot.source);for(auto& [key,buffer]:buffers)alDeleteBuffers(1,&buffer.id);alcMakeContextCurrent(nullptr);alcDestroyContext(context);context=nullptr;}if(device){alcCloseDevice(device);device=nullptr;}}
    ~Impl(){cleanup();}
    ALuint buffer(ISoundPCMData* pcm,unsigned start,unsigned length,bool spatial) {
        Key key{pcm,start,length,spatial};auto found=buffers.find(key);if(found!=buffers.end())return found->second.id;
        unsigned channels=pcm->GetNumberOfChannels(),bits=pcm->GetBitsPerSample();
        if((channels!=1 && channels!=2) || bits!=16)throw std::runtime_error("Unsupported decoded PCM format");
        std::vector<int16_t> samples(pcm->GetSize()/2);pcm->GetData(samples.data(),0,pcm->GetSize());
        unsigned frameBytes=channels*2;unsigned first=start/frameBytes,last=(start+length)/frameBytes;
        if(spatial && channels==2){for(size_t i=0;i<samples.size()/2;++i)samples[i]=int16_t((int(samples[2*i])+samples[2*i+1])/2);samples.resize(samples.size()/2);channels=1;}
        ALuint id=0;alGenBuffers(1,&id);alBufferData(id,channels==1?AL_FORMAT_MONO16:AL_FORMAT_STEREO16,samples.data(),ALsizei(samples.size()*2),pcm->GetSampleRate());
        if(length){ALint points[]={ALint(first),ALint(last)};alBufferiv(id,AL_LOOP_POINTS_SOFT,points);}
        if(alGetError()!=AL_NO_ERROR){alDeleteBuffers(1,&id);throw std::runtime_error("OpenAL PCM/loop upload failed");}
        buffers.emplace(key,Buffer{pcm,id});return id;
    }
    HRESULT create(TRef<ISoundInstance>& out,ISoundPCMData* data,bool loop,unsigned start,unsigned length,ISoundPositionSource* source) {
        if(!data || !data->GetSize() || start>data->GetSize() || length>data->GetSize()-start)return E_INVALIDARG;
        if(preparing){TRef<Instance> i=new Instance(*this,0,data,loop,start,length,source);preparingInstances.push_back(i);out=i;return S_OK;}
        // Logical voices scale with the world. Only the physical OpenAL source
        // pool is bounded; audibility and authored priority select its owners.
        TRef<Instance> instance=new Instance(*this,0,data,loop,start,length,source);instances.push_back(instance);out=instance;return S_OK;
    }
    void update(unsigned ms) {
        updates->Trigger(int(ms));audible.clear();
        for(auto& instance:instances) {
            instance->update(ms);
            if(instance->playing && instance->score>-80)audible.push_back(instance);
        }
        size_t count=(std::min)(capacity,audible.size());
        std::partial_sort(audible.begin(),audible.begin()+count,audible.end(),[](Instance* a,Instance* b){return a->score>b->score;});
        for(size_t i=0;i<count;++i)audible[i]->selected=true;
        for(auto& slot:slots)if(slot.instance && !slot.instance->selected){slot.instance->releaseSource();slot.instance=nullptr;}
        size_t free=0;
        for(size_t i=0;i<count;++i)if(!audible[i]->source) {
            while(slots[free].instance)++free;
            auto& slot=slots[free];slot.instance=audible[i];slot.instance->attach(slot.source);
        }
        for(size_t i=0;i<instances.size();) {
            if(instances[i]->playing){++i;continue;}
            retired.push_back(instances[i]);instances[i]=instances.back();instances.pop_back();
        }
        // Event handlers may append the next serialized sound. Notify only after
        // retiring stopped instances and releasing their physical mixer sources.
        for(size_t i=0;i<retired.size();++i){auto sound=retired[i];if(!sound->notified){sound->notified=true;sound->finished->Trigger();}}
        retired.clear();
    }

};
OpenALBufferSource::OpenALBufferSource(bool offline):impl_(std::make_unique<Impl>(offline)){}
OpenALBufferSource::~OpenALBufferSource(){stopAll();impl_->update(0);}
HRESULT OpenALBufferSource::CreateStaticBuffer(TRef<ISoundInstance>& out,ISoundPCMData* data,bool loop,ISoundPositionSource* source){return impl_->create(out,data,loop,0,0,source);}
HRESULT OpenALBufferSource::CreateASRBuffer(TRef<ISoundInstance>& out,ISoundPCMData* data,unsigned start,unsigned length,ISoundPositionSource* source){return impl_->create(out,data,true,start,length,source);}
IIntegerEventSource* OpenALBufferSource::GetUpdateEventSource(){return impl_->updates;}
void OpenALBufferSource::update(unsigned ms){impl_->update(ms);}
void OpenALBufferSource::prepare(ISoundTemplate* definition){
    if(!definition)return;
    auto& b=*impl_;b.preparing=true;
    try {
        TRef<ISoundInstance> root;TRef<ISoundPositionSource> source=new GenericSoundSource;
        HRESULT result=definition->CreateSound(root,this,source);
        if(FAILED(result))throw std::runtime_error("Cannot prepare authored sound ("+std::to_string(result)+")");
        for(auto& i:b.preparingInstances)b.buffer(i->data,i->loopStart,i->loopLength,i->spatial);
        b.preparingInstances.clear();b.preparing=false;
    }catch(...){b.preparingInstances.clear();b.preparing=false;throw;}
}
void OpenALBufferSource::listener(glm::vec3 p,glm::vec3 v,glm::vec3 f,glm::vec3 u){impl_->listenerPosition=Vector(p.x,p.y,p.z);alListener3f(AL_POSITION,p.x,p.y,p.z);alListener3f(AL_VELOCITY,v.x,v.y,v.z);float o[]={f.x,f.y,f.z,u.x,u.y,u.z};alListenerfv(AL_ORIENTATION,o);}
void OpenALBufferSource::volume(float gain){alListenerf(AL_GAIN,std::clamp(gain,0.f,1.f));}
void OpenALBufferSource::stopAll(){for(auto& i:impl_->instances)i->Stop(true);}
void OpenALBufferSource::pause(bool value) {
    auto& b=*impl_;if(b.paused==value)return;b.paused=value;
    for(auto& slot:b.slots)if(slot.instance && slot.instance->playing) {
        if(value)alSourcePause(slot.source);else alSourcePlay(slot.source);
    }
}
void OpenALBufferSource::render(std::span<float> stereo){if(!impl_->offline || stereo.size()%2)throw std::runtime_error("Audio capture requires a stereo loopback context");impl_->renderSamples(impl_->device,stereo.data(),ALsizei(stereo.size()/2));}
size_t OpenALBufferSource::active()const{size_t count=0;for(auto& i:impl_->instances)if(i->playing)++count;return count;}
size_t OpenALBufferSource::physical()const{size_t count=0;for(auto& slot:impl_->slots)if(slot.instance)++count;return count;}
}
