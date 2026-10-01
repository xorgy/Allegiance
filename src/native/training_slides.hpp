#pragma once
#include "assets.hpp"
#include "audio.hpp"
#include "legacy.hpp"
#include "frontend.hpp"
#include "../soundengine/soundengine.h"
namespace Native {
struct TrainingSlide {MenuView view;int sound{-1};};
std::vector<TrainingSlide> loadTrainingSlides(Assets& assets,int mission,bool post);
// Original slideshow lifecycle: real narration completion, a one-second gap,
// and any key/click advancing immediately. Decoding happens before playback.
class TrainingSlides {
    Audio& audio_;std::vector<TrainingSlide> slides_;size_t index_{};
    TRef<SoundEngine::ISoundInstance> cue_;double gap_{-1};
    void start();
public:
    TrainingSlides(Assets& assets,Audio& audio,int mission,bool post);
    ~TrainingSlides();
    void advance();void skip();void tick(double dt);bool finished()const{return index_>=slides_.size();}
    // Slideshow::Dismiss retains its last image while the simulation-start
    // message is presented before loading the mission.
    TrainingSlide& current(){return slides_.at(std::min(index_,slides_.size()-1));}
    const TrainingSlide& current()const{return slides_.at(std::min(index_,slides_.size()-1));}
    const std::vector<TrainingSlide>& all()const{return slides_;}
    size_t index()const{return index_;}
};
}
