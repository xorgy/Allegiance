#include "game.hpp"
#include "test_fixture.hpp"
#include "../ClientCore/FlightFeedback.h"
#include <algorithm>
#include <cstdarg>

using namespace Native;
using NativeTest::require;
namespace {
void policies(){
    struct Tip{bool critical;std::string text;};std::vector<Tip> tips;
    auto post=[&](bool critical,const char* format,auto... args){char text[512];snprintf(text,sizeof(text),format,args...);tips.push_back({critical,text});};
    Allegiance::RescuePilotFeedback(true,"O'Brien",post);
    Allegiance::RescuePilotFeedback(false,"O'Brien",post);
    Allegiance::RipcordActivationFeedback("Wasp","Wasp",true,post);
    Allegiance::RipcordActivationFeedback("Wasp","Aegis",false,post);
    require(tips.size()==4 && !tips[0].critical && tips[0].text=="You rescued O'Brien." && tips[1].critical && tips[1].text=="O'Brien rescued you.","frozen original rescue captions/priorities");
    require(tips[2].critical && tips[2].text=="Ripcording to Wasp" && tips[3].critical && tips[3].text=="Ripcording to Wasp, which is closest to Aegis","frozen original ripcord captions/priorities");
}
auto loader=[](auto&,auto&){return ModelHandle{1,1};};
SessionOptions options(){SessionOptions o;o.scenario=Scenario::Conquest;o.startInHangar=true;o.enemies=0;o.rules.miners=0;return o;}
void rescue(const std::filesystem::path& art){
    auto o=options();Game authority(art,loader,0,0,o,{},GameRole::Authority);
    auto* rescuer=authority.playerShip();int picked=authority.addLocalPilot("Stranded pilot",0),other=authority.addLocalPilot("Bystander",0);
    require(picked>=0 && other>=0,"rescue actors");auto* pod=authority.core()->GetShip(picked);
    auto* home=rescuer->GetStation();auto* cluster=home->GetCluster();
    rescuer->SetStation(nullptr);rescuer->SetCluster(cluster);
    pod->SetBaseHullType(pod->GetSide()->GetCivilization()->GetLifepod());pod->SetStation(nullptr);pod->SetCluster(cluster);
    Game pilotView(art,loader,0,0,o,{},GameRole::Replica),podView(art,loader,0,0,o,{},GameRole::Replica),otherView(art,loader,0,0,o,{},GameRole::Replica);
    pilotView.applyNetworkFrame(authority.networkFrame(rescuer->GetObjectID(),60,0));
    podView.applyNetworkFrame(authority.networkFrame(picked,60,0));otherView.applyNetworkFrame(authority.networkFrame(other,60,0));
    for(auto* view:{&pilotView,&podView,&otherView}){view->clearMessages();view->clearSounds();}
    require(authority.core()->GetIgcSite()->RescueShipEvent(pod,rescuer) && pod->GetStation()==home,"real rescue returns pod to own restart");
    auto receive=[&](Game& view,int pilot){auto frame=authority.networkFrame(pilot,61,0);view.applyNetworkFrame(Net::frame(Net::packet(frame)));};
    receive(pilotView,rescuer->GetObjectID());receive(podView,picked);receive(otherView,other);
    auto tip=[&](const Game& game,const char* text,bool critical){return std::count_if(game.state().messages.begin(),game.state().messages.end(),[&](auto& m){return m.text==text && m.tip && m.warning==critical && !m.chat;});};
    auto sound=[&](const Game& game,int id){return std::count_if(game.sounds().begin(),game.sounds().end(),[&](auto& s){return s.id==id;});};
    require(tip(pilotView,"You rescued Stranded pilot.",false)==1 && sound(pilotView,newNonCriticalMsgSound)==1 && sound(pilotView,rescuePlayerSound)==1,"rescuing pilot lacks original acknowledgement/cues");
    std::string rescued=std::string(rescuer->GetName())+" rescued you.";
    require(tip(podView,rescued.c_str(),true)==1 && sound(podView,newCriticalMsgSound)==1 && sound(podView,jumpSound)==1,"rescued pilot lacks original critical tip/jump cues");
    require(!sound(pilotView,jumpSound) && !sound(podView,rescuePlayerSound) && !sound(otherView,rescuePlayerSound) && !sound(otherView,jumpSound) && otherView.state().messages.empty(),"rescue role/private cues reach another pilot");
    auto positional=std::find_if(pilotView.sounds().begin(),pilotView.sounds().end(),[](auto& s){return s.id==rescuePlayerSound;});
    require(positional!=pilotView.sounds().end() && positional->emitter,"private rescue-player cue loses its original ship emitter");
    require(podView.state().docked && !podView.state().dead && std::none_of(podView.state().messages.begin(),podView.state().messages.end(),[](auto& m){return m.text.starts_with("Docked at ") || m.text.starts_with("Rescued by ") || m.text=="Returned to a restart station.";}),"rescue keeps substitute docking/rescue console text");
    // FedSrv announces the pickup before it discovers that no restart remains.
    pod->SetStation(nullptr);pod->SetCluster(cluster);
    while(auto* l=rescuer->GetSide()->GetStations()->first())l->data()->Terminate();
    require(!authority.core()->GetIgcSite()->RescueShipEvent(pod,rescuer) && !pod->GetStation(),"missing restart invents a docking destination");
    auto failed=authority.networkFrame(picked,62,0);
    require(std::count_if(failed.notices.begin(),failed.notices.end(),[&](auto& n){return n.text==rescued;})==2,"missing restart suppresses original pre-docking rescue acknowledgement");
    puts("Original rescue: both pilot tips/priorities/cues, private delivery, positional emitter, successful docking and lost-base feedback ordering passed");
}
IstationIGC* copyStation(ImissionIGC* core,IstationIGC* source,IclusterIGC* cluster,int id){
    DataStationIGC d;require(source->Export(&d)==sizeof(d),"ripcord station export");d.stationID=id;d.clusterID=cluster->GetObjectID();d.position=Vector(10000,10000,10000);strcpy(d.name,"Remote restart");
    auto* station=static_cast<IstationIGC*>(core->CreateObject(core->GetLastUpdate(),OT_station,&d,sizeof(d)));require(station,"remote ripcord station");station->SetSideVisibility(station->GetSide(),true);station->Release();return station;
}
void ripcordStation(IstationIGC* station){
    for(auto* l=station->GetMission()->GetStationTypes()->first();l;l=l->next())if(l->data()->HasCapability(c_sabmRipcord)){station->SetBaseStationType(l->data());return;}
    require(false,"core must contain a ripcord-capable station type");
}
void ripcord(const std::filesystem::path& art){
    auto o=options();Game game(art,loader,0,0,o);auto* core=game.core();auto* ship=game.playerShip();auto* home=ship->GetStation();
    ripcordStation(home);
    auto* local=home->GetCluster();IclusterIGC* remote=nullptr;
    for(auto* l=core->GetClusters()->first();l;l=l->next())if(l->data()!=local && !l->data()->GetStations()->n()){remote=l->data();break;}
    require(remote,"remote ripcord sector");auto* station=copyStation(core,home,remote,31000);
    for(auto* l=core->GetWarps()->first();l;l=l->next())l->data()->SetSideVisibility(ship->GetSide(),true);
    ship->SetStation(nullptr);ship->SetCluster(local);ship->SetPosition(home->GetPosition()+Vector(50000,0,0));
    require(!ship->GetBaseHullType()->HasCapability(c_habmNoRipcord),"ripcord test hull prohibits ripcording");
    require(station->GetStationType()->HasCapability(c_sabmRipcord),"ripcord test station lacks destination capability");
    require(ship->FindRipcordModel(remote)==station,"known remote station must be the IGC ripcord destination");
    game.clearCommand(); // Settle sector discoveries introduced by the station fixture.
    auto reset=[&]{game.clearMessages();game.clearSounds();};
    auto heard=[&](int id){return std::count_if(game.sounds().begin(),game.sounds().end(),[&](auto& s){return s.id==id;});};
    ship->SetCommand(c_cmdCurrent,nullptr,c_cidNone);ship->SetCommand(c_cmdAccepted,station,c_cidGoto);reset();
    require(game.ripcord() && ship->GetRipcordModel()==station,"keyboard ripcord ignores accepted order and substitutes home base");
    std::string active="Ripcording to "+std::string(remote->GetName());
    if(!(game.state().messages.size()==1 && game.state().messages[0].text==active && game.state().messages[0].warning && game.state().messages[0].tip && heard(newCriticalMsgSound)==1)){
        for(auto& message:game.state().messages)fprintf(stderr,"Ripcord tip: '%s' critical=%d tip=%d\n",message.text.c_str(),message.warning,message.tip);
        for(auto& sound:game.sounds())fprintf(stderr,"Ripcord cue: %d\n",sound.id);
        throw std::runtime_error("ripcord activation lacks original sector/critical tip");
    }
    reset();core->GetIgcSite()->RequestRipcord(ship,remote);require(game.state().messages.empty() && game.sounds().empty(),"same ripcord request repeats feedback or resets the original countdown");
    require(game.ripcord() && !ship->GetRipcordModel() && heard(salRipcordAbortedSound)==1 && game.state().messages.empty(),"keyboard cancel lacks original abort speech or adds text");
    ship->SetCommand(c_cmdCurrent,home,c_cidGoto);reset();require(game.ripcord() && ship->GetRipcordModel()==home,"keyboard current-order precedence");
    reset();require(game.ripcord(remote->GetObjectID()) && ship->GetRipcordModel()==station && game.state().messages.size()==1 && game.state().messages[0].text==active,"explicit sector selection cancels instead of retargeting an active ripcord");
    core->GetIgcSite()->RequestRipcord(ship,nullptr);ship->SetCommand(c_cmdCurrent,nullptr,c_cidNone);ship->SetCommand(c_cmdAccepted,nullptr,c_cidNone);reset();
    require(game.ripcord() && ship->GetRipcordModel()==home,"no-order keyboard ripcord must request its current sector");
    core->GetIgcSite()->RequestRipcord(ship,nullptr);
    IclusterIGC* fallback=nullptr;for(auto* l=core->GetClusters()->first();l;l=l->next())if(l->data()!=local && l->data()!=remote && !l->data()->GetStations()->n()){fallback=l->data();break;}
    require(fallback,"nearest-sector fallback fixture");reset();require(game.ripcord(fallback->GetObjectID()),"nearest-sector fallback ripcord request");
    auto* actual=ship->GetRipcordModel()->GetCluster();require(actual!=fallback,"fallback unexpectedly contains a ripcord target");
    require(game.state().messages.size()==1 && game.state().messages[0].text=="Ripcording to "+std::string(actual->GetName())+", which is closest to "+fallback->GetName(),"original nearest-sector activation caption");
    // Actual arrival retains its original jump cue but has no completion text.
    auto* destination=ship->GetRipcordModel();reset();require(core->GetIgcSite()->UseRipcord(ship,destination),"real ripcord arrival");
    ship->SetRipcordModel(nullptr);require(ship->GetCluster()==actual && heard(jumpSound)==1 && game.state().messages.empty(),"ripcord arrival changes location/cue or adds a completion message");
    ship->SetFlag(0);reset();require(!game.ripcord() && heard(salShipCantRipcordSound)==1 && game.state().messages.empty(),"flag-carrying ship lacks original cannot-ripcord speech");ship->SetFlag(NA);
    home->SetSide(core->GetSide(1));station->SetSide(core->GetSide(1));game.clearCommand();reset();
    require(!game.ripcord() && heard(salNoRipcordSound)==1 && game.state().messages.empty(),"denied ripcord lacks original speech or adds native text");
    auto soundIds=game.soundIds();for(int id:{rescuePlayerSound,salShipCantRipcordSound,salNoRipcordSound,salRipcordAbortedSound})require(std::find(soundIds.begin(),soundIds.end(),id)!=soundIds.end(),"newly restored cues are not preloaded");
    puts("Original ripcord: current/accepted/current-sector intent, idempotent requests, activation/fallback captions, cancellation, active retargeting, arrival, flag prohibition, denial and cue preloading passed");
}
void ripcordCrew(const std::filesystem::path& art){
    auto o=options();Game authority(art,loader,0,0,o,{},GameRole::Authority);auto* core=authority.core();auto* pilot=authority.playerShip();auto* home=pilot->GetStation();
    int passengerId=authority.addLocalPilot("Ripcord passenger",0),bystanderId=authority.addLocalPilot("Independent pilot",0);
    auto* passenger=core->GetShip(passengerId);require(passenger && bystanderId>=0,"ripcord crew participants");ripcordStation(home);
    passenger->SetParentShip(pilot);passenger->SetTurretID(NA);passenger->SetStation(home);
    auto* local=home->GetCluster();IclusterIGC* remote=nullptr;for(auto* l=core->GetClusters()->first();l;l=l->next())if(l->data()!=local && !l->data()->GetStations()->n()){remote=l->data();break;}
    require(remote,"ripcord crew remote sector");auto* station=copyStation(core,home,remote,31000);
    pilot->SetStation(nullptr);pilot->SetCluster(local);pilot->SetPosition(home->GetPosition()+Vector(50000,0,0));
    Game pilotView(art,loader,0,0,o,{},GameRole::Replica),crewView(art,loader,0,0,o,{},GameRole::Replica),otherView(art,loader,0,0,o,{},GameRole::Replica);
    auto receive=[&](int tick){for(auto pair:{std::pair{&pilotView,int(pilot->GetObjectID())},std::pair{&crewView,passengerId},std::pair{&otherView,bystanderId}}){auto frame=authority.networkFrame(pair.second,tick,0);pair.first->applyNetworkFrame(Net::frame(Net::packet(frame)));}};
    auto reset=[&]{for(auto* view:{&pilotView,&crewView,&otherView}){view->clearMessages();view->clearSounds();}};
    auto heard=[](Game& game,int id){return std::count_if(game.sounds().begin(),game.sounds().end(),[&](auto& sound){return sound.id==id;});};
    receive(60);reset();core->GetIgcSite()->RequestRipcord(pilot,remote);receive(61);
    std::string caption="Ripcording to "+std::string(remote->GetName());
    require(pilotView.state().messages.size()==1 && pilotView.state().messages[0].text==caption && crewView.state().messages.size()==1 && crewView.state().messages[0].text==caption && otherView.state().messages.empty(),"initial ripcord activation must reach pilot and source crew only");
    reset();core->GetIgcSite()->RequestRipcord(pilot,local);receive(62);
    require(pilotView.state().messages.size()==1 && crewView.state().messages.empty() && otherView.state().messages.empty(),"active ripcord retarget must notify only the requesting pilot");
    reset();core->GetIgcSite()->RequestRipcord(pilot,nullptr);receive(63);
    require(heard(pilotView,salRipcordAbortedSound)==1 && heard(crewView,salRipcordAbortedSound)==1 && !heard(otherView,salRipcordAbortedSound),"ripcord abort must reach pilot/source crew without leaking to bystanders");
    for(auto* view:{&pilotView,&crewView}){auto cue=std::find_if(view->sounds().begin(),view->sounds().end(),[](auto& sound){return sound.id==salRipcordAbortedSound;});require(cue!=view->sounds().end() && cue->emitter,"private ripcord abort must retain the original source-ship emitter");}
    core->GetIgcSite()->RequestRipcord(pilot,remote);station->SetSide(core->GetSide(1));reset();
    require(!core->GetIgcSite()->ContinueRipcord(pilot,station),"lost destination must abort ongoing ripcord");receive(64);
    require(heard(pilotView,salRipcordAbortedSound)==1 && heard(crewView,salRipcordAbortedSound)==1 && otherView.state().messages.empty(),"invalidated destination loses original abort feedback");
    puts("Original ripcord crew delivery: initial source crew, private retarget, cancellation, source emitters and invalidated destination passed");
}
void deploy(const std::filesystem::path& art){
    auto o=options();Game game(art,loader,0,0,o);auto* ship=game.playerShip();auto* home=ship->GetStation();ship->SetStation(nullptr);ship->SetCluster(home->GetCluster());ship->SetPosition(home->GetPosition()+Vector(50000,0,0));
    auto* dispenser=static_cast<IdispenserIGC*>(ship->GetMountedPart(ET_ChaffLauncher,0));
    if(!dispenser)for(auto* l=game.core()->GetPartTypes()->first();l;l=l->next())if(l->data()->GetEquipmentType()==ET_ChaffLauncher && ship->GetBaseHullType()->CanMount(l->data(),0)){dispenser=static_cast<IdispenserIGC*>(ship->CreateAndAddPart(l->data(),0,2));break;}
    require(dispenser,"chaff deployment fixture");dispenser->SetAmount(2);game.clearMessages();game.clearSounds();
    game.core()->GetIgcSite()->FireExpendable(ship,dispenser,game.core()->GetLastUpdate());
    require(dispenser->GetAmount()==1 && game.state().messages.empty() && std::count_if(game.sounds().begin(),game.sounds().end(),[](auto& s){return s.id==deployChaffSound;})==1,"original deployment loses quantity/cue or adds native text");
    puts("Original deployment: real IGC chaff creation/quantity/sound without substitute console text passed");
}
}
int main(int argc,char** argv)try{auto art=argc>1?std::filesystem::path(argv[1]):Assets::steamArtwork();policies();rescue(art);ripcord(art);ripcordCrew(art);deploy(art);return 0;}catch(const std::exception& e){fprintf(stderr,"Flight feedback: %s\n",e.what());return 1;}
