// Numeric loadout-panel policy from WinTrek/load.cpp, independent of its UI.
#pragma once
#include "../Igc/igc.h"
#include "LoadoutMetrics.h"
#include <cmath>
#include <string>

namespace Allegiance {
template<class Post>
void LoadoutPresetFeedback(const char* hull,int bank,bool saved,Post post) {
    post(false,saved?"Saved %s loadout %d":"Loaded %s loadout %d",hull,bank);
}
inline std::string BuyableName(const char* name,int price){return price?std::string(name)+" $("+std::to_string(price)+")":std::string(name);}

inline LoadoutPartMetrics DescribeLoadoutPart(ImissionIGC* mission,const IhullTypeIGC* hull,const GlobalAttributeSet& ga,IpartTypeIGC* part,bool baseHull=false) {
    using enum LoadoutMetric;
    LoadoutPartMetrics metrics;auto emit=[&](LoadoutMetric name,float value){metrics.set(name,value);};
    auto ratio=[](float a,float b){return b!=0?a/b:0.f;};
    auto signature=part->GetSignature()/ga.GetAttribute(c_gaSignature);
    emit(Signature,signature);emit(Mass,part->GetMass());emit(EquipmentType,part->GetEquipmentType());
    switch(part->GetEquipmentType()) {
    case ET_Weapon: {
        auto* w=static_cast<const DataWeaponTypeIGC*>(part->GetData());auto* p=mission->GetProjectileType(w->projectileTypeID);if(!p)break;
        float rate=ratio(1,w->dtimeBurst),speed=p->GetSpeed()*(w->cAmmoPerShot?ga.GetAttribute(c_gaSpeedAmmo):1);
        emit(WeaponDamage,(p->GetPower()+p->GetBlastPower())*rate*ga.GetAttribute(c_gaDamageGuns));
        emit(WeaponRange,speed*p->GetLifespan()*(w->cAmmoPerShot?1:ga.GetAttribute(c_gaLifespanEnergy)));
        emit(WeaponSpeed,speed);emit(WeaponRate,rate);emit(WeaponDamageClass,p->GetDamageType());break;
    }
    case ET_Magazine: {
        auto* m=static_cast<ImissileTypeIGC*>(static_cast<IlauncherTypeIGC*>(part)->GetExpendableType());float life=m->GetLifespan();
        emit(MagazineDamage,(m->GetPower()+m->GetBlastPower())*ga.GetAttribute(c_gaDamageMissiles));
        emit(MagazineRange,m->GetInitialSpeed()*life+.5f*m->GetAcceleration()*life*life);
        emit(MagazineLockTime,m->GetLockTime());emit(MagazineMaxLock,m->GetMaxLock());emit(MagazineDamageClass,m->GetDamageType());break;
    }
    case ET_ChaffLauncher: {
        auto* c=static_cast<const DataChaffTypeIGC*>(static_cast<IlauncherTypeIGC*>(part)->GetExpendableType()->GetData());
        float strength=c->chaffStrength<=1?c->chaffStrength/2:1-1/(2*c->chaffStrength);emit(ChaffStrength,strength);break;
    }
    case ET_Dispenser: {
        auto* e=static_cast<IlauncherTypeIGC*>(part)->GetExpendableType();
        if(e->GetObjectType()==OT_probeType){auto* p=static_cast<IprobeTypeIGC*>(e);auto* d=static_cast<const DataProbeTypeIGC*>(p->GetData());auto* projectile=p->GetProjectileType();
            float range=projectile?projectile->GetLifespan()*projectile->GetSpeed()/1.2f*(p->GetAmmo()?ga.GetAttribute(c_gaSpeedAmmo):1):d->scannerRange*ga.GetAttribute(c_gaScanRange);
            emit(ProbeScanRange,range);emit(ProbeVisibility,d->signature/ga.GetAttribute(c_gaSignature));emit(ProbeHitPts,d->hitPoints);emit(ProbeLifespan,d->lifespan);
        }else if(e->GetObjectType()==OT_mineType){auto* d=static_cast<const DataMineTypeIGC*>(e->GetData());emit(EquipmentType,-4);
            emit(MineDamage,d->power);emit(MineVisibility,d->signature/ga.GetAttribute(c_gaSignature));emit(MineRadius,d->radius);emit(MineLifespan,d->lifespan);}
        break;
    }
    case ET_Shield: {
        auto* s=static_cast<const DataShieldTypeIGC*>(part->GetData());float hp=s->maxStrength*ga.GetAttribute(c_gaMaxShieldShip);
        emit(ShieldRegen,ratio(hp,s->rateRegen*ga.GetAttribute(c_gaShieldRegenerationShip)));emit(ShieldHitPts,hp);emit(ShieldDefenseClass,s->defenseType);break;
    }
    case ET_Cloak: {
        auto* c=static_cast<const DataCloakTypeIGC*>(part->GetData());float recharge=hull->GetRechargeRate();
        float energy=hull->GetMaxEnergy()*(baseHull?ga.GetAttribute(c_gaMaxEnergy):1.f);
        float duration=c->energyConsumption<=recharge?-1:ratio(energy,c->energyConsumption-recharge);
        emit(CloakCloaking,c->maxCloaking);emit(CloakDuration,duration);emit(CloakEnergy,energy);emit(CloakRecharge,recharge);break;
    }
    case ET_Afterburner: {
        auto* a=static_cast<const DataAfterburnerTypeIGC*>(part->GetData());float thrust=static_cast<const DataHullTypeIGC*>(hull->GetData())->thrust;
        float hullSpeed=hull->GetMaxSpeed()*(baseHull?ga.GetAttribute(c_gaMaxSpeed):1.f);
        float speed=hullSpeed*(1+ratio(a->maxThrust,thrust));
        emit(AfterburnerMaxSpeed,speed);emit(AfterburnerBurnRate,ratio(hull->GetMaxFuel(),a->maxThrust*a->fuelConsumption));emit(AfterburnerHullSpeed,hullSpeed);emit(AfterburnerHullThrust,thrust);break;
    }
    default:break;
    }
    return metrics;
}
}
