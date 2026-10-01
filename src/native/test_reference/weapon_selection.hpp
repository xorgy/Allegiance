#pragma once
#include "../game.hpp"
#include "../test_fixture.hpp"

namespace OriginalWeaponTest {
// Equip a sparse three-hardpoint loadout in a real mission, without changing
// the selection policy under test. Slot one can be absent between two guns.
inline void fit(Native::Game& game,IshipIGC* ship,unsigned mask) {
    using NativeTest::require;
    IhullTypeIGC* hull=nullptr;
    for(auto* l=game.core()->GetHullTypes()->first();l;l=l->next())if(l->data()->GetMaxFixedWeapons()>=3){hull=l->data();break;}
    require(hull,"weapon fixture needs three fixed hardpoints");
    while(auto* l=ship->GetParts()->first())l->data()->Terminate();ship->SetBaseHullType(hull);
    for(Mount mount=0;mount<3;++mount)if(mask&(1u<<mount)){
        IpartTypeIGC* weapon=nullptr;
        for(auto* l=game.core()->GetPartTypes()->first();l;l=l->next())if(l->data()->GetEquipmentType()==ET_Weapon && hull->CanMount(l->data(),mount)){weapon=l->data();break;}
        auto* part=weapon?ship->CreateAndAddPart(weapon,mount,1):nullptr;
        require(part,"weapon fixture equipped hardpoint");part->SetMountedFraction(1);
    }
    ship->SetAmmo(hull->GetMaxAmmo());ship->SetEnergy(hull->GetMaxEnergy());
}
inline int next(unsigned mask,int selected,bool previous) {
    // The original BaseClient walks equipped hardpoints cyclically and leaves
    // fGroupFire alone. Compute expectations from the ordered occupied slots.
    std::vector<int> equipped;for(int slot=0;slot<3;++slot)if(mask&(1u<<slot))equipped.push_back(slot);
    auto at=std::find(equipped.begin(),equipped.end(),selected)-equipped.begin();
    return equipped[(at+equipped.size()+(previous?-1:1))%equipped.size()];
}
inline void hud(const Native::Game& game,int selected,bool group) {
    using NativeTest::require;
    require(game.state().selectedWeapon==(group?-1:selected),"original grouped/single HUD selection");
    for(const auto& part:game.state().equipment)if(part.fixedWeapon)
        require(part.selected==(group || part.mount==selected),"inventory highlights disagree with selected fire mode");
}
inline bool cue(int sound){return sound==groupWeaponsSound || sound==ungroupWeaponsSound;}
inline int cues(const Native::Game& game) {
    return std::count_if(game.sounds().begin(),game.sounds().end(),[](const auto& sound){return cue(sound.id);});
}
}
