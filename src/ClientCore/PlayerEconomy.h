// FedSrv player accounts. Storage and transport remain in authority adapters.
#pragma once
#include "../Igc/igc.h"
namespace Allegiance {
// CFSPlayer::SetAutoDonate keeps at most one donation hop, and breaks a
// reciprocal link before reversing it. The clear callback preserves its event.
template<class Clear>
IshipIGC* AutoDonationTarget(IshipIGC* donor,IshipIGC* requested,Clear clear) {
    if(!requested)return nullptr;
    IshipIGC* target=requested->GetAutoDonate();
    if(!target)return requested;
    assert(target->GetAutoDonate()==nullptr);
    if(target==donor){clear(requested);return requested;}
    return target;
}
inline Money PaydayShare(Money pool,int players){return players?pool/players:0;}
template<class Human,class Credit>
void CreditPayday(const ShipListIGC* ships,Money share,Human human,Credit credit) {
    for(auto* link=ships->first();link;link=link->next()) {
        IshipIGC* ship=link->data();
        if(human(ship))credit(ship->GetAutoDonate()?ship->GetAutoDonate():ship,share);
    }
}
// FM_CS_REQUEST_MONEY prefers the auto-donation recipient, then the richest
// teammate able to cover the request. An existing recipient remains eligible
// when nobody else can pay; the request itself never transfers money.
template<class Balance>
IshipIGC* MoneyRequestRecipient(IshipIGC* sender,Money amount,Balance balance) {
    IshipIGC* target=sender->GetAutoDonate();
    if(!target || balance(target)<amount) {
        Money richest=amount-1;
        for(auto* link=sender->GetSide()->GetShips()->first();link;link=link->next()) {
            auto* ship=link->data();
            if(ship!=sender && balance(ship)>richest){richest=balance(ship);target=ship;}
        }
    }
    return target;
}
struct LeaderContender {IshipIGC* ship;int votes;};
// PickNewLeader's original order and strict tie behavior; caller supplies the
// original n-ships scratch storage. No container or frame-loop dependency.
inline IshipIGC* PickDonationLeader(const ShipListIGC* ships,IshipIGC* ignored,int mostVotes,LeaderContender* contenders) {
    int count=0;IshipIGC* best=nullptr;
    for(auto* link=ships->first();link;link=link->next()) {
        IshipIGC* target=link->data()->GetAutoDonate();
        if(target && target!=ignored) {
            int i=0;for(;i<count;++i)if(contenders[i].ship==target){++contenders[i].votes;break;}
            if(i==count){contenders[count].ship=target;contenders[count++].votes=1;}
            if(contenders[i].votes>mostVotes){mostVotes=contenders[i].votes;best=contenders[i].ship;}
        }
    }
    return best;
}
// RemovePlayerFromSide: a departing leader's incoming donations follow the
// replacement leader. Other departing accounts stop receiving donations.
template<class SetDonation>
void RedirectDepartingDonations(const ShipListIGC* ships,IshipIGC* departing,IshipIGC* leader,SetDonation set) {
    if(leader && leader->GetAutoDonate()==departing)set(leader,nullptr);
    for(auto* link=ships->first();link;link=link->next()) {
        auto* ship=link->data();
        if(ship->GetAutoDonate()==departing && ship!=leader)set(ship,leader);
    }
}
}
