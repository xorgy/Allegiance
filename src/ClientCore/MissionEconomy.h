// IGC domain operations extracted from BaseClient and FedSrv; no UI or transport.
#pragma once
#include "../Igc/igc.h"

namespace Allegiance {
// BaseClient::BuyPartOnBudget: launcher prices are per round, other prices are
// per part. Amount zero leaves both inventory and budget untouched.
inline short BuyPartOnBudget(IshipIGC* ship,IpartTypeIGC* type,Mount mount,Money* budget) {
    Money price=type->GetPrice();short amount;
    if(price) {
        if(IlauncherTypeIGC::IsLauncherType(type->GetEquipmentType())) {
            amount=type->GetAmount(ship);if(price*amount>*budget)amount=*budget/price;price*=amount;
        } else amount=*budget>=price?0x7fff:0;
    } else amount=0x7fff;
    if(amount){*budget-=price;ship->CreateAndAddPart(type,mount,amount);}return amount;
}
inline void BuyPreferredParts(IshipIGC* ship,IstationIGC* station,Money* budget) {
    auto* hull=ship->GetBaseHullType();
    for(auto* l=hull->GetPreferredPartTypes()->first();l;l=l->next()) {
        auto* type=l->data();if(!station->CanBuy(type))continue;
        type=static_cast<IpartTypeIGC*>(station->GetSuccessor(type));EquipmentType et=type->GetEquipmentType();
        Mount count=et==ET_Weapon?hull->GetMaxWeapons():1;
        for(Mount m=0;m<count;++m)if(!ship->GetMountedPart(et,m) && hull->CanMount(type,m))
            if(!BuyPartOnBudget(ship,type,m,budget))break;
    }
}
// BaseClient's authored default reserve priorities. The historical client
// proposes remaining reserves without debiting its preview budget. A local
// authority must charge them; otherwise repeated refits can mint credits.
inline void BuyDefaultCargo(IshipIGC* pship,IstationIGC* pstation,Money* pbudget,bool chargeRemainder=false) {
    auto* pht=pship->GetBaseHullType();
    auto remaining=[&](IpartTypeIGC* type,Mount mount) {
        if(chargeRemainder)BuyPartOnBudget(pship,type,mount,pbudget);
        else pship->CreateAndAddPart(type,mount,0x7fff);
    };
        //Default loadout for cargo
        // First get part types to use.

        IpartTypeIGC*   pptFuel;
        if (pship->GetMountedPart(ET_Afterburner, 0) != NULL)
        {
            pptFuel = pship->GetMission()->GetFuelPack();
            assert (pptFuel);
            assert (pptFuel->GetGroupID() >= 0);

            if (!pstation->CanBuy(pptFuel))
                pptFuel = NULL;
        }
        else
            pptFuel = NULL;

        IpartTypeIGC*   pptAmmo;
        {
            bool    bCarryAmmo = false;
            for (Mount i = 0; (i < pht->GetMaxWeapons()); i++)
            {
                IweaponIGC* pweapon = (IweaponIGC*)(pship->GetMountedPart(ET_Weapon, i));
                if (pweapon && (pweapon->GetAmmoPerShot() > 0))
                {
                    bCarryAmmo = true;
                    break;
                }
            }

            if (bCarryAmmo)
            {
                pptAmmo = pship->GetMission()->GetAmmoPack();
                assert (pptAmmo);
                assert (pptAmmo->GetGroupID() >= 0);

                if (!pstation->CanBuy(pptAmmo))
                    pptAmmo = NULL;
            }
            else
                pptAmmo = NULL;
        }

        IpartTypeIGC*   pptMissile;
        if (pship->GetMountedPart(ET_Magazine, 0) != NULL)
        {
            pptMissile = pship->GetMountedPart(ET_Magazine, 0)->GetPartType();
            assert(pptMissile);
            assert(pptMissile->GetGroupID() >= 0);

            if (!pstation->CanBuy(pptMissile))
                pptMissile = NULL;
        }
        else
            pptMissile = NULL;

        IpartTypeIGC*   pptDispenser;
        if (pship->GetMountedPart(ET_Dispenser, 0) != NULL)
        {
            pptDispenser = pship->GetMountedPart(ET_Dispenser, 0)->GetPartType();
            assert(pptDispenser);
            assert(pptDispenser->GetGroupID() >= 0);

            if (!pstation->CanBuy(pptDispenser) ||
                pptDispenser->GetPartMask() == c_pbm2) //ignore int pulse probes
            {
                pptDispenser = NULL;
            }
        }
        else
            pptDispenser = NULL;

        IpartTypeIGC* nan = pship->GetMission()->GetPartType(91);
        if (nan)
            nan = (IpartTypeIGC*)(pstation->GetSuccessor(nan));

        // Check if it's a combat ship or a scout-like one
        bool combatShip = !(pht->GetDefenseType() == 1 && // light armor
            pht->GetScannerRange() >= 2200.0f);

        //Start actually filling the cargo
        Mount   cargo = -c_maxCargo;
        while (pship->GetMountedPart(NA, cargo) != NULL)
            cargo++;

        // Buy nan
        if (nan && pship->GetHullType()->CanMount(nan, 0) && pstation->CanBuy(nan)) // GT scouts can mount a nan only in the second slot
            if (nan && cargo < 0)
                BuyPartOnBudget(pship, nan, cargo++, pbudget);
        // Buy ammo
        if (pptAmmo && cargo < 0) {
            BuyPartOnBudget(pship, pptAmmo, cargo++, pbudget);
            if (pht->GetMaxWeapons() > 2 && cargo < 0)
                BuyPartOnBudget(pship, pptAmmo, cargo++, pbudget);
        }
        // Buy 2 fuel
        if (pptFuel && cargo < 0)
            BuyPartOnBudget(pship, pptFuel, cargo++, pbudget);
        if (pptFuel && cargo < 0 && combatShip)
            BuyPartOnBudget(pship, pptFuel, cargo++, pbudget);
        // Buy one dispenser item for combat ships
        if (combatShip && pptDispenser && cargo < 0)
            BuyPartOnBudget(pship, pptDispenser, cargo++, pbudget);
        // Buy items for non-combat ships
        if (!combatShip) {
            // One extra ammo for missile-less scouts
            if (!pptMissile && pptAmmo && cargo < 0)
                BuyPartOnBudget(pship, pptAmmo, cargo++, pbudget);
            // Buy prox
            IpartTypeIGC* prox = pship->GetMission()->GetPartType(59);
            if (prox && pstation->CanBuy(prox) && pship->GetHullType()->CanMount(prox, 0)) {
                prox = (IpartTypeIGC*)(pstation->GetSuccessor(prox)); //won't return NULL
                if (cargo < 0)
                    BuyPartOnBudget(pship, prox, cargo++, pbudget);
                if (cargo < -2) //GT scout
                    BuyPartOnBudget(pship, prox, cargo++, pbudget);
            }
            else {
                //Buy Technoflux Plas Gen
                IpartTypeIGC* plasGen = pship->GetMission()->GetPartType(65);
                if (plasGen && pstation->CanBuy(plasGen) && pship->GetHullType()->CanMount(plasGen, 0)) {
                    plasGen = (IpartTypeIGC*)(pstation->GetSuccessor(plasGen)); //won't return NULL
                    if (cargo < 0)
                        BuyPartOnBudget(pship, plasGen, cargo++, pbudget);
                    if (cargo < 0)
                        BuyPartOnBudget(pship, plasGen, cargo++, pbudget);
                }
            }
        }

        //Fill up any remaining slots
        bool    bBuyFuel = true;
        while (cargo < 0)
        {
            if (pptMissile)
                remaining(pptMissile, cargo++);
            else if (pptDispenser)
                remaining(pptDispenser, cargo++);
            else if (pptFuel && bBuyFuel) {
                remaining(pptFuel, cargo++);
                bBuyFuel = (pptAmmo == NULL);
            }
            else if (pptAmmo)
                remaining(pptAmmo, cargo++);
            else {
                // If nothing else is needed, take an extra counter (empty troop transport, EW bomber)
                if (pship->GetMountedPart(ET_ChaffLauncher, 0) != NULL)
                {
                    IpartTypeIGC*   pptChaff = pship->GetMountedPart(ET_ChaffLauncher, 0)->GetPartType();
                    if (pstation->CanBuy(pptChaff))
                        remaining(pptChaff, cargo++);
                }

                break;
            }
        }
}
inline void CompleteDevelopment(IsideIGC* side,IdevelopmentIGC* development) {
    side->ApplyDevelopmentTechs(development->GetEffectTechs());
    if(!development->GetTechOnly())side->ApplyGlobalAttributeSet(development->GetGlobalAttributeSet());
}
inline void UpgradeStations(IsideIGC* side) {
    for(auto* l=side->GetStations()->first();l;l=l->next()) {
        auto* station=l->data();auto* old=station->GetBaseStationType();auto* next=old->GetSuccessorStationType(side);
        if(old!=next){station->SetName(next->GetName());station->SetBaseStationType(next);}
    }
}
// FedSrv::UpgradeShip. Drones pay for their equipment in their purchase price.
inline void UpgradeDrone(IshipIGC* ship,const IstationIGC* station) {
    auto* old=ship->GetBaseHullType();auto* next=static_cast<IhullTypeIGC*>(station->GetSuccessor(old));
    if(old!=next) {
        while(auto* l=ship->GetParts()->first())l->data()->Terminate();ship->SetBaseHullType(next);
        for(auto* l=next->GetPreferredPartTypes()->first();l;l=l->next()) {
            auto* type=l->data();if(!station->CanBuy(type))continue;
            type=static_cast<IpartTypeIGC*>(station->GetSuccessor(type));auto et=type->GetEquipmentType();
            Mount count=et==ET_Weapon?next->GetMaxWeapons():1;
            for(Mount m=0;m<count;++m)if(!ship->GetMountedPart(et,m) && next->CanMount(type,m))ship->CreateAndAddPart(type,m,SHRT_MAX);
        }
    } else {
        auto* l=ship->GetParts()->last();while(l) {
            auto* part=l->data();l=l->txen();Mount mount=part->GetMountID();
            if(mount<0)part->Terminate();
            else {
                auto* type=part->GetPartType();auto* upgraded=static_cast<IpartTypeIGC*>(station->GetSuccessor(type));
                if(upgraded!=type){part->Terminate();ship->CreateAndAddPart(upgraded,mount,SHRT_MAX);}
            }
        }
    }
}
// WinTrek's authored offline construction preserves the asteroid orientation,
// hull fraction and requested type. Multiplayer authority has different rules.
inline IstationIGC* BuildOfflineStation(IasteroidIGC* asteroid,IsideIGC* side,IstationTypeIGC* type,Time now) {
    auto* mission=asteroid->GetMission();DataStationIGC data{};
    strcpy(data.name,type->GetName());data.clusterID=asteroid->GetCluster()->GetObjectID();
    data.position=asteroid->GetPosition();data.forward=asteroid->GetOrientation().GetForward();data.up=asteroid->GetOrientation().GetUp();
    data.rotation.axis(data.forward);data.rotation.angle(0);data.sideID=side->GetObjectID();
    data.stationID=mission->GenerateNewStationID();data.stationTypeID=type->GetObjectID();
    data.bpHull=asteroid->GetFraction();data.bpShield=0;
    asteroid->Terminate();
    return static_cast<IstationIGC*>(mission->CreateObject(now,OT_station,&data,sizeof(data)));
}
// FedSrv's authoritative station placement, distinct from WinTrek's training
// orientation. Returns a caller-owned reference; the adapter preserves discovery
// history and removes the asteroid after creating the station.
inline IstationIGC* ConstructStation(IasteroidIGC* asteroid,IsideIGC* side,IstationTypeIGC* type,Time now) {
    type=type->GetSuccessorStationType(side);auto* mission=side->GetMission();auto* cluster=asteroid->GetCluster();
    DataStationIGC data{};strcpy(data.name,type->GetName());data.clusterID=cluster->GetObjectID();data.position=asteroid->GetPosition();
    data.forward=Vector(0,0,1);data.up=Vector(0,1,0);data.rotation.axis(data.forward);data.rotation.angle(0);
    data.sideID=side->GetObjectID();data.stationID=mission->GenerateNewStationID();data.stationTypeID=type->GetObjectID();
    data.bpHull=asteroid->GetFraction();data.bpShield=0;
    for(auto* l=cluster->GetProbes()->first();l;l=l->next()) {
        auto* p=l->data();float d=(p->GetPosition()-asteroid->GetPosition()).LengthSquared(),r=type->GetRadius()+p->GetRadius();
        if(d>0 && d<r*r)p->SetExpiration(now+1.f);
    }
    auto* station=static_cast<IstationIGC*>(mission->CreateObject(now,OT_station,&data,sizeof(data)));
    if(station)station->SetRoidID(asteroid->GetObjectID());return station;
}
}
