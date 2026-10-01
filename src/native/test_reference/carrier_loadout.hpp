// Frozen FedSrv.CPP at 39ef2c0; desired-array parameter replaces the CFSPlayer wrapper.
#pragma once
namespace OriginalCarrier {
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
// Frozen AlliedRearm equipment policy from FedSrv, before factoring.
inline void RearmAtAlliedStation(IshipIGC* pship,IpartTypeIGC* const* desired) {
    ImissionIGC*    pMission = pship->GetMission();
    TechTreeBitMask  ttbm = pship->GetSide()->GetDevelopmentTechs() | pship->GetSide()->GetBuildingTechs();

    //Now ... go through the players ship and restore the expendables that are
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
                        assert (ppart->GetPrice() == 0);
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

inline void RearmAtCarrier(IshipIGC* pship,IpartTypeIGC* const* desired) {
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
            for (int i = 0; i < 3; i++)
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
struct CachedPart { IpartTypeIGC* ppt; Mount mount; bool bDuplicated; };
using CachedPartLink=Slink_utl<CachedPart>;
using CachedPartList=Slist_utl<CachedPart>;
inline void RestoreOffline(IshipIGC* pship,CachedPartList& cache) {
            {
                //Mark all the parts in the cached loadout as not duplicated
                for (CachedPartLink* l = cache.first(); (l != NULL); l = l->next())
                    l->data().bDuplicated = false;
            }

            //Mark parts that are mounted as duplicated and replenish them
            {
                const PartListIGC*  pparts = pship->GetParts();
                for (PartLinkIGC* l = pparts->first(); (l != NULL); l = l->next())
                {
                    IpartIGC*       ppart = l->data();
                    Mount           mount = ppart->GetMountID();

                    IpartTypeIGC*   ppt = ppart->GetPartType();
                    ppart->SetAmount(SHRT_MAX); //Things that don't care about amounts wont care if I try and set it.

                    //Mark the corresponding part in the cached loadout as having been duplicated
                    {
                        bool found = false;
                        CachedPartLink* lWrongMountMatch = NULL;
                        for (CachedPartLink* l = cache.last(); (l != NULL); l = l->txen()) // mount 2,1,0,0,..,-1,-2,.. with previously sorted list
                        {
                            if ((l->data().ppt == ppt) &&
                                !l->data().bDuplicated)
                            {
                                if (l->data().mount == mount) //otherwise first slots get set as duplicated regardless of which are empty
                                {
                                    l->data().bDuplicated = true;
                                    found = true;
                                    break;
                                }
                                else
                                    lWrongMountMatch = l;
                            }
                        }
                        if (!found && lWrongMountMatch)
                            lWrongMountMatch->data().bDuplicated = true;
                    }
                }
            }

            //Fill all of the cargo slots with what was there before. Also fill module slots which were empty.
            Mount   cargo = -c_maxCargo;
            while (pship->GetMountedPart(NA, cargo) != NULL)
                cargo++;
            for (CachedPartLink* l = cache.first(); (l != NULL); l = l->next())
            {
                CachedPart& cpl = l->data();

                if (!cpl.bDuplicated)
                {
                    IpartTypeIGC*   ppt = cpl.ppt;
                    if (ppt)
                    {
                        EquipmentType   et = ppt->GetEquipmentType();

                        if ((cpl.mount >= 0) && //anything not cargo
                            (pship->GetMountedPart(et, cpl.mount) == NULL) &&
                            (pship->GetHullType()->CanMount(ppt, cpl.mount)))
                        {

                            pship->CreateAndAddPart(ppt, cpl.mount, ppt->GetAmount(pship));
                        }
                        else if (cargo < 0)
                        {
                            pship->CreateAndAddPart(ppt, cargo++, SHRT_MAX);
                            while (pship->GetMountedPart(NA, cargo) != NULL)
                                cargo++;
                        }

                    }
                }
            }
}
}
