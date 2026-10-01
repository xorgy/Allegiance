/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	KDnode.cpp
**
**  Author: 
**
**  Description:
**
**  History:
*/
#include "pch.h"
#if defined(ALLEGIANCE_NATIVE) && defined(__SSE2__)
#include <xmmintrin.h>
namespace {
bool nativeBoxesSeparated(const BoundingBox& query,const BoundingBox& bounds) {
    // XY lanes are {-query.min, query.max} and {-node.max, node.min}.
    return _mm_movemask_ps(_mm_cmple_ps(_mm_loadu_ps(query.axes[0].values),_mm_loadu_ps(bounds.axes[0].values))) ||
           query.axes[2].values[1]<=bounds.axes[2].values[0] ||
           query.axes[2].values[0]>=bounds.axes[2].values[1];
}
bool nativeBoxesOverlap(const BoundingBox& query,const BoundingBox& bounds) {
    auto b=_mm_loadu_ps(bounds.axes[0].values);
    b=_mm_xor_ps(_mm_shuffle_ps(b,b,_MM_SHUFFLE(2,3,0,1)),_mm_setr_ps(-0.f,0.f,-0.f,0.f));
    return _mm_movemask_ps(_mm_cmpgt_ps(_mm_loadu_ps(query.axes[0].values),b))==15 &&
           query.axes[2].values[1]>bounds.axes[2].values[0] &&
           query.axes[2].values[0]<bounds.axes[2].values[1];
}
}
#endif

//Allocate space for the node's hitTest and endpoint arrays
//(as a single allocation)
void    KDnode::allocHitTestsEndpoints(int n
#ifdef ALLEGIANCE_NATIVE
                                     ,bool endpoints
#endif
                                     )
{
#ifdef ALLEGIANCE_NATIVE
    if(m_parent){
        if(m_maxHitTests<n || (endpoints && !m_nativeEndpointStorage)){
            m_nativeOrder.reset(new uint32_t[n*(endpoints?1+2*c_nAxes:1)]);
            m_maxHitTests=n;m_nativeEndpointStorage=endpoints;
        }
        return;
    }
#endif
    if (m_maxHitTests < n)
    {
        //Not enough space ... realloc
        delete [] m_ppHitTests;
#ifdef ALLEGIANCE_NATIVE
        // Children use compact endpoint tokens; only the root needs the
        // original pointer arrays for its unchanged endpoint sorting.
        m_nativeOrder.reset(new uint32_t[n*(1+2*c_nAxes)]);
        m_nativeEndpointStorage=true;
        void** v = new void* [n*(1+2*c_nAxes)];
#else
        void**  v = new void* [n * (1 + 2 * c_nAxes)];
#endif
        assert (v);

        m_maxHitTests = n;
        m_ppHitTests = (HitTest**)v;
        m_ppEndpoints[0] = (Endpoint**)(v + n);

        for (int i = 1; (i < c_nAxes); i++)
            m_ppEndpoints[i] = m_ppEndpoints[i - 1] + (n << 1);
    }
}

KDnode::KDnode(KDnode*          parent)
:
    m_parent(parent),
    m_pivotAxis(-1),
    m_pivotValue(0.0f),
    m_nHitTests(0),
    m_maxHitTests(0),
    m_ppHitTests(NULL)
{
    m_ppEndpoints[0] = NULL;
    m_children[0] = m_children[1] = NULL;
}

KDnode::~KDnode(void)
{
    delete m_children[1];
    delete m_children[0];
    delete [] m_ppHitTests;
}

