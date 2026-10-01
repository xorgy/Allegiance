// Frozen HEAD WinTrek targeting searches (39ef2c0). Original branch text;
// test adapter supplies client context and captures SetTarget without mutation.
#pragma once
#include "../../Igc/igc.h"
namespace OriginalTargeting {
struct Window {
    struct Client {
        IshipIGC* ship;IclusterIGC* view;int resumes{};
        bool autoPilot(){return ship->GetAutopilot();}void SetAutoPilot(bool){++resumes;}
        IshipIGC* GetShip(){return ship;} IsideIGC* GetSide(){return ship->GetSide();}
        IclusterIGC* GetCluster(){return view?view:ship->GetCluster();}
        IclusterIGC* GetViewCluster(){return view;}
        bool flyingF(){return ship->GetCluster()!=nullptr;}
    } trekClient;
    ImodelIGC* result{};CommandID command{c_cidNone};int calls{};
    void SetTarget(ImodelIGC* m,CommandID c){result=m;command=c;++calls;}
int   GetSimilarTargetMask(ImodelIGC* pmodel)
{
    assert (pmodel);

    ObjectType  type = pmodel->GetObjectType();

    int tt = GetTypebits(type);
    if ((type == OT_ship) || (type == OT_station) || (type == OT_probe))
		tt |= ((trekClient.GetShip()->GetSide() == pmodel->GetSide()) || IsideIGC::AlliedSides(trekClient.GetShip()->GetSide(), pmodel->GetSide())) // #ALLY -was: == imago fixed 7/8/09
              ? c_ttFriendly
              : c_ttEnemy;
    else
        tt |= c_ttAllSides;

    return tt;
}
void    TrekFindTarget(int              ttMask,
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
        if (trekClient.GetViewCluster())
        {
            IstationIGC*    pstation = trekClient.GetShip()->GetStation();
            if (pstation && (pstation->GetCluster() == trekClient.GetViewCluster()))
                pposition = &(pstation->GetPosition());
            else
                pposition = &(Vector::GetZero());
        }
		else
			pposition = &(trekClient.GetShip()->GetPosition());

		ImodelIGC*  m = NULL; //Imago 7/31/09 ALLY 

		if (trekClient.GetSide()->GetAllies() != NA) { 

			//only target our sides bases unless in a pod
			if (tkNearest == TK_TargetFriendlyBaseNearest) {

				//determine if we're in a pod or not
				bool bLifepod = false;
				IhullTypeIGC*   pht = trekClient.GetShip()->GetSourceShip()->GetBaseHullType();
				if (pht) {
					HullAbilityBitMask  habm = pht->GetCapabilities();
					if (habm & c_habmLifepod)
						bLifepod = true;
				}

				if (!bLifepod)
					m = FindTarget(trekClient.GetShip()->GetSourceShip(), ttMask, trekClient.GetShip()->GetCommandTarget(c_cmdCurrent),
						trekClient.GetCluster(), pposition, NULL, abm, 0x7fffffff, 0); // false = don't search allied bases
				else
					m = FindTarget(trekClient.GetShip()->GetSourceShip(), ttMask, trekClient.GetShip()->GetCommandTarget(c_cmdCurrent),
						trekClient.GetCluster(), pposition, NULL, abm); // true = (default) search allied bases/rescue probes

			//targets allied bases only
			} else if (tkNearest == TK_TargetAlliedBaseNearest) {
				m = FindTarget(trekClient.GetShip()->GetSourceShip(), ttMask, trekClient.GetShip()->GetCommandTarget(c_cmdCurrent),
					trekClient.GetCluster(), pposition, NULL, abm, 0x7fffffff, 2); // 2 = (hack) search allied bases/rescue probes only

			} else {
				//not after a friendly/allied base, do our regular thing (default)
				m = FindTarget(trekClient.GetShip()->GetSourceShip(), ttMask, trekClient.GetShip()->GetCommandTarget(c_cmdCurrent),
					trekClient.GetCluster(), pposition, NULL, abm);
			}

		} else { // team has no allies, do our regular thing

			m = FindTarget(trekClient.GetShip()->GetSourceShip(),
				ttMask,
				trekClient.GetShip()->GetCommandTarget(c_cmdCurrent),
				trekClient.GetCluster(),
				pposition, NULL, abm);
		}

		SetTarget(m, c_cidDefault);
    }
    bool run(TrekKey tk){switch(tk){
            case TK_TargetHostile:  // target the thing that has done the most damage to me recently
            {
                DamageTrack*    pdt = trekClient.GetShip()->GetDamageTrack();
                assert (pdt);
                int n = pdt->GetDamageBuckets()->n();
                if (n > 0)
                {
                    DamageBucketLink* pdbl = pdt->GetDamageBuckets()->first();
                    assert (pdbl);

                    ImodelIGC*   pmodel = pdbl->data()->model();
                    if ((n > 1) && (trekClient.GetShip()->GetCommandTarget(c_cmdCurrent) == pmodel))
                    {
                        assert (pdbl->next());
                        pmodel = pdbl->next()->data()->model();
                    }

                    SetTarget(pmodel, c_cidDefault);
                }
                else
                {
                    TrekFindTarget((c_ttShip | c_ttEnemy | c_ttNearest), tk, TK_TargetEnemyNearest, TK_TargetEnemyPrev, c_habmRescue | c_habmMiner | c_habmBuilder);
                }
            }
            break;

            case TK_TargetCenter:   // target the ship closest to the reticle
            {
                if (trekClient.flyingF())
                {
                    const Orientation*  porientation = &(trekClient.GetShip()->GetOrientation());
                    IshipIGC*           pship = trekClient.GetShip()->GetParentShip();

                    if (pship)
                    {
                        Mount   turretID = trekClient.GetShip()->GetTurretID();
                        if (turretID == NA)
                            porientation = &(pship->GetOrientation());
                    }
                    else
                        pship = trekClient.GetShip();

                    ImodelIGC*  m = FindTarget(pship,
                                               (c_ttFront | c_ttAllTypes | c_ttAllSides),
                                               trekClient.GetShip()->GetCommandTarget(c_cmdCurrent),
                                               NULL, NULL,
                                               porientation);

                    SetTarget(m, c_cidDefault);
                }
            }
            break;

            case TK_TargetEnemy:
            case TK_TargetEnemyNearest:
            case TK_TargetEnemyPrev:
            {
                TrekFindTarget((c_ttShip | c_ttEnemy), tk, TK_TargetEnemyNearest, TK_TargetEnemyPrev, c_habmRescue | c_habmMiner | c_habmBuilder);
            }
            break;

            case TK_TargetEnemyBomber:
            case TK_TargetEnemyBomberNearest:
            case TK_TargetEnemyBomberPrev:
            {
                TrekFindTarget((c_ttShip | c_ttEnemy), tk, TK_TargetEnemyBomberNearest, TK_TargetEnemyBomberPrev, c_habmThreatToStation);
            }
            break;

            case TK_TargetEnemyFighter:
            case TK_TargetEnemyFighterNearest:
            case TK_TargetEnemyFighterPrev:
            {
                TrekFindTarget((c_ttShip | c_ttEnemy), tk, TK_TargetEnemyFighterNearest, TK_TargetEnemyFighterPrev, c_habmFighter);
            }
            break;

            case TK_TargetEnemyMiner:
            case TK_TargetEnemyMinerNearest:
            case TK_TargetEnemyMinerPrev:
            {
                TrekFindTarget((c_ttShip | c_ttEnemy), tk, TK_TargetEnemyMinerNearest, TK_TargetEnemyMinerPrev, c_habmMiner);
            }
            break;

            case TK_TargetEnemyConstructor:
            case TK_TargetEnemyConstructorNearest:
            case TK_TargetEnemyConstructorPrev:
            {
                TrekFindTarget((c_ttShip | c_ttEnemy), tk, TK_TargetEnemyConstructorNearest, TK_TargetEnemyConstructorPrev, c_habmBuilder);
            }
            break;

            case TK_TargetEnemyBase:
            case TK_TargetEnemyBaseNearest:
            case TK_TargetEnemyBasePrev:
            {
                TrekFindTarget((c_ttStation | c_ttEnemy | c_ttAnyCluster), tk, TK_TargetEnemyBaseNearest, TK_TargetEnemyBasePrev, ~c_sabmPedestal);
            }
            break;

            case TK_TargetEnemyMajorBase:
            case TK_TargetEnemyMajorBaseNearest:
            case TK_TargetEnemyMajorBasePrev:
            {
                TrekFindTarget((c_ttStation | c_ttEnemy | c_ttAnyCluster), tk, TK_TargetEnemyMajorBaseNearest, TK_TargetEnemyMajorBasePrev, c_sabmFlag);
            }
            break;

            case TK_TargetFriendlyBase:
            case TK_TargetFriendlyBaseNearest:
            case TK_TargetFriendlyBasePrev:
            {
                int             ttMask = (c_ttStation | c_ttFriendly | c_ttAnyCluster);
                AbilityBitMask  abm = c_sabmLand;

                {
                    IhullTypeIGC*   pht = trekClient.GetShip()->GetSourceShip()->GetBaseHullType();
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

                TrekFindTarget(ttMask, tk, TK_TargetFriendlyBaseNearest, TK_TargetFriendlyBasePrev,
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
                    IhullTypeIGC*   pht = trekClient.GetShip()->GetSourceShip()->GetBaseHullType();
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

                TrekFindTarget(ttMask, tk, TK_TargetAlliedBaseNearest, TK_TargetAlliedBasePrev,
                               abm);
            }
            break;

            case TK_TargetFriendlyMajorBase:
            case TK_TargetFriendlyMajorBaseNearest:
            case TK_TargetFriendlyMajorBasePrev:
            {
                TrekFindTarget((c_ttStation | c_ttFriendly | c_ttAnyCluster), tk, TK_TargetFriendlyMajorBaseNearest, TK_TargetFriendlyMajorBasePrev, c_sabmFlag);
            }
            break;

            case TK_Target:
            case TK_TargetNearest:
            case TK_TargetPrev:
            {
                ImodelIGC*  pmodel = trekClient.GetShip()->GetCommandTarget(c_cmdCurrent);

                TrekFindTarget(pmodel && (pmodel != trekClient.GetShip())
                               ? GetSimilarTargetMask(pmodel)
                               : (c_ttShipTypes | c_ttEnemy),
                               tk, TK_TargetNearest, TK_TargetPrev);
            }
            break;

            case TK_TargetFriendly:
            case TK_TargetFriendlyNearest:
            case TK_TargetFriendlyPrev:
            {
                TrekFindTarget((c_ttShip | c_ttFriendly), tk, TK_TargetFriendlyNearest, TK_TargetFriendlyPrev);
            }
            break;

            case TK_TargetFriendlyBomber:
            case TK_TargetFriendlyBomberNearest:
            case TK_TargetFriendlyBomberPrev:
            {
                TrekFindTarget((c_ttShip | c_ttFriendly), tk, TK_TargetFriendlyBomberNearest, TK_TargetFriendlyBomberPrev, c_habmThreatToStation);
            }
            break;

            case TK_TargetFriendlyFighter:
            case TK_TargetFriendlyFighterNearest:
            case TK_TargetFriendlyFighterPrev:
            {
                TrekFindTarget((c_ttShip | c_ttFriendly), tk, TK_TargetFriendlyFighterNearest, TK_TargetFriendlyFighterPrev, c_habmFighter);
            }
            break;

            case TK_TargetFriendlyMiner:
            case TK_TargetFriendlyMinerNearest:
            case TK_TargetFriendlyMinerPrev:
            {
                TrekFindTarget((c_ttShip | c_ttFriendly), tk, TK_TargetFriendlyMinerNearest, TK_TargetFriendlyMinerPrev, c_habmMiner);
            }
            break;

            case TK_TargetFriendlyConstructor:
            case TK_TargetFriendlyConstructorNearest:
            case TK_TargetFriendlyConstructorPrev:
            {
                TrekFindTarget((c_ttShip | c_ttFriendly), tk, TK_TargetFriendlyConstructorNearest, TK_TargetFriendlyConstructorPrev, c_habmBuilder);
            }
            break;

            case TK_TargetFriendlyLifepod:
            case TK_TargetFriendlyLifepodNearest:
            case TK_TargetFriendlyLifepodPrev:
            {
                TrekFindTarget((c_ttShip | c_ttFriendly), tk, TK_TargetFriendlyLifepodNearest, TK_TargetFriendlyLifepodPrev, c_habmLifepod);
            }
            break;

            case TK_TargetArtifact:
            case TK_TargetArtifactNearest:
            case TK_TargetArtifactPrev:
            {
                TrekFindTarget((c_ttTreasure | c_ttNeutral), tk, TK_TargetArtifactNearest, TK_TargetArtifactPrev, 1);
            }
            break;

            case TK_TargetFlag:
            case TK_TargetFlagNearest:
            case TK_TargetFlagPrev:
            {
                TrekFindTarget((c_ttTreasure | c_ttEnemy), tk, TK_TargetFlagNearest, TK_TargetFlagPrev, 1);
            }
            break;

            case TK_TargetWarp:
            case TK_TargetWarpNearest:
            case TK_TargetWarpPrev:
            {
                TrekFindTarget((c_ttWarp | c_ttNeutral), tk, TK_TargetWarpNearest, TK_TargetWarpPrev);
            }
            break;

default:return false;
    }return true;}
void targetKilled(ImodelIGC*  pmodel)
{
    ImodelIGC*  pmodelCurrent = trekClient.GetShip()->GetCommandTarget(c_cmdCurrent);
    if (pmodel == pmodelCurrent)
    {
        IshipIGC*   pshipSource = trekClient.GetShip()->GetSourceShip();
        if (pshipSource->GetCluster())
        {
            int mask = GetSimilarTargetMask(pmodel) | c_ttNearest;

            int abm;
            if (mask == (c_ttShip | c_ttEnemy | c_ttNearest))
                abm = c_habmRescue;
            else
                abm = 0;

            ImodelIGC*  pmodelTarget = FindTarget(pshipSource,
                                                  mask,
                                                  pmodel, NULL, NULL, NULL, abm);
            if ((pmodelTarget == NULL) && abm)
            {
                pmodelTarget = FindTarget(pshipSource,
                                          mask,
                                          pmodel, NULL, NULL, NULL, c_habmMiner | c_habmBuilder);

                if (pmodelTarget == NULL)
                    pmodelTarget = FindTarget(pshipSource,
                                              mask,
                                              pmodel);
            }

            if (pmodelTarget == pmodel)
                pmodelTarget = NULL;

            SetTarget(pmodelTarget, c_cidDefault);
            if (trekClient.autoPilot() && (trekClient.GetShip()->GetCommandTarget(c_cmdPlan) == pmodel))
                trekClient.SetAutoPilot(true);
        }
    }
}
};
}
