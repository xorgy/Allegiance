// Original client combat actions, extracted from clintlib.cpp and trekigc.cpp.
// No renderer, transport, extra virtual interface, or per-frame allocation.
#pragma once
#include "../Igc/igc.h"

namespace Allegiance {
struct MissileLaunchVectors {
  Vector vecPosition, vecForward, vecVelocity;
};

// Writes directly to the caller's records, including the existing Windows
// MissileLaunchData packet array. Packet types/layout and ownership stay
// outside. Preserve the original random draw order, stratified salvo spread,
// and time extrapolation: these values affect combat, not just visual
// appearance.
template <class LaunchData, class Random>
void ComputeMissileLaunches(IshipIGC *pShip, ImagazineIGC *pMagazine,
                            Time timeFired, int iNumMissiles,
                            LaunchData *missileLaunchData, Random random) {
  if (iNumMissiles <= 0)
    return;
  const Vector &myVelocity = pShip->GetVelocity();
  ImissileTypeIGC *missileType = pMagazine->GetMissileType();
  const Orientation &myOrientation = pShip->GetOrientation();
  Vector myPosition =
      pShip->GetPosition() + pMagazine->GetEmissionPt() * myOrientation;
  Time lastUpdate = pShip->GetLastUpdate();
  Vector vecLaunchPosition =
      myPosition + (myVelocity * (timeFired - lastUpdate));
  Vector vecLaunchForward = myOrientation.GetForward();
  Vector vecLaunchVelocity = myVelocity;

  // disperse the missiles over the launch region
  float fDispersion = missileType->GetDispersion();
  float fDispersionBase = -fDispersion;
  float fUniformDispersion = (2.0f * fDispersion) / (float)iNumMissiles;
  float fHalfUniformDispersion = fUniformDispersion * 0.5f;
  for (int i = 0; i < iNumMissiles; i++) {
    missileLaunchData[i].vecForward = vecLaunchForward;
    if (fDispersion != 0.0f) {
      float fRandomDispersion =
          (random(-0.75f, 0.75f) * fHalfUniformDispersion) +
          (fDispersionBase + fHalfUniformDispersion);
      fDispersionBase += fUniformDispersion;
      missileLaunchData[i].vecForward +=
          fRandomDispersion * myOrientation.GetRight();
      missileLaunchData[i].vecForward +=
          (random(-0.1f, 0.1f) * fDispersion) * myOrientation.GetUp();
      missileLaunchData[i].vecForward.SetNormalize();
    }
    ZRetailAssert(missileLaunchData[i].vecForward *
                      missileLaunchData[i].vecForward >=
                  0.95f);
    ZRetailAssert(missileLaunchData[i].vecForward *
                      missileLaunchData[i].vecForward <=
                  1.05f);

    missileLaunchData[i].vecPosition =
        vecLaunchPosition +
        (missileLaunchData[i].vecForward *
         (missileType->GetInitialSpeed() * random(0.1f, 0.2f)));
    ZRetailAssert(missileLaunchData[i].vecPosition *
                      missileLaunchData[i].vecPosition >=
                  0.0f);
    ZRetailAssert(missileLaunchData[i].vecPosition *
                      missileLaunchData[i].vecPosition <=
                  4.0e10f);

    missileLaunchData[i].vecVelocity =
        vecLaunchVelocity +
        (missileType->GetInitialSpeed() * missileLaunchData[i].vecForward);
    ZRetailAssert(missileLaunchData[i].vecVelocity *
                      missileLaunchData[i].vecVelocity >=
                  0.0f);
    // ZRetailAssert(missileLaunchData[i].vecVelocity *
    // missileLaunchData[i].vecVelocity <= 1.0e6);
    // //Imago 8/12/09
  }
}

// Local authority only. Connected Windows clients retain their existing request
// path, while their offline path and the native session share object creation.
template <class Random>
bool CreateOfflineExpendable(IshipIGC *pShip, IdispenserIGC *pDispenser,
                             Time timeFired, Random random) {
  ImissionIGC *mission = pShip->GetMission();
  IclusterIGC *pCluster = pShip->GetCluster();
  if (!pCluster || pDispenser->GetAmount() <= 0)
    return false;
  IexpendableTypeIGC *pet = pDispenser->GetExpendableType();
  ObjectType type = pet->GetObjectType();
  const Vector &myPosition = pShip->GetPosition();
  const Vector &myVelocity = pShip->GetVelocity();

  if (type == OT_chaffType) {
    assert(type == OT_chaffType);

    // Drop the chaff "behind" the ship

    DataChaffIGC dataChaff{};

    dataChaff.time0 = timeFired;
    dataChaff.p0 = myPosition;
    dataChaff.v0 = myVelocity + pShip->GetOrientation().GetUp() * 25.0f;

    dataChaff.pcluster = pCluster;
    dataChaff.pchafftype = (IchaffTypeIGC *)pet;

    IchaffIGC *c = (IchaffIGC *)(mission->CreateObject(
        timeFired, OT_chaff, &dataChaff, sizeof(dataChaff)));
    assert(c != NULL);

    // Confuse any missiles lauched at the ship
    for (MissileLinkIGC *pml = pCluster->GetMissiles()->first(); (pml != NULL);
         pml = pml->next()) {
      ImissileIGC *pmissile = pml->data();
      if (pmissile->GetTarget() == pShip) {
        // A missile aimed at me .. does the chaff work?
        float chaff = ((IchaffTypeIGC *)pet)->GetChaffStrength();
        float missile = pmissile->GetMissileType()->GetChaffResistance();

        // The following is equivalent to random(0, chaff) > random(0, missile)
        float cm = chaff * missile;
        float f = (chaff > missile) ? (cm - 0.5f * missile * missile)
                                    : (0.5f * chaff * chaff);

        if (random(0.0f, cm) <= f) {
          // Missile lost lock
          pmissile->SetTarget(c);
        }
      }
    }

    c->Release();
  } else {

    float speed2 = myVelocity.LengthSquared();
    float offset = (pet->GetRadius() + pShip->GetRadius()) + 5.0f;
    Vector displace = (speed2 < 1.0f)
                          ? (pShip->GetOrientation().GetBackward() * offset)
                          : (myVelocity * (-offset / float(sqrt(speed2))));
    Vector launchPosition = myPosition + displace;
    if (type == OT_mineType) {
      DataMineIGC dataMine{};
      dataMine.pminetype = (ImineTypeIGC *)pet;

      dataMine.time0 = timeFired + 3.0f; // And the count shall be 3.
      dataMine.p0 = launchPosition;
      dataMine.exportF = false;

      dataMine.psideLauncher =
          pShip->GetSide(); // bahdohday (commit by AEM 7.12.07) Fix to allow
                            // mines to be dropped in training.
      dataMine.pshipLauncher = pShip;
      dataMine.pcluster = pCluster;

      dataMine.mineID = mission->GenerateNewMineID();

      ImineIGC *m = (ImineIGC *)(mission->CreateObject(
          timeFired, OT_mine, &dataMine, sizeof(dataMine)));
      assert(m != NULL);
      m->Release();
    } else if (type == OT_probeType) {
      // Drop the probe "behind" the player's ship

      DataProbeIGC dataProbe{};
      dataProbe.pprobetype = (IprobeTypeIGC *)pet;

      dataProbe.time0 = timeFired;
      dataProbe.p0 = launchPosition;
      dataProbe.exportF = false;

      dataProbe.pcluster = pCluster;
      dataProbe.pside = pShip->GetSide();
      dataProbe.pship = pShip;
      dataProbe.pmodelTarget = pShip->GetCommandTarget(c_cmdCurrent);

      dataProbe.probeID = mission->GenerateNewProbeID();

      IprobeIGC *p = (IprobeIGC *)(mission->CreateObject(
          timeFired, OT_probe, &dataProbe, sizeof(dataProbe)));
      assert(p != NULL);
      p->Release();
    }
  }
  return type == OT_chaffType || type == OT_mineType || type == OT_probeType;
}
} // namespace Allegiance
