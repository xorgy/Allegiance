#include "test_fixture.hpp"
#include "game.hpp"
#include "../ClientCore/WeaponIndicator.h"
#include "test_reference/weapon_indicator.hpp"
#include "test_reference/radar.hpp"
#include "../ClientCore/RadarModel.h"
#include "../ClientCore/ShipLifecycle.h"
using NativeTest::require;
namespace {
void reference(NativeTest::Fixture& f) {
    auto* ship=f.ship();auto* target=f.ship(1);auto* relay=f.ship(0);
    auto* sector=ship->GetCluster();
    while(auto* l=sector->GetAsteroids()->first())l->data()->Terminate();
    auto* station=sector->GetStations()->first()->data();
    for(auto* l=sector->GetStations()->first();l;){auto* next=l->next();if(l->data()!=station)l->data()->Terminate();l=next;}
    auto* original=ship->GetBaseHullType();
    auto cloneHull=[&](int id,int caps) {
        std::vector<uint8_t> bytes(original->Export(nullptr));original->Export(bytes.data());
        auto* data=reinterpret_cast<DataHullTypeIGC*>(bytes.data());data->hullID=id;data->habmCapabilities=HullAbilityBitMask(caps);data->successorHullID=NA;
        auto* hull=static_cast<IhullTypeIGC*>(f.mission->CreateObject(f.mission->GetLastUpdate(),OT_hullType,bytes.data(),bytes.size()));require(hull,"indicator hull fixture");hull->Release();return hull;
    };
    int caps=original->GetCapabilities()&~(c_habmLeadIndicator|c_habmRemoteLeadIndicator);
    auto* blind=cloneHull(31000,caps);auto* local=cloneHull(31001,caps|c_habmLeadIndicator);auto* remote=cloneHull(31002,caps|c_habmRemoteLeadIndicator);
    auto stationData=*static_cast<const DataStationTypeIGC*>(station->GetBaseStationType()->GetData());stationData.stationTypeID=31000;stationData.successorStationTypeID=NA;stationData.sabmCapabilities|=c_sabmRemoteLeadIndicator;
    auto* relayType=static_cast<IstationTypeIGC*>(f.mission->CreateObject(f.mission->GetLastUpdate(),OT_stationType,&stationData,sizeof(stationData)));require(relayType,"relay station fixture");station->SetBaseStationType(relayType);relayType->Release();
    target->SetPosition(Vector(0,0,-100));target->SetBaseHullType(blind);
    int count=0;
    for(bool ownLead:{false,true})for(bool turret:{false,true})for(bool shipRelay:{false,true})for(bool stationRelay:{false,true})for(bool ally:{false,true})for(bool far:{false,true}) {
        ship->SetBaseHullType(ownLead?local:blind);relay->SetBaseHullType(shipRelay?remote:blind);
        relay->SetSide(f.mission->GetSide(ally?2:0));station->SetSide(f.mission->GetSide(ally?2:0));relay->SetCluster(sector);
        f.mission->GetSide(0)->SetAllies(3);f.mission->GetSide(2)->SetAllies(3);
        relay->SetPosition(far?Vector(100000,0,0):Vector::GetZero());relay->SetOrientation(Orientation(Vector(0,0,-1)));
        station->SetPosition(stationRelay&&!far?Vector::GetZero():Vector(100000,0,0));
        bool expected=ownLead || turret || (!ally&&!far&&(shipRelay||stationRelay));
        if(Allegiance::HasLeadIndicator(ship,target,turret)!=expected)fprintf(stderr,"relay own=%d turret=%d ship=%d station=%d ally=%d far=%d scan=%d/%d caps=%d/%d\n",ownLead,turret,shipRelay,stationRelay,ally,far,relay->InScannerRange(target),station->InScannerRange(target),ship->GetBaseHullType()->GetCapabilities(),relay->GetBaseHullType()->GetCapabilities());
        require(Allegiance::HasLeadIndicator(ship,target,turret)==expected,"lead requires local hull, turret or own-side relay in scanner range");
        require(OriginalIndicator::relay(ship,target,turret)==expected,"frozen lead relay differs");++count;
    }
    auto* gun=f.types(ET_Weapon,ship).front();auto* weapon=static_cast<IweaponIGC*>(ship->CreateAndAddPart(gun,0,1));
    bool damage=false,healing=false;
    for(auto* l=f.mission->GetPartTypes()->first();l;l=l->next()) {
        auto* type=l->data();if(type->GetEquipmentType()!=ET_Weapon)continue;
        weapon->Terminate();weapon=static_cast<IweaponIGC*>(ship->CreateAndAddPart(type,NA,1));
        bool heal=static_cast<const DataProjectileTypeIGC*>(static_cast<IweaponIGC*>(weapon)->GetProjectileType()->GetData())->power<0;
        (heal?healing:damage)=true;
        for(SideID sid:{SideID(0),SideID(1),SideID(2),SideID(NA)})for(bool chase:{false,true})for(bool missile:{false,true}) {
            ImodelIGC* check=station;
            if(sid==NA)check=f.mission->GetWarps()->first()->data();else station->SetSide(f.mission->GetSide(sid));
            require(OriginalIndicator::target(weapon,check,ship->GetSide(),chase,missile)==Allegiance::WeaponIndicatorTarget(weapon,check,ship->GetSide(),chase||missile),"weapon target allegiance and camera policy");++count;
        }
    }
    require(damage&&healing,"reference tests must exercise both damage and nanite weapons");
    for(float f:{0.f,.25f,.99f,1.f,1.5f,1.99f,2.f,3.f}) {
        float expected=f<2?(f<1?1-f*.75f:.5f-f/4):0;
        require(std::abs(Allegiance::CockpitAimGreen(f*100,100)-expected)<1e-6,"original cockpit aim curve");
    }
    require(Allegiance::ExternalAimGreen(1,100,0,10)==0 && Allegiance::ExternalAimGreen(1,100,1,0)==0,"finite perpendicular/zero-radius external aim");
    printf("%d frozen indicator policy comparisons, local/remote/allied relays and damage/healing weapons passed\n",count);
}
void radarPolicy() {
    int cases=0;
    for(int mode=0;mode<4;++mode)for(int type:{0,1,2,3,4,5,7,8})for(int flags=0;flags<256;++flags)for(int range:{0,100,999,1000,3000})for(int useful=0;useful<3;++useful) {
        Native::Contact detail{};detail.type=type;detail.own=flags&1;detail.ally=flags&2;detail.pedestalFlag=flags&4;detail.missileThreat=flags&8;detail.plainRock=flags&16;detail.treasureUse=useful;
        bool important=flags&32,onScreen=flags&64,self=flags&128;
        OriginalRadar::Side mine{true},ally{detail.ally},enemy{false};
        OriginalRadar::Model source{&mine,nullptr,false,0},target{detail.missileThreat?&mine:&enemy,nullptr,false,0};
        OriginalRadar::Model model{detail.own?&mine:&ally,&target,detail.pedestalFlag,useful};
        // Native contacts omit the player's source ship; cover that original-only
        // branch with an adapter whose self flag can be set explicitly.
        struct Info : Native::Contact {bool isSelf;bool self()const{return isSelf;}} info;static_cast<Native::Contact&>(info)=detail;info.isSelf=self;
        auto a=OriginalRadar::check(mode,type,important,onScreen,range,range,&model,self?&model:&source,&mine,detail.plainRock?"Asteroid":"Uranium");
        auto b=Allegiance::RadarPresentation(mode,type,important,onScreen,range,range,info);
        require(a.icon==b.icon && a.label==b.label && a.stats==b.stats && a.tech==b.tech && a.range==b.range,"original radar LOD/object/range policy differs");++cases;
    }
    Native::Contact none{};auto hidden=Allegiance::RadarPresentation(-1,0,true,true,10,10,none);require(!hidden.icon&&!hidden.label&&!hidden.stats&&!hidden.range,"radar none suppresses all contacts");
    printf("%d frozen radar mode/object/range/importance/treasure comparisons passed\n",cases);
}
void session(const std::filesystem::path& art) {
    Native::Assets assets(art);Native::SessionOptions options;options.scenario=Native::Scenario::FreeFlight;options.enemies=0;options.startInHangar=true;
    Native::Game game(art,[&](auto& name,auto&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options);
    int friendId=game.addLocalPilot("Indicator target",0);require(friendId>=0,"friendly target pilot");auto* target=game.core()->GetShip(friendId);game.launch();for(int i=0;i<32;++i)game.step({},.1);auto* player=game.playerShip();
    target->SetStation(nullptr);target->SetCluster(player->GetCluster());target->SetPosition(player->GetPosition()+player->GetOrientation().GetForward()*500);target->SetVelocity(Vector::GetZero());
    game.selectTarget(OT_ship,friendId);game.step({},.01);
    require(!game.state().hasLead && !game.state().inRange,"damage weapon does not show friendly lead or range");
    Native::CameraInput view;view.mode=Native::CameraMode::Chase;game.camera(view);
    require(game.state().hasExternalAim && game.state().hasWeaponSolution && !game.state().hasLead,"chase view shows original projected aim rather than cockpit lead");
    view.mode=Native::CameraMode::Cockpit;game.camera(view);require(!game.state().hasExternalAim && !game.state().inRange,"camera switch immediately updates weapon policy");
    target->SetSide(game.core()->GetSide(1));target->SetCluster(player->GetCluster());for(int i=0;i<20;++i)game.step({},.01);
    game.selectTarget(OT_ship,friendId);game.step({},.01);require(game.state().hasWeaponSolution && game.state().inRange,"visible hostile has valid IGC weapon solution");
    auto contact=std::find_if(game.contacts().begin(),game.contacts().end(),[&](auto& c){return c.type==Native::ContactShip && c.id==friendId;});
    require(contact!=game.contacts().end() && contact->enemy && !contact->own && !contact->ally,"native radar observes changed allegiance");
    require(!contact->threat,"an enemy contact alone is not a firing threat");
    auto* core=game.core();
    player->ReceiveDamage(c_dmgidCollision,5,core->GetLastUpdate(),player->GetPosition(),player->GetPosition()+Vector(1,0,0),target);
    game.step({},.01);
    contact=std::find_if(game.contacts().begin(),game.contacts().end(),[&](auto& c){return c.type==Native::ContactShip && c.id==friendId;});
    require(contact!=game.contacts().end() && contact->threat && contact->threatFlash,"actual damage bucket marks and flashes the attacker");
    // Another ship taking damage from this attacker must not create a threat for
    // the local player after its own history is cleared.
    player->GetDamageTrack()->Reset();
    int otherId=game.addLocalPilot("Other victim",0);require(otherId>=0,"second victim fixture");auto* other=core->GetShip(otherId);
    other->SetStation(nullptr);other->SetCluster(player->GetCluster());other->SetPosition(player->GetPosition()+Vector(500,0,0));
    other->ReceiveDamage(c_dmgidCollision,5,core->GetLastUpdate(),other->GetPosition(),other->GetPosition()+Vector(1,0,0),target);
    game.step({},.01);
    contact=std::find_if(game.contacts().begin(),game.contacts().end(),[&](auto& c){return c.type==Native::ContactShip && c.id==friendId;});
    require(contact!=game.contacts().end() && !contact->threat,"authority damage histories do not leak other victims into local HUD");
    auto* treasure=Allegiance::CreatePartTreasure(core->GetLastUpdate(),player,10,core->GetFuelPack(),Vector(0,0,10),0,60,[](){return Vector(1,0,0);});
    require(treasure && Allegiance::UsefulRadarTreasure(player,treasure)==1,"fuel reserve useful with afterburner");
    if(auto* burner=player->GetMountedPart(ET_Afterburner,0))burner->Terminate();
    require(Allegiance::UsefulRadarTreasure(player,treasure)==0,"fuel reserve hidden without afterburner");
    int treasureId=treasure->GetObjectID();game.step({},.01);
    auto reserve=std::find_if(game.contacts().begin(),game.contacts().end(),[&](auto& c){return c.type==Native::ContactTreasure && c.id==treasureId;});
    require(reserve!=game.contacts().end() && reserve->treasureUse==0,"native radar carries original reserve usefulness");
    for(auto name:game.radarIcons())require(assets.image(std::string(name))!=nullptr,"every referenced core radar icon can be preloaded");
    auto direction=game.state().leadDirection;require(std::abs(glm::length(direction)-1)<1e-4f,"lead is a world direction independent of camera parallax");
    while(auto* l=player->GetParts()->first())l->data()->Terminate();game.step({},.01);
    require(!game.state().hasLead && !game.state().inRange && !game.state().hasWeaponSolution,"losing weapon clears stale range and aim");
    puts("Native indicator allegiance, camera transition, original lead and stale-range cleanup passed");
}
}
int main(int argc,char** argv) {
    try{auto art=argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork();{NativeTest::Fixture f(art);reference(f);}radarPolicy();session(art);return 0;}
    catch(const std::exception& e){fprintf(stderr,"indicator: %s\n",e.what());return 1;}
}
