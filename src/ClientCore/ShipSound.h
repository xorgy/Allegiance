// ShipSoundSite's engine response, shared by DirectSound and OpenAL adapters.
#pragma once
#include "../Igc/igc.h"
#include <algorithm>

namespace Allegiance
{
// WinTrekClient::PlayNotificationSound's ship/parent routing. Callers supply a
// live player; world effects have separate spatial/sector rules.
inline bool ReceivesShipNotification(IshipIGC *player, ImodelIGC *model)
{
    return model == player || model == player->GetSourceShip();
}

inline float ShipTurnSoundLevel(IshipIGC *ship)
{
    const ControlData &controls = ship->GetControls();
    return controls.jsValues[c_axisYaw] * controls.jsValues[c_axisYaw] +
           controls.jsValues[c_axisPitch] * controls.jsValues[c_axisPitch] +
           controls.jsValues[c_axisRoll] * controls.jsValues[c_axisRoll];
}
// WinTrek's incoming-missile warning is independent of visibility. Fold it
// into the existing sector sound traversal without allocating another list.
struct IncomingMissileWarning {
    bool active=false;float lock=0;
    void Observe(ImissileIGC* missile,IshipIGC* source) {
        if(missile->GetTarget()==source){active=true;lock=(std::max)(lock,missile->GetLock());}
    }
    float Pitch()const{return .75f+lock/2;}
};
struct ShipSoundLevels
{
    float thrust = 0, turn = 0;
    void Update(float forward, float sidewaysOrTurn, float dt, bool reset)
    {
        if (reset)
        {
            thrust = forward;
            turn = sidewaysOrTurn;
        }
        else
        {
            // Original rates were per millisecond: .0005 and .002.
            thrust = (std::max)(thrust - .5f * dt, (std::min)(thrust + .5f * dt, forward));
            turn = (std::max)(turn - 2 * dt, (std::min)(turn + 2 * dt, sidewaysOrTurn));
        }
    }
    void Update(IshipIGC *ship, float dt, bool reset)
    {
        const IhullTypeIGC *hull = ship->GetHullType();
        float forward = (std::max)(ship->GetControls().jsValues[c_axisThrottle] * .5f + .5f,
                                   (ship->GetStateM() & afterburnerButtonIGC) ? 1.0f : 0.0f);
        float sideThrust = hull->GetThrust() / hull->GetSideMultiplier();
        const Vector &backward = ship->GetOrientation().GetBackward();
        float forwardThrust = (std::max)(0.0f, -1 * ship->GetEngineVector() * backward);
        Vector side = ship->GetEngineVector() + forwardThrust * backward;
        float fraction = sideThrust > 0 ? side.LengthSquared() / (sideThrust * sideThrust) : 0;
        float turning = (std::min)(1.0f, (std::max)(ShipTurnSoundLevel(ship) * 20, fraction));
        Update(forward, turning, dt, reset);
    }
    float MainGain(bool interior) const
    {
        float factor = 1 - thrust;
        if (interior)
        {
            factor *= factor;
            factor *= factor;
            factor *= factor;
            factor *= factor;
            factor *= factor;
            factor *= factor;
            return factor * -60;
        }
        return factor * -40;
    }
    float TurnGain(bool interior) const
    {
        float factor = 1 - turn;
        return interior ? factor * factor * -60 : factor * -40;
    }
    float MainPitch() const
    {
        return .5f + thrust / 2;
    }
    float TurnPitch(bool interior) const
    {
        return .5f + turn / (interior ? 3 : 2);
    }
};
} // namespace Allegiance

