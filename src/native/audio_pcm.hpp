#pragma once
#include "../soundengine/soundbase.h"
#include "audio_decode.hpp"
namespace Native {
class AudioPCM final : public SoundEngine::ISoundPCMData {
    DecodedAudio audio_;
public:
    explicit AudioPCM(DecodedAudio audio):audio_(std::move(audio)){}
    unsigned GetNumberOfChannels()override{return audio_.channels;}
    unsigned GetBitsPerSample()override{return 16;}
    unsigned GetSampleRate()override{return audio_.rate;}
    unsigned GetSize()override{return unsigned(audio_.samples.size()*sizeof(int16_t));}
    void GetData(void* destination,unsigned offset,unsigned length)override{
        assert(offset<=GetSize() && length<=GetSize()-offset);
        memcpy(destination,reinterpret_cast<const char*>(audio_.samples.data())+offset,length);
    }
};
}
