// Frozen original source excerpts, commit 39ef2c072b1eac0e9f97ab1428bc96572cbd5854.
// Only dependencies/parameter names adapted; do not replace with shared helpers.
#pragma once
#include "igc.h"
namespace OriginalLifecycle {
template<class Random>
void Carrier(IshipIGC* pshipCarrier,IshipIGC* pship,float exitSpeed,Random rng) {
        const Orientation&  orientation = pshipCarrier->GetOrientation();

        float   vLaunch = exitSpeed;

        Vector  position = pshipCarrier->GetPosition();
        Vector  velocity = pshipCarrier->GetVelocity();

        position.x += rng(-0.5f, 0.5f);
        position.y += rng(-0.5f, 0.5f);
        position.z += rng(-0.5f, 0.5f);

        IhullTypeIGC*   phtCarrier = pshipCarrier->GetBaseHullType();

        Orientation         orientationBfr;
        const Orientation*  porientation;
        if (phtCarrier->GetLaunchSlots() == 0)
        {
            position -= orientation.GetBackward() * (pshipCarrier->GetRadius() + pship->GetRadius() + vLaunch * 0.5f);
            velocity -= orientation.GetBackward() * vLaunch;

            porientation = &orientation;
        }
        else
        {
            short   slot = pshipCarrier->GetLaunchSlot();

            position += phtCarrier->GetLaunchPosition(slot) * orientation;

            Vector  forward = phtCarrier->GetLaunchDirection(slot) * orientation;

            position += forward * (pship->GetRadius() + vLaunch * 0.5f);
            velocity += forward * vLaunch;

            orientationBfr = orientation;
            orientationBfr.TurnTo(forward);

            porientation = &orientationBfr;
        }

        pship->SetPosition(position);
        pship->SetVelocity(velocity);
        pship->SetOrientation(*porientation);

}
template<class Random>
void Eject(Time timeCollision,IshipIGC* pship,ImodelIGC* pcredit,const Vector& p1,const Vector& p2,Random rng) {
    TRef<IshipIGC> pshipParent=pship->GetParentShip();
    IclusterIGC* pcluster=pship->GetCluster();
    IhullTypeIGC* pht=pship->GetSide()->GetCivilization()->GetLifepod();
    Vector  v = pship->GetSourceShip()->GetVelocity();

    //Let our teammates and any observers know that we are now in an eject pod

    //Put us in the eject pod.
    if (pshipParent)
        pship->SetParentShip(NULL);
    pship->SetBaseHullType(pht);

    Vector      f = rng();
    Vector      position;
    Orientation o;
    if (pshipParent)
    {
        o.Set(f);

        //Start the escape pod well away from the parent ship
        position = p1 - f * (40.0f + 0.5f * pht->GetLength() + pshipParent->GetRadius());
    }
    else
    {
        bool    bLook;
        Vector  vectorLook;
        if (pcredit)
        {
            ImodelIGC*  plauncher = (pcredit->GetObjectType() == OT_ship)
                                    ? ((IshipIGC*)pcredit)->GetSourceShip()
                                    : pcredit;

            if (plauncher->GetCluster() == pcluster)
            {
                bLook = true;
                vectorLook = plauncher->GetPosition();
            }
            else
            {
                bLook = false;
            }
        }
        else
        {
            bLook = true;
            vectorLook = p2;
        }

        if (bLook)
        {
            Vector  dp = (vectorLook - p1);
            float   length2 = dp.LengthSquared();

            if (length2 >= 1.0f)
            {
                o.Set(dp);

                Vector  fNew = (f + dp * (8.0f / float(sqrt(length2))));
                length2 = fNew.LengthSquared();
                if (length2 > 0.01f)
                    f = fNew / float(sqrt(length2));
            }
            else
                o.Set(f);
        }
        else
            o.Set(f);

        position = p1;
    }

    v -= f * 100.0f;
    pship->SetPosition(position);
    pship->SetVelocity(v);
    pship->SetOrientation(o);

    pship->SetCurrentTurnRate(c_axisRoll, pi * 2.0f);
    pship->SetBB(timeCollision, timeCollision, 0.0f);

}
}
