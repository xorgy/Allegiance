#pragma once
#include <cstdint>

namespace Allegiance {
// CFSMission::CheckPositionRequest / GetSideRankSum. Unranked pilots weigh
// one for Enforce Balance; ordinary count imbalance allows two newcomers.
constexpr bool UnrankedLobbyPositionAvailable(bool beforeStart,int imbalance,int count,int minimum) {
    return beforeStart || imbalance==32767 ||
        (imbalance==32766?count==minimum:count<minimum+imbalance+2);
}
enum class LobbyJoinFailure : uint32_t {
    None,Other,Rejected,SideGone,NoDefections,ServerPaused,SidesLocked,Banned,NoBase,TeamBalance
};
// TeamScreen::OnDelRequest captions. These are action/event results, not
// additional periodic state or editable client admission policy.
constexpr const char* LobbyJoinFailureMessage(LobbyJoinFailure reason) {
    switch(reason){
    case LobbyJoinFailure::Rejected:return "You have not been accepted to the team.";
    case LobbyJoinFailure::SideGone:return "The team you were trying to join has been deleted.";
    case LobbyJoinFailure::NoDefections:return "You can't join this team - defections are not allowed.";
    case LobbyJoinFailure::ServerPaused:return "The server is shutting down and is not accepting new players.";
    case LobbyJoinFailure::SidesLocked:return "The sides are currently locked.";
    case LobbyJoinFailure::Banned:return "You have been removed (i.e. booted) from this team by the commander.";
    case LobbyJoinFailure::NoBase:return "You can't join this team - it no longer owns a base.";
    case LobbyJoinFailure::TeamBalance:return "You can't join this team - it would unbalance the teams.";
    default:return "You were not able to join the team.";
    }
}
}
