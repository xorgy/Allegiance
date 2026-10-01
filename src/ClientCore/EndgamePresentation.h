#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <algorithm>
namespace Allegiance {
// Frozen public end-of-round records. No object pointers, implicit padding or
// persistent account data; native state and its shared report use these bytes.
struct EndgamePlayer {
    char name[32]{};
    int16_t id{-1},side{-1},civilization{-1},rank{-1};
    int16_t kills{},deaths{},ejections{},assists{},baseKills{},baseCaptures{};
    uint32_t flags{}; // 1 commander credit, 2 win/loss counted
    float score{},played{};
    bool operator==(const EndgamePlayer&)const=default;
};
struct EndgameTeam {
    char name[32]{};float color[4]{1,1,1,1};
    int16_t id{-1},civilization{-1},kills{},deaths{},ejections{},baseKills{},baseCaptures{},flags{},artifacts{};
    uint16_t reserved{};
    bool operator==(const EndgameTeam&)const=default;
};
static_assert(sizeof(EndgamePlayer)==64 && offsetof(EndgamePlayer,played)+sizeof(float)==sizeof(EndgamePlayer));
static_assert(sizeof(EndgameTeam)==68 && offsetof(EndgameTeam,reserved)+sizeof(uint16_t)==sizeof(EndgameTeam));
inline int EndgameNameCompare(std::string_view a,std::string_view b){
    auto fold=[](unsigned char c){return c>='A' && c<='Z'?c+('a'-'A'):c;};
    for(size_t i=0;i<std::min(a.size(),b.size());++i)if(fold(a[i])!=fold(b[i]))return fold(a[i])<fold(b[i])?-1:1;
    return a.size()<b.size()?-1:a.size()>b.size()?1:0;
}
inline std::string EndgamePlayTime(float seconds){int minutes=int(std::max(0.f,seconds))/60;return std::to_string(minutes/60)+(minutes%60<10?":0":":")+std::to_string(minutes%60);}
inline bool EndgamePlayerLess(const EndgamePlayer& a,const EndgamePlayer& b,int column,bool reverse,bool pods){
    int order=0;auto compare=[&](auto x,auto y){order=x<y?-1:x>y?1:0;};
    switch(column){case 1:order=EndgameNameCompare(a.name,b.name);break;case 2:compare(a.rank,b.rank);if(!order)compare(a.civilization,b.civilization);break;
    case 3:compare(a.baseKills,b.baseKills);break;case 4:compare(a.baseCaptures,b.baseCaptures);break;case 5:compare(a.score,b.score);break;
    case 6:compare(a.assists,b.assists);break;case 7:compare(a.kills,b.kills);break;case 8:compare(pods?a.ejections:a.deaths,pods?b.ejections:b.deaths);break;case 9:compare(a.played,b.played);break;}
    return reverse?order>0:order<0;
}
inline bool EndgameTeamLess(const EndgameTeam& a,const EndgameTeam& b,int column,bool reverse){
    int order=0;auto compare=[&](auto x,auto y){order=x<y?-1:x>y?1:0;};
    switch(column){case 1:order=EndgameNameCompare(a.name,b.name);break;case 2:compare(a.baseKills,b.baseKills);break;case 3:compare(a.baseCaptures,b.baseCaptures);break;
    case 4:compare(a.flags,b.flags);break;case 5:compare(a.artifacts,b.artifacts);break;case 6:compare(a.kills,b.kills);break;case 7:compare(a.deaths,b.deaths);break;}
    return reverse?order>0:order<0;
}
}
