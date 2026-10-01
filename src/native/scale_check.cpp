// Policy equivalence on identical live IGC objects, independent of allocator order.
#include "test_fixture.hpp"
#include "dodge_index.hpp"
#include "scanner_index.hpp"
#include "../Igc/clusterIGC.h"
#include "../Igc/shipIGC.h"
#include "../ClientCore/Visibility.h"
#include <random>
#include <cstring>
#include <bitset>
using NativeTest::require;
namespace {
void endpointOrder(){
    std::mt19937 random(456);std::vector<Endpoint> storage(4096);std::vector<Endpoint*> reference,optimized;
    auto compare=[](Endpoint* a,Endpoint* b){return a->value>b->value?1:a->value<b->value?-1:int(b->highF-a->highF);};
    for(int layout=0;layout<120;++layout){
        int count=layout%3?256:4096;reference.clear();
        for(int i=0;i<count;++i){auto& e=storage[i];e.value=layout%2?float(int(random()%100)-50):float(int(random()%100000)-50000);e.highF=random()%2;reference.push_back(&e);}
        // Temporal coherence, large appended shot groups, and dense tied keys.
        if(layout%4){int appended=layout%4==1?count/4:count/16;
            std::stable_sort(reference.begin(),reference.end()-appended,[&](auto* a,auto* b){return compare(a,b)<0;});}
        optimized=reference;
        // Frozen original backward-search insertion; compare pointer order,
        // not just sorted keys, because ties can affect the KD partition.
        for(int i=1;i<count;++i)if(compare(reference[i-1],reference[i])>0){auto* value=reference[i];int at=i-1;
            while(at>=0 && compare(reference[at],value)>0)--at;
            for(int j=i;j>at+1;--j)reference[j]=reference[j-1];reference[at+1]=value;}
        Endpoint::shortSort(optimized.data(),optimized.data()+count-1);
        require(reference==optimized,"endpoint insertion changes the original pointer/tie order");
    }
}
}
// Frozen original traversal on the same tree and HitTests. Check queue order,
// shapes and times before sorting as well as the final collision ordering.
class NativeScaleCollisionReference {
    static void partitions(const KDnode& node){
        const KDnode* root=&node;while(root->m_parent)root=root->m_parent;
        auto endpoint=[&](const KDnode& from,int axis,int at){auto token=from.nativeEndpoints(axis)[at];return &root->m_ppHitTests[token>>1]->m_endpoints[axis][token&1];};
        for(int i=0;i<node.m_nHitTests;++i)require(node.m_nativeHits==root->m_ppHitTests && root->m_ppHitTests[node.m_nativeOrder[i]]==node.nativeHit(i),"compact KD HitTest indices change pointer order");
        if(!node.m_parent && node.m_nHitTests>=c_minRootSize)for(int a=0;a<c_nAxes;++a)for(int i=0;i<node.m_nHitTests*2;++i){
            require(endpoint(node,a,i)==node.m_ppEndpoints[a][i],"root endpoint tokens change original sort order");
            auto token=node.nativeEndpoints(a)[i];float value=node.m_nativeBoxes[token>>1].bounds.axes[a].values[token&1];
            require(std::memcmp(&value,&node.m_ppEndpoints[a][i]->value,sizeof(value))==0,"compact endpoint value differs from original endpoint");
        }
        bool hasTargets=false;for(int i=0;i<node.m_nHitTests;++i)hasTargets|=node.nativeHit(i)->m_id!=nullptr;
        require(hasTargets==node.m_nativeHasTargets,"KD target eligibility cache disagrees with live IDs");
        require(hasTargets || node.m_pivotAxis<0,"cast-only KD branch was subdivided");
        if(node.m_parent){
            const auto& parent=*node.m_parent;bool high=parent.m_children[1]==&node;int axis=parent.m_pivotAxis;
            auto belongs=[&](HitTest* hit){const auto& box=hit->m_boundingBox.axes[axis];return high?box.values[1]>parent.m_pivotValue:box.values[0]<parent.m_pivotValue;};
            int at=0;for(int i=parent.m_nHitTests-1;i>=0;--i)if(belongs(parent.nativeHit(i))){
                require(at<node.m_nHitTests && node.nativeHit(at++)==parent.nativeHit(i),"compact KD flags change original child membership/order");}
            require(at==node.m_nHitTests,"compact KD flags change child count");
            if(hasTargets && node.m_nHitTests>=c_minNodeSize)for(int a=0;a<c_nAxes;++a){at=0;
                for(int i=0;i<parent.m_nHitTests*2;++i)if(belongs(endpoint(parent,a,i)->pHitTest)){
                    require(at<node.m_nHitTests*2 && endpoint(node,a,at++)==endpoint(parent,a,i),"compact KD flags change original endpoint permutation");}
                require(at==node.m_nHitTests*2,"compact KD flags change endpoint count");
            }
        }
        if(node.m_pivotAxis>=0){partitions(*node.m_children[0]);partitions(*node.m_children[1]);}
    }
    static void test(const KDnode& node,HitTest* query,CollisionQueue& queue) {
        if(node.m_pivotAxis>=0){const auto& interval=query->m_boundingBox.axes[node.m_pivotAxis];
            if(interval.values[0]<node.m_pivotValue)test(*node.m_children[0],query,queue);
            if(interval.values[1]>node.m_pivotValue)test(*node.m_children[1],query,queue);
        } else for(int i=node.m_nHitTests-1;i>=0;--i){auto* hit=node.nativeHit(i);
            if(query->m_id<hit->m_id && hit->m_lastTest!=query && query->m_noHit!=hit && hit->m_noHit!=query){hit->m_lastTest=query;query->Collide(hit,&queue);}}
    }
    static void requireEqual(CollisionQueue& original,CollisionQueue& optimized,bool sort=true){
        require(original.n()==optimized.n(),"collision subtree pruning loses collisions");
        for(int phase=0;phase<(sort?2:1);++phase){if(phase){original.sort(0);optimized.sort(0);}
            for(int i=0;i<original.n();++i){const auto& a=original[i];const auto& b=optimized[i];
                require(a.m_pHitTest1==b.m_pHitTest1 && a.m_pHitTest2==b.m_pHitTest2 && a.m_tCollision==b.m_tCollision && a.m_hts1==b.m_hts1 && a.m_hts2==b.m_hts2,"collision subtree pruning changes original collision order/time/shape");}}
    }
    static void pivots(const KDnode& node,std::vector<const KDnode*>& result){
        if(node.m_pivotAxis<0 || result.size()>=24)return;
        result.push_back(&node);pivots(*node.m_children[0],result);pivots(*node.m_children[1],result);
    }
public:
    static void castOnlyTransitions(ImodelIGC* model,Time now) {
        KDroot root(false);std::vector<TRef<HitTest>> hits;
        for(int i=0;i<512;++i){auto* hit=new HitTest(model,8,false,c_htsSphere);
            hits.emplace_back(hit);hit->Release();hit->SetPosition({float(i%16)*40,float(i/16)*40,0});
            hit->SetBB(now,now+1,1);root.addHitTest(hit);}
        TRef<HitTest> query=new HitTest(model,4,false,c_htsSphere);query->Release();
        query->SetPosition({10,0,0});query->SetDeadF(false);query->SetBB(now,now+1,1);
        for(bool targets:{false,true,false,true}){
            hits[0]->SetID(targets?model:nullptr);hits[17]->SetID(targets?model:nullptr);
            root.update();partitions(root);
            require(root.m_nativeHasTargets==targets,"moving KD root failed to regain/drop target branches");
            CollisionQueue original,optimized;
            for(auto& hit:hits)hit->SetLastTest(nullptr);test(root,query,original);
            for(auto& hit:hits)hit->SetLastTest(nullptr);root.test(query,&optimized);
            requireEqual(original,optimized);
            require(targets?original.n()>0:original.n()==0,"cast-only transition fixture lacks collision coverage");
            original.purge();optimized.purge();
        }
        for(auto& hit:hits)root.deleteHitTest(hit);root.flush();
    }
    static size_t compare(CclusterIGC& cluster,Time start,float dt) {
        for(auto* l=cluster.GetModels()->first();l;l=l->next())l->data()->SetBB(start,start+dt,dt);
        cluster.m_kdrStatic.update();cluster.m_kdrMoving.update();
        partitions(cluster.m_kdrStatic);partitions(cluster.m_kdrMoving);
        CollisionQueue original,optimized,batched;
        for(bool native:{false,true}){
            for(auto* l=cluster.GetModels()->first();l;l=l->next())l->data()->GetHitTest()->SetLastTest(nullptr);
            auto& queue=native?optimized:original;
            for(auto* l=cluster.m_modelsCastRay.first();l;l=l->next()){auto* ht=l->data()->GetHitTest();if(ht->GetDeadF())continue;
                if(native){cluster.m_kdrStatic.test(ht,&queue);cluster.m_kdrMoving.test(ht,&queue);}
                else{test(cluster.m_kdrStatic,ht,queue);test(cluster.m_kdrMoving,ht,queue);}}
        }
        for(auto* l=cluster.GetModels()->first();l;l=l->next())l->data()->GetHitTest()->SetLastTest(nullptr);
        std::vector<KDroot::NativeCast> casts;
        for(auto* l=cluster.m_modelsCastRay.first();l;l=l->next())if(auto* hit=l->data()->GetHitTest();!hit->GetDeadF())casts.push_back(KDroot::nativeCast(hit));
        KDroot::NativeCandidates staticHits,movingHits;
        cluster.m_kdrStatic.nativeBatch(casts,staticHits);cluster.m_kdrMoving.nativeBatch(casts,movingHits);
        for(size_t i=0;i<casts.size();++i){cluster.m_kdrStatic.applyNativeBatch(casts[i],staticHits,int(i),&batched);cluster.m_kdrMoving.applyNativeBatch(casts[i],movingHits,int(i),&batched);}
        size_t count=original.n();requireEqual(original,optimized,false);requireEqual(original,batched,false);
        original.sort(0);optimized.sort(0);batched.sort(0);requireEqual(original,optimized,false);requireEqual(original,batched,false);
        original.purge();optimized.purge();batched.purge();
        // Point casts exactly on a partition boundary, and sphere bounds
        // touching it from either side, must retain the original strict tests.
        std::vector<const KDnode*> partitions;pivots(cluster.m_kdrMoving,partitions);
        for(const auto* node:partitions)for(int offset:{-1,0,1}){
            float radius=offset?1.f:0.f;TRef<HitTest> query=new HitTest(cluster.GetModels()->first()->data(),radius,false,offset?c_htsSphere:c_htsPoint);
            query->Release(); // Transfer the constructor's reference to TRef.
            auto position=node->nativeHit(0)->GetPosition();float coordinate=node->m_pivotValue+float(offset);
            if(node->m_pivotAxis==0)position.x=coordinate;else if(node->m_pivotAxis==1)position.y=coordinate;else position.z=coordinate;
            query->SetPosition(position);query->SetDeadF(false);query->SetBB(start,start+dt,dt);
            for(bool native:{false,true}){
                for(auto* l=cluster.GetModels()->first();l;l=l->next())l->data()->GetHitTest()->SetLastTest(nullptr);
                if(native)cluster.m_kdrMoving.test(query,&optimized);else test(cluster.m_kdrMoving,query,original);
            }
            for(auto* l=cluster.GetModels()->first();l;l=l->next())l->data()->GetHitTest()->SetLastTest(nullptr);
            std::vector<KDroot::NativeCast> packet(4,KDroot::nativeCast(query));KDroot::NativeCandidates hits;
            cluster.m_kdrMoving.nativeBatch(packet,hits);cluster.m_kdrMoving.applyNativeBatch(packet[0],hits,0,&batched);
            requireEqual(original,optimized,false);requireEqual(original,batched,false);
            original.sort(0);optimized.sort(0);batched.sort(0);requireEqual(original,optimized,false);requireEqual(original,batched,false);
            original.purge();optimized.purge();batched.purge();
        }
        return count;
    }
};
static void checkDenseModels(CclusterIGC& cluster) {
    std::vector<ImodelIGC*> dense,original;
    for(size_t i=cluster.m_nativeModels.size();i>0;){--i;if(auto* model=cluster.m_nativeModels[i]){
        dense.push_back(model);auto found=cluster.m_modelLinks.find(model);
        require(found!=cluster.m_modelLinks.end() && found->second.handle==cluster.m_nativeModelHandles[i] &&
            cluster.m_nativeModelPositions[found->second.handle]==i && cluster.m_nativeModelTypes[i]==model->GetObjectType(),
            "dense model handle/type disagrees with live model");
    }}
    for(auto* l=cluster.GetModels()->first();l;l=l->next())original.push_back(l->data());
    require(dense==original,"dense model traversal changes original list order or retains a retired model");
}
int main(){try {
    endpointOrder();
    {
        NativeTest::Fixture f(Native::Assets::steamArtwork(),false);std::bitset<32768> used;
        for(int i=0;i<32768;++i){auto id=f.mission->GenerateNewShipID();require(id>=0 && !used.test(id),"ship allocator wraps into a negative or duplicate identity");used.set(id);}
        require(f.mission->GenerateNewShipID()==NA,"exhausted ship allocator must fail explicitly");
        DataShipIGC data{};data.shipID=0;data.hullID=414;data.sideID=0;data.pilotType=c_ptPlayer;data.baseObjectID=NA;data.partsOffset=sizeof(data);
        auto* ship=static_cast<IshipIGC*>(f.mission->CreateObject(f.mission->GetLastUpdate(),OT_ship,&data,sizeof(data)));require(ship,"reserved ship creation");ship->Terminate();ship->Release();
        require(f.mission->GenerateNewShipID()==0 && f.mission->GenerateNewShipID()==NA,"retired ship identity is not recycled safely");
    }
    NativeTest::Fixture fixture(Native::Assets::steamArtwork());
    std::vector<IshipIGC*> ships;for(int i=0;i<160;++i)ships.push_back(fixture.ship(i%2,414,i%3?c_ptWingman:c_ptPlayer));
    auto* cluster=static_cast<CclusterIGC*>(ships.front()->GetCluster());
    NativeScaleCollisionReference::castOnlyTransitions(ships.front(),fixture.mission->GetLastUpdate());
    std::vector<TRef<IprojectileIGC>> projectiles;
    for(int i=0;i<512;++i){DataProjectileIGC data{};data.projectileTypeID=fixture.mission->GetProjectileTypes()->first()->data()->GetObjectID();data.forward=Vector(0,0,1);data.lifespan=100;
        auto* p=static_cast<IprojectileIGC*>(fixture.mission->CreateObject(fixture.mission->GetLastUpdate(),OT_projectile,&data,sizeof(data)));require(p,"collision projectile fixture");
        projectiles.emplace_back(p);p->Release();p->SetLauncher(ships[i%ships.size()]);p->SetCluster(cluster);}
    std::mt19937 random(734);auto value=[&](float a,float b){return std::uniform_real_distribution<float>(a,b)(random);};
    Allegiance::SideScanners scanners;for(auto* s:ships)scanners.Add(s->GetSide()->GetObjectID(),s);
    size_t dodges=0,paths=0,eyes=0,collisions=0;
    for(int layout=0;layout<60;++layout){
        for(auto* l=fixture.mission->GetSides()->first();l;l=l->next())l->data()->SetAllies(layout%5?NA:0);
        float spread=layout%4?6000.f:180.f;
        for(size_t i=0;i<ships.size();++i){auto* s=ships[i];s->SetPosition({value(-spread,spread),value(-spread,spread),value(-spread,spread)});
            s->SetVelocity(layout%3==0?Vector::GetZero():Vector(value(-400,400),value(-400,400),value(-400,400)));
            s->SetOrientation(Orientation(Vector(value(-1,1),value(-1,1),value(-1,1))));float radius=value(1,100);
            s->SetRadius(layout%5==0?float(10*(i%4+1)):radius);s->SetSignature(value(.1f,4));s->SetFlag(layout%10==0?0:NA);}
        // Overlaps, coasting obstacles, fast approaches, and equal-time ties.
        ships[1]->SetPosition(ships[0]->GetPosition());ships[2]->SetPosition(ships[0]->GetPosition());
        for(size_t i=0;i<projectiles.size();++i){IprojectileIGC* p=projectiles[i];p->SetPosition(ships[(i+17)%ships.size()]->GetPosition()+Vector(value(-100,100),value(-100,100),value(-100,100)));
            p->SetVelocity({value(-1000,1000),value(-1000,1000),value(-1000,1000)});}
        collisions+=NativeScaleCollisionReference::compare(*cluster,fixture.mission->GetLastUpdate(),layout%2?.5f:.006f);
        checkDenseModels(*cluster);
        std::vector<ImodelIGC*> tracked,filtered;
        for(auto* l=cluster->GetModels()->first();l;l=l->next())if(l->data()->GetAttributes()&c_mtSeenBySide)tracked.push_back(l->data());
        for(auto* l=cluster->GetPickableModels()->last();l;l=l->txen())if(l->data()->GetAttributes()&c_mtSeenBySide)filtered.push_back(l->data());
        require(tracked==filtered,"visibility subset changes original tracked model order");
        cluster->m_dodgeIndex.begin();cluster->m_shipDodgeIndex.begin();
        for(auto* s:ships){cluster->GetDodgeCandidates(s,1,false);cluster->GetDodgeCandidates(s,1,true);}
        require(cluster->m_dodgeIndex.rebuilds()<=3 && cluster->m_shipDodgeIndex.rebuilds()<=3,"interleaved player/drone sides repeatedly rebuild spatial snapshots");
        for(size_t i=0;i<ships.size();++i){auto* s=ships[i];auto* ignore=ships[(i+1)%ships.size()];
            for(bool onlyShips:{false,true})for(float horizon:{-1.f,0.f,.01f,1.f,20.f}){int original=0,optimized=0;cluster->m_dodgeIndex.end();cluster->m_shipDodgeIndex.end();bool a=Dodge(s,ignore,&original,onlyShips,horizon);cluster->m_dodgeIndex.begin();cluster->m_shipDodgeIndex.begin();bool b=Dodge(s,ignore,&optimized,onlyShips,horizon);require(a==b && original==optimized,"spatial Dodge changes original decisions");++dodges;}
            auto* target=ships[(i+37)%ships.size()];auto controls=s->GetControls();auto state=s->GetStateM();
            auto run=[&](bool optimized){if(optimized){cluster->m_dodgeIndex.begin();cluster->m_shipDodgeIndex.begin();}else{cluster->m_dodgeIndex.end();cluster->m_shipDodgeIndex.end();}s->SetControls(controls);s->SetStateM(state);GotoPlan plan(s);plan.Set(Waypoint::c_oGoto,target);bool done=plan.Execute(fixture.mission->GetLastUpdate(),1.f/165,true);return std::tuple{done,s->GetStateM(),s->GetControls()};};
            auto a=run(false),b=run(true);require(std::get<0>(a)==std::get<0>(b) && std::get<1>(a)==std::get<1>(b) && std::memcmp(&std::get<2>(a),&std::get<2>(b),sizeof(ControlData))==0,"spatial path planning changes original controls");s->SetControls(controls);s->SetStateM(state);++paths;
        }
        cluster->m_dodgeIndex.end();cluster->m_shipDodgeIndex.end();
        for(int side=0;side<2;++side){NativeScannerIndex index;auto* list=scanners.Get(side);index.begin(list);
            for(auto* model:ships){IscannerIGC* original=nullptr;for(auto* l=list->first();l;l=l->next())if(l->data()->InScannerRange(model)){original=l->data();break;}
                require(index.find(list,model)==original,"scanner index changes original first spotter");++eyes;}
        }
    }
    // Correlated and opposing motion at large coordinates stresses broad-phase
    // cancellation and horizon boundaries without changing the original policy.
    for(int layout=0;layout<12;++layout){
        Vector offset(layout%2?400000.f:-400000.f,200000.f,-300000.f);
        Vector drift(layout%3==0?300.f:0.f,layout%3==1?-250.f:0.f,layout%3==2?200.f:0.f);
        for(size_t i=0;i<ships.size();++i){auto* s=ships[i];
            Vector relative(value(-500,500),value(-500,500),value(-500,500));
            s->SetPosition(offset+relative);s->SetRadius(float(10*(i%4+1)));
            s->SetVelocity(drift+relative*(layout%2?.5f:-.5f));
        }
        for(auto* s:ships)for(bool onlyShips:{false,true})for(float horizon:{-1.f,0.f,.001f,.25f,.5f,1.f,2.f,3.99f,4.f,4.01f,8.f,20.f}){
            int original=0,optimized=0;cluster->m_dodgeIndex.end();cluster->m_shipDodgeIndex.end();
            bool a=Dodge(s,nullptr,&original,onlyShips,horizon);cluster->m_dodgeIndex.begin();cluster->m_shipDodgeIndex.begin();
            bool b=Dodge(s,nullptr,&optimized,onlyShips,horizon);require(a==b && original==optimized,"motion bounds change original decisions at large coordinates");++dodges;
        }
    }
    // Ignore's lower-ID drone rule must react to state, ripcord, identity and
    // allegiance changes inside an already-built spatial phase.
    for(size_t i=0;i<ships.size();++i){ships[i]->SetPosition({20000.f+float(i)*100,20000,20000});ships[i]->SetVelocity(Vector::GetZero());ships[i]->SetStateM(0);}
    auto* own=ships[158];auto* low=ships[2];own->SetPosition({50000,0,0});low->SetPosition(own->GetPosition());
    int mutation=0;auto compareMutation=[&](bool expected){int a=0,b=0;bool indexed=Dodge(own,nullptr,&a,false,1);cluster->m_dodgeIndex.end();bool original=Dodge(own,nullptr,&b,false,1);
        if(indexed!=original || a!=b || indexed!=expected)fprintf(stderr,"mutation %d: indexed/original/expected %d/%d/%d states %d/%d, IDs %d/%d, capabilities %x, mining %d, ripcord %p\n",mutation,indexed,original,expected,a,b,own->GetObjectID(),low->GetObjectID(),own->GetBaseHullType()->GetCapabilities(),bool(low->GetStateM()&miningMaskIGC),static_cast<void*>(low->GetRipcordModel()));++mutation;
        require(indexed==original && a==b && indexed==expected,"spatial drone precedence ignores a live state change");cluster->m_dodgeIndex.begin();cluster->GetDodgeCandidates(own,1);};
    cluster->m_dodgeIndex.begin();cluster->GetDodgeCandidates(own,1);compareMutation(false);
    low->SetStateM(miningMaskIGC);compareMutation(true);low->SetStateM(0);compareMutation(false);
    low->SetRipcordModel(fixture.mission->GetStations()->first()->data());compareMutation(true);low->SetRipcordModel(nullptr);compareMutation(false);
    auto oldID=low->GetObjectID(),newID=fixture.mission->GenerateNewShipID();static_cast<CshipIGC*>(low)->SetShipID(newID);require(fixture.mission->GetShip(newID)==low && !fixture.mission->GetShip(oldID),"native ID lookup rekeys with ship identity");compareMutation(true);static_cast<CshipIGC*>(low)->SetShipID(oldID);require(fixture.mission->GetShip(oldID)==low && !fixture.mission->GetShip(newID),"native ID lookup restores identity");compareMutation(false);
    // SetSide deliberately removes a ship from its cluster in the original
    // client; put it back before checking the changed allegiance.
    low->SetSide(fixture.mission->GetSide(1));low->SetCluster(cluster);compareMutation(false);
    low->SetStateM(miningMaskIGC);low->SetSide(own->GetSide());low->SetCluster(cluster);compareMutation(true);low->SetStateM(0);
    // Mutation invalidates an active snapshot; removed objects cannot survive it.
    cluster->m_dodgeIndex.begin();cluster->m_shipDodgeIndex.begin();cluster->GetDodgeCandidates(ships[0],1);ships[1]->SetVelocity({1,2,3});ships[1]->SetRadius(500);ships[1]->SetPosition(ships[0]->GetPosition());
    int a=0,b=0;bool indexed=Dodge(ships[0],nullptr,&a);cluster->m_dodgeIndex.end();cluster->m_shipDodgeIndex.end();bool original=Dodge(ships[0],nullptr,&b);require(indexed==original && a==b,"active spatial mutation uses a stale snapshot");
    cluster->m_dodgeIndex.begin();cluster->m_shipDodgeIndex.begin();cluster->GetDodgeCandidates(ships[0],1);ships[1]->SetCluster(nullptr);Dodge(ships[0],nullptr,&a);cluster->m_dodgeIndex.end();cluster->m_shipDodgeIndex.end();Dodge(ships[0],nullptr,&b);require(a==b,"spatial removal retains a deleted candidate");scanners.Clear();
    for(auto& p:projectiles)p->Terminate();projectiles.clear();
    checkDenseModels(*cluster);cluster->CompactNativeModels();checkDenseModels(*cluster);
    require(collisions>1000,"collision equivalence fixture lacks dense moving/projectile coverage");
    printf("Scale policy equivalence: %zu Dodge decisions, %zu path controls, %zu original first-spotter queries, %zu ordered collisions and snapshot mutations passed.\n",dodges,paths,eyes,collisions);
}catch(const std::exception& e){fprintf(stderr,"scale check: %s\n",e.what());return 1;}}
