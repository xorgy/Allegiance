// BaseClient::ReplaceLoadout equipment policy. Callers own cache/storage and UI.
#pragma once
#include "MissionEconomy.h"
namespace Allegiance {
// BaseClient::RestoreLoadout's matching-hull branch. Cache adapters preserve
// original mount order without imposing linked lists or IGC pointers on users.
template<class CachedPart,class Forward,class Reverse,class Resolve>
Money RestoreMatchingLoadout(IshipIGC* source,IshipIGC* ship,IstationIGC* station,
    IhullTypeIGC* saved,Money budget,Forward forward,Reverse reverse,Resolve resolve) {
    auto* hull=station->CanBuy(saved)?static_cast<IhullTypeIGC*>(station->GetSuccessor(saved)):saved;
    assert(saved->GetPrice()==hull->GetPrice());
    budget-=hull->GetPrice();ship->SetBaseHullType(hull);
    forward([](CachedPart& part){part.bDuplicated=false;});
    for(auto* link=source->GetParts()->first();link;link=link->next()) {
        auto* old=link->data();auto mount=old->GetMountID();auto* type=old->GetPartType();
        if(mount<0 || !hull->CanMount(type,mount))continue;
        if(station->CanBuy(type))BuyPartOnBudget(ship,static_cast<IpartTypeIGC*>(station->GetSuccessor(type)),mount,&budget);
        else {
            Money price=type->GetPrice();short amount=old->GetAmount();
            if(price!=0) {
                if(IlauncherTypeIGC::IsLauncherType(type->GetEquipmentType())) {
                    if(price*amount>budget){assert(price>0);amount=budget/price;}
                    price*=amount;
                } else if(budget>=price)amount=0; // Preserve the original priced-part rule.
            }
            if(amount!=0){budget-=price;ship->CreateAndAddPart(type,mount,amount);}
        }
        bool found=false;CachedPart* wrongMount=nullptr;
        reverse([&](CachedPart& part){
            if(resolve(part)==type && !part.bDuplicated) {
                if(part.mount==mount){part.bDuplicated=true;found=true;return false;}
                else wrongMount=&part;
            }
            return true;
        });
        if(!found && wrongMount)wrongMount->bDuplicated=true;
    }
    Mount cargo=-c_maxCargo;
    forward([&](CachedPart& part){
        if(part.bDuplicated)return;
        auto* old=resolve(part);if(!old)return;
        auto* type=station->CanBuy(old)?static_cast<IpartTypeIGC*>(station->GetSuccessor(old)):station->GetSimilarPart(old);
        if(!type)return;
        if(part.mount>=0 && !ship->GetMountedPart(type->GetEquipmentType(),part.mount) && hull->CanMount(type,part.mount))
            BuyPartOnBudget(ship,type,part.mount,&budget);
        else if(cargo<0)BuyPartOnBudget(ship,type,cargo++,&budget);
    });
    if(saved!=hull)BuyPreferredParts(ship,station,&budget);
    return budget;
}
inline IhullTypeIGC* PurchasableLoadoutHull(IstationIGC* station,IhullTypeIGC* saved,Money budget) {
    if(!saved || !station->CanBuy(saved))return NULL;
    auto* successor=static_cast<IhullTypeIGC*>(station->GetSuccessor(saved));
    // Check the actual purchase price, not the obsolete hull's price.
    return successor->GetPrice()<=budget?successor:NULL;
}
template<class VisitParts>
inline void BuyCachedLoadoutParts(IshipIGC* ship,IstationIGC* station,IhullTypeIGC* saved,Money* budget,VisitParts visit) {
    auto* hull=ship->GetBaseHullType();
    visit([&](IpartTypeIGC* old,Mount mount) {
        if(!old)return;
        auto* part=station->CanBuy(old)?static_cast<IpartTypeIGC*>(station->GetSuccessor(old)):station->GetSimilarPart(old);
        if(part && (mount<0 || part->GetEquipmentType()!=ET_Weapon || mount<hull->GetMaxWeapons()) && hull->CanMount(part,mount))BuyPartOnBudget(ship,part,mount,budget);
    });
    if(saved!=hull)BuyPreferredParts(ship,station,budget);
}
}
