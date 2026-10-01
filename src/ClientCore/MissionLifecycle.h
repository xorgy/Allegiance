#pragma once
#include <cstdint>

namespace Allegiance {
// Existing native lobby flags carry these authority-owned policies. They are
// not editable client mission settings and add no fields to periodic reports.
inline constexpr uint32_t LobbyAutoRestart=16,LobbyServerPaused=32,
    LobbyRequirePlayers=64,LobbyNoRestart=128;
inline constexpr unsigned LobbyMinimumShift=8;
inline constexpr uint32_t LobbyObjectModelCreated=1u<<16; // Original fixed-world games cannot change active sides.
inline constexpr uint32_t LobbyGameOpenLocked=1u<<17; // Original bLockGameOpen disables lobby closing/Auto Accept.
inline constexpr uint32_t LobbyAutoStart=1u<<18;
inline constexpr uint32_t LobbyReconnecting=1u<<3;
inline constexpr unsigned LobbyReconnectingShift=19; // Faction seats in authenticated reconnect grace; No Team is neutral.
inline constexpr uint32_t LobbyReconnectingSides(uint32_t flags){return (flags>>LobbyReconnectingShift)&63;}
inline constexpr int LobbyMinimumPlayers(uint32_t flags){return (flags&LobbyRequirePlayers)?int((flags>>LobbyMinimumShift)&255):0;}

struct MissionLifecyclePolicy {
    bool allowRestart{true},autoStart{},autoRestart{},allowEmpty{true},gameOpenLocked{};
    float restartCountdown{60};unsigned minimum{1};
    template<class Params> explicit MissionLifecyclePolicy(const Params& p):
        allowRestart(p.bAllowRestart),autoStart(p.bAutoStart),autoRestart(p.bAutoRestart),
        allowEmpty(p.bAllowEmptyTeams),gameOpenLocked(p.bLockGameOpen),restartCountdown(p.fRestartCountdown),minimum(p.nMinPlayersPerTeam){}
    template<class Params> void apply(Params& p)const{
        p.bAllowRestart=allowRestart;p.bAutoStart=autoStart;p.bAutoRestart=autoRestart;
        p.bAllowEmptyTeams=allowEmpty;p.fRestartCountdown=restartCountdown;p.nMinPlayersPerTeam=minimum;
        p.bLockGameOpen=gameOpenLocked;
    }
};
inline bool MissionCanRestart(bool paused,bool allowed){return !paused && allowed;}
enum class AutomaticRestart {Wait,Warn,Briefing,Cancel};
// FedSrv's auto-restart loop: warning at twice the briefing interval, then
// force-readiness/minimum-player evaluation until the original deadline.
inline AutomaticRestart MissionAutomaticRestart(double remaining,double previous,
                                                 double briefing,bool allowEmpty,bool ready){
    if(remaining<=briefing){
        if(allowEmpty || ready)return AutomaticRestart::Briefing;
        if(remaining<=0)return AutomaticRestart::Cancel;
    }else if(remaining<=2*briefing && previous>2*briefing && !allowEmpty && !ready)
        return AutomaticRestart::Warn;
    return AutomaticRestart::Wait;
}
inline constexpr const char* MissionServerPausedMessage="The server for this game is shutting down and is not accepting new users.";
inline constexpr const char* MissionRestartCanceledMessage="Game canceled due to lack of players";
}
