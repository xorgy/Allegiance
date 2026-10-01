// Frozen CFSMission::DoTick at 39ef2c0. Wrapper only; do not follow production edits.
#pragma once
namespace OriginalTimeout {
inline std::pair<IsideIGC*,const char*> check(ImissionIGC* pm) {
 const auto* pmp=pm->GetMissionParams();
                IsideIGC*   psideWin = NULL;

                const char* pszClue = "kills";

                if (pmp->IsConquestGame())
                {
                    pszClue = "conquest";

                    int maxFlags = 0;
                    for (SideLinkIGC*   psl = pm->GetSides()->first(); (psl != NULL); psl = psl->next())
                    {
                        IsideIGC*   pside = psl->data();
                        int nFlags = 0;
                        for (StationLinkIGC*    l = pside->GetStations()->first(); (l != NULL); l = l->next())
                        {
                            if (l->data()->GetBaseStationType()->HasCapability(c_sabmFlag))
                                nFlags++;
                        }

                        if (nFlags > maxFlags)
                        {
                            maxFlags = nFlags;
                            psideWin = pside;
                        }
                        else if (nFlags == maxFlags)
                        {
                            psideWin = NULL;
                        }
                    }
                }

                if ((psideWin == NULL) && pmp->IsTerritoryGame())
                {
                    pszClue = "territory";

                    unsigned char maxSectors = 0;
                    for (SideLinkIGC*   psl = pm->GetSides()->first(); (psl != NULL); psl = psl->next())
                    {
                        IsideIGC*   pside = psl->data();
                        int nSectors = pside->GetTerritoryCount();

                        if (nSectors > maxSectors)
                        {
                            maxSectors = nSectors;
                            psideWin = pside;
                        }
                        else if (nSectors == maxSectors)
                        {
                            psideWin = NULL;
                        }
                    }
                }

                if ((psideWin == NULL) && pmp->IsProsperityGame())
                {
                    pszClue = "prosperity";

                    DWORD maxTime = 0;
                    for (SideLinkIGC*   psl = pm->GetSides()->first(); (psl != NULL); psl = psl->next())
                    {
                        IsideIGC*   pside = psl->data();

                        IbucketIGC* pbucket = pside->GetBuckets()->last()->data();
                        assert ((pbucket->GetBucketType() == OT_development) &&
                                (pbucket->GetBuyable()->GetObjectID() == c_didTeamMoney));

                        DWORD   time = pbucket->GetTime();

                        if (time > maxTime)
                        {
                            maxTime = time;
                            psideWin = pside;
                        }
                        else if (time == maxTime)
                        {
                            psideWin = NULL;
                        }
                    }
                }

                if ((psideWin == NULL) && pmp->IsArtifactsGame())
                {
                    pszClue = "artifacts";

                    short   maxFlags = 0;
                    for (SideLinkIGC*   psl = pm->GetSides()->first(); (psl != NULL); psl = psl->next())
                    {
                        IsideIGC*   pside = psl->data();

                        short   nFlags = pside->GetArtifacts();

                        if (nFlags > maxFlags)
                        {
                            maxFlags = nFlags;
                            psideWin = pside;
                        }
                        else if (nFlags == maxFlags)
                        {
                            psideWin = NULL;
                        }
                    }
                }

                if ((psideWin == NULL) && pmp->IsFlagsGame())
                {
                    pszClue = "flags";

                    short   maxFlags = 0;
                    for (SideLinkIGC*   psl = pm->GetSides()->first(); (psl != NULL); psl = psl->next())
                    {
                        IsideIGC*   pside = psl->data();

                        short   nFlags = pside->GetFlags();

                        if (nFlags > maxFlags)
                        {
                            maxFlags = nFlags;
                            psideWin = pside;
                        }
                        else if (nFlags == maxFlags)
                        {
                            psideWin = NULL;
                        }
                    }
                }

                if (psideWin == NULL)
                {
                    pszClue = "kills";

                    int     nKills = -1;
                    int     nDeaths = INT_MAX;
                    for (SideLinkIGC*   psl = pm->GetSides()->first(); (psl != NULL); psl = psl->next())
                    {
                        IsideIGC*   pside = psl->data();
                        int         k = pside->GetKills();
                        int         d = pside->GetDeaths();

                        if ((k > nKills) ||
                            ((k == nKills) && (d < nDeaths)))
                        {
                            nKills = k;
                            nDeaths = d;
                            psideWin = pside;
                        }
                        else if ((k == nKills) && (d == nDeaths))
                        {
                            psideWin = NULL;
                        }
                    }
                }

 return {psideWin,pszClue};
}
}
