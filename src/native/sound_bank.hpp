#pragma once
#include "legacy.hpp"
#include "assets.hpp"
#include "../soundengine/soundtemplates.h"
#include <functional>
#include <memory>
namespace Native {
// Parses the sound-only MDL namespace and constructs the original SoundEngine
// templates. Geometry/UI MDL never enters the audio backend.
class SoundBank {
public:
    using WaveLoader=std::function<TRef<SoundEngine::ISoundPCMData>(const std::filesystem::path&)>;
    SoundBank(Assets& assets,WaveLoader load);
    ~SoundBank();
    TRef<SoundEngine::ISoundTemplate> sound(int id);
    TRef<SoundEngine::ISoundTemplate> sound(const std::string& name);
    // Enumerates every random/serialized/intermittent branch without advancing
    // its playback state, so decode, repeat-fire mixing and AL upload happen
    // during loadout preparation rather than on the first trigger pull.
    TRef<SoundEngine::ISoundTemplate> preparation(int id);
    std::vector<int> ids()const;
    void resetSpeech();
    void categoryGains(float effectsDecibels,float voiceDecibels);
    int registerSound(const std::string& name); // Native catalog voices outside the static IGC ID table.
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
