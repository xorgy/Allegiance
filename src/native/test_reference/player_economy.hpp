#pragma once
#include "../../Igc/igc.h"
// Frozen FedSrv excerpts from 39ef2c072b1eac0e9f97ab1428bc96572cbd5854.
// CFS private-data/transport access is replaced by supplied account callbacks.
// These references do not include the production PlayerEconomy header.
namespace OriginalPlayerEconomy {
template<class Clear>
IshipIGC* donation(IshipIGC* donor,IshipIGC* requested,Clear clear) {
    IshipIGC* pship;
    if(requested) {
        pship=requested->GetAutoDonate();
        if(pship==NULL)pship=requested;
        else {
            assert(pship->GetAutoDonate()==NULL);
            if(pship==donor){clear(requested);pship=requested;}
        }
    } else pship=NULL;
    return pship;
}
template<class Balance>
IshipIGC* request(IshipIGC* sender,Money amount,Balance balance) {
    IshipIGC* pshipAD=sender->GetAutoDonate();
    if((!pshipAD) || (balance(pshipAD)<amount)) {
        Money moneyMax=amount-1;IsideIGC* pside=sender->GetSide();
        for(ShipLinkIGC* psl=pside->GetShips()->first();psl!=NULL;psl=psl->next()) {
            IshipIGC* pship=psl->data();
            if(pship!=sender) {
                if(balance(pship)>moneyMax){moneyMax=balance(pship);pshipAD=pship;}
            }
        }
    }
    return pshipAD;
}
struct Contender {IshipIGC* pship;int nVotes;};
inline IshipIGC* leader(const ShipListIGC* pships,IshipIGC* pshipIgnore,int maxVotes,Contender* pcontenders) {
    int nContenders=0;IshipIGC* pshipMax=NULL;
    for(ShipLinkIGC* psl=pships->first();psl!=NULL;psl=psl->next()) {
        IshipIGC* pshipAD=psl->data()->GetAutoDonate();
        if(pshipAD && pshipAD!=pshipIgnore) {
            int i;for(i=0;i<nContenders;i++) {
                if(pcontenders[i].pship==pshipAD){pcontenders[i].nVotes++;break;}
            }
            if(i==nContenders){pcontenders[nContenders].pship=pshipAD;pcontenders[nContenders++].nVotes=1;}
            if(pcontenders[i].nVotes>maxVotes){maxVotes=pcontenders[i].nVotes;pshipMax=pcontenders[i].pship;}
        }
    }
    return pshipMax;
}
}
