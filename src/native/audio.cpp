#include "audio.hpp"
#include "quick_chat.hpp"
#include "sound_bank.hpp"
#include "audio_backend.hpp"
#include "audio_pcm.hpp"
#include "../soundengine/soundutil.h"
#include <array>
#include <unordered_map>
#include <unordered_set>

namespace Native {
using namespace SoundEngine;
namespace {
class FollowingSoundSource final:public GenericSoundSource {
    std::shared_ptr<SoundEmitter> emitter_;glm::vec3 offset_,direction_;
    static Vector convert(glm::vec3 p){return {p.x,p.y,p.z};}
public:
    FollowingSoundSource(std::shared_ptr<SoundEmitter> emitter,glm::vec3 offset,glm::vec3 direction)
        :emitter_(std::move(emitter)),offset_(offset),direction_(direction){}
    HRESULT GetPosition(Vector& p)override{p=convert(emitter_->relative?offset_:emitter_->position+emitter_->orientation*offset_);return S_OK;}
    HRESULT GetVelocity(Vector& v)override{v=convert(emitter_->relative?glm::vec3(0):emitter_->velocity);return S_OK;}
    HRESULT GetOrientation(Vector& d)override{d=convert(emitter_->relative?direction_:emitter_->orientation*direction_);return S_OK;}
    HRESULT IsListenerRelative()override{return emitter_->relative?S_OK:S_FALSE;}
    HRESULT IsPlaying()override{return emitter_->active?S_OK:S_FALSE;}
};
Vector vector(glm::vec3 v){return {v.x,v.y,v.z};}
float decibels(float gain){return gain>0?std::max(-100.f,20.f*std::log10(gain)):-100.f;}
}
struct Audio::Impl {
    struct Voice {
        TRef<ISoundInstance> sound;TRef<GenericSoundSource> position;
        uint32_t generation{};bool loop{};float gain{-100},linearGain{1},pitch{1};
        int64_t older{-1},newer{-1};
        glm::vec3 at{},velocity{},direction{0,0,-1};
    };
    TRef<OpenALBufferSource> backend;
    std::unordered_map<std::string,TRef<ISoundPCMData>> waves;
    SoundBank bank;
    static constexpr unsigned oneShotCapacity=512;
    std::vector<Voice> voices;std::vector<uint32_t> free;
    int64_t oldestShot{-1},newestShot{-1};size_t oneShots{};
    struct LoopVoice {SoundHandle handle{};int id{-1};bool relative{};uint64_t frame{};};
    std::unordered_map<uint64_t,LoopVoice> loops;uint64_t loopFrame{};
    std::unordered_set<int> missing,prepared;
    float master{.7f};bool muted{},muteOutput{true},offline{};double renderFrames{};uint32_t serial{};double milliseconds{};
    explicit Impl(Assets& assets,bool offline):backend(new OpenALBufferSource(offline)),bank(assets,[&](const auto& path){
        auto key=path.string();auto found=waves.find(key);if(found!=waves.end())return found->second;
        TRef<ISoundPCMData> pcm=new AudioPCM(decodeAudio(path));waves.emplace(std::move(key),pcm);return pcm;
    }),offline(offline){grow();}
    void grow(){size_t old=voices.size(),count=std::max<size_t>(512,old*2);if(count>UINT32_MAX)throw std::runtime_error("Audio identity arena exhausted");voices.resize(count);for(size_t i=count;i>old;)free.push_back(uint32_t(--i));}
    Voice* find(SoundHandle handle){uint32_t index=uint32_t(handle);return handle && index<voices.size() && voices[index].generation==uint32_t(handle>>32) && voices[index].sound?&voices[index]:nullptr;}
    void retire(uint32_t index){auto& v=voices[index];if(!v.sound)return;
        if(!v.loop){if(v.older<0)oldestShot=v.newer;else voices[v.older].newer=v.newer;
            if(v.newer<0)newestShot=v.older;else voices[v.newer].older=v.older;--oneShots;}
        v={};free.push_back(index);
    }
    void reap(){for(uint32_t i=0;i<voices.size();++i)if(voices[i].sound && voices[i].sound->IsPlaying()!=S_OK)retire(i);}
    SoundHandle start(int id,glm::vec3 at,glm::vec3 velocity,bool relative,bool loop,float gain,glm::vec3 direction,TRef<GenericSoundSource> emitter=nullptr);
    TRef<ISoundTemplate> definition(int id) {
        if(missing.contains(id))return nullptr;
        try{auto value=bank.sound(id);if(!value)missing.insert(id);return value;}
        catch(const std::exception& e){fprintf(stderr,"Audio template %d: %s\n",id,e.what());missing.insert(id);return nullptr;}
    }
};
Audio::Audio(Assets& assets,bool offline):impl_(std::make_unique<Impl>(assets,offline)){}
Audio::~Audio(){stopAll();}
size_t Audio::decodedWaveCount()const{return impl_->waves.size();}
size_t Audio::logicalVoiceCount()const{return impl_->voices.size()-impl_->free.size();}
size_t Audio::physicalVoiceCount()const{return impl_->backend->physical();}
void Audio::preload(QuickChatCatalog& catalog) {
    for(auto& chat:catalog.sonicChats)for(auto& voice:chat.voices)voice.sound=impl_->bank.registerSound(voice.soundName);
    catalog.refreshSounds();preload(catalog.soundIds);
    // Some installations omit a voice file. Keep the logical message ID and its
    // own authored text, while substituting a startup-warmed notification cue.
    int fallback=impl_->bank.registerSound("newOfflineChatMsgSound");preload(std::span(&fallback,1));
    for(auto& chat:catalog.sonicChats)for(auto& voice:chat.voices)if(impl_->missing.contains(voice.sound))voice.sound=fallback;
    catalog.refreshSounds();
}
void Audio::preload(std::span<const int> ids){
    auto& a=*impl_;
    for(int id:ids)if(id>=0 && a.prepared.insert(id).second && a.definition(id)) {
        try{a.backend->prepare(a.bank.preparation(id));}
        catch(const std::exception& e){fprintf(stderr,"Audio preparation %d: %s\n",id,e.what());a.missing.insert(id);}
    }
}
SoundHandle Audio::Impl::start(int id,glm::vec3 at,glm::vec3 velocity,bool relative,bool loop,float gain,glm::vec3 direction,TRef<GenericSoundSource> emitter) {
    auto& a=*this;auto definition=a.definition(id);if(!definition)return 0;
    // The one-shot budget is independent of persistent world loops. Retire the
    // oldest shot in O(1), avoiding a fleet-wide scan for every incoming shot.
    if(!loop && a.oneShots>=oneShotCapacity){auto chosen=uint32_t(a.oldestShot);a.voices[chosen].sound->Stop(true);a.retire(chosen);}
    if(a.free.empty())a.grow();uint32_t chosen=a.free.back();a.free.pop_back();auto& voice=a.voices[chosen];
    if(emitter)voice.position=emitter;
    else voice.position=new GenericSoundSource(vector(at),relative?Vector::GetZero():vector(velocity),vector(direction),relative);
    HRESULT result=definition->CreateSound(voice.sound,a.backend,voice.position);
    if(FAILED(result) || !voice.sound){fprintf(stderr,"Audio template %d: cannot create sound (%d)\n",id,result);voice={};a.free.push_back(chosen);return 0;}
    voice.generation=++a.serial;voice.loop=loop;voice.at=at;voice.velocity=velocity;voice.linearGain=gain;voice.gain=decibels(gain);voice.direction=direction;
    if(!loop){voice.older=a.newestShot;if(a.newestShot<0)a.oldestShot=chosen;else a.voices[a.newestShot].newer=chosen;a.newestShot=chosen;++a.oneShots;}
    if(gain!=1)if(auto tweak=voice.sound->GetISoundTweakable())tweak->SetGain(voice.gain);
    return (SoundHandle(voice.generation)<<32)|chosen;
}
TRef<ISoundInstance> Audio::startCue(int id){auto handle=impl_->start(id,{},{},true,false,1,{0,0,-1});auto* voice=impl_->find(handle);return voice?voice->sound:TRef<ISoundInstance>();}
SoundHandle Audio::play(int id,glm::vec3 at,glm::vec3 velocity,bool relative,bool loop,float gain,glm::vec3 direction){return impl_->start(id,at,velocity,relative,loop,gain,direction);}
SoundHandle Audio::play(const SoundEvent& event){
    TRef<GenericSoundSource> source=event.emitter?new FollowingSoundSource(event.emitter,event.offset,event.direction):nullptr;
    return impl_->start(event.id,event.position,event.velocity,event.relative,false,1,event.direction,source);
}
void Audio::update(SoundHandle handle,glm::vec3 at,glm::vec3 velocity,float gain,float pitch,glm::vec3 direction) {
    auto* voice=impl_->find(handle);if(!voice)return;auto& v=*voice;
    if(v.at!=at){v.at=at;v.position->SetPosition(vector(at));}
    if(v.direction!=direction){v.direction=direction;v.position->SetOrientation(vector(direction));}
    if(v.velocity!=velocity){v.velocity=velocity;v.position->SetVelocity(vector(velocity));}
    if(gain!=v.linearGain){v.linearGain=gain;gain=decibels(gain);}else gain=v.gain;
    pitch=std::clamp(pitch,.1f,4.f);
    if(gain!=v.gain || pitch!=v.pitch)if(auto tweak=v.sound->GetISoundTweakable()) {
        if(gain!=v.gain){v.gain=gain;tweak->SetGain(gain);}
        if(pitch!=v.pitch){v.pitch=pitch;tweak->SetPitch(pitch);}
    }
}
void Audio::synchronize(std::span<const SoundLoop> sounds) {
    auto& a=*impl_;++a.loopFrame;
    for(const auto& sound:sounds) {
        if(sound.id<0)continue;
        auto& loop=a.loops[sound.key];
        if(loop.id!=sound.id || !a.find(loop.handle)) {
            stop(loop.handle);loop={play(sound.id,sound.position,sound.velocity,sound.relative,true,sound.gain,sound.direction),sound.id,sound.relative,a.loopFrame};
        }
        if(loop.relative!=sound.relative && loop.handle) {
            if(auto* voice=a.find(loop.handle))voice->position->SetIsListenerRelative(sound.relative);
            loop.relative=sound.relative;
        }
        update(loop.handle,sound.position,sound.velocity,sound.gain,sound.pitch,sound.direction);
        loop.frame=a.loopFrame;
    }
    for(auto it=a.loops.begin();it!=a.loops.end();)if(it->second.frame!=a.loopFrame){stop(it->second.handle);it=a.loops.erase(it);}else ++it;
}
void Audio::tick(double seconds) {
    auto& a=*impl_;if(a.muted)return;seconds=std::clamp(seconds,0.,1.);
    a.milliseconds+=seconds*1000;auto whole=unsigned(a.milliseconds);a.milliseconds-=whole;a.backend->update(whole);
    if(a.offline) {
        // Loopback devices have no hardware clock. Render the actual mixer, so
        // authored SoundFinishedCondition observes real PCM completion in tests.
        a.renderFrames+=seconds*48000;unsigned frames=unsigned(a.renderFrames);a.renderFrames-=frames;
        std::array<float,960> samples;
        while(frames){unsigned count=std::min(frames,480u);a.backend->render(std::span(samples).first(count*2));frames-=count;}
        a.backend->update(0);
    }
    a.reap();
}
void Audio::stop(SoundHandle handle){if(auto* v=impl_->find(handle))v->sound->Stop(false);}
void Audio::listener(glm::vec3 p,glm::vec3 v,glm::vec3 f,glm::vec3 u){impl_->backend->listener(p,v,f,u);}
void Audio::paused(bool value,bool muteOutput){impl_->muted=value;impl_->muteOutput=muteOutput;impl_->backend->pause(value);impl_->backend->volume(value && muteOutput?0:impl_->master);}
void Audio::volume(float gain){impl_->master=std::clamp(gain,0.f,1.f);paused(impl_->muted,impl_->muteOutput);}
void Audio::categoryGains(float effects,float voice){impl_->bank.categoryGains(effects,voice);}
void Audio::stopAll(){auto& a=*impl_;a.loops.clear();a.bank.resetSpeech();for(auto& v:a.voices)if(v.sound)v.sound->Stop(true);a.backend->stopAll();a.backend->update(0);a.reap();}
}
