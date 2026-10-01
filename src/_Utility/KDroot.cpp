/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	KDroot.cpp
**
**  Author: 
**
**  Description:
**
**  History:
*/
#include "pch.h"
#ifdef ALLEGIANCE_NATIVE
#include <algorithm>
#include <limits>
#endif
#if defined(ALLEGIANCE_NATIVE) && defined(__SSE2__)
#include <xmmintrin.h>
#endif

KDroot::KDroot(bool bStatic)
:
    KDnode(NULL),
    m_nAdded(0),
    m_nDeleted(0),
    m_bStatic(bStatic)
{
    //allocate enough space for everything we'll ever want
    allocHitTestsEndpoints((c_maxHitTests * 3) >> 1);
}

KDroot::~KDroot(void)
{
    //everything should be marked dead before the root destructor is called.
    flush();
    assert (m_nHitTests == 0);
}

void    KDroot::addHitTest(HitTest*   pHitTest)
{
    assert (m_nHitTests <= m_maxHitTests);

    //Not allowed to add a node that was marked dead but not yet pruned.
    //Danger: we are performing an operation on another KDRoot which,
    //theoretically, could have been freed. The only reason it works is
    //that -- if it were freed -- then it would have been flushed and all
    //of the hittests that refered back to the it would have had their
    //KDRoot's cleared.
    if (pHitTest->GetKDRoot() != NULL)
        pHitTest->GetKDRoot()->flush();

    assert (pHitTest->GetKDRoot() == NULL);

    pHitTest->SetKDRoot(this);
    pHitTest->SetDeadF(false);
    pHitTest->SetDeletedF(false);

    if (m_nHitTests == m_maxHitTests)
    {
        //Need more space but, unlike a KDnode, we can't simply nuke the
        //object/endpoint arrays: we need to create, copy and then nuke.
        assert (m_ppHitTests);

        //Double the existing space
        int n = m_maxHitTests << 1;
        void**  v = new void* [n * (1 + 2 * c_nAxes)];
#ifdef ALLEGIANCE_NATIVE
        m_nativeOrder.reset(new uint32_t[n*(1+2*c_nAxes)]);
#endif
        assert (v);

        m_maxHitTests = n;
        for (int i = 0; (i < c_nAxes); i++)
        {
            Endpoint**  pOldEndpoints = m_ppEndpoints[i];
            m_ppEndpoints[i] = (i == 0)
                               ? ((Endpoint**)(v + n))
                               : m_ppEndpoints[i - 1] + (m_maxHitTests << 1);

            for (int j = 0; (j < (m_nHitTests << 1)); j++)
                m_ppEndpoints[i][j] = pOldEndpoints[j];
        }
        {
            HitTest**    ppOldHitTests = m_ppHitTests;
            m_ppHitTests = (HitTest**)v;

            for (int i = 0; (i < m_nHitTests); i++)
                m_ppHitTests[i] = ppOldHitTests[i];

            delete [] ppOldHitTests;
        }
    }
    assert (m_nHitTests < m_maxHitTests);

    if (m_bStatic)
        pHitTest->UpdateStaticBB();

    //Add a reference to the hitTest's data object
    pHitTest->AddRef();

    //Add the object to the list of 
    m_ppHitTests[m_nHitTests] = pHitTest;

    //create and sort the endpoint lists for each axis.
    {
        int ht2 = m_nHitTests << 1;

        for (int axis = 0; (axis < c_nAxes); axis++)
        {
            m_ppEndpoints[axis][ht2    ] = &pHitTest->m_endpoints[axis][0];
            m_ppEndpoints[axis][ht2 + 1] = &pHitTest->m_endpoints[axis][1];
        }
    }
    
    m_nHitTests++;

    //Increment the changed count (which determines which sort is used in the update).
    m_nAdded++;
}

