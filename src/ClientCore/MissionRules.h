// Original FedSrv mission rules. Adapters own player removal, notifications and
// transport; these functions operate only on IGC state and allocate no storage.
#pragma once
#include "../Igc/igc.h"

namespace Allegiance {
// CFSMission::HasPlayers. Adapters retain their player/drone classification.
template<class IsPlayer>
bool MissionHasPlayers(ImissionIGC* mission,IsideIGC* side,bool countGhosts,IsPlayer isPlayer) {
    auto* list=side?side->GetShips():mission->GetShips();
    for(auto* link=list->first();link;link=link->next()) {
        auto* ship=link->data();if(isPlayer(ship) && (countGhosts || !ship->IsGhost()))return true;
    }
    return false;
}
struct StationVictory {
    IsideIGC* winner{};
    IsideIGC* eliminated{};
    bool changed{};
    bool alliances{};
};
inline StationVictory CheckStationVictory(ImissionIGC* mission,IstationIGC* killed,IsideIGC* oldSide) {
    StationVictory result;
    const auto* params=mission->GetMissionParams();
    const auto* sides=mission->GetSides();
    if(params->IsConquestGame()) {
        int total=0,counts[c_cSidesMax]={};
        for(auto* l=mission->GetStations()->first();l;l=l->next()) {
            auto* station=l->data();
            if(station!=killed && station->GetBaseStationType()->HasCapability(c_sabmFlag)) {
                ++total;++counts[station->GetSide()->GetObjectID()];
            }
        }
        int active=0;
        if(total) {
            for(auto* l=sides->first();l;l=l->next()) {
                auto* side=l->data();
                if(side->GetActiveF())++active;
                if(side->GetAllies()!=NA)result.alliances=true;
                unsigned char percent=100*counts[side->GetObjectID()]/total;
                if(percent!=side->GetConquestPercent()) {
                    result.changed=true;side->SetConquestPercent(percent);
                    if(percent>=params->iGoalConquestPercentage)result.winner=side;
                }
            }
            if(params->bAllowEmptyTeams && active==1)result.winner=nullptr;
        }
    }
    if(params->IsTerritoryGame() && !result.winner) {
        unsigned char counts[c_cSidesMax]={};
        auto* clusters=mission->GetClusters();
        for(auto* l=clusters->first();l;l=l->next()) {
            IsideIGC* owner=nullptr;auto* st=l->data()->GetStations()->first();
            for(;st;st=st->next())if(st->data()!=killed) {
                auto* side=st->data()->GetSide();
                if(!owner)owner=side;else if(owner!=side)break;
            }
            if(owner && !st)++counts[owner->GetObjectID()];
        }
        unsigned char threshold=(50+clusters->n()*params->iGoalTerritoryPercentage)/100;
        for(auto* l=sides->first();l;l=l->next()) {
            auto* side=l->data();auto count=counts[side->GetObjectID()];
            if(count!=side->GetTerritoryCount()) {
                result.changed=true;side->SetTerritoryCount(count);
                if(count>=threshold)result.winner=side;
            }
        }
    }
    if(!result.winner && oldSide) {
        auto* l=oldSide->GetStations()->first();
        for(;l;l=l->next())if(l->data()!=killed && l->data()->GetStationType()->HasCapability(c_sabmRestart))break;
        if(!l)result.eliminated=oldSide;
    }
    return result;
}
// FedSrv checks this again after side deactivation. Return the same last active
// side as the original nested loop; alliances do not alter conquest percentages.
inline IsideIGC* AlliedSurvivor(ImissionIGC* mission) {
    IsideIGC* winner=nullptr;
    for(auto* l=mission->GetSides()->first();l;l=l->next()) {
        auto* side=l->data();if(!side->GetActiveF())continue;
        for(auto* other=mission->GetSides()->first();other;other=other->next()) {
            auto* candidate=other->data();if(!candidate->GetActiveF())continue;
            if(candidate!=side && !IsideIGC::AlliedSides(side,candidate))return nullptr;
            winner=candidate;
        }
    }
    return winner;
}

struct TimedVictory { IsideIGC* winner;const char* reason; };
// CFSMission::DoTick tie-break order: enabled objectives, then kills/deaths.
inline TimedVictory CheckTimedVictory(ImissionIGC* pm) {
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
inline IsideIGC* CheckKillVictory(IsideIGC* side) {
    auto* mission=side->GetMission();const auto* params=mission->GetMissionParams();
    return params->IsDeathMatchGame() && side->GetKills()>=params->nGoalTeamKills?side:NULL;
}
}
