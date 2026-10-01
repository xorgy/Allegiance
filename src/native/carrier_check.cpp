#include "test_fixture.hpp"
#include "../ClientCore/CarrierLoadout.h"
#include "test_reference/carrier_loadout.hpp"
#include "test_reference/carrier_fallback.hpp"
#include <tuple>
using NativeTest::require;
namespace {
auto inventory(IshipIGC* ship) {
    std::vector<std::tuple<int,int,int,float>> result;
    for(auto* l=ship->GetParts()->first();l;l=l->next()) {
        auto* p=l->data();result.emplace_back(p->GetPartType()->GetObjectID(),p->GetMountID(),p->GetAmount(),p->GetMountedFraction());
    }
    return result;
}
void reset(IshipIGC* ship) {
    while(auto* l=ship->GetParts()->first())l->data()->Terminate();
    ship->SetFraction(.3f);ship->SetAmmo(3);ship->SetFuel(1);ship->SetEnergy(2);
}
void compare(IshipIGC* a,IshipIGC* b) {
    require(inventory(a)==inventory(b),"carrier original part order/type/amount/mount differs");
    require(a->GetFraction()==b->GetFraction() && a->GetAmmo()==b->GetAmmo() && a->GetFuel()==b->GetFuel() && a->GetEnergy()==b->GetEnergy(),"carrier resources differ");
}
void run(NativeTest::Fixture& f) {
    auto* a=f.ship();auto* b=f.ship();auto* gun=f.types(ET_Weapon,a).front();
    auto* shield=f.types(ET_Shield,a).front();auto* burner=f.types(ET_Afterburner,a).front();
    auto* base=static_cast<IlauncherTypeIGC*>(f.types(ET_Magazine,a).front());
    auto missile=*static_cast<const DataMissileTypeIGC*>(base->GetExpendableType()->GetData());
    auto launcher=*static_cast<const DataLauncherTypeIGC*>(base->GetData());
    auto make=[&](int id,int successor,int price,bool available) {
        missile.expendabletypeID=id;missile.launcherDef.price=price;
        missile.launcherDef.ttbmRequired.ClearAll();if(!available)missile.launcherDef.ttbmRequired.SetAll();
        auto* e=f.mission->CreateObject(f.mission->GetLastUpdate(),OT_missileType,&missile,sizeof(missile));require(e,"carrier missile fixture");e->Release();
        launcher.partID=id;launcher.successorPartID=successor;launcher.expendabletypeID=id;
        auto* p=static_cast<IpartTypeIGC*>(f.mission->CreateObject(f.mission->GetLastUpdate(),OT_partType,&launcher,sizeof(launcher)));require(p,"carrier launcher fixture");p->Release();return p;
    };
    auto* upgrade=make(31000,NA,0,true);auto* free=make(31001,31000,0,true);
    auto* paid=make(31002,NA,11,true);auto* blocked=make(31003,NA,0,false);
    int cases=0;
    for(int mask=0;mask<128;++mask)for(int service=0;service<3;++service) {
        reset(a);reset(b);
        for(auto* ship:{a,b}) {
            ship->CreateAndAddPart(shield,0,1);
            if(mask&1)ship->CreateAndAddPart(gun,0,1);
            if(mask&2)ship->CreateAndAddPart(burner,0,1);
            if(mask&4)ship->CreateAndAddPart(free,0,2);
            if(mask&8)ship->CreateAndAddPart(paid,-1,2);
            if(mask&16)ship->CreateAndAddPart(free,-2,1);
            if(mask&32)ship->CreateAndAddPart(blocked,-3,1);
            if(mask&64)ship->CreateAndAddPart(f.mission->GetFuelPack(),-4,1);
        }
        IpartTypeIGC* desired[c_maxCargo+3]{free,nullptr,nullptr,free,free,blocked};
        IpartTypeIGC* saved[c_maxCargo+3];Allegiance::SaveDesiredCarrierLoadout(a,saved);
        require(saved[0]==(mask&4?free:nullptr) && saved[3]==nullptr && saved[4]==(mask&16?free:nullptr) && saved[5]==(mask&32?blocked:nullptr),"desired carrier list preserves only free launchers");
        if(service==2)OriginalCarrier::RearmAtAlliedStation(a,desired);
        else OriginalCarrier::RearmAtCarrier(a,service?desired:nullptr);
        if(service==2)Allegiance::RearmAtAlliedStation(b,desired);
        else Allegiance::RearmAtCarrier(b,service?desired:nullptr);
        compare(a,b);
        if(mask&8)require(b->GetMountedPart(NA,-1)->GetAmount()==2,"carrier must not manufacture paid rounds");
        if(mask&4)require(b->GetMountedPart(ET_Magazine,0)->GetPartType()==upgrade,"carrier upgrades free launchers with available team technology");
        if(service==2 && !(mask&4))require(!b->GetMountedPart(ET_Magazine,0),"allied station incorrectly adopts carrier remounting of a depleted launcher");
        if(mask&32)require(b->GetMountedPart(NA,-3)->GetAmount()==1,"unavailable equipment not refilled");
        ++cases;
    }
    for(int mask=0;mask<256;++mask) {
        reset(a);reset(b);OriginalCarrier::CachedPartList cache;
        std::vector<OriginalCarrier::CachedPart> native;
        for(auto [part,mount]:std::vector<std::pair<IpartTypeIGC*,Mount>>{{free,-5},{gun,-4},{free,-3},{paid,-2},{gun,0},{shield,0},{burner,0},{free,0}}) {
            native.push_back({part,mount,false});auto* l=new OriginalCarrier::CachedPartLink;l->data()=native.back();cache.last(l);
        }
        for(size_t i=0;i<native.size();++i)if(mask&(1<<i))for(auto* ship:{a,b})ship->CreateAndAddPart(native[i].ppt,native[i].mount,1);
        // Shift duplicate reserves to test preference for the exact mount and
        // otherwise reverse cache matching, not merely empty inventory restore.
        if(auto* p=a->GetMountedPart(NA,-5)){p->SetMountID(-1);b->GetMountedPart(NA,-5)->SetMountID(-1);}
        OriginalCarrier::RestoreOffline(a,cache);
        Allegiance::RestoreOfflineCarrierLoadout(b,[&](auto visit){for(auto& p:native)if(!visit(p))break;},[&](auto visit){for(auto i=native.rbegin();i!=native.rend();++i)if(!visit(*i))break;},[](auto& p){return p.ppt;});
        compare(a,b);++cases;
    }
    reset(a);reset(b);for(auto* ship:{a,b})ship->CreateAndAddPart(paid,-3,2);
    Money expectedBudget=a->GetMountedPart(NA,-3)->GetPrice(),originalBudget=-1,nativeBudget=-1;IstationIGC* originalStation=nullptr;IstationIGC* nativeStation=nullptr;
    OriginalCarrier::RestoreWithoutCache(a,[&](IshipIGC*,IstationIGC* station,IhullTypeIGC* hull,Money* budget){require(!hull,"fallback must retain current hull");originalBudget=*budget;originalStation=station;});
    Allegiance::RestoreUncachedCarrierLoadout(b,[&](IshipIGC*,IstationIGC* station,Money* budget){nativeBudget=*budget;nativeStation=station;});
    require(originalStation && nativeStation==originalStation && nativeBudget==originalBudget && nativeBudget==expectedBudget,"uncached carrier station choice and cargo trade-in budget");compare(a,b);++cases;
    while(auto* l=a->GetSide()->GetStations()->first())l->data()->Terminate();
    for(int mask=0;mask<16;++mask)for(bool hasBurner:{false,true})for(bool hasGun:{false,true}) {
        reset(a);reset(b);
        for(auto* ship:{a,b}) {
            if(hasBurner)ship->CreateAndAddPart(burner,0,1);if(hasGun)ship->CreateAndAddPart(gun,0,1);
            for(int i=0;i<4;++i)if(mask&(1<<i))ship->CreateAndAddPart(f.mission->GetFuelPack(),Mount(-5+i),1);
        }
        OriginalCarrier::RestoreWithoutCache(a,[](IshipIGC*,IstationIGC*,IhullTypeIGC*,Money*){throw std::runtime_error("unexpected station in carrier fallback fixture");});
        Allegiance::RestoreUncachedCarrierLoadout(b,[](IshipIGC*,IstationIGC*,Money*){throw std::runtime_error("unexpected station in carrier fallback fixture");});
        compare(a,b);++cases;
    }
    reset(b);b->CreateAndAddPart(burner,0,1);
    for(Mount m=-c_maxCargo;m<0;++m)b->CreateAndAddPart(f.mission->GetFuelPack(),m,1);
    auto count=b->GetParts()->n();Allegiance::RestoreUncachedCarrierLoadout(b,[](IshipIGC*,IstationIGC*,Money*){});
    require(b->GetParts()->n()==count && !b->GetMountedPart(ET_Pack,0),"full cargo must not create an invalid mounted pack");
    printf("%d frozen server/offline carrier and allied-station loadout comparisons passed\n",cases);
}
}
int main(int argc,char** argv) {
    try{NativeTest::Fixture f(argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork());run(f);return 0;}
    catch(const std::exception& e){fprintf(stderr,"carrier: %s\n",e.what());return 1;}
}
