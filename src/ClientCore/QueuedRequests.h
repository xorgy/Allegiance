// WinTrek's single queued order/money request and acknowledgement policy.
#pragma once
#include "../Igc/igc.h"
#include <string>
#include <string_view>
namespace Allegiance {
constexpr float QueuedRequestLifetime=15.f;
inline std::string MoneyRequestPrompt(std::string_view sender,Money amount,std::string_view hull) {
    std::string text(sender);text+=" has requested $";text+=std::to_string(amount);
    if(!hull.empty()){text+=" to buy a ";text+=hull;}
    text+="  Press the [Insert] key to approve it.";return text;
}
inline Money ApprovedMoneyRequest(Money requested,Money available) {
    if(requested>available)requested=available;
    return requested>0?requested:0;
}
template<class ClearMoney,class ClearOrder,class GetSender,class ClearSender,class Reply>
bool RejectQueuedRequest(Money requested,CommandID command,bool clear,bool negative,IshipIGC* player,
    ClearMoney clearMoney,ClearOrder clearOrder,GetSender getSender,ClearSender clearSender,Reply reply) {
    if(requested!=0)clearMoney();
    else if(command!=c_cidNone){if(clear)clearOrder();}
    else return false;
    if(auto* sender=getSender()) {
        if(negative && sender!=player)reply(sender);
        clearSender();
    }
    return true;
}
template<class RescueHull>
bool RecoveryOrderTip(bool pickup,bool targetIsSender,RescueHull rescueHull) {
    return pickup && targetIsSender && rescueHull();
}
}
