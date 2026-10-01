// Frozen from original console.cpp at 39ef2c072b1eac0e9f97ab1428bc96572cbd5854.
#pragma once
namespace OriginalQuickChat {
inline ImodelIGC* target(IshipIGC* ship,CommandID& cid,int ttMask,AbilityBitMask aabm) {
        if (cid == c_cidMyAccepted)
            cid = ship->GetCommandID(c_cmdAccepted);

        //
        // figure out the target
        //
        ImodelIGC* pmodel;
        if (cid != c_cidNone)
        {
            assert (ttMask);

            switch (ttMask)
            {
                case c_ttMyAccepted:
                case c_ttMyQueued:
                case c_ttMyTarget:
                    assert (char(c_ttMyAccepted) == c_cmdAccepted);
                    assert (char(c_ttMyQueued) == c_cmdQueued);
                    assert (char(c_ttMyTarget) == c_cmdCurrent);

                    pmodel = ship->GetCommandTarget(char(ttMask));
                break;

                case c_ttWorstEnemy:
                {
                    DamageTrack*        pdt = ship->GetDamageTrack();
                    assert (pdt);
                    DamageBucketLink* pdbl = pdt->GetDamageBuckets()->first();
                    pmodel = pdbl ? pdbl->data()->model() : NULL;
                }
                break;

                case c_ttMe:
                    pmodel = ship;
                break;

                default:
                    pmodel = ship->GetCommandTarget(c_cmdCurrent);

                    if ((!pmodel) ||
                        (pmodel == ship) ||
                        (!FindableModel(pmodel, ship->GetSide(), ttMask, aabm)) ||
                        (!ship->CanSee(pmodel)))
                    {
                        pmodel = FindTarget(ship, ttMask, NULL, NULL, NULL, NULL, aabm);
                    }
            }

            if (pmodel == NULL)
                cid = c_cidNone;
        }
        else
            pmodel = NULL;

    return pmodel;
}
}
