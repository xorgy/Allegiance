#pragma once
#include "../Igc/igc.h"

namespace Allegiance {
// BaseClient::FM_S_PLAYER_RESCUED. The adapter owns recipient and sound routing.
template<class Post> inline void RescuePilotFeedback(bool rescuing,const char* other,Post post) {
    if(rescuing)post(false,"You rescued %s.",other);
    else post(true,"%s rescued you.",other);
}
// WinTrek::TK_Ripcord asks about current/accepted orders, then its own sector.
// BaseClient resolves a visible model or a current remote ship-status record.
template<class Resolve> inline IclusterIGC* RipcordKeyboardCluster(IshipIGC* ship,Resolve resolve) {
    auto* target=ship->GetCommandTarget(c_cmdCurrent);
    if(!target)target=ship->GetCommandTarget(c_cmdAccepted);
    auto* cluster=target?resolve(target):nullptr;
    return cluster?cluster:ship->GetCluster();
}
// BaseClient::FM_S_RIPCORD_ACTIVATE distinguishes requested and actual sectors.
template<class Post> inline void RipcordActivationFeedback(const char* destination,const char* requested,bool same,Post post) {
    if(same)post(true,"Ripcording to %s",destination);
    else post(true,"Ripcording to %s, which is closest to %s",destination,requested);
}
}
