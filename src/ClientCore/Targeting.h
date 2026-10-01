// WinTrek target selection policy. IGC FindTarget supplies discovery, alliance,
// category, capability, range, cycling and cross-sector search semantics.
#pragma once
#include "../Igc/igc.h"
namespace Allegiance {
inline int SimilarTargetMask(IshipIGC* ship, ImodelIGC* pmodel)
{
    assert (pmodel);

    ObjectType  type = pmodel->GetObjectType();

    int tt = GetTypebits(type);
    if ((type == OT_ship) || (type == OT_station) || (type == OT_probe))
		tt |= ((ship->GetSide() == pmodel->GetSide()) || IsideIGC::AlliedSides(ship->GetSide(), pmodel->GetSide())) // #ALLY -was: == imago fixed 7/8/09
              ? c_ttFriendly
              : c_ttEnemy;
    else
        tt |= c_ttAllSides;

    return tt;
}
// WinTrekClient::TargetKilled: preserve category/side and its rescue, then
// miner/builder preference before falling back to any similar enemy ship.
inline ImodelIGC* ReplacementTarget(IshipIGC* ship,ImodelIGC* destroyed) {
    auto* source=ship->GetSourceShip();int mask=SimilarTargetMask(ship,destroyed)|c_ttNearest;
    int capabilities=mask==(c_ttShip|c_ttEnemy|c_ttNearest)?c_habmRescue:0;
    auto* replacement=FindTarget(source,mask,destroyed,NULL,NULL,NULL,capabilities);
    if(!replacement && capabilities){replacement=FindTarget(source,mask,destroyed,NULL,NULL,NULL,c_habmMiner|c_habmBuilder);
        if(!replacement)replacement=FindTarget(source,mask,destroyed);}
    return replacement==destroyed?NULL:replacement;
}
template<class SetTarget,class Resume>
inline void TargetKilled(IshipIGC* ship,ImodelIGC* destroyed,SetTarget setTarget,Resume resume) {
    if(destroyed==ship->GetCommandTarget(c_cmdCurrent) && ship->GetSourceShip()->GetCluster()) {
        setTarget(ReplacementTarget(ship,destroyed),c_cidDefault);
        if(ship->GetAutopilot() && ship->GetCommandTarget(c_cmdPlan)==destroyed)resume();
    }
}
inline ImodelIGC* FindClientTarget(IshipIGC* ship, IclusterIGC* viewCluster, int ttMask,
                           TrekKey          tk,
                           TrekKey          tkNearest,
                           TrekKey          tkPrevious,
                           AbilityBitMask   abm = 0)
    {
        assert ((ttMask & c_ttFront) == 0);
		if (tk == tkNearest)
            ttMask |= c_ttNearest;
		else if (tk == tkPrevious) 
            ttMask |= c_ttPrevious;

        const Vector*   pposition;
        if (viewCluster)
        {
            IstationIGC*    pstation = ship->GetStation();
            if (pstation && (pstation->GetCluster() == viewCluster))
                pposition = &(pstation->GetPosition());
            else
                pposition = &(Vector::GetZero());
        }
		else
			pposition = &(ship->GetPosition());

		ImodelIGC*  m = NULL; //Imago 7/31/09 ALLY 

		if (ship->GetSide()->GetAllies() != NA) { 

			//only target our sides bases unless in a pod
			if (tkNearest == TK_TargetFriendlyBaseNearest) {

				//determine if we're in a pod or not
				bool bLifepod = false;
				IhullTypeIGC*   pht = ship->GetSourceShip()->GetBaseHullType();
				if (pht) {
					HullAbilityBitMask  habm = pht->GetCapabilities();
					if (habm & c_habmLifepod)
						bLifepod = true;
				}

				if (!bLifepod)
					m = FindTarget(ship->GetSourceShip(), ttMask, ship->GetCommandTarget(c_cmdCurrent),
						(viewCluster ? viewCluster : ship->GetCluster()), pposition, NULL, abm, 0x7fffffff, 0); // false = don't search allied bases
				else
					m = FindTarget(ship->GetSourceShip(), ttMask, ship->GetCommandTarget(c_cmdCurrent),
						(viewCluster ? viewCluster : ship->GetCluster()), pposition, NULL, abm); // true = (default) search allied bases/rescue probes

			//targets allied bases only
			} else if (tkNearest == TK_TargetAlliedBaseNearest) {
				m = FindTarget(ship->GetSourceShip(), ttMask, ship->GetCommandTarget(c_cmdCurrent),
					(viewCluster ? viewCluster : ship->GetCluster()), pposition, NULL, abm, 0x7fffffff, 2); // 2 = (hack) search allied bases/rescue probes only

			} else {
				//not after a friendly/allied base, do our regular thing (default)
				m = FindTarget(ship->GetSourceShip(), ttMask, ship->GetCommandTarget(c_cmdCurrent),
					(viewCluster ? viewCluster : ship->GetCluster()), pposition, NULL, abm);
			}

		} else { // team has no allies, do our regular thing

			m = FindTarget(ship->GetSourceShip(),
				ttMask,
				ship->GetCommandTarget(c_cmdCurrent),
				(viewCluster ? viewCluster : ship->GetCluster()),
				pposition, NULL, abm);
		}

		return m;
    }
template<class Select>
bool SelectTargetForKey(IshipIGC* ship,IclusterIGC* viewCluster,TrekKey tk,Select select) {
    auto find=[&](int mask,TrekKey key,TrekKey nearest,TrekKey previous,AbilityBitMask abilities=0) {
        select(FindClientTarget(ship,viewCluster,mask,key,nearest,previous,abilities),c_cidDefault);
    };
    switch(tk) {
            case TK_TargetHostile:  // target the thing that has done the most damage to me recently
            {
                DamageTrack*    pdt = ship->GetDamageTrack();
                assert (pdt);
                int n = pdt->GetDamageBuckets()->n();
                if (n > 0)
                {
                    DamageBucketLink* pdbl = pdt->GetDamageBuckets()->first();
                    assert (pdbl);

                    ImodelIGC*   pmodel = pdbl->data()->model();
                    if ((n > 1) && (ship->GetCommandTarget(c_cmdCurrent) == pmodel))
                    {
                        assert (pdbl->next());
                        pmodel = pdbl->next()->data()->model();
                    }

                    select(pmodel, c_cidDefault);
                }
                else
                {
                    find((c_ttShip | c_ttEnemy | c_ttNearest), tk, TK_TargetEnemyNearest, TK_TargetEnemyPrev, c_habmRescue | c_habmMiner | c_habmBuilder);
                }
            }
            break;

            case TK_TargetCenter:   // target the ship closest to the reticle
            {
                if ((ship->GetCluster() != NULL))
                {
                    const Orientation*  porientation = &(ship->GetOrientation());
                    IshipIGC*           pship = ship->GetParentShip();

                    if (pship)
                    {
                        Mount   turretID = ship->GetTurretID();
                        if (turretID == NA)
                            porientation = &(pship->GetOrientation());
                    }
                    else
                        pship = ship;

                    ImodelIGC*  m = FindTarget(pship,
                                               (c_ttFront | c_ttAllTypes | c_ttAllSides),
                                               ship->GetCommandTarget(c_cmdCurrent),
                                               NULL, NULL,
                                               porientation);

                    select(m, c_cidDefault);
                }
            }
            break;

            case TK_TargetEnemy:
            case TK_TargetEnemyNearest:
            case TK_TargetEnemyPrev:
            {
                find((c_ttShip | c_ttEnemy), tk, TK_TargetEnemyNearest, TK_TargetEnemyPrev, c_habmRescue | c_habmMiner | c_habmBuilder);
            }
            break;

            case TK_TargetEnemyBomber:
            case TK_TargetEnemyBomberNearest:
            case TK_TargetEnemyBomberPrev:
            {
                find((c_ttShip | c_ttEnemy), tk, TK_TargetEnemyBomberNearest, TK_TargetEnemyBomberPrev, c_habmThreatToStation);
            }
            break;

            case TK_TargetEnemyFighter:
            case TK_TargetEnemyFighterNearest:
            case TK_TargetEnemyFighterPrev:
            {
                find((c_ttShip | c_ttEnemy), tk, TK_TargetEnemyFighterNearest, TK_TargetEnemyFighterPrev, c_habmFighter);
            }
            break;

            case TK_TargetEnemyMiner:
            case TK_TargetEnemyMinerNearest:
            case TK_TargetEnemyMinerPrev:
            {
                find((c_ttShip | c_ttEnemy), tk, TK_TargetEnemyMinerNearest, TK_TargetEnemyMinerPrev, c_habmMiner);
            }
            break;

            case TK_TargetEnemyConstructor:
            case TK_TargetEnemyConstructorNearest:
            case TK_TargetEnemyConstructorPrev:
            {
                find((c_ttShip | c_ttEnemy), tk, TK_TargetEnemyConstructorNearest, TK_TargetEnemyConstructorPrev, c_habmBuilder);
            }
            break;

            case TK_TargetEnemyBase:
            case TK_TargetEnemyBaseNearest:
            case TK_TargetEnemyBasePrev:
            {
                find((c_ttStation | c_ttEnemy | c_ttAnyCluster), tk, TK_TargetEnemyBaseNearest, TK_TargetEnemyBasePrev, ~c_sabmPedestal);
            }
            break;

            case TK_TargetEnemyMajorBase:
            case TK_TargetEnemyMajorBaseNearest:
            case TK_TargetEnemyMajorBasePrev:
            {
                find((c_ttStation | c_ttEnemy | c_ttAnyCluster), tk, TK_TargetEnemyMajorBaseNearest, TK_TargetEnemyMajorBasePrev, c_sabmFlag);
            }
            break;

            case TK_TargetFriendlyBase:
            case TK_TargetFriendlyBaseNearest:
            case TK_TargetFriendlyBasePrev:
            {
                int             ttMask = (c_ttStation | c_ttFriendly | c_ttAnyCluster);
                AbilityBitMask  abm = c_sabmLand;

                {
                    IhullTypeIGC*   pht = ship->GetSourceShip()->GetBaseHullType();
                    if (pht)
                    {
                        HullAbilityBitMask  habm = pht->GetCapabilities();

                        if (habm & c_habmLifepod)
                        {
                            abm = c_sabmRescue | c_sabmRescueAny | c_sabmLand;
                            ttMask |= c_ttProbe;
                        }
                        else if ((habm & c_habmFighter) == 0)
                            abm = c_sabmCapLand;
                    }
                }

                find(ttMask, tk, TK_TargetFriendlyBaseNearest, TK_TargetFriendlyBasePrev,
                               abm);
            }
            break;

            case TK_TargetAlliedBase:
            case TK_TargetAlliedBaseNearest:
            case TK_TargetAlliedBasePrev:
            {
                int             ttMask = (c_ttStation | c_ttFriendly | c_ttAnyCluster);
                AbilityBitMask  abm = c_sabmLand;

                {
                    IhullTypeIGC*   pht = ship->GetSourceShip()->GetBaseHullType();
                    if (pht)
                    {
                        HullAbilityBitMask  habm = pht->GetCapabilities();

                        if (habm & c_habmLifepod)
                        {
                            abm = c_sabmRescue | c_sabmRescueAny | c_sabmLand;
                            ttMask |= c_ttProbe;
                        }
                        else if ((habm & c_habmFighter) == 0)
                            abm = c_sabmCapLand;
                    }
                }

                find(ttMask, tk, TK_TargetAlliedBaseNearest, TK_TargetAlliedBasePrev,
                               abm);
            }
            break;

            case TK_TargetFriendlyMajorBase:
            case TK_TargetFriendlyMajorBaseNearest:
            case TK_TargetFriendlyMajorBasePrev:
            {
                find((c_ttStation | c_ttFriendly | c_ttAnyCluster), tk, TK_TargetFriendlyMajorBaseNearest, TK_TargetFriendlyMajorBasePrev, c_sabmFlag);
            }
            break;

            case TK_Target:
            case TK_TargetNearest:
            case TK_TargetPrev:
            {
                ImodelIGC*  pmodel = ship->GetCommandTarget(c_cmdCurrent);

                find(pmodel && (pmodel != ship)
                               ? SimilarTargetMask(ship, pmodel)
                               : (c_ttShipTypes | c_ttEnemy),
                               tk, TK_TargetNearest, TK_TargetPrev);
            }
            break;

            case TK_TargetFriendly:
            case TK_TargetFriendlyNearest:
            case TK_TargetFriendlyPrev:
            {
                find((c_ttShip | c_ttFriendly), tk, TK_TargetFriendlyNearest, TK_TargetFriendlyPrev);
            }
            break;

            case TK_TargetFriendlyBomber:
            case TK_TargetFriendlyBomberNearest:
            case TK_TargetFriendlyBomberPrev:
            {
                find((c_ttShip | c_ttFriendly), tk, TK_TargetFriendlyBomberNearest, TK_TargetFriendlyBomberPrev, c_habmThreatToStation);
            }
            break;

            case TK_TargetFriendlyFighter:
            case TK_TargetFriendlyFighterNearest:
            case TK_TargetFriendlyFighterPrev:
            {
                find((c_ttShip | c_ttFriendly), tk, TK_TargetFriendlyFighterNearest, TK_TargetFriendlyFighterPrev, c_habmFighter);
            }
            break;

            case TK_TargetFriendlyMiner:
            case TK_TargetFriendlyMinerNearest:
            case TK_TargetFriendlyMinerPrev:
            {
                find((c_ttShip | c_ttFriendly), tk, TK_TargetFriendlyMinerNearest, TK_TargetFriendlyMinerPrev, c_habmMiner);
            }
            break;

            case TK_TargetFriendlyConstructor:
            case TK_TargetFriendlyConstructorNearest:
            case TK_TargetFriendlyConstructorPrev:
            {
                find((c_ttShip | c_ttFriendly), tk, TK_TargetFriendlyConstructorNearest, TK_TargetFriendlyConstructorPrev, c_habmBuilder);
            }
            break;

            case TK_TargetFriendlyLifepod:
            case TK_TargetFriendlyLifepodNearest:
            case TK_TargetFriendlyLifepodPrev:
            {
                find((c_ttShip | c_ttFriendly), tk, TK_TargetFriendlyLifepodNearest, TK_TargetFriendlyLifepodPrev, c_habmLifepod);
            }
            break;

            case TK_TargetArtifact:
            case TK_TargetArtifactNearest:
            case TK_TargetArtifactPrev:
            {
                find((c_ttTreasure | c_ttNeutral), tk, TK_TargetArtifactNearest, TK_TargetArtifactPrev, 1);
            }
            break;

            case TK_TargetFlag:
            case TK_TargetFlagNearest:
            case TK_TargetFlagPrev:
            {
                find((c_ttTreasure | c_ttEnemy), tk, TK_TargetFlagNearest, TK_TargetFlagPrev, 1);
            }
            break;

            case TK_TargetWarp:
            case TK_TargetWarpNearest:
            case TK_TargetWarpPrev:
            {
                find((c_ttWarp | c_ttNeutral), tk, TK_TargetWarpNearest, TK_TargetWarpPrev);
            }
            break;

    case TK_TargetSelf:select(ship,c_cidNone);break;
    case TK_TargetNothing:select(nullptr,c_cidNone);break;
    default:return false;
    }return true;
}
}
