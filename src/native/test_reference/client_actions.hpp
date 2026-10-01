// WinTrek excerpts: Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
// Frozen reference excerpts from 39ef2c072b1eac0e9f97ab1428bc96572cbd5854.
// Source: src/clintlib/clintlib.cpp (BaseClient::FireMissile/Reload) and
// src/WinTrek/trekigc.cpp (WinTrekClient::HitWarpEvent).
// Test-only; never compiled into either client. Do not update alongside production.
// Adaptations: injected RNG/output array, caller-supplied exit speed; transport,
// presentation notifications and outer preconditions omitted. Inventory includes
// the original commented-out algorithm to retain exact source provenance.
#pragma once
namespace OriginalReference {
template<class Record,class Random>
void Missile(IshipIGC* pShip, ImagazineIGC* pMagazine, Time timeFired,
             int iNumMissiles, Record* missileLaunchData, Random random) {
    ImodelIGC* pTarget = nullptr;
    IclusterIGC* pCluster = pShip->GetCluster();
    float flLock = 1;
    const Vector&       myVelocity      = pShip->GetVelocity();
    const Orientation&  myOrientation   = pShip->GetOrientation();
    Vector              myPosition      = pShip->GetPosition() +
                                          pMagazine->GetEmissionPt() * myOrientation;

    int                 iNumRemainingMissiles = pMagazine->GetAmount ();
    Time                lastUpdate = pShip->GetLastUpdate();

    if (iNumRemainingMissiles < iNumMissiles)
        iNumMissiles = iNumRemainingMissiles;
    iNumRemainingMissiles -= iNumMissiles;

    DataMissileIGC      dataMissile;
    dataMissile.pmissiletype    = pMagazine->GetMissileType();
    dataMissile.pLauncher       = pShip;
    dataMissile.pTarget         = pTarget;
    dataMissile.pCluster        = pCluster;
    dataMissile.lock            = flLock;


    Vector              vecLaunchPosition = myPosition + (myVelocity * (timeFired - lastUpdate));
    Vector              vecLaunchForward = myOrientation.GetForward();
    Vector              vecLaunchVelocity = myVelocity;

    // disperse the missiles over the launch region
    float               fDispersion = dataMissile.pmissiletype->GetDispersion();
    float               fDispersionBase = -fDispersion;
    float               fUniformDispersion = (2.0f * fDispersion) / (float) iNumMissiles;
    float               fHalfUniformDispersion = fUniformDispersion * 0.5f;
    for (int i = 0; i < iNumMissiles; i++)
    {
        missileLaunchData[i].vecForward = vecLaunchForward;
        if (fDispersion != 0.0f)
        {
            float   fRandomDispersion = (random (-0.75f, 0.75f) * fHalfUniformDispersion) + (fDispersionBase + fHalfUniformDispersion);
            fDispersionBase += fUniformDispersion;
            missileLaunchData[i].vecForward += fRandomDispersion * myOrientation.GetRight();
            missileLaunchData[i].vecForward += (random (-0.1f, 0.1f) * fDispersion) * myOrientation.GetUp();
            missileLaunchData[i].vecForward.SetNormalize();
        }
        ZRetailAssert(missileLaunchData[i].vecForward * missileLaunchData[i].vecForward >= 0.95f);
        ZRetailAssert(missileLaunchData[i].vecForward * missileLaunchData[i].vecForward <= 1.05f);

        missileLaunchData[i].vecPosition = vecLaunchPosition + (missileLaunchData[i].vecForward * (dataMissile.pmissiletype->GetInitialSpeed() * random (0.1f, 0.2f)));
        ZRetailAssert(missileLaunchData[i].vecPosition * missileLaunchData[i].vecPosition >= 0.0f);
        ZRetailAssert(missileLaunchData[i].vecPosition * missileLaunchData[i].vecPosition <= 4.0e10f);

        missileLaunchData[i].vecVelocity = vecLaunchVelocity + (dataMissile.pmissiletype->GetInitialSpeed() * missileLaunchData[i].vecForward);
        ZRetailAssert(missileLaunchData[i].vecVelocity * missileLaunchData[i].vecVelocity >= 0.0f);
        //ZRetailAssert(missileLaunchData[i].vecVelocity * missileLaunchData[i].vecVelocity <= 1.0e6); //Imago 8/12/09
    }

}
template<class Record>
int Reload(IshipIGC* pship, IlauncherIGC*& plauncher, EquipmentType type, Record (&reloads)[8]) {
 int nReloads=0;
        const IhullTypeIGC* pht = pship->GetHullType();

        switch (type)
        {
            case ET_Weapon:
            case ET_Afterburner:
            {
                PackType    ptDesired;
                short       ammo;
                short       maxAmmo;
                if (type == ET_Weapon)
                {
                    ptDesired = c_packAmmo;
                    ammo = pship->GetAmmo();
                    maxAmmo = pht->GetMaxAmmo();
                }
                else
                {
                    ptDesired = c_packFuel;
                    ammo = short(pship->GetFuel());
                    maxAmmo = short(pht->GetMaxFuel());
                }

                while (ammo < maxAmmo)
                {
                    /*
                    //Find the ammo/fuel pack with the least amount of ammo/fuel in it (first if all the same)
                    IpackIGC*   ppack = NULL;
                    short       amount = 0x7fff;
                    {
                        for (Mount i = -1; (i >= -c_maxCargo); i--)
                        {
                            IpartIGC*   ppart = pship->GetMountedPart(NA, i);

                            if (ppart && (ppart->GetObjectType() == OT_pack))
                            {
                                IpackIGC*   p = (IpackIGC*)ppart;

                                if (p->GetPackType() == ptDesired)
                                {
                                    short   a = p->GetAmount();
                                    if (a < amount)
                                    {
                                        ppack = p;
                                        amount = a;
                                    }
                                }
                            }
                        }
                    }
                    */
                    // Find the last ammo/fuel pack (avoids mysterious behavior in inventory pane)
                    IpackIGC*   ppack = NULL;
                    short       amount = 0x7fff;
                    {
                        for (Mount i = -1; (i >= -c_maxCargo); i--)
                        {
                            IpartIGC*   ppart = pship->GetMountedPart(NA, i);

                            if (ppart && (ppart->GetObjectType() == OT_pack))
                            {
                                IpackIGC*   p = (IpackIGC*)ppart;

                                if (p->GetPackType() == ptDesired)
                                {
                                    ppack = p;
                                    amount = p->GetAmount();
                                }
                            }
                        }
                    }

                    if (ppack)
                    {
                        reloads[nReloads].mount = ppack->GetMountID();
                        short   transfer;
                        if (ammo + amount > maxAmmo)
                        {
                            //There will be stuff left in the pack
                            transfer = maxAmmo - ammo;
                            ammo = maxAmmo;

                            ppack->SetAmount(amount - transfer);
                        }
                        else
                        {
                            //Entire pack gets trasnfered
                            transfer = NA;
                            ammo += amount;

                            ppack->Terminate();
                        }
                        reloads[nReloads++].amountTransfered = transfer;
                    }
                    else
                        break;
                }

                if (nReloads != 0)
                {
                    if (type == ET_Weapon)
                    {
                        pship->SetAmmo(ammo);

                        //disable all mounted weapons that use ammo
                        Mount   maxWeapons = pht->GetMaxWeapons();
                        for (Mount i = 0; (i < maxWeapons); i++)
                        {
                            IweaponIGC* pw = (IweaponIGC*)(pship->GetMountedPart(ET_Weapon, i));
                            if (pw && (pw->GetAmmoPerShot() != 0))
                                pw->SetMountedFraction(0.0f);
                        }
                    }
                    else
                    {
                        pship->SetFuel(float(ammo));

                        //disable the afterburner
                        IpartIGC*   p = pship->GetMountedPart(ET_Afterburner, 0);
                        if (p)
                            p->SetMountedFraction(0.0f);
                    }
                }
            }
            break;

            case ET_Magazine:
            case ET_Dispenser:
            case ET_ChaffLauncher:
            {
                IpartTypeIGC*   pparttype;
                short           ammo;
                short           maxAmmo;
                if (plauncher)
                {
                    pparttype = plauncher->GetPartType();
                    ammo = plauncher->GetAmount();
                    maxAmmo = ((IlauncherTypeIGC*)pparttype)->GetAmount(pship);
                }
                else
                {
                    pparttype = NULL;
                    ammo = 0;
                    maxAmmo = 0x7fff;
                }

                while (ammo < maxAmmo)
                {
                    IlauncherIGC*       plauncherReload = NULL;
                    IpartTypeIGC*       pptReload;
                    short               amount = 0x7fff;

                    {
                        for (Mount mount = -1; (mount >= -c_maxCargo); mount--)
                        {
                            IpartIGC*       p = pship->GetMountedPart(NA, mount);
                            if (p && (p->GetEquipmentType() == type))
                            {
                                IpartTypeIGC*   ppt = p->GetPartType();

                                if (pht->CanMount(ppt, 0))
                                {
                                    short           a = ((IlauncherIGC*)p)->GetAmount();
                                    assert (a > 0);

                                    if (pparttype == NULL)
                                    {
                                        //when one has nothing ... one will pick anything
                                        reloads[nReloads].amountTransfered = NA;
                                        reloads[nReloads++].mount = mount;

                                        if (plauncher)
                                            plauncher->Terminate();

                                        p->SetMountID(0);

                                        plauncher = (IlauncherIGC*)p;
                                        pparttype = plauncher->GetPartType();
                                        ammo = plauncher->GetAmount();
                                        assert (ammo > 0);
                                        maxAmmo = ((IlauncherTypeIGC*)pparttype)->GetAmount(pship);

                                        assert (plauncherReload == NULL);

                                        if (ammo == maxAmmo)
                                            break;              //No point in continuing if we are full up
                                    }
                                    else if ((ppt == pparttype) /*&& (a < amount)*/)
                                    {
                                        plauncherReload = (IlauncherIGC*)p;
                                        pptReload = ppt;
                                        amount = a;
                                    }
                                }
                            }
                        }
                    }

                    if (plauncherReload)
                    {
                        reloads[nReloads].mount = plauncherReload->GetMountID();

                        short   transfer;
                        if (ammo + amount <= maxAmmo)
                        {
                            transfer = amount;
                            plauncherReload->Terminate();
                        }
                        else
                        {
                            transfer = maxAmmo - ammo;
                            plauncherReload->SetAmount(amount - transfer);
                        }

                        ammo += transfer;

                        plauncher->SetAmount(ammo);
                        plauncher->SetMountedFraction(0.0f);
                        plauncher->ResetTimeLoaded();

                        reloads[nReloads++].amountTransfered = transfer;
                    }
                    else if (pparttype && (ammo == 0))
                    {
                        //Couldn't find a way to reload the existing launcher
                        //try again being less picky
                        assert (plauncher);

                        pparttype = NULL;
                        maxAmmo = 0x7fff;
                    }
                    else
                        break;
                }
            }
            break;
        }


 return nReloads;
}
template<class Random>
void Warp(IshipIGC* ship, IwarpIGC* warp, IwarpIGC* destination, float exitSpeed, Random random) {
                Orientation alephOrientation = destination->GetOrientation();
                const Vector&   v = ship->GetVelocity();
                float           speed2 = v.LengthSquared();
                float           speed  = float(sqrt(speed2));
                float           error;
                {
                    //How close is the ship coming to the center of the warp?
                    Vector          dp = warp->GetPosition() - ship->GetPosition();

                    float   t = (dp * v) / v.LengthSquared();
                    float   d = (dp - t * v).LengthSquared();
                    float   r = warp->GetRadius();

                    error = d / (r*r) + 0.125f;      //Error ranges from 0.125 to 1.125
                }

                alephOrientation.Pitch(random(-error, error));
                alephOrientation.Yaw(random(-error, error));

                ship->SetCurrentTurnRate(c_axisRoll,
                                         ship->GetCurrentTurnRate(c_axisRoll) +
                                         random(pi * 0.5f * error, pi * 2.0f * error));
                const Vector&   backward = alephOrientation.GetBackward();
                ship->SetOrientation(alephOrientation);

                speed = -(speed + exitSpeed);
                ship->SetVelocity(backward * speed);
                ship->SetPosition(destination->GetPosition() +
                                  (alephOrientation.GetUp() * random(2.0f, 5.0f)) +
                                  (alephOrientation.GetRight() * random(2.0f, 5.0f)) -
                                  (ship->GetRadius() + 5.0f) * backward);
}
}
