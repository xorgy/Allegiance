// Original FedSrv carrier rearm; stack-only, no transport or player wrappers.
#pragma once
#include "../Igc/igc.h"
namespace Allegiance {
inline void RefillPart(IshipIGC* pship, IpartIGC* ppart, const TechTreeBitMask&   ttbm)
{
    IpartTypeIGC*   pptBase = ppart->GetPartType();
    if (pptBase->GetRequiredTechs() <= ttbm)
    {
        IpartTypeIGC*   ppt = pptBase;

        while (true)
        {
            IpartTypeIGC*   pptNext = ppt->GetSuccessorPartType();
            if (pptNext && (pptNext->GetRequiredTechs() <= ttbm))
                ppt = pptNext;
            else
                break;
        }

        short   newAmount = (ppart->GetPrice() == 0)
                            ? SHRT_MAX
                            : ppart->GetAmount();

        if (ppt == pptBase)
        {
            //Things that don't care about amounts wont care if I try and set it.
            ppart->SetAmount(newAmount);
        }
        else
        {
            Mount   oldMount = ppart->GetMountID();
            ppart->Terminate();
            pship->CreateAndAddPart(ppt, oldMount, newAmount);
        }
    }
}

inline bool MatchPT(IpartTypeIGC* ppt1, IpartTypeIGC* ppt2)
{
    do
    {
        if (ppt1 == ppt2)
            return true;

        ppt1 = ppt1->GetSuccessorPartType();
    }
    while (ppt1);

    return false;
}

inline void ErasePart(IshipIGC* pship, EquipmentType et, Mount mount, IpartTypeIGC*  ptDesired[c_maxCargo + 3])
{
    IpartIGC*   ppart = pship->GetMountedPart(et, mount);
    if (ppart)
    {
        IpartTypeIGC*   ppt = ppart->GetPartType();
        //Go through the list of parts and nuke one that corresponds to this part
        for (int i = 0; (i < c_maxCargo + 3); i++)
        {
            if (ptDesired[i] && MatchPT(ptDesired[i], ppt))
            {
                ptDesired[i] = NULL;
                break;
            }
        }
    }
}

inline void SaveDesiredCarrierLoadout(IshipIGC* ship,IpartTypeIGC** desired) {
    const EquipmentType launchers[]={ET_Magazine,ET_Dispenser,ET_ChaffLauncher};
    for(int i=0;i<3;++i){auto* part=ship->GetMountedPart(launchers[i],0);desired[i]=part && part->GetPrice()==0?part->GetPartType():NULL;}
    for(Mount i=0;i<c_maxCargo;++i){auto* part=ship->GetMountedPart(NA,-1-i);desired[i+3]=part && part->GetPrice()==0 && IlauncherIGC::IsLauncher(part->GetObjectType())?part->GetPartType():NULL;}
}
// FedSrv AlliedRearm shares the refill/cargo policy, but predates the carrier
// change that remounts depleted launchers. Retain that distinction explicitly.
template<bool RestoreMountedLaunchers>
inline void RearmFreeEquipment(IshipIGC* pship,IpartTypeIGC* const* desired) {
    ImissionIGC*    pMission = pship->GetMission();
    TechTreeBitMask  ttbm = pship->GetSide()->GetDevelopmentTechs() | pship->GetSide()->GetBuildingTechs();

    //Now ... go through the ship and restore the expendables that are
    //free and partially gone
    {
        //refill parts backwards so that newly added parts will not be revisited
        PartLinkIGC*    ppl = pship->GetParts()->last();
        while (ppl)
        {
            IpartIGC*   ppart = ppl->data();
            ppl = ppl->txen();

            if (ppart)
                RefillPart(pship, ppart, ttbm);
        }
    }

    if (desired) {
        //Copy the list of desired parts
        IpartTypeIGC*   ptDesired[c_maxCargo + 3];      //Must be kept sync'd with
        memcpy(ptDesired, desired, sizeof(ptDesired));

        //Go through and fill any holes with the desired parts
        {
            ErasePart(pship, ET_Magazine, 0, ptDesired);
            ErasePart(pship, ET_Dispenser, 0, ptDesired);
            ErasePart(pship, ET_ChaffLauncher, 0, ptDesired);
            for (Mount i = 0; (i < c_maxCargo); i++)
                ErasePart(pship, NA, -1 - i, ptDesired);
        }

        {
            IpartTypeIGC*   ptFuel = (pship->GetMountedPart(ET_Afterburner, 0) != NULL)
                ? pMission->GetFuelPack()
                : NULL;

            IpartTypeIGC*   ptAmmo = NULL;
            {
                for (Mount i = pship->GetHullType()->GetMaxWeapons() - 1; (i >= 0); i--)
                {
                    IweaponIGC* pw = (IweaponIGC*)(pship->GetMountedPart(ET_Weapon, i));
                    if (pw && (pw->GetAmmoPerShot() != 0))
                    {
                        ptAmmo = pMission->GetAmmoPack();
                        break;
                    }
                }
            }

            bool    bBuyFuel = true;

            // Remount launcher parts if they are depleted AaronMoore 1/10 (Imago)
            if (RestoreMountedLaunchers)for (int i = 0; i < 3; i++)
            {
                if ((i == 0 && pship->GetMountedPart(ET_Magazine, 0) == NULL) ||
                    (i == 1 && pship->GetMountedPart(ET_Dispenser, 0) == NULL) ||
                    (i == 2 && pship->GetMountedPart(ET_ChaffLauncher, 0) == NULL))
                {
                    IpartTypeIGC*   pt = ptDesired[i];

                    if (pt)
                    {
                        // Upgrade part if posible
                        while (true)
                        {
                            IpartTypeIGC*   ptNext = pt->GetSuccessorPartType();
                            if (ptNext && (ptNext->GetRequiredTechs() <= ttbm))
                                pt = ptNext;
                            else
                                break;
                        }
                        pship->CreateAndAddPart(pt, 0, SHRT_MAX);

                        // Remove this part from the desired list so it is not added again into cargo
                        ptDesired[i] = NULL;
                    }
                }
            }
            //end AaaronMoore 1/10


            int     iDesired = 0;
            for (Mount i = 0; (i < c_maxCargo); i++)
            {
                IpartIGC*   ppart = pship->GetMountedPart(NA, -1 - i);
                if (ppart == NULL)
                {
                    //Shove an instance of the first part left in the
                    //desired part list into the slot
                    while (iDesired < c_maxCargo + 3)
                    {
                        IpartTypeIGC*   pt = ptDesired[iDesired++];

                        if (pt && (pt->GetRequiredTechs() <= ttbm))
                        {
                            while (true)
                            {
                                IpartTypeIGC*   ptNext = pt->GetSuccessorPartType();
                                if (ptNext && (ptNext->GetRequiredTechs() <= ttbm))
                                    pt = ptNext;
                                else
                                    break;
                            }

                            ppart = pship->CreateAndAddPart(pt, -1 - i, SHRT_MAX);
                            assert(ppart->GetPrice() == 0);
                            break;
                        }
                    }

                    if (ppart == NULL)
                    {
                        if (bBuyFuel && ptFuel)
                        {
                            pship->CreateAndAddPart(ptFuel, -1 - i, SHRT_MAX);
                            bBuyFuel = (ptAmmo == NULL);
                        }
                        else if (ptAmmo)
                        {
                            pship->CreateAndAddPart(ptAmmo, -1 - i, SHRT_MAX);
                            bBuyFuel = true;
                        }
                    }
                }
            }
        }
    }

    pship->SetAmmo(SHRT_MAX);
    pship->SetFuel(FLT_MAX);
    pship->SetEnergy(pship->GetHullType()->GetMaxEnergy());
    pship->SetFraction(1.0f);
    {
        IshieldIGC* pshield = (IshieldIGC*)(pship->GetMountedPart(ET_Shield, 0));
        if (pshield)
            pshield->SetFraction(1.0f);
    }

}

inline void RearmAtCarrier(IshipIGC* ship,IpartTypeIGC* const* desired) {
    RearmFreeEquipment<true>(ship,desired);
}
inline void RearmAtAlliedStation(IshipIGC* ship,IpartTypeIGC* const* desired) {
    RearmFreeEquipment<false>(ship,desired);
}

// Offline no-cache fallback. The caller supplies its existing default-purchase
// transaction; traversal, cargo budget and fallback packs remain shared.
template<class BuyDefault>
inline void RestoreUncachedCarrierLoadout(IshipIGC* pship,BuyDefault buyDefault) {
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

                buyDefault(pship, pstation, &money);
            }
            else {


                IpartTypeIGC*   pptFuel = NULL;;
                if (pship->GetMountedPart(ET_Afterburner, 0) != NULL)
                    pptFuel = pship->GetMission()->GetFuelPack();

                IpartTypeIGC*   pptAmmo = NULL;
                {
                    IweaponIGC* pweapon = (IweaponIGC*)(pship->GetMountedPart(ET_Weapon, 0));
                    if (pweapon && (pweapon->GetAmmoPerShot() > 0))
                        pptAmmo = pship->GetMission()->GetAmmoPack();
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
                    while (cargo < 0 && pship->GetMountedPart(NA, cargo) != NULL)
                        cargo++;
                    if (cargo == 0) break; // full cargo must never become a mounted pack
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

// BaseClient::RestoreLoadoutOnCarrier cached branch. Traversal adapters retain
// Windows' linked cache and native contiguous storage without conversion.
template<class Forward,class Backward,class PartType>
inline void RestoreOfflineCarrierLoadout(IshipIGC* ship,Forward forward,Backward backward,PartType type) {
    forward([](auto& cached){cached.bDuplicated=false;return true;});
    for(auto* l=ship->GetParts()->first();l;l=l->next()) {
        auto* part=l->data();part->SetAmount(SHRT_MAX);bool found=false;bool* wrong=NULL;
        backward([&](auto& cached) {
            if(!cached.bDuplicated && type(cached)==part->GetPartType()) {
                if(cached.mount==part->GetMountID()){cached.bDuplicated=true;found=true;return false;}
                wrong=&cached.bDuplicated;
            }
            return true;
        });
        if(!found && wrong)*wrong=true;
    }
    Mount cargo=-c_maxCargo;
    auto skipFull=[&](){while(cargo<0 && ship->GetMountedPart(NA,cargo))++cargo;};skipFull();
    forward([&](auto& cached) {
        auto* part=type(cached);
        if(!cached.bDuplicated && part) {
            auto et=part->GetEquipmentType();Mount mount=cached.mount;
            if(mount>=0 && !ship->GetMountedPart(et,mount) && ship->GetHullType()->CanMount(part,mount))
                ship->CreateAndAddPart(part,mount,part->GetAmount(ship));
            else if(cargo<0){ship->CreateAndAddPart(part,cargo++,SHRT_MAX);skipFull();}
        }
        return true;
    });
}
}
