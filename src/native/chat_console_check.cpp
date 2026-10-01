#include "chat_console.hpp"
#include "console_pane.hpp"
#include "test_fixture.hpp"
#include "network_records.hpp"
#include "command_input.hpp"
#include "../ClientCore/MissionChatCommands.h"
#include <limits>
using namespace Native;
using NativeTest::require;
using Allegiance::ComposeState;
namespace {
void panelAnchors() {
    // Independently specified original Side* screen positions. Canonical menus
    // remain centered; only their attached console layer follows a screen edge.
    constexpr glm::vec2 anchors[]={{0,.5f},{0,0},{.5f,0},{1,0},{1,.5f},{1,1},{.5f,1},{0,1},{.5f,.5f}};
    for(auto size:{glm::ivec2(1920,1080),glm::ivec2(2560,1440),glm::ivec2(2561,1441)})for(int scale:{1,2})for(auto anchor:anchors){
        MenuView view;view.original=true;view.classicScale=scale;HudOverlay overlay;overlay.anchor=anchor;overlay.on=glm::vec2(view.canvas)*anchor+glm::vec2(-20,30);overlay.off=overlay.on+glm::vec2(120,-80);overlay.duration=.75;
        view.slide=overlaySlide(overlay,10);view.items.push_back({{overlay.on,20,10},{},{},{MenuOp::Resume}});
        auto layout=originalMenuLayout(size,scale,view.canvas);auto final=layout.pixel(overlay.on+menuSlideOffset(view,size,10.75));
        require(final==glm::floor(glm::vec2(size)/layout.scale*anchor+glm::vec2(-20,30))*layout.scale,"screen anchor does not preserve authored pixel placement");
        auto half=menuSlideOffset(view,size,10.375)-menuSlideOffset(view,size,10.75);require(half==glm::vec2(60,-40),"authored panel duration/entrance differs");
        view.slide->closing=true;require(menuSlideOffset(view,size,10.375)-menuSlideOffset(view,size,10)==half,"authored exit differs");
        view.slide->duration=0;view.slide->closing=false;view.slide->started=uiTime()-1;
        auto hitPixel=layout.pixel(overlay.on+menuSlideOffset(view,size,uiTime())+glm::vec2(2));
        if(hitPixel.x>=0 && hitPixel.y>=0 && hitPixel.x<size.x && hitPixel.y<size.y){require(menuHit(view,size,1,hitPixel).has_value(),"authored slide rendering and hits use different anchors");view.slide->undetectable=true;require(!menuHit(view,size,1,hitPixel),"undetectable overlay captures pointer input");}
    }
    puts("Original panel anchors: all nine sides, 1x/2x widescreen placement, authored durations, zero duration and mouse transparency passed");
}
void notifications(const std::filesystem::path& art) {
    Assets assets(art);ConsolePane pane(assets);GameState state;state.playerSide=0;state.sideColors[0]={0,.75f,.25f,1};
    state.commands[0]={5,1,0,"goto Garrison"};state.commands[1]={3,0,7,"defend Scout"};
    uint64_t serial=0;
    auto post=[&](std::string text,bool critical,double now){GameMessage m{};m.serial=++serial;m.text=std::move(text);m.warning=critical;m.tip=true;m.displayTime=now;state.messages.push_back(std::move(m));};
    auto control=[](const MenuView& view,std::string_view name)->const OriginalControl&{auto i=std::find_if(view.artwork.begin(),view.artwork.end(),[&](const auto& c){return c.name==name;});require(i!=view.artwork.end(),"authored console control missing");return *i;};
    auto view=[&](double now,int scale=1,bool loadout=false)->const MenuView&{return pane.view({2560,1440},scale,state,7,loadout,true,now);};
    const auto& empty=view(1);auto& queued=control(empty,"QueuedOrder");auto& accepted=control(empty,"AcceptedOrder");
    require(empty.original && empty.overlay && queued.bounds.x==57 && queued.bounds.y==57 && accepted.bounds.x==57 && accepted.bounds.y==77 && queued.font=="medVerdana" && queued.additive && accepted.additive,"original order anchors/font/additive blend");
    require(queued.text=="goto Garrison" && accepted.text=="defend Scout" && accepted.color==glm::vec4(.586f,.578f,.680f,1),"original accepted/queued command text and colors");
    require(std::count_if(empty.artwork.begin(),empty.artwork.end(),[](const auto& c){return c.image && c.additive && c.image->name.find("com")!=std::string::npos;})==2,"original comqueued/comaccept artwork missing");
    post("First critical",true,1);post("Second critical",true,2);post("Third critical",true,3);post("Fourth critical",true,4);post("Fifth critical",true,5);
    const auto& stacked=view(5);
    require(control(stacked,"CriticalTipText0").text=="Fifth critical" && control(stacked,"CriticalTipText3").text=="Second critical" && control(stacked,"CriticalTipText0").bounds.y==134 && control(stacked,"CriticalTipText3").bounds.y==92,"four original critical lines scroll upward below chat");
    auto* cached=stacked.artwork.data();require(view(5.1).artwork.data()==cached,"unchanged console reconstructs its controls every frame");
    require(control(view(9.5),"CriticalTipText3").text.empty() && control(view(9.5),"CriticalTipText0").text=="Fifth critical","critical tips expire independently at 7.5 seconds on display time");
    require(control(view(12.5),"CriticalTipText0").text.empty(),"paused simulation retains expired critical tip");
    post("First ordinary",false,6);post("Second ordinary",false,6);post("Third ordinary",false,6);post("Fourth ordinary",false,6);
    const auto& ordinary=view(100);require(control(ordinary,"TipText").text=="Fourth ordinary" && control(ordinary,"TipText1").text=="Third ordinary" && control(ordinary,"TipText2").text=="Second ordinary","three ordinary tips remain until replaced");
    require(control(ordinary,"TipText").bounds.y==1426 && control(ordinary,"TipText1").bounds.y==1415 && control(ordinary,"TipText2").bounds.y==1406 && control(ordinary,"TipText").color==state.sideColors[0],"original bottom anchors and faction tint");
    state.messages.clear();require(control(view(101),"TipText").text=="Fourth ordinary","clearing chat also clears independent console tips");
    post(std::string(200,'W'),true,102);const auto& wrapped=view(102);
    require(control(wrapped,"CriticalTipText0").text.empty() && control(wrapped,"CriticalTipText1").text==std::string(200,'W'),"wrapped critical tip does not reserve its original blank line");
    const auto& doubled=view(102,2,true);require(doubled.classicScale==2 && doubled.canvas==glm::ivec2(1280,720) && control(doubled,"CriticalTipText0").bounds.y==122 && control(doubled,"TipText").bounds.y==706,"integer scaling/loadout chat rows change authored anchors incorrectly");
    pane.reset();state.messages.clear();state.trainingMission=2;
    post("Training instruction",true,103);post("Replacement instruction",true,104);
    require(control(view(104),"CriticalTipText0").text=="Replacement instruction" && control(view(104),"CriticalTipText1").text.empty(),"training critical text scrolls instead of replacing");
    require(control(view(133.9),"CriticalTipText0").text=="Replacement instruction" && control(view(134),"CriticalTipText0").text.empty(),"training tips do not retain the original 30-second lifetime");
    post("Uncensored text",false,135);view(135);state.messages.back().displayRevision=2;state.messages.back().displayText="Censored text";
    require(control(view(136),"TipText").text=="Censored text","display preference changes do not refresh retained tips");
    require(pane.view({2560,1440},1,state,7,false,false,136).artwork.empty(),"console shown outside a playable session");
    state.ballot={2,0,166,"Veteran has proposed resigning.  Press [Y] to vote yes, [N] to vote no."};
    auto& vote=control(view(136),"CurrentVote");
    require(vote.text==state.ballot.text && vote.font=="medBoldVerdana" && vote.justify==1 && vote.bounds.x==970 && vote.bounds.y==169 && vote.bounds.z==620 && vote.color==glm::vec4(1),"authored white centered ballot font/620-pixel chat-relative anchor");
    cached=view(136).artwork.data();require(view(136.1).artwork.data()==cached,"ballot reconstructs MDL/layout every frame");
    require(control(view(136,2,true),"CurrentVote").bounds==glm::vec4(330,157,620,600),"ballot integer scaling/loadout anchor differs");
    state.ballot={};require(control(view(137),"CurrentVote").text.empty(),"removed ballot still visible");
    puts("Authored order artwork/anchors, independent critical/ordinary tips, wrapping, training expiry, caching and integer scaling passed");
}
void ballotRules() {
    using namespace Allegiance;
    require(BallotCommand("#ReSiGn suffix")==BallotKind::Resign && BallotCommand("#DRAW")==BallotKind::OfferDraw &&
        BallotCommand("#mutinyanything")==BallotKind::Mutiny && BallotCommand(" #draw")==BallotKind::None && BallotCommand("#dra")==BallotKind::None,"FedSrv command prefix/case behavior");
    // Independent exhaustive majority oracle: abstain/no/yes for two voters
    // on each of three sides. FedSrv requires yes > no on every occupied side.
    for(int pattern=0;pattern<729;++pattern){
        Ballot b;b.kind=BallotKind::AcceptDraw;b.expires=30;for(int id=0;id<6;++id)b.voters.push_back({id,id/2});b.prepare(4);
        int digits=pattern;std::array<int,3> yes{},no{};int abstaining=0;
        for(int id=0;id<6;++id){int vote=digits%3;digits/=3;
            if(vote){require(b.cast(id,id/2,vote==2),"eligible ballot vote rejected");++(vote==2?yes:no)[id/2];require(!b.cast(id,id/2,true),"duplicate ballot counted twice");}
            else ++abstaining;
        }
        bool pass=true;for(int side=0;side<3;++side)pass&=yes[side]>no[side];
        require(b.passed()==pass && b.remaining==abstaining && b.ready(30)==(abstaining==0) && b.ready(30.001),"original per-side majority/strict expiry/all-votes rules differ");
    }
    Ballot b;b.kind=BallotKind::Resign;b.name="Veteran";b.voters={{7,0},{3,0},{9,0},{11,0}};b.prepare(3);
    require(b.cast(7,0,true) && b.cast(3,0,false) && !b.cast(9,1,true) && !b.cast(22,0,true),"foreign, switched-side or unknown voter accepted");
    require(b.result([](int){return "Blue";})=="Veteran's proposal to resign has been defeated.  ( 1 for 1 against 2 abstained)","original tied tally text differs");
    require(BallotInstructions(b.prompt())=="Veteran has proposed resigning.  Press [Y] to vote yes, [N] to vote no.","original ballot instruction text differs");
    b.kind=BallotKind::AcceptDraw;b.name="Blue";require(b.cast(9,0,true),"eligible voter lost after rejected defection");
    require(b.result([](int){return "Red";})=="Blue's offer of a draw has passed.  (Red: 2 for 1 against 1 abstained)","original multi-side tally names/order differ");
    puts("Ballot rules: 729 independent three-side vote combinations, prefix commands, duplicate/foreign votes, strict expiry and exact original text passed");
}
void ballots(const std::filesystem::path& art) {
    SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.rules.miners=1;options.rules.teams=3;options.rules.startingMoney=9;
    auto make=[&]{return std::make_unique<Game>(art,[](auto&,auto&){return ModelHandle{1,1};},0,0,options);};
    auto game=make();int owner=game->state().shipId,proposer=game->addLocalPilot("Veteran",0),support=game->addLocalPilot("Support",0),abstain=game->addLocalPilot("Abstain",0);
    int red=game->addLocalPilot("Red voter",1),green=game->addLocalPilot("Green voter",2);
    require(proposer>=0 && support>=0 && abstain>=0 && red>=0 && green>=0,"ballot human fixtures");
    auto seat=[&](int id){require(game->controlLocalPilot(id),"ballot seat selection");};
    seat(proposer);require(game->sendChat(ChatChannel::Everyone,"#RESIGN") && game->state().ballot.id<0,"proposer must auto-vote yes and hide own ballot");
    require(game->sendChat(ChatChannel::Team,"#draw"),"second ballot kind can coexist");
    seat(owner);require(game->state().ballot.text=="Veteran has proposed resigning.  Press [Y] to vote yes, [N] to vote no.","FIFO original ballot prompt");
    require(game->vote(false) && game->state().ballot.text.starts_with("Veteran has proposed offering a draw."),"vote immediately advances to next pending ballot");
    require(game->vote(true) && game->state().ballot.id<0 && !game->vote(true),"voted ballot repeats or empty vote succeeds");
    seat(support);require(game->vote(false) && game->vote(true),"second pilot queued votes");
    seat(red);require(game->state().ballot.id<0 && game->sendChat(ChatChannel::Team,"#draw") && game->state().messages.back().text.ends_with("Draw vote already in progress..."),"ballot type uniqueness must be global, duplicate feedback private");
    seat(abstain);require(game->state().ballot.text.starts_with("Veteran has proposed resigning."),"abstaining pilot prompt lost");
    FlightInput neutral;neutral.active=false;for(int i=0;i<301;++i)game->step(neutral,.1);
    require(game->state().ballot.id<0 && !game->state().complete,"expired prompt or defeated resign ends game");
    require(std::any_of(game->state().messages.begin(),game->state().messages.end(),[](auto& m){return m.text.ends_with("Veteran's proposal to resign has been defeated.  ( 1 for 2 against 1 abstained)");}),"resign result counts drones or changes abstention text");
    seat(red);require(game->state().ballot.text.find("has offered a draw.")!=std::string::npos,"opposing team did not receive accepted draw offer");
    int late=game->addLocalPilot("Late arrival",1);require(late>=0,"late voter fixture");seat(late);require(game->state().ballot.id<0 && !game->vote(true),"late joiner became eligible for an existing ballot");
    seat(red);require(game->vote(true),"first opposing faction accepts draw");game->step(neutral,.01);require(!game->state().complete,"draw passed without every participating faction");
    seat(green);require(game->vote(true),"second opposing faction accepts draw");game->step(neutral,.01);
    require(game->state().complete && game->state().endgame.winner==NA && game->state().result=="The game was declared a draw" && game->state().ballot.id<0,"accepted draw does not freeze original no-winner result/clear prompt");
    game=make();owner=game->state().shipId;proposer=game->addLocalPilot("New commander",0);support=game->addLocalPilot("Support",0);
    require(game->sendChat(ChatChannel::Team,"#mutiny") && game->state().ballot.id<0,"commander can initiate a mutiny");
    seat(proposer);require(game->setAutoDonate(owner) && game->sendChat(ChatChannel::Team,"#mutiny"),"mutiny proposer");
    seat(owner);require(game->state().ballot.id<0,"commander sees hidden mutiny instead of automatically voting no");
    int money=game->pilotBalance(owner)+game->pilotBalance(proposer);
    seat(support);require(game->setAutoDonate(owner) && game->vote(true),"mutiny support");game->step(neutral,.01);
    require(game->teamLeader(0)==proposer && game->pilotBalance(owner)==0 && game->pilotBalance(proposer)==money,"passed mutiny did not transfer command and funds");
    require(game->core()->GetShip(ShipID(owner))->GetAutoDonate()==game->core()->GetShip(ShipID(proposer)) && game->playerShip()->GetAutoDonate()==game->core()->GetShip(ShipID(proposer)) && !game->core()->GetShip(ShipID(proposer))->GetAutoDonate(),"mutiny donation cascade differs from FedSrv");
    const auto& messages=game->state().messages;
    auto tally=std::find_if(messages.begin(),messages.end(),[](auto& m){return m.text.ends_with("New commander's proposal to mutiny has passed.  ( 2 for 1 against)");});
    auto leader=std::find_if(messages.begin(),messages.end(),[](auto& m){return m.text=="New commander has become the team leader";});
    require(tally!=messages.end() && leader!=messages.end() && tally<leader,"original mutiny tally is missing or follows its leadership feedback");
    game=make();owner=game->state().shipId;proposer=game->addLocalPilot("Resigner",0);seat(proposer);game->sendChat(ChatChannel::Team,"#resign");seat(owner);require(game->vote(true),"resign yes");game->step(neutral,.01);
    require(!game->core()->GetSide(0)->GetActiveF() && !game->state().complete && game->state().dead,"resign did not eliminate just its side in a three-team match");
    puts("IGC ballots: FIFO/auto-votes, original prompts/tallies, global duplicate feedback, drone/late-join exclusion, expiry, three-team draw, mutiny funds/donation cascade and resignation passed");
}
void missionChat(const std::filesystem::path& art) {
    using namespace Allegiance;using K=MissionChatKind;
    MissionChatContext owner{true,true,true,true};
    for(auto [text,kind,value]:{std::tuple{"#AUTOACCEPT suffix",K::AutoAccept,-1},
        std::tuple{"#autobalance 1suffix",K::AutoBalance,1},std::tuple{"#autobalance 2",K::AutoBalance,2},
        std::tuple{"#autobalance A",K::AutoBalance,32766},std::tuple{"#autobalance N",K::AutoBalance,32767},
        std::tuple{"#autobalance x",K::AutoBalance,-1},std::tuple{"#autobalance  1",K::AutoBalance,-1},
        std::tuple{"#autobalance",K::None,-1},std::tuple{"#autobalance1",K::None,-1},
        std::tuple{"#LiStSuffix",K::List,-1},std::tuple{" #list",K::None,-1},std::tuple{"#ban ",K::Ban,-1},std::tuple{"#ban",K::None,-1}}){
        auto parsed=ParseMissionChat(text,owner);require(parsed.kind==kind && parsed.imbalance==value,"FedSrv chat command casing/prefix/spacing/length rules");
    }
    require(ParseMissionChat("#autoaccept",{false,false,true,true}).kind==K::None &&
        ParseMissionChat("#autobalance n",{false,true,true,true}).kind==K::None &&
        ParseMissionChat("#list",{true,true,true,false}).kind==K::None &&
        ParseMissionChat("#list",{false,false,false,true}).kind==K::None &&
        ParseMissionChat("#ban Waiting",{true,true,true,false}).target=="Waiting","original role/stage/No Team/owner lobby-boot gates");
    require(ChatNameEqual("Waiting Pilot","wAiTiNg pIlOt") && !ChatNameEqual("Waiting Pilot ","Waiting Pilot"),"original exact case-insensitive ban name matching");
    require(ValidTeamImbalance(1) && ValidTeamImbalance(2) && ValidTeamImbalance(32766) && ValidTeamImbalance(32767) &&
        !ValidTeamImbalance(0) && !ValidTeamImbalance(3) && !ValidTeamImbalance(32768),"live rule record accepted unsupported values");
    SessionOptions options;options.scenario=Scenario::Conquest;options.enemies=0;options.startInHangar=true;options.rules.miners=0;options.rules.startingMoney=9;
    Game game(art,[](auto&,auto&){return ModelHandle{1,1};},0,0,options);Assets assets(art);Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.networkSession(game.sessionOptions());ui.setPlaying(true);ui.open(Screen::Hangar,&game.state());
    int original=game.state().shipId,friendID=game.addLocalPilot("Waiting Pilot",0),other=game.addLocalPilot("Other Commander",1);
    require(friendID>=0 && other>=0 && game.state().autoAccept==0x3f,"original standalone automatic-admission default");
    for(auto text:{"#draw","#list","#autobalance n","#autoaccept"}){
        auto before=game.state().messages.size();game.sendChat(ChatChannel::Target,text,original);
        require(game.state().messages.size()==before+1 && game.state().messages.back().text.ends_with(text) && game.state().messages.back().chatTarget==CHAT_INDIVIDUAL && game.state().ballot.id<0 && game.state().autoAccept==0x3f && game.state().maxImbalance==1,"individual self chat invokes server commands or loses its sole local echo");
    }
    game.sendChat(ChatChannel::Team,"#LiStSuffix");
    require(game.state().messages.back().text.ends_with(MissionChatHelp) && !game.state().messages.back().fromPlayer && game.state().messages.back().chatTarget==CHAT_INDIVIDUAL_NOFILTER &&
        game.state().messages[game.state().messages.size()-2].text.ends_with("#LiStSuffix"),"original locally echoed list command and private HQ help");
    game.sendChat(ChatChannel::Team,"#autoaccept");require(!(game.state().autoAccept&1),"standalone commander autoaccept toggle");
    require(game.controlLocalPilot(friendID),"noncommander chat fixture");game.sendChat(ChatChannel::Everyone,"#autoaccept");require(!(game.state().autoAccept&1),"noncommander changed automatic admission");
    require(game.controlLocalPilot(other),"opposing commander chat fixture");game.sendChat(ChatChannel::Team,"#autoaccept");require(game.state().autoAccept==0x3c,"commander changed another faction's automatic admission");
    game.sendChat(ChatChannel::Everyone,"#autobalance n");require(game.core()->GetMissionParams()->iMaxImbalance==1,"non-owner changed balance policy");
    require(game.controlLocalPilot(original),"owner chat fixture");
    for(auto [text,value]:{std::pair{"#autobalance A",32766},std::pair{"#autobalance n",32767},std::pair{"#autobalance 2extra",2},std::pair{"#autobalance 1",1}}){
        auto* ship=game.playerShip();auto money=game.state().money;game.sendChat(ChatChannel::Team,text);ui.sync(game.state());
        require(game.core()->GetMissionParams()->iMaxImbalance==value && game.sessionOptions().rules.maxImbalance==value && ui.session().rules.maxImbalance==value && game.playerShip()==ship && game.state().money==money && game.state().messages.back().chatTarget==CHAT_TEAM,"live IGC/options/frontend balance or original author audience changed without continuity");
    }
    game.sendChat(ChatChannel::Team,"#autobalance x");require(game.state().maxImbalance==1,"invalid autobalance parameter changes policy");
    game.sendChat(ChatChannel::Team,"#ban Waiting Pilot");require(game.core()->GetShip(ShipID(friendID)),"owner ban removes an active faction member");
    require(game.moveNetworkPilotToTeam(friendID,SIDE_TEAMLOBBY),"standalone No Team boot fixture");
    game.sendChat(ChatChannel::Team,"#ban Waiting Pilot ");require(game.core()->GetShip(ShipID(friendID)),"ban incorrectly trims its target name");
    game.sendChat(ChatChannel::Team,"#BaN wAiTiNg PiLoT");require(!game.core()->GetShip(ShipID(friendID)) && game.state().shipId==original,"owner did not retire the unassigned pilot safely");
    game.core()->SetMissionStage(STAGE_NOTSTARTED);game.sendChat(ChatChannel::Everyone,"#list");require(game.state().messages.back().fromPlayer && game.state().messages.back().text.ends_with("#list"),"prestart chat command should pass through as ordinary chat");
    puts("Original mission chat: role/stage/prefix/spacing rules, private help/local echo, standalone admission defaults, commander toggles, live IGC/frontend rules, retained ships/funds and exact No Team-only owner boot passed");
}
void composition(const std::filesystem::path& art) {
    Assets assets(art);QuickChatCatalog quick(assets);SessionOptions options;
    options.scenario=Scenario::Conquest;options.startInHangar=true;options.rules.miners=0;
    Game game(art,[](auto&,auto&){return ModelHandle{1,1};},0,0,options);game.quickChats(quick);
    ChatConsole console(assets,quick);console.sync(game);
    console.startChat(game);require(console.mode()==ComposeState::Chat && console.audience().channel==ChatChannel::Team,"original chat defaults to team");
    console.text(game,"hello");auto& view=console.view({2560,1440},1);
    require(view.original && view.overlay && view.canvas==glm::ivec2(2560,1440) && view.slide && view.slide->off==glm::vec2(-20,226),"inline composer uses original bottom anchor and half-second movement");
    require(std::count_if(view.artwork.begin(),view.artwork.end(),[](auto& c){return c.kind==OriginalControl::Kind::Text && c.font=="smallFont" && c.singleLine;})==2,"recipient and entry retain authored bitmap font and clipping");
    auto entry=std::find_if(view.artwork.begin(),view.artwork.end(),[](auto& c){return c.text=="hello|";});
    require(entry!=view.artwork.end() && entry->justify==3 && entry->bounds.z==154 && entry->bounds.w==17 && entry->bounds.x==1242,"original 154-pixel chat field clips its leading text without wrapping");
    console.key(game,SDLK_RETURN);require(!console.active() && game.state().messages.back().text.find("hello")!=std::string::npos,"Enter sends chat and returns keyboard ownership");
    console.startChat(game);console.key(game,SDLK_TAB);require(console.audience().channel==ChatChannel::Everyone,"team Tab advances to all");
    console.key(game,SDLK_TAB);require(console.audience().channel==ChatChannel::Wing,"all Tab advances to wing without allies");
    console.key(game,SDLK_ESCAPE);console.startChat(game);
    console.text(game,Game::wingName(3));console.key(game,SDLK_TAB);
    require(console.audience().channel==ChatChannel::Wing && console.audience().recipient==3 && console.typed().empty(),"Tab completes explicit wing and clears typed prefix");
    console.key(game,SDLK_ESCAPE);console.startChat(game);
    require(console.audience().recipient==3,"ordinary chat recipient survives leaving composition");
    console.text(game,"!");require(console.mode()==ComposeState::Shell,"leading exclamation starts original shell pane");
    console.key(game,SDLK_BACKSPACE);require(console.mode()==ComposeState::Chat && console.typed().empty(),"erasing shell prefix returns to chat");
    console.text(game,std::string(300,'x'));require(console.typed().size()==255,"original chat buffer bound");console.key(game,SDLK_ESCAPE);
    console.startChat(game);console.text(game,std::string(254,'x'));console.text(game,"é");require(console.typed().size()==254,"inline chat capacity splits a UTF-8 input character");console.key(game,SDLK_BACKSPACE);console.text(game,"é");require(console.typed().size()==255 && console.typed().ends_with("é"),"inline chat rejects a fitting UTF-8 character");console.key(game,SDLK_BACKSPACE);require(console.typed().size()==253,"inline chat Backspace leaves a continuation byte");console.key(game,SDLK_ESCAPE);
    game.launch();for(int i=0;i<32;++i)game.step({},.1);game.commandView(true);console.sync(game);
    const auto* homeCursor=game.core()->GetSide(0)->GetStations()->first()->data();
    require(game.commandCursor(OT_station,homeCursor->GetObjectID(),console.subjects(),false,0,game.state().shipId)=="goto","station cursor uses original individual default order");
    require(game.commandCursor(OT_station,homeCursor->GetObjectID(),console.subjects(),false,0,game.state().shipId,0).empty(),"left click only selects our ships");
    require(game.commandCursor(OT_station,homeCursor->GetObjectID(),console.subjects(),true,c_cidAttack,game.state().shipId).empty(),"illegal explicit command does not offer a hover target");
    DataShipIGC minerData{};minerData.shipID=game.core()->GenerateNewShipID();minerData.hullID=game.playerShip()->GetBaseHullType()->GetObjectID();
    minerData.sideID=0;minerData.pilotType=c_ptMiner;minerData.abmOrders=c_aabmMineHe3;minerData.baseObjectID=NA;minerData.partsOffset=sizeof(minerData);
    auto* miner=static_cast<IshipIGC*>(game.core()->CreateObject(game.core()->GetLastUpdate(),OT_ship,&minerData,sizeof(minerData)));require(miner,"cursor miner fixture");
    std::array<int,1> minerSubject{miner->GetObjectID()};bool mined=false;
    for(auto& c:game.contacts())if(c.type==OT_asteroid)if(auto* rock=game.core()->GetAsteroid(AsteroidID(c.id)))if(rock->HasCapability(c_aabmMineHe3)) {
        require(game.commandCursor(c.type,c.id,minerSubject,false,0,miner->GetObjectID())=="mine","miner hover uses IGC default order and capabilities");mined=true;break;
    }
    require(mined,"visible helium rock fixture");miner->Terminate();miner->Release();
    std::array<Contact,4> projected{};
    projected[0].position={0,0,.5f};projected[0].radius=.02f;projected[0].id=1;
    projected[1]=projected[0];projected[1].position.x=.2f;projected[1].radius=.3f;projected[1].id=2;
    projected[2]=projected[0];projected[2].position.x=.2f;projected[2].radius=.3f;projected[2].id=3;
    projected[3]=projected[0];projected[3].position.z=-.1f;projected[3].radius=100;projected[3].id=4;
    auto* picked=commandHover(projected,{59,50},{100,100},glm::mat4(1),[](auto&){return true;});
    require(picked && picked->id==3,"hover uses projected radius, nearest center, later tie and visible clip depth");
    picked=commandHover(projected,{50,50},{100,100},glm::mat4(1),[](auto& c){return c.id!=1 && c.id!=3;});
    require(picked && picked->id==2,"command legality filters closer candidates before choosing a target");
    console.verb(game,5);
    require(console.mode()==ComposeState::Command && console.audience().recipient==game.state().shipId,"command view starts with self independently of chat recipient");
    console.text(game,"me");require(!console.target(),"IGC forbids a self goto target");
    int other=game.addLocalPilot("Selected recipient",0);require(other>=0 && game.pickCommandUnit(other),"typed command recipient fixture");
    console.selectUnits(game);require(console.target() && console.target()->id==game.state().shipId && console.target()->name=="me","typed orders complete self when legal for another selected ship");
    console.key(game,SDLK_ESCAPE);require(console.mode()==ComposeState::Chat && console.typed().empty(),"command Escape first returns to chat");
    console.key(game,SDLK_ESCAPE);console.startChat(game);console.key(game,SDLK_A,true);
    require(console.mode()==ComposeState::Command && console.currentVerb()==1,"Ctrl plus verb initial converts empty chat into an order");
    console.key(game,SDLK_ESCAPE);console.key(game,SDLK_ESCAPE);console.recipient(game,0);console.text(game,"goto");console.key(game,SDLK_TAB);
    require(console.mode()==ComposeState::Command && console.currentVerb()==5 && console.typed().empty(),"exact verb Tab enters command composition");
    auto sector=std::find_if(game.state().sectors.begin(),game.state().sectors.end(),[](auto& s){return s.current;});require(sector!=game.state().sectors.end(),"current known cluster");
    console.text(game,sector->name);require(console.target() && console.target()->type==-2,"typed sector uses cluster buoy completion");
    console.key(game,SDLK_RETURN);require(!console.active(),"typed sector order ends composition even if IGC rejects an order to the current sector");
    auto* home=game.core()->GetSide(0)->GetStations()->first()->data();
    console.recipient(game,0);console.verb(game,5);console.text(game,home->GetName());console.key(game,SDLK_RETURN);
    require(!console.active() && game.playerShip()->GetCommandID(c_cmdAccepted)==c_cidGoto && game.playerShip()->GetCommandTarget(c_cmdAccepted)==home,"typed station order uses original self acceptance");
    console.wings(game);auto& wings=console.view({2560,1440},1);require(wings.items.size()==10,"original wing combo exposes all ten authored wings");
    auto from=glm::vec2(wings.items[1].bounds)+glm::vec2(2),p=glm::vec2(wings.items[4].bounds)+glm::vec2(2);console.pointer(game,{2560,1440},from,true);console.hover({2560,1440},p);console.pointer(game,{2560,1440},p,false);
    require(console.audience().channel==ChatChannel::Wing && console.audience().recipient==4 && game.state().wing!=4,"command combo selects recipient and does not change pilot wing");
    console.wings(game);auto released=glm::vec2(console.view({2560,1440},1).items[7].bounds)+glm::vec2(2);console.pointer(game,{2560,1440},released,false);
    require(console.audience().recipient==7,"popup release requires a down event on its row instead of accepting the opening combo drag");
    console.wings(game);auto first=glm::vec2(console.view({2560,1440},1).items[0].bounds);console.pointer(game,{2560,1440},first-glm::vec2(.5f),true);
    require(console.view({2560,1440},1).items.size()==10,"menu border click closes the wing popup");console.pointer(game,{2560,1440},{0,0},true);
    require(console.view({2560,1440},1).items.empty(),"outside click does not dismiss the popup");
    console.key(game,SDLK_ESCAPE);console.startQuick(game);auto& menu=console.view({2560,1440},1);
    require(menu.items.size()==quick.menus[0].items.size() && menu.items.front().bounds.x==11,"quick chat preserves authored item order and lower-left popup origin");
    auto child=std::find_if(quick.menus[0].items.begin(),quick.menus[0].items.end(),[](auto& item){return item.menu>=0;});require(child!=quick.menus[0].items.end(),"authored quick-chat submenu fixture");
    console.key(game,SDL_Keycode(child->key));
    require(console.view({2560,1440},1).items.size()==quick.menus[0].items.size()+quick.menus[child->menu].items.size(),"authored quick-chat key opens cascading child menu");
    while(console.active())console.key(game,SDLK_ESCAPE);
    auto compiled=console.view({2560,1440},1).artwork.data();console.sync(game);
    require(console.view({2560,1440},1).artwork.data()==compiled,"steady console state retains compiled artwork");
    puts("Original inline chat, shell, target/recipient completion, command transitions, wing selection and quick-chat cascades passed");
}
void routing(const std::filesystem::path& art) {
    SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.rules.miners=0;
    Game game(art,[](auto&,auto&){return ModelHandle{1,1};},0,0,options,{},GameRole::Authority);
    int sender=game.state().shipId,one=game.joinNetworkPilot("Recipient One",0,false),two=game.joinNetworkPilot("Recipient Two",0,false);
    require(one>=0 && two>=0,"authority group recipients");
    auto* core=game.core();auto* target=game.playerShip()->GetStation();int station=target->GetObjectID();
    Net::Action first{};first.op=Net::Operation::ChatOrder;first.args={int(ChatChannel::Group),one,OT_station,station};first.count=c_cidGoto+1;
    require(game.authorityAction(sender,first),"authenticated original group order");
    auto second=first;second.args[0]=int(ChatChannel::GroupNoEcho);second.args[1]=two;
    require(game.authorityAction(sender,second),"subsequent group recipient uses no-echo route");
    require(core->GetShip(ShipID(one))->GetCommandID(c_cmdQueued)==c_cidGoto && core->GetShip(ShipID(two))->GetCommandID(c_cmdQueued)==c_cidGoto,"each human receives the group command");
    auto f=game.networkFrame(sender,1,0),a=game.networkFrame(one,1,0),b=game.networkFrame(two,1,0);
    auto messages=[](const Net::Frame& frame){size_t count=0;for(auto& notice:frame.notices)if(std::string_view(notice.text).find("goto")!=std::string_view::npos)++count;return count;};
    require(messages(f)==1 && messages(a)>=1 && messages(b)>=1,"sender gets one group echo and every recipient gets individual delivery");
    require(game.setWing(two,4),"explicit wing assignment");
    Net::Action chat{};chat.op=Net::Operation::Chat;chat.args={int(ChatChannel::Wing),4};Net::text(chat.text,"Wing-four delivery");
    require(game.authorityAction(sender,chat),"chat to a wing other than sender's");
    a=game.networkFrame(one,2,0);b=game.networkFrame(two,2,0);
    auto wing=[](const Net::Frame& frame){return std::any_of(frame.notices.begin(),frame.notices.end(),[](auto& n){return std::string_view(n.text).find("Wing-four delivery")!=std::string_view::npos;});};
    require(!wing(a) && wing(b),"explicit wing routing excludes unrelated pilots");
    auto invalid=first;invalid.args[0]=int(ChatChannel::Count);require(!game.authorityAction(sender,invalid),"invalid command route is rejected");
    invalid=first;invalid.args[3]+=65536;require(!game.authorityAction(sender,invalid),"command target cannot truncate its identity");
    invalid=first;invalid.count=11;require(!game.authorityAction(sender,invalid),"out-of-range command verb is rejected");
    invalid={};invalid.op=Net::Operation::ChatWaypoint;invalid.args={int(ChatChannel::Target),sender,c_cidGoto,game.playerShip()->GetStation()->GetCluster()->GetObjectID()};invalid.position.x=std::numeric_limits<float>::infinity();
    require(!game.authorityAction(sender,invalid),"non-finite command waypoint is rejected");
    Game replica(art,[](auto&,auto&){return ModelHandle{1,1};},0,0,options,EffectLoader{},GameRole::Replica);
    require(!replica.sendChat(ChatChannel::Target,"Before bootstrap"),"chat accepts an unassigned replica");
    replica.applyNetworkFrame(f);replica.clearMessages();
    int submitted=0;replica.networkActions([&](const Net::Action&){++submitted;return true;});submitted=0;
    require(replica.sendChat(ChatChannel::Target,"#list") && replica.sendChat(ChatChannel::Target,"#draw",replica.state().shipId) && submitted==0 && replica.state().messages.size()==2 && replica.state().messages.back().chatTarget==CHAT_INDIVIDUAL,"individual self chat submitted network work");
    puts("Authority group delivery, single author echo, explicit-wing isolation, local-only self chat and command validation passed");
}
}
int main(int argc,char** argv) {
    try {auto art=argc>1?std::filesystem::path(argv[1]):Assets::steamArtwork();panelAnchors();ballotRules();notifications(art);ballots(art);missionChat(art);composition(art);routing(art);return 0;}
    catch(const std::exception& e){fprintf(stderr,"Console check: %s\n",e.what());return 1;}
}