void    KDnode::reset(bool             highF)
{
    assert (m_parent);

    //Set asside enough space for the same number of hitTests as the parent
    //(pessimistic, but fast)
#ifndef ALLEGIANCE_NATIVE
    allocHitTestsEndpoints(m_parent->m_nHitTests);
#endif

    int     pivotAxis = m_parent->m_pivotAxis;
    assert (pivotAxis >= 0);
    assert (pivotAxis < c_nAxes);
    float   pivotValue = m_parent->m_pivotValue;
#ifdef ALLEGIANCE_NATIVE
    m_nativeBoxes=m_parent->m_nativeBoxes;m_nativeFlags=m_parent->m_nativeFlags;m_nativeHits=m_parent->m_nativeHits;
    m_nativeHasTargets=false;
#endif

    //Extract and mark the subset of the parent's hitTests that belong to this node
    {
        m_nHitTests = 0;
#ifdef ALLEGIANCE_NATIVE
        // Membership is the low endpoint below the pivot, or the high endpoint
        // above it. Find that boundary once in the parent's sorted token array;
        // mark dense byte flags instead of reading every object's bounds.
        for(int i=0;i<m_parent->m_nHitTests;++i)m_nativeFlags[m_parent->m_nativeOrder[i]]&=2;
        const auto* endpoints=m_parent->nativeEndpoints(pivotAxis);int size=m_parent->m_nHitTests*2;
        int first=0,last=size;
        while(first<last){int middle=first+(last-first)/2;auto token=endpoints[middle];
            float value=m_nativeBoxes[token>>1].bounds.axes[pivotAxis].values[token&1];
            if(highF?value<=pivotValue:value<pivotValue)first=middle+1;else last=middle;
        }
        int members=0;
        for(int i=highF?first:0,end=highF?size:first;i<end;++i){auto token=endpoints[i];
            if(bool(token&1)==highF){auto& flags=m_nativeFlags[token>>1];flags|=1;++members;m_nativeHasTargets|=bool(flags&2);}
        }
        // Count from the same qualifying endpoints before reserving storage.
        // Leaves need only membership indices; internal nodes additionally
        // need their three sorted endpoint arrays. Capacity survives frames.
        allocHitTestsEndpoints(members,members>=c_minNodeSize && m_nativeHasTargets);
        // Original member order is retained for construction and its oracle;
        // the query cache materializes only eligible targets at leaves.
        for(int parentID=m_parent->m_nHitTests-1;parentID>=0;--parentID){
            auto index=m_parent->m_nativeOrder[parentID];auto flags=m_nativeFlags[index];
            if(flags&1)m_nativeOrder[m_nHitTests++]=index;
        }
#else
        for (HitTest** ppHitTest = &m_parent->m_ppHitTests[m_parent->m_nHitTests - 1];
             (ppHitTest >= m_parent->m_ppHitTests);
             ppHitTest--)
        {
            HitTest* pHitTest = *ppHitTest;
            assert (pHitTest);
            if (highF)
            {
                //Creating the high child ... anything extending above the pivot value is accepted
                if (pHitTest->m_boundingBox.axes[pivotAxis].values[1] > pivotValue)
                {
                    pHitTest->m_activeF = true;
                    m_ppHitTests[m_nHitTests++] = pHitTest;
                }
                else
                    pHitTest->m_activeF = false;
            }
            else
            {
                //Creating the low child ... anything extending below the pivot value is accepted
                if (pHitTest->m_boundingBox.axes[pivotAxis].values[0] < pivotValue)
                {
                    pHitTest->m_activeF = true;
                    m_ppHitTests[m_nHitTests++] = pHitTest;
                }
                else
                    pHitTest->m_activeF = false;
            }
        }

#endif

        assert (m_nHitTests != 0);
        assert (m_nHitTests != m_parent->m_nHitTests);
    }

#ifdef ALLEGIANCE_NATIVE
    // Null-ID objects cast rays but can never pass query.id < target.id.
    // Such a branch returns nothing regardless of how it would partition;
    // retain its membership but avoid sorting and recursively building it.
    if(!m_nativeHasTargets){m_pivotAxis=-1;return;}
#endif

    if (m_nHitTests >= c_minNodeSize)
    {
        //Generate the sorted endpoint lists for this node
        //use the fact that the parent's endpoint lists are sorted
        //and that the endpoint lists for this node will be a subset
        //(consisting of only the endpoints for the "active" hitTests)
        for (int axis = 0; (axis < c_nAxes); axis++)
        {
            //Work backwards to so the conditionals test vs. a non-equation.
            int thisID = (m_nHitTests << 1) - 1;

            //Note: the condition tests thisID so that the loop will stop as soon as
            //the endpoint list is filled (the remaining endpoints in the parent's
            //endpoint list must not be active).
            for (int parentID = (m_parent->m_nHitTests << 1) - 1;
                 (thisID >= 0);
                 parentID--)
            {
                assert (parentID >= 0);

#ifdef ALLEGIANCE_NATIVE
                auto token=m_parent->nativeEndpoints(axis)[parentID];
                if(m_nativeFlags[token>>1]&1)nativeEndpoints(axis)[thisID--]=token;
#else
                auto* endpoint=m_parent->m_ppEndpoints[axis][parentID];
                if(endpoint->pHitTest->m_activeF)m_ppEndpoints[axis][thisID--]=endpoint;
#endif
            }
            //There's no need to sort the endpoint list (since is was sorted in the parent).
        }

#ifdef ALLEGIANCE_NATIVE
        refreshNativeBounds(true);
#endif
        pivot();
    }
    else {
        m_pivotAxis = -1;
#ifdef ALLEGIANCE_NATIVE
        refreshNativeBounds();
#endif
    }
}

