#include "../ClientCore/CombatActions.h"
#include "../ClientCore/Inventory.h"
#include "../ClientCore/ShipSound.h"
#include "ship_notifications.hpp"
#include "../ClientCore/WarpTransit.h"
#include "assets.hpp"
#include "igc.h"
#include "legacy.hpp"
#include "test_reference/client_actions.hpp"
#include "test_fixture.hpp"
#include "test_reference/cargo.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace {
void require(bool ok, const char *what) {
  if (!ok)
    throw std::runtime_error(what);
}
void near(const Vector &a, const Vector &b, const char *what) {
  require((a - b).LengthSquared() < 1e-8f, what);
}
struct RandomStream {
  size_t draws{};
  std::vector<std::pair<float, float>> ranges;
  float operator()(float low, float high) {
    static constexpr float values[] = {.13f, .83f, .37f, .63f, .01f,
                                       .97f, .24f, .72f, .5f};
    ranges.emplace_back(low, high);
    return low + (high - low) * values[(draws++) % std::size(values)];
  }
};
using NativeTest::Fixture;
IpartIGC *add(IshipIGC *ship, IpartTypeIGC *type, Mount mount,
              short amount = 0x7fff) {
  auto *p = ship->CreateAndAddPart(type, mount, amount);
  require(p, "create part");
  p->SetMountedFraction(1);
  p->Arm();
  return p;
}
void clear(IshipIGC *ship) {
  while (auto *l = ship->GetParts()->first())
    l->data()->Terminate();
}
using PartState = std::tuple<int, int, int, int, float>;
std::vector<PartState> inventory(IshipIGC *ship) {
  std::vector<PartState> result;
  for (auto *l = ship->GetParts()->first(); l; l = l->next()) {
    auto *p = l->data();
    int amount = 0;
    auto type = p->GetEquipmentType();
    if (type == ET_Pack)
      amount = static_cast<IpackIGC *>(p)->GetAmount();
    else if (type == ET_Magazine || type == ET_ChaffLauncher ||
             type == ET_Dispenser)
      amount = static_cast<IlauncherIGC *>(p)->GetAmount();
    result.emplace_back(type, p->GetMountID(), p->GetPartType()->GetObjectID(),
                        amount, p->GetMountedFraction());
  }
  std::sort(result.begin(), result.end());
  return result;
}
void missileChecks(Fixture &f, IshipIGC *ship) {
  auto magazines = f.types(ET_Magazine, ship);
  size_t salvos = 0;
  bool spread = false, straight = false;
  // Training data alone need not exercise both dispersion branches. Clone one
  // authored definition for explicit zero/spread and multi-launch fixtures.
  for (int variant = 0; variant < 2; ++variant) {
    auto *launcher = static_cast<IlauncherTypeIGC *>(magazines.front());
    auto missile = *static_cast<const DataMissileTypeIGC *>(
        launcher->GetExpendableType()->GetData());
    missile.expendabletypeID = 30000 + variant;
    missile.dispersion = variant ? .2f : 0.f;
    auto *type = f.mission->CreateObject(
        f.mission->GetLastUpdate(), OT_missileType, &missile, sizeof(missile));
    require(type, "create dispersion fixture");
    type->Release();
    auto data = *static_cast<const DataLauncherTypeIGC *>(launcher->GetData());
    data.expendabletypeID = missile.expendabletypeID;
    data.partID = 30000 + variant;
    data.launchCount = 3;
    auto *part = static_cast<IpartTypeIGC *>(f.mission->CreateObject(
        f.mission->GetLastUpdate(), OT_partType, &data, sizeof(data)));
    require(part, "create salvo fixture");
    magazines.push_back(part);
    part->Release();
  }
  ship->SetPosition(Vector(271, -82, 917));
  ship->SetVelocity(Vector(45, -19, 37));
  Orientation o(Vector(.4f, -.2f, -1));
  o.Roll(.9f);
  ship->SetOrientation(o);
  for (auto *type : magazines) {
    clear(ship);
    auto *mag = static_cast<ImagazineIGC *>(add(ship, type, 0));
    spread |= mag->GetMissileType()->GetDispersion() != 0;
    straight |= mag->GetMissileType()->GetDispersion() == 0;
    for (int count : {1, std::max(1, int(mag->GetLaunchCount()) - 1),
                      int(mag->GetLaunchCount())})
      for (float dt : {0.f, .017f, .11f}) {
        std::vector<Allegiance::MissileLaunchVectors> old(count), now(count);
        RandomStream a, b;
        Time time = ship->GetLastUpdate() + dt;
        OriginalReference::Missile(ship, mag, time, count, old.data(),
                                   std::ref(a));
        Allegiance::ComputeMissileLaunches(ship, mag, time, count, now.data(),
                                           std::ref(b));
        require(a.draws == b.draws && a.ranges == b.ranges,
                "missile RNG order/ranges changed");
        for (int i = 0; i < count; ++i) {
          near(old[i].vecPosition, now[i].vecPosition,
               "missile timed emission position");
          near(old[i].vecForward, now[i].vecForward, "missile fan direction");
          near(old[i].vecVelocity, now[i].vecVelocity,
               "missile launch velocity");
        }
        ++salvos;
      }
    RandomStream empty;
    Allegiance::ComputeMissileLaunches(
        ship, mag, ship->GetLastUpdate(), 0,
        static_cast<Allegiance::MissileLaunchVectors *>(nullptr),
        std::ref(empty));
    require(!empty.draws, "empty salvo consumed RNG");
  }
  require(spread && straight, "missile fixtures lack spread/straight coverage");
  printf("Missiles: %zu timed/partial/full salvos agree with frozen original, "
         "including RNG ranges/order\n",
         salvos);
}
void warpChecks(Fixture &f, IshipIGC *ship, IshipIGC *reference) {
  size_t cases = 0;
  for (auto *l = f.mission->GetWarps()->first(); l; l = l->next()) {
    auto *warp = l->data();
    auto *dest = warp->GetDestination();
    if (!dest)
      continue;
    float speed = f.mission->GetFloatConstant(c_fcidExitWarpSpeed);
    for (float offset : {0.f, .5f, .95f}) {
      auto start =
          warp->GetPosition() + Vector(warp->GetRadius() * offset, 0, 200);
      for (auto *s : {ship, reference}) {
        s->SetPosition(start);
        s->SetVelocity(Vector(0, 0, -127));
        s->SetCurrentTurnRate(c_axisRoll, -.2f);
        s->SetCluster(dest->GetCluster());
      }
      RandomStream a, b;
      OriginalReference::Warp(reference, warp, dest, speed, std::ref(a));
      Allegiance::ApplyOfflineWarpExit(ship, warp, dest, speed, std::ref(b));
      require(a.ranges == b.ranges && a.draws == 5,
              "warp RNG order/ranges changed");
      near(ship->GetPosition(), reference->GetPosition(), "warp exit position");
      near(ship->GetVelocity(), reference->GetVelocity(), "warp exit velocity");
      near(ship->GetOrientation().GetForward(),
           reference->GetOrientation().GetForward(), "warp facing");
      near(ship->GetOrientation().GetUp(), reference->GetOrientation().GetUp(),
           "warp roll");
      require(ship->GetCurrentTurnRate(c_axisRoll) ==
                  reference->GetCurrentTurnRate(c_axisRoll),
              "warp spin changed");
      ++cases;
    }
    ship->SetPosition(warp->GetPosition());
    ship->SetVelocity(Vector::GetZero());
    RandomStream a;
    Allegiance::ApplyOfflineWarpExit(ship, warp, dest, speed, std::ref(a));
    require(std::isfinite(ship->GetPosition().LengthSquared()) &&
                std::isfinite(ship->GetVelocity().LengthSquared()),
            "stationary warp produced NaNs");
  }
  require(cases > 0, "no warp coverage");
  printf("Alephs: %zu centered/off-center exits agree; stationary entry "
         "remains finite\n",
         cases);
}
void reloadChecks(Fixture &f, IshipIGC *ship, IshipIGC *reference) {
  auto packs = f.types(ET_Pack, ship);
  IpartTypeIGC *ammo = nullptr;
  IpartTypeIGC *fuel = nullptr;
  for (auto *type : packs) {
    clear(ship);
    auto *p = static_cast<IpackIGC *>(add(ship, type, -1));
    require(type->Export(nullptr) == sizeof(DataPackTypeIGC),
            "pack binary record size");
    if (p->GetPackType() == c_packAmmo)
      ammo = type;
    else if (p->GetPackType() == c_packFuel)
      fuel = type;
  }
  DataPackTypeIGC layout{};
  require(sizeof(layout) == 400 &&
              reinterpret_cast<const char *>(&layout.packType) -
                      reinterpret_cast<const char *>(&layout) ==
                  396 &&
              reinterpret_cast<const char *>(&layout.amount) -
                      reinterpret_cast<const char *>(&layout) ==
                  398,
          "Windows core pack ABI");
  require(ammo && fuel, "need ammo and fuel packs");
  auto weapons = f.types(ET_Weapon, ship);
  auto ammoWeapon = std::find_if(weapons.begin(), weapons.end(), [](auto *t) {
    return static_cast<const DataWeaponTypeIGC *>(t->GetData())->cAmmoPerShot >
           0;
  });
  require(ammoWeapon != weapons.end(), "need ammunition weapon");
  auto booster = f.types(ET_Afterburner, ship).front();
  auto magazines = f.types(ET_Magazine, ship);
  require(magazines.size() > 1, "need alternate missile types");
  size_t cases = 0;
  auto check =
      [&](EquipmentType et, const std::function<void(IshipIGC *)> &setup,
          int expectedCount, int expectedFirstMount, int expectedTransfer) {
        for (auto *s : {ship, reference}) {
          clear(s);
          setup(s);
        }
        IlauncherIGC *a = nullptr;
        IlauncherIGC *b = nullptr;
        if (et != ET_Weapon && et != ET_Afterburner) {
          a = static_cast<IlauncherIGC *>(reference->GetMountedPart(et, 0));
          b = static_cast<IlauncherIGC *>(ship->GetMountedPart(et, 0));
        }
        Allegiance::ReloadChange old[8]{}, now[8]{};
        int na = OriginalReference::Reload(reference, a, et, old),
            nb = Allegiance::ReloadInventory(ship, b, et, now);
        require(na == nb && nb == expectedCount, "reload count");
        for (int i = 0; i < na; ++i)
          require(old[i].mount == now[i].mount &&
                      old[i].amountTransfered == now[i].amountTransfered,
                  "reload packet records diverged");
        if (nb)
          require(now[0].mount == expectedFirstMount &&
                      now[0].amountTransfered == expectedTransfer,
                  "reload selection/transfer policy");
        require(inventory(ship) == inventory(reference),
                "reload inventory/remount state diverged");
        require(ship->GetAmmo() == reference->GetAmmo() &&
                    ship->GetFuel() == reference->GetFuel() &&
                    ship->GetMass() == reference->GetMass(),
                "reload resources/mass diverged");
        require(bool(a) == bool(b), "reload launcher replacement");
        if (a && b)
          require(a->GetPartType() == b->GetPartType() &&
                      a->GetTimeLoaded() == b->GetTimeLoaded(),
                  "reload launcher type/time diverged");
        ++cases;
      };
  for (auto et : {ET_Weapon, ET_Afterburner}) {
    auto *pack = et == ET_Weapon ? ammo : fuel;
    auto setup = [&](IshipIGC *s, int deficit) {
      if (et == ET_Weapon) {
        s->SetAmmo(s->GetHullType()->GetMaxAmmo() - deficit);
        for (Mount i = 0; i < s->GetHullType()->GetMaxWeapons(); ++i)
          if (s->GetHullType()->CanMount(*ammoWeapon, i))
            add(s, *ammoWeapon, i);
      } else {
        s->SetFuel(s->GetHullType()->GetMaxFuel() - deficit);
        add(s, booster, 0);
      }
    };
    check(
        et,
        [&](IshipIGC *s) {
          setup(s, 1);
          add(s, pack, -1, 3);
          add(s, pack, -5, 3);
        },
        1, -5, 1);
    require(
        static_cast<IpackIGC *>(ship->GetMountedPart(NA, -5))->GetAmount() == 2,
        "partial pack remainder");
    auto *active = ship->GetMountedPart(et, 0);
    require(active && active->GetMountedFraction() == 0,
            "reload must dismount resource consumer");
    int chunk =
        std::max(1, (et == ET_Weapon ? ship->GetHullType()->GetMaxAmmo()
                                     : int(ship->GetHullType()->GetMaxFuel())) /
                        5);
    check(
        et,
        [&](IshipIGC *s) {
          setup(s, 3 * chunk);
          add(s, pack, -1, chunk);
          add(s, pack, -3, chunk);
          add(s, pack, -5, chunk);
        },
        3, -5, NA);
    require(!ship->GetMountedPart(NA, -1) && !ship->GetMountedPart(NA, -3) &&
                !ship->GetMountedPart(NA, -5),
            "consumed packs retained");
    check(
        et,
        [&](IshipIGC *s) {
          setup(s, 0);
          add(s, pack, -5, 20);
        },
        0, 0, 0);
    check(et, [&](IshipIGC *s) { setup(s, 10); }, 0, 0, 0);
  }
  for (auto et : {ET_Magazine, ET_ChaffLauncher, ET_Dispenser}) {
    auto types = f.types(et, ship);
    auto *type = types.front();
    clear(ship);
    auto *full = static_cast<IlauncherIGC *>(add(ship, type, 0));
    int capacity = full->GetAmount();
    require(capacity > 0, "launcher capacity");
    if (capacity > 1) {
      check(
          et,
          [&](IshipIGC *s) {
            add(s, type, 0, capacity - 1);
            add(s, type, -1, 2);
            add(s, type, -5, 2);
          },
          1, -5, 1);
      require(static_cast<IlauncherIGC *>(ship->GetMountedPart(NA, -5))
                      ->GetAmount() == 1,
              "partial launcher remainder");
      require(ship->GetMountedPart(et, 0)->GetMountedFraction() == 0,
              "launcher reload did not reset remount delay");
    }
    check(
        et,
        [&](IshipIGC *s) {
          auto *p = static_cast<IlauncherIGC *>(add(s, type, 0));
          p->SetAmount(0);
          add(s, type, -5, 1);
        },
        1, -5, 1);
    require(!ship->GetMountedPart(NA, -5), "empty reserve launcher retained");
    check(et, [&](IshipIGC *s) { add(s, type, -3); }, 1, -3, NA);
    require(ship->GetMountedPart(et, 0) && !ship->GetMountedPart(NA, -3),
            "empty slot not filled from cargo");
    if (types.size() > 1) {
      auto *other = types[1];
      check(
          et,
          [&](IshipIGC *s) {
            auto *p = static_cast<IlauncherIGC *>(add(s, type, 0));
            p->SetAmount(0);
            add(s, other, -2);
          },
          1, -2, NA);
      require(ship->GetMountedPart(et, 0)->GetPartType() == other,
              "alternate launcher not promoted");
    }
    check(
        et,
        [&](IshipIGC *s) {
          auto *p = static_cast<IlauncherIGC *>(add(s, type, 0));
          p->SetAmount(0);
        },
        0, 0, 0);
  }
  printf("Inventory: %zu original-reference cases, full/partial packs, all "
         "launcher classes, alternate types and remount delays\n",
         cases);
}
void expendableChecks(Fixture &f, IshipIGC *ship, IshipIGC *target) {
  bool targetedProbe = false;
  clear(ship);
  ship->SetPosition(Vector(100, 200, 300));
  ship->SetOrientation(Orientation(Vector(.3f, .1f, -1)));
  ship->SetCluster(f.mission->GetClusters()->first()->data());
  target->SetCluster(ship->GetCluster());
  Time fired = ship->GetLastUpdate() + .1f;
  for (auto kind : {OT_mineType, OT_probeType}) {
    auto types = f.types(ET_Dispenser, ship, kind);
    auto *type = types.front();
    // Choose a targeted probe where available, so its command target survives
    // export.
    if (kind == OT_probeType)
      for (auto *t : types)
        if (static_cast<IlauncherTypeIGC *>(t)
                ->GetExpendableType()
                ->GetCapabilities() &
            c_eabmShootOnlyTarget) {
          type = t;
          break;
        }
    if (kind == OT_probeType && !(static_cast<IlauncherTypeIGC *>(type)
                                      ->GetExpendableType()
                                      ->GetCapabilities() &
                                  c_eabmShootOnlyTarget)) {
      // This hull's authored probes have no target-only variant. Exercise that
      // original IGC branch with a test-only clone of the same definition.
      auto *launcher = static_cast<IlauncherTypeIGC *>(type);
      auto probe = *static_cast<const DataProbeTypeIGC *>(
          launcher->GetExpendableType()->GetData());
      probe.expendabletypeID = 30002;
      probe.eabmCapabilities |= c_eabmShootOnlyTarget;
      auto *p = f.mission->CreateObject(f.mission->GetLastUpdate(),
                                        OT_probeType, &probe, sizeof(probe));
      require(p, "create targeted probe fixture");
      p->Release();
      auto data =
          *static_cast<const DataLauncherTypeIGC *>(launcher->GetData());
      data.partID = 30002;
      data.expendabletypeID = probe.expendabletypeID;
      type = static_cast<IpartTypeIGC *>(f.mission->CreateObject(
          f.mission->GetLastUpdate(), OT_partType, &data, sizeof(data)));
      require(type, "create targeted probe launcher fixture");
      type->Release();
    }
    for (auto velocity : {Vector::GetZero(), Vector(10, 30, -70)}) {
      clear(ship);
      auto *p = static_cast<IdispenserIGC *>(add(ship, type, 0));
      ship->SetVelocity(velocity);
      ship->SetCommand(c_cmdCurrent, target, c_cidAttack);
      float offset =
          p->GetExpendableType()->GetRadius() + ship->GetRadius() + 5;
      auto expected =
          ship->GetPosition() + (velocity.LengthSquared() < 1
                                     ? ship->GetOrientation().GetBackward()
                                     : -velocity.Normalize()) *
                                    offset;
      RandomStream rng;
      require(
          Allegiance::CreateOfflineExpendable(ship, p, fired, std::ref(rng)),
          "deploy expendable");
      require(rng.draws == 0, "mine/probe unexpectedly consumed RNG");
      ImodelIGC *model = nullptr;
      if (kind == OT_mineType) {
        auto *mine = f.mission->GetMines()->last()->data();
        model = mine;
        DataMineExport data{};
        mine->Export(&data);
        require(data.time0 == fired + 3.f &&
                    data.sideID == ship->GetSide()->GetObjectID() &&
                    data.launcherID == ship->GetObjectID(),
                "mine arming/ownership differs");
      } else {
        auto *probe = f.mission->GetProbes()->last()->data();
        model = probe;
        DataProbeExport data{};
        probe->Export(&data);
        require(data.time0 == fired &&
                    data.sideID == ship->GetSide()->GetObjectID() &&
                    data.shipID == ship->GetObjectID(),
                "probe time/ownership differs");
        if (p->GetExpendableType()->GetCapabilities() & c_eabmShootOnlyTarget) {
          targetedProbe = true;
          require(data.oidTarget == target->GetObjectID() &&
                      data.otTarget == OT_ship,
                  "probe command target lost");
        }
      }
      near(model->GetPosition(), expected,
           "moving/stationary deployment position");
      require(model->GetCluster() == ship->GetCluster(), "deployment sector");
    }
  }
  require(targetedProbe, "need targeted probe coverage");
  clear(ship);
  auto *chaff = static_cast<IdispenserIGC *>(
      add(ship, f.types(ET_ChaffLauncher, ship).front(), 0));
  auto *missileType = static_cast<ImissileTypeIGC *>(
      static_cast<IlauncherTypeIGC *>(f.types(ET_Magazine, ship).front())
          ->GetExpendableType());
  DataMissileIGC d{};
  d.pmissiletype = missileType;
  d.pLauncher = target;
  d.pTarget = ship;
  d.pCluster = ship->GetCluster();
  d.position = ship->GetPosition() + Vector(0, 0, 500);
  d.forward = Vector(0, 0, -1);
  d.velocity = Vector(0, 0, -100);
  d.lock = 1;
  d.missileID = f.mission->GenerateNewMissileID();
  auto *m = static_cast<ImissileIGC *>(
      f.mission->CreateObject(fired, OT_missile, &d, sizeof(d)));
  require(m, "create incoming missile");
  size_t attempts = 0;
  auto success = [&](float low, float high) {
    require(low == 0 && high >= 0, "chaff probability range");
    ++attempts;
    return low;
  };
  require(Allegiance::CreateOfflineExpendable(ship, chaff, fired, success),
          "deploy chaff");
  require(attempts == 1 && m->GetTarget() &&
              m->GetTarget()->GetObjectType() == OT_chaff,
          "chaff did not redirect incoming missile");
  near(m->GetTarget()->GetPosition(), ship->GetPosition(), "chaff position");
  near(m->GetTarget()->GetVelocity(),
       ship->GetVelocity() + ship->GetOrientation().GetUp() * 25,
       "chaff velocity");
  m->Release();
  d.missileID = f.mission->GenerateNewMissileID();
  auto *resistant = static_cast<ImissileIGC *>(
      f.mission->CreateObject(fired, OT_missile, &d, sizeof(d)));
  require(resistant, "create resistant missile");
  d.pTarget = target;
  d.missileID = f.mission->GenerateNewMissileID();
  auto *unrelated = static_cast<ImissileIGC *>(
      f.mission->CreateObject(fired, OT_missile, &d, sizeof(d)));
  require(unrelated, "create unrelated missile");
  attempts = 0;
  auto failure = [&](float low, float high) {
    require(low == 0 && high > 0, "need nonzero chaff resistance");
    ++attempts;
    return high;
  };
  require(Allegiance::CreateOfflineExpendable(ship, chaff, fired, failure),
          "deploy failed decoy");
  require(attempts == 1 && resistant->GetTarget() == ship &&
              unrelated->GetTarget() == target,
          "chaff failure/target isolation");
  resistant->Release();
  unrelated->Release();
  puts("Expendables: moving/stationary placement, mine arming, probe "
       "ownership/target and chaff diversion");
}
void cargoChecks(Fixture& f,IshipIGC* ship,IshipIGC* reference) {
  auto weapons=f.types(ET_Weapon,ship);auto magazines=f.types(ET_Magazine,ship);
  IpartTypeIGC* types[]={nullptr,f.mission->GetAmmoPack(),weapons.front(),magazines.front()};
  int comparisons=0;
  for(int layout=0;layout<1024;++layout) {
    clear(ship);int code=layout;
    for(Mount mount=-1;mount>=-c_maxCargo;--mount){int type=code%4;code/=4;if(type)add(ship,types[type],mount);}
    for(Mount start=-1;start>=-c_maxCargo;--start) {
      OriginalCargo::Navigation original{ship,start};Mount selected=start;
      for(int i=0;i<6;++i){original.NextCargoPart();selected=Allegiance::NextCargoMount(ship,selected);require(selected==original.mountSelected,"original grouped cargo traversal");++comparisons;}
      for(auto type:{ET_Weapon,ET_Magazine})for(auto* excluded:{static_cast<IpartTypeIGC*>(nullptr),types[2]}) {
        original.mountSelected=start;selected=start;PartMask mask=ship->GetHullType()->GetPartMask(type,0);
        bool a=original.SelectCargoPartOfType(type,mask,excluded),b=Allegiance::SelectCargoType(ship,selected,type,mask,excluded);
        require(a==b && selected==original.mountSelected,"original compatible cargo selection");++comparisons;
      }
    }
  }
  clear(ship);clear(reference);require(weapons.size()>1,"two swap weapons");
  auto* a=add(ship,weapons[0],0);add(ship,weapons[1],-1);
  auto* b=add(reference,weapons[0],0);auto* other=add(reference,weapons[1],-1);
  Allegiance::SwapPartMounts(a,-1);
  // Frozen BaseClient::SwapPart mutation, leaving transport and reload out.
  Mount old=b->GetMountID();b->SetMountID(c_mountNA);other->SetMountID(old);b->SetMountID(-1);
  require(inventory(ship)==inventory(reference),"original swap mount/fraction/amount semantics");
  add(ship,f.mission->GetAmmoPack(),-2);auto* last=add(ship,f.mission->GetAmmoPack(),-5);
  require(Allegiance::LastCargoOfType(ship,f.mission->GetAmmoPack())==last,"drop last duplicate preserves grouped selection");
  const int masks[]={oneWeaponIGC,afterburnerButtonIGC,missileFireIGC,mineFireIGC,chaffFireIGC};
  const EquipmentType equipment[]={ET_Weapon,ET_Afterburner,ET_Magazine,ET_Dispenser,ET_ChaffLauncher};
  for(int bits=0;bits<32;++bits) {
    int state=0;std::vector<EquipmentType> expected,actual;
    for(int i=0;i<5;++i)if(bits&(1<<i))state|=masks[i];else expected.push_back(equipment[i]);
    ship->SetStateM(state);Allegiance::ManualReload(ship,[&](IlauncherIGC*,EquipmentType type){actual.push_back(type);});
    require(actual==expected,"manual reload excludes actively firing equipment in original order");
  }
  ship->SetStateM(0);printf("Cargo: %d frozen-original navigation comparisons, swaps, duplicate drops and reload gates.\n",comparisons);
}
} // namespace
int main(int argc, char **argv) {
  try {
    Fixture f(argc > 1 ? std::filesystem::path(argv[1])
                       : Native::Assets::steamArtwork());
    auto *ship = f.ship();
    auto *reference = f.ship();
    missileChecks(f, ship);
    warpChecks(f, ship, reference);
    reloadChecks(f, ship, reference);
    expendableChecks(f, ship, reference);
    cargoChecks(f,ship,reference);
    require(Allegiance::ReceivesShipNotification(ship, ship) &&
                !Allegiance::ReceivesShipNotification(ship, reference) &&
                !Allegiance::ReceivesShipNotification(ship, nullptr),
            "notification self/other routing");
    auto *observer = f.ship();
    observer->SetParentShip(ship);
    require(Allegiance::ReceivesShipNotification(observer, ship) &&
                Allegiance::ReceivesShipNotification(observer, observer) &&
                !Allegiance::ReceivesShipNotification(observer, reference),
            "notification parent routing");
    auto* sibling=f.ship();sibling->SetParentShip(ship);
    const std::vector<IshipIGC*> players{ship,reference,observer,sibling};
    for(auto* model:std::vector<ImodelIGC*>{ship,reference,observer,sibling,nullptr,f.mission->GetStations()->first()->data()}) {
        std::vector<int> expected,actual;
        for(auto* pilot:players)if(Allegiance::ReceivesShipNotification(pilot,model))expected.push_back(pilot->GetObjectID());
        Native::visitShipNotificationRecipients(model,[&](IshipIGC* pilot){actual.push_back(pilot->GetObjectID());});
        std::sort(expected.begin(),expected.end());std::sort(actual.begin(),actual.end());
        require(actual==expected,"direct notification audience matches original per-player predicate");
    }
    sibling->Terminate();
    observer->Terminate();
    return 0;
  } catch (const std::exception &e) {
    fprintf(stderr, "Client action check: %s\n", e.what());
    return 1;
  }
}
