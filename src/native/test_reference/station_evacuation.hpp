// Frozen CFSMission::VacateStation from 39ef2c072b1eac0e9f97ab1428bc96572cbd5854.
// Only the server-wrapper acknowledgement is injected. Do not copy production policy.
#pragma once
#include "igc.h"
namespace OriginalStationEvacuation {
template<class Launched>
void VacateStation(IstationIGC * pstation, Launched launched)
{

    IsideIGC*   psideGhost = NULL;

    //Everyone goes (in the event of a capture: the person capturing has not docked yet)
    const ShipListIGC*  pships = pstation->GetShips();
    ShipLinkIGC*        pshiplink = pships->first();
    while (pshiplink)
    {
        IshipIGC*   pship = pshiplink->data();
        if (pship->GetParentShip() == NULL)
        {
            if (pship->IsGhost())
            {
                //Ghosts ... move the ghosts in the next pass
                psideGhost = pship->GetSide();
                pshiplink = pshiplink->next();
            }
            else
            {
                //Force any kids who haven't undocked to undock
                for (ShipLinkIGC*   psl = pship->GetChildShips()->first();
                   (psl != NULL);
                   psl = psl->next())
                {
                  psl->data()->SetStation(NULL);
                }

                pship->SetStation(NULL);

                launched(pship); // caller retains the CFSShip transport update

                pshiplink = pships->first();
            }

        }
        else
            pshiplink = pshiplink->next();
    }

    if (psideGhost)
    {
        //Find an alternate base for the ghosts ... if they don't have one, don't worry: side
        //will be eliminated
        for (StationLinkIGC*    psl = psideGhost->GetStations()->first(); (psl != NULL); psl = psl->next())
        {
            IstationIGC*    ps = psl->data();
            if ((ps != pstation) && ps->GetBaseStationType()->HasCapability(c_sabmRestart))
            {
                ShipLinkIGC*        pshiplink = pships->first();
                while (pshiplink)
                {
                    IshipIGC*   pship = pshiplink->data();
                    pshiplink = pshiplink->next();

                    assert (pship->GetParentShip() == NULL);
                    assert (pship->IsGhost());
                    pship->SetStation(ps);
                }
                break;
            }
        }
    }
}
}
