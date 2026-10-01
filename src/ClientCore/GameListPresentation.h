#pragma once
#include <cstddef>
#include <cstdio>
#include <string_view>
#include <string>

namespace Allegiance {
inline constexpr std::size_t GameNameCapacity=65; // IGC c_cbGameName, including the terminator.
inline std::string_view CreatedGameNameError(std::string_view name) {
    return name.empty()?"Error : Game name is empty":name.size()>=GameNameCapacity?"Error : Game name is too long":"";
}
inline std::string DefaultCreatedGameName(std::string_view pilot) {
    // CreateGameDialog removes the appended rank at the last '('; preserve its
    // exact prefix (including any space), rather than normalizing the callsign.
    auto rank=pilot.rfind('(');if(rank!=pilot.npos && rank>1)pilot=pilot.substr(0,rank);
    return std::string(pilot)+"'s game";
}
// Preserve the Games List's original countdown, clock-skew and long-game labels.
inline void FormatGameListTime(char* text,std::size_t size,int seconds,bool countdown) {
    int hours=seconds/3600,minutes=seconds/60-hours*60;
    if(seconds<0 && countdown)std::snprintf(text,size,"-%d:%02d",-hours,-minutes);
    else if(seconds<-100 || hours>99)std::snprintf(text,size,"days");
    else if(seconds<0)std::snprintf(text,size,"0:00");
    else std::snprintf(text,size,"%d:%02d",hours,minutes);
}
}
