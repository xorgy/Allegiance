#include "game.hpp"
#include "test_fixture.hpp"
#include "../ClientCore/FlagLifecycle.h"
#include "../ClientCore/MissionRules.h"
#include "test_reference/mission_timeout.hpp"
using NativeTest::require;
namespace {
struct Session {
    Native::Assets assets;Native::Game game;
    Session(const std::filesystem::path& art,Native::MissionSettings rules):assets(art),game(art,[&](auto& name,auto&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options(rules)){}
    static Native::SessionOptions options(Native::MissionSettings rules){Native::SessionOptions o;o.scenario=Native::Scenario::Conquest;o.startInHangar=true;o.enemies=0;o.rules=rules;return o;}
    void pickup(ItreasureIGC* treasure) {
        auto* ship=game.playerShip();if(ship->GetStation())game.launch();ship->SetAutopilot(false);
        ship->SetCluster(treasure->GetCluster());ship->SetPosition(treasure->GetPosition()+Vector(0,0,1));ship->SetVelocity(Vector::GetZero());
        ship->SetBB(game.core()->GetLastUpdate(),game.core()->GetLastUpdate(),0);
        for(int i=0;i<50 && ship->GetFlag()==NA;++i)game.step({},1./165);
    }
    ItreasureIGC* flag(int id) {
        for(auto* l=game.core()->GetTreasures()->first();l;l=l->next())if(l->data()->GetTreasureCode()==c_tcFlag && l->data()->GetTreasureID()==id)return l->data();return nullptr;
    }
};
void flags(const std::filesystem::path& art,bool artifact) {
    Native::MissionSettings rules;rules.conquest=0;rules.territory=100;rules.flags=artifact?0:2;rules.artifacts=artifact?2:0;
    Session s(art,rules);auto& game=s.game;auto* ship=game.playerShip();auto* side=ship->GetSide();int flag=artifact?SIDE_TEAMLOBBY:1;
    require(s.flag(flag),"original map generator populated flag/artifact objective");
    auto* home=ship->GetStation();int id=ship->GetObjectID();
    if(!artifact){s.pickup(s.flag(0));require(ship->GetFlag()==NA && s.flag(0),"own flag cannot be picked up");}
    s.pickup(s.flag(flag));require(ship->GetFlag()==flag && !game.state().carriedObjective.empty(),"real IGC treasure collision collects objective and updates HUD");
    require(!game.ripcord(home->GetCluster()->GetObjectID()),"carried objective blocks ripcord");
    game.eject();require(ship->GetFlag()==NA && ship->GetObjectID()==id && game.state().pod,"ejection drops objective and preserves ship identity");
    require(s.flag(flag),"ejection leaves original flag treasure");
    game.eject();require(game.state().docked,"pod abandonment returns to own base");
    game.step({},1./165); // RestoreLoadout is deferred until the next client update.
    require(!game.state().pod,"docked client update restores the original purchasable ship");
    for(int count=1;count<=2;++count) {
        s.pickup(s.flag(flag));require(ship->GetFlag()==flag,"objective recollected");
        require(game.returnToBase(),"objective return-to-base order");
        // Transit is covered separately; exercise the actual IGC garage approach
        // in the home sector rather than waiting for a random multi-sector path.
        ship->SetCluster(home->GetCluster());ship->SetPosition(home->GetPosition()+Vector(0,0,home->GetRadius()+1000));ship->SetVelocity(Vector::GetZero());
        ship->SetBB(game.core()->GetLastUpdate(),game.core()->GetLastUpdate(),0);
        for(int i=0;i<30000 && !game.state().docked && !game.state().dead && !game.state().complete;++i)game.step({},1./165);
        require(game.state().docked && ship->GetFlag()==NA && game.state().carriedObjective.empty(),"actual station docking secures carried objective");
        require((artifact?side->GetArtifacts():side->GetFlags())==count,"objective counter incremented exactly once");
        require(game.state().complete==(count==2),"objective win fires only at configured threshold");
        require(s.flag(flag),"delivered objective respawns at original location policy");
    }
    require(game.state().teamScores.front().flags==(artifact?0:2) && game.state().teamScores.front().artifacts==(artifact?2:0),"native scoreboard exposes objective totals");
    printf("Actual %s pickup, ejection, recovery, return, respawn and victory passed\n",artifact?"artifact":"flag");
}
void timed(const std::filesystem::path& art) {
    Native::MissionSettings rules;rules.conquest=0;rules.territory=100;rules.minutes=1;
    Session session(art,rules);auto& game=session.game;auto* core=game.core();auto params=*core->GetMissionParams();
    auto* first=core->GetSide(0);auto* second=core->GetSide(1);int cases=0;
    // Frozen comparison includes objective ties, kill ties and death tie-breaks.
    for(int mask=0;mask<16;++mask)for(int score=0;score<4;++score) {
        params.iGoalConquestPercentage=(mask&1)?100:0;params.iGoalTerritoryPercentage=(mask&2)?100:0;
        params.nGoalFlagsCount=(mask&4)?3:0;params.nGoalArtifactsCount=(mask&8)?3:0;core->SetMissionParams(&params);
        first->SetFlags(score&1);second->SetFlags((score>>1)&1);first->SetArtifacts((score>>1)&1);second->SetArtifacts(score&1);
        auto a=OriginalTimeout::check(core);auto b=Allegiance::CheckTimedVictory(core);require(a.first==b.winner && std::string(a.second)==b.reason,"original timed victory priority/ties");++cases;
    }
    for(int i=0;i<5;++i){(i%2?first:second)->AddKill();if(i==2)first->AddDeath();auto a=OriginalTimeout::check(core);auto b=Allegiance::CheckTimedVictory(core);require(a.first==b.winner && std::string(a.second)==b.reason,"kill/death timed tie break");++cases;}
    params.iGoalConquestPercentage=params.iGoalTerritoryPercentage=params.nGoalFlagsCount=params.nGoalArtifactsCount=0;core->SetMissionParams(&params);
    for(int i=0;i<610 && !game.state().complete;++i)game.step({},.1);
    require(game.state().complete && game.state().result.find("Time expired:")==0 && game.state().timeRemaining==0,"native mission clock expires through shared victory rules");
    printf("%d frozen timed-victory comparisons and native countdown completion passed\n",cases);
}
void terminal(const std::filesystem::path& art,bool flying) {
    Native::MissionSettings rules;rules.conquest=0;rules.territory=100;rules.minutes=1;
    Session fixture(art,rules);auto& game=fixture.game;auto* core=game.core();auto* player=game.playerShip();auto* home=player->GetStation();
    int otherId=game.addLocalPilot("Terminal pilot",0);require(otherId>=0,"terminal test pilot");auto* other=core->GetShip(ShipID(otherId));
    auto outfit=game.currentLoadout();require(!outfit.parts.empty(),"terminal test has a real purchasable loadout");
    if(flying)game.launch();
    player->SetCommand(c_cmdAccepted,home,c_cidGoto);player->SetCommand(c_cmdQueued,other,c_cidDefend);player->SetAutopilot(false);
    for(int i=0;i<610 && !game.state().complete;++i)game.step({},.1);
    require(game.state().complete && core->GetMissionStage()==STAGE_OVER,"completed local authority must leave the started mission stage");
    auto result=game.state().result;double time=game.state().time;int money=game.state().money,value=player->GetValue(),ships=core->GetShips()->n(),buoys=core->GetBuoys()->n();
    float hull=player->GetFraction(),fuel=player->GetFuel();int ammo=player->GetAmmo(),bits=player->GetStateM();auto position=player->GetPosition();auto* station=player->GetStation();
    auto accepted=player->GetCommandID(c_cmdAccepted),queued=player->GetCommandID(c_cmdQueued);auto* acceptedTarget=player->GetCommandTarget(c_cmdAccepted);auto* queuedTarget=player->GetCommandTarget(c_cmdQueued);
    int otherMoney=game.pilotBalance(otherId);auto wing=other->GetWingID();auto* autoDonate=player->GetAutoDonate();int hullId=player->GetBaseHullType()->GetObjectID();
    require(!game.buyHull(hullId) && !game.applyLoadout(outfit) && !game.replacePart(Native::PartKind::Weapon,0,outfit.parts.front().id),"completed hangar cannot refit or purchase equipment");
    require(!game.invest(0,1) && !game.donateMoney(otherId,1) && !game.setAutoDonate(otherId) && !game.requestMoney(1) && !game.respondMoneyRequest(false),"completed accounts cannot move credits or requests");
    require(!game.setWing(otherId,1) && !game.handOverCommand(otherId) && !game.controlLocalPilot(otherId) && !game.removeLocalPilot(otherId) && game.addLocalPilot("Late pilot",0)==-1,"completed roster cannot change authority, seats or wings");
    require(!game.order(otherId,OT_station,home->GetObjectID(),c_cidGoto) && !game.acceptCommand(true) && !game.commandPosition({100,200,300}),"completed session cannot enqueue or execute orders");
    require(!game.reload() && !game.dropCargo() && !game.swapCargo(Native::PartKind::Weapon,0) && !game.ripcord(home->GetCluster()->GetObjectID()) && !game.returnToBase() && !game.boardShip(otherId) && !game.leaveShip() && !game.promoteGunner(otherId),"completed flight cannot mutate equipment, crew or travel");
    game.toggleAutopilot();game.toggleCloak();game.toggleVectorLock();game.eject();game.respawn();game.recoverToHangar();game.selectHull(hullId);game.clearCommand();game.rejectCommand();game.launch();
    Native::QuickChatCommand defend;defend.command=c_cidDefend;defend.targetMask=c_ttMe;defend.text="Defend me";
    require(game.sendQuickChat(Native::ChatChannel::Team,defend) && game.sendChat(Native::ChatChannel::Team,"Reviewing results"),"completed session retains text and voice-only quick chat");
    core->GetIgcSite()->SendChat(player,CHAT_TEAM,0,NA,"Late order",c_cidDefend,OT_ship,player->GetObjectID(),player,false);
    Native::FlightInput input;input.fire=input.missile=input.boost=true;input.throttle=1;input.yaw=1;game.step(input,.1);
    require(game.state().result==result && game.state().time==time && game.state().money==money && game.pilotBalance(otherId)==otherMoney && player->GetValue()==value && core->GetShips()->n()==ships && core->GetBuoys()->n()==buoys,"terminal operations changed result, time, credits, equipment or world objects");
    require(player==game.playerShip() && player->GetStation()==station && player->GetPosition()==position && player->GetFraction()==hull && player->GetFuel()==fuel && player->GetAmmo()==ammo && player->GetStateM()==bits && !player->GetAutopilot() && player->GetAutoDonate()==autoDonate && other->GetWingID()==wing,"terminal operations changed pilot, lifecycle, controls or donation links");
    require(player->GetCommandID(c_cmdAccepted)==accepted && player->GetCommandTarget(c_cmdAccepted)==acceptedTarget && player->GetCommandID(c_cmdQueued)==queued && player->GetCommandTarget(c_cmdQueued)==queuedTarget,"terminal operations/voice-only quick chat changed accepted/queued command slots");
    printf("Completed %s authority rejects refits, accounts, roster, orders, crew and flight mutations; text chat and result inspection remain available\n",flying?"flight":"hangar");
}
void prosperity(const std::filesystem::path& art) {
    Native::MissionSettings rules;rules.conquest=0;rules.territory=100;rules.prosperity=.01f;
    Session s(art,rules);auto& game=s.game;IbucketIGC* bucket=nullptr;
    for(auto* l=game.playerShip()->GetSide()->GetBuckets()->first();l;l=l->next())if(l->data()->GetBucketType()==OT_development && l->data()->GetBuyable()->GetObjectID()==c_didTeamMoney)bucket=l->data();
    require(bucket,"original EnterGame created prosperity bucket");require(game.invest(bucket->GetObjectID()),"prosperity bucket accepts credits");
    for(int i=0;i<100000 && !game.state().complete;++i)game.step({},.1);
    require(game.state().complete && bucket->GetCompleteF() && game.state().result.find("prosperity")!=std::string::npos,"original prosperity development ends session");
    puts("Original prosperity bucket production and victory passed");
}
void teamKills(const std::filesystem::path& art) {
    Native::MissionSettings rules;rules.conquest=0;rules.territory=100;rules.teamKills=1;
    Session s(art,rules);auto& game=s.game;auto* core=game.core();auto* player=game.playerShip();game.launch();IshipIGC* enemy=nullptr;
    for(auto* l=core->GetShips()->first();l;l=l->next())if(l->data()->GetSide()!=player->GetSide()){enemy=l->data();break;}
    require(enemy,"enemy miner for kill victory callback");enemy->SetCluster(player->GetCluster());
    int id=enemy->GetObjectID();core->GetIgcSite()->KillShipEvent(core->GetLastUpdate(),enemy,player,1,enemy->GetPosition(),enemy->GetPosition());game.step({},.01);
    require(game.state().complete && player->GetSide()->GetKills()==1 && !core->GetShip(id),"team-kill victory settles dead drone after terminal callback");
}
}
int main(int argc,char** argv) {
    try{auto art=argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork();flags(art,false);flags(art,true);timed(art);terminal(art,false);terminal(art,true);prosperity(art);teamKills(art);return 0;}
    catch(const std::exception& e){fprintf(stderr,"goals: %s\n",e.what());return 1;}
}
