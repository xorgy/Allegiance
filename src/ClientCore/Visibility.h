// Visibility policy factored from WinTrek and FedSrv ThingSiteImpl.
#pragma once
#include "../Igc/igc.h"

namespace Allegiance {
// Same-side visibility is unconditional; allied visibility is a mission rule.
inline bool SharesSight(IsideIGC* viewer, IsideIGC* owner, bool alliedSight) {
    return viewer == owner || (alliedSight && IsideIGC::AlliedSides(viewer, owner));
}

// WinTrek's fast path. Keep scanner registration, discovery events and the
// existing Windows fallback loop in their adapters; no new frame-wide work.
inline bool CachedSideEye(IsideIGC* viewer, ImodelIGC* model,
                          IscannerIGC* lastSpotter, bool alliedSight) {
    return SharesSight(viewer, model->GetSide(), alliedSight) ||
        (lastSpotter && SharesSight(viewer, lastSpotter->GetSide(), alliedSight) &&
         lastSpotter->InScannerRange(model));
}

// FedSrv's side-specific scan/retention policy. A known predictable object stays
// known after losing the scanner; current eye does not. The caller supplies the
// authorized scanner lists and handles discovery/replication/score notifications.
// Keeping the last spotter avoids repeating the fallback scan for tracked models.
template<class FindSpotter>
void UpdateSideSight(SideVisibility& sight, ImodelIGC* model, IsideIGC* side,
                     bool alliedSight, FindSpotter findSpotter) {
    bool eye = CachedSideEye(side, model, sight.pLastSpotter(), alliedSight);
    if (!eye) {
        IscannerIGC* spotter = findSpotter();
        if (spotter != sight.pLastSpotter()) sight.pLastSpotter(spotter);
        eye = spotter != nullptr;
    }
    sight.CurrentEyed(eye);
    sight.fVisible(eye || (sight.fVisible() && (model->GetAttributes() & c_mtPredictable)));
}

// Original FedSrv scanner lists, with explicit cleanup for headless/offline
// mission teardown. Registration is driven by IGC, not a rebuilt frame cache.
class SideScanners {
    ScannerListIGC scanners_[c_cSidesMax];
public:
    SideScanners() = default;
    SideScanners(const SideScanners&) = delete;
    SideScanners& operator=(const SideScanners&) = delete;
    ~SideScanners() { Clear(); }
    void Add(SideID side, IscannerIGC* scanner) {
        assert(side>=0 && side<c_cSidesMax);
        AddIbaseIGC((BaseListIGC*)&scanners_[side],scanner);
    }
    void Delete(SideID side, IscannerIGC* scanner) {
        assert(side>=0 && side<c_cSidesMax);
        DeleteIbaseIGC((BaseListIGC*)&scanners_[side],scanner);
    }
    const ScannerListIGC* Get(SideID side) const {
        assert(side>=0 && side<c_cSidesMax);return &scanners_[side];
    }
    void Clear() {
        for (auto& list:scanners_) while(auto* link=list.first()) {
            auto* scanner=link->data();delete link;scanner->Release();
        }
    }
};

class ModelVisibility {
    SideVisibility sides_[c_cSidesMax];
public:
    ModelVisibility() = default;
    ModelVisibility(const ModelVisibility&) = delete;
    ModelVisibility& operator=(const ModelVisibility&) = delete;
    void Clear() { for(auto& s:sides_){s.pLastSpotter(nullptr);s.CurrentEyed(false);s.fVisible(false);} }
    bool Known(IsideIGC* side) const {return side && sides_[side->GetObjectID()].fVisible();}
    bool Eye(IsideIGC* side) {return side && sides_[side->GetObjectID()].CurrentEyed();}
    IscannerIGC* LastSpotter(IsideIGC* side) {return side?sides_[side->GetObjectID()].pLastSpotter():nullptr;}
    void Set(IsideIGC* side,bool known,bool eye) {
        auto& s=sides_[side->GetObjectID()];s.fVisible(known);s.CurrentEyed(eye);
        if(!eye)s.pLastSpotter(nullptr);
    }
    void ChangeCluster(ImodelIGC* model) {
        const bool predictable=(model->GetAttributes()&c_mtPredictable)!=0;
        for(auto& s:sides_){s.pLastSpotter(nullptr);s.CurrentEyed(false);if(!predictable)s.fVisible(false);}
    }
    template<class Discovered>
    void Update(ImodelIGC* model,IclusterIGC* cluster,Discovered discovered) {
        if(!cluster || !(model->GetAttributes()&c_mtSeenBySide))return;
        auto* mission=cluster->GetMission();auto* site=cluster->GetClusterSite();
        bool allied=mission->GetMissionParams()->bAllowAlliedViz;
        for(auto* l=mission->GetSides()->first();l;l=l->next()) {
            auto* side=l->data();auto& sight=sides_[side->GetObjectID()];bool known=sight.fVisible(),eye=sight.CurrentEyed(),cached=true;
            UpdateSideSight(sight,model,side,allied,[&]() -> IscannerIGC* {
                cached=false;
                // Own scanners first, then permitted allies. Revalidate alliance
                // on the cached fast path too, so a broken alliance cannot leak.
                auto scan=[&](SideID id) -> IscannerIGC* {
#ifdef ALLEGIANCE_NATIVE
                    return site->FindScanner(id,model);
#else
                    auto* list=site->GetScanners(id);if(!list)return nullptr;
                    for(auto* s=list->first();s;s=s->next())if(s->data()->InScannerRange(model))return s->data();
                    return nullptr;
#endif
                };
                if(auto* s=scan(side->GetObjectID()))return s;
                if(allied)for(auto* a=mission->GetSides()->first();a;a=a->next())
                    if(a->data()!=side && IsideIGC::AlliedSides(side,a->data()))
                        if(auto* s=scan(a->data()->GetObjectID()))return s;
                return nullptr;
            });
            if(sight.fVisible()!=known || sight.CurrentEyed()!=eye)
                discovered(side,known,sight.fVisible(),eye,sight.CurrentEyed(),cached);
        }
    }
};
}