#ifdef ALLEGIANCE_NATIVE
template<class Box>
static bool findPivot(int nHitTests,const uint32_t* ppEndpoints,const Box* boxes,int axis,float* pValue,int* pCost)
#else
static bool findPivot(int               nHitTests,
                      Endpoint* const * ppEndpoints,
                      float*            pValue,
                      int*              pCost)
#endif
{
    assert (nHitTests >= c_minNodeSize);
    assert (ppEndpoints);
    assert (pValue);
    assert (pCost);

    bool    optimalF = false;

    int nEndpoints = nHitTests << 1;

    //costBias is used to guarantee that the pivot is worthwhile. Must be
    //in the range [nHitTests, nHitTests * 2].
    int costBias = (5 * nHitTests) >> 2;

    //This is used to generate the minCost (below) and test for the when it's time to quit
    int bestBaseCost = costBias - (nHitTests << 1);

    //Theoretical lowest cost for this node ... account for the fact that it is impossible to
    //perfectly balance a node with an odd number of hitTests
    int minCost = bestBaseCost + (nHitTests & 0x01);   //fast mod 2

    //The endpoints start with a low and end with a high, meaning we can skip the first endpoint
    //in the loop (hence the pre-increment) and only test for exit on the high endpoints.
#ifdef ALLEGIANCE_NATIVE
    assert(!(ppEndpoints[0]&1));assert(ppEndpoints[(nHitTests<<1)-1]&1);
#else
    assert (!ppEndpoints[0]->highF);
    assert (ppEndpoints[(nHitTests << 1) - 1]->highF);
#endif

    int nLow = 0;               //Number of intervals completely below of the current endpoint
    int nHigh = nHitTests - 1;   //Number of intervales completely above the current endpoint

    //Only pivots with a cost less than zero will be considered by pivot, so set the best cost to
    //zero so the only the positives costs pivots will be ignored.
    float   bestValue = 0.0f;  // was 0, changed to 0.0f
    int     bestCost = 0;

    while (true)
    {
        assert (nLow < nHitTests);
#ifdef ALLEGIANCE_NATIVE
        auto token=*(++ppEndpoints);
        struct Value {bool highF;const float& value;} endpoint{bool(token&1),boxes[token>>1].bounds.axes[axis].values[token&1]};
        const auto* pEndpoint=&endpoint;
#else
        const Endpoint* pEndpoint = *(++ppEndpoints);
#endif

        if (pEndpoint->highF)
        {
            //crossed a high endpoint, meaning there is one less
            //interval completely below the current point
            nLow++;

            //Calculate the unbalanced and totals here since this will give a lower
            //overall cost than calculating it before nLow is incremented.
            int unbalanced = nLow - nHigh;
            int baseCost = costBias - ((nLow + nHigh) << 1);

            //thisCost == base + |unbalanced| ... just do it fast
            int thisCost = (unbalanced >= 0)
                           ? (baseCost + unbalanced)
                           : (baseCost - unbalanced);

            if (thisCost < bestCost)
            {
                bestCost = thisCost;
                bestValue = pEndpoint->value;

                if (bestCost == minCost)
                {
                    optimalF = true;
                    break;
                }
            }
            else if ((bestCost <= bestBaseCost + unbalanced) ||
                     (nLow == nHitTests))
            {
                //Since unbalanced only increases, we can give up when the best cost
                //is <= the best cost we can hope to achieve

                //we can also give up if we've reached the end of the endpoint list
                break;
            }

        }
        else
        {
            //No need to calculate costs ... either the previous endpoint was a right
            //endpoint (and the cost wouldn't change since nHigh would be decremented
            //after caclulating the cost) or it was a left endpoint (in which case the
            //cost could only be higher than it was before).

            //Crossed a low endpoint, meaning there is one less
            //interval completely above the current point
            nHigh--;
        }
    }

    *pCost = bestCost;
    *pValue = bestValue;

    return optimalF;
}

