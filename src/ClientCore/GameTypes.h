#pragma once
#include <array>

namespace Allegiance {
// The authored presets and recognition policy used by WinTrek's game settings.
struct GameTypeValues {
    bool developments,invulnerableStations,ejectPods;
    short lives;
    float prosperity;
    short conquest,flags,artifacts,kills,territory;
    float seconds;
    bool experimental;
};
struct GameTypeDefinition {const char* name;GameTypeValues values;short map;};
inline constexpr std::array<GameTypeDefinition,6> GameTypes{{
    {"Conquest",{true,false,true,32767,0,100,0,0,0,100,0,false},8},
    {"Death Match",{false,true,false,32767,0,0,0,0,20,100,0,false},6},
    {"Capture the Flag",{false,true,true,32767,0,0,10,0,0,100,0,false},8},
    {"Artifacts",{true,false,true,32767,0,0,0,10,0,100,0,false},2},
    {"Territorial",{true,false,true,32767,0,0,0,0,0,70,0,false},12},
    {"Experimental",{true,false,true,32767,0,100,0,0,0,100,0,true},8}
}};
inline bool MatchesGameType(const GameTypeValues& a,const GameTypeValues& b) {
    return a.developments==b.developments && a.invulnerableStations==b.invulnerableStations && a.ejectPods==b.ejectPods &&
        (a.lives==32767)==(b.lives==32767) && (a.prosperity==0)==(b.prosperity==0) && (a.conquest==0)==(b.conquest==0) &&
        (a.flags==0)==(b.flags==0) && (a.artifacts==0)==(b.artifacts==0) && (a.kills==0)==(b.kills==0) &&
        (a.territory==100)==(b.territory==100) && (a.seconds==0)==(b.seconds==0) && a.experimental==b.experimental;
}
}
