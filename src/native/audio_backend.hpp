#pragma once
#include "legacy.hpp"
#include "../soundengine/soundbase.h"
#include <glm/glm.hpp>
#include <memory>
#include <span>
namespace SoundEngine { class ISoundTemplate; }
namespace Native {
class OpenALBufferSource final : public SoundEngine::ISoundBufferSource {
public:
    explicit OpenALBufferSource(bool offline=false);
    ~OpenALBufferSource()override;
    HRESULT CreateStaticBuffer(TRef<SoundEngine::ISoundInstance>& out,SoundEngine::ISoundPCMData* data,bool loop,SoundEngine::ISoundPositionSource* source=nullptr)override;
    HRESULT CreateASRBuffer(TRef<SoundEngine::ISoundInstance>& out,SoundEngine::ISoundPCMData* data,unsigned start,unsigned length,SoundEngine::ISoundPositionSource* source=nullptr)override;
    IIntegerEventSource* GetUpdateEventSource()override;
    void update(unsigned elapsedMilliseconds);
    void prepare(SoundEngine::ISoundTemplate* definition);
    void listener(glm::vec3 position,glm::vec3 velocity,glm::vec3 forward,glm::vec3 up);
    void volume(float gain);
    void pause(bool value);
    void stopAll();
    void render(std::span<float> stereo); // Loopback capture for deterministic audio verification.
    size_t active()const;
    size_t physical()const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
