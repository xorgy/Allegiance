#pragma once
#include <algorithm>

namespace Allegiance {
// WinTrek::HelpPosition: half-second linear motion, reversible from its
// current position. Coordinates are original image coordinates (Y up).
class HelpMotion {
    bool on_{};
    double started_{};
    float initial_{};
public:
    bool on()const{return on_;}
    float value(double time)const{
        return std::clamp(initial_+float((time-started_)*2)*(on_?1.f:-1.f),0.f,1.f);
    }
    void toggle(bool forceOn,double time){initial_=value(time);started_=time;on_=forceOn || !on_;}
    void close(double time){initial_=value(time);started_=time;on_=false;}
    float x(double time)const{return -1300.f+1330.f*value(time);}
    static constexpr float y=30;
};
}
