#pragma once

namespace Allegiance {
// Shared with MissionParams::Invalid(true). Native clients retain their larger
// pilot capacity; objective/admin rules and their messages match WinTrek.
inline const char* MissionClientFlagsError(bool noWinningConditions,bool autoRestart,bool objectModelClubGame){
    if(noWinningConditions)return "You must choose one or more winning conditions.";
    if(autoRestart || objectModelClubGame)return "HACK: one or more admin-only flags are set.";
    return nullptr;
}
template<class Active> const char* MissionTeamReductionError(int previousTeams,int teams,Active active){
    if(teams<previousTeams){
        for(int side=0;side<teams;++side)if(active(side))return nullptr;
        return "You can't reduce the number of teams if all remaining teams would be inactive.";
    }
    return nullptr;
}
}