void    KDroot::deleteHitTest(HitTest*    pHitTest)
{
    //Actually defer the delete until the next update
    m_nDeleted++;
    pHitTest->SetDeadF(true);
    pHitTest->SetDeletedF(true);

    //If we are deleting a hit test ... it must preserve the pointer to the old hit test
    assert (pHitTest->GetKDRoot() == this);
}

void    KDroot::flush(void)
{
    if (m_nDeleted > 0)
    {
#ifdef ALLEGIANCE_NATIVE
        m_nativeNodes.clear();m_nativeLeaves.clear();
        m_nativeBoxes=nullptr;m_nativeBoxStorage.clear();m_nativeFlags=nullptr;m_nativeFlagStorage.clear();m_nativeHits=nullptr;
#endif
        //One or more objects were deleted ... scan through the object and endpoint arrays, nuking the dead objects.
        //Endpoint arrays first
        for (int axis = 0; (axis < c_nAxes); axis++)
        {
            int dest = 0;
            for (int source = 0; (source < (m_nHitTests << 1)); source++)
            {
                if (!m_ppEndpoints[axis][source]->pHitTest->m_deletedF)
                {
                    m_ppEndpoints[axis][dest++] = m_ppEndpoints[axis][source];
                }
            }
        }

        {
            //Do the same for the objects ... but also release the object as well
            int dest = 0;
            for (int source = 0; (source < m_nHitTests); source++)
            {
                HitTest*    pht = m_ppHitTests[source];
                if (pht->m_deletedF)
                {
                    pht->SetKDRoot(NULL);   //We are not allowed to add this hit test to another KD tree until after this point
                                            //so mark the node as not being dead (and acceptable to add) here.

                    pht->Release();
                }
                else
                {
                    m_ppHitTests[dest++] = pht;
                }
            }

            m_nHitTests = dest;
        }

        m_nDeleted = 0;
    }
}

void    KDroot::update(void)
{
    //Only works from the root
    assert (!m_parent);

    bool    bSort = (!m_bStatic) || (m_nAdded > 0);

    if (bSort || ((m_nDeleted << 4) > m_nHitTests))
    {
        flush();
#ifdef ALLEGIANCE_NATIVE
        // A frame snapshot of bounds and partition flags keeps KD construction
        // from repeatedly dirtying and chasing scattered HitTest allocations.
        // Endpoints still sort and partition in their original pointer order.
        m_nativeBoxStorage.resize(m_nHitTests);m_nativeBoxes=m_nativeBoxStorage.data();
        m_nativeFlagStorage.resize(m_nHitTests);m_nativeFlags=m_nativeFlagStorage.data();m_nativeHits=m_ppHitTests;m_nativeHasTargets=false;
        for(int i=0;i<m_nHitTests;++i){auto* hit=m_ppHitTests[i];m_nativeOrder[i]=uint32_t(i);m_nativeBoxes[i].bounds=hit->m_boundingBox;
            m_nativeFlags[i]=hit->m_id?2:0;m_nativeHasTargets|=bool(m_nativeFlags[i]&2);
            for(int axis=0;axis<c_nAxes;++axis)for(int high=0;high<2;++high){auto& endpoint=hit->m_endpoints[axis][high];
                endpoint.value=hit->m_boundingBox.axes[axis].values[high];endpoint.nativeIndex=uint32_t(i);
            }
        }
#endif
        if (m_nHitTests >= c_minRootSize)
        {
            if (bSort)
            {
                //Resort the list of endpoints.
                //Use the short or long sort depending on how things have changed
                for (int    axis = 0; (axis < c_nAxes); axis++)
                {
                    if ((m_nAdded << 2) > m_nHitTests)
                    {
                        //Lots of things have changed ... use the long sort.
                        Endpoint::longSort(&m_ppEndpoints[axis][0],
                                           &m_ppEndpoints[axis][(m_nHitTests << 1) - 1]);
                    }
                    else
                    {
                        //Only minor changes ... use the short sort.
                        Endpoint::shortSort(&m_ppEndpoints[axis][0],
                                            &m_ppEndpoints[axis][(m_nHitTests << 1) - 1]);
                    }
                }
                m_nAdded = 0;
            }

#ifdef ALLEGIANCE_NATIVE
            for(int axis=0;axis<c_nAxes;++axis)for(int i=0;i<m_nHitTests*2;++i){auto* endpoint=m_ppEndpoints[axis][i];
                nativeEndpoints(axis)[i]=(endpoint->nativeIndex<<1)|uint32_t(endpoint->highF);
            }
            refreshNativeBounds(true);
#endif
#ifdef ALLEGIANCE_NATIVE
            if(m_nativeHasTargets)pivot();else m_pivotAxis=-1;
#else
            pivot();
#endif
        }
        else {
            m_pivotAxis = -1;
#ifdef ALLEGIANCE_NATIVE
            refreshNativeBounds();
#endif
        }
#ifdef ALLEGIANCE_NATIVE
        // Moving trees rebuild each frame, static trees when repivoted. Keep the original topology and
        // reverse leaf order, but query compact contiguous bounds instead of
        // repeatedly following pointers into unrelated hull allocations.
        m_nativeNodes.clear();m_nativeLeaves.clear();
        if(m_nHitTests){
            BoundingBox region;
            for(auto& axis:region.axes){axis.values[0]=-std::numeric_limits<float>::infinity();axis.values[1]=std::numeric_limits<float>::infinity();}
            appendNativeTree(*this,region);
        }
#endif
    }
}

