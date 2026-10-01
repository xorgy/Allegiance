// InventoryImage's row membership/order, independent of any renderer. Pointers
// are borrowed only for the visit; callers copy observations into owned state.
#pragma once
#include "../Igc/igc.h"
#include <array>
namespace Allegiance {
enum class InventoryRowKind {Part,Empty,Observer,Turret};
struct InventorySlot {
    InventoryRowKind kind;IpartIGC* part{};IshipIGC* gunner{};
    EquipmentType equipment{NA};Mount mount{NA};const char* key{""};const char* location{""};int observers{};
};
template<class Visit>void VisitInventoryRows(IshipIGC* viewer,Mount selectedCargo,Visit visit) {
    auto* ship=viewer->GetSourceShip();const auto* hull=ship->GetHullType();if(!hull)return;
    std::array<IpartIGC*,c_maxCargo> cargo{};
    for(auto* l=ship->GetParts()->first();l;l=l->next()) {
        auto* part=l->data();auto mount=part->GetMountID();if(mount<0 && mount>=-c_maxCargo)cargo[-mount-1]=part;
    }
    const bool pilot=viewer==ship;
    auto process=[&](EquipmentType type,Mount mount,const char* key,IshipIGC* gunner=nullptr,const char* location="") {
        if(!hull->GetPartMask(type,mount))return;
        auto* part=ship->GetMountedPart(type,mount);bool compatible=part!=nullptr;
        if(!compatible)for(auto* l=ship->GetParts()->first();l;l=l->next())if(l->data()->GetEquipmentType()==type && hull->CanMount(l->data()->GetPartType(),mount)){compatible=true;break;}
        if(compatible)visit(InventorySlot{part?InventoryRowKind::Part:InventoryRowKind::Empty,part,gunner,type,mount,key,location});
    };
    constexpr const char* fixedKeys[]={"1.","2.","3.","4."};
    constexpr const char* turretKeys[]={"Y.","U.","I.","O.","","","",""};
    const Mount fixed=hull->GetMaxFixedWeapons();
    for(Mount mount=0;mount<fixed;++mount)process(ET_Weapon,mount,pilot?fixedKeys[mount]:"");
    int observers=0;for(auto* l=ship->GetChildShips()->first();l;l=l->next())observers+=l->data()->GetTurretID()==NA;
    if(observers)visit(InventorySlot{InventoryRowKind::Observer,nullptr,nullptr,NA,NA,"","",observers});
    for(Mount mount=fixed;mount<hull->GetMaxWeapons();++mount) {
        auto* gunner=ship->GetGunner(mount);auto* location=hull->GetHardpointData(mount).locationAbreviation;
        process(ET_Weapon,mount,pilot?turretKeys[mount-fixed]:"",gunner,location);
        // Original ProcessTurret adds a crew line only when it is occupied.
        if(gunner)visit(InventorySlot{InventoryRowKind::Turret,ship->GetMountedPart(ET_Weapon,mount),gunner,ET_Weapon,mount,"",location});
    }
    process(ET_Magazine,0,"5.");process(ET_ChaffLauncher,0,"6.");process(ET_Shield,0,"7.");
    process(ET_Cloak,0,"8.");process(ET_Afterburner,0,"9.");process(ET_Dispenser,0,"0.");
    if(hull->HasCapability(c_habmLifepod))return;
    for(int offset=0;offset<c_maxCargo;++offset) {
        Mount mount=selectedCargo-offset;if(mount<-c_maxCargo)mount+=c_maxCargo;
        auto* part=cargo[-mount-1];auto* type=part?part->GetPartType():nullptr;auto object=part?part->GetObjectType():NA;
        bool show=true;
        if(!part || object==OT_pack || object==OT_magazine || object==OT_dispenser)
            for(Mount previous=mount+1;previous<0;++previous){auto* other=cargo[-previous-1];if(type==(other?other->GetPartType():nullptr)){show=false;break;}}
        if(show)visit(InventorySlot{part?InventoryRowKind::Part:InventoryRowKind::Empty,part,nullptr,NA,mount,offset==0?"S.":"","Cargo"});
    }
}
} // namespace Allegiance
