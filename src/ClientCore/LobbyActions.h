#pragma once

namespace Allegiance {
// TeamScreen::UpdateButtons leaves side locks and defection policy to the
// authority. A pending position request turns JOIN into Cancel.
constexpr bool LobbyJoinEnabled(bool pending,bool ownNoTeam,bool viewedNoTeam,
    bool ownTeam,bool positionsAvailable,bool active) {
    return pending || (ownNoTeam && !viewedNoTeam) ||
        ((viewedNoTeam || (positionsAvailable && active)) && !ownTeam);
}
// WinTrek::ShowPlayerContextMenu / TeamScreen::OnSelPlayer and FedSrv's
// QUIT_SIDE gate. Locking team settings prevents a commander from booting
// teammates, but does not prevent rejecting an outstanding join request.
constexpr bool CanBootTeammate(bool commander,bool self,bool teammate,bool locked) {
    return commander && !self && teammate && !locked;
}
struct LobbyPlayerActions {bool accept{},reject{},makeLeader{};};
enum class PlayerBootEffect {RejectRequest,LeaveTeam,LeaveMission};
// teampane.cpp::BootSink resolves a request before choosing QUIT_SIDE or
// QUIT_MISSION. Lobby context Boot always uses QUIT_SIDE instead.
constexpr PlayerBootEffect ConfirmedPlayerBoot(bool request,bool defections) {
    return request?PlayerBootEffect::RejectRequest:defections?PlayerBootEffect::LeaveTeam:PlayerBootEffect::LeaveMission;
}
constexpr LobbyPlayerActions LobbyPlayerMenu(bool commander,bool self,bool teammate,
    bool requesting,bool available,bool locked) {
    return {commander && !self && !teammate && requesting && available,
        CanBootTeammate(commander,self,teammate,locked) || (commander && !self && !teammate && requesting),
        commander && !self && teammate};
}
}
