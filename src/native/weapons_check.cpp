#include "test_fixture.hpp"
#include "game.hpp"
#include "../ClientCore/WeaponSelection.h"
#include "../ClientCore/EquipmentStatus.h"
#include "test_reference/weapon_selection.hpp"
using NativeTest::require;
namespace {
void reference(NativeTest::Fixture& f) {
    int cases=0;
    for(auto* l=f.mission->GetHullTypes()->first();l;l=l->next()) {
        auto* type=l->data();int count=type->GetMaxFixedWeapons();if(count>4)continue;
        auto* ship=f.ship(0,type->GetObjectID());
        for(int mask=0;mask<(1<<count);++mask) {
            while(auto* p=ship->GetParts()->first())p->data()->Terminate();
            for(Mount mount=0;mount<count;++mount)if(mask&(1<<mount)) {
                for(auto* p=f.mission->GetPartTypes()->first();p;p=p->next())if(p->data()->GetEquipmentType()==ET_Weapon && type->CanMount(p->data(),mount)) {require(ship->CreateAndAddPart(p->data(),mount,1),"weapon fixture");break;}
            }
            for(Mount selected=0;selected<std::max(1,count);++selected)for(bool previous:{false,true}) {
                OriginalWeapons::Client original{ship,selected};if(previous)original.PreviousWeapon();else original.NextWeapon();
                require(original.m_selectedWeapon==Allegiance::CycleWeapon(ship,selected,previous),"original weapon cycling differs");++cases;
                for(Mount mount=0;mount<5;++mount) {
                    OriginalWeapons::Client original{ship,selected};original.SetSelectedWeapon(mount);Mount actual=selected;
                    if(Allegiance::CanSelectWeapon(ship,mount))actual=mount;
                    require(actual==original.m_selectedWeapon,"original selection validity differs");++cases;
                }
            }
        }
    }
    auto* ship=f.ship();auto* weapon=ship->CreateAndAddPart(f.types(ET_Weapon,ship).front(),0,1);
    auto* launcher=ship->CreateAndAddPart(f.types(ET_Magazine,ship).front(),0,10);
    for(auto* part:{weapon,launcher})for(Mount mount:{Mount(-1),Mount(0)})for(float mounted:{0.f,.4f,1.f})for(float armed:{0.f,.7f,1.f}) {
        part->SetMountID(mount);part->SetMountedFraction(mounted);int calls=0;
        auto actual=Allegiance::EquipmentReadyState(part,[&](IlauncherIGC*){++calls;return armed;});
        require(actual==OriginalWeapons::ready(part,armed),"original ready-state policy differs");
        require(calls==(mount>=0 && mounted>=1 && IlauncherIGC::IsLauncher(part->GetObjectType())?1:0),"arming query must remain lazy");++cases;
    }
    int calls=0;require(Allegiance::EquipmentReadyState(nullptr,[&](IlauncherIGC*){++calls;return 1;})==0 && !calls,"empty equipment has no arming query");
    for(float load:{.1f,1.f,2.f})for(float remaining:{-1.f,0.f,.01f,.5f,1.f,2.f})require(Allegiance::LauncherArmingFraction(remaining,load)==1.f-std::max(0.f,remaining/load),"original launcher arming fraction");
    printf("%d original weapon selection/readiness comparisons passed\n",cases);
}
void session(const std::filesystem::path& art) {
    Native::Assets assets(art);Native::SessionOptions options;options.enemies=0;options.startInHangar=false;
    Native::Game game(art,[&](auto& name,auto&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options);
    auto* ship=game.playerShip();int count=ship->GetHullType()->GetMaxFixedWeapons();require(count>1,"multi-gun native fixture");
    game.selectWeapon(1);game.step({},.01);
    require(game.state().selectedWeapon==1 && (ship->GetStateM()&selectedWeaponMaskIGC)==1,"single fire selects requested gun");
    game.selectWeapon(1);game.step({},.01);
    require(game.state().selectedWeapon<0 && (ship->GetStateM()&selectedWeaponMaskIGC)==1,"group fire retains selected lead gun");
    Native::FlightInput fire;fire.fire=true;game.step(fire,.01);
    require((ship->GetStateM()&weaponsMaskIGC)==allWeaponsIGC,"group fires all guns");
    game.selectWeapon(-1);game.step(fire,.01);
    require(game.state().selectedWeapon==1 && (ship->GetStateM()&weaponsMaskIGC)==oneWeaponIGC,"toggle group returns to remembered gun");
    auto expected=Allegiance::CycleWeapon(ship,1,false);game.cycleWeapon();game.step({},.01);
    require(game.state().selectedWeapon==expected,"native next weapon follows original installed mounts");
    auto selected=game.state().selectedWeapon;game.selectWeapon(32768);require(game.state().selectedWeapon==selected,"invalid mount cannot wrap into a valid gun");
    game.takeFeedback();bool shotFeedback=false;
    for(int i=0;i<200;++i){game.step(fire,.01);shotFeedback|=bool(game.takeFeedback().mask&1);}
    require(shotFeedback && !game.takeFeedback().mask,"actual gun shots queue bounded feedback and draining clears it");
    auto* site=game.core()->GetIgcSite();site->PlayFFEffect(effectFire,ship);site->PlayFFEffect(effectBounce,ship,135);
    site->PlayFFEffect(effectExplode,ship);site->PlayFFEffect(-1,ship);site->PlayFFEffect(3,ship);
    auto feedback=game.takeFeedback();require(feedback.mask==7 && feedback.direction[1]==135,"original force IDs and impact direction preserved");
    site->PlayFFEffect(effectFire,game.core()->GetStations()->first()->data());require(!game.takeFeedback().mask,"other model feedback cannot shake the controlled pilot's device");
    Vector p=ship->GetPosition();site->DamageShipEvent(game.core()->GetLastUpdate(),ship,ship,c_dmgidCollision,1,1,p,p+ship->GetOrientation().GetRight());
    feedback=game.takeFeedback();require(feedback.mask==2 && feedback.direction[1]==90,"damage event uses original local impact direction");
    for(auto* l=ship->GetParts()->first();l;l=l->next())if(IlauncherIGC::IsLauncher(l->data()->GetObjectType()) && l->data()->GetMountID()>=0) {
        auto* launcher=static_cast<IlauncherIGC*>(l->data());launcher->SetMountedFraction(1);launcher->SetTimeLoaded(game.core()->GetLastUpdate()+.5f);
    }
    game.step({},.001);int arming=0;
    for(auto& e:game.state().equipment)if(e.mount>=0 && (e.kind==Native::PartKind::Missile || e.kind==Native::PartKind::Chaff || e.kind==Native::PartKind::Dispenser)) {require(e.readiness==2 && e.ready<1,"all launcher types expose original arming status");++arming;}
    require(arming>=2,"arming coverage includes missile and countermeasure");
    puts("Native gun/group/lead selection, cycling and launcher readiness passed");
}
}
int main(int argc,char** argv) {
    try{auto art=argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork();{NativeTest::Fixture f(art);reference(f);}session(art);return 0;}
    catch(const std::exception& e){fprintf(stderr,"weapons: %s\n",e.what());return 1;}
}
