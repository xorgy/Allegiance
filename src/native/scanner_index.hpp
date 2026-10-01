// Native scanner broad phase. Original InScannerRange still decides sight.
#pragma once
#include "igc.h"
#include <algorithm>
#include <array>
#include <numeric>
#include <vector>
class NativeScannerIndex {
    struct Entry {IscannerIGC* scanner;Vector position;float range,radius;};
    struct Node {std::array<float,3> low,high;float range{},radius{};int begin{},end{},left{-1},right{-1},ordinal{};};
    std::vector<Entry> entries_;std::vector<int> order_;std::vector<Node> nodes_;bool active_{};
    int build(int begin,int end) {
        int id=int(nodes_.size());nodes_.emplace_back();Node n;n.begin=begin;n.end=end;n.ordinal=int(entries_.size());
        for(int a=0;a<3;++a)n.low[a]=n.high[a]=entries_[order_[begin]].position[a];
        for(int i=begin;i<end;++i){int ordinal=order_[i];auto& e=entries_[ordinal];n.ordinal=std::min(n.ordinal,ordinal);n.range=std::max(n.range,e.range);n.radius=std::max(n.radius,e.radius);
            for(int a=0;a<3;++a){n.low[a]=std::min(n.low[a],e.position[a]);n.high[a]=std::max(n.high[a],e.position[a]);}}
        if(end-begin>8){std::array<float,3> extent;for(int a=0;a<3;++a)extent[a]=n.high[a]-n.low[a];int axis=extent[1]>extent[0]?1:0;if(extent[2]>extent[axis])axis=2;int mid=(begin+end)/2;
            std::nth_element(order_.begin()+begin,order_.begin()+mid,order_.begin()+end,[&](int a,int b){return entries_[a].position[axis]<entries_[b].position[axis];});n.left=build(begin,mid);n.right=build(mid,end);}
        nodes_[id]=n;return id;
    }
    void find(int id,ImodelIGC* model,float signature,int& best)const {
        const auto& n=nodes_[id];if(n.ordinal>=best)return;float r=n.radius+model->GetRadius()+n.range*signature;
        float d2=0;for(int a=0;a<3;++a){float p=model->GetPosition()[a];float d=std::max({n.low[a]-p,0.f,p-n.high[a]});d2+=d*d;}
        if(d2>(r+.01f)*(r+.01f))return;
        if(n.left<0){for(int i=n.begin;i<n.end;++i){int index=order_[i];if(index<best && entries_[index].scanner->InScannerRange(model))best=index;}}
        else{int a=n.left,b=n.right;if(nodes_[b].ordinal<nodes_[a].ordinal)std::swap(a,b);find(a,model,signature,best);find(b,model,signature,best);}
    }
public:
    void end(){active_=false;}
    void begin(const ScannerListIGC* scanners) {
        entries_.clear();active_=scanners && scanners->n()>=64;if(!active_)return;
        for(auto* l=scanners->first();l;l=l->next()){auto* s=l->data();float range;
            switch(s->GetObjectType()){
            case OT_ship:range=static_cast<IshipIGC*>(s)->GetHullType()->GetScannerRange();break;
            case OT_station:range=static_cast<IstationIGC*>(s)->GetStationType()->GetScannerRange();break;
            case OT_probe:range=static_cast<IprobeIGC*>(s)->GetProbeType()->GetScannerRange();break;
            default:active_=false;return;
            }
            entries_.push_back({s,s->GetPosition(),range,s->GetRadius()});}
        order_.resize(entries_.size());std::iota(order_.begin(),order_.end(),0);nodes_.clear();nodes_.reserve(entries_.size()*2);build(0,int(entries_.size()));
    }
    IscannerIGC* find(const ScannerListIGC* scanners,ImodelIGC* model)const {
        float signature=model->GetSignature();if(auto* side=model->GetSide())signature/=side->GetGlobalAttributeSet().GetAttribute(c_gaSignature);
        if(!active_ || model->GetFlag()!=NA || !std::isfinite(signature) || signature<0){if(scanners)for(auto* l=scanners->first();l;l=l->next())if(l->data()->InScannerRange(model))return l->data();return nullptr;}
        int best=int(entries_.size());find(0,model,signature,best);return best<int(entries_.size())?entries_[best].scanner:nullptr;
    }
};
