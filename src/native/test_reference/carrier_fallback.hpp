// BaseClient::RestoreLoadoutOnCarrier no-cache branch at
// 39ef2c072b1eac0e9f97ab1428bc96572cbd5854. Tested only where the
// original cargo traversal remains in bounds; saturation is tested separately.
#pragma once
namespace OriginalCarrier {
template<class Buy> void RestoreWithoutCache(IshipIGC* pship,Buy BuyDefaultLoadout) {
    auto* m_pCoreIGC=pship->GetMission();
            IstationIGC* pstation = NULL;
            for (StationLinkIGC* l = pship->GetSide()->GetStations()->first(); l != NULL; l = l->next())
                if (l->data()->GetStationType()->GetCapabilities() & c_sabmLand) {
                    pstation = l->data();
                    break;
                }
            if (pstation) {
                //Money for the cargo
                Money   money = Money(0);
                Mount cargo = -c_maxCargo;
                while (cargo < 0) {
                    IpartIGC* pp = pship->GetMountedPart(NA, cargo++);
                    if (pp)
                        money += pp->GetPrice();
                }

                BuyDefaultLoadout(pship, pstation, NULL, &money);
            }
            else {
                debugf(" Just add fuel and ammo\n");

                IpartTypeIGC*   pptFuel = NULL;;
                if (pship->GetMountedPart(ET_Afterburner, 0) != NULL)
                    pptFuel = m_pCoreIGC->GetFuelPack();

                IpartTypeIGC*   pptAmmo = NULL;
                {
                    IweaponIGC* pweapon = (IweaponIGC*)(pship->GetMountedPart(ET_Weapon, 0));
                    if (pweapon && (pweapon->GetAmmoPerShot() > 0))
                        pptAmmo = m_pCoreIGC->GetAmmoPack();
                }

                int fuelCount = 0;
                Mount cargo = -c_maxCargo;
                while (cargo < 0) {
                    if (pship->GetMountedPart(NA, cargo) && pship->GetMountedPart(NA, cargo)->GetPartType() == pptFuel)
                        fuelCount++;
                    cargo++;
                }
                
                cargo = -c_maxCargo;
                while (cargo < 0) {
                    while (pship->GetMountedPart(NA, cargo) != NULL)
                        cargo++;
                    if (pptFuel && fuelCount < 3)
                        pship->CreateAndAddPart(pptFuel, cargo, 0x7fff);
                    else if (pptAmmo)
                        pship->CreateAndAddPart(pptAmmo, cargo, 0x7fff);
                    cargo++;
                }
            }

            const PartListIGC*  pparts = pship->GetParts();
            for (PartLinkIGC* l = pparts->first(); (l != NULL); l = l->next())
            {
                IpartIGC*       ppart = l->data();

                ppart->SetAmount(SHRT_MAX); //Things that don't care about amounts wont care if I try and set it.
            }
}
}
