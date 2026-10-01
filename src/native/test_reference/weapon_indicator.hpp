// Frozen indicator.cpp at 39ef2c0; camera/ship context supplied explicitly.
#pragma once
namespace OriginalIndicator {
inline bool relay(IshipIGC* pship,ImodelIGC* targetModel,bool pshipParent) {
    auto* pcluster=pship->GetCluster();auto* pside=pship->GetSide();
                            bool    bLeadIndicator;
                            if (pshipParent || pship->GetBaseHullType()->HasCapability(c_habmLeadIndicator) /*|| g_bCheatLeadIndicator*/)
                            {
                                bLeadIndicator = true;
                            }
                            else
                            {
                                bLeadIndicator = false;
                                //See if a friendly relay station can spot the target
                                for (StationLinkIGC* psl = pcluster->GetStations()->first(); (psl != NULL); psl = psl->next())
                                {
                                    IstationIGC*   ps = psl->data();
                                    if ((ps->GetSide() == pside) &&
                                        ps->GetBaseStationType()->HasCapability(c_sabmRemoteLeadIndicator) &&
                                        ps->InScannerRange(targetModel))
                                    {
                                        bLeadIndicator = true;
                                        break;
                                    }
                                }

                                //See if a friendly relay ship can spot the target
                                if (!bLeadIndicator)
                                {
                                    for (ShipLinkIGC* psl = pcluster->GetShips()->first(); (psl != NULL); psl = psl->next())
                                    {
                                        IshipIGC*   ps = psl->data();
                                        if ((ps->GetSide() == pside) && //#ALLYTD: change this if allies should relay lead ind SCAN
                                            (ps->GetParentShip() == NULL) &&
                                            ps->GetBaseHullType()->HasCapability(c_habmRemoteLeadIndicator) &&
                                            ps->InScannerRange(targetModel))
                                        {
                                            bLeadIndicator = true;
                                            break;
                                        }
                                    }
                                }
                            }

    return bLeadIndicator;
}
inline bool target(IweaponIGC* w,ImodelIGC* targetModel,IsideIGC* pside,bool chase,bool missile) {
    return w && (chase || missile || (((targetModel->GetSide() == pside) || IsideIGC::AlliedSides(targetModel->GetSide(), pside)) == (w->GetProjectileType()->GetPower() < 0.0f)));
}
}
