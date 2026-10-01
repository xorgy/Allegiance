#pragma once
// Frozen WinTrek::RejectQueuedCommand / AcceptQueuedCommand from
// 39ef2c072b1eac0e9f97ab1428bc96572cbd5854. No ClientCore calls.
namespace OriginalQueuedRequests {
template<class ClearMoney,class ClearOrder,class GetSender,class ClearSender,class Reply>
bool reject(Money requested,CommandID command,bool clear,bool negative,IshipIGC* player,
    ClearMoney clearMoney,ClearOrder clearOrder,GetSender getSender,ClearSender clearSender,Reply reply) {
    if(requested!=0) {
        clearMoney();auto* sender=getSender();
        if(sender){if(negative){if(sender!=player)reply(sender);}clearSender();}
    } else if(command!=c_cidNone) {
        if(clear)clearOrder();auto* sender=getSender();
        if(sender){if(negative){if(sender!=player)reply(sender);}clearSender();}
    } else return false;
    return true;
}
inline Money approve(Money request,Money balance) {
    Money amount=balance;if(request>amount)request=amount;
    if(request>0)return request;
    return 0;
}
}
