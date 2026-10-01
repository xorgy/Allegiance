#pragma once
#include "legacy.hpp"
#include "../soundengine/soundutil.h"
#include <algorithm>
#include <cmath>
#include <memory>

namespace Native {
// A live gamepanes gain replaces the original modifiable MDL number. Instances
// register intrusively: settings changes update active sounds, while playback
// adds no category traversal, polling, allocation or decoder work each frame.
class CategoryGain {
    class Instance;
    friend class CategoryTemplate;
    Instance* first_{};
    float gain_{};
public:
    void set(float gain);
    float value()const{return gain_;}
};
class CategoryGain::Instance final:public SoundEngine::SoundInstanceWrapper,public SoundEngine::SoundTweakableWrapper {
    std::shared_ptr<CategoryGain> category_;
    float baseGain_{};
    Instance *previous_{},*next_{};
    friend class CategoryGain;
public:
    Instance(std::shared_ptr<CategoryGain> category,SoundEngine::ISoundInstance* sound,SoundEngine::ISoundTweakable* tweak)
        :SoundInstanceWrapper(sound),SoundTweakableWrapper(tweak),category_(std::move(category)),next_(category_->first_){
        if(next_)next_->previous_=this;category_->first_=this;
    }
    ~Instance()override{if(previous_)previous_->next_=next_;else category_->first_=next_;if(next_)next_->previous_=previous_;}
    HRESULT SetGain(float gain)override{baseGain_=gain;return SoundTweakableWrapper::SetGain(gain+category_->gain_);}
    TRef<SoundEngine::ISoundTweakable> GetISoundTweakable()override{return this;}
};
inline void CategoryGain::set(float gain){
    gain=std::isfinite(gain)?std::clamp(gain,-100.f,0.f):0.f;if(gain==gain_)return;gain_=gain;
    for(auto* instance=first_;instance;instance=instance->next_)instance->SetGain(instance->baseGain_);
}
class CategoryTemplate final:public SoundEngine::ISoundTemplate {
    std::shared_ptr<CategoryGain> category_;
    TRef<SoundEngine::ISoundTemplate> source_;
public:
    CategoryTemplate(std::shared_ptr<CategoryGain> category,SoundEngine::ISoundTemplate* source):category_(std::move(category)),source_(source){}
    HRESULT CreateSound(TRef<SoundEngine::ISoundInstance>& out,SoundEngine::ISoundBufferSource* backend,SoundEngine::ISoundPositionSource* position)override{
        HRESULT result=source_->CreateSound(out,backend,position);if(FAILED(result))return result;
        auto tweak=out->GetISoundTweakable();if(!tweak)return E_FAIL;
        result=tweak->SetGain(category_->value());if(FAILED(result))return result;
        out=new CategoryGain::Instance(category_,out,tweak);return S_OK;
    }
};
}
