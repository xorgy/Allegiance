#pragma once
#include <array>
#include <span>
#include <string>

namespace Allegiance {
// WinTrek's FillWinCondition shows the leader and self, keeping list order
// for exact ties. It deliberately labels the best other #2 when self ties #1.
struct GameStateRank {int index{-1},rank{};};
template<class T,class Greater>
std::array<GameStateRank,2> GameStateRanks(std::span<const T> entries,int self,Greater greater) {
    if(self<0 || self>=int(entries.size()))return {};
    int other=-1,rank=1;
    for(int i=0;i<int(entries.size());++i)if(i!=self){
        if(other<0 || greater(entries[i],entries[other]))other=i;
        if(greater(entries[i],entries[self]))++rank;
    }
    return rank==1?std::array<GameStateRank,2>{{{self,1},{other,other<0?0:2}}}:
                   std::array<GameStateRank,2>{{{other,1},{self,rank}}};
}
inline bool GameStateKillsGreater(int kills,int deaths,int otherKills,int otherDeaths) {
    return kills>otherKills || (kills==otherKills && deaths<otherDeaths);
}
inline std::string GameStateClock(bool countdown,int seconds) {
    if(seconds<0)return countdown?"Game Over":"Game Starting";
    return std::string(countdown?"Time Remaining: ":"Time Elapsed: ")+std::to_string(seconds/60)+":"+
        (seconds%60<10?"0":"")+std::to_string(seconds%60);
}
}
