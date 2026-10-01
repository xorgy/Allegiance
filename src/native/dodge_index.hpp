// Conservative broad phase for the original IGC Dodge policy. Native only.
#pragma once
#include "igc.h"
#include <algorithm>
#include <array>
#include <numeric>
#include <limits>
#include <vector>
#ifdef __SSE2__
#include <xmmintrin.h>
#elif defined(__aarch64__)
#include <arm_neon.h>
#endif
class NativeDodgeIndex {
    struct Entry {ImodelIGC* model;Vector position,velocity;float radius;bool path,ship;int priority;int radiusIndex{};};
    struct alignas(16) Node {std::array<float,4> low{},high{},velocityLow{},velocityHigh{},futureLow{},futureHigh{};float radius{};int begin{},end{},left{-1},right{-1},skip{};bool path{};uint16_t radiusIndex{};int priority{-1};};
    struct SortKey {uint32_t code;int index;};
    struct Snapshot {std::vector<Entry> entries;std::vector<int> order;std::vector<Node> nodes;std::vector<SortKey> keys;std::vector<float> radii;int ships{};bool dirty{true};uint8_t excludedSides{};};
    std::array<Snapshot,c_cSidesMax+1> snapshots_;
    // Membership indexed by the original list ordinal. Spatial traversal sets
    // bits; consuming words restores policy order without sorting candidates.
    std::vector<uint64_t> hitWords_;size_t firstHitWord_{},lastHitWord_{};
    std::vector<ImodelIGC*> result_;
    void beginHits(size_t entries){hitWords_.resize((entries+63)/64,0);firstHitWord_=hitWords_.size();lastHitWord_=0;}
    void addHit(int index){size_t word=size_t(index)/64;hitWords_[word]|=uint64_t(1)<<(index%64);firstHitWord_=std::min(firstHitWord_,word);lastHitWord_=std::max(lastHitWord_,word);}
    std::vector<float> horizons_;
    bool active_{};uint32_t rebuilds_{};
    struct Query {
        const Vector& position;const Vector& velocity;
        float radius,acceleration,tMax;int minimumID;
        float* horizons{};mutable uint64_t readyHorizons{};
#ifdef __SSE2__
        __m128 packedPosition,packedVelocity;
#elif defined(__aarch64__)
        float32x4_t packedPosition,packedVelocity;
#endif
    };
#ifdef __SSE2__
    static __m128 packed(const Vector& v){return _mm_setr_ps(v.x,v.y,v.z,0);}
    static __m128 packed(const std::array<float,4>& v){return _mm_loadu_ps(v.data());}
    static bool sweptBoundsOverlap(__m128 low,__m128 high,float radius) {
        auto sign=_mm_set1_ps(-0.f);
        auto magnitude=_mm_max_ps(_mm_andnot_ps(sign,low),_mm_andnot_ps(sign,high));
        auto margin=_mm_add_ps(_mm_set1_ps(radius+.01f),_mm_mul_ps(_mm_set1_ps(1e-5f),magnitude));
        auto reject=_mm_or_ps(_mm_cmpgt_ps(low,margin),_mm_cmplt_ps(high,_mm_sub_ps(_mm_setzero_ps(),margin)));
        return (_mm_movemask_ps(reject)&7)==0;
    }
#elif defined(__aarch64__)
    static float32x4_t packed(const Vector& v){return {v.x,v.y,v.z,0};}
    static float32x4_t packed(const std::array<float,4>& v){return vld1q_f32(v.data());}
    static bool sweptBoundsOverlap(float32x4_t low,float32x4_t high,float radius) {
        auto magnitude=vmaxq_f32(vabsq_f32(low),vabsq_f32(high));
        auto margin=vaddq_f32(vdupq_n_f32(radius+.01f),vmulq_n_f32(magnitude,1e-5f));
        auto reject=vorrq_u32(vcgtq_f32(low,margin),vcltq_f32(high,vnegq_f32(margin)));
        // The fourth lane is padding, just as in the SSE path.
        return vmaxvq_u32(vsetq_lane_u32(0,reject,3))==0;
    }
#endif
    static bool sweptOverlap(const Vector& delta,const Vector& velocity,float radius,float horizon) {
#ifdef __SSE2__
        auto sweep=_mm_mul_ps(packed(velocity),_mm_set1_ps(horizon));
        auto low=_mm_add_ps(packed(delta),_mm_min_ps(_mm_setzero_ps(),sweep));
        auto high=_mm_add_ps(packed(delta),_mm_max_ps(_mm_setzero_ps(),sweep));
        return sweptBoundsOverlap(low,high,radius);
#elif defined(__aarch64__)
        auto sweep=vmulq_n_f32(packed(velocity),horizon);
        auto low=vaddq_f32(packed(delta),vminq_f32(vdupq_n_f32(0),sweep));
        auto high=vaddq_f32(packed(delta),vmaxq_f32(vdupq_n_f32(0),sweep));
        return sweptBoundsOverlap(low,high,radius);
#else
        for(int a=0;a<3;++a){float sweep=velocity[a]*horizon;float low=delta[a]+std::min(0.f,sweep),high=delta[a]+std::max(0.f,sweep);
            float margin=radius+.01f+1e-5f*std::max(std::abs(low),std::abs(high));if(low>margin || high<-margin)return false;}
        return true;
#endif
    }
    int build(Snapshot& tree,int begin,int end) {
        int id=int(tree.nodes.size());tree.nodes.emplace_back();Node node;node.begin=begin;node.end=end;
        if(end-begin>8){int mid=begin==0 && end==int(tree.entries.size()) && tree.ships>0 && tree.ships<end?tree.ships:(begin+end)/2;node.left=build(tree,begin,mid);node.right=build(tree,mid,end);
            // Spatial ordering only groups broad-phase bounds. Exact leaf
            // tests and the original linked-list result order remain intact.
            const auto& left=tree.nodes[node.left];const auto& right=tree.nodes[node.right];
            node.path=left.path || right.path;node.priority=std::max(left.priority,right.priority);node.radius=std::max(left.radius,right.radius);
            for(int a=0;a<3;++a){node.low[a]=std::min(left.low[a],right.low[a]);node.high[a]=std::max(left.high[a],right.high[a]);node.velocityLow[a]=std::min(left.velocityLow[a],right.velocityLow[a]);node.velocityHigh[a]=std::max(left.velocityHigh[a],right.velocityHigh[a]);node.futureLow[a]=std::min(left.futureLow[a],right.futureLow[a]);node.futureHigh[a]=std::max(left.futureHigh[a],right.futureHigh[a]);}
        }else{
            for(int a=0;a<3;++a){node.low[a]=node.high[a]=tree.entries[tree.order[begin]].position[a];node.velocityLow[a]=node.velocityHigh[a]=tree.entries[tree.order[begin]].velocity[a];}
            for(int i=begin;i<end;++i){const auto& e=tree.entries[tree.order[i]];node.path|=e.path;node.priority=std::max(node.priority,e.priority);node.radius=std::max(node.radius,e.radius);
                for(int a=0;a<3;++a){node.low[a]=std::min(node.low[a],e.position[a]);node.high[a]=std::max(node.high[a],e.position[a]);node.velocityLow[a]=std::min(node.velocityLow[a],e.velocity[a]);node.velocityHigh[a]=std::max(node.velocityHigh[a],e.velocity[a]);}}
        }
        if(node.left<0)for(int a=0;a<3;++a){
            node.futureLow[a]=std::numeric_limits<float>::infinity();node.futureHigh[a]=-node.futureLow[a];
            for(int i=begin;i<end;++i){const auto& e=tree.entries[tree.order[i]];
                float future=e.position[a]+e.velocity[a]*4;
                float error=.01f+1e-5f*(std::abs(e.position[a])+std::abs(e.velocity[a])*4);
                node.futureLow[a]=std::min(node.futureLow[a],future-error);node.futureHigh[a]=std::max(node.futureHigh[a],future+error);
            }
        }
        if(tree.radii.size()<=64)node.radiusIndex=uint16_t(std::lower_bound(tree.radii.begin(),tree.radii.end(),node.radius)-tree.radii.begin());
        node.skip=int(tree.nodes.size());tree.nodes[id]=node;return id;
    }
    static uint32_t interleave(uint32_t x) {
        x=(x|(x<<16))&0x030000ffu;x=(x|(x<<8))&0x0300f00fu;
        x=(x|(x<<4))&0x030c30c3u;x=(x|(x<<2))&0x09249249u;return x;
    }
    void spatialOrder(Snapshot& tree) {
        const auto& first=tree.entries.front().position;
        std::array<float,3> low{first.x,first.y,first.z},high=low;
        for(const auto& e:tree.entries)for(int a=0;a<3;++a){low[a]=std::min(low[a],e.position[a]);high[a]=std::max(high[a],e.position[a]);}
        std::array<float,3> scale;for(int a=0;a<3;++a)scale[a]=high[a]>low[a]?1.f/(high[a]-low[a]):0;
        tree.keys.resize(tree.entries.size());tree.ships=0;
        for(size_t i=0;i<tree.entries.size();++i){uint32_t code=tree.entries[i].ship?0:0x80000000u;tree.ships+=tree.entries[i].ship;
            for(int a=0;a<3;++a){auto coordinate=uint32_t(std::clamp((tree.entries[i].position[a]-low[a])*scale[a],0.f,1.f)*1023.f);code|=interleave(coordinate)<<a;}
            tree.keys[i]={code,int(i)};}
        std::sort(tree.keys.begin(),tree.keys.end(),[](const SortKey& a,const SortKey& b){return a.code<b.code || (a.code==b.code && a.index<b.index);});
        for(size_t i=0;i<tree.keys.size();++i)tree.order[i]=tree.keys[i].index;
    }
    static float horizon(const Query& q,int index,float radius) {
        if(q.tMax>=0)return q.tMax;
        if(!q.horizons)return std::sqrt(4*radius/q.acceleration);
        uint64_t bit=uint64_t(1)<<index;
        if(!(q.readyHorizons&bit)){q.horizons[index]=std::sqrt(4*radius/q.acceleration);q.readyHorizons|=bit;}
        return q.horizons[index];
    }
    void query(const Snapshot& tree,int id,const Query& q) {
      // Nodes are built in depth-first order. skip is the first node after a
      // subtree, so rejection needs no recursion or per-query traversal stack.
      while(id<int(tree.nodes.size())) {
        const auto& node=tree.nodes[id];if(node.priority<q.minimumID){id=node.skip;continue;}float r=q.radius+node.radius;
        float until=horizon(q,node.radiusIndex,r);
        // Relative swept AABBs include every possible impact. A generous small
        // margin keeps float rounding in solveForImpact from affecting policy.
        // A secant from the current position bounds to bounds of p+4v
        // retains position/velocity correlation for 0<=t<=4. Those cached
        // slopes replace the independent velocity extrema in the same sweep
        // test; longer horizons retain the original conservative bounds.
        bool correlated=until>=0 && until<=4;
        const auto& velocityLow=correlated?node.futureLow:node.velocityLow;
        const auto& velocityHigh=correlated?node.futureHigh:node.velocityHigh;
#ifdef __SSE2__
        auto lowSweep=_mm_mul_ps(_mm_sub_ps(packed(velocityLow),q.packedVelocity),_mm_set1_ps(until));
        auto highSweep=_mm_mul_ps(_mm_sub_ps(packed(velocityHigh),q.packedVelocity),_mm_set1_ps(until));
        auto low=_mm_add_ps(_mm_sub_ps(packed(node.low),q.packedPosition),_mm_min_ps(_mm_setzero_ps(),lowSweep));
        auto high=_mm_add_ps(_mm_sub_ps(packed(node.high),q.packedPosition),_mm_max_ps(_mm_setzero_ps(),highSweep));
        if(!sweptBoundsOverlap(low,high,r)){id=node.skip;continue;}
#elif defined(__aarch64__)
        auto lowSweep=vmulq_n_f32(vsubq_f32(packed(velocityLow),q.packedVelocity),until);
        auto highSweep=vmulq_n_f32(vsubq_f32(packed(velocityHigh),q.packedVelocity),until);
        auto low=vaddq_f32(vsubq_f32(packed(node.low),q.packedPosition),vminq_f32(vdupq_n_f32(0),lowSweep));
        auto high=vaddq_f32(vsubq_f32(packed(node.high),q.packedPosition),vmaxq_f32(vdupq_n_f32(0),highSweep));
        if(!sweptBoundsOverlap(low,high,r)){id=node.skip;continue;}
#else
        bool overlap=true;
        for(int a=0;a<3;++a){float low=node.low[a]-q.position[a]+std::min(0.f,(velocityLow[a]-q.velocity[a])*until);
            float high=node.high[a]-q.position[a]+std::max(0.f,(velocityHigh[a]-q.velocity[a])*until);
            float margin=r+.01f+1e-5f*std::max(std::abs(low),std::abs(high));if(low>margin || high<-margin){overlap=false;break;}}
        if(!overlap){id=node.skip;continue;}
#endif
        if(node.left<0){for(int i=node.begin;i<node.end;++i){int index=tree.order[i];const auto& e=tree.entries[index];if(e.priority<q.minimumID)continue;float combined=q.radius+e.radius;
            float until=horizon(q,e.radiusIndex,combined);
#ifdef __SSE2__
            auto delta=_mm_sub_ps(packed(e.position),q.packedPosition);
            auto sweep=_mm_mul_ps(_mm_sub_ps(packed(e.velocity),q.packedVelocity),_mm_set1_ps(until));
            auto low=_mm_add_ps(delta,_mm_min_ps(_mm_setzero_ps(),sweep));
            auto high=_mm_add_ps(delta,_mm_max_ps(_mm_setzero_ps(),sweep));
            if(sweptBoundsOverlap(low,high,combined))addHit(index);
#elif defined(__aarch64__)
            auto delta=vsubq_f32(packed(e.position),q.packedPosition);
            auto sweep=vmulq_n_f32(vsubq_f32(packed(e.velocity),q.packedVelocity),until);
            auto low=vaddq_f32(delta,vminq_f32(vdupq_n_f32(0),sweep));
            auto high=vaddq_f32(delta,vmaxq_f32(vdupq_n_f32(0),sweep));
            if(sweptBoundsOverlap(low,high,combined))addHit(index);
#else
            if(sweptOverlap(e.position-q.position,e.velocity-q.velocity,combined,until))addHit(index);
#endif
        }}
        ++id;
      }
    }
    Snapshot& rebuild(const ModelListIGC* models,IshipIGC* ship) {
        // Ignore's original drone policy excludes enemy ships entirely. Keep
        // them out of this query's tree, including when alliances change.
        auto* side=ship->GetSide();bool drone=ship->GetPilotType()<c_ptPlayer;int key=drone && side && side->GetObjectID()>=0 && side->GetObjectID()<c_cSidesMax?side->GetObjectID()+1:0;
        auto& tree=snapshots_[key];uint8_t excluded=0;if(drone){
            for(auto* l=ship->GetMission()->GetSides()->first();l;l=l->next())if(l->data()!=side && !IsideIGC::AlliedSides(side,l->data()))excluded|=1u<<l->data()->GetObjectID();
            if(side)excluded|=1u<<c_cSidesMax;}
        if(excluded!=tree.excludedSides){tree.dirty=true;tree.excludedSides=excluded;}
        if(tree.dirty){++rebuilds_;tree.entries.clear();for(auto* l=models->first();l;l=l->next()){auto* m=l->data();auto velocity=m->GetVelocity();auto type=m->GetObjectType();if(type==OT_buoy)continue;
                if(type==OT_ship && drone && m->GetSide()!=side && !IsideIGC::AlliedSides(side,m->GetSide()))continue;
                int priority=32768;if(type==OT_ship){auto* other=static_cast<IshipIGC*>(m);if(other->GetPilotType()<c_ptPlayer && !(other->GetStateM()&miningMaskIGC) && !other->GetRipcordModel())priority=other->GetObjectID();}
                tree.entries.push_back({m,m->GetPosition(),velocity,m->GetRadius(),type!=OT_mine && (type!=OT_ship || velocity.LengthSquared()<1),type==OT_ship,priority});}
            // Fleets reuse a small number of hull/obstacle radii. Cache the
            // exact acceleration-dependent horizon per radius for each query.
            // Unusually varied radii use the original calculation instead.
            tree.radii.clear();tree.radii.push_back(0);
            for(const auto& e:tree.entries)tree.radii.push_back(e.radius);
            std::sort(tree.radii.begin(),tree.radii.end());tree.radii.erase(std::unique(tree.radii.begin(),tree.radii.end()),tree.radii.end());
            if(tree.radii.size()<=64)for(auto& e:tree.entries)e.radiusIndex=int(std::lower_bound(tree.radii.begin(),tree.radii.end(),e.radius)-tree.radii.begin());
            tree.order.resize(tree.entries.size());tree.nodes.clear();tree.nodes.reserve(tree.entries.size()*2);if(!tree.entries.empty()){
                spatialOrder(tree);build(tree,0,int(tree.entries.size()));
                // Convert endpoint envelopes once during construction, rather
                // than interpolating two boxes at every visited query node.
                for(auto& node:tree.nodes)for(int a=0;a<3;++a){node.futureLow[a]=(node.futureLow[a]-node.low[a])*.25f;node.futureHigh[a]=(node.futureHigh[a]-node.high[a])*.25f;}
            }tree.dirty=false;}
        return tree;
    }
    void path(const Snapshot& tree,int id,const Vector& from,const Vector& to,float radius,int minimumID) {
        const auto& n=tree.nodes[id];if(!n.path || n.priority<minimumID)return;
        float margin=radius+n.radius+.01f;
        for(int a=0;a<3;++a)if(n.low[a]>std::max(from[a],to[a])+margin || n.high[a]<std::min(from[a],to[a])-margin)return;
        if(n.left<0){for(int i=n.begin;i<n.end;++i)if(tree.entries[tree.order[i]].path && tree.entries[tree.order[i]].priority>=minimumID)addHit(tree.order[i]);}
        else{path(tree,n.left,from,to,radius,minimumID);path(tree,n.right,from,to,radius,minimumID);}
    }
    const std::vector<ImodelIGC*>* result(const Snapshot& tree){
        result_.clear();
        if(firstHitWord_<hitWords_.size())for(size_t word=firstHitWord_;word<=lastHitWord_;++word){
            uint64_t bits=hitWords_[word];hitWords_[word]=0;
            while(bits){unsigned bit=__builtin_ctzll(bits);result_.push_back(tree.entries[word*64+bit].model);bits&=bits-1;}
        }
        return &result_;
    }

public:
    void begin(){active_=true;rebuilds_=0;invalidate();}
    void end(){active_=false;}
    // Every begin() rebuilds the phase snapshot. Motion outside an active
    // query phase cannot invalidate anything currently used by Dodge.
    void invalidate(){if(active_)for(auto& tree:snapshots_)tree.dirty=true;}
    uint32_t rebuilds()const{return rebuilds_;}
    const std::vector<ImodelIGC*>* pathCandidates(const ModelListIGC* models,IshipIGC* ship,const Vector& goal,float margin) {
        if(!active_ || models->n()<64)return nullptr;auto& tree=rebuild(models,ship);beginHits(tree.entries.size());if(!tree.nodes.empty())path(tree,0,ship->GetPosition(),goal,ship->GetRadius()+margin,ship->GetPilotType()<c_ptPlayer?ship->GetObjectID():-1);return result(tree);
    }
    const std::vector<ImodelIGC*>* candidates(const ModelListIGC* models,IshipIGC* ship,float tMax) {
        if(!active_ || models->n()<64)return nullptr;
        float acceleration=ship->GetHullType()->GetThrust()/ship->GetMass();
        if(!std::isfinite(tMax) || (tMax<0 && !(acceleration>0)))return nullptr;
        auto& tree=rebuild(models,ship);
        Query q{ship->GetPosition(),ship->GetVelocity(),ship->GetRadius()+1,acceleration,tMax,ship->GetPilotType()<c_ptPlayer?ship->GetObjectID():-1};
        if(tMax<0 && tree.radii.size()<=64){
            horizons_.resize(tree.radii.size());
            q.horizons=horizons_.data();
        }
#if defined(__SSE2__) || defined(__aarch64__)
        q.packedPosition=packed(q.position);q.packedVelocity=packed(q.velocity);
#endif
        beginHits(tree.entries.size());if(!tree.nodes.empty())query(tree,0,q);
        // Dodge selects the first object on ties; retain the linked-list order.
        return result(tree);
    }
};
