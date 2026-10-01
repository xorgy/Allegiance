// Original PurchasesPane/PartListItem policy, independent of the UI renderer.
#pragma once
#include "../Igc/igc.h"
#include <algorithm>
#include <vector>

namespace Allegiance {
// SectorMapPane::HasBuildableAsteroid gives the rock subtype precedence over
// the generic buildable bit. This is a display hint, not construction legality.
inline AsteroidAbilityBitMask InvestmentAsteroidMask(AsteroidAbilityBitMask mask) {
    return (mask & ~c_aabmBuildable) ? mask & ~c_aabmBuildable : mask;
}
inline Money InvestmentAmount(Money price,Money paid,Money wallet,bool partial) {
    Money remaining=price-paid;
    if(remaining<=0 || wallet<=0)return 0;
    if(!partial)return std::min(remaining,wallet);
    // Original right-click invests ten percent without completing the bucket.
    return std::max(Money(0),std::min({Money(price*.1),wallet,Money(remaining-1)}));
}
// ListPaneOld::RemoveItemByIdx moves a removed selection to the preceding
// surviving card. Removing the first selected card leaves no selection.
// Presence is supplied by the refreshed view's ID set; this runs only when
// bucket state changes, without retaining any IGC pointers in the UI.
template<class Range,class Present>
int InvestmentSelectionAfterRemoval(const Range& previous,int selected,Present present) {
    if(selected<0 || present(selected))return selected;
    int preceding=-1;
    for(int id:previous){if(id==selected)return preceding;if(present(id))preceding=id;}
    return -1;
}
inline std::vector<IbucketIGC*> InvestmentPrerequisites(IbucketIGC* selected) {
    std::vector<IbucketIGC*> result;auto* side=selected->GetSide();
    if(side->CanBuy(selected))return result;
    auto required=selected->GetRequiredTechs(),missing=required-side->GetTechs(),remaining=missing;
    for(auto* l=side->GetBuckets()->first();l;l=l->next()) {
        auto* bucket=l->data();
        if(bucket->GetCompleteF() || (bucket->GetPredecessor() && !bucket->GetPredecessor()->GetCompleteF()))continue;
        auto effect=bucket->GetEffectTechs();
        if(!(effect&missing).GetAllZero()){remaining-=effect;result.push_back(bucket);}
    }
    // Remove redundant successor-station dependencies, retaining original order.
    for(int i=int(result.size())-1;i>=0;--i) {
        if(result[i]->GetBucketType()!=OT_stationType)continue;
        auto reason=required&result[i]->GetEffectTechs();
        for(size_t j=0;j<result.size();++j) {
            if(size_t(i)==j || result[j]->GetBucketType()!=OT_stationType)continue;
            auto other=required&result[j]->GetEffectTechs();
            if(reason>=other && reason!=other)continue;
            auto* station=static_cast<IstationTypeIGC*>(result[i]->GetBuyable());
            auto* prerequisite=static_cast<IstationTypeIGC*>(result[j]->GetBuyable());
            if(prerequisite->GetDirectSuccessorStationType()==station){result.erase(result.begin()+i);break;}
        }
    }
    auto* buyable=selected->GetBuyable();
    if(result.empty() && !remaining.GetAllZero() && buyable->GetObjectType()!=OT_development) {
        if(buyable->GetObjectType()==OT_stationType && static_cast<IstationTypeIGC*>(buyable)->GetSuccessorStationType(side)!=buyable)return result;
        for(auto* l=side->GetBuckets()->first();l;l=l->next()) {
            auto* bucket=l->data();if(bucket==selected || bucket->GetBucketType()!=OT_stationType)continue;
            auto local=static_cast<IstationTypeIGC*>(bucket->GetBuyable())->GetLocalTechs();
            if(!(local&remaining).GetAllZero())result.push_back(bucket);
        }
    }
    return result;
}
}
