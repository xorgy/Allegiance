// Members of Game::Impl. Only snapshot IGC observations; MDL formats them.
void refreshInventory() {
    if(state.dead || state.docked){state.inventory.clear();return;}
    size_t used=0;
    Allegiance::VisitInventoryRows(player,selectedCargo,[&](const Allegiance::InventorySlot& slot) {
        if(used==state.inventory.size())state.inventory.emplace_back();auto& row=state.inventory[used++];
        row.name.clear();row.gunner.clear();row.key=slot.key;row.location=slot.location;row.observers=slot.observers;
        row.range=row.damage=row.rate=row.count=row.mounted=row.armed=row.afterburnSpeed=row.afterburnTime=row.shieldStrength=row.rechargeTime=row.cloakTime=0;
        row.readiness=0;row.selected=row.active=row.outOfAmmo=false;
        auto* part=slot.part;
        if(slot.kind==Allegiance::InventoryRowKind::Observer)row.mdl="invobserver";
        else if(slot.kind==Allegiance::InventoryRowKind::Turret){row.mdl="invturret";row.gunner=slot.gunner->GetName();}
        else if(!part){if(slot.mount<0)row.mdl="invemptycargo";else {row.mdl="invEmpty";row.mdl+=IpartTypeIGC::GetEquipmentTypeName(slot.equipment);}}
        else {
            row.mdl=part->GetPartType()->GetInventoryLineMDLName();row.name=part->GetPartType()->GetName();
            row.count=Allegiance::InventoryPartCount(part);row.active=part->fActive();row.outOfAmmo=Allegiance::InventoryOutOfAmmo(part);
            if(part->GetMountID()>=0)row.mounted=part->GetMountedFraction();
            if(IlauncherIGC::IsLauncher(part->GetObjectType()) && part->GetMountID()>=0) {
                auto* launcher=static_cast<IlauncherIGC*>(part);row.armed=launcherReady(launcher,static_cast<IlauncherTypeIGC*>(part->GetPartType())->GetExpendableType());
            }
            row.readiness=Allegiance::EquipmentReadyState(part,[&](IlauncherIGC*){return row.armed;});
            switch(part->GetEquipmentType()) {
            case ET_Weapon: {
                auto* weapon=static_cast<IweaponIGC*>(part);
                row.selected=part->GetMountID()>=0 && (state.passenger?weapon->GetGunner()==player:part->GetMountID()<player->GetHullType()->GetMaxFixedWeapons() && (groupFire || part->GetMountID()==selectedWeapon));
                row.rate=Allegiance::InventoryWeaponRate(part);
                [[fallthrough]];
            }
            case ET_Magazine:row.range=Allegiance::InventoryWeaponRange(part,player);row.damage=Allegiance::InventoryPartDamage(part,player);break;
            case ET_Shield:row.shieldStrength=Allegiance::InventoryShieldStrength(part);row.rechargeTime=Allegiance::InventoryShieldRechargeTime(part);break;
            case ET_Cloak:row.cloakTime=Allegiance::InventoryCloakTimeLeft(part,player);break;
            case ET_Afterburner:row.afterburnSpeed=Allegiance::InventoryAfterburnerTopSpeed(part,player);row.afterburnTime=Allegiance::InventoryAfterburnerTimeLeft(part,player);break;
            }
        }
        for(char& ch:row.mdl)if(ch>='A' && ch<='Z')ch+=char('a'-'A');
    });
    state.inventory.resize(used);
}
