#include "game.hpp"
#include "test_fixture.hpp"
#include "../ClientCore/InventoryRows.h"
#include "../ClientCore/InventoryHud.h"
#include "test_reference/inventory.hpp"
#include <algorithm>
#include <cmath>
using NativeTest::require;
namespace {
// InventoryImage::Rebuild/ProcessPart/ProcessCargo/ProcessTurret frozen as a
// descriptor visitor. Deliberately retain the original independent scans.
std::vector<Allegiance::InventorySlot> originalRows(IshipIGC* viewer,Mount selected) {
    using namespace Allegiance;std::vector<InventorySlot> rows;auto* ship=viewer->GetSourceShip();auto* hull=ship->GetHullType();bool pilot=viewer==ship;
    auto process=[&](EquipmentType type,Mount mount,const char* key,IshipIGC* gunner=nullptr,const char* location="") {
        if(!hull || (mount>=0 && !hull->GetPartMask(type,mount)))return;
        auto* part=ship->GetMountedPart(type,mount);bool has=part!=nullptr;
        if(!part && mount>=0)for(auto* l=ship->GetParts()->first();l;l=l->next())if(l->data()->GetEquipmentType()==type && hull->CanMount(l->data()->GetPartType(),mount)){has=true;break;}
        if(has)rows.push_back({part?InventoryRowKind::Part:InventoryRowKind::Empty,part,gunner,type,mount,key,location});
    };
    constexpr const char* fixedKeys[]={"1.","2.","3.","4."};constexpr const char* turretKeys[]={"Y.","U.","I.","O.","","","",""};
    Mount mount=0;for(;mount<hull->GetMaxFixedWeapons();++mount)process(ET_Weapon,mount,pilot?fixedKeys[mount]:"");
    int observers=0;for(auto* l=ship->GetChildShips()->first();l;l=l->next())if(l->data()->GetTurretID()==NA)++observers;
    if(observers)rows.push_back({InventoryRowKind::Observer,nullptr,nullptr,NA,NA,"","",observers});
    for(;mount<hull->GetMaxWeapons();++mount){auto* gunner=ship->GetGunner(mount);auto* location=hull->GetHardpointData(mount).locationAbreviation;
        process(ET_Weapon,mount,pilot?turretKeys[mount-hull->GetMaxFixedWeapons()]:"",gunner,location);
        if(gunner)rows.push_back({InventoryRowKind::Turret,ship->GetMountedPart(ET_Weapon,mount),gunner,ET_Weapon,mount,"",location});}
    process(ET_Magazine,0,"5.");process(ET_ChaffLauncher,0,"6.");process(ET_Shield,0,"7.");process(ET_Cloak,0,"8.");process(ET_Afterburner,0,"9.");process(ET_Dispenser,0,"0.");
    if(!hull || hull->HasCapability(c_habmLifepod))return rows;
    for(int offset=0;offset<c_maxCargo;++offset){Mount cargo=selected-offset;if(cargo<-c_maxCargo)cargo+=c_maxCargo;
        auto* part=ship->GetMountedPart(NA,cargo);auto* type=part?part->GetPartType():nullptr;auto object=part?part->GetObjectType():NA;bool show=true;
        if(!part || object==OT_pack || object==OT_magazine || object==OT_dispenser)for(Mount previous=cargo+1;previous<0;++previous){auto* other=ship->GetMountedPart(NA,previous);if(type==(other?other->GetPartType():nullptr)){show=false;break;}}
        if(show)rows.push_back({part?InventoryRowKind::Part:InventoryRowKind::Empty,part,nullptr,NA,cargo,offset==0?"S.":"","Cargo"});}
    return rows;
}
size_t compareRows(IshipIGC* ship) {
    size_t count=0;for(Mount selected=-1;selected>=-c_maxCargo;--selected){auto reference=originalRows(ship,selected);std::vector<Allegiance::InventorySlot> actual;
        Allegiance::VisitInventoryRows(ship,selected,[&](auto row){actual.push_back(row);});require(actual.size()==reference.size(),"original inventory row count");
        for(size_t i=0;i<actual.size();++i){const auto& a=actual[i];const auto& b=reference[i];require(a.kind==b.kind && a.part==b.part && a.gunner==b.gunner && a.equipment==b.equipment && a.mount==b.mount && a.observers==b.observers && std::string_view(a.key)==b.key && std::string_view(a.location)==b.location,"original inventory row membership/order/keys/crew");}++count;}
    return count;
}
void equal(float actual,float expected){require(actual==expected || (std::isnan(actual) && std::isnan(expected)),"frozen original inventory statistic differs");}
void comparePart(IpartIGC* part,IshipIGC* viewer) {
    using namespace Allegiance;equal(InventoryPartCount(part),OriginalInventory::GetCount(part));equal(InventoryOutOfAmmo(part),OriginalInventory::IsOutOfAmmo(part));
    switch(part->GetEquipmentType()) {
    case ET_Weapon:equal(InventoryWeaponRate(part),OriginalInventory::GetRate(part));[[fallthrough]];
    case ET_Magazine:equal(InventoryWeaponRange(part,viewer),OriginalInventory::GetRange(part,viewer));equal(InventoryPartDamage(part,viewer),OriginalInventory::GetDamage(part,viewer));break;
    case ET_Shield:equal(InventoryShieldStrength(part),OriginalInventory::GetShieldStrength(part));equal(InventoryShieldRechargeTime(part),OriginalInventory::GetRechargeTime(part));break;
    case ET_Afterburner:equal(InventoryAfterburnerTopSpeed(part,viewer),OriginalInventory::GetAfterburnerTopSpeed(part,viewer));equal(InventoryAfterburnerTimeLeft(part,viewer),OriginalInventory::GetAfterburnerTimeLeft(part,viewer));break;
    case ET_Cloak:equal(InventoryCloakTimeLeft(part,viewer),OriginalInventory::GetCloakTimeLeft(part,viewer));break;
    }
}
}
int main(int argc,char** argv)try {
    require(argc>1,"ARTWORK required");NativeTest::Fixture fixture(argv[1]);size_t rowCases=0,partCases=0,hulls=0;IshipIGC* crewParent=nullptr;
    for(auto* h=fixture.mission->GetHullTypes()->first();h;h=h->next()) {
        auto* hull=h->data();if(hull->GetGroupID()<0 || hull->HasCapability(c_habmLifepod))continue;
        auto* ship=fixture.ship(0,hull->GetObjectID());++hulls;rowCases+=compareRows(ship);
        for(EquipmentType type=0;type<=ET_Afterburner;++type)if(type!=ET_Pack){int mounts=type==ET_Weapon?hull->GetMaxWeapons():1;
            for(Mount mount=0;mount<mounts;++mount)for(auto* p=fixture.mission->GetPartTypes()->first();p;p=p->next())if(p->data()->GetEquipmentType()==type && hull->CanMount(p->data(),mount)){auto* part=ship->CreateAndAddPart(p->data(),mount,3);require(part,"create mounted inventory part");part->SetMountedFraction(1);part->Arm();break;}}
        rowCases+=compareRows(ship);
        for(auto* p=ship->GetParts()->first();p;p=p->next()) {comparePart(p->data(),ship);++partCases;}
        for(int mode=0;mode<3;++mode) {
            for(Mount m=-1;m>=-c_maxCargo;--m)if(auto* p=ship->GetMountedPart(NA,m))p->Terminate();
            IpartTypeIGC* cargoType=nullptr;for(auto* p=fixture.mission->GetPartTypes()->first();p;p=p->next())if((mode==0?p->data()->GetEquipmentType()==ET_Pack:mode==1?p->data()->GetEquipmentType()==ET_Weapon:p->data()->GetEquipmentType()==ET_Magazine)){cargoType=p->data();break;}
            require(cargoType,"cargo type fixture");
            for(Mount m=-1;m>=-3;--m){auto* p=ship->CreateAndAddPart(cargoType,m,short(2-m));require(p,"create cargo duplicate");}
            rowCases+=compareRows(ship);
            for(Mount m=-1;m>=-3;--m){comparePart(ship->GetMountedPart(NA,m),ship);++partCases;}
        }
        if(!crewParent && hull->GetMaxWeapons()>hull->GetMaxFixedWeapons())crewParent=ship;
    }
    require(crewParent,"turret hull fixture");auto* observer=fixture.ship();observer->SetParentShip(crewParent);rowCases+=compareRows(crewParent);rowCases+=compareRows(observer);
    auto* gunner=fixture.ship();gunner->SetParentShip(crewParent);gunner->SetTurretID(crewParent->GetHullType()->GetMaxFixedWeapons());rowCases+=compareRows(crewParent);rowCases+=compareRows(gunner);
    gunner->SetParentShip(nullptr);observer->SetParentShip(nullptr);
    auto* pod=fixture.ship(0,fixture.mission->GetCivilization(25)->GetLifepod()->GetObjectID());rowCases+=compareRows(pod);
    printf("Inventory policy: %zu hulls, %zu frozen row/selection/stack/empty/crew cases, %zu frozen part-statistic comparisons passed\n",hulls,rowCases,partCases);
    Native::SessionOptions options;options.enemies=0;Native::Game game(argv[1],[](auto&,auto&){return Native::ModelHandle{1,1};},0,0,options);
    auto* ship=game.playerShip();game.step({},0);require(!game.state().inventory.empty(),"flight inventory snapshot");
    size_t used=0;Allegiance::VisitInventoryRows(ship,game.state().selectedCargo,[&](auto slot){require(used<game.state().inventory.size(),"inventory snapshot row count");const auto& row=game.state().inventory[used++];require(row.key==slot.key && row.location==slot.location,"snapshot preserves original row keys/location");if(slot.part){require(row.name==slot.part->GetPartType()->GetName(),"snapshot part identity");equal(row.count,OriginalInventory::GetCount(slot.part));}});
    require(used==game.state().inventory.size(),"inventory snapshot does not append physical cargo duplicates");
    auto* resident=game.state().inventory.data();game.step({},0);require(game.state().inventory.data()==resident,"unchanged inventory snapshot reuses row storage");
    game.nextCargo();require(game.state().inventory.front().mdl.size()>0,"cargo cycle retains compiled namespace metadata");
    game.recoverToHangar();
    puts("Live IGC inventory snapshot: original row order/quantities/namespace metadata and stable storage passed");return 0;
}catch(const std::exception& e){fprintf(stderr,"Inventory check: %s\n",e.what());return 1;}
