// Frozen WinTrek.cpp at 39ef2c0. OS/client calls are test callbacks.
#pragma once
namespace OriginalCommands {
template<class Sound,class Join,class Rescue,class Select,class Accept,class Reachable,class Engage>
void queued(IshipIGC* ship,bool bExecute,Sound sound,Join join,Rescue rescue,Select select,Accept accept,Reachable reachable,Engage engage) {
            CommandID   cid = ship->GetCommandID(c_cmdQueued);
            if (cid != c_cidNone)
            {
                ImodelIGC*  pmodel = ship->GetCommandTarget(c_cmdQueued);
                if (pmodel && ship->LegalCommand(cid, pmodel))
                {
                    sound();
                    if (cid == c_cidJoin)
                    {
                        if (pmodel->GetObjectType() == OT_ship)
                            join(static_cast<IshipIGC*>(pmodel));
                    }
                    else
                    {
                        if ((cid == c_cidPickup) &&
                            (pmodel->GetObjectType() == OT_ship) &&
                            ship->GetBaseHullType()->HasCapability(c_habmRescue))
                        {
                            //Tell the other person to pick me up as well
                            rescue(static_cast<IshipIGC*>(pmodel));
                        }

                        select(pmodel, cid);
                        accept(pmodel, cid);

                        if (bExecute &&
                            (ship->GetStation() == NULL) &&
                            (ship->GetParentShip() == NULL) &&
                            reachable(pmodel))
                        {
                            engage();
                        }
                    }

                    ship->SetCommand(c_cmdQueued, NULL, c_cidNone);
                }
            }
}
}
