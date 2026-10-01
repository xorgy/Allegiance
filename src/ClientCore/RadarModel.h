// Original radar object metadata; no renderer, container or virtual adapter.
#pragma once
#include "../Igc/igc.h"
#include "RadarPresentation.h"
namespace Allegiance {
constexpr int c_iNotUseful=0,c_iUsefulPart=1,c_iUsefulTech=2;
    inline int UsefulRadarTreasure(IshipIGC*    pship, ItreasureIGC* pt)
    {
        TreasureCode    tc = pt->GetTreasureCode();
        switch (tc)
        {
            case c_tcPowerup:
            {
                if (!pship->GetCluster())
                    return c_iNotUseful;
            }
            //no break

            case c_tcCash:
            case c_tcFlag:
            {
                return c_iUsefulPart;
            }
            break;

            case c_tcDevelopment:
            {
                IdevelopmentIGC*    pd = pship->GetMission()->GetDevelopment(pt->GetTreasureID());
                assert (pd);
                return pd->IsObsolete(pship->GetSide()->GetDevelopmentTechs())
                       ? c_iNotUseful
                       : c_iUsefulPart;     //It really is a tech, but use its icon instead of the special icon
            }
            break;

            default:
            {
                assert (tc == c_tcPart);

                IpartTypeIGC*       ppt = pship->GetMission()->GetPartType(pt->GetTreasureID());
                assert (ppt);
                EquipmentType       et  = ppt->GetEquipmentType();
                if (et == ET_Pack)
                {
                    if (pship->GetCluster() != NULL)
                    {
                        const DataPackTypeIGC*  pdata = (const DataPackTypeIGC*)(ppt->GetData());
                        if (pdata->packType == c_packFuel)
                        {
                            //Fuel is useful only if we have an afterburner
                            return (pship->GetMountedPart(ET_Afterburner, 0) != NULL)
                                   ? c_iUsefulPart
                                   : c_iNotUseful;
                        }
                        else
                        {
                            assert (pdata->packType == c_packAmmo);
                            Mount   m = pship->GetHullType()->GetMaxWeapons();

                            for (Mount i = 0; (i < m); i++)
                            {
                                IweaponIGC* pw = (IweaponIGC*)(pship->GetMountedPart(ET_Weapon, i));
                                if (pw && (pw->GetAmmoPerShot() != 0))
                                    return c_iUsefulPart;
                            }

                        }
                    }

                    return c_iNotUseful;
                }
                else
                {
                    if (!(ppt->GetEffectTechs() <= pship->GetSide()->GetDevelopmentTechs()))
                        return c_iUsefulTech;

                    if (IlauncherTypeIGC::IsLauncherType(et) && pship->GetCluster() && pship->GetHullType()->CanMount(ppt, 0))
                        return c_iUsefulPart;

                    return c_iNotUseful;
                }
            }
            break;
        }
    }

struct RadarModel {
    ImodelIGC* model;IshipIGC* source;IsideIGC* side;IsideIGC* mine;
    bool ownSide()const{return side==mine;}
    bool allied()const{return IsideIGC::AlliedSides(side,mine);}
    bool self()const{return model==source;}
    bool pedestal()const{return static_cast<IstationIGC*>(model)->GetBaseStationType()->HasCapability(c_sabmPedestal);}
    bool missileTargetsOwnSide()const {auto* target=static_cast<ImissileIGC*>(model)->GetTarget();return target && target->GetSide()==mine;}
    int usefulTreasure()const{return UsefulRadarTreasure(source,static_cast<ItreasureIGC*>(model));}
    bool plainAsteroid()const{return !model->GetName()[0] || model->GetName()[0]=='A';}
};
static_assert(OT_ship==0 && OT_station==1 && OT_missile==2 && OT_mine==3 && OT_probe==4 && OT_asteroid==5 && OT_warp==7 && OT_treasure==8,"radar presentation object IDs");
}
