#include "game.hpp"
#include "../Igc/igc.h"
#include <cstdio>
int main(int argc,char** argv) {
    try {
        Native::Assets assets(argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork());size_t loaded=0;
        Native::Game game(assets.root(),[&](auto& name,auto& texture) {
            fprintf(stderr,"Model %s, skin %s\n",name.c_str(),texture.c_str());
            auto model=assets.model(name);++loaded;return Native::ModelHandle{uint32_t(loaded),model->radius};
        },0,0);
        const auto& loadedState=game.state();
        if(loadedState.equipment.empty() || loadedState.sectors.size()!=1 || !loadedState.targetHasModel || loadedState.targetType.empty())
            throw std::runtime_error("HUD snapshot lacks equipment, sector, or target data");
        for(auto& contact:game.contacts())if(!contact.icon.empty())assets.image(std::string(contact.icon));
        bool stationSelected=false;
        for(auto& c:game.contacts())if(c.type==Native::ContactStation && !c.enemy) {
            int type=c.type,id=c.id;game.selectTarget(type,id);stationSelected=true;break;
        }
        if(!stationSelected)throw std::runtime_error("Practice station missing");
        game.toggleAutopilot();
        for(int i=0;i<6600 && !game.state().docked && !game.state().dead;++i)game.step({},1.0/165);
        if(!game.state().docked)throw std::runtime_error("Autopilot failed to dock at selected friendly station");
        game.launch();game.step({},1.0/165);
        if(game.state().docked || game.state().dead || game.state().hull<0.99f)throw std::runtime_error("Dock repair/launch failed");
        auto fireRounds=[&](int mount) {
            game.respawn();game.selectWeapon(0);if(mount!=0)game.selectWeapon(mount);Native::FlightInput guns;guns.fire=true;
            int selected=0;for(auto& e:game.state().equipment)selected+=e.selected;
            if(selected!=(mount<0?game.state().fixedWeapons:1))throw std::runtime_error("Inventory selection disagrees with weapon command");
            int ammo=game.state().ammo;for(int i=0;i<100;++i)game.step(guns,1.0/165);return ammo-game.state().ammo;
        };
        int single=fireRounds(0),group=fireRounds(-1);
        if(single<=0 || group<=single)throw std::runtime_error("Selected/group firing does not change actual gun fire");
        game.respawn();game.step({},1.0/165);game.toggleAutopilot();
        if(!game.state().autopilot)throw std::runtime_error("Autopilot command failed");
        for(int i=0;i<100;++i)game.step({},1.0/165);
        if(!game.state().autopilot || game.state().speed<1)throw std::runtime_error("Autopilot is not flying the ship");
        Native::FlightInput manual;manual.yaw=0.3f;game.step(manual,1.0/165);
        if(game.state().autopilot)throw std::runtime_error("Manual steering did not cancel autopilot");
        game.respawn();
        auto amount=[&](Native::PartKind kind){for(auto& e:game.state().equipment)if(e.kind==kind)return e.amount;return -1;};
        int chaff=amount(Native::PartKind::Chaff),mines=amount(Native::PartKind::Dispenser);
        Native::FlightInput expendables;expendables.chaff=expendables.deploy=true;
        for(int i=0;i<80;++i)game.step(expendables,1.0/165);
        if(chaff<=0 || mines<=0 || amount(Native::PartKind::Chaff)>=chaff || amount(Native::PartKind::Dispenser)>=mines)
            throw std::runtime_error("Equipment commands did not deploy chaff/mines");
        bool mine=false;for(auto& c:game.contacts())mine|=c.type==Native::ContactMine;
        if(!mine)throw std::runtime_error("Deployed mine absent from the world");
        game.respawn();
        for(auto& c:game.contacts())if(c.type==Native::ContactStation) {
            int id=c.id,type=c.type;std::string name(c.name);game.selectTarget(type,id);
            if(game.state().target!=name || !game.state().targetHasShield || !game.state().targetHasModel)
                throw std::runtime_error("Station target selection/preview state failed");
            break;
        }
        game.nextTarget();game.respawn();
        auto initial=game.state();Native::FlightInput turn;turn.pitch=-0.4f;turn.yaw=-0.4f;
        for(int i=0;i<80;++i)game.step(turn,1.0/165);
        if(glm::dot(game.state().forward,initial.up)<=0 || glm::dot(game.state().forward,initial.right)<=0)
            throw std::runtime_error("Pitch-up / yaw-right controls reversed");
        game.respawn();
        int missiles=game.state().missiles;Native::FlightInput launch;launch.missile=true;
        for(int i=0;i<1650 && !game.state().dead;++i)game.step(launch,1.0/165);
        if(missiles<=0 || game.state().missiles>=missiles)throw std::runtime_error("Missile launch did not consume ammunition");
        game.respawn();int deaths=game.state().deaths,identity=game.state().shipId;bool sawPod=false,fired=false;
        Native::FlightInput input;input.throttle=0.0f;input.fire=true;
        for(int i=0;i<24000;++i){game.step(input,1.0/165);sawPod|=game.state().pod;fired|=game.state().ammo<720;}

        const auto& s=game.state();printf("World ran: %zu model loads, %zu contacts, ship %s, speed %.2f, hull %.2f, deaths %d, ammo %d\n",loaded,game.contacts().size(),s.ship.c_str(),s.speed,s.hull,s.deaths,s.ammo);
        if(s.dead || !s.docked || !sawPod || !fired || s.deaths!=deaths+1 || s.shipId!=identity)
            throw std::runtime_error("Combat must fire, eject to pod, then return to hangar with the same ship identity");
        // Combat/pod recovery has already been verified above. Isolate the
        // subsequent docking case: original autopilot deliberately dodges a
        // pursuing fighter, which need not permit docking within this timeout.
        for(auto* link=game.core()->GetShips()->first();link;) {
            auto* ship=link->data();link=link->next();
            if(ship!=game.playerShip() && ship->GetPilotType()<c_ptPlayer)ship->Terminate();
        }
        game.respawn();game.step({},1.0/165);
        if(game.state().dead || game.state().hull<0.99f)throw std::runtime_error("Respawn failed");
        if(!game.returnToBase())throw std::runtime_error("Return to base unavailable");
        // Prevent practice from spawning replacement opponents during docking.
        for(int i=0;i<12000 && !game.state().docked;++i) {
            game.step({},1.0/165);
            for(auto* link=game.core()->GetShips()->first();link;) {
                auto* ship=link->data();link=link->next();
                if(ship!=game.playerShip() && ship->GetPilotType()<c_ptPlayer)ship->Terminate();
            }
        }
        if(!game.state().docked) {
            const auto& state=game.state();
            fprintf(stderr,"Return diagnostic: dead=%d pod=%d autopilot=%d target=%s distance=%.1f speed=%.1f hull=%.3f deaths=%d position=(%.1f,%.1f,%.1f)\n",state.dead,state.pod,state.autopilot,state.target.c_str(),state.targetDistance,state.speed,state.hull,state.deaths,state.position.x,state.position.y,state.position.z);
            auto* ship=game.playerShip();
            fprintf(stderr,"pilot=%d state=%x clock=%u fuel=%f\n",ship->GetPilotType(),ship->GetStateM(),game.core()->GetLastUpdate().clock(),ship->GetFuel());
            for(Command slot=0;slot<c_cmdMax;++slot)fprintf(stderr,"cmd %d=%d target %s\n",slot,ship->GetCommandID(slot),ship->GetCommandTarget(slot)?ship->GetCommandTarget(slot)->GetName():"null");
            for(auto& message:state.messages)fprintf(stderr,"%.2f %s\n",message.time,message.text.c_str());
            throw std::runtime_error("Return to base failed to dock");
        }
        for(int i=0;i<5;++i){game.nextHull();game.step({},1.0/165);if(!game.state().docked)throw std::runtime_error("Refit left the hangar");}
        printf("Respawn and all six native loadouts passed\n");
        return 0;
    }catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}
}
