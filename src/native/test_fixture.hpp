// Real IGC integration fixture; installed core and world, no renderer or transport.
#pragma once
#include "assets.hpp"
#include "igc.h"
#include "legacy.hpp"
#include <cstdio>
#include <stdexcept>
#include <vector>
namespace NativeTest {
inline void require(bool ok, const char* what) {if(!ok)throw std::runtime_error(what);}
struct Fixture : IIgcSite {
  ImissionIGC *mission{};
  std::vector<TRef<IshipIGC>> ships;
  explicit Fixture(const std::filesystem::path &art, bool loadMap = true) {
    require(std::filesystem::is_directory(art), "installed Artwork directory is missing; pass its path to the check");
    UTL::SetArtPath(art.string().c_str());
    mission = CreateMission();
    mission->Initialize(Time(1000u), this);
    require(LoadIGCStaticCore("PCore014", mission, false) != 0, "load core");
    MissionParams params;
    params.nTeams = 6;
    params.bAllowEmptyTeams = true;
    mission->SetMissionParams(&params);
    constexpr CivID civs[] = {25, 35, 18, 25, 35, 18};
    for (SideID sid = 0; sid < 6; ++sid) {
      DataSideIGC data{};
      data.sideID = sid;
      data.civilizationID = civs[sid];
      data.squadID = NA;
      data.gasAttributes.Initialize();
      data.color.SetRGBA(.2f, .5f, 1);
      snprintf(data.name, sizeof(data.name), "Side %d", sid);
      data.ttbmDevelopmentTechs =
          mission->GetCivilization(civs[sid])->GetBaseTechs();
      data.ttbmInitialTechs = data.ttbmDevelopmentTechs;
      auto *side = static_cast<IsideIGC *>(mission->CreateObject(
          mission->GetLastUpdate(), OT_side, &data, sizeof(data)));
      require(side, "create side");
      side->Release();
    }
    if(loadMap)require(LoadIGCFile("training_6", mission), "load warp map");
  }
  ~Fixture() override {
    ships.clear();
    mission->Terminate();
    delete mission;
  }
  IshipIGC *ship(SideID side = 0, HullID hull = 414, PilotType pilot = c_ptPlayer) {
    DataShipIGC data{};
    data.shipID = mission->GenerateNewShipID();
    data.hullID = hull;
    data.sideID = side;
    data.pilotType = pilot;
    data.baseObjectID = NA;
    data.partsOffset = sizeof(data);
    snprintf(data.name, sizeof(data.name), "Reference %d", data.shipID);
    auto *s = static_cast<IshipIGC *>(mission->CreateObject(
        mission->GetLastUpdate(), OT_ship, &data, sizeof(data)));
    require(s, "create ship");
    ships.emplace_back(s);
    s->Release();
    s->SetCluster(mission->GetClusters()->first()->data());
    return s;
  }
  std::vector<IpartTypeIGC *> types(EquipmentType type, IshipIGC *ship,
                                    ObjectType expendable = OT_invalid) {
    std::vector<IpartTypeIGC *> result;
    for (auto *l = mission->GetPartTypes()->first(); l; l = l->next()) {
      auto *p = l->data();
      if (p->GetEquipmentType() != type ||
          (type != ET_Pack && !ship->GetHullType()->CanMount(p, 0)))
        continue;
      if (expendable != OT_invalid && static_cast<IlauncherTypeIGC *>(p)
                                              ->GetExpendableType()
                                              ->GetObjectType() != expendable)
        continue;
      result.push_back(p);
    }
    require(!result.empty(), "no compatible part types");
    return result;
  }
};
}
