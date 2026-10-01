// Frozen from HEAD 39ef2c0 src/clintlib/clintlib.h, with cache adapter only.
#pragma once
namespace OriginalLoadout {
struct CachedPart { IpartTypeIGC* ppt;Mount mount;bool bDuplicated{}; };
using CachedPartList=Slist_utl<CachedPart>;
using CachedPartLink=Slink_utl<CachedPart>;
struct CachedLoadout {IhullTypeIGC* pht;CachedPartList cpl;};
struct Reference {
    static short    BuyPartOnBudget(IshipIGC*       pship,
                                    IpartTypeIGC*   ppt,
                                    Mount           mount,
                                    Money*          pbudget)
    {
        Money   price = ppt->GetPrice();

        short   amount;
        if (price != 0)
        {
            if (IlauncherTypeIGC::IsLauncherType(ppt->GetEquipmentType()))
            {
                amount = ppt->GetAmount(pship);
                if (price * amount > *pbudget)
                {
                    assert (price > 0);
                    amount = *pbudget / price;
                }

                price *= amount;
            }
            else
                amount = (*pbudget >= price) ? 0x7fff : 0;
        }
        else
            amount = 0x7fff;

        if (amount != 0)
        {
            *pbudget -= price;
            pship->CreateAndAddPart(ppt, mount, amount);
        }

        return amount;
    }
    static void     TryToBuyParts(IshipIGC*              pship,
                                  IstationIGC*           pstation, 
                                  Money*                 pbudget,
                                  IhullTypeIGC*          pht,
                                  const PartTypeListIGC* ppartsShip)
    {
        assert (pship);
        assert (pstation);
        assert (*pbudget >= 0);
        assert (pship->GetBaseHullType() == pht);

        for (PartTypeLinkIGC*   ptl = ppartsShip->first();
             (ptl != NULL);
             ptl = ptl->next())
        {
            IpartTypeIGC*   ppt = ptl->data();
            if (pstation->CanBuy(ppt))
            {
                ppt = (IpartTypeIGC*)(pstation->GetSuccessor(ppt));
                assert (pstation->CanBuy(ppt));

                //We can buy this part ... try mounting it somewhere
                EquipmentType   et = ppt->GetEquipmentType();
                Mount           iMountMax = (et == ET_Weapon)
                    ? pht->GetMaxWeapons()
                    : 1;

                for (Mount i = 0; (i < iMountMax); i++)
                {
                    //If the slot is empty and we can mount the part in the slot
                    if ((pship->GetMountedPart(et, i) == NULL) && pht->CanMount(ppt, i))
                    {
                        if (BuyPartOnBudget(pship, ppt, i, pbudget) == 0)
                            break;      //Ran out of money ... don't try to mount any more instances of this part
                    }
                }
            }
        }
    }
static Money buy(IshipIGC* pship,IstationIGC* pstation,CachedLoadout* pcl,Money budget) {
    IhullTypeIGC* pht=pship->GetBaseHullType();
            //Clone the previous loadout
            for (CachedPartLink*  l = pcl->cpl.first(); (l != NULL); l = l->next())
            {
                CachedPart& cpl = l->data();

                IpartTypeIGC*   ppt;
                if (pstation->CanBuy(cpl.ppt))
                {
                    ppt = (IpartTypeIGC*)(pstation->GetSuccessor(cpl.ppt));
                }
                else
                    ppt = pstation->GetSimilarPart(cpl.ppt); //AaronMoore 1/10

                if (ppt)
                {
                    if (pht->CanMount(ppt, cpl.mount))
                        BuyPartOnBudget(pship, ppt, cpl.mount, &budget);
                    else
                        debugf("**** successor unable to mount %s/%s\n", pht->GetName(), ppt->GetName());
                }
            }

            //Fill any remaining slots with the default items if the hull type is upgraded
            if (pcl->pht != pht)
                TryToBuyParts(pship, pstation, &budget, pht, pht->GetPreferredPartTypes());
return budget;
}
static Money restore(IshipIGC* pshipSource,IshipIGC* pshipSink,IstationIGC* pstation,CachedLoadout* cache,Money budget) {
    struct CacheLink { CachedLoadout* cache;CachedLoadout& data(){return *cache;} } link{cache};
    auto* pcll=&link;
            IhullTypeIGC*   pht = pcll->data().pht;

            //We have the same hull as the saved loaded ... try to preserve as much
            //of the old loadout as possible
            const PartListIGC*  pparts = pshipSource->GetParts();

            //Upgrade the hull if possible
            IhullTypeIGC*   phtSuccessor = (pstation->CanBuy(pht))
                                           ? (IhullTypeIGC*)(pstation->GetSuccessor(pht))
                                           : pht;

            assert (pht->GetPrice() == phtSuccessor->GetPrice());
            budget -= phtSuccessor->GetPrice();
            pshipSink->SetBaseHullType(phtSuccessor);

            {
                //Mark all the parts in the cached loadout as not duplicated
                for (CachedPartLink*  l = pcll->data().cpl.first(); (l != NULL); l = l->next())
                    l->data().bDuplicated = false;
            }

            //Copy over the parts that were mounted on the old hull
            {
                for (PartLinkIGC*  l = pparts->first(); (l != NULL); l = l->next())
                {
                    IpartIGC*       ppart = l->data();
                    Mount           mount = ppart->GetMountID();
                    if (mount >= 0)
                    {
                        IpartTypeIGC*   ppt = ppart->GetPartType();

                        if (phtSuccessor->CanMount(ppt, mount))
                        {
                            if (pstation->CanBuy(ppt))
                                BuyPartOnBudget(pshipSink, (IpartTypeIGC*)(pstation->GetSuccessor(ppt)), mount, &budget);
                            else
                            {
                                //Re-use the old part
                                Money   price = ppt->GetPrice();

                                short   amount = ppart->GetAmount();
                                if (price != 0)
                                {
                                    if (IlauncherTypeIGC::IsLauncherType(ppt->GetEquipmentType()))
                                    {
                                        if (price * amount > budget)
                                        {
                                            assert (price > 0);
                                            amount = budget / price;
                                        }

                                        price *= amount;
                                    }
                                    else if (budget >= price)
                                        amount = 0;
                                }

                                if (amount != 0)
                                {
                                    budget -= price;
                                    pshipSink->CreateAndAddPart(ppt, mount, amount);
                                }
                            }

                            //Mark the corresponding part in the cached loadout as having been duplicated
                            {
                                bool found = false;
                                CachedPartLink* lWrongMountMatch = NULL;
                                for (CachedPartLink* l = pcll->data().cpl.last(); (l != NULL); l = l->txen()) // mount 2,1,0,0,..,-1,-2,.. with previously sorted list
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
                        else
                            debugf("**** successor unable to mount %s/%s\n", phtSuccessor->GetName(), ppt->GetName());
                    }
                }
            }

            //Fill all of the cargo slots with what was there before. Also fill module slots which were empty.
            Mount   cargo = -c_maxCargo;
            for (CachedPartLink*  l = pcll->data().cpl.first(); (l != NULL); l = l->next())
            {
                CachedPart& cpl = l->data();

                if (!cpl.bDuplicated)
                {
                    IpartTypeIGC*   ppt;
                    if (pstation->CanBuy(cpl.ppt))
                    {
                        //We have an empty slot and a part we can buy ... now see if there is an instance
                        //of this part in the available instance of this part in the new loadout
                        ppt = (IpartTypeIGC*)(pstation->GetSuccessor(cpl.ppt));
                    }
                    else
                        ppt = pstation->GetSimilarPart(cpl.ppt);

                    if (ppt)
                    {
                        EquipmentType   et =  ppt->GetEquipmentType();
                        
                        if ((cpl.mount >= 0) && //anything not cargo
                            (pshipSink->GetMountedPart(et, cpl.mount) == NULL) &&
                            (phtSuccessor->CanMount(ppt, cpl.mount)))
                        {
                            debugf("RestoreLoadout: Restoring empty module slot %d, type %d with %d \n", cpl.mount, et, ppt->GetObjectID());
                            BuyPartOnBudget(pshipSink, ppt, cpl.mount, &budget);
                        }
                        else if (cargo < 0)
                        {
                            BuyPartOnBudget(pshipSink, ppt, cargo++, &budget);
                        }

                    }
                }
            }

            if (pht != phtSuccessor)
            {
                //Fill any remaining slots with the default items
                TryToBuyParts(pshipSink, pstation, &budget, phtSuccessor, phtSuccessor->GetPreferredPartTypes());
            }
    return budget;
}

};
}
