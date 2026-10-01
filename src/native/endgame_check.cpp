#include "frontend.hpp"
#include "network_codec.hpp"
#include "network_arena.hpp"
#include "test_fixture.hpp"
#include "../ClientCore/MissionLifecycle.h"
#include <fstream>
#include <numeric>
using namespace Native;
using NativeTest::require;
void scoreTransitions(const std::filesystem::path& art){
    auto loader=[](const std::string&,const std::string&){return ModelHandle{1,1};};
    auto advance=[](Game& game,int ticks){for(int i=0;i<ticks;++i)game.step({},.1);};
    auto finish=[](Game& game,int losingSide){auto* base=game.core()->GetSide(SideID(losingSide))->GetStations()->first()->data();game.core()->GetIgcSite()->KillStationEvent(base,nullptr,1);game.step({},.01);require(game.state().endgame.frozen,"score transition fixture did not finish");};
    auto row=[](const Game& game,int id)->const Net::ResultPlayer&{const auto& rows=game.state().endgame.players;auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& p){return p.id==id;});require(found!=rows.end(),"transitioned pilot is missing from postgame");return *found;};
    for(int from:{0,1})for(int route:{0,1,2}){
        SessionOptions options;options.scenario=Scenario::Conquest;options.team=from;options.enemies=0;options.rules.miners=0;options.startInHangar=true;
        Game game(art,loader,0,0,options,{},GameRole::Authority);int moving=game.state().shipId;
        require(game.addLocalPilot("Remaining commander",from)>=0,"side-transition anchor missing");
        auto* pilot=game.playerShip();
        DataShipIGC data{};data.shipID=game.core()->GenerateNewShipID();data.sideID=SideID(1-from);data.hullID=pilot->GetBaseHullType()->GetObjectID();data.pilotType=c_ptWingman;data.baseObjectID=NA;data.partsOffset=sizeof(data);Net::text(data.name,"Scoring drone");
        TRef<IshipIGC> victim=static_cast<IshipIGC*>(game.core()->CreateObject(game.core()->GetLastUpdate(),OT_ship,&data,sizeof(data)));require(victim,"score reset victim missing");victim->Release();victim->CreateDamageTrack();victim->SetCluster(pilot->GetStation()->GetCluster());
        auto now=game.core()->GetLastUpdate();victim->GetDamageTrack()->ApplyDamage(now,pilot,100);
        game.core()->GetIgcSite()->KillShipEvent(now,victim,pilot,100,victim->GetPosition(),Vector::GetZero());
        require(pilot->GetKills()==1,"side-transition fixture did not earn a kill");
        pilot->SetDeaths(2);pilot->SetEjections(3);advance(game,100);
        if(route)require(game.moveNetworkPilotToTeam(moving,SIDE_TEAMLOBBY),"No Team score retirement failed");
        if(route==1)advance(game,20);
        if(route!=2)require(game.moveNetworkPilotToTeam(moving,1-from),"faction score reset failed");
        require(pilot->GetKills()==0 && pilot->GetDeaths()==2 && pilot->GetEjections()==3,"original side reset did not clear kills and retain lives");
        advance(game,50);finish(game,from);
        if(route==2)require(std::none_of(game.state().endgame.players.begin(),game.state().endgame.players.end(),[&](const auto& p){return p.id==moving;}),"No Team pilot retains a departed faction's results row");
        else {const auto& p=row(game,moving);require(p.side==1-from && p.kills==0 && p.assists==0 && p.deaths==2 && p.ejections==3 && std::abs(p.played-5)<.02f && p.score==0 && (p.flags&1),"faction switch carries old score/time or loses new command credit");}
    }
    for(bool resume:{false,true}){
        SessionOptions options;options.scenario=Scenario::Conquest;options.enemies=0;options.rules.miners=0;options.startInHangar=true;
        Game game(art,loader,0,0,options,{},GameRole::Authority);int pilot=game.addLocalPilot("Reconnect timer",0);require(pilot>=0,"reconnect score fixture missing");
        advance(game,100);game.networkPilotConnected(pilot,false);game.networkPilotConnected(pilot,false);advance(game,100);
        if(resume){game.networkPilotConnected(pilot,true);game.networkPilotConnected(pilot,true);advance(game,50);}
        else game.retireNetworkPilot(pilot);
        finish(game,1);require(std::abs(row(game,pilot).played-(resume?15:10))<.02f,"reconnect grace contributes to played time or duplicate transitions restart it");
        auto frozen=game.state().endgame;game.networkPilotConnected(pilot,true);advance(game,10);require(game.state().endgame==frozen,"late reconnect mutates frozen results");
    }
    {
        SessionOptions options;options.scenario=Scenario::Conquest;options.enemies=0;options.rules.miners=0;options.startInHangar=true;
        Game game(art,loader,0,0,options,{},GameRole::Authority);advance(game,100);int claimed=game.joinNetworkPilot("Claimed pilot",0,true);advance(game,50);
        finish(game,1);require(std::abs(row(game,claimed).played-5)<.02f && (row(game,claimed).flags&1),"initial seat contributes pre-admission time or loses command scoring");
    }
    {
        SessionOptions options;options.scenario=Scenario::Conquest;options.enemies=0;options.rules.miners=0;options.startInHangar=true;
        Game game(art,loader,0,0,options,{},GameRole::Authority);int departed=game.state().shipId;advance(game,100);game.retireNetworkPilot(departed);advance(game,50);
        finish(game,1);require(game.state().endgame.players.size()==1 && row(game,departed).played>9.9f,"fallback Server seat appears as a human postgame row");
    }
    puts("Postgame membership: six directional/direct/No Team score resets, retained deaths/ejections, new command credit, reconnect/retirement clocks, immutable late resume and two placeholder cases passed");
}
int main(int argc,char** argv)try {
    require(argc>1,"ARTWORK argument required");scoreTransitions(argv[1]);Assets assets(argv[1]);Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.setPlaying(true);
    GameState state;state.complete=true;state.playerSide=0;state.result="Green won <&>";state.endgame.frozen=true;state.endgame.winner=0;state.endgame.pods=true;
    for(int i=0;i<2;++i){Net::ResultTeam team;team.id=i;team.civilization=25;team.kills=i?9:3;Net::text(team.name,i?"Red":"Green");team.color[0]=i?1:0;team.color[1]=i?0:1;state.endgame.teams.push_back(team);}
    for(int i=0;i<1024;++i){Net::ResultPlayer p;p.id=i;p.side=i%2;p.civilization=25;Net::text(p.name,"Pilot "+std::to_string(i));p.score=i+.5f;p.played=3661;p.ejections=2;p.deaths=3;p.flags=i%2?0:3;state.endgame.players.push_back(p);}
    auto click=[&](MenuOp op,int value=0,int delta=-999){auto& items=ui.view().items;auto i=std::find_if(items.begin(),items.end(),[&](auto& i){return i.enabled && i.action.op==op && i.action.value==value && (delta==-999 || i.action.delta==delta);});if(i==items.end())throw std::runtime_error("Missing results action "+std::to_string(int(op))+"/"+std::to_string(value)+"/"+std::to_string(delta));return ui.activate(size_t(i-items.begin()));};
    auto control=[&](std::string name)->const OriginalControl&{auto& art=ui.view().artwork;auto i=std::find_if(art.begin(),art.end(),[&](auto& c){return c.name==name;});require(i!=art.end(),"original results control exists");return *i;};
    ui.open(Screen::Results,&state);require(ui.view().original && ui.view().canvas==glm::ivec2(800,600),"results use original authored canvas");
    require(control("textTitle").text=="You are victorious!" && control("textReason").text==state.result,"typed winner chooses original title and reason");
    require(control("resultPlayer/1023/32").text=="Pilot 1023" && control("resultTeam/0/55").text=="Green","original default sorts: player score descending, team kills ascending");
    require(control("resultPlayer/1023/491").text=="2" && control("resultPlayer/1023/543").text=="1:01" && control("resultPlayer/1023/543").color==glm::vec4(1,0,0,1),"original ejections, H:MM and uncounted-time color");
    auto visibleRows=[&]{return std::count_if(ui.view().items.begin(),ui.view().items.end(),[](auto& i){return i.action.op==MenuOp::ResultsSelect && i.action.delta==0;});};
    require(visibleRows()==12,"1024-pilot table paints only visible rows");
    auto artworkCount=ui.view().artwork.size(),itemCount=ui.view().items.size();auto larger=state;
    for(int i=1024;i<2048;++i){auto p=state.endgame.players[i%1024];p.id=i;p.score=i+.5f;larger.endgame.players.push_back(p);}ui.sync(larger);
    require(visibleRows()==12 && ui.view().artwork.size()==artworkCount && ui.view().items.size()==itemCount,"doubling the player table creates offscreen artwork or hit regions");ui.sync(state);
    printf("Postgame visible table: 12 rows, %zu authored controls and %zu hit regions at both 1024 and 2048 pilots\n",artworkCount,itemCount);
    auto dragPlayers=[&](bool bottom){auto& items=ui.view().items;auto thumb=std::find_if(items.begin(),items.end(),[](auto& i){return i.action.op==MenuOp::ScrollThumb && i.action.value==-3;});require(thumb!=items.end(),"results player scrollbar exists");auto point=glm::vec2(thumb->bounds)+glm::vec2(thumb->bounds.z,thumb->bounds.w)*.5f;auto index=size_t(thumb-items.begin());ui.pointerMove(point);ui.pointerPress(index);ui.pointerMove({point.x,bottom?10000.f:-10000.f});ui.pointerRelease({});};
    click(MenuOp::ResultsSelect,1023);require(ui.wantsText(),"original player selection retains chat focus");
    ui.key(SDLK_END,SDL_SCANCODE_END);require(control("resultPlayer/1023/32").text=="Pilot 1023","focused chat does not route End to the player list");
    ui.text("hello");require(ui.chatText()=="hello","chat remains editable after player selection");for(int i=0;i<5;++i)ui.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);
    dragPlayers(true);require(control("resultPlayer/0/32").text=="Pilot 0","authored thumb reaches last player row");
    click(MenuOp::ResultsSort,5);require(control("resultPlayer/1023/32").text=="Pilot 1023","sort keeps pixel scroll position");
    dragPlayers(false);require(control("resultPlayer/0/32").text=="Pilot 0","sort reverses numeric ordering");
    click(MenuOp::ResultsSort,0);require(control("resultPlayer/1023/32").text=="Pilot 1023","position header reverses current sort");
    click(MenuOp::ResultsSort,6,1);require(control("resultTeam/1/55").text=="Red","team header reverses its original default sort");
    for(int i=0;i<20;++i)state.messages.push_back({double(i),"A long result chat message with words and padding: "+std::string(140,'x'),false,uint64_t(i+1),{},0,0,glm::vec4(1),true});
    ui.sync(state);require(std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](auto& c){return c.name.starts_with("resultChat/") && c.text.starts_with("  ");}),"postgame chat wraps and indents continuation lines with original font widths");
    state.messages.push_back({20,"Commander: round complete",false,21,{},0,0,glm::vec4(1),true,true});ui.sync(state);
    require(std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](auto& c){return c.name.starts_with("resultChat/") && c.font=="smallBoldFont" && c.text.find("Commander")!=c.text.npos;}),"postgame commander uses the original bold chat font");
    click(MenuOp::ComposeChat);ui.text("admin: hello");auto sent=ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);require(sent.effect==FrontEffect::SendChat && sent.value==int(ChatChannel::Admin) && ui.outgoingChatText()==" hello","original admin prefix routing");ui.chatSent();
    ui.text("!echo hello");sent=ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);require(sent.value==int(ChatChannel::Everyone) && ui.outgoingChatText()=="!echo hello","shell input retains original text and routing");ui.chatSent();
    require(click(MenuOp::SaveStats).effect==FrontEffect::SaveStats,"original Save Stats button exports");
    state.networked=state.networkLobby=true;state.complete=false;ui.open(Screen::Results,&state);require(click(MenuOp::ResultsBack).effect==FrontEffect::None && ui.screen()==Screen::RemoteLobby,"postgame Back retains the network connection and returns to Team Lobby");
    ui.open(Screen::Results,&state);state.networkCountdown=35;state.lobbyFlags=Allegiance::LobbyAutoRestart;ui.sync(state);
    require(ui.screen()==Screen::Results && control("textCountdown").text=="35","auto-restart delay must retain scores and the original timer before briefing without applying its rounding lag twice");
    state.missionStage=STAGE_STARTING;state.networkCountdown=5;ui.sync(state);require(ui.screen()==Screen::RemoteLobby,"an on-team pilot returns from results to Team Lobby when briefing starts");
    state.playerSide=noTeam;ui.open(Screen::Results,&state);ui.sync(state);require(ui.screen()==Screen::Results && control("textCountdown").text=="5","No Team must retain original postgame countdown without receiving a briefing");
    state.missionStage=STAGE_OVER;state.networkCountdown=-1;ui.sync(state);require(control("textCountdown").text.empty() && click(MenuOp::ResultsBack).effect==FrontEffect::Leave,"ended/canceled mission Back does not leave or retains a stale restart timer");
    state.playerSide=0;state.missionStage=STAGE_NOTSTARTED;state.lobbyFlags=0;
    state.complete=false;state.endgame={};ui.sync(state);require(ui.results().frozen,"previous report survives next-round lobby");state.complete=true;state.endgame.frozen=true;state.endgame.winner=1;ui.sync(state);require(ui.results().winner==1 && ui.results().players.empty(),"a later round replaces the cached previous report");
    ui.setPlaying(false);require(!ui.results().frozen,"leaving the match clears unrelated previous statistics");
    SessionOptions options;options.scenario=Scenario::Conquest;options.enemies=0;options.rules.miners=0;options.rules.teamKills=1;options.startInHangar=true;options.pilot="Alpha <&> '";
    Game game(argv[1],[](auto&,auto&){return ModelHandle{1,1};},0,0,options,EffectLoader{},GameRole::Authority);int self=game.state().shipId,assist=game.addLocalPilot("Assistant",0),enemy=game.addLocalPilot("Victim",1),retired=game.addLocalPilot("Retired",0);require(assist>=0 && enemy>=0 && retired>=0,"scoring pilot fixture");
    for(int i=0;i<1900;++i)game.step({},.1);require(!game.state().complete && game.removeLocalPilot(retired),"departed player score fixture");
    require(game.controlLocalPilot(enemy),"control scoring victim");game.launch();require(game.controlLocalPilot(self),"restore scorer");
    auto* victim=game.core()->GetShip(enemy);auto now=game.core()->GetLastUpdate();victim->GetDamageTrack()->ApplyDamage(now,game.core()->GetShip(self),60);victim->GetDamageTrack()->ApplyDamage(now,game.core()->GetShip(assist),40);
    game.core()->GetIgcSite()->KillShipEvent(now,victim,game.core()->GetShip(assist),100,victim->GetPosition(),Vector::GetZero());game.step({},.01);
    auto frozen=game.state().endgame;require(frozen.frozen && frozen.winner==0 && frozen.players.size()==4 && frozen.teams.size()==2,"actual victory freezes current and departed pilot/team results");
    auto player=[&](int id)->const Allegiance::EndgamePlayer&{auto i=std::find_if(frozen.players.begin(),frozen.players.end(),[&](auto& p){return p.id==id;});require(i!=frozen.players.end(),"scored pilot exists");return *i;};
    require(player(self).kills==1 && player(assist).kills==0 && player(assist).assists==1 && player(enemy).ejections==1,"largest damage bucket gets kill, second gets assist, victim gets ejection");
    float points=game.core()->GetFloatConstant(c_fcidPointsPlayer);require(std::abs(player(self).score-points*1.2f)<.01f && std::abs(player(assist).score-points*.8f)<.01f,"original fractional scoring and win bonus are used");
    require((player(self).flags&3)==3 && (player(enemy).flags&2) && player(retired).played>180,"original command credit and eligible win/loss time include departed pilots");
    for(int i=0;i<100;++i)game.step({},.1);require(game.state().endgame==frozen,"completed report remains frozen");
    auto file=std::filesystem::temp_directory_path()/"allegiance-endgame-check.xml";game.saveEndgame(file,frozen,"Green <wins> & yes","Local & server");std::ifstream in(file);std::string xml((std::istreambuf_iterator<char>(in)),{});in.close();std::filesystem::remove(file);
    require(xml.find("Alpha &lt;&amp;&gt; &apos;")!=xml.npos && xml.find("<Core>static_core</Core>")!=xml.npos && xml.find("<GameOverReason>Green &lt;wins&gt; &amp; yes</GameOverReason>")!=xml.npos && xml.find("<IsCommander>true</IsCommander>")!=xml.npos,"original XML schema exports actual core, escaped names/reason and command credit");
    Net::Frame report;report.header.kind=Net::Kind::Snapshot;report.resultPlayers=frozen.players;report.resultTeams=frozen.teams;auto wire=Net::packet(report);require(Net::frame(wire).resultPlayers==report.resultPlayers,"raw public result records roundtrip");Net::ArenaEncoder encoder;
    for(int recipient=0;recipient<1024;++recipient)encoder.publishWorld(0x100001,1,1,report);require(encoder.encodes()==1,"frozen public report encodes once for 1024 recipients");
    std::reverse(report.resultPlayers.begin(),report.resultPlayers.end());auto changed=Net::packet(report);require(Net::expandedAgainst(Net::compressed(changed,1,wire),wire)==changed,"result relocation deltas survive record reordering");
    game.resetAuthority(options);auto previous=game.networkReport(game.state().shipId,1,0);
    auto arena=[](const Net::ReportCapture& r){auto i=std::find_if(r.worlds.begin(),r.worlds.end(),[](auto& world){return world.domain==0x100001;});require(i!=r.worlds.end(),"result arena exists");return i->frame;};
    auto old=arena(previous);require(old->header.tick==1 && old->resultPlayers==frozen.players,"previous round survives authority replacement");
    auto* base=game.core()->GetSide(0)->GetStations()->first()->data();char originalTip[512];snprintf(originalTip,sizeof(originalTip),START_COLOR_STRING "%s's %s" END_COLOR_STRING " in %s was destroyed.",(PCC)ConvertColorToString(base->GetSide()->GetColor()),base->GetSide()->GetName(),base->GetName(),base->GetCluster()->GetName());game.core()->GetIgcSite()->KillStationEvent(base,nullptr,1);
    auto destroyedReport=game.networkFrame(game.state().shipId,2,0);
    require(std::any_of(destroyedReport.notices.begin(),destroyedReport.notices.end(),[&](auto& n){return n.text==std::string(originalTip) && (n.flags&(Net::NoticeTip|Net::NoticeWarning))==(Net::NoticeTip|Net::NoticeWarning);}),"station destruction loses original unknown-killer wording, owner color or sector");
    game.step({},.1);auto current=arena(game.networkReport(game.state().shipId,2,0));
    require(current->header.tick>old->header.tick && game.state().endgame.winner==1,"current result gets a new immutable arena revision even while next-round reset is delayed");
    Net::ArenaEncoder revisions;revisions.publishWorld(0x100001,old->header.tick,1,*old);revisions.publishWorld(0x100001,current->header.tick,1,*current);require(revisions.encodes()==2,"prior and current reports cannot reuse stale encoded results within one match");
    for(bool paused:{false,true})for(bool allowed:{false,true})require(Allegiance::MissionCanRestart(paused,allowed)==(!paused && allowed),"original restart permission/shutdown-pause policy");
    for(double previous:{0.,14.,15.,16.,29.,30.,31.,59.,60.,61.})for(double remaining:{-1.,0.,1.,14.,15.,16.,29.,30.,31.,60.})for(bool empty:{false,true})for(bool ready:{false,true}){
        auto expected=Allegiance::AutomaticRestart::Wait;
        if(remaining<=15){if(empty || ready)expected=Allegiance::AutomaticRestart::Briefing;else if(remaining<=0)expected=Allegiance::AutomaticRestart::Cancel;}
        else if(remaining<=30 && previous>30 && !empty && !ready)expected=Allegiance::AutomaticRestart::Warn;
        require(Allegiance::MissionAutomaticRestart(remaining,previous,15,empty,ready)==expected,"FedSrv warning/briefing/cancellation boundaries differ");
    }
    puts("Original mission lifecycle: 400 deadline/readiness combinations, restart/shutdown-pause permission, score delay, No Team countdown, briefing transition and ended-mission Back passed");
    puts("Original postgame artwork/tables, clipping, numeric sorts, scrolling/chat, subsequent rounds and XML export; real IGC damage-share scoring, departed pilots, commander credit, win/loss timers and shared frozen reports passed");return 0;
}catch(const std::exception& e){fprintf(stderr,"Endgame check: %s\n",e.what());return 1;}