void    KDnode::pivot(void)
{
    //Select the best pivot axis and value for this node
    //based on the endpoint data

    assert (m_nHitTests >= c_minNodeSize);
    //Rotate the axes to encourage (in the event of a tie for best cost)
    //that children pivot on different axes than their parents.
    int     startAxis = m_parent ? ((m_parent->m_pivotAxis + 1) % c_nAxes) : 0;

    int     bestAxis = startAxis;
    float   bestValue;
    int     bestCost;

    bool optimalF = findPivot(m_nHitTests,
#ifdef ALLEGIANCE_NATIVE
                              nativeEndpoints(bestAxis),m_nativeBoxes,bestAxis,
#else
                              m_ppEndpoints[bestAxis],
#endif
                              &bestValue,
                              &bestCost);

    for (int i = 1; ((!optimalF) && (i < c_nAxes)); i++)
    {
        int     axis = (i + startAxis) % c_nAxes;
        float   value;
        int     cost;
        
        optimalF = findPivot(m_nHitTests,
#ifdef ALLEGIANCE_NATIVE
                             nativeEndpoints(axis),m_nativeBoxes,axis,
#else
                             m_ppEndpoints[axis],
#endif
                             &value, &cost);

        if (cost < bestCost)
        {
            bestAxis = axis;
            bestValue = value;
            bestCost = cost;
        }
    }

    if (bestCost < 0)
    {
        //Found a pivot value that did some good ... repivot on the new pivot point
        m_pivotAxis = bestAxis;
        m_pivotValue = bestValue;

        if (!m_children[0])
        {
            assert (!m_children[1]);

            //This node has no children ... create some
            m_children[0] = new KDnode(this);
            m_children[1] = new KDnode(this);
        }

        assert (m_children[0] && m_children[1]);

        //Reset both children based on the new pivot axis/value.
        m_children[0]->reset(false);
        m_children[1]->reset(true);
    }
    else
        m_pivotAxis = -1;
}

#ifdef ALLEGIANCE_NATIVE
void KDnode::refreshNativeBounds(bool sortedEndpoints)
{
    if(!m_nHitTests)return;
    if(sortedEndpoints){
        // Filtering preserves endpoint order, so its extremes are also the
        // exact union bounds. Avoid another object scan at every tree level.
        for(int a=0;a<c_nAxes;++a){
            auto low=nativeEndpoints(a)[0],high=nativeEndpoints(a)[(m_nHitTests<<1)-1];
            m_nativeBounds.axes[a].values[0]=m_nativeBoxes[low>>1].bounds.axes[a].values[low&1];
            m_nativeBounds.axes[a].values[1]=m_nativeBoxes[high>>1].bounds.axes[a].values[high&1];
        }
    }else{
        m_nativeBounds=m_nativeBoxes[m_nativeOrder[0]].bounds;
        for(int i=1;i<m_nHitTests;++i)for(int a=0;a<c_nAxes;++a){const auto& b=m_nativeBoxes[m_nativeOrder[i]].bounds.axes[a];auto& bound=m_nativeBounds.axes[a];
            bound.values[0]=std::min(bound.values[0],b.values[0]);bound.values[1]=std::max(bound.values[1],b.values[1]);}
    }
#ifdef __SSE2__
    auto xy=_mm_loadu_ps(m_nativeBounds.axes[0].values);
    xy=_mm_xor_ps(_mm_shuffle_ps(xy,xy,_MM_SHUFFLE(2,3,0,1)),_mm_setr_ps(-0.f,0.f,-0.f,0.f));
    _mm_storeu_ps(m_nativeBounds.axes[0].values,xy);
#endif
}
#endif

