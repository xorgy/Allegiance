// BaseClient weapon cycling and WinTrek's single/group selection policy.
#pragma once
#include "../Igc/igc.h"
namespace Allegiance {
inline Mount CycleWeapon(IshipIGC* ship,Mount selected,bool previous) {
    if(ship->GetParentShip())return selected;
    Mount count=ship->GetHullType()->GetMaxFixedWeapons();
    if(count<=0)return selected;
    // A hull replacement can reduce hardpoints. Bound the cycle even if the
    // caller has retained a selection from that hull.
    if(selected<0 || selected>=count)selected=0;
    Mount start=selected;
    do {
        selected=previous?(selected?selected-1:count-1):(selected+1)%count;
    } while(!ship->GetMountedPart(ET_Weapon,selected) && selected!=start);
    return selected;
}
inline bool CanSelectWeapon(IshipIGC* ship,Mount mount) {
    return !ship->GetParentShip() && mount>=0 && mount<ship->GetHullType()->GetMaxFixedWeapons() && ship->GetMountedPart(ET_Weapon,mount);
}
template<class Select>
SoundID ToggleWeaponSelection(IshipIGC* ship,Mount mount,bool& group,Select select) {
    if(!CanSelectWeapon(ship,mount))return NA;
    Mount selected=(ship->GetStateM()&selectedWeaponMaskIGC)>>selectedWeaponShiftIGC;
    if(group || mount!=selected){select(mount);group=false;return ungroupWeaponsSound;}
    group=true;return groupWeaponsSound;
}
}
