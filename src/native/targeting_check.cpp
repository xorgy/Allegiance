#include "test_fixture.hpp"
#include "game.hpp"
#include "settings.hpp"
#include "../ClientCore/Targeting.h"
#include "test_reference/targeting.hpp"
using NativeTest::require;
namespace {
struct Known : ThingSite {
    bool visible{true};
    bool GetSideVisibility(IsideIGC*)override{return visible;}
};
struct Fixture : NativeTest::Fixture {
    Fixture(const std::filesystem::path& art):NativeTest::Fixture(art,false){require(LoadIGCFile("training_6",mission),"targeting map");}
    TRef<ThingSite> CreateThingSite(ImodelIGC*)override{return new Known;}
    ~Fixture(){for(auto* l=mission->GetShips()->first();l;l=l->next())l->data()->DeleteDamageTrack();}
};
void reference(const std::filesystem::path& art) {
    Fixture f(art);auto* player=f.ship();player->CreateDamageTrack();auto* cluster=player->GetCluster();
    std::vector<ImodelIGC*> current{nullptr,player};
    int index=0;
    for(auto* l=f.mission->GetHullTypes()->first();l;l=l->next()) {
        auto* hull=l->data();if(!hull->GetCapabilities())continue;
        auto* ship=f.ship(index%3,hull->GetObjectID());ship->SetPosition(Vector((index%5)*100.f,(index%7)*50.f,-200.f-(index%11)*150.f));
        static_cast<Known*>(ship->GetThingSite())->visible=index%5!=0;
        if(current.size()<25)current.push_back(ship);
        if(++index==60)break;
    }
    for(auto* l=cluster->GetModels()->first();l;l=l->next())if(l->data()->GetObjectType()!=OT_ship)current.push_back(l->data());
    auto* station=cluster->GetStations()->first()->data();auto* remote=f.mission->GetClusters()->last()->data();
    auto* attacker=f.ship(1);auto* attacker2=f.ship(1);attacker->SetPosition(Vector(0,0,-500));attacker2->SetPosition(Vector(200,0,-600));
    current.push_back(attacker);current.push_back(attacker2);
    int cases=0,found=0;
    for(bool allied:{false,true})for(int posture=0;posture<3;++posture)for(int damage=0;damage<2;++damage)for(auto* selected:current) {
        f.mission->GetSide(0)->SetAllies(allied?3:NA);f.mission->GetSide(2)->SetAllies(allied?3:NA);
        player->GetDamageTrack()->Reset();
        if(damage){player->GetDamageTrack()->ApplyDamage(f.mission->GetLastUpdate(),attacker,10);player->GetDamageTrack()->ApplyDamage(f.mission->GetLastUpdate(),attacker2,5);}
        player->SetCommand(c_cmdCurrent,selected,c_cidNone);
        IclusterIGC* view=posture==0?nullptr:posture==1?cluster:remote;
        if(posture==1){player->SetCluster(nullptr);player->SetStation(station);}else{player->SetStation(nullptr);player->SetCluster(cluster);}
        if(selected)for(bool automatic:{false,true}) {
            player->SetAutopilot(automatic);player->SetCommand(c_cmdCurrent,selected,c_cidNone);player->SetCommand(c_cmdPlan,selected,c_cidGoto);
            OriginalTargeting::Window original{{player,view}};original.targetKilled(selected);
            ImodelIGC* replacement=nullptr;int calls=0,resumes=0;
            Allegiance::TargetKilled(player,selected,[&](ImodelIGC* m,CommandID c){replacement=m;require(c==c_cidDefault,"replacement target default command");++calls;},[&]{++resumes;});
            require(replacement==original.result && calls==original.calls && resumes==original.trekClient.resumes,"frozen destroyed-target selection/autopilot differs");
        }
        player->SetAutopilot(false);player->SetCommand(c_cmdPlan,nullptr,c_cidNone);
        for(TrekKey key=0;key<TK_Max;++key) {
            if(key==TK_TargetSelf || key==TK_TargetNothing)continue;
            OriginalTargeting::Window original{{player,view}};bool expected=original.run(key);
            ImodelIGC* result=nullptr;CommandID command=c_cidNone;int calls=0;
            bool actual=Allegiance::SelectTargetForKey(player,view,key,[&](ImodelIGC* model,CommandID cid){result=model;command=cid;++calls;});
            require(expected==actual && original.result==result && original.command==command && original.calls==calls,"frozen WinTrek targeting differs");
            if(actual){++cases;found+=result!=nullptr;}
        }
    }
    require(found>100,"targeting reference selected too few real models");
    // Rescue and capital landing filters use the source hull; also exercise the
    // parent/observer and gunner orientations, not only an independent fighter.
    auto* parent=f.ship(0,290);parent->SetOrientation(Orientation(Vector(1,0,0)));
    for(auto* l=f.mission->GetHullTypes()->first();l;l=l->next())if(l->data()->HasCapability(c_habmLifepod) || !l->data()->HasCapability(c_habmFighter)) {
        player->SetStation(nullptr);player->SetBaseHullType(l->data());player->SetCluster(cluster);
        for(auto key:{TK_TargetFriendlyBaseNearest,TK_TargetAlliedBaseNearest}) {
            OriginalTargeting::Window original{{player,nullptr}};original.run(key);ImodelIGC* result=nullptr;
            Allegiance::SelectTargetForKey(player,nullptr,key,[&](ImodelIGC* m,CommandID){result=m;});
            require(original.result==result,"source hull rescue/capital destination filter");++cases;
        }
    }
    player->SetParentShip(parent);player->SetOrientation(Orientation(Vector(0,0,-1)));
    for(Mount turret:{Mount(NA),Mount(0)}) {
        player->SetTurretID(turret);OriginalTargeting::Window original{{player,nullptr}};original.run(TK_TargetCenter);ImodelIGC* result=nullptr;
        Allegiance::SelectTargetForKey(player,nullptr,TK_TargetCenter,[&](ImodelIGC* m,CommandID){result=m;});
        require(result==original.result,"reticle target observer/gunner orientation");++cases;
    }
    player->SetParentShip(nullptr);
    printf("%d frozen targeting comparisons passed, %d real selections\n",cases,found);
}
void session(const std::filesystem::path& art) {
    Native::Assets assets(art);Native::SessionOptions options;options.enemies=3;options.allies=2;options.startInHangar=false;
    Native::Game game(art,[&](auto& name,auto&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options);
    game.step({},.01);auto* ship=game.playerShip();
    for(auto binding:Native::targetBindings) {
        auto* accepted=ship->GetCommandTarget(c_cmdAccepted);auto cid=ship->GetCommandID(c_cmdAccepted);
        require(game.targetKey(binding.key),"native target binding not implemented");
        require(ship->GetCommandTarget(c_cmdAccepted)==accepted && ship->GetCommandID(c_cmdAccepted)==cid,"target search overwrote accepted order");
    }
    require(game.targetKey(TK_TargetEnemyNearest) && ship->GetCommandTarget(c_cmdCurrent) && ship->GetCommandTarget(c_cmdCurrent)->GetSide()!=ship->GetSide(),"native nearest enemy selection");
    auto* first=ship->GetCommandTarget(c_cmdCurrent);game.nextTarget();require(ship->GetCommandTarget(c_cmdCurrent)!=first,"generic next target cycles original similar category");
    game.targetKey(TK_TargetNothing);require(!ship->GetCommandTarget(c_cmdCurrent) && game.state().targetId<0,"target nothing clears HUD immediately");
    game.targetKey(TK_TargetSelf);require(ship->GetCommandTarget(c_cmdCurrent)==ship,"target self retains original no-order semantics");
    auto id=game.addLocalPilot("Idle local pilot",0);require(id>=0,"idle local pilot fixture");auto* idle=game.core()->GetShip(id);idle->SetStation(nullptr);idle->SetCluster(ship->GetCluster());idle->SetPosition(ship->GetPosition()+Vector(200,0,0));
    for(auto* l=game.core()->GetShips()->first();l;l=l->next())if(l->data()->GetPilotType()<c_ptPlayer)while(auto* p=l->data()->GetParts()->first())p->data()->Terminate();
    game.targetKey(TK_TargetNothing);for(int i=0;i<81;++i)game.step({},.1);
    require(!game.state().dead && !ship->GetCommandTarget(c_cmdCurrent),"periodic practice spawning must not reacquire an explicitly cleared target");
    require(idle->GetCommandID(c_cmdCurrent)==c_cidNone && idle->GetCommandID(c_cmdAccepted)==c_cidNone,"practice drone orders must not commandeer an inactive human pilot");
    game.targetKey(TK_TargetEnemyNearest);auto* destroyed=ship->GetCommandTarget(c_cmdCurrent);require(destroyed,"destruction target fixture");
    TRef<ImodelIGC> keep=destroyed;destroyed->Terminate();
    require(ship->GetCommandTarget(c_cmdCurrent) && ship->GetCommandTarget(c_cmdCurrent)!=destroyed && ship->GetCommandTarget(c_cmdCurrent)->GetSide()!=ship->GetSide(),"native termination must retarget a similar enemy");
    puts("Native targeting keys dispatch to original search and preserve accepted commands");
}
}
int main(int argc,char** argv) {
    try{auto art=argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork();reference(art);session(art);return 0;}
    catch(const std::exception& e){fprintf(stderr,"targeting: %s\n",e.what());return 1;}
}
