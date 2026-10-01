// ConsoleData's three retained tips and four independently expiring critical
// tips. The display clock is independent of simulation and network ticks.
#pragma once
#include "MessageLifetime.h"
#include <array>
#include <cstdint>
#include <string>
#include <utility>
namespace Allegiance {
struct ConsoleTip {std::string text;double time{};uint64_t serial{};};
struct ConsoleTips {
    std::array<ConsoleTip,3> ordinary;
    std::array<ConsoleTip,4> critical;
    void post(ConsoleTip tip,bool warning,bool training,bool wraps=false) {
        if(!warning){for(size_t i=ordinary.size()-1;i>0;--i)ordinary[i]=std::move(ordinary[i-1]);ordinary[0]=std::move(tip);return;}
        auto set=[&](ConsoleTip value){
            if(!training)for(size_t i=critical.size()-1;i>0;--i)critical[i]=std::move(critical[i-1]);
            critical[0]=std::move(value);
        };
        auto time=tip.time;set(std::move(tip));
        // Original SetCriticalTipText reserves the next line by posting "".
        if(wraps)set({"",time,0});
    }
    unsigned visible(bool training,double now) {
        unsigned mask=0;for(size_t i=0;i<critical.size();++i){
            auto& tip=critical[i];if(now-tip.time>=CriticalTipDuration(training)){tip.text.clear();tip.serial=0;tip.time=now+3600;}
            if(!tip.text.empty())mask|=1u<<i;
        }
        return mask;
    }
};
}
