// Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
// Offline aleph exit behavior from WinTrekClient::HitWarpEvent.
// This is not a substitute for the server-authoritative online transit path.
#pragma once
#include "../Igc/igc.h"

namespace Allegiance {
template <class Random>
void ApplyOfflineWarpExit(IshipIGC *ship, IwarpIGC *warp, IwarpIGC *destination,
                          float exitSpeed, Random random) {
  Orientation alephOrientation = destination->GetOrientation();
  const Vector &v = ship->GetVelocity();
  float speed2 = v.LengthSquared();
  float speed = float(sqrt(speed2));
  float error;
  {
    // How close is the ship coming to the center of the warp?
    Vector dp = warp->GetPosition() - ship->GetPosition();

    float t = speed2 > 0.0f ? (dp * v) / speed2 : 0.0f;
    float d = (dp - t * v).LengthSquared();
    float r = warp->GetRadius();

    error = d / (r * r) + 0.125f; // Error ranges from 0.125 to 1.125
  }

  alephOrientation.Pitch(random(-error, error));
  alephOrientation.Yaw(random(-error, error));

  ship->SetCurrentTurnRate(c_axisRoll,
                           ship->GetCurrentTurnRate(c_axisRoll) +
                               random(pi * 0.5f * error, pi * 2.0f * error));
  const Vector &backward = alephOrientation.GetBackward();
  ship->SetOrientation(alephOrientation);

  speed = -(speed + exitSpeed);
  ship->SetVelocity(backward * speed);
  ship->SetPosition(destination->GetPosition() +
                    (alephOrientation.GetUp() * random(2.0f, 5.0f)) +
                    (alephOrientation.GetRight() * random(2.0f, 5.0f)) -
                    (ship->GetRadius() + 5.0f) * backward);
}
} // namespace Allegiance