namespace Allegiance
{
struct WeaponSoundSelection
{
    SoundID active=NA,burst=NA,turret=NA,shot=NA;
};
// Original ShipSoundSite activation/burst/turret rules. Keeping the previous
// burst bit suppresses the final shot when an ASR burst is releasing.
inline WeaponSoundSelection SelectWeaponSounds(IshipIGC* ship,Mount mount,IshipIGC* player,bool wasBurst)
{
    WeaponSoundSelection sounds;
    auto* weapon=static_cast<IweaponIGC*>(ship->GetMountedPart(ET_Weapon,mount));
    if(!weapon)return sounds;
    if(weapon->fActive())sounds.active=weapon->GetActivateSound();
    if(weapon->GetGunner() && (weapon->fActive() || weapon->GetGunner()==player) && ShipTurnSoundLevel(weapon->GetGunner())>.01f)
        sounds.turret=ship->GetHullType()->GetHardpointData(mount).turnSound;
    if(weapon->fFiringBurst())sounds.burst=weapon->GetBurstSound();
    if(weapon->fFiringShot() && sounds.burst==NA && !wasBurst)sounds.shot=weapon->GetSingleShotSound();
    return sounds;
}
struct ShipSoundNotifications
{
    float hull=1,ammo=1,fuel=1,energy=1;
    bool emptyWeapon=false,emptyBooster=false;
    int ripcordCountdown=11;
    template<class Emit> void Update(IshipIGC* player,Emit emit)
    {
        IshipIGC* ship=player->GetSourceShip();const auto* type=ship->GetHullType();
        if(!type)return;
        float h=ship->GetFraction();
        float a=type->GetMaxAmmo()>0?ship->GetAmmo()/float(type->GetMaxAmmo()):1;
        float f=type->GetMaxFuel()>0?ship->GetFuel()/type->GetMaxFuel():1;
        float e=type->GetMaxEnergy()>0?ship->GetEnergy()/type->GetMaxEnergy():1;
        if(h<.15f && hull>=.15f)emit(hullLowSound);
        if(a<.15f && ammo>=.15f)emit(ammoLowSound);
        if(f<.15f && fuel>=.15f)emit(fuelLowSound);
        if(e<.05f && energy>=.05f)emit(energyLowSound);
        hull=h;ammo=a;fuel=f;energy=e;
        bool empty=false;Mount turret=player->GetTurretID();
        Mount first=turret==NA?0:turret,end=turret==NA?type->GetMaxFixedWeapons():turret+1;
        for(Mount m=first;m<end;++m) {
            const auto* w=static_cast<IweaponIGC*>(ship->GetMountedPart(ET_Weapon,m));
            empty|=w && w->fActive() && w->GetMountedFraction()>=1 && w->GetAmmoPerShot()>ship->GetAmmo();
        }
        auto* booster=static_cast<IafterburnerIGC*>(ship->GetMountedPart(ET_Afterburner,0));
        bool emptyFuel=booster && (ship->GetStateM()&afterburnerButtonIGC) && ship->GetFuel()<=0 && booster->GetMountedFraction()>=1;
        if(empty && !emptyWeapon)emit(salNoAmmoSound);
        if(emptyFuel && !emptyBooster)emit(salNoFuelSound);
        emptyWeapon=empty;emptyBooster=emptyFuel;
        int countdown=ship->fRipcordActive()?1+int(ship->GetRipcordTimeLeft()):11;
        if(countdown!=ripcordCountdown && countdown>=1 && countdown<=10) {
            const SoundID countdowns[]={ripcord1Sound,ripcord2Sound,ripcord3Sound,ripcord4Sound,ripcord5Sound,
                ripcord6Sound,ripcord7Sound,ripcord8Sound,ripcord9Sound,ripcord10Sound};
            emit(countdowns[countdown-1]);
        }
        ripcordCountdown=countdown;
    }
};
}

namespace Allegiance
{
inline SoundID ShipImpactSound(bool ownShip,bool shield)
{
    return ownShip?(shield?myShieldHitSound:myHullHitSound):(shield?otherShieldHitSound:otherHullHitSound);
}
inline SoundID ExplosionSound(float radius)
{
    return radius>100?largeExplosionSound:radius>10?mediumExplosionSound:smallExplosionSound;
}
inline SoundID DroneUnderAttackSound(IshipIGC* ship)
{
    switch(ship->GetPilotType()) {
    case c_ptCarrier:return carrierUnderAttackSound;
    case c_ptMiner:return ship->GetFraction()<.5f?minerCriticalSound:minerUnderAttackSound;
    case c_ptBuilder:{auto* type=static_cast<IstationTypeIGC*>(static_cast<IbaseIGC*>(ship->GetBaseData()));return type?type->GetConstructorUnderAttackSound():droneUnderAttackSound;}
    default:return droneUnderAttackSound;
    }
}
}
