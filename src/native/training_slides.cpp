#include "training_slides.hpp"
#include <cmath>
#include <climits>
namespace Native {
namespace {
const UiNumbers& soundIds() {
    static const UiNumbers ids=[] {
        UiNumbers result;int id=1001;
#define DEFSOUND(name) result.emplace(#name "SoundId",id++);
#define DEFEMPTYSOUND(name) DEFSOUND(name)
#include "../Igc/sounds.h"
#undef DEFSOUND
#undef DEFEMPTYSOUND
        return result;
    }();
    return ids;
}
}
std::vector<TrainingSlide> loadTrainingSlides(Assets& assets,int mission,bool post) {
    const char* name=nullptr;
    switch(mission){case 1:name="tm_1_introduction";break;case 2:name="tm_2_basic_flight";break;case 3:name="tm_3_basic_weaponry";break;
    case 4:name="tm_4_enemy_engagement";break;case 5:name="tm_5_command_view";break;
    case 6:case 10:name="tm_6_practice_arena";break;case 8:name="tm_8_nanite";break;default:throw std::runtime_error("Unknown training slides");}
    OriginalUi ui(assets);auto images=ui.imageList(std::string(name)+(post && mission!=1?"_post":""),"slides",soundIds());
    if(images.empty())throw std::runtime_error("Empty authored training slideshow");
    std::vector<TrainingSlide> result;result.reserve(images.size());
    for(auto& image:images){
        if(!std::isfinite(image.value) || image.value<-1 || image.value>INT_MAX || image.value!=std::floor(image.value))throw std::runtime_error("Invalid authored slide sound ID");
        TrainingSlide slide;slide.sound=int(image.value);slide.view.original=true;slide.view.artwork=std::move(image.artwork);
        for(auto& control:slide.view.artwork)control.translate({0,600});
        result.push_back(std::move(slide));
    }
    return result;
}
TrainingSlides::TrainingSlides(Assets& assets,Audio& audio,int mission,bool post):audio_(audio),slides_(loadTrainingSlides(assets,mission,post)) {
    std::vector<int> ids;for(auto& slide:slides_)ids.push_back(slide.sound);audio_.preload(ids);start();
}
TrainingSlides::~TrainingSlides(){if(cue_)cue_->Stop(true);}
void TrainingSlides::start(){gap_=-1;if(!finished())cue_=audio_.startCue(slides_[index_].sound);}
void TrainingSlides::advance(){if(cue_){cue_->Stop(true);cue_=nullptr;}if(!finished())++index_;start();}
void TrainingSlides::skip(){if(cue_){cue_->Stop(true);cue_=nullptr;}index_=slides_.size();}
void TrainingSlides::tick(double dt){if(finished())return;if(cue_ && cue_->IsPlaying()==S_OK)return;
    if(gap_<0)gap_=0;else gap_+=dt;if(gap_>=1)advance();}
}
