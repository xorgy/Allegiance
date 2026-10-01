#pragma once
#include <string>
#include <string_view>
namespace Allegiance {
// ConsoleData's original BoardState: no parent, observer, occupied turret.
enum class CrewBoardState {Pilot,Observer,Turret};
enum class CrewTransfer {Board,Disembark,Teleport};
inline std::string CrewTransferMessage(CrewTransfer transfer,std::string_view name) {
    switch(transfer){
    case CrewTransfer::Board:return "Boarding "+std::string(name)+"'s ship....";
    case CrewTransfer::Disembark:return "Disembarking from "+std::string(name)+"'s ship....";
    case CrewTransfer::Teleport:return "Teleporting to "+std::string(name)+"....";
    }
    return {};
}
constexpr CrewBoardState CrewState(bool parent,int turret) {
    return !parent?CrewBoardState::Pilot:turret<0?CrewBoardState::Observer:CrewBoardState::Turret;
}
inline std::string CrewLoadoutLabel(CrewBoardState state,std::string_view sourcePilot) {
    return state==CrewBoardState::Pilot?std::string{}:
        std::string(state==CrewBoardState::Observer?"Observer of ":"Turret of ")+std::string(sourcePilot);
}
// HangarScreen::OnBoardShip and BaseClient's LOADOUT_CHANGE/PROMOTE feedback.
// The callback preserves PostText's critical/non-critical tip and sound policy.
template<class Post> inline void CrewBoardFeedback(const char* parent,bool observer,Post post) {
    if(parent)post(false,"You have boarded %s's ship as %s.",parent,observer?"an observer":"a turret gunner");
    else post(false,"You have disembarked.");
}
template<class Post> inline void CrewPassengerFeedback(const char* passenger,bool observer,Post post) {
    post(true,observer?"%s has boarded your ship as an observer.":"%s has boarded your ship as a turret.",passenger);
}
template<class Post> inline void CrewPromotionFeedback(bool promoted,Post post) {
    post(true,promoted?"You have been promoted to pilot":"You have been demoted to a turret gunner");
}
}
