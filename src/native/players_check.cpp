#include "game.hpp"
#include "network_records.hpp"
#include "test_fixture.hpp"
#include "../ClientCore/PlayerEconomy.h"
#include "../ClientCore/LobbyReady.h"
#include "test_reference/player_economy.hpp"
#include "test_reference/alliances.hpp"
#include "../ClientCore/Alliances.h"
#include "../ClientCore/ClientSoundEvents.h"
#include "test_reference/chat_filters.hpp"
#include "test_reference/chat_censor.hpp"
#include "chat_censor.hpp"
#include "../ClientCore/ChatPresentation.h"
#include "test_reference/chat_presentation.hpp"
#include <fstream>
#include <chrono>
using NativeTest::require;
namespace {
void membership(const std::filesystem::path& art) {
    Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;
    options.startInHangar=true;options.rules.miners=0;
    Native::Game game(art,[](auto&,auto&){return Native::ModelHandle{1,1};},0,0,options,{},Native::GameRole::Authority);
    auto* core=game.core();auto* pilot=game.playerShip();int id=pilot->GetObjectID();
    int successor=game.joinNetworkPilot("Successor",0,false),enemy=game.joinNetworkPilot("Other commander",1,false);
    require(successor>=0 && enemy>=0,"membership test pilots");
    auto* next=core->GetShip(ShipID(successor));auto* opponent=core->GetShip(ShipID(enemy));
    auto* base=opponent->GetStation();auto* asteroid=core->GetAsteroids()->first()->data();
    Money total=game.pilotBalance(id)+game.pilotBalance(successor),hullValue=pilot->GetValue();
    pilot->SetKills(3);pilot->SetDeaths(4);pilot->SetEjections(2);
    require(!game.moveNetworkPilotToTeam(id,SIDE_TEAMLOBBY-1) && !game.moveNetworkPilotToTeam(id,NA) && !game.moveNetworkPilotToTeam(id,6) &&
        !game.moveNetworkPilotToTeam(SHRT_MAX,0) && !game.moveNetworkPilotToTeam(id+65536,1) && pilot->GetSide()==core->GetSide(0),"invalid admission cannot modify membership");
    game.authorityStep({},.05);double time=game.state().time;
    require(game.moveNetworkPilotToTeam(id,SIDE_TEAMLOBBY),"original No Team departure");
    require(game.core()==core && core->GetShip(ShipID(id))==pilot && pilot->GetSide()==core->GetSide(SIDE_TEAMLOBBY) &&
        !pilot->GetStation() && !pilot->GetCluster() && !pilot->GetParentShip() && !pilot->GetParts()->n(),"departure preserves identity and removes physical seat/equipment");
    require(pilot->GetBaseHullType()->HasCapability(c_habmLifepod) && pilot->GetKills()==0 && pilot->GetDeaths()==4 &&
        pilot->GetEjections()==2 && !pilot->GetAutoDonate(),"original partial Reset preserves lifetime deaths/ejections and leaves a lifepod");
    require(!next->GetAutoDonate() && game.pilotBalance(id)==0 &&
        game.pilotBalance(successor)>=total+hullValue-1 && game.pilotBalance(successor)<=total+hullValue,
        "departure elects successor and conserves the old side's account and hull refund");
    Money refunded=game.pilotBalance(successor);
    require(game.moveNetworkPilotToTeam(id,SIDE_TEAMLOBBY) && game.pilotBalance(successor)==refunded,"repeated departure cannot refund twice");
    require(game.state().dead && !game.state().sideActive && game.state().pilots.empty() && game.state().sectors.empty() &&
        game.contacts().empty() && game.scene(false).empty(),"No Team has no former faction's contacts, map, inventory or scene");
    require(game.state().kills==0 && game.state().deaths==4 && game.state().ejections==2 &&
        game.state().selectedCargo==NA && std::all_of(game.state().commands.begin(),game.state().commands.end(),[](auto& command){return command.command==NA && command.text.empty();}),
        "No Team frontend stats follow the original partial Reset and clear old order/equipment indicators");
    auto lobby=game.networkFrame(id,3,0);lobby.status.stage=STAGE_STARTED;
    require(lobby.entities.size()==size_t(core->GetSides()->n()+1) && lobby.parts.empty() && lobby.buckets.empty() &&
        std::all_of(lobby.entities.begin(),lobby.entities.end(),[&](auto& entity){return entity.identity.type==OT_side || (entity.identity.type==OT_ship && entity.identity.id==id);}),
        "No Team report contains only public factions and its own unassigned identity");
    auto replicaOptions=options;replicaOptions.team=1;
    Native::Game replica(art,[](auto&,auto&){return Native::ModelHandle{1,1};},0,0,replicaOptions,{},Native::GameRole::Replica);
    replica.applyNetworkFrame(lobby);
    for(auto* l=core->GetSides()->first();l;l=l->next())require(
        replica.core()->GetSide(l->data()->GetObjectID())->GetCivilization()->GetObjectID()==l->data()->GetCivilization()->GetObjectID(),
        "reported faction identity overrides a replica's temporary bootstrap seat/civilization");
    require(replica.state().networkLobby && replica.playerShip()->GetSide()->GetObjectID()==SIDE_TEAMLOBBY &&
        replica.state().shipId==id && replica.state().sectors.empty() && replica.contacts().empty() && replica.scene(false).empty(),
        "No Team replica drops former faction visibility and returns to the lobby during a running match");
    require(game.moveNetworkPilotToTeam(enemy,SIDE_TEAMLOBBY),"second No Team visibility fixture");
    game.beginNetworkPublication(3);
    auto ownReport=game.networkReport(id,3,0),otherReport=game.networkReport(enemy,3,0);
    require(ownReport.worlds.size()==2 && otherReport.worlds.size()==2 &&
        ownReport.worlds[0].frame==otherReport.worlds[0].frame && ownReport.worlds[1].domain!=otherReport.worlds[1].domain &&
        ownReport.worlds[1].frame->entities.size()==1 && ownReport.worlds[1].frame->entities[0].identity.id==id &&
        otherReport.worlds[1].frame->entities[0].identity.id==enemy && ownReport.personal.entities.empty() && otherReport.personal.entities.empty(),
        "unassigned pilots share one public faction arena and retain distinct private identity arenas");
    require(game.moveNetworkPilotToTeam(enemy,1),"other pilot re-enters without changing world identity");
    replica.predict({},1./60.);replica.commandView(true);
    require(!replica.state().commandView && !replica.viewCamera(Native::CameraMode::Chase),"No Team cannot restore a former sector through camera controls");
    Native::Net::Action action{};action.op=Native::Net::Operation::Launch;
    require(!game.authorityAction(id,action) && !game.invest(0,1) && !game.handOverCommand(successor),"unassigned pilot cannot act on former faction");
    Native::FlightInput input;input.active=true;input.forward=input.fire=true;
    std::pair<int,Native::FlightInput> pending{id,input};game.authorityStep(std::span(&pending,1),.05);
    require(game.state().time>time && core->GetShip(ShipID(enemy))==opponent && opponent->GetStation()==base &&
        core->GetAsteroid(asteroid->GetObjectID())==asteroid && !pilot->GetCluster(),"other faction and world survive departure while global simulation continues");
    require(game.moveNetworkPilotToTeam(id,1) && core->GetShip(ShipID(id))==pilot && pilot->GetSide()==opponent->GetSide() &&
        pilot->GetStation()==base && !pilot->GetCluster() && pilot->GetParts()->n()>0 && pilot->GetAutoDonate()==opponent &&
        !game.state().dead && game.state().docked && !game.state().commander,"re-entry restores the new faction's starter, station and donation policy on the same pilot");
    auto frame=game.networkFrame(id,3,0);
    frame.status.stage=STAGE_STARTED;replica.applyNetworkFrame(frame);
    require(replica.playerShip()->GetSide()==replica.core()->GetSide(1) && replica.state().docked && !replica.state().networkLobby &&
        replica.sessionOptions().team==1 && !replica.state().research.empty(),"re-entry restores replica faction, hangar and research in its first report");
    require(frame.status.pilot==id && std::any_of(frame.teams.begin(),frame.teams.end(),[&](auto& team){return team.id==0 && team.commander==successor;}),"successor and unchanged membership identity publish correctly");
    require(game.moveNetworkPilotToTeam(id,SIDE_TEAMLOBBY),"disconnect-from-No-Team fixture");
    game.retireNetworkPilot(id);require(!core->GetShip(ShipID(id)),"unassigned disconnect retires the identity safely");

    options.scenario=Native::Scenario::FreeFlight;options.enemies=0;
    Native::Game crew(art,[](auto&,auto&){return Native::ModelHandle{1,1};},0,0,options,{},Native::GameRole::Authority);
    auto* gunner=crew.playerShip();int parent=crew.addLocalPilot("Departing gunship",0,290);
    require(parent>=0 && crew.boardShip(parent),"membership crew fixture");
    auto* departing=crew.core()->GetShip(ShipID(parent));auto* hull=departing->GetBaseHullType();
    require(crew.moveNetworkPilotToTeam(parent,SIDE_TEAMLOBBY) && !gunner->GetParentShip() && gunner->GetBaseHullType()==hull &&
        gunner->GetStation() && gunner->GetParts()->n()>0 && departing->GetSide()==crew.core()->GetSide(SIDE_TEAMLOBBY),
        "departing pilot promotes remaining crew and preserves the occupied hull and station");
    puts("Incremental team departure/re-entry: stable identities/world, original Reset, leader/donations/refunds, No Team isolation, simulation, disconnect and crew promotion passed.");
}
void reference(const std::filesystem::path& art) {
    NativeTest::Fixture f(art);IshipIGC* pilots[4];for(auto& pilot:pilots)pilot=f.ship();
    int cases=0;
    // All acyclic, at-most-one-hop donation graphs over four actual IGC pilots.
    // Reinstall each graph since reversing a link changes incoming links too.
    for(int graph=0;graph<625;++graph) {
        int choices[4],value=graph;bool valid=true;
        for(int i=0;i<4;++i){choices[i]=value%5-1;value/=5;if(choices[i]==i)valid=false;}
        for(int i=0;i<4;++i)if(choices[i]>=0 && choices[choices[i]]>=0)valid=false;
        if(!valid)continue;
        auto install=[&]{for(auto* pilot:pilots)pilot->SetAutoDonate(nullptr);for(int i=0;i<4;++i)if(choices[i]>=0)pilots[i]->SetAutoDonate(pilots[choices[i]]);};
        for(int donor=0;donor<4;++donor)for(int target=-1;target<4;++target)if(target!=donor) {
            auto run=[&](bool original) {
                install();int cleared=-1;
                auto clear=[&](IshipIGC* pilot){for(int i=0;i<4;++i)if(pilots[i]==pilot)cleared=i;pilot->SetAutoDonate(nullptr);};
                auto* recipient=target<0?nullptr:pilots[target];
                auto* result=original?OriginalPlayerEconomy::donation(pilots[donor],recipient,clear):Allegiance::AutoDonationTarget(pilots[donor],recipient,clear);
                pilots[donor]->SetAutoDonate(result);
                std::array<int,6> trace;trace.fill(-1);trace[0]=cleared;
                for(int i=0;i<4;++i){if(result==pilots[i])trace[1]=i;for(int j=0;j<4;++j)if(pilots[i]->GetAutoDonate()==pilots[j])trace[i+2]=j;}
                return trace;
            };
            require(run(false)==run(true),"original donation resolution and reciprocal/incoming links");++cases;
        }
        install();
        for(int old=-1;old<4;++old)for(int threshold=0;threshold<5;++threshold) {
            Allegiance::LeaderContender actual[4];OriginalPlayerEconomy::Contender expected[4];
            require(Allegiance::PickDonationLeader(f.mission->GetSide(0)->GetShips(),old<0?nullptr:pilots[old],threshold,actual)==
                OriginalPlayerEconomy::leader(f.mission->GetSide(0)->GetShips(),old<0?nullptr:pilots[old],threshold,expected),"original vote ordering and strict ties");++cases;
        }
        for(int pattern=0;pattern<81;++pattern) {
            int balances[4],v=pattern;for(auto& money:balances){money=(v%3)*1000;v/=3;}
            auto balance=[&](IshipIGC* ship){for(int i=0;i<4;++i)if(ship==pilots[i])return balances[i];return 0;};
            for(int donor=0;donor<4;++donor)for(int amount:{1,1000,2000,3000}) {
                require(Allegiance::MoneyRequestRecipient(pilots[donor],amount,balance)==OriginalPlayerEconomy::request(pilots[donor],amount,balance),"original request recipient, affordability and ties");++cases;
            }
        }
    }
    printf("%d frozen original donation, leadership and request comparisons passed.\n",cases);
}
void alliances() {
    uint32_t random=127;unsigned cases=0;
    for(int teams=2;teams<=6;++teams)for(int graph=0;graph<4096;++graph) {
        char current[6];int value=graph;for(auto& team:current){team=char(value%4-1);value/=4;}
        for(int mutation=0;mutation<7;++mutation) {
            random=random*1664525+1013904223;unsigned mask=random & ((1u<<teams)-1);
            auto active=[&](SideID team){return bool(mask&(1u<<team));};
            SideID from=short(mutation-1),to=short((random>>8)%7-1);char actual[6],expected[6];
            Allegiance::UpdateAlliances(current,short(teams),from,to,active,actual);
            OriginalAlliances::update(current,short(teams),from,to,active,expected);
            require(std::equal(std::begin(actual),std::end(actual),std::begin(expected)),"original alliance merge/removal/active-team normalization");++cases;
        }
    }
    printf("%u frozen original alliance group/active-team/mutation comparisons passed.\n",cases);
}
void alliedSession(const std::filesystem::path& art) {
    Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;options.startInHangar=true;options.rules.teams=3;options.rules.miners=0;options.team=1;
    options.teamCivilizations={4,2,1,-1,-1,-1};options.alliances={0,0,-1,-1,-1,-1};
    Native::Game game(art,[](auto&,auto&){return Native::ModelHandle{1,10};},0,0,options);
    auto* core=game.core();auto* controlled=game.playerShip();
    auto catalog=Native::Game::civilizations(art);
    for(int team=0;team<3;++team)require(core->GetSide(team)->GetCivilization()->GetObjectID()==catalog[options.teamCivilizations[team]].id,"per-team civilization selection");
    require(controlled->GetSide()==core->GetSide(1) && IsideIGC::AlliedSides(core->GetSide(0),controlled->GetSide()) && !IsideIGC::AlliedSides(core->GetSide(2),controlled->GetSide()),"selected seat and original alliance normalization");
    int ally=game.addLocalPilot("Allied pilot",0),enemy=game.addLocalPilot("Enemy pilot",2);
    auto row=std::find_if(game.state().pilots.begin(),game.state().pilots.end(),[&](auto& pilot){return pilot.id==ally;});
    require(row!=game.state().pilots.end() && row->allied && row->money==0 && std::none_of(game.state().pilots.begin(),game.state().pilots.end(),[&](auto& pilot){return pilot.id==enemy;}),"roster hides enemy pilots and starts observed allied credits at zero");
    int before=game.pilotBalance(ally),money=game.state().money;
    require(game.donateMoney(ally,100) && game.pilotBalance(ally)==before+100 && game.state().money==money-100,"manual allied donation conserves balances");
    row=std::find_if(game.state().pilots.begin(),game.state().pilots.end(),[&](auto& pilot){return pilot.id==ally;});require(row!=game.state().pilots.end() && row->money==100,"offline allied credits expose actual funds or suppress the observed donation");
    require(!game.state().messages.empty() && game.state().messages.back().tip && game.state().messages.back().text=="You gave Allied pilot $100. You now have $"+std::to_string(money-100)+".","allied donation loses original ordinary-tip feedback");
    require(!game.setAutoDonate(ally) && !game.setWing(ally,2) && !game.handOverCommand(ally),"alliance does not grant own-team command/account rights");
    auto chat=[&](IshipIGC* sender,ChatTarget target,ObjectID recipient,bool expected){
        game.clearMessages();core->GetIgcSite()->ReceiveChat(sender,target,recipient,NA,"Recipient audit",c_cidNone,NA,NA);
        require(!game.state().messages.empty()==expected,"original allied/sector/leader chat recipient policy");
    };
    auto* allyShip=core->GetShip(ShipID(ally));auto* enemyShip=core->GetShip(ShipID(enemy));
    const auto sector=controlled->GetStation()->GetCluster()->GetObjectID();
    chat(allyShip,CHAT_ALLIES,0,true);chat(enemyShip,CHAT_ALLIES,2,false);
    chat(allyShip,CHAT_TEAM,0,false);chat(allyShip,CHAT_TEAM,1,true);
    chat(allyShip,CHAT_ALL_SECTOR,sector,true);chat(allyShip,CHAT_FRIENDLY_SECTOR,sector,false);
    chat(allyShip,CHAT_LEADERS,NA,true);chat(allyShip,CHAT_ADMIN,NA,false);
    require(game.sendChat(Native::ChatChannel::Allies,"Allied channel") && game.sendChat(Native::ChatChannel::FriendlySector,"Friendly sector channel") &&
        game.sendChat(Native::ChatChannel::Ship,"Uncrewed ship fallback") && game.sendChat(Native::ChatChannel::Leaders,"Commander channel"),"additional original chat channels send while docked");
    require(!game.sendChat(Native::ChatChannel::Count,"Invalid recipient"),"invalid chat channel is inert");
    game.launch();chat(allyShip,CHAT_FRIENDLY_SECTOR,sector,true);chat(enemyShip,CHAT_FRIENDLY_SECTOR,sector,false);
    chat(allyShip,CHAT_FRIENDLY_SECTOR,ObjectID(sector+1),false);
    auto alliance=core->GetSide(0)->GetAllies();core->GetSide(0)->SetAllies(NA);core->GetSide(1)->SetAllies(NA);
    chat(allyShip,CHAT_ALLIES,0,false);chat(allyShip,CHAT_FRIENDLY_SECTOR,sector,false);
    core->GetSide(0)->SetAllies(alliance);core->GetSide(1)->SetAllies(alliance);
    int successor=game.addLocalPilot("New commander",1);require(successor>=0 && game.handOverCommand(successor),"commander chat handover fixture");
    chat(allyShip,CHAT_LEADERS,NA,false);require(game.controlLocalPilot(successor),"control new commander for chat");chat(allyShip,CHAT_LEADERS,NA,true);
    options.alliances={0,0,0,-1,-1,-1};auto groups=Native::Game::effectiveAlliances(options);require(std::all_of(groups.begin(),groups.end(),[](char value){return value==NA;}),"alliance containing every active team must be rejected");
    options.rules.teams=2;options.alliances={0,0,-1,-1,-1,-1};groups=Native::Game::effectiveAlliances(options);require(groups[0]==NA && groups[1]==NA,"two-team mission cannot form an alliance");
    Native::Game invalid(art,[](auto&,auto&){return Native::ModelHandle{1,10};},0,0,options,{},Native::GameRole::Authority);
    auto policy=invalid.missionPolicy();auto wire=Native::Net::session(invalid.sessionOptions());
    require(policy.alliances==groups && wire.alliances==groups && wire.rules.maxImbalance==policy.imbalance &&
        wire.rules.allowDefections==bool(policy.flags&1),"rejected initial alliance leaves inconsistent authority/welcome policy");
    Native::Game replica(art,[](auto&,auto&){return Native::ModelHandle{1,10};},0,0,Native::Net::options(wire,1,"Policy replica"),{},Native::GameRole::Replica);
    require(replica.missionPolicy()==policy,"initial canonical alliance policy changes during replica creation");
    auto bad=policy;bad.flags=0x80;require(!invalid.applyMissionPolicy(bad) && invalid.missionPolicy()==policy,"invalid policy flags mutate mission state");
    bad=policy;bad.alliances[0]=3;require(!invalid.applyMissionPolicy(bad) && invalid.missionPolicy()==policy,"invalid alliance group mutates mission state");
    puts("Native per-team civilizations, alliances, private roster/donations and allied/flying-sector/commander chat roles passed.");
}
void crewChat(const std::filesystem::path& art){
    Native::SessionOptions options;options.scenario=Native::Scenario::FreeFlight;options.startInHangar=true;options.enemies=0;
    Native::Game game(art,[](auto&,auto&){return Native::ModelHandle{1,1};},0,0,options);
    int gunner=game.state().shipId,pilot=game.addLocalPilot("Chat gunship",0,290),other=game.addLocalPilot("Other ship",0);
    require(pilot>=0 && other>=0 && game.boardShip(pilot),"real gunner chat fixture boards an equipped turret");
    auto* core=game.core();auto* parent=core->GetShip(ShipID(pilot));auto* outsider=core->GetShip(ShipID(other));
    game.clearMessages();core->GetIgcSite()->ReceiveChat(parent,CHAT_SHIP,NA,NA,"Crew only",c_cidNone,NA,NA);require(game.state().messages.size()==1,"gunner receives their parent's ship chat");
    core->GetIgcSite()->ReceiveChat(outsider,CHAT_SHIP,NA,NA,"Different crew",c_cidNone,NA,NA);require(game.state().messages.size()==1,"different parent cannot receive ship crew chat");
    require(game.sendChat(Native::ChatChannel::Ship,"Gunner to crew"),"gunner sends original ship recipient");
    require(game.controlLocalPilot(pilot),"control parent for ship chat");game.clearMessages();
    core->GetIgcSite()->ReceiveChat(core->GetShip(ShipID(gunner)),CHAT_SHIP,NA,NA,"Parent only",c_cidNone,NA,NA);require(game.state().messages.size()==1,"parent receives a child's ship chat");
    require(game.sendChat(Native::ChatChannel::Ship,"Pilot to crew"),"parent sends original ship recipient");
    puts("Original ship-chat parent/gunner isolation and sending passed.");
}
void chatCensor() {
    unsigned cases=0;
    auto compare=[&](const std::string& text){require(Native::censorChat(text)==OriginalChatCensor::filter(text),"original case/overlap/substitution display censor");require(Native::censorChat(text,false)==text,"disabled censor changes text");++cases;};
    auto words=OriginalChatCensor::words();
    require(words.size()==113,"original censor dictionary count");
    for(const auto& word:words) {
        auto upper=word;for(auto& c:upper)if(c>='a' && c<='z')c-=32;
        compare(word);compare(upper);compare("before "+word+" after "+upper);compare(word+word);compare("prefix"+word+"suffix");
        for(const auto& other:words)compare(word+" "+other);
    }
    compare("");compare("ordinary text / café / "+std::string(1,char(255)));compare("line one\nline two");
    printf("%u frozen original display-censor case/overlap/disabled/UTF-8 comparisons passed.\n",cases);
}
void sonicCatalog(const std::filesystem::path& art) {
    auto dir=std::filesystem::temp_directory_path()/("allegiance-sonic-check-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    {std::ofstream out(dir/"quickchat.mdl");out<<R"(
        a=SonicChat([SonicChatVoice(voDefendSound,"Defend fighter"),SonicChatVoice(voNegativeSound,"Alternate voice")]);
        b=SonicChat([SonicChatVoice(voDefendSound,"Defend capital")]);
        replacement=SonicChat([SonicChatVoice(voDefendSound,"Replacement capital")]);
        unused=SonicChat([SonicChatVoice(voAffirmativeSound,"Outside the menu")]);
        unknown=SonicChat([SonicChatVoice(voAffirmativeSound,"Unknown chat")]);
        empty=SonicChat([]);
        sonicChats=[(4,a),(5,b),(2000,a),(5,replacement),(100,unused),(99,unknown),(10,empty)];
        mainMenu=QuickChatMenu([
            QuickChatMenuItem('1',"Fighter",QuickChatCommand(a,-1,0,0)),
            QuickChatMenuItem('2',"Capital",QuickChatCommand(b,-1,0,0))]);
    )";}
    {
        Native::Assets fixture(dir);Native::QuickChatCatalog catalog(fixture);
        require(catalog.sonicChats.size()==6 && catalog.commands.size()==2 && catalog.commands[0].sonic==2000 && catalog.commands[1].sonic==5,"original sonic index overwrite and reused-object SetIndex semantics");
        auto* fighter=catalog.voiceForChat(4);auto* capital=catalog.voiceForChat(5);
        require(fighter && capital && fighter->sound==capital->sound && fighter->text!=capital->text && capital->text=="Replacement capital","shared recording cannot identify distinct sonic texts");
        require(catalog.voiceForChat(2000,1) && catalog.voiceForChat(2000,1)->text=="Alternate voice" && !catalog.voiceForChat(2000,2) && catalog.voiceForChat(100) && catalog.sonicChat(10) && !catalog.voiceForChat(10) && !catalog.sonicChat(0),"variants, non-menu entries, empty voices and sparse ID holes");
        Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;options.rules.teams=2;options.rules.miners=0;
        Native::Game game(art,[](auto&,auto&){return Native::ModelHandle{1,1};},0,0,options);game.quickChats(catalog);
        int id=game.addLocalPilot("Sonic sender",0);require(id>=0,"sonic sender fixture");auto* sender=game.core()->GetShip(ShipID(id));
        auto receive=[&](int logical){game.clearMessages();game.clearSounds();game.core()->GetIgcSite()->ReceiveChat(sender,CHAT_INDIVIDUAL,game.state().shipId,logical,nullptr,c_cidNone,NA,NA);};
        receive(4);require(game.state().messages.size()==1 && game.state().messages[0].text.find("Defend fighter")!=std::string::npos && game.sounds().size()==1 && game.sounds()[0].id==fighter->sound,"incoming logical fighter ID resolves its recording and text");
        receive(5);require(game.state().messages[0].text.find("Replacement capital")!=std::string::npos && game.sounds()[0].id==fighter->sound,"same recording resolves the distinct capital message");
        receive(99);require(game.state().messages.empty() && game.sounds().empty(),"original unknown filter compares sentinel text even for an authored voice");
        catalog.sonicChats.back().voices[0].sound=32000;catalog.refreshSounds();
        require(catalog.commands[0].sound==32000 && catalog.voiceForChat(2000)->text=="Defend fighter","audio registration changes resource IDs without invalidating logical lookup");
        game.clearMessages();game.clearSounds();require(game.sendQuickChat(Native::ChatChannel::Team,catalog.commands[0]) && game.sounds().size()==1 && game.sounds()[0].id==32000 && game.state().messages[0].text.find("Defend fighter")!=std::string::npos,"outgoing command preserves authored logical ID after registration");
        Allegiance::ChatPreferences prefs;prefs.filterUnknown=false;game.chatPreferences(prefs);receive(32767);
        require(game.state().messages.size()==1 && game.sounds().empty(),"unfiltered out-of-table sonic chat keeps Unknown chat text and remains silent");
        receive(10);require(game.state().messages.size()==1 && game.sounds().size()==1 && game.sounds()[0].id==10,"empty registered voice retains original direct-sound fallback");
    }
    std::filesystem::remove_all(dir);
    puts("Original logical sonic IDs, duplicate/aliased indexes, shared recordings, voice variants, sparse holes, non-menu entries and audio remapping passed.");
}
void chatFilters(const std::filesystem::path& art) {
    for(int mode=1;mode<=3;++mode)for(bool lobby:{false,true})for(bool personal:{false,true})for(bool inGame:{false,true})for(bool privileged:{false,true}){
        Allegiance::ChatPreferences prefs;prefs.filterLobby=mode;
        auto result=Allegiance::FilterChat(prefs,true,true,false,false,true,false,false,false,lobby,personal,inGame,privileged);
        bool expected=lobby && inGame && !privileged && (mode==2 || (mode==1 && !personal));
        require(result.discard==expected,"original lobby filter includes PMs only in its second mode, only in game, with privileged PM exemption");
    }
    for(unsigned mask=0;mask<1024;++mask) {
        auto bit=[&](int i){return bool(mask&(1u<<i));};
        Allegiance::ChatPreferences prefs{bit(0),bit(1),bit(2)};
        auto actual=Allegiance::FilterChat(prefs,bit(3),bit(4),bit(5),bit(6),bit(7),bit(8),bit(9));
        auto expected=OriginalChatFilters::receive(bit(0),bit(1),bit(2),bit(3),bit(4),bit(5),bit(6),bit(7),bit(8),bit(9));
        require(actual.discard==expected.discard && (actual.discard || actual.quietVoice==expected.quietVoice),"frozen original chat discard/voice policy");
    }
    Native::Assets assets(art);Native::QuickChatCatalog catalog(assets);
    auto* fighter=catalog.voiceForChat(4);auto* capital=catalog.voiceForChat(5);auto* yes=catalog.voiceForChat(15);
    require(fighter && capital && yes && fighter->sound==capital->sound && fighter->text=="Defend fighter" && capital->text=="Defend capital ship" && yes->text=="Affirmative!","installed original sonic IDs 4/5/15 distinguish shared recordings");
    printf("Installed sonic table: %zu IDs, %zu menu commands, ID 15 affirmative; IDs 4/5 share audio but retain distinct text.\n",catalog.sonicChats.size(),catalog.commands.size());
    Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;options.rules.teams=2;options.rules.miners=0;
    Native::Game game(art,[](auto&,auto&){return Native::ModelHandle{1,1};},0,0,options);game.quickChats(catalog);
    auto* core=game.core();auto* player=game.playerShip();int own=player->GetObjectID();
    int id=game.addLocalPilot("Filter sender",0),other=game.addLocalPilot("Filter recipient",0);
    require(id>=0 && other>=0,"chat filter human seats");auto* sender=core->GetShip(ShipID(id));auto* inactive=core->GetShip(ShipID(other));
    auto voice=std::find_if(catalog.commands.begin(),catalog.commands.end(),[](auto& command){return command.command==c_cidNone && command.sound!=NA;});
    require(voice!=catalog.commands.end() && catalog.voiceForChat(voice->sonic) && !catalog.voiceForChat(32767),"immutable catalog known/unknown voice lookup");
    auto reset=[&]{game.clearMessages();game.clearSounds();player->SetCommand(c_cmdQueued,nullptr,c_cidNone);inactive->SetCommand(c_cmdQueued,nullptr,c_cidNone);};
    auto receive=[&](IshipIGC* from,ChatTarget channel,int sound,const char* text,CommandID command=c_cidNone){
        core->GetIgcSite()->ReceiveChat(from,channel,channel==CHAT_INDIVIDUAL?own:NA,sound,text,command,OT_ship,id,sender);
    };
    Allegiance::ChatPreferences prefs;prefs.filterAll=true;game.chatPreferences(prefs);reset();
    receive(sender,CHAT_EVERYONE,voice->sound,"Public message");require(game.state().messages.empty() && game.sounds().empty(),"all-channel filter discards both text and sound");
    receive(nullptr,CHAT_EVERYONE,NA,"Headquarters retained");require(game.state().messages.size()==1 && game.sounds().size()==1,"HQ bypasses pilot filters");
    reset();receive(sender,CHAT_INDIVIDUAL,NA,"Private retained");require(game.state().messages.size()==1,"all-channel filter keeps personal messages");
    require(!game.mutePilot(-1,true) && !game.mutePilot(65536+id,true) && !game.mutePilot(own,true) && game.mutePilot(id,true),"mute identity validation");
    auto pilot=std::find_if(game.state().pilots.begin(),game.state().pilots.end(),[&](auto& p){return p.id==id;});require(pilot!=game.state().pilots.end() && pilot->muted,"roster exposes mute state");
    reset();receive(sender,CHAT_INDIVIDUAL,voice->sound,"Muted order",c_cidDefend);
    require(game.state().messages.empty() && game.sounds().empty() && player->GetCommandID(c_cmdQueued)==c_cidNone,"muting discards text, voice and orders");
    core->GetIgcSite()->SendChat(sender,CHAT_INDIVIDUAL,other,voice->sound,"Muted inactive order",c_cidDefend,OT_ship,id,sender);
    require(inactive->GetCommandID(c_cmdQueued)==c_cidNone,"muted sender cannot queue on an inactive human seat");
    require(game.mutePilot(id,false),"unmute pilot");reset();receive(sender,CHAT_INDIVIDUAL,voice->sonic,nullptr,c_cidDefend);
    require(game.state().messages.size()==2 && game.state().messages[0].chat && game.state().messages[0].text.find(catalog.voiceForChat(voice->sonic)->text)!=std::string::npos && game.state().messages[1].warning && game.sounds().size()==2 && game.sounds()[0].id==voice->sound && game.sounds()[1].id==newCriticalMsgSound && player->GetCommandID(c_cmdQueued)==c_cidDefend,"known voice resolves text and keeps legal order with its original critical tip");
    prefs.filterQuick=true;game.chatPreferences(prefs);reset();receive(sender,CHAT_INDIVIDUAL,voice->sonic,nullptr,c_cidDefend);
    require(game.state().messages.size()==2 && game.sounds().size()==2 && game.sounds()[0].id==newCommandMsgSound && game.sounds()[1].id==newCriticalMsgSound && player->GetCommandID(c_cmdQueued)==c_cidDefend,"quick filter keeps text/order/tip and replaces voice with command cue");
    reset();receive(sender,CHAT_INDIVIDUAL,voice->sound,"Explicit spoken text");require(game.sounds().size()==1 && game.sounds()[0].id==voice->sound,"explicit non-quick voice is retained");
    reset();receive(sender,CHAT_INDIVIDUAL,32767,nullptr,c_cidDefend);require(game.state().messages.empty() && game.sounds().empty() && player->GetCommandID(c_cmdQueued)==c_cidNone,"unknown human voice filter also blocks orders");
    prefs.filterUnknown=false;game.chatPreferences(prefs);reset();receive(sender,CHAT_INDIVIDUAL,32767,nullptr);
    require(game.state().messages.size()==1 && game.state().messages.back().text.find("Unknown chat")!=std::string::npos && game.sounds().size()==1 && game.sounds()[0].id==newPersonalMsgSound,"unknown voice can be shown independently of voice suppression");
    prefs.filterAll=false;game.chatPreferences(prefs);reset();require(game.sendQuickChat(Native::ChatChannel::Team,*voice),"outgoing quick chat");
    require(game.state().messages.size()==1 && game.sounds().size()==1 && game.sounds()[0].id==newOfflineChatMsgSound,"own quick chat follows voice preference while keeping text");
    reset();core->GetIgcSite()->SendChat(sender,CHAT_INDIVIDUAL,other,NA,"Unmuted order",c_cidDefend,OT_ship,id,sender);
    require(inactive->GetCommandID(c_cmdQueued)==c_cidDefend,"unmuting restores inactive-seat orders");
    reset();const std::string raw="Display-only FUCK message";receive(sender,CHAT_INDIVIDUAL,NA,raw.c_str());
    require(game.state().messages.size()==1 && game.state().messages[0].text.find(raw)!=std::string::npos && game.state().messages[0].displayText.find("FUCK")==std::string::npos,"message stores raw and censored display separately");
    auto serial=game.state().messages[0].serial,revision=game.state().messages[0].displayRevision;
    prefs.censor=false;game.chatPreferences(prefs);
    require(game.state().messages[0].displayText==game.state().messages[0].text && game.state().messages[0].serial==serial && game.state().messages[0].displayRevision>revision,"live censor preference rebuilds history without changing raw messages or identities");
    puts("1,024 frozen chat filter cases and real pilot mute, catalog text, HQ, unknown voice and order delivery passed.");
}
void chatPresentation(const std::filesystem::path& art) {
    for(unsigned flags=0;flags<32;++flags){auto bit=[&](int i){return bool(flags&(1u<<i));};
        require(Allegiance::RecordChat(bit(0),bit(1),bit(2),bit(3),bit(4))==OriginalChatPresentation::record(bit(0),bit(1),bit(2),bit(3),bit(4)),"frozen original chat history selection");
        require(Allegiance::ChatTintScale(bit(4),bit(2),bit(0))==OriginalChatPresentation::tint(bit(4),bit(2),bit(0)),"frozen original sender/recipient tint");
    }
    struct ShellCase {const char* text;bool recognized,muted;const char* name;};
    for(auto c:{ShellCase{"!mute Test pilot",true,true,"Test pilot"},{"!UnMuTe Café",true,false,"Café"},{"!MUTE ",true,true,""},
        {"!unmute  Pilot ",true,false," Pilot "},{"!mute",false,false,""},{"!mute\tPilot",false,false,""},{"!muted Pilot",false,false,""},
        {"!muteX Pilot",false,false,""},{" !mute Pilot",false,false,""},{"!other Pilot",false,false,""},{"hello !mute Pilot",false,false,""}}) {
        auto actual=Allegiance::ParseMuteChatCommand(c.text);require(actual.recognized==c.recognized && (!actual.recognized || (actual.muted==c.muted && std::string_view(actual.name)==c.name)),"original case-insensitive shell prefix and exact name remainder");
    }
    Native::SessionOptions options;options.scenario=Native::Scenario::FreeFlight;options.startInHangar=true;options.enemies=0;
    Native::Game game(art,[](auto&,auto&){return Native::ModelHandle{1,1};},0,0,options);
    auto* core=game.core();auto* player=game.playerShip();int own=player->GetObjectID(),id=game.addLocalPilot("Radio pilot",0);
    require(id>=0,"chat presentation seat");auto* sender=core->GetShip(ShipID(id));auto* side=player->GetSide();auto* station=player->GetStation();auto* cluster=station->GetCluster();
    auto reset=[&]{game.clearMessages();game.clearSounds();player->SetCommand(c_cmdQueued,nullptr,c_cidNone);sender->SetCommand(c_cmdQueued,nullptr,c_cidNone);};
    auto receive=[&](ChatTarget channel,int recipient,CommandID command=c_cidNone,const char* text="Repeated message"){
        core->GetIgcSite()->ReceiveChat(sender,channel,ObjectID(recipient),NA,text,command,OT_ship,id,sender);
    };
    reset();receive(CHAT_EVERYONE,NA);receive(CHAT_EVERYONE,NA);
    require(game.state().messages.size()==2 && game.state().messages[0].text=="Radio pilot (all): Repeated message" && game.state().messages[1].serial>game.state().messages[0].serial,"rapid identical chat keeps separate history identities and audience labels");
    auto color=side->GetColor();require(game.state().messages.back().color==glm::vec4(color.R()*.85f,color.G()*.85f,color.B()*.85f,1),"ordinary chat retains original side tint");
    reset();receive(CHAT_TEAM,side->GetObjectID());require(game.state().messages.back().text==std::string("Radio pilot (")+side->GetName()+"): Repeated message","team recipient name");
    player->SetWingID(3);sender->SetWingID(3);reset();receive(CHAT_WING,3);require(game.state().messages.back().text==std::string("Radio pilot (")+c_pszWingName[3]+"): Repeated message","authored wing recipient name");
    reset();receive(CHAT_ALL_SECTOR,cluster->GetObjectID());require(game.state().messages.back().text==std::string("Radio pilot (")+cluster->GetName()+"): Repeated message","docked sector recipient name");
    reset();core->GetIgcSite()->ReceiveChat(nullptr,CHAT_EVERYONE,NA,NA,"Headquarters",c_cidNone,NA,NA);require(game.state().messages.back().text=="HQ (all): Headquarters" && game.state().messages.back().color==glm::vec4(color.R(),color.G(),color.B(),1),"original headquarters label retains full side color");
    reset();core->GetIgcSite()->SendChat(player,CHAT_GROUP_NOECHO,own,NA,"No local echo");require(game.state().messages.empty() && game.sounds().empty(),"send-only group route suppresses the local author's echo");
    core->GetIgcSite()->SendChat(sender,CHAT_GROUP_NOECHO,own,NA,"Forwarded group delivery");
    require(game.state().messages.size()==1 && game.state().messages[0].text==std::string("Radio pilot (")+player->GetName()+"): Forwarded group delivery" && game.sounds().size()==1 && game.sounds()[0].id==newPersonalMsgSound,"server group delivery becomes an individual message for its actual recipient");
    reset();require(game.sendChat(Native::ChatChannel::Target,"Private outgoing",id),"outgoing individual chat");
    require(game.state().messages.size()==1 && game.state().messages[0].text==std::string(player->GetName())+" (Radio pilot): Private outgoing" && game.sounds().size()==1 && game.sounds()[0].id==newOfflineChatMsgSound,"outgoing echo preserves original recipient and non-personal offline cue");
    require(game.state().messages[0].color==glm::vec4(color.R()*.75f,color.G()*.75f,color.B()*.75f,1),"outgoing remote chat dims by the original factor");
    game.launch();sender->SetStation(nullptr);sender->SetCluster(player->GetCluster());sender->SetPosition(player->GetPosition()+Vector(100,0,0));
    reset();receive(CHAT_INDIVIDUAL,own,c_cidDefend,nullptr);
    require(game.state().messages.size()==2 && game.state().messages[0].text==std::string("Radio pilot (")+player->GetName()+"): "+c_cdAllCommands[c_cidDefend].szVerb+" Radio pilot in "+cluster->GetName() && game.state().messages[1].warning && player->GetCommandID(c_cmdQueued)==c_cidDefend,"unvoiced incoming order describes verb, target and visible sector with a critical tip");
    reset();receive(CHAT_TEAM,side->GetObjectID(),c_cidDefend,"Broadcast order");
    require(game.state().messages.size()==1 && game.state().messages[0].warning && !game.state().messages[0].chat && player->GetCommandID(c_cmdQueued)==c_cidDefend,"incoming broadcast command presents a critical tip without becoming a personal chat record");
    reset();core->GetIgcSite()->SendChat(player,CHAT_INDIVIDUAL,id,NA,nullptr,c_cidDefend,OT_ship,own,player);
    require(game.state().messages.size()==1 && game.state().messages[0].text.find(std::string(c_cdAllCommands[c_cidDefend].szVerb)+" "+player->GetName())!=std::string::npos && player->GetCommandID(c_cmdQueued)==c_cidNone && sender->GetCommandID(c_cmdQueued)==c_cidDefend,"unvoiced outgoing remote order echoes description without queueing on its sender");
    reset();core->GetIgcSite()->ReceiveChat(player,CHAT_INDIVIDUAL,own,NA,nullptr,c_cidDefend,OT_ship,id,sender);
    require(std::none_of(game.state().messages.begin(),game.state().messages.end(),[](auto& message){return message.chat;}) && player->GetCommandTarget(c_cmdAccepted)==sender,"individual self-order is accepted and excluded from chat history");
    reset();double time=game.state().time;auto balance=game.state().money;
    require(game.sendChat(Native::ChatChannel::Target,"!MuTe rAdIo PiLoT",-1),"shell command does not depend on a valid chat recipient");
    receive(CHAT_EVERYONE,NA);require(game.state().messages.empty() && game.sounds().empty(),"typed mute prevents delivery without emitting chat or audio");
    require(game.sendChat(Native::ChatChannel::Everyone,"!unmute Radio") && game.sendChat(Native::ChatChannel::Everyone,"!unknown Radio pilot"),"unknown shell/name consumes composition locally");
    receive(CHAT_EVERYONE,NA);require(game.state().messages.empty(),"name prefixes do not match original exact-name shell lookup");
    require(game.sendChat(Native::ChatChannel::Everyone,"!UNMUTE RADIO PILOT"),"typed unmute");receive(CHAT_EVERYONE,NA);
    require(game.state().messages.size()==1 && game.state().time==time && game.state().money==balance,"typed unmute restores delivery without simulation/economy mutation");
    reset();require(game.sendChat(Native::ChatChannel::Everyone,std::string("!mute ")+player->GetName()) && game.sendChat(Native::ChatChannel::Team,"Self muted"),"original shell permits self-mute");
    require(game.state().messages.empty() && game.sendChat(Native::ChatChannel::Everyone,std::string("!unmute ")+player->GetName()),"self-unmute remains usable after muting local echoes");
    for(int i=0;i<601;++i)receive(CHAT_EVERYONE,NA);
    require(game.state().messages.size()==600 && game.state().messages.back().serial-game.state().messages.front().serial==599,"original 600-entry history evicts exactly one oldest repeated chat per arrival");
    puts("Original chat audience/tint/history decisions, repeated records, unvoiced orders, self/remote echoes and typed mute/unmute passed.");
}
void queuedMoney(const std::filesystem::path& art) {
    Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;options.startInHangar=true;options.rules.miners=0;options.rules.startingMoney=9;
    Native::Game game(art,[](auto&,auto&){return Native::ModelHandle{1,1};},0,0,options);
    auto* core=game.core();auto* player=game.playerShip();int own=player->GetObjectID(),b=game.addLocalPilot("Requesting pilot",0),c=game.addLocalPilot("Other requester",0),enemy=game.addLocalPilot("Enemy requester",1);
    require(b>=0 && c>=0 && enemy>=0,"queued money seats");auto* sender=core->GetShip(ShipID(b));auto* other=core->GetShip(ShipID(c));
    auto request=[&](IshipIGC* from,int amount){core->GetIgcSite()->GetMoneyRequest(from,amount,player->GetBaseHullType()->GetObjectID());game.step({},.001);};
    auto amount=game.pilotBalance(own),before=game.pilotBalance(b);
    request(sender,amount+500);require(game.state().moneyRequest.sender==b && game.state().moneyRequest.prompt.find("to buy a ")!=std::string::npos && !game.state().messages.empty() && game.state().messages.back().warning,"money request queues with its hull and critical tip");
    game.clearSounds();require(game.acceptCommand() && game.pilotBalance(own)==0 && game.pilotBalance(b)==before+amount && game.state().moneyRequest.sender<0,"accept key approves available funds even below the requested amount");
    require(std::any_of(game.sounds().begin(),game.sounds().end(),[](auto& event){return event.id==voAffirmativeSound;}),"money approval sends original affirmative acknowledgement");
    before=game.pilotBalance(b);request(sender,50);require(game.acceptCommand(true) && game.pilotBalance(b)==before && game.pilotBalance(own)==0 && game.state().moneyRequest.sender<0,"execute key consumes zero-balance money request without minting credits");
    require(game.controlLocalPilot(b) && game.donateMoney(own,1000) && game.controlLocalPilot(own),"restore request recipient funds");
    auto balance=game.pilotBalance(own);request(sender,100);game.clearSounds();request(other,200);
    require(game.state().moneyRequest.sender==c && std::none_of(game.sounds().begin(),game.sounds().end(),[](auto& event){return event.id==voNegativeSound;}),"new sender replaces queued request without an automatic negative reply");
    game.clearSounds();game.rejectCommand();require(game.state().moneyRequest.sender<0 && game.pilotBalance(own)==balance && std::any_of(game.sounds().begin(),game.sounds().end(),[](auto& event){return event.id==voNegativeSound;}),"reject key declines money request with original negative acknowledgement and no transfer");
    auto order=[&]{core->GetIgcSite()->ReceiveChat(sender,CHAT_INDIVIDUAL,own,NA,"Defend the requester",c_cidDefend,OT_ship,b,sender);game.step({},.001);};
    order();require(player->GetCommandID(c_cmdQueued)==c_cidDefend,"queued order before money replacement");request(sender,100);
    require(player->GetCommandID(c_cmdQueued)==c_cidNone && game.state().moneyRequest.sender==b,"money request replaces the same sender's queued order");
    order();require(player->GetCommandID(c_cmdQueued)==c_cidDefend && game.state().moneyRequest.sender<0,"order replaces the same sender's queued money request");
    for(int i=0;i<151;++i)game.step({},.1);game.clearSounds();
    require(player->GetCommandID(c_cmdQueued)==c_cidDefend && game.acceptCommand() && std::none_of(game.sounds().begin(),game.sounds().end(),[](auto& event){return event.id==voAffirmativeSound;}),"timeout retains late-acceptable order but removes its acknowledgement sender");
    request(sender,125);balance=game.pilotBalance(own);before=game.pilotBalance(b);for(int i=0;i<151;++i)game.step({},.1);
    require(game.state().moneyRequest.sender<0 && !game.respondMoneyRequest(true) && game.pilotBalance(own)==balance && game.pilotBalance(b)==before,"money request timeout consumes the request without a transfer");
    request(core->GetShip(ShipID(enemy)),100);require(game.state().moneyRequest.sender<0,"enemy cannot bypass original own-team request route");
    require(!game.requestMoney(100,65536+player->GetBaseHullType()->GetObjectID()) && !game.donateMoney(65536+b,1),"money/hull actions reject truncated identities");
    require(game.controlLocalPilot(b) && game.requestMoney(100) && game.controlLocalPilot(own) && game.state().moneyRequest.sender==b && game.acceptCommand(),"inactive local recipient request follows pilot control and the original acceptance key");
    require(game.controlLocalPilot(b) && game.requestMoney(100),"inactive financial timeout fixture");for(int i=0;i<151;++i)game.step({},.1);
    require(game.controlLocalPilot(own) && game.state().moneyRequest.sender<0,"inactive local money request expires without changing seats");
    request(sender,100);require(game.removeLocalPilot(b) && game.state().moneyRequest.sender<0 && !game.respondMoneyRequest(true),"departing requester loses both financial request and acknowledgement identity");
    puts("Original queued money/order replacement, partial/zero approval, accept/execute/reject keys, reply lifetime, own-team authority and inactive-seat timeout passed.");
}
void lobby(const std::filesystem::path& art) {
    int cases=0;
    for(int count=0;count<8;++count)for(int minimum=0;minimum<8;++minimum)for(bool force:{false,true})for(unsigned mask=0;mask<256;++mask) {
        int expectedQueries=0;bool expected;
        // Frozen CFSMission branch order, including lazy force-ready handling.
        if(count<minimum)expected=false;else if(force)expected=true;else {expected=true;for(int i=0;i<count;++i){++expectedQueries;if(!(mask&(1u<<i))){expected=false;break;}}}
        int queries=0;bool actual=Allegiance::SideReady(count,minimum,force,[&]{for(int i=0;i<count;++i){++queries;if(!(mask&(1u<<i)))return false;}return true;});
        require(actual==expected && queries==expectedQueries,"original team readiness eligibility and lazy traversal");++cases;
    }
    Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;options.startInHangar=true;options.rules.miners=0;
    Native::Game baseline(art,[](auto&,auto&){return Native::ModelHandle{1,1};},0,0,options);Money starting=baseline.state().money;
    options.localPilots={{"Wing one",0,true},{"Wing two",0,true},{"Other commander",1,true}};
    Native::Game game(art,[](auto&,auto&){return Native::ModelHandle{1,1};},0,0,options);
    auto* player=game.playerShip();require(game.state().pilots.size()==3 && game.state().money==(starting/3)*3,"pregame pilots precede initial payday without minting credits");
    int accounts=0;
    for(auto* link=game.core()->GetShips()->first();link;link=link->next())if(link->data()->GetPilotType()>=c_ptPlayer) {
        auto* pilot=link->data();++accounts;require(pilot->GetStation() && !pilot->GetCluster(),"pregame seats start at a real side station");
        if(pilot!=player && pilot->GetSide()==player->GetSide())require(pilot->GetAutoDonate()==player && game.pilotBalance(pilot->GetObjectID())==0,"original starting commander donation graph");
        if(pilot->GetSide()!=player->GetSide())require(!pilot->GetAutoDonate() && game.pilotBalance(pilot->GetObjectID())>0,"new side commander receives its initial side account");
    }
    require(accounts==4,"all configured pilot identities are real IGC ships");
    options.localPilots[0].team=6;require(!Native::Game::missionError(options).empty(),"invalid lobby seat side rejected before mission construction");
    printf("%d original readiness/lazy traversal comparisons; pregame pilot identities, stations, donation graphs and initial payday passed.\n",cases);
}
void session(const std::filesystem::path& art) {
    Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;options.startInHangar=true;options.rules.miners=0;options.rules.startingMoney=9;
    Native::Game game(art,[](auto&,auto&){return Native::ModelHandle{1,10};},0,0,options);
    auto* core=game.core();auto* a=game.playerShip();int aid=a->GetObjectID();Money start=game.state().money;
    int bid=game.addLocalPilot("Second pilot",0),cid=game.addLocalPilot("Third pilot",0),eid=game.addLocalPilot("Enemy pilot",1);
    require(bid>=0 && cid>=0 && eid>=0 && game.pilotBalance(aid)==start,"additional free starters do not charge or share the commander account");
    auto* b=core->GetShip(ShipID(bid));auto* c=core->GetShip(ShipID(cid));
    auto tip=[&](const std::string& text,bool critical=false){return std::count_if(game.state().messages.begin(),game.state().messages.end(),[&](const auto& m){return m.tip && !m.chat && m.warning==critical && m.text==text;});};
    auto heard=[&](int sound){return std::count_if(game.sounds().begin(),game.sounds().end(),[&](const auto& s){return s.id==sound && s.relative;});};
    require(b->GetAutoDonate()==a && c->GetAutoDonate()==a && game.pilotBalance(bid)==0 && game.state().pilots.size()==3,"initial individual accounts and commander donation links");
    require(tip("Second pilot is now donating to you",true)==1 && tip("Third pilot is now donating to you",true)==1 && heard(investorSound)==1,"new donors lack original critical investor feedback or repeat the first-donor cue");
    game.clearMessages();game.clearSounds();
    require(!game.donateMoney(bid,-1) && !game.donateMoney(bid,0) && !game.donateMoney(bid,start+1) && !game.donateMoney(aid,1) && !game.donateMoney(eid,1),"invalid transfers never mint or leak credits");
    require(game.donateMoney(bid,2000) && game.pilotBalance(aid)==start-2000 && game.pilotBalance(bid)==2000,"explicit transfer conserves balances");
    require(game.state().messages.empty() && game.sounds().empty(),"same-team sender receives its recipient's private donation feedback");
    require(game.controlLocalPilot(bid) && game.state().money==2000 && !game.state().commander,"controlled pilot sees their own account");
    require(game.setAutoDonate(-1) && !b->GetAutoDonate(),"pilot can retain future income");
    require(tip(std::string("You have stopped donating your income to ")+a->GetName(),true)==1 && heard(newCriticalMsgSound)==1,"retaining income lacks original critical donor feedback");
    game.clearMessages();game.clearSounds();
    Money beforeA=game.pilotBalance(aid),beforeB=game.pilotBalance(bid);
    core->GetIgcSite()->PaydayEvent(b->GetSide(),10);game.step({},.001);
    require(game.pilotBalance(aid)==beforeA+6 && game.pilotBalance(bid)==beforeB+3 && game.pilotBalance(cid)==0,"original equal share, remainder and donation routing");
    require(tip("You received a payday of $3.")==1 && heard(paydaySound)==1 && heard(newNonCriticalMsgSound)==1,"personal payday lacks original amount, console tip or sounds");
    core->GetIgcSite()->PaydayEvent(b->GetSide(),2);game.step({},.001);
    require(game.pilotBalance(aid)==beforeA+8 && game.pilotBalance(bid)==beforeB+4,"remainder retained across mining paydays");
    require(tip("You received a payday of $1.")==1 && heard(paydaySound)==2,"mining payday repeats or loses retained-remainder feedback");
    auto project=std::find_if(game.state().research.begin(),game.state().research.end(),[](auto& item){return item.available && !item.complete && item.price>0;});
    require(project!=game.state().research.end(),"available personal bucket contribution");int bucket=project->id;
    auto before=game.state().money;beforeA=game.pilotBalance(aid);require(game.invest(bucket,1) && game.state().money==before-1 && game.pilotBalance(aid)==beforeA,"bucket funding spends only this pilot's credits");
    require(game.requestMoney(1000) && game.controlLocalPilot(aid) && game.state().moneyRequest.sender==bid && game.state().moneyRequest.amount==1000,"request reaches original chosen account owner");
    beforeA=game.pilotBalance(aid);beforeB=game.pilotBalance(bid);
    require(game.respondMoneyRequest(true) && game.state().moneyRequest.sender<0 && game.pilotBalance(aid)==beforeA-1000 && game.pilotBalance(bid)==beforeB+1000,"accepting request transfers once and consumes the request");
    require(!game.respondMoneyRequest(true),"consumed request cannot transfer twice");
    require(game.setWing(bid,4) && b->GetWingID()==4 && !game.setWing(bid,10),"commander wing authority and bounds");
    require(game.controlLocalPilot(bid) && !game.setWing(aid,2) && game.setWing(bid,3),"ordinary pilot assigns only their own wing");
    require(game.setAutoDonate(aid,7) && b->GetAutoDonate()==a,"one-time payment and future income routing");
    require(game.controlLocalPilot(aid),"donation reversal controller fixture");game.clearMessages();game.clearSounds();
    require(game.setAutoDonate(bid) && a->GetAutoDonate()==b && !b->GetAutoDonate() && c->GetAutoDonate()==b && !game.state().commander,"reciprocal donation reversal redirects incoming donors and elects the majority leader");
    require(tip("Second pilot has stopped donating to you",true)==1 && tip("You have started donating your income to Second pilot",true)==1 && tip("Second pilot has become the team leader",true)==1 && heard(commanderSound)==0,"reciprocal reversal loses original personal/leader feedback or leaks the commander's cue");
    require(game.controlLocalPilot(bid) && game.state().commander,"majority donation vote chooses the commander");
    beforeA=game.pilotBalance(aid);beforeB=game.pilotBalance(bid);
    require(game.handOverCommand(aid) && game.pilotBalance(bid)==0 && game.pilotBalance(aid)==beforeA+beforeB && b->GetAutoDonate()==a && c->GetAutoDonate()==a,"command handover conserves the full account and reassigns incoming donations");
    require(!game.handOverCommand(cid),"previous commander cannot hand over twice");
    require(game.controlLocalPilot(aid) && game.state().commander,"new commander UI state");
    require(!game.removeLocalPilot(aid),"controlled pilot cannot be removed behind its view");
    require(game.controlLocalPilot(bid),"departure controller fixture");
    Money total=game.pilotBalance(aid)+game.pilotBalance(bid)+game.pilotBalance(cid),value=a->GetValue();
    require(game.removeLocalPilot(aid) && !core->GetShip(ShipID(aid)) && !b->GetAutoDonate() && c->GetAutoDonate()==b && game.state().commander,"departing leader promotes successor and redirects incoming donations");
    Money remaining=game.pilotBalance(bid)+game.pilotBalance(cid);
    require(remaining<=total+value && remaining>=total+value-1,"departing leader returns balance and ship value, preserving only original integer remainder");
    value=c->GetValue();total=remaining;
    require(game.removeLocalPilot(cid) && game.state().pilots.size()==1 && game.pilotBalance(bid)>=total+value,"last teammate departure returns its account/value and retained remainder");
    require(!game.removeLocalPilot(cid),"departing account cannot pay twice");
    puts("Native player accounts, cash flow, personal purchases, requests, wings, donation votes and command handover passed.");
}
void cashRewards(const std::filesystem::path& art) {
    Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;options.startInHangar=true;options.rules.miners=0;
    Native::Game game(art,[](auto&,auto&){return Native::ModelHandle{1,1};},0,0,options);
    int investor=game.state().shipId,pilot=game.addLocalPilot("Salvager",0);require(pilot>=0 && game.controlLocalPilot(pilot),"cash reward pilot fixture");
    auto* ship=game.playerShip();auto* core=game.core();auto* cluster=ship->GetStation()->GetCluster();
    auto collect=[&]{DataTreasureIGC data{};data.time0=core->GetLastUpdate();data.createNow=true;data.lifespan=60;data.objectID=core->GenerateNewTreasureID();data.treasureCode=c_tcCash;data.treasureID=99;data.clusterID=cluster->GetObjectID();data.p0=cluster->GetWarps()->first()->data()->GetPosition();
        TRef<ItreasureIGC> treasure=static_cast<ItreasureIGC*>(core->CreateObject(core->GetLastUpdate(),OT_treasure,&data,sizeof(data)));require(treasure,"cash reward creation");treasure->Release();
        require(core->GetIgcSite()->HitTreasureEvent(core->GetLastUpdate(),ship,treasure),"cash reward collection");treasure->Terminate();game.step({},.001);};
    for(bool donate:{true,false}) {
        if(!donate)require(game.setAutoDonate(-1),"cash reward retained-income fixture");
        game.clearMessages();game.clearSounds();Money a=game.pilotBalance(investor),b=game.pilotBalance(pilot);collect();
        require(game.pilotBalance(investor)==a+(donate?99:0) && game.pilotBalance(pilot)==b+(donate?0:99),"cash reward credits the original donation recipient");
        std::string expected=donate?std::string("You donated your reward of $99 to ")+core->GetShip(ShipID(investor))->GetName()+".":"You received a reward of $99.";
        require(std::count_if(game.state().messages.begin(),game.state().messages.end(),[&](const auto& m){return m.tip && !m.warning && !m.chat && m.text==expected;})==1,"cash pickup loses original reward tip");
        for(int sound:{pickUpCashSound,newNonCriticalMsgSound})require(std::count_if(game.sounds().begin(),game.sounds().end(),[&](const auto& s){return s.id==sound && s.relative;})==1,"cash reward loses or duplicates its personal sounds");
    }
    puts("Original retained/donated cash reward amounts, personal tips and pickup sounds passed");
}
}
int main(int argc,char** argv) {
    try{auto art=argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork();membership(art);reference(art);alliances();lobby(art);session(art);cashRewards(art);alliedSession(art);crewChat(art);chatCensor();sonicCatalog(art);chatFilters(art);chatPresentation(art);queuedMoney(art);return 0;}
    catch(const std::exception& e){fprintf(stderr,"players: %s\n",e.what());return 1;}
}