#ifdef ALLEGIANCE_NATIVE
namespace {
void includeNativeBounds(BoundingBox& destination,const BoundingBox& source) {
#ifdef __SSE2__
    _mm_storeu_ps(destination.axes[0].values,_mm_min_ps(_mm_loadu_ps(destination.axes[0].values),_mm_loadu_ps(source.axes[0].values)));
    destination.axes[2].values[0]=std::min(destination.axes[2].values[0],source.axes[2].values[0]);
    destination.axes[2].values[1]=std::max(destination.axes[2].values[1],source.axes[2].values[1]);
#else
    for(int axis=0;axis<c_nAxes;++axis){
        destination.axes[axis].values[0]=std::min(destination.axes[axis].values[0],source.axes[axis].values[0]);
        destination.axes[axis].values[1]=std::max(destination.axes[axis].values[1],source.axes[axis].values[1]);
    }
#endif
}
}
int KDroot::appendNativeTree(const KDnode& source,const BoundingBox& region){
    if(source.m_pivotAxis>=0 && source.m_children[0]->m_nativeHasTargets!=source.m_children[1]->m_nativeHasTargets){
        // A unary target branch repeats its child's clipped bounds. Those
        // bounds already enforce the strict ancestor partition, so materialize
        // only the eligible child while retaining its original visitation order.
        bool high=source.m_children[1]->m_nativeHasTargets;auto clipped=region;
        auto& interval=clipped.axes[source.m_pivotAxis];
        if(high)interval.values[0]=std::max(interval.values[0],source.m_pivotValue);
        else interval.values[1]=std::min(interval.values[1],source.m_pivotValue);
        return appendNativeTree(*source.m_children[high],clipped);
    }
    int id=int(m_nativeNodes.size());m_nativeNodes.emplace_back();
    NativeNode node{{},-1,0,-1,source.m_pivotAxis,source.m_pivotValue};
    if(!source.m_nativeHasTargets){m_nativeNodes[id]=node;return id;}
    node.bounds=source.m_nativeBounds;
    if(source.m_pivotAxis>=0){
        auto lowRegion=region,highRegion=region;int axis=source.m_pivotAxis;float pivot=source.m_pivotValue;
        lowRegion.axes[axis].values[1]=std::min(lowRegion.axes[axis].values[1],pivot);
        highRegion.axes[axis].values[0]=std::max(highRegion.axes[axis].values[0],pivot);
        int lowId=appendNativeTree(*source.m_children[0],lowRegion),highId=appendNativeTree(*source.m_children[1],highRegion);
        const auto& low=m_nativeNodes[lowId];const auto& high=m_nativeNodes[highId];
        node.high=highId;node.count=low.count+high.count;
        if(low.count){node.bounds=low.bounds;if(high.count)includeNativeBounds(node.bounds,high.bounds);}
        else if(high.count)node.bounds=high.bounds;
    }
    else {node.first=int(m_nativeLeaves.size());for(int i=source.m_nHitTests-1;i>=0;--i){
        auto* hit=source.nativeHit(i);
        // Null IDs are cast-only: no query can pass the original id < target.id
        // test against them. Keep their casts and the original tree partitions,
        // but exclude them from the target cache and its conservative bounds.
        if(!hit->m_id)continue;
        auto bounds=m_nativeBoxes[source.m_nativeOrder[i]].bounds;
        // Each duplicate carries its original KD partition. Strict overlap
        // with the clipped interval is equivalent to passing both the target
        // bounds and every ancestor pivot, including boundary contacts.
        for(int axis=0;axis<c_nAxes;++axis){
            bounds.axes[axis].values[0]=std::max(bounds.axes[axis].values[0],region.axes[axis].values[0]);
            bounds.axes[axis].values[1]=std::min(bounds.axes[axis].values[1],region.axes[axis].values[1]);
        }
#ifdef __SSE2__
        auto xy=_mm_loadu_ps(bounds.axes[0].values);
        xy=_mm_xor_ps(_mm_shuffle_ps(xy,xy,_MM_SHUFFLE(2,3,0,1)),_mm_setr_ps(-0.f,0.f,-0.f,0.f));
        _mm_storeu_ps(bounds.axes[0].values,xy);
#endif
        if(!node.count)node.bounds=bounds;else includeNativeBounds(node.bounds,bounds);
        m_nativeLeaves.push_back({bounds,hit,hit->m_id});++node.count;
    }
    }
    m_nativeNodes[id]=node;return id;
}
void KDroot::test(HitTest* hit,CollisionQueue* queue)const{
    if(m_nativeNodes.empty()){KDnode::test(hit,queue);return;}
    auto query=hit->m_boundingBox;
#ifdef __SSE2__
    _mm_storeu_ps(query.axes[0].values,_mm_xor_ps(_mm_loadu_ps(query.axes[0].values),_mm_setr_ps(-0.f,0.f,-0.f,0.f)));
#endif
    testNativeTree(0,hit,queue,query);
}
void KDroot::testNativeTree(int id,HitTest* hit,CollisionQueue* queue,const BoundingBox& query)const{
    // Retain the original strict pivot gating and low/high visitation order.
    // A bounded local stack handles ordinary trees; exceptionally deep trees
    // continue in another invocation without imposing a tree-depth limit.
    int pending[64],count=0;
    for(;;){
        {
        const auto& node=m_nativeNodes[id];
        if(!node.count)goto next;
#ifdef __SSE2__
        if(_mm_movemask_ps(_mm_cmple_ps(_mm_loadu_ps(query.axes[0].values),_mm_loadu_ps(node.bounds.axes[0].values))) ||
            query.axes[2].values[1]<=node.bounds.axes[2].values[0] || query.axes[2].values[0]>=node.bounds.axes[2].values[1])goto next;
#else
        bool separated=false;for(int a=0;a<c_nAxes;++a)separated|=query.axes[a].values[1]<=node.bounds.axes[a].values[0] || query.axes[a].values[0]>=node.bounds.axes[a].values[1];
        if(separated)goto next;
#endif
        if(node.axis>=0){
            float minimum=query.axes[node.axis].values[0];
#ifdef __SSE2__
            if(node.axis<2)minimum=-minimum;
#endif
            bool low=minimum<node.pivot,high=query.axes[node.axis].values[1]>node.pivot;
            if(low){
                if(high){
                    if(count==64){testNativeTree(id,hit,queue,query);goto next;}
                    pending[count++]=node.high;
                }
                ++id;continue;
            }
            if(high){id=node.high;continue;}
            goto next;
        }
        for(int i=node.first;i<node.first+node.count;++i){
            const auto& entry=m_nativeLeaves[i];if(!(hit->m_id<entry.id))continue;
#ifdef __SSE2__
            if(_mm_movemask_ps(_mm_cmpgt_ps(_mm_loadu_ps(query.axes[0].values),_mm_loadu_ps(entry.bounds.axes[0].values)))!=15 ||
                !(query.axes[2].values[1]>entry.bounds.axes[2].values[0] && query.axes[2].values[0]<entry.bounds.axes[2].values[1]))continue;
#else
            bool overlaps=true;for(int a=0;a<c_nAxes;++a)overlaps&=query.axes[a].values[1]>entry.bounds.axes[a].values[0] && query.axes[a].values[0]<entry.bounds.axes[a].values[1];if(!overlaps)continue;
#endif
            auto* other=entry.hit;
            if(other->m_lastTest!=hit && hit->m_noHit!=other && other->m_noHit!=hit){other->m_lastTest=hit;hit->Collide(other,queue);}
        }
        }
    next:
        if(!count)return;
        id=pending[--count];
    }
}
KDroot::NativeCast KDroot::nativeCast(HitTest* hit){
    NativeCast q{hit,hit->m_boundingBox,hit->m_id,hit->m_noHit};
#ifdef __SSE2__
    _mm_storeu_ps(q.bounds.axes[0].values,_mm_xor_ps(_mm_loadu_ps(q.bounds.axes[0].values),_mm_setr_ps(-0.f,0.f,-0.f,0.f)));
#endif
    return q;
}
void KDroot::nativeBatch(const std::vector<NativeCast>& queries,NativeCandidates& out)const{
    out.entries.clear();out.heads.assign(queries.size(),-1);out.tails.assign(queries.size(),-1);
    if(m_nativeNodes.empty() || queries.empty())return;
    if(m_nativeBatchWorkspace.size()<queries.size())m_nativeBatchWorkspace.resize(queries.size());
    for(size_t i=0;i<queries.size();++i)m_nativeBatchWorkspace[i]=int(i);
    size_t top=queries.size();gatherNativeBatch(0,0,int(queries.size()),queries,out,top);
}
void KDroot::gatherNativeBatch(int nodeID,size_t input,int count,const std::vector<NativeCast>& queries,NativeCandidates& out,size_t& top)const{
    const auto& node=m_nativeNodes[nodeID];if(!node.count || !count)return;
    size_t mark=top,low=top,high=top+count;int lowCount=0,highCount=0;
    if(node.axis>=0){top+=size_t(count)*2;if(m_nativeBatchWorkspace.size()<top)m_nativeBatchWorkspace.resize(top);}
    auto accept=[&](int index){const auto& q=queries[index];
        if(node.axis>=0){float minimum=q.bounds.axes[node.axis].values[0];
#ifdef __SSE2__
            if(node.axis<2)minimum=-minimum;
#endif
            if(minimum<node.pivot)m_nativeBatchWorkspace[low+lowCount++]=index;
            if(q.bounds.axes[node.axis].values[1]>node.pivot)m_nativeBatchWorkspace[high+highCount++]=index;
            return;
        }
        for(int leaf=node.first;leaf<node.first+node.count;++leaf){const auto& entry=m_nativeLeaves[leaf];if(!(q.id<entry.id) || entry.hit==q.noHit)continue;
#ifdef __SSE2__
            if(_mm_movemask_ps(_mm_cmpgt_ps(_mm_loadu_ps(q.bounds.axes[0].values),_mm_loadu_ps(entry.bounds.axes[0].values)))!=15 ||
                !(q.bounds.axes[2].values[1]>entry.bounds.axes[2].values[0] && q.bounds.axes[2].values[0]<entry.bounds.axes[2].values[1]))continue;
#else
            bool overlap=true;for(int a=0;a<c_nAxes;++a)overlap&=q.bounds.axes[a].values[1]>entry.bounds.axes[a].values[0] && q.bounds.axes[a].values[0]<entry.bounds.axes[a].values[1];if(!overlap)continue;
#endif
            int candidate=int(out.entries.size());out.entries.push_back({leaf,-1});
            if(out.tails[index]>=0)out.entries[out.tails[index]].next=candidate;else out.heads[index]=candidate;
            out.tails[index]=candidate;
        }
    };
    int i=0;
#ifdef __SSE2__
    // Transpose four XY query boxes so every SIMD lane represents one ray.
    // Node bounds remain in registers across the whole input group.
    const auto* b=node.bounds.axes[0].values;
    for(;i+4<=count;i+=4){int ids[4];for(int lane=0;lane<4;++lane)ids[lane]=m_nativeBatchWorkspace[input+i+lane];
        const auto& q0=queries[ids[0]].bounds;const auto& q1=queries[ids[1]].bounds;
        const auto& q2=queries[ids[2]].bounds;const auto& q3=queries[ids[3]].bounds;
        auto x0=_mm_loadu_ps(q0.axes[0].values),x1=_mm_loadu_ps(q1.axes[0].values),x2=_mm_loadu_ps(q2.axes[0].values),x3=_mm_loadu_ps(q3.axes[0].values);
        _MM_TRANSPOSE4_PS(x0,x1,x2,x3);
        auto reject=_mm_or_ps(_mm_cmple_ps(x0,_mm_set1_ps(b[0])),_mm_cmple_ps(x1,_mm_set1_ps(b[1])));
        reject=_mm_or_ps(reject,_mm_or_ps(_mm_cmple_ps(x2,_mm_set1_ps(b[2])),_mm_cmple_ps(x3,_mm_set1_ps(b[3]))));
        auto z0=_mm_setr_ps(q0.axes[2].values[0],q1.axes[2].values[0],q2.axes[2].values[0],q3.axes[2].values[0]);
        auto z1=_mm_setr_ps(q0.axes[2].values[1],q1.axes[2].values[1],q2.axes[2].values[1],q3.axes[2].values[1]);
        reject=_mm_or_ps(reject,_mm_or_ps(_mm_cmpge_ps(z0,_mm_set1_ps(node.bounds.axes[2].values[1])),_mm_cmple_ps(z1,_mm_set1_ps(node.bounds.axes[2].values[0]))));
        int mask=_mm_movemask_ps(reject);for(int lane=0;lane<4;++lane)if(!(mask&(1<<lane)))accept(ids[lane]);
    }
#endif
    for(;i<count;++i){int index=m_nativeBatchWorkspace[input+i];const auto& q=queries[index].bounds;
#ifdef __SSE2__
        if(_mm_movemask_ps(_mm_cmple_ps(_mm_loadu_ps(q.axes[0].values),_mm_loadu_ps(node.bounds.axes[0].values))) ||
            q.axes[2].values[1]<=node.bounds.axes[2].values[0] || q.axes[2].values[0]>=node.bounds.axes[2].values[1])continue;
#else
        bool separated=false;for(int a=0;a<c_nAxes;++a)separated|=q.axes[a].values[1]<=node.bounds.axes[a].values[0] || q.axes[a].values[0]>=node.bounds.axes[a].values[1];if(separated)continue;
#endif
        accept(index);
    }
    if(node.axis>=0){gatherNativeBatch(nodeID+1,low,lowCount,queries,out,top);gatherNativeBatch(node.high,high,highCount,queries,out,top);}
    top=mark;
}
void KDroot::applyNativeBatch(const NativeCast& q,const NativeCandidates& candidates,int index,CollisionQueue* queue)const{
    for(int item=candidates.heads[index];item>=0;item=candidates.entries[item].next){auto* other=m_nativeLeaves[candidates.entries[item].leaf].hit;
        if(other->m_lastTest!=q.hit && q.hit->m_noHit!=other && other->m_noHit!=q.hit){other->m_lastTest=q.hit;q.hit->Collide(other,queue);}
    }
}

#endif
