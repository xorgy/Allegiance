#include "smoke.hpp"
#include "game.hpp"
#include "legacy.hpp"
#include "igc.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

namespace Native {
int diagnosticShipCount(Game& game){return game.core()->GetShips()->n();}
void maintainScaleObserver(Game& game){auto* ship=game.playerShip();ship->SetFraction(1);if(auto* shield=static_cast<IshieldIGC*>(ship->GetMountedPart(ET_Shield,0)))shield->SetFraction(1);}
void arrangeScaleBattle(Game& game,bool spread,bool idle,float observerDistance,bool commandObserver) {
    std::array<std::vector<IshipIGC*>,2> teams;
    for(auto* l=game.core()->GetShips()->first();l;l=l->next())if(l->data()->GetPilotType()==c_ptWingman)
        teams[l->data()->GetSide()->GetObjectID()].push_back(l->data());
    for(int side=0;side<2;++side) {
        float spacing=spread?320.f:battleSpawnSpacing,minimum=spacing*.8f;
        float radius=spacing*.7f*std::sqrt(float(teams[side].size()));
        std::mt19937 random(game.simulationRandomState()^(side?0x85ebca6bu:0xc2b2ae35u));
        std::uniform_real_distribution<float> coordinate(-radius,radius);
        std::vector<Vector> positions;positions.reserve(teams[side].size());
        for(size_t i=0;i<teams[side].size();++i) {
            Vector position;
            do {position=Vector(coordinate(random),coordinate(random),0);}
            while(position.LengthSquared()>radius*radius ||
                std::any_of(positions.begin(),positions.end(),[&](const Vector& previous){return (position-previous).LengthSquared()<minimum*minimum;}));
            positions.push_back(position);
            position.z=side?(spread?4000.f:1400.f):(spread?-4000.f:-1400.f);
            auto* bot=teams[side][i];bot->SetPosition(position);bot->SetVelocity(Vector::GetZero());
            bot->SetOrientation(Orientation(Vector(0,0,side?-1.f:1.f)));bot->SetAutopilot(!idle);
            for(Command c=0;c<c_cmdMax;++c)bot->SetCommand(c,nullptr,c_cidNone);
            if(!idle && !teams[1-side].empty())bot->SetCommand(c_cmdAccepted,teams[1-side][i%teams[1-side].size()],c_cidAttack);
        }
        for(auto hull:battleHullIds) {
            int count=0;for(auto* ship:teams[side])if(ship->GetBaseHullType()->GetObjectID()==hull)++count;
            if(count)fprintf(stderr,"Scale team %d: %d %s\n",side,count,game.core()->GetHullType(hull)->GetName());
        }
    }
    if(commandObserver) {
        auto* player=game.playerShip();auto* cluster=player->GetCluster();IstationIGC* station=nullptr;
        for(auto* l=player->GetSide()->GetStations()->first();l;l=l->next())
            if(l->data()->GetCluster()==cluster && l->data()->GetStationType()->HasCapability(c_sabmLand)){station=l->data();break;}
        if(!station)throw std::runtime_error("Command observer needs a friendly station in the battle sector");
        for(Command c=0;c<c_cmdMax;++c)player->SetCommand(c,nullptr,c_cidNone);
        player->SetAutopilot(false);player->SetVelocity(Vector::GetZero());player->SetStation(station);
        game.commandView(true);
        if(!game.viewSector(cluster->GetObjectID()))throw std::runtime_error("Cannot open the battle sector in command view");
        float distance=std::clamp(observerDistance,50.f,20000.f);
        game.zoomCommandCamera(distance>10000.f,std::abs(distance/10000.f-1.f)*.5f);
        fprintf(stderr,"Scale command observer: docked at %s, camera %.1f m from encounter center; no player ship in space\n",station->GetName(),distance);
        return;
    }
    Vector observer(0,7000,spread?-13000.f:-7000.f);
    if(observerDistance>0)observer*=observerDistance/observer.Length();
    auto* player=game.playerShip();player->SetPosition(observer);player->SetOrientation(Orientation(Vector(0,-observer.y,-observer.z)));player->SetVelocity(Vector::GetZero());
    fprintf(stderr,"Scale observer: (%.1f, %.1f, %.1f), %.1f m from encounter center\n",observer.x,observer.y,observer.z,observer.Length());
}

void prepareEliminationSmoke(Game& game) {
    int pilot=game.addLocalPilot("Evacuating pilot",0),crew=game.addLocalPilot("Evacuating crew",0);
    if(pilot<0 || crew<0)throw std::runtime_error("Elimination smoke crew provisioning failed");
    auto* child=game.core()->GetShip(crew);
    child->SetParentShip(game.core()->GetShip(pilot));child->SetTurretID(NA);
    game.launch();game.eject();game.eject();
    if(!game.state().ghost)throw std::runtime_error("Elimination smoke did not exhaust the observer's lives");
}
void loseEliminationSmokeStations(Game& game) {
    auto* core=game.core();auto* side=game.playerShip()->GetSide();
    std::vector<TRef<IstationIGC>> bases;
    for(auto* l=side->GetStations()->first();l;l=l->next())bases.emplace_back(l->data());
    for(auto& base:bases)core->GetIgcSite()->KillStationEvent(base,nullptr,1);
    game.step({},.001);
    if(game.state().sideActive || game.state().complete || !game.state().dead || core->GetMissionStage()!=STAGE_STARTED)
        throw std::runtime_error("Elimination smoke stopped the surviving teams");
}

int prepareCrewUiSmoke(Game& game) {
    int id=game.addLocalPilot("Gunship pilot",0);
    if(id<0)throw std::runtime_error("Crew UI pilot fixture failed");
    auto* parent=game.core()->GetShip(ShipID(id));IhullTypeIGC* hull=nullptr;
    for(auto* l=game.core()->GetHullTypes()->first();l;l=l->next())
        if(l->data()->GetGroupID()>=0 && l->data()->GetMaxWeapons()>l->data()->GetMaxFixedWeapons()){hull=l->data();break;}
    if(!hull)throw std::runtime_error("Crew UI turret hull fixture failed");
    while(auto* l=parent->GetParts()->first())l->data()->Terminate();
    parent->SetBaseHullType(hull);
    for(Mount mount=hull->GetMaxFixedWeapons();mount<hull->GetMaxWeapons();++mount)
        for(auto* l=game.core()->GetPartTypes()->first();l;l=l->next())
            if(l->data()->GetEquipmentType()==ET_Weapon && hull->CanMount(l->data(),mount)){
                auto* weapon=parent->CreateAndAddPart(l->data(),mount,1);weapon->SetMountedFraction(1);weapon->Arm();break;
            }
    if(!game.boardShip(id) || game.state().turret<0)throw std::runtime_error("Crew UI gunner fixture failed");
    return id;
}
void completeResultsUiSmoke(Game& game){
    auto* core=game.core();TRef<IstationIGC> base=core->GetSide(1)->GetStations()->first()->data();
    core->GetIgcSite()->KillStationEvent(base,nullptr,1);game.step({},.01);
}
}
