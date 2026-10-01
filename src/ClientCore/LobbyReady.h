#pragma once
namespace Allegiance {
struct LobbyTeamState {
    int players{};
    bool active{true},ready{true};
};
enum class LobbyBlock { None,Unbalanced,TooFew,TooMany,NotReady };
struct LobbyStartState {
    LobbyBlock block{LobbyBlock::None};
    int side{-1};
    bool multiple{};
    bool CanStart()const {return block==LobbyBlock::None;}
};
// WinTrek::UpdateStatusText / FedSrv::FAllReady. Native pilots currently have
// no authenticated rank: FedSrv weighs every unranked player as rank one,
// giving Enforce Balance a rank threshold of one. Count all sides here (the
// old UI only traversed them when rank balancing was enabled).
template<class TeamAt>
inline LobbyStartState LobbyStart(int teams,int minimum,int maximum,int imbalance,bool allowEmpty,TeamAt teamAt) {
    int smallest=maximum,largest=0;
    for(int side=0;side<teams;++side){
        auto team=teamAt(side);
        if(allowEmpty && !team.active && !team.players)continue;
        if(team.players<smallest)smallest=team.players;
        if(team.players>largest)largest=team.players;
    }
    if(imbalance!=32767 && largest-smallest>(imbalance==32766?1:imbalance))
        return {LobbyBlock::Unbalanced};
    LobbyStartState result;
    for(int side=0;side<teams;++side){
        auto team=teamAt(side);LobbyBlock block=LobbyBlock::None;
        if(team.players<(team.active?minimum:0))block=LobbyBlock::TooFew;
        else if(team.players>maximum)block=LobbyBlock::TooMany;
        else if(team.active && !team.ready)block=LobbyBlock::NotReady;
        if(block!=LobbyBlock::None){
            if(result.block!=LobbyBlock::None){result.multiple=true;break;}
            result.block=block;result.side=side;
        }
    }
    return result;
}
inline const char* LobbyBlockText(LobbyBlock block) {
    switch(block){
    case LobbyBlock::TooFew:return "BELOW MINIMUM SIZE";
    case LobbyBlock::TooMany:return "ABOVE MAXIMUM SIZE";
    case LobbyBlock::NotReady:return "NOT READY";
    default:return "";
    }
}
// SetCountdownTimer's original h:mm:ss / m:ss / seconds formatting.
template<class IntegerString>
inline auto LobbyTimer(int seconds,IntegerString number) {
    if(seconds<0)seconds=0;
    auto result=number(seconds);
    if(seconds>=60){
        int hours=seconds/3600,minutes=(seconds/60)%60,remainder=seconds%60;
        result=hours?number(hours)+":"+(minutes<10?"0":"")+number(minutes):number(minutes);
        result+=":";if(remainder<10)result+="0";result+=number(remainder);
    }
    return result;
}
// CFSMission::CheckForSideAllReady. Keep readiness traversal lazy, including
// force-ready and the minimum non-ghost player count supplied by the adapter.
template<class AllPlayersReady>
inline bool SideReady(int players,int minimum,bool forceReady,AllPlayersReady allPlayersReady) {
    return players>=minimum && (forceReady || allPlayersReady());
}
}
