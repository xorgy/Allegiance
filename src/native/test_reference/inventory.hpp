// Frozen original PartWrapper calculations for equivalence checks.
#pragma once
#include "../../Igc/igc.h"
namespace OriginalInventory {
inline float GetRange(IpartIGC* part, IshipIGC* viewer)
{
    if (!part)
        return 0;

    EquipmentType et = part->GetPartType()->GetEquipmentType();

    if (et == ET_Weapon)
    {
        IprojectileTypeIGC* ppt = ((IweaponIGC*)(IpartIGC*)part)->GetProjectileType();
        float               range = ppt->GetSpeed()*ppt->GetLifespan();

        const GlobalAttributeSet&   ga = viewer->GetSide()->GetGlobalAttributeSet();
        range *= ga.GetAttribute((((IweaponIGC*)(IpartIGC*)part)->GetAmmoPerShot())
                                 ? c_gaSpeedAmmo
                                 : c_gaLifespanEnergy);

        return range;
    }
    else if (et == ET_Magazine)
    {
        ImissileTypeIGC* pmt = (ImissileTypeIGC*)((IlauncherTypeIGC*)part->GetPartType())->GetExpendableType();
        float range = pmt->GetLifespan()*(pmt->GetInitialSpeed()+0.5f*pmt->GetLifespan()*pmt->GetAcceleration());
        return range;
    }
    else
    {
        ZAssert(false);
        return 0;
    }
}
inline float GetDamage(IpartIGC* part, IshipIGC* viewer)
{
    if (!part)
        return 0;

    EquipmentType et = part->GetPartType()->GetEquipmentType();

    const GlobalAttributeSet&   ga = viewer->GetSide()->GetGlobalAttributeSet();

    if (et == ET_Weapon)
    {
        IprojectileTypeIGC* ppt = ((IweaponIGC*)(IpartIGC*)part)->GetProjectileType();
        return (ppt->GetPower() + ppt->GetBlastPower()) * ga.GetAttribute(c_gaDamageGuns);
    }
    else if (et == ET_Magazine)
    {
        ImissileTypeIGC* pmt = (ImissileTypeIGC*)((IlauncherTypeIGC*)part->GetPartType())->GetExpendableType();
        return (pmt->GetPower() + pmt->GetBlastPower()) * ga.GetAttribute(c_gaDamageMissiles);
    }
    else
    {
        ZAssert(false);
        return 0;
    }
}
inline float GetRate(IpartIGC* part)
{
	if (!part)
        return 0;

	EquipmentType et = part->GetPartType()->GetEquipmentType();

	if (et == ET_Weapon)
    {
        DataWeaponTypeIGC*  pdwt = (DataWeaponTypeIGC*)((IpartTypeIGC*)part->GetPartType())->GetData();
        return (1.0f /pdwt->dtimeBurst);
    }

    else
    {
        ZAssert(false);
        return 0;
    }
}
inline float GetCount(IpartIGC* part)
{
    if (!part)
        return 0;
    else
    {
        ObjectType ot = part->GetObjectType();
        if (part->GetShip() && part->GetMountID() < 0)
        {
            float fCount = 0.0f;

            IpartTypeIGC *ppartType = part->GetPartType();

            // add up all of the instances of this in the cargo
            for (Mount mount = -1; mount >= -c_maxCargo; --mount)
            {
                IpartIGC* ppart = part->GetShip()->GetMountedPart(NA, mount);

                if (ppart && ppart->GetPartType() == ppartType)
                {
                    fCount += ppart->GetAmount();
                }
            }

            if (part->GetObjectType() == OT_pack)
            {
                IpackIGC*   p = (IpackIGC*)(IpartIGC*)part;
                const IhullTypeIGC *pht = part->GetShip()->GetHullType();

                if (pht && p->GetPackType() == c_packAmmo)
                {
                    fCount /= pht->GetMaxAmmo();
                }
                else
                {
                    fCount /= pht->GetMaxFuel();
                }
            }

            return fCount;
        }
        else
            return (float)(part->GetAmount());
    }
}
inline float GetAfterburnerTopSpeed(IpartIGC* part, IshipIGC* viewer)
{
    if (!part)
        return 0;
    else if (part->GetPartType()->GetEquipmentType() == ET_Afterburner)
    {
        IafterburnerIGC* pa = (IafterburnerIGC*)(IpartIGC*)part;
        const IhullTypeIGC* pht = viewer->GetSourceShip()->GetHullType();

        float maxAfterburnerThrust = pa->GetMaxThrustWithGA(); //TheRock 15-8-2009
        float thrust = pht->GetThrust();

        return pht->GetMaxSpeed() * (1.0f + (maxAfterburnerThrust / thrust));
    }
    else
    {
        ZAssert(false);
        return 0;
    }
}
inline float GetAfterburnerTimeLeft(IpartIGC* part, IshipIGC* viewer)
{
    if (!part)
        return 0;
    else if (part->GetPartType()->GetEquipmentType() == ET_Afterburner)
	{
		IafterburnerIGC* pa = (IafterburnerIGC*)(IpartIGC*)part;
		const IhullTypeIGC* pht = viewer->GetSourceShip()->GetHullType();

		float fuel = viewer->GetFuel();
		float fuelConsumption = pa->GetFuelConsumption();

		if (fuelConsumption <= 0){
			return -1.0f;
		}
		else
			return fuel / (fuelConsumption * pa->GetMaxThrust());
	}
	else
    {
        ZAssert(false);
        return 0;
    }
}
inline float GetShieldStrength(IpartIGC* part)
{
	float   f = 0.0f;
    if (!part)
        return 0;
    else if (part->GetPartType()->GetEquipmentType() == ET_Shield)
    {
        IshieldIGC* pshield = (IshieldIGC*)(IpartIGC*)part;
		f = pshield->GetFraction();
		return f * pshield->GetMaxStrength();
	}
	else
    {
        ZAssert(false);
        return 0;
    }
}
inline float GetRechargeTime(IpartIGC* part)
{
	float   f = 0.0f;
	float MaxHP = 0.0f;
	float HP = 0.0f;
    if (!part)
        return 0;
    else if (part->GetPartType()->GetEquipmentType() == ET_Shield)
    {
        IshieldIGC* pshield = (IshieldIGC*)(IpartIGC*)part;
		MaxHP = pshield->GetMaxStrength();
		f = pshield->GetFraction();
		HP = f * pshield->GetMaxStrength();
		return (MaxHP - HP) / pshield->GetRegeneration();
	}
	else
    {
        ZAssert(false);
        return 0;
    }
}
inline float GetCloakTimeLeft(IpartIGC* part, IshipIGC* viewer)
{
	if (!part)
        return 0;
    else if (part->GetPartType()->GetEquipmentType() == ET_Cloak)
	{
		const IhullTypeIGC* pht = viewer->GetSourceShip()->GetHullType();
		IcloakIGC* pcloak = (IcloakIGC*)(IpartIGC*)part;

		float shipEnergy = viewer->GetEnergy();
		float shipEnergyGen = pht->GetRechargeRate();
		float cloakConsumption = pcloak->GetEnergyConsumption();

		if (cloakConsumption <= shipEnergyGen){
			return -1.0f;
		}
		else
			return shipEnergy/(cloakConsumption - shipEnergyGen);
	}
	else
    {
        ZAssert(false);
        return 0;
    }
}
inline float IsOutOfAmmo(IpartIGC* part)
{
    if (!part)
        return 0.0f;

    ObjectType type = part->GetObjectType();

    if (type == OT_weapon)
    {
        return (part->GetShip()->GetAmmo() == 0) ? 1.0f : 0.0f;
    }
    else if (type == OT_afterburner)
    {
        return (part->GetShip()->GetFuel() == 0) ? 1.0f : 0.0f;
    }
    else if (IlauncherIGC::IsLauncher(type))
    {
        return (part->GetAmount() == 0) ? 1.0f : 0.0f;
    }
    else
    {
        return 0.0f;
    }
}
}
