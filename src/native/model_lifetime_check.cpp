#include "game.hpp"
#include "network_records.hpp"
#include "test_fixture.hpp"
#include "../Igc/projectileIGC.h"
#include "../Igc/missileigc.h"
#include <algorithm>
#include <cstring>
#include <memory>
#include <unordered_map>
using namespace Native;
using NativeTest::require;
// Internal cluster-target overload used by the public model-target pathfinder.
IwarpIGC *FindPath(IshipIGC *, IclusterIGC *, bool);
namespace {
struct World {
    Assets assets;
    std::vector<std::shared_ptr<ModelData>> owners;
    std::unordered_map<std::string, ModelHandle> models;
    ModelLoader loader;
    SessionOptions options;
    std::unique_ptr<Game> authority, replica;
    uint64_t tick{};
    explicit World(const std::filesystem::path &art): assets(art) {
        loader = [this](const std::string &name, const std::string &) {
            if (auto i = models.find(name); i != models.end()) return i->second;
            auto m = assets.model(name);
            owners.push_back(m);
            return models.emplace(name, ModelHandle{1, m->radius, m->frames, m->lights}).first->second;
        };
        options.scenario = Scenario::Conquest;
        options.startInHangar = true;
        options.enemies = 0;
        options.rules.miners = 0;
        options.rules.showMap = options.rules.showHome = true;
        options.seed = 47;
        authority = std::make_unique<Game>(art, loader, 0, 0, options, EffectLoader{}, GameRole::Authority);
        replica = std::make_unique<Game>(art, loader, 0, 0, options, EffectLoader{}, GameRole::Replica);
    }
    ImissionIGC *core() { return authority->core(); }
    Net::Frame frame() { return authority->networkFrame(authority->playerShip()->GetObjectID(), ++tick, 0); }
    void publish() { replica->applyNetworkFrame(frame()); }
    TRef<IbuoyIGC> buoy() {
        DataBuoyIGC d{};
        d.buoyID = core()->GenerateNewBuoyID();
        d.clusterID = authority->playerShip()->GetStation()->GetCluster()->GetObjectID();
        d.type = c_buoyWaypoint; d.visible = true; d.position = Vector(4200, 3100, 1700);
        auto *created = static_cast<IbuoyIGC *>(core()->CreateObject(core()->GetLastUpdate(), OT_buoy, &d, sizeof(d)));
        require(created, "waypoint fixture");
        TRef<IbuoyIGC> result = created; created->Release(); return result;
    }
};
// Retain payload/entity alignment while exercising visibility retirement and
// the swap-delete order used by real IGC ThingSites.
template<class Keep>
Net::Frame filter(Net::Frame frame, Keep keep, bool effectsFirst = false) {
    auto entities = std::move(frame.entities); auto payload = std::move(frame.payload);
    std::vector<std::pair<Net::Entity, size_t>> retained;
    size_t offset = 0;
    for (auto &e : entities) {
        if (keep(e)) retained.emplace_back(e, offset);
        offset += e.payloadBytes;
    }
    if (effectsFirst)
        std::stable_partition(retained.begin(), retained.end(), [](auto &e) {
            return e.first.identity.type == OT_side || e.first.identity.type == OT_cluster || e.first.identity.type == OT_buildingEffect;
        });
    frame.entities.clear(); frame.payload.clear();
    for (auto &[e, start] : retained) {
        frame.entities.push_back(e);
        frame.payload.insert(frame.payload.end(), payload.begin() + start, payload.begin() + start + e.payloadBytes);
    }
    return frame;
}
IbuildingEffectIGC *shell(ImissionIGC *mission, int sector) {
    for (auto *l = mission->GetCluster(sector)->GetModels()->first(); l; l = l->next())
        if (l->data()->GetObjectType() == OT_buildingEffect) return static_cast<IbuildingEffectIGC *>(l->data());
    return nullptr;
}
void consumers(const std::filesystem::path &art) {
    World w(art);
    int pilot = w.authority->addLocalPilot("Leaving pilot", 0);
    require(pilot >= 0, "second pilot fixture");
    auto marker = w.buoy();
    auto *ship = w.core()->GetShip(pilot);
    for (Command c = 0; c < c_cmdMax; ++c) ship->SetCommand(c, marker, c_cidGoto);
    w.publish();
    TRef<IbuoyIGC> copy = w.replica->core()->GetBuoy(marker->GetObjectID());
    require(copy, "published waypoint");
    w.authority->removeLocalPilot(pilot);
    require(!marker->GetMission(), "terminating a ship releases every buoy consumer");
    w.publish();
    require(!copy->GetMission(), "replica retires the waypoint and departing ship without an orphan consumer");
    require(!w.replica->core()->GetBuoy(copy->GetObjectID()), "retired waypoint leaves the mission index");
    auto explicitMarker = w.buoy();
    for (Command c = 0; c < c_cmdMax; ++c)
        w.authority->playerShip()->SetCommand(c, explicitMarker, c_cidGoto);
    explicitMarker->Terminate();
    require(!explicitMarker->GetMission(), "explicit buoy termination tolerates synchronous consumer cleanup");
    for (Command c = 0; c < c_cmdMax; ++c)
        require(w.authority->playerShip()->GetCommandTarget(c) != explicitMarker, "explicit waypoint removal clears all consumers");
    auto fleetMarker = w.buoy();
    for (int i = 0; i < 65536; ++i) fleetMarker->AddConsumer();
    for (int i = 0; i < 65535; ++i) fleetMarker->ReleaseConsumer();
    require(fleetMarker->GetMission(), "fleet-scale waypoint ownership cannot overflow a signed short");
    fleetMarker->ReleaseConsumer();
    require(!fleetMarker->GetMission(), "fleet-scale waypoint retires at its actual final consumer");
}
void targets(const std::filesystem::path &art) {
    World w(art);
    w.authority->launch();
    auto *pilot = w.authority->playerShip();
    auto *rock = pilot->GetCluster()->GetAsteroids()->first()->data();
    pilot->SetCommand(c_cmdCurrent, rock, c_cidGoto);
    pilot->SetAutopilot(true);
    w.publish();
    TRef<IasteroidIGC> old = w.replica->core()->GetAsteroid(rock->GetObjectID());
    require(old && w.replica->playerShip()->GetAutopilot(), "autopilot target fixture");
    // Keep a reversible presentation reference across retirement too.
    auto frame = w.authority->networkFrame(pilot->GetObjectID(), w.tick, 0);
    w.replica->presentNetwork(frame.entities);
    rock->Terminate();
    w.publish();
    w.replica->restoreNetworkPresentation();
    require(!old->GetMission(), "missing asteroid is detached");
    for (Command c = 0; c < c_cmdMax; ++c)
        if (auto *target = w.replica->playerShip()->GetCommandTarget(c))
            require(target != old && target->GetMission() == w.replica->core(), "no command retains a detached autopilot target");
    FlightInput input{}; input.active = false;
    w.replica->predict(input, 1. / Net::tickRate);
    w.replica->camera({}); w.replica->scene(false);
}
void effects(const std::filesystem::path &art) {
    World w(art); w.authority->launch();
    auto *cluster = w.authority->playerShip()->GetCluster();
    auto *rock = cluster->GetAsteroids()->first()->data();
    DataBuildingEffectExport d{};
    d.timeStart = w.core()->GetLastUpdate(); d.clusterID = cluster->GetObjectID(); d.asteroidID = rock->GetObjectID();
    d.positionStart = rock->GetPosition() + Vector(100, 0, 0); d.positionStop = rock->GetPosition();
    d.radiusAsteroid = rock->GetRadius(); d.radiusStation = 100; d.color = Color(.2f, .5f, 1);
    auto *created = w.core()->CreateObject(w.core()->GetLastUpdate(), OT_buildingEffect, &d, sizeof(d));
    require(created, "construction shell fixture"); created->Release();
    // Effects can occupy an earlier ThingSite slot than their dependencies.
    w.replica->applyNetworkFrame(filter(w.frame(), [](auto &) { return true; }, true));
    TRef<IbuildingEffectIGC> old = shell(w.replica->core(), d.clusterID);
    require(old && old->GetAsteroid(), "shell resolves its later-created asteroid");
    TRef<IasteroidIGC> oldRock = old->GetAsteroid();
    oldRock->Terminate(); // Original IGC cascades into shell termination.
    require(!old->GetMission(), "original rock removal cancels its shell");
    w.publish();
    auto *current = shell(w.replica->core(), d.clusterID);
    require(current && current != old && current->GetAsteroid() && current->GetAsteroid() != oldRock,
            "identity map recreates live models after a local cascading termination");
    // A delayed publication can still contain a shell beyond its local expiry.
    auto expired = w.frame(); expired.header.tick = 41 * Net::tickRate;
    Net::Entity effect{};
    for (auto &e : expired.entities) if (e.identity.type == OT_buildingEffect) effect = e;
    Net::Event event{}; event.number = 1; event.time = 41; event.kind = Net::EventKind::HullHit; event.model = effect.identity;
    expired.events.push_back(event);
    w.replica->applyNetworkFrame(expired);
    require(!shell(w.replica->core(), d.clusterID), "self-expiring shell is not accessed through a released ThingSite");
    w.replica->applyNetworkFrame(expired);
    w.replica->presentNetwork(expired.entities); w.replica->restoreNetworkPresentation();
}
void destinations(const std::filesystem::path &art) {
    World w(art); w.authority->launch(); w.publish();
    IwarpIGC *a = nullptr, *b = nullptr;
    for (auto *l = w.replica->playerShip()->GetCluster()->GetWarps()->first(); l; l = l->next())
        if (auto *destination = l->data()->GetDestination()) { a = l->data(); b = destination; break; }
    require(a && b, "visible paired aleph fixture");
    TRef<IwarpIGC> old = b;
    auto hidden = filter(w.frame(), [&](auto &e) { return e.identity.type != OT_warp || e.identity.id != b->GetObjectID(); });
    w.replica->applyNetworkFrame(hidden);
    require(!old->GetMission() && !a->GetDestination(), "cached destination forgets a retired endpoint");
    auto *remote = w.replica->core()->GetClusters()->last()->data();
    if (auto *route = FindPath(w.replica->playerShip(), remote, false))
        require(route->GetDestination() && route->GetDestination()->GetMission(), "pathfinding never follows a detached endpoint");
    GotoPlan plan(w.replica->playerShip(), .5f); plan.Set(Waypoint::c_oEnter, a);
    w.replica->playerShip()->SetCluster(remote);
    plan.Execute(w.replica->core()->GetLastUpdate(), 1.f / Net::tickRate, false);
    w.publish();
    require(a->GetDestination() && a->GetDestination() != old && a->GetDestination()->GetMission() == w.replica->core(),
            "reveal resolves the new endpoint lifetime with the same object ID");
}
void actors(const std::filesystem::path &art) {
    World w(art);
    int a = w.authority->playerShip()->GetObjectID();
    int b = w.authority->addLocalPilot("Other pilot", 0); require(b >= 0, "actor fixture");
    auto *rock = w.authority->playerShip()->GetStation()->GetCluster()->GetAsteroids()->first()->data();
    TRef<IasteroidIGC> retained = rock;
    Net::Action target{}; target.op = Net::Operation::Target; target.args = {OT_asteroid, rock->GetObjectID()};
    require(w.authority->authorityAction(a, target) && w.authority->authorityAction(b, target), "both actors select the rock");
    w.authority->playerShip()->SetRipcordModel(rock);
    rock->Terminate();
    // The original collision tree releases removed HitTests at its next
    // update. That deliberate lease is distinct from a stale actor target.
    w.authority->authorityStep({}, 1. / Net::tickRate);
    if(retained->GetCount()!=1)fprintf(stderr,"Detached actor target references: %u\n",unsigned(retained->GetCount()));
    require(retained->GetCount() == 1, "current and inactive actor selections release the detached target");
    for (int id : {a, b}) w.authority->networkFrame(id, ++w.tick, 0);
    require(!w.authority->playerShip()->GetRipcordModel(), "ripcord relationship clears before its target detaches");
}
void docking(const std::filesystem::path &art) {
    World w(art); w.publish();
    DataStationIGC d{}; w.authority->playerShip()->GetStation()->Export(&d);
    d.stationID = 30000; d.position += Vector(3000, 3000, 0);
    auto *created = static_cast<IstationIGC *>(w.core()->CreateObject(w.core()->GetLastUpdate(), OT_station, &d, sizeof(d)));
    require(created, "new docking destination fixture");
    TRef<IstationIGC> station = created; created->Release();
    w.authority->playerShip()->SetStation(station);
    w.publish();
    TRef<IstationIGC> copy = w.replica->core()->GetStation(d.stationID);
    require(copy && w.replica->playerShip()->GetStation() == copy, "new docking destination resolves after all entities exist");
    if (auto *upgrade = station->GetBaseStationType()->GetDirectSuccessorStationType()) {
        station->SetBaseStationType(upgrade); w.publish();
        require(w.replica->core()->GetStation(d.stationID) == copy && copy->GetBaseStationType()->GetObjectID() == upgrade->GetObjectID(),
                "station site reload preserves the model identity and observation owner");
    }
    station->Terminate(); w.publish();
    require(!copy->GetMission() && !w.replica->playerShip()->GetStation(), "station retirement releases docked relationships");
    w.replica->camera({}); w.replica->scene(false);
}
void shots(const std::filesystem::path &art) {
    World w(art);
    auto *cluster = w.authority->playerShip()->GetStation()->GetCluster();
    auto create = [&] {
        DataProjectileIGC d{}; d.projectileTypeID = w.core()->GetProjectileTypes()->first()->data()->GetObjectID();
        d.forward = Vector(0, 0, -1); d.velocity = Vector(0, 0, -100); d.lifespan = 10;
        auto *p = static_cast<IprojectileIGC *>(w.core()->CreateObject(w.core()->GetLastUpdate(), OT_projectile, &d, sizeof(d)));
        require(p, "projectile replacement fixture"); TRef<IprojectileIGC> result = p;
        p->SetLauncher(w.authority->playerShip()); p->SetCluster(cluster); p->Release(); return result;
    };
    auto old = create(), replacement = create();
    auto *site = w.core()->GetIgcSite(); constexpr uint64_t id = 0x100000001;
    site->NativeProjectileCreated(old, id); site->NativeProjectileCreated(replacement, id);
    old->Terminate();
    require(site->NativeShotExists(id), "old projectile retirement cannot erase a replacement with the same shot key");
    replacement->Terminate(); require(!site->NativeShotExists(id), "current projectile retirement clears the shot key");
    auto flying=create();auto* launcher=w.authority->playerShip();
    auto* excluded=flying->GetHitTest()->GetNoHit();
    auto* hull=w.core()->GetHullType(launcher->GetBaseHullType()->GetObjectID()==414?412:414);
    require(hull,"replacement firing hull fixture");
    launcher->SetBaseHullType(hull);
    require(excluded!=launcher->GetHitTest() && excluded->GetData()==launcher,
            "a flying projectile retains its excluded collision hull across a launcher hull replacement");
    flying->GetHitTest()->SetNoHit(launcher->GetHitTest());
    require(flying->GetHitTest()->GetNoHit()==launcher->GetHitTest(),"collision exclusion can safely change hulls");
    flying->Terminate();
    // Deferred damage callbacks may queue a model which another original IGC
    // path subsequently retires before the death queue is drained.
    IprobeTypeIGC *probeType = nullptr;
    for(auto *l = w.core()->GetExpendableTypes()->first(); l; l = l->next())
        if(l->data()->GetObjectType() == OT_probeType) { probeType = static_cast<IprobeTypeIGC *>(l->data()); break; }
    require(probeType, "deferred probe death fixture");
    DataProbeIGC d{}; d.probeID = w.core()->GenerateNewProbeID(); d.time0 = w.core()->GetLastUpdate();
    d.pprobetype = probeType; d.pside = w.authority->playerShip()->GetSide(); d.pcluster = cluster; d.p0 = Vector(7000, 7000, 7000);
    auto *p = static_cast<IprobeIGC *>(w.core()->CreateObject(d.time0, OT_probe, &d, sizeof(d)));
    require(p, "probe death fixture creation"); TRef<IprobeIGC> probe = p; p->Release();
    site->KillProbeEvent(probe); probe->Terminate(); w.authority->authorityStep({}, 1. / Net::tickRate);
    require(!probe->GetMission(), "death queue does not terminate an already detached model again");
}
void assets(const std::filesystem::path &art) {
    World w(art);
    std::string name = w.authority->playerShip()->GetBaseHullType()->GetModelName();
    auto original = w.assets.model(name), alias = w.assets.model(name + "#object");
    require(!original->frames.empty(), "authored ship attachment frames");
    auto frames = std::span(original->frames);
    auto first = frames.front().position;
    auto count = frames.size();
    // A second namespace/model view can enrich or replace its attachment
    // metadata. The first published handle's frame pointers must stay valid.
    std::vector<ModelFrame> changed(65536); alias->frames.swap(changed);
    changed.clear(); changed.shrink_to_fit();
    require(frames.size() == count && frames.front().position == first && original->frames.size() == count,
            "aliased geometry metadata cannot invalidate previously published model frames");
    require(original->meshes == alias->meshes, "alias isolation still shares every geometry allocation");
}
void teardown(const std::filesystem::path &art) {
    for (int round = 0; round < 8; ++round) {
        World w(art);
        auto marker = w.buoy();
        TRef<IshipIGC> pilot = w.authority->playerShip();
        for (Command c = 0; c < c_cmdMax; ++c) pilot->SetCommand(c, marker, c_cidGoto);
        w.publish();
        TRef<IshipIGC> copy = w.replica->playerShip();
        TRef<IbuoyIGC> replicaMarker = w.replica->core()->GetBuoy(marker->GetObjectID());
        w.replica->presentNetwork(w.frame().entities);
        w.replica.reset(); w.authority.reset();
        require(!pilot->GetMission() && !copy->GetMission() && !marker->GetMission() && !replicaMarker->GetMission(),
                "teardown detaches retained models before releasing their mission and presentation owners");
        for (Command c = 0; c < c_cmdMax; ++c)
            require(!pilot->GetCommandTarget(c) && !copy->GetCommandTarget(c), "teardown clears all retained ship commands");
    }
}
}
int main(int argc, char **argv) {
    try {
        auto art = Assets::steamArtwork(); std::string only;
        for (int i = 1; i < argc; ++i)
            if (std::string_view(argv[i]) == "--case" && i + 1 < argc) only = argv[++i];
            else art = argv[i];
        if (only.empty() || only == "consumers") consumers(art);
        if (only.empty() || only == "targets") targets(art);
        if (only.empty() || only == "effects") effects(art);
        if (only.empty() || only == "destinations") destinations(art);
        if (only.empty() || only == "actors") actors(art);
        if (only.empty() || only == "docking") docking(art);
        if (only.empty() || only == "shots") shots(art);
        if (only.empty() || only == "assets") assets(art);
        if (only.empty() || only == "teardown") teardown(art);
        printf("Model lifetimes: consumers, autopilot/presentation, cascades/expiry, endpoint reuse, actors/ripcord, docking/upgrades, shots/deferred deaths, asset aliases and teardown passed.\n");
        return 0;
    } catch (const std::exception &e) { fprintf(stderr, "Model lifetime check: %s\n", e.what()); return 1; }
}
