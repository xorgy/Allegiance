#include "game.hpp"
#include "test_fixture.hpp"
#include "../ClientCore/CombatFeedback.h"
#include <algorithm>
#include <array>
using namespace Native;
using NativeTest::require;
namespace {
struct Tip{bool critical{};std::string text;bool operator==(const Tip&)const=default;};
std::string tint(IsideIGC* side,const std::string& text){char value[512];snprintf(value,sizeof(value),START_COLOR_STRING "%s" END_COLOR_STRING,(PCC)ConvertColorToString(side->GetColor()),text.c_str());return value;}
// Frozen FM_S_KILL_SHIP caption table; adapted PlayerInfo fields only.
Tip originalLoss(IshipIGC* victim,IshipIGC* launcher,bool death,int role,int side){
    auto* owner=victim->GetSide();auto type=victim->GetPilotType();bool drone=type==c_ptBuilder || type==c_ptMiner || type==c_ptCarrier;
    if(!launcher){
        if(role==0)return {true,death?"You were killed":"You ejected"};
        if(drone)return {owner->GetObjectID()==side,tint(owner,victim->GetName())+" was killed"};
        return {false,tint(owner,victim->GetName())+(death?" was killed":" ejected")};
    }
    if(role==0)return {true,std::string(death?"You were killed by ":"You were forced to eject by ")+tint(launcher->GetSide(),launcher->GetName())};
    if(role==1)return {true,death?"You killed "+tint(owner,victim->GetName()):"You forced "+tint(owner,victim->GetName())+" to eject"};
    auto name=drone && owner->GetObjectID()!=side?tint(owner,std::string(owner->GetName())+" "+victim->GetName()):std::string(victim->GetName());
    return {drone,name+(death?" was killed by ":" was forced to eject by ")+launcher->GetName()};
}
void policies(const std::filesystem::path& art){
    NativeTest::Fixture world(art);auto* launcher=world.ship(0);launcher->SetName("Attacker");int cases=0;
    for(auto type:{c_ptPlayer,c_ptCheatPlayer,c_ptWingman,c_ptMiner,c_ptBuilder,c_ptCarrier,c_ptLayer}){
        auto* victim=world.ship(1,414,type);victim->SetName("Victim");
        for(bool death:{false,true})for(bool credit:{false,true})for(int role=0;role<3;++role)for(SideID side=0;side<6;++side){
            auto* actor=credit?launcher:nullptr;ShipID viewer=role==0?victim->GetObjectID():role==1?launcher->GetObjectID():31000;Tip actual;int calls=0;
            Allegiance::ShipLossFeedback(victim,actor,death,viewer,side,[&](bool critical,const char* format,auto... args){char text[512];snprintf(text,sizeof(text),format,args...);actual={critical,text};++calls;});
            require(calls==1 && actual==originalLoss(victim,actor,death,role,side),"original kill/ejection caption, color or priority");++cases;
        }
        if(type==c_ptBuilder){victim->SetBaseData(world.mission->GetStationTypes()->first()->data());require(Allegiance::DroneLossSound(victim)==static_cast<IstationTypeIGC*>(victim->GetBaseData())->GetConstructorDestroyedSound(),"original constructor loss cue");}
        else require(Allegiance::DroneLossSound(victim)==(type==c_ptMiner?minerDestroyedSound:type==c_ptCarrier?carrierDestroyedSound:towerLayerDestroyedSound),"original drone loss cue");
        if(type==c_ptLayer){bool mine=false;for(auto* l=world.mission->GetExpendableTypes()->first();l;l=l->next())if(l->data()->GetObjectType()==OT_mineType){victim->SetBaseData(l->data());require(Allegiance::DroneLossSound(victim)==minefieldLayerDestroyedSound,"original minefield-layer loss cue");mine=true;break;}require(mine,"core minefield-layer sound fixture");}
    }
    auto* ship=world.ship(0);ship->SetName("Courier");
    for(SideID flag:{SideID(1),SideID(2),SideID(SIDE_TEAMLOBBY)})for(SideID viewer=0;viewer<6;++viewer){
        std::vector<Tip> tips;std::vector<SoundID> sounds;auto* side=world.mission->GetSide(viewer);
        Allegiance::FlagPickupFeedback(ship,flag,side,[&](bool critical,const char* format,auto... args){char text[512];snprintf(text,sizeof(text),format,args...);tips.push_back({critical,text});},[&](SoundID sound){sounds.push_back(sound);});
        bool announced=viewer==0 || (flag>0 && viewer==flag);require(tips.size()==size_t(announced) && sounds.size()==size_t(announced),"original flag/artifact recipient gate");
        if(announced){std::string text=viewer==0?(flag==SIDE_TEAMLOBBY?"Courier has found an artifact.":"Courier has stolen "+tint(world.mission->GetSide(flag),std::string(world.mission->GetSide(flag)->GetName())+"'s")+" flag."):tint(ship->GetSide(),"Courier")+" of "+tint(ship->GetSide(),ship->GetSide()->GetName())+" has stolen our flag.";
            require(tips[0]==Tip{true,text} && sounds[0]==(viewer==0?(flag==SIDE_TEAMLOBBY?artifactFoundSound:enemyFlagLostSound):flagLostSound),"original objective tip/cue");}
    }
    auto* foreign=world.ship(1);std::vector<Tip> sideZero;Allegiance::FlagPickupFeedback(foreign,0,world.mission->GetSide(0),[&](auto...){sideZero.push_back({});},[](auto){});require(sideZero.empty(),"historical side-zero flag-loss gate changed");
    printf("%d original kill/ejection role/type/fate/side comparisons and 19 objective audience/caption/cue cases passed\n",cases);
}
auto loader=[](auto&,auto&){return ModelHandle{1,1};};
SessionOptions options(){SessionOptions o;o.scenario=Scenario::Conquest;o.startInHangar=true;o.enemies=0;o.rules.miners=0;o.rules.teams=3;return o;}
struct Views{
    Game& authority;SessionOptions setup;std::vector<int> ids;std::vector<std::unique_ptr<Game>> games;
    Views(Game& host,const std::filesystem::path& art,SessionOptions o,std::vector<int> pilots):authority(host),setup(o),ids(std::move(pilots)){
        for(size_t i=0;i<ids.size();++i)games.push_back(std::make_unique<Game>(art,loader,0,0,setup,EffectLoader{},GameRole::Replica));receive(60);clear();
    }
    void receive(int tick){for(size_t i=0;i<ids.size();++i){auto frame=authority.networkFrame(ids[i],tick,0);games[i]->applyNetworkFrame(Net::frame(Net::packet(frame)));}}
    void clear(){for(auto& game:games){game->clearMessages();game->clearSounds();}}
};
int tip(Game& game,const std::string& text,bool critical){return std::count_if(game.state().messages.begin(),game.state().messages.end(),[&](auto& m){return m.text==text && m.tip && m.warning==critical && !m.chat;});}
int heard(Game& game,int id){return std::count_if(game.sounds().begin(),game.sounds().end(),[&](auto& s){return s.id==id;});}
void losses(const std::filesystem::path& art){
    auto o=options();Game authority(art,loader,0,0,o,{},GameRole::Authority);auto* pilot=authority.playerShip();auto* core=authority.core();
    int victimId=authority.addLocalPilot("Target pilot",1),friendId=authority.addLocalPilot("Witness",0),friend2=authority.addLocalPilot("Second witness",0),otherId=authority.addLocalPilot("Third faction",2);
    require(victimId>=0 && friendId>=0 && friend2>=0 && otherId>=0,"combat feedback participants");auto* victim=core->GetShip(victimId);auto* sector=pilot->GetStation()->GetCluster();
    for(auto* ship:{pilot,victim}){ship->SetStation(nullptr);ship->SetCluster(sector);ship->SetPosition(Vector(50000,0,0));}
    Views views(authority,art,o,{pilot->GetObjectID(),victimId,friendId,friend2,otherId});
    core->GetIgcSite()->KillShipEvent(core->GetLastUpdate(),victim,pilot,100,victim->GetPosition(),Vector::GetZero());views.receive(61);
    require(victim->GetBaseHullType()->HasCapability(c_habmLifepod),"real ejection preserves pod recovery");
    require(tip(*views.games[0],"You forced "+tint(victim->GetSide(),"Target pilot")+" to eject",true)==1 && tip(*views.games[1],"You were forced to eject by "+tint(pilot->GetSide(),pilot->GetName()),true)==1,"real attacker/victim ejection tips");
    for(size_t i=2;i<views.games.size();++i)require(tip(*views.games[i],"Target pilot was forced to eject by "+std::string(pilot->GetName()),false)==1 && heard(*views.games[i],newNonCriticalMsgSound)==1 && !heard(*views.games[i],newCriticalMsgSound),"common ejection tip/cue differs by uninvolved pilot");
    auto reportA=authority.networkReport(friendId,61,0),reportB=authority.networkReport(friend2,61,0);const Net::Frame* sharedA=nullptr;const Net::Frame* sharedB=nullptr;
    for(auto& w:reportA.worlds)if(w.domain==0x300000)sharedA=w.frame.get();for(auto& w:reportB.worlds)if(w.domain==0x300000)sharedB=w.frame.get();
    require(sharedA && sharedA==sharedB && std::none_of(reportA.personal.notices.begin(),reportA.personal.notices.end(),[](auto& n){return n.flags&Net::NoticeSharedTip;}),"equal audience tips are copied into separate recipient reports");
    auto sharedFeedback=[](const Net::ReportCapture& report){for(const auto& world:report.worlds)if(world.domain==0x300000)return world.frame;return std::shared_ptr<const Net::Frame>{};};
    authority.beginNetworkPublication(62);auto retained=sharedFeedback(authority.networkReport(friendId,62,0));
    authority.beginNetworkPublication(63);auto next=sharedFeedback(authority.networkReport(friendId,63,0));
    require(retained && retained==next,"Unchanged feedback is recaptured each publication cycle");
    views.clear();views.receive(62);for(auto& game:views.games)require(game->state().messages.empty() && game->sounds().empty(),"unchanged reports repeat combat tips/cues");
    auto frame=authority.networkFrame(friendId,62,0);uint64_t watermark=frame.notices.back().number;auto acknowledged=authority.networkFrame(friendId,63,0,false,{},false,{watermark,UINT64_MAX});require(acknowledged.notices.empty(),"acknowledged common/private tips remain in flattened reports");
    views.clear();core->GetIgcSite()->KillShipEvent(core->GetLastUpdate(),victim,nullptr,-1,victim->GetPosition(),Vector::GetZero());views.receive(64);
    require(tip(*views.games[1],"You were killed",true)==1 && victim->GetStation(),"real abandoned-pod death/restart tip");
    for(size_t i:{size_t(0),size_t(2),size_t(3),size_t(4)})require(tip(*views.games[i],tint(victim->GetSide(),"Target pilot")+" was killed",false)==1,"environmental death announcement to nonvictims");
    DataShipIGC data{};data.shipID=core->GenerateNewShipID();data.hullID=pilot->GetBaseHullType()->GetObjectID();data.sideID=1;data.pilotType=c_ptMiner;data.baseObjectID=NA;data.partsOffset=sizeof(data);strcpy(data.name,"Survey miner");
    auto* miner=static_cast<IshipIGC*>(core->CreateObject(core->GetLastUpdate(),OT_ship,&data,sizeof(data)));require(miner,"miner loss fixture");miner->SetCluster(sector);miner->SetPosition(Vector(45000,0,0));miner->Release();views.receive(65);views.clear();
    core->GetIgcSite()->KillShipEvent(core->GetLastUpdate(),miner,pilot,100,miner->GetPosition(),Vector::GetZero());views.receive(66);
    require(tip(*views.games[1],"Survey miner was killed by "+std::string(pilot->GetName()),true)==1 && heard(*views.games[1],minerDestroyedSound)==1,"own miner loss lacks original critical tip/destruction cue");
    for(size_t i:{size_t(0),size_t(2),size_t(3),size_t(4)})require(!heard(*views.games[i],minerDestroyedSound),"miner destruction cue leaks beyond its faction");
    require(tip(*views.games[4],tint(miner->GetSide(),std::string(miner->GetSide()->GetName())+" Survey miner")+" was killed by "+pilot->GetName(),true)==1,"foreign miner loss lacks original side-qualified color/critical tip");
    core->GetShip(friendId)->SetSide(core->GetSide(1));views.games[2]->clearMessages();views.games[2]->clearSounds();auto changedTeam=authority.networkFrame(friendId,67,0);views.games[2]->applyNetworkFrame(changedTeam);
    require(!heard(*views.games[2],minerDestroyedSound),"changing factions replays historical faction-only destruction cues");
    Game newcomer(art,loader,0,0,o,{},GameRole::Replica);auto oldReports=authority.networkFrame(victimId,67,0);newcomer.applyNetworkFrame(oldReports);
    require(!tip(newcomer,"Survey miner was killed by "+std::string(pilot->GetName()),true) && !heard(newcomer,minerDestroyedSound),"a new subscriber hears historical mission-wide deaths/cues");
    puts("Actual combat reports: principal/common tips, exact priorities/cues, shared faction arena, acknowledgement/replay, pod recovery and drone-loss privacy passed");
}
void objectives(const std::filesystem::path& art){
    auto o=options();o.rules.conquest=0;o.rules.territory=100;o.rules.flags=2;Game authority(art,loader,0,0,o,{},GameRole::Authority);auto* core=authority.core();auto* ship=authority.playerShip();auto* home=ship->GetStation();
    int teammate=authority.addLocalPilot("Objective witness",0),enemy=authority.addLocalPilot("Flag owner",1),third=authority.addLocalPilot("Other faction",2);require(teammate>=0 && enemy>=0 && third>=0,"objective report participants");
    ItreasureIGC* flag=nullptr;for(auto* l=core->GetTreasures()->first();l;l=l->next())if(l->data()->GetTreasureCode()==c_tcFlag && l->data()->GetTreasureID()==1){flag=l->data();break;}require(flag,"generated enemy flag fixture");
    ship->SetStation(nullptr);ship->SetCluster(flag->GetCluster());Views views(authority,art,o,{ship->GetObjectID(),teammate,enemy,third});
    require(core->GetIgcSite()->HitTreasureEvent(core->GetLastUpdate(),ship,flag),"actual enemy flag pickup");flag->Terminate();views.receive(61);
    std::string pickup=std::string(ship->GetName())+" has stolen "+tint(core->GetSide(1),std::string(core->GetSide(1)->GetName())+"'s")+" flag.";
    require(tip(*views.games[0],pickup,true)==1 && tip(*views.games[1],pickup,true)==1 && heard(*views.games[0],enemyFlagLostSound)==1,"own faction flag pickup text/cue");
    require(heard(*views.games[2],flagLostSound)==1 && views.games[3]->state().messages.empty() && views.games[3]->sounds().empty(),"flag pickup audience/cue isolation");
    views.clear();require(core->GetIgcSite()->DockWithStationEvent(ship,home) && ship->GetFlag()==NA && ship->GetSide()->GetFlags()==1,"actual flag docking retains count/clearing");views.receive(62);
    auto chat=[&](Game& game,const std::string& text,int sound){return std::count_if(game.state().messages.begin(),game.state().messages.end(),[&](auto& m){return m.chat && !m.tip && m.chatTarget==CHAT_TEAM && m.text.ends_with(text);})==1 && heard(game,sound)==1;};
    std::string secured=std::string(ship->GetName())+" has secured "+core->GetSide(1)->GetName()+"'s flag.";
    require(chat(*views.games[0],secured,enemyFlagSecuredSound) && chat(*views.games[1],secured,enemyFlagSecuredSound),"flag delivery must use own team chat/voice");
    require(chat(*views.games[2],std::string(ship->GetName())+" of "+ship->GetSide()->GetName()+" has secured your flag.",flagSecuredSound) && views.games[3]->state().messages.empty() && views.games[3]->sounds().empty(),"flag delivery must reach only the two affected team channels");
    // A docked part unlocks its own named technology, without a generic summary.
    auto* weapon=ship->GetMountedPart(ET_Weapon,0);require(weapon,"technology recovery weapon fixture");auto* source=weapon->GetPartType();std::vector<char> bytes(source->Export(nullptr));source->Export(bytes.data());auto* data=reinterpret_cast<DataPartTypeIGC*>(bytes.data());data->partID=31000;data->successorPartID=NA;strcpy(data->name,"Recovered equipment");data->ttbmEffects.ClearAll();data->ttbmEffects.SetBit(c_ttbMax-1);
    auto* recovered=static_cast<IpartTypeIGC*>(core->CreateObject(core->GetLastUpdate(),OT_partType,bytes.data(),int(bytes.size())));require(recovered && ship->GetSide()->IsNewDevelopmentTechs(recovered->GetEffectTechs()),"new recoverable part technology");
    if(auto* old=ship->GetMountedPart(NA,-1))old->Terminate();require(ship->CreateAndAddPart(recovered,-1,1),"recovered technology cargo");recovered->Release();ship->SetStation(nullptr);views.receive(63);views.clear();ship->SetStation(home);views.receive(64);
    std::string technology=std::string(ship->GetName())+" has secured Recovered equipment";
    require(tip(*views.games[0],technology,true)==1 && tip(*views.games[1],technology,true)==1 && !tip(*views.games[2],technology,true) && !tip(*views.games[3],technology,true),"named recovered part tip must be critical and faction-private");
    require(!ship->GetSide()->IsNewDevelopmentTechs(data->ttbmEffects),"technology tip must retain the actual development unlock");
    views.clear();ship->SetStation(nullptr);ship->SetStation(home);views.receive(65);require(!tip(*views.games[0],technology,true) && !tip(*views.games[1],technology,true),"already unlocked technology repeats its recovery tip");
    puts("Actual objective reports: pickup colors/cues, team-only flag delivery chat/voice/counts, named technology unlock and repeated-dock retirement passed");
}
void stations(const std::filesystem::path& art){
    auto o=options();o.rules.conquest=0;o.rules.territory=100;Game authority(art,loader,0,0,o,{},GameRole::Authority);auto* core=authority.core();auto* pilot=authority.playerShip();auto* home=pilot->GetStation();
    int ally=authority.addLocalPilot("Allied witness",1),enemy=authority.addLocalPilot("Hostile witness",2);require(ally>=0 && enemy>=0,"station feedback participants");
    auto policy=authority.missionPolicy(1);policy.alliances[0]=policy.alliances[1]=0;require(authority.applyMissionPolicy(policy),"station feedback alliance policy");
    Views views(authority,art,o,{pilot->GetObjectID(),ally,enemy});for(auto& view:views.games)require(view->applyMissionPolicy(policy),"replica station alliance policy");
    for(int mode=0;mode<3;++mode){
        DataStationIGC data;require(home->Export(&data)==sizeof(data),"station announcement export");data.stationID=31000+mode;data.position+=Vector(30000,0,0);strcpy(data.name,"Forward outpost");
        auto* station=static_cast<IstationIGC*>(core->CreateObject(core->GetLastUpdate(),OT_station,&data,sizeof(data)));require(station,"station announcement fixture");station->SetSideVisibility(core->GetSide(1),true);station->SetSideVisibility(core->GetSide(2),mode!=2);station->Release();
        views.receive(61+mode*2);views.clear();auto* type=station->GetStationType();int ownCue=type->GetDestroyedSound(),enemyCue=type->GetEnemyDestroyedSound();
        std::string expected=mode==1?tint(home->GetSide(),std::string(home->GetSide()->GetName())+"'s Forward outpost")+" in "+home->GetCluster()->GetName()+" was destroyed.":tint(pilot->GetSide(),pilot->GetName())+" destroyed "+tint(home->GetSide(),std::string(home->GetSide()->GetName())+"'s Forward outpost")+" in "+home->GetCluster()->GetName()+".";
        core->GetIgcSite()->KillStationEvent(station,mode==1?nullptr:pilot,1);views.receive(62+mode*2);
        for(size_t i=0;i<views.games.size();++i){bool announced=i!=2 || mode!=2;require(tip(*views.games[i],expected,true)==int(announced),"original station-loss wording/sector/color/scanner audience");
            int cue=i<2?ownCue:enemyCue;if(cue>=0)require(heard(*views.games[i],cue)==int(announced),"original own/allied/enemy station-loss announcement cue");}
    }
    puts("Actual station loss: attacker/unknown branches, sector/name/colors, own/allied/enemy cues and unseen-station isolation passed");
}
}
int main(int argc,char** argv)try{auto art=argc>1?std::filesystem::path(argv[1]):Assets::steamArtwork();policies(art);losses(art);objectives(art);stations(art);return 0;}catch(const std::exception& e){fprintf(stderr,"Combat feedback: %s\n",e.what());return 1;}
