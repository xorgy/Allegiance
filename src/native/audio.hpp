#pragma once
#include "assets.hpp"
#include "audio_scene.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <span>

template<class T> class TRef;
namespace SoundEngine {class ISoundInstance;}
namespace Native {
struct QuickChatCatalog;
using SoundHandle=uint64_t;
class Audio {
public:
    explicit Audio(Assets& assets,bool offline=false);
    ~Audio();
    TRef<SoundEngine::ISoundInstance> startCue(int id); // original graph completion for authored training
    void preload(std::span<const int> ids);
    void preload(QuickChatCatalog& catalog);
    size_t decodedWaveCount() const;
    size_t logicalVoiceCount()const;
    size_t physicalVoiceCount()const;
    SoundHandle play(int id,glm::vec3 position={},glm::vec3 velocity={},bool relative=false,bool loop=false,float gain=1,glm::vec3 direction={0,0,-1});
    SoundHandle play(const SoundEvent& event);
    void update(SoundHandle voice,glm::vec3 position,glm::vec3 velocity,float gain,float pitch,glm::vec3 direction={0,0,-1});
    void synchronize(std::span<const SoundLoop> loops);
    void stop(SoundHandle voice);
    void tick(double seconds);
    void listener(glm::vec3 position,glm::vec3 velocity,glm::vec3 forward,glm::vec3 up);
    void paused(bool value,bool muteOutput=true);
    void volume(float gain);
    void categoryGains(float effectsDecibels,float voiceDecibels);
    void stopAll();
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
