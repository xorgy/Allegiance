#include "../ClientCore/Visibility.h"
#include "../ClientCore/MissionEconomy.h"
#include "test_fixture.hpp"
#include <cstdio>

namespace {
using NativeTest::require;
struct Vision : ThingSite {
    ImodelIGC* model;Allegiance::ModelVisibility visibility;unsigned changes{};
    explicit Vision(ImodelIGC* m):model(m){}
    void Terminate()override{visibility.Clear();}
    void SetCluster(ImodelIGC* m,IclusterIGC* c)override{visibility.ChangeCluster(m);UpdateSideVisibility(m,c);}
    void UpdateSideVisibility(ImodelIGC* m,IclusterIGC* c)override {
        visibility.Update(m,c,[&](IsideIGC*,bool,bool,bool,bool,bool){++changes;});
    }
    bool GetSideVisibility(IsideIGC* s)override{return !(model->GetAttributes()&c_mtSeenBySide)||visibility.Known(s);}
    bool GetCurrentEye(IsideIGC* s)override{return (model->GetAttributes()&c_mtSeenBySide)&&visibility.Eye(s);}
};
struct Sector : ClusterSite {
    Allegiance::SideScanners scanners;
    void Terminate()override{scanners.Clear();}
    void AddScanner(SideID s,IscannerIGC* m)override{scanners.Add(s,m);}
    void DeleteScanner(SideID s,IscannerIGC* m)override{scanners.Delete(s,m);}
    const ScannerListIGC* GetScanners(SideID s)const override{return scanners.Get(s);}
};
struct Fixture : NativeTest::Fixture {
    struct RemovedRock {AsteroidID id;SectorID sector;SideID side;};
    std::vector<RemovedRock> removedRocks;
    explicit Fixture(const std::filesystem::path& art):NativeTest::Fixture(art,false){require(LoadIGCFile("training_6",mission),"load map");}
    TRef<ThingSite> CreateThingSite(ImodelIGC* m)override{return new Vision(m);}
    TRef<ClusterSite> CreateClusterSite(IclusterIGC*)override{return new Sector;}
    void KillAsteroidEvent(AsteroidID id,SectorID sector,IsideIGC* side)override{removedRocks.push_back({id,sector,side->GetObjectID()});}
};
void update(ImodelIGC* m){m->UpdateSeenBySide();}
// Frozen WinTrek fast-path expression (39ef2c07, trekigc.cpp), independent
// of the production predicate. Check both results and alliance revocation.
bool originalEye(IsideIGC* playerSide,ImodelIGC* model,IscannerIGC* scanner,bool allies) {
    return model->GetSide()==playerSide ||
        (allies && model->GetSide() && IsideIGC::AlliedSides(model->GetSide(),playerSide)) ||
        (scanner && scanner->InScannerRange(model) &&
            (scanner->GetSide()==playerSide ||
             (allies && IsideIGC::AlliedSides(scanner->GetSide(),playerSide))));
}
void checks(Fixture& f) {
    auto* a=f.ship(0);auto* b=f.ship(1);auto* ally=f.ship(2);
    auto* c=a->GetCluster();auto* other=c->GetMission()->GetClusters()->last()->data();
    // Isolate scanners from authored station/probe coverage. Empty the sector's
    // static obstacles too so range tests aren't accidentally shadow tests.
    while(auto* l=c->GetStations()->first())l->data()->Terminate();
    while(auto* l=c->GetAsteroids()->first())l->data()->Terminate();
    auto* sa=a->GetSide();auto* sb=b->GetSide();auto* sc=ally->GetSide();
    a->SetPosition(Vector(0,0,0));a->SetOrientation(Orientation(Vector(0,0,-1)));
    ally->SetPosition(Vector(100000,0,0));
    float range=a->GetHullType()->GetScannerRange()*b->GetSignature()/sb->GetGlobalAttributeSet().GetAttribute(c_gaSignature)+a->GetRadius()+b->GetRadius();
    b->SetPosition(Vector(0,0,-range*.75f));update(b);
    require(b->SeenBySide(sa)&&b->GetCurrentEye(sa),"front scanner detects enemy");
    require(c->GetClusterSite()->GetScanners(0)->n()==1,"ship scanner registered once");
    b->SetPosition(Vector(0,0,range*.75f));update(b);
    require(!b->SeenBySide(sa)&&!b->GetCurrentEye(sa),"rear scanner range penalty");
    b->SetPosition(Vector(0,0,-range*.75f));b->SetCloaking(.01f);update(b);
    require(!b->SeenBySide(sa),"cloaking reduces signature and detection range");
    b->SetCloaking(1);update(b);require(b->SeenBySide(sa),"uncloak reacquires");
    // Use a real asteroid to check the original angular occlusion calculation.
    require(other->GetAsteroids()->first(),"occlusion fixture asteroid");
    TRef<IasteroidIGC> rock=other->GetAsteroids()->first()->data();
    rock->SetCluster(c);rock->SetPosition(b->GetPosition()*.5f);rock->SetRadius(200);
    update(b);require(!b->SeenBySide(sa),"asteroid shadows scanner contact");
    rock->SetCluster(other);update(b);require(b->SeenBySide(sa),"contact reacquired after occluder leaves");
    a->SetPosition(Vector(100000,0,0));ally->SetPosition(Vector(0,0,0));ally->SetOrientation(Orientation(Vector(0,0,-1)));
    sa->SetAllies(3);sc->SetAllies(3);
    auto params=*f.mission->GetMissionParams();params.bAllowAlliedViz=true;f.mission->SetMissionParams(&params);
    update(b);require(b->SeenBySide(sa),"allied scanner shares contact");
    params.bAllowAlliedViz=false;f.mission->SetMissionParams(&params);update(b);
    require(!b->SeenBySide(sa),"disabled allied sight invalidates cached spotter");
    params.bAllowAlliedViz=true;f.mission->SetMissionParams(&params);update(b);
    sc->SetAllies(NA);update(b);require(!b->SeenBySide(sa),"revoked alliance invalidates cached spotter");
    for(bool allowed:{false,true})for(auto* viewer:{sa,sb,sc})for(auto* scanner:{a,b,ally}) {
        require(originalEye(viewer,b,scanner,allowed)==Allegiance::CachedSideEye(viewer,b,scanner,allowed),"original WinTrek fast path");
        require(originalEye(viewer,b,nullptr,allowed)==Allegiance::CachedSideEye(viewer,b,nullptr,allowed),"original fast path without spotter");
    }
    a->SetPosition(Vector(0,0,0));update(b);b->SetCluster(other);
    require(!b->SeenBySide(sa),"moving enemy loses contact across sectors");
    require(c->GetClusterSite()->GetScanners(1)->n()==0,"old sector unregisters scanner");
    require(other->GetClusterSite()->GetScanners(1)->find(b),"new sector registers scanner");
    b->SetCluster(c);update(b);require(b->SeenBySide(sa),"returning scanner contact");
    // A discovered static object persists without current eye.
    auto* warp=c->GetWarps()->first()->data();a->SetPosition(warp->GetPosition()+Vector(0,0,20));update(warp);
    require(warp->SeenBySide(sa)&&warp->GetCurrentEye(sa),"static object discovered");
    a->SetCluster(other);update(warp);
    require(warp->SeenBySide(sa)&&!warp->GetCurrentEye(sa),"static knowledge retained without eye");
    // Cached references must not keep a terminated scanner usable or leak cycles.
    a->SetCluster(c);a->SetPosition(Vector(0,0,0));update(b);a->Terminate();update(b);
    require(!b->SeenBySide(sa),"terminated cached scanner rejected");
    // Stations and probes are registered by their IGC objects, independently of
    // the player's ship. A remote scanner must actually contribute to sight.
    require(f.mission->GetStations()->first(),"station scanner fixture");
    auto* station=f.mission->GetStations()->first()->data();station->SetSide(sa);
    b->SetCluster(station->GetCluster());b->SetPosition(station->GetPosition()+Vector(0,0,station->GetRadius()+100));update(b);
    require(b->GetCurrentEye(sa),"station scanner provides eye");
    b->SetCluster(c);b->SetPosition(Vector(0,0,100));
    IprobeTypeIGC* probeType=nullptr;
    for(auto* l=f.mission->GetExpendableTypes()->first();l;l=l->next())if(l->data()->GetObjectType()==OT_probeType) {
        auto* p=static_cast<IprobeTypeIGC*>(l->data());if(p->GetScannerRange()>500){probeType=p;break;}
    }
    require(probeType,"authored scanner probe");
    DataProbeIGC data{};data.pprobetype=probeType;data.time0=f.mission->GetLastUpdate();
    data.p0=Vector(0,0,0);data.pside=sa;data.pcluster=c;data.probeID=f.mission->GenerateNewProbeID();
    TRef<IprobeIGC> probe=static_cast<IprobeIGC*>(f.mission->CreateObject(data.time0,OT_probe,&data,sizeof(data)));
    require(probe,"create scanner probe");probe->Release();update(b);
    require(b->GetCurrentEye(sa),"probe scanner provides eye");
    probe->Terminate();update(b);require(!b->GetCurrentEye(sa),"destroyed probe removes eye");
    // Exercise ClusterIGC's original retained-asteroid notification after an
    // unseen construction. Only presentation retains the old rock; the physical
    // asteroid has been removed, and the replacement station remains unobserved.
    for(auto* l=other->GetStations()->first();l;l=l->next())l->data()->SetPosition(Vector(0,0,1000000));
    auto* scout=f.ship(0);scout->SetCluster(other);
    scout->SetPosition(Vector(0,0,100000));rock->SetPosition(Vector(0,0,0));rock->SetRadius(200);rock->SetSignature(2);
    auto* replacement=Allegiance::ConstructStation(rock,sb,station->GetBaseStationType(),f.mission->GetLastUpdate());
    require(replacement,"station replaces retained rock");
    replacement->SetRoidID(rock->GetObjectID());replacement->SetRoidPos(rock->GetPosition());replacement->SetRoidSig(rock->GetSignature());
    replacement->SetRoidRadius(rock->GetRadius());replacement->SetRoidCaps(rock->GetCapabilities());replacement->SetRoidSide(0);
    replacement->SetSignature(0);replacement->SetRadius(1);rock->Terminate();
    scout->SetPosition(Vector(0,0,-1000));scout->SetOrientation(Orientation(Vector(0,0,1)));
    static_cast<Vision*>(replacement->GetThingSite())->visibility.Set(sa,false,false);
    update(replacement);require(!replacement->SeenBySide(sa),"replacement station outside actual scanner range");
    f.mission->SetMissionStage(STAGE_STARTED);f.mission->Update(f.mission->GetLastUpdate()+.1f);
    require(std::any_of(f.removedRocks.begin(),f.removedRocks.end(),[&](auto& r){return r.id==replacement->GetRoidID() && r.side==0;}),"IGC removes retained rock when its old site is scanned");
    require(!replacement->GetRoidSide(0),"stale-rock notification only occurs once per side");replacement->Release();
    printf("Visibility: range, rear penalty, cloak, occlusion, station/probe scanners, allied policy/revocation, static memory, sector transfer and teardown passed.\n");
}
}
int main(int argc,char** argv) {
    try{Fixture f(argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork());checks(f);return 0;}
    catch(const std::exception& e){fprintf(stderr,"visibility: %s\n",e.what());return 1;}
}
