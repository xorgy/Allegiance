#include "legacy.hpp"
#include "audio_pcm.hpp"
#include "../soundengine/soundutil.h"
namespace SoundEngine {
HRESULT LoadWaveFile(TRef<ISoundPCMData>& result,const ZString& path,const bool mono) {
    try{result=new Native::AudioPCM(Native::decodeAudio(static_cast<const char*>(path),mono));return S_OK;}
    catch(const std::exception&){return E_FAIL;}
}
}