void   KDnode::test(HitTest*        pHitTest1,
                    CollisionQueue* pQueue) const
{
#if defined(ALLEGIANCE_NATIVE) && defined(__SSE2__)
    BoundingBox query=pHitTest1->m_boundingBox;
    auto xy=_mm_xor_ps(_mm_loadu_ps(query.axes[0].values),_mm_setr_ps(-0.f,0.f,-0.f,0.f));
    _mm_storeu_ps(query.axes[0].values,xy);
    testNative(pHitTest1,pQueue,query);
}
void KDnode::testNative(HitTest* pHitTest1,CollisionQueue* pQueue,const BoundingBox& query)const
{
#endif
#ifdef ALLEGIANCE_NATIVE
    if(!m_nHitTests || !m_nativeHasTargets)return;
    // The original pivot traversal and leaf order stay intact. Reject only
    // subtrees whose complete swept boxes cannot pass HitTest::Collide's box test.
#ifdef __SSE2__
    if(nativeBoxesSeparated(query,m_nativeBounds))return;
#else
    for(int a=0;a<c_nAxes;++a){const auto& query=pHitTest1->m_boundingBox.axes[a];const auto& bound=m_nativeBounds.axes[a];
        if(query.values[1]<=bound.values[0] || query.values[0]>=bound.values[1])return;}
#endif
#endif
    if (m_pivotAxis >= 0)
    {
        //Pass the hit tests along to one or both children
        const Interval&  interval = pHitTest1->m_boundingBox.axes[m_pivotAxis];

        if (interval.values[0] < m_pivotValue)
#if defined(ALLEGIANCE_NATIVE) && defined(__SSE2__)
            m_children[0]->testNative(pHitTest1,pQueue,query);
#else
            m_children[0]->test(pHitTest1,pQueue);
#endif

        if (interval.values[1] > m_pivotValue)
#if defined(ALLEGIANCE_NATIVE) && defined(__SSE2__)
            m_children[1]->testNative(pHitTest1,pQueue,query);
#else
            m_children[1]->test(pHitTest1,pQueue);
#endif
    }
    else
    {
        //this is a terminal node: test bb against each contained hitTest's bounding box
#ifdef ALLEGIANCE_NATIVE
        for(int i=m_nHitTests-1;i>=0;--i){
            HitTest* pHitTest2=nativeHit(i);
#else
        for (HitTest** ppHitTest = &m_ppHitTests[m_nHitTests - 1];
             (ppHitTest >= m_ppHitTests);
             ppHitTest--)
        {
            HitTest*    pHitTest2 = *ppHitTest;
#endif
            if ((pHitTest1->m_id < pHitTest2->m_id) &&
#ifdef ALLEGIANCE_NATIVE
#ifdef __SSE2__
                nativeBoxesOverlap(query,pHitTest2->m_boundingBox) &&
#else
                (pHitTest1->m_boundingBox.axes[0].values[1] > pHitTest2->m_boundingBox.axes[0].values[0]) &&
                (pHitTest1->m_boundingBox.axes[0].values[0] < pHitTest2->m_boundingBox.axes[0].values[1]) &&
                (pHitTest1->m_boundingBox.axes[1].values[1] > pHitTest2->m_boundingBox.axes[1].values[0]) &&
                (pHitTest1->m_boundingBox.axes[1].values[0] < pHitTest2->m_boundingBox.axes[1].values[1]) &&
                (pHitTest1->m_boundingBox.axes[2].values[1] > pHitTest2->m_boundingBox.axes[2].values[0]) &&
                (pHitTest1->m_boundingBox.axes[2].values[0] < pHitTest2->m_boundingBox.axes[2].values[1]) &&
#endif
#endif
                (pHitTest2->m_lastTest != pHitTest1) &&
                (pHitTest1->m_noHit != pHitTest2) &&
                (pHitTest2->m_noHit != pHitTest1))
            {
                pHitTest2->m_lastTest = pHitTest1;
                pHitTest1->Collide(pHitTest2, pQueue);
            }
        }
    }
}
