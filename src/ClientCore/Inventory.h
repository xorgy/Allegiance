// Inventory transfer logic extracted from BaseClient::Reload.
#pragma once
#include "../Igc/igc.h"

namespace Allegiance {
// WinTrek's grouped cargo traversal: packs/launchers and empty slots represent
// a stack; non-stackable equipment remains individually selectable.
inline Mount NextCargoMount(IshipIGC* ship,Mount selected) {
  for(int attempts=0;attempts<c_maxCargo;++attempts) {
    if(--selected < -c_maxCargo)selected=-1;
    auto* part=ship->GetMountedPart(NA,selected);auto* type=part?part->GetPartType():nullptr;
    ObjectType object=part?part->GetObjectType():NA;bool first=true;
    if(!part || object==OT_pack || object==OT_magazine || object==OT_dispenser)
      for(Mount prev=selected+1;prev<0;++prev) {
        auto* other=ship->GetMountedPart(NA,prev);
        if(type==(other?other->GetPartType():nullptr)){first=false;break;}
      }
    if(first)return selected;
  }
  return -1;
}
inline Mount CanonicalCargoMount(IshipIGC* ship,Mount selected,bool emptyOnly=false) {
  if(selected>=0 || selected < -c_maxCargo)return -1;
  if(selected < -1 && (!emptyOnly || !ship->GetMountedPart(NA,selected)))return NextCargoMount(ship,selected+1);
  return selected;
}
inline bool SelectCargoType(IshipIGC* ship,Mount& selected,EquipmentType equipment,PartMask mask,IpartTypeIGC* excluded) {
  selected=CanonicalCargoMount(ship,selected);auto* first=ship->GetMountedPart(NA,selected);
  auto matches=[&](IpartIGC* p){return p?(p->GetEquipmentType()==equipment && !(p->GetPartType()->GetPartMask()&~mask) && p->GetPartType()!=excluded):excluded!=nullptr;};
  if(matches(first))return true;
  for(int attempts=0;attempts<c_maxCargo;++attempts) {
    selected=CanonicalCargoMount(ship,NextCargoMount(ship,selected));auto* part=ship->GetMountedPart(NA,selected);
    if(part==first)return false;
    if(matches(part))return true;
  }
  return false;
}
inline IpartIGC* LastCargoOfType(IshipIGC* ship,IpartTypeIGC* type) {
  for(Mount m=-c_maxCargo;m<0;++m)if(auto* p=ship->GetMountedPart(NA,m))if(p->GetPartType()==type)return p;
  return nullptr;
}
// Caller validates compatibility and retains transport/automatic reload policy.
inline void SwapPartMounts(IpartIGC* part,Mount destination) {
  Mount old=part->GetMountID();auto* other=part->GetShip()->GetMountedPart(part->GetEquipmentType(),destination);
  if(other){part->SetMountID(c_mountNA);other->SetMountID(old);}
  part->SetMountID(destination);
}
template<class Reload>
void ManualReload(IshipIGC* ship,Reload reload) {
  int state=ship->GetStateM();
  if(!(state&(oneWeaponIGC|allWeaponsIGC)))reload(nullptr,ET_Weapon);
  if(!(state&afterburnerButtonIGC))reload(nullptr,ET_Afterburner);
  if(!(state&missileFireIGC))reload(static_cast<IlauncherIGC*>(ship->GetMountedPart(ET_Magazine,0)),ET_Magazine);
  if(!(state&mineFireIGC))reload(static_cast<IlauncherIGC*>(ship->GetMountedPart(ET_Dispenser,0)),ET_Dispenser);
  if(!(state&chaffFireIGC))reload(static_cast<IlauncherIGC*>(ship->GetMountedPart(ET_ChaffLauncher,0)),ET_ChaffLauncher);
}
struct ReloadChange {
  short amountTransfered;
  Mount mount;
};
constexpr int MaxReloadChanges = 8;
static_assert(c_maxCargo < MaxReloadChanges,
              "Reload buffer must fit every cargo transfer");

// Caller owns authority, empty-launcher termination, notifications and
// transport. In particular, SetAmmo/SetFuel can call IIgcSite::Reload; no UI is
// required. Change must have the original ReloadData fields. No allocation or
// extra pass.
template <class Change>
int ReloadInventory(IshipIGC *pship, IlauncherIGC *&plauncher,
                    EquipmentType type, Change (&reloads)[MaxReloadChanges]) {
  int nReloads = 0;
  const IhullTypeIGC *pht = pship->GetHullType();

  switch (type) {
  case ET_Weapon:
  case ET_Afterburner: {
    PackType ptDesired;
    short ammo;
    short maxAmmo;
    if (type == ET_Weapon) {
      ptDesired = c_packAmmo;
      ammo = pship->GetAmmo();
      maxAmmo = pht->GetMaxAmmo();
    } else {
      ptDesired = c_packFuel;
      ammo = short(pship->GetFuel());
      maxAmmo = short(pht->GetMaxFuel());
    }

    while (ammo < maxAmmo) {
      // Find the last ammo/fuel pack (avoids mysterious behavior in inventory
      // pane)
      IpackIGC *ppack = NULL;
      short amount = 0x7fff;
      {
        for (Mount i = -1; (i >= -c_maxCargo); i--) {
          IpartIGC *ppart = pship->GetMountedPart(NA, i);

          if (ppart && (ppart->GetObjectType() == OT_pack)) {
            IpackIGC *p = (IpackIGC *)ppart;

            if (p->GetPackType() == ptDesired) {
              ppack = p;
              amount = p->GetAmount();
            }
          }
        }
      }

      if (ppack) {
        reloads[nReloads].mount = ppack->GetMountID();
        short transfer;
        if (ammo + amount > maxAmmo) {
          // There will be stuff left in the pack
          transfer = maxAmmo - ammo;
          ammo = maxAmmo;

          ppack->SetAmount(amount - transfer);
        } else {
          // Entire pack gets trasnfered
          transfer = NA;
          ammo += amount;

          ppack->Terminate();
        }
        reloads[nReloads++].amountTransfered = transfer;
      } else
        break;
    }

    if (nReloads != 0) {
      if (type == ET_Weapon) {
        pship->SetAmmo(ammo);

        // disable all mounted weapons that use ammo
        Mount maxWeapons = pht->GetMaxWeapons();
        for (Mount i = 0; (i < maxWeapons); i++) {
          IweaponIGC *pw = (IweaponIGC *)(pship->GetMountedPart(ET_Weapon, i));
          if (pw && (pw->GetAmmoPerShot() != 0))
            pw->SetMountedFraction(0.0f);
        }
      } else {
        pship->SetFuel(float(ammo));

        // disable the afterburner
        IpartIGC *p = pship->GetMountedPart(ET_Afterburner, 0);
        if (p)
          p->SetMountedFraction(0.0f);
      }
    }
  } break;

  case ET_Magazine:
  case ET_Dispenser:
  case ET_ChaffLauncher: {
    IpartTypeIGC *pparttype;
    short ammo;
    short maxAmmo;
    if (plauncher) {
      pparttype = plauncher->GetPartType();
      ammo = plauncher->GetAmount();
      maxAmmo = ((IlauncherTypeIGC *)pparttype)->GetAmount(pship);
    } else {
      pparttype = NULL;
      ammo = 0;
      maxAmmo = 0x7fff;
    }

    while (ammo < maxAmmo) {
      IlauncherIGC *plauncherReload = NULL;
      short amount = 0x7fff;

      {
        for (Mount mount = -1; (mount >= -c_maxCargo); mount--) {
          IpartIGC *p = pship->GetMountedPart(NA, mount);
          if (p && (p->GetEquipmentType() == type)) {
            IpartTypeIGC *ppt = p->GetPartType();

            if (pht->CanMount(ppt, 0)) {
              short a = ((IlauncherIGC *)p)->GetAmount();
              assert(a > 0);

              if (pparttype == NULL) {
                // when one has nothing ... one will pick anything
                reloads[nReloads].amountTransfered = NA;
                reloads[nReloads++].mount = mount;

                if (plauncher)
                  plauncher->Terminate();

                p->SetMountID(0);

                plauncher = (IlauncherIGC *)p;
                pparttype = plauncher->GetPartType();
                ammo = plauncher->GetAmount();
                assert(ammo > 0);
                maxAmmo = ((IlauncherTypeIGC *)pparttype)->GetAmount(pship);

                assert(plauncherReload == NULL);

                if (ammo == maxAmmo)
                  break; // No point in continuing if we are full up
              } else if (ppt == pparttype) {
                plauncherReload = (IlauncherIGC *)p;
                amount = a;
              }
            }
          }
        }
      }

      if (plauncherReload) {
        reloads[nReloads].mount = plauncherReload->GetMountID();

        short transfer;
        if (ammo + amount <= maxAmmo) {
          transfer = amount;
          plauncherReload->Terminate();
        } else {
          transfer = maxAmmo - ammo;
          plauncherReload->SetAmount(amount - transfer);
        }

        ammo += transfer;

        plauncher->SetAmount(ammo);
        plauncher->SetMountedFraction(0.0f);
        plauncher->ResetTimeLoaded();

        reloads[nReloads++].amountTransfered = transfer;
      } else if (pparttype && (ammo == 0)) {
        // Couldn't find a way to reload the existing launcher
        // try again being less picky
        assert(plauncher);

        pparttype = NULL;
        maxAmmo = 0x7fff;
      } else
        break;
    }
  } break;
  }

  return nReloads;
}
// The original depletion/reload notification policy for player launchers.
template <class Site>
void NotifyLauncherReload(Site &site, IshipIGC *ship, EquipmentType type,
                          bool reloaded) {
  const bool missile = type == ET_Magazine;
  const bool chaff = type == ET_ChaffLauncher;
  site.PlayNotificationSound(reloaded ? (missile ? salReloadingMissilesSound
                                         : chaff ? salReloadingChaffSound
                                                 : salReloadingDispenserSound)
                                      : (missile ? salMissilesDepletedSound
                                         : chaff ? salChaffDepletedSound
                                                 : salDispenserEmptySound),
                             ship);
  if (reloaded && missile)
    site.PlaySoundEffect(startReloadSound, ship);
  site.PostText(false, reloaded ? (missile ? "Reloading missiles..."
                                   : chaff ? "Reloading chaff..."
                                           : "Reloading dispenser...")
                                : (missile ? "Missiles depleted."
                                   : chaff ? "Chaff depleted."
                                           : "Dispenser empty."));
  if (reloaded && !missile)
    site.PlaySoundEffect(startReloadSound, ship);
}
} // namespace Allegiance
