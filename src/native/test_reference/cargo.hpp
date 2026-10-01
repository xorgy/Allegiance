// Frozen WinTrek cargo navigation at 39ef2c072b1eac0e9f97ab1428bc96572cbd5854.
// The host is replaced with a real IGC ship; method bodies are unchanged.
#pragma once
#include "igc.h"
namespace OriginalCargo {
struct Navigation {
    IshipIGC* ship;Mount mountSelected=-1;
    IshipIGC* GetShip(){return ship;}
    IpartIGC* GetCargoPart() {
    assert (mountSelected < 0 && mountSelected >= -c_maxCargo);

    IpartIGC* ppart = GetShip()->GetMountedPart(NA, mountSelected);

    // make sure it's the first item of a grouped set of items.  
    if (mountSelected < -1) // first item in the list is always the first item of its type
    {
        mountSelected++;
        NextCargoPart();
        ppart = GetShip()->GetSourceShip()->GetMountedPart(NA, mountSelected);
    }

    return ppart;
}
    void NextCargoPart() {
    // select a real part or the first empty cargo slot
    bool bFoundPart = false;
    do
    {
        --mountSelected;
        if (mountSelected < -c_maxCargo)
            mountSelected = -1;

        IpartIGC *ppart = GetShip()->GetSourceShip()->GetMountedPart(NA, mountSelected);
        IpartTypeIGC *ppartType = ppart ? ppart->GetPartType() : NULL;

        bFoundPart = true;

        // If this is ammo/missiles/etc, is this the first instance of this part?
        ObjectType ot = ppart ? ppart->GetObjectType() : NA;
        if (ppart == NULL || ot == OT_pack || ot == OT_magazine || ot == OT_dispenser)
        {
            for (Mount mountPrev = mountSelected + 1; mountPrev < 0; ++mountPrev)
            {
                IpartIGC *ppartPrev = GetShip()->GetSourceShip()->GetMountedPart(NA, mountPrev);
                IpartTypeIGC *ppartTypePrev = ppartPrev ? ppartPrev->GetPartType() : NULL;

                if (ppartType == ppartTypePrev)
                {
                    bFoundPart = false;
                    break;
                }
            }
        }
    }
    while (!bFoundPart);
}
    bool SelectCargoPartOfType(EquipmentType et,PartMask maxPartMask,IpartTypeIGC* pptNotThis) {
    IpartIGC* ppartStart = GetCargoPart();
        
    if (ppartStart && (
               (ppartStart->GetEquipmentType() != et) ||
               ((ppartStart->GetPartType()->GetPartMask() & ~maxPartMask) != 0) ||
               (ppartStart->GetPartType() == pptNotThis))
        || (!ppartStart && !pptNotThis))
    {
        IpartIGC* ppart;

        do
        {
            NextCargoPart();                    
            ppart = GetCargoPart();
        }
        while (ppart != ppartStart
            &&  (
                ppart && (
                    ppart->GetEquipmentType() != et
                    || ((ppart->GetPartType()->GetPartMask() & ~maxPartMask) != 0)
                    || ppart->GetPartType() == pptNotThis)
                || (!ppart && !pptNotThis)
            )
            );

        if (ppart == ppartStart)
        {
            // no part in cargo found
            return false;
        }

        return true;
    }
    else
    {
        return true;
    }
}
};
}
