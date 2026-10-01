#include "test_reference/authored_hosts.hpp"
#include "test_reference/lobby_roster.hpp"
#include "test_reference/lobby_actions.hpp"
#include "test_reference/lobby_alliances.hpp"
#include "test_reference/wings.hpp"
#include "test_reference/classic_ui_batch.hpp"
#include "test_reference/classic_keyboard.hpp"
#include "test_reference/training_menu.hpp"
#include "test_reference/docked_workflows.hpp"
#include "test_reference/lan.hpp"
#include "test_reference/local_game.hpp"
#include "test_reference/configuration_actions.hpp"
#include "test_reference/connection_info.hpp"
#include "test_reference/custom_maps.hpp"
#include "test_reference/mission_setup.hpp"
#include "test_reference/list_input.hpp"
#include "test_reference/popup_input.hpp"
#include "test_reference/player_search.hpp"
#include "test_reference/core_identity.hpp"
#include "test_reference/ui_feedback.hpp"
#include "frontend.hpp"
#include "../Igc/igc.h"
#include "game.hpp"
#include "training_prompts.hpp"
#include "message_pager.hpp"
#include "text_wrap.hpp"
#include "input_mapping.hpp"
#include "../ClientCore/MessageLifetime.h"
#include "../ClientCore/CommandPicking.h"
#include "../ClientCore/GameTypes.h"
#include "../ClientCore/GameListPresentation.h"
#include "../ClientCore/InvestmentPresentation.h"
#include "../ClientCore/MissionLifecycle.h"
#include "../ClientCore/SectorMapPresentation.h"
#include "../ClientCore/CommPanePresentation.h"
#include "../WinTrek/mappvmaker.h"
#include <cstdio>
#include <fstream>
#include <thread>
#include <chrono>
#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif
using namespace Native;
int main(int argc,char** argv) {
    try {
        auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
        {
            std::array<Allegiance::LobbyTeamState,3> teams{{{3,true,true},{1,true,true},{1,true,true}}};
            auto state=[&](int imbalance,int minimum=0,int maximum=32768){return Allegiance::LobbyStart(3,minimum,maximum,imbalance,true,[&](int i){return teams[i];});};
            require(state(1).block==Allegiance::LobbyBlock::Unbalanced,"start gate must traverse every side even without ranked balance");
            require(state(32766).block==Allegiance::LobbyBlock::Unbalanced,"unranked Enforce Balance uses original rank-one threshold");
            require(state(32767).CanStart(),"no imbalance restriction permits uneven teams");
            teams[0].players=2;teams[1].ready=false;
            auto blocked=state(1);require(blocked.side==1 && blocked.block==Allegiance::LobbyBlock::NotReady && !blocked.multiple,"one blocking side must be named");
            teams[2].ready=false;require(state(1).multiple,"multiple blockers use original collective status");
            teams[1].ready=teams[2].ready=true;
            require(state(32767,2).block==Allegiance::LobbyBlock::TooFew,"minimum-size status precedes readiness");
            require(state(32767,0,1).block==Allegiance::LobbyBlock::TooMany,"maximum-size status is preserved");
            teams[2]={0,false,false};teams[0].players=teams[1].players=2;
            require(state(0,2).CanStart(),"inactive empty team does not block an Allow Empty Teams mission");
            for(auto [seconds,expected]:std::array<std::pair<int,const char*>,6>{{{0,"0"},{9,"9"},{60,"1:00"},{599,"9:59"},{3600,"1:00:00"},{3661,"1:01:01"}}})
                require(Allegiance::LobbyTimer(seconds,[](int n){return std::to_string(n);})==expected,"original lobby timer formatting");
            puts("Original lobby start policy: count/rank-one balance, side readiness, size precedence, inactive teams and timer formatting passed");
        }
        {
            using Allegiance::SectorMapPoint;using Allegiance::SectorMapBounds;
            std::array<SectorMapPoint,2> points{{{-1,-1},{1,1}}};auto bounds=SectorMapBounds::Calculate(points,120,96);
            auto a=bounds.Pixel(points[0],120,96),b=bounds.Pixel(points[1],120,96);
            if(!bounds.canDrag || std::abs(a.x-20)>1 || std::abs(a.y-83)>1 || std::abs(b.x-100)>1 || std::abs(b.y-3)>1)throw std::runtime_error("Original map extent, 2.2-unit clipping or bottom-origin conversion differs");
            auto dragged=SectorMapBounds::Calculate(points,120,96,{100,100});
            if(std::abs(dragged.drag.x)>1e-5 || std::abs(dragged.drag.y-.2f)>1e-5)throw std::runtime_error("Map drag exceeds original buffered map bounds");
            std::array<SectorMapPoint,1> single{{{.25f,-.75f}}};auto centered=SectorMapBounds::Calculate(single,120,96);
            auto p=centered.Pixel(single[0],120,96);if(centered.canDrag || std::abs(p.x-60)>1 || std::abs(p.y-47)>1)throw std::runtime_error("One known sector must remain finite and centered");
            std::array<int,6> stations{2,2,0,3,0,1};if(Allegiance::SectorOwners(stations)!=std::pair(3,0))throw std::runtime_error("Map owner/secondary tie ordering differs");
            stations.fill(0);if(Allegiance::SectorOwners(stations)!=std::pair(-1,-1))throw std::runtime_error("Empty sector has an owner");
            stations={0,4,4,0,0,0};if(Allegiance::SectorOwners(stations)!=std::pair(1,2))throw std::runtime_error("Equal owners must retain original side-list priority");
            constexpr const char* images[]={nullptr,"sectorcombatbmp","sectorminerbmp","sectorbomberbmp","sectorcapitalbmp","sectorwarningbmp"};
            constexpr int warnings[]={3,4,5,8,10,13};for(size_t i=0;i<std::size(warnings);++i){auto actual=Allegiance::SectorWarningImage(warnings[i]);if((actual==nullptr)!=(images[i]==nullptr) || (actual && std::string_view(actual)!=images[i]))throw std::runtime_error("Original sector warning precedence differs");}
            puts("Sector map policy: original clipping/aspect/axes, single discovery, bounded drag, station ownership/ties and warning markers passed");
        }
        {
            GameState history;MessagePager pager;
            for(uint64_t id=1;id<=40;++id)history.messages.push_back({double(id),std::to_string(id),false,id});
            pager.page(history.messages,40,true);
            auto offset=pager.update(history.messages,40);
            if(offset!=12 || history.messages.rbegin()[offset].text!="28")throw std::runtime_error("Original message paging advances a full visible page");
            history.messages.push_back({41,"41",false,41});history.messages.pop_front();
            offset=pager.update(history.messages,41);
            if(history.messages.rbegin()[offset].text!="28")throw std::runtime_error("New messages or history eviction move a retained historical page");
            pager.page(history.messages,42,true);pager.page(history.messages,43,true);
            if(pager.update(history.messages,43)!=history.messages.size()-MessagePager::rows)throw std::runtime_error("Paging must clamp to oldest complete history page");
            pager.page(history.messages,44,false);pager.page(history.messages,45,false);pager.page(history.messages,46,false);
            if(pager.update(history.messages,46)!=0)throw std::runtime_error("Page down must restore newest messages");
            pager.page(history.messages,47,true);
            if(pager.update(history.messages,56.99)==0 || pager.update(history.messages,57)!=0)throw std::runtime_error("Original ten-second autoscroll restoration");
            history.messages.clear();if(pager.update(history.messages,58))throw std::runtime_error("Cleared history retains an invalid page");
            for(size_t rows=1;rows<=10;++rows){pager.reset();pager.setRows(rows);
                for(uint64_t id=1;id<=20;++id)history.messages.push_back({0,std::to_string(id),false,id});
                pager.page(history.messages,58,true);if(pager.offset()!=rows)throw std::runtime_error("Configured chat page differs from ListPaneOld full-page increments");
                for(int i=0;i<30;++i)pager.page(history.messages,58,true);
                if(pager.offset()!=history.messages.size()-rows)throw std::runtime_error("Configured chat page cannot reach the oldest row");history.messages.clear();}
            pager.reset();pager.setRows(MessagePager::rows);
            // Wrapped rows use sparse message/line IDs, rather than assuming
            // that consecutive visible rows have consecutive serial numbers.
            for(uint64_t id=1;id<=30;++id)history.messages.push_back({0,std::to_string(id),false,(id<<16)|(id%3)});
            pager.page(history.messages,60,true);auto anchored=history.messages.rbegin()[pager.update(history.messages,60)].serial;
            for(uint64_t id=31;id<=34;++id)history.messages.push_back({0,std::to_string(id),false,(id<<16)|(id%3)});
            history.messages.pop_front();
            if(history.messages.rbegin()[pager.update(history.messages,61)].serial!=anchored)throw std::runtime_error("Wrapped history sparse row IDs move the anchored page");
            history.messages.clear();pager.reset();pager.setRows(4);
            pager.page(history.messages,70,true);require(!pager.following(),"PageUp on empty chat must still pause autoscroll");
            pager.page(history.messages,70,false);require(!pager.following(),"PageDown with no last item cannot enable original autoscroll");
            for(uint64_t id=1;id<=6;++id)history.messages.push_back({0,std::to_string(id),false,id});
            require(pager.update(history.messages,71)==2 && history.messages[0].serial==1,"short historical page loses its top as new chat arrives");
            require(pager.update(history.messages,79.99)==2 && pager.update(history.messages,80)==0 && pager.following(),"empty/short chat hold must expire after ten seconds");
            pager.page(history.messages,81,true);pager.page(history.messages,82,false);require(pager.following(),"PageDown at the bottom restores autoscroll immediately");
            history.messages.push_back({0,"7",false,7});require(pager.update(history.messages,83)==0,"new chat after PageDown must follow the newest row");
            pager.page(history.messages,84,true);pager.setRows(3);require(pager.following() && !pager.offset(),"original chat resize rebuilds and restores autoscroll");
            for(int lines:{1,6,7,20})for(int fontHeight:{8,12,22})for(bool loadout:{false,true}) {
                Allegiance::CommPanePresentation pane(lines,fontHeight,loadout);
                int configured=loadout?std::min(lines,6):lines,height=configured*fontHeight+8;
                require(pane.slots==size_t(height/fontHeight) && pane.listHeight==height,"original chat padding/visible slots/bottom anchor differ");
            }
            puts("Original chat: full pages, sparse/evicted history, empty/short holds, timeout, resize and 24 font/loadout slot combinations passed");
            std::vector<WrappedTextLine> lines;
            auto mono=[](std::string_view glyph){return glyph.empty()?0.f:1.f;};
            std::string wrapped="alpha beta café supercalifragilistic\nnext paragraph";
            wrapText(wrapped,8,2,mono,lines);
            std::string reconstructed;for(auto& line:lines){auto text=std::string_view(wrapped).substr(line.offset,line.length);if(!text.empty() && (uint8_t(text.front())&0xc0)==0x80)throw std::runtime_error("Wrapped text splits a UTF-8 codepoint");reconstructed+=text;}
            auto strip=[](std::string text){std::erase_if(text,[](char c){return c==' ' || c=='\n';});return text;};
            if(strip(reconstructed)!=strip(wrapped) || lines.size()<6)throw std::runtime_error("Word wrapping loses long words or Unicode text");
            wrapText("\nA\n",1,0,mono,lines);if(lines.size()!=3 || lines[0].length || lines[1].length!=1 || lines[2].length)throw std::runtime_error("Word wrapping loses explicit empty lines");
            wrapText("ABCDE",.1f,0,mono,lines);if(lines.size()!=5)throw std::runtime_error("A narrow text column must still make progress");
            for(bool critical:{false,true})for(bool training:{false,true})for(double age:{-1.,0.,7.499,7.5,7.501,29.999,30.,30.001,1000.}) {
                bool expected=!critical || age<(training?30.:7.5);
                if(Allegiance::MessageVisible(critical,training,age)!=expected)throw std::runtime_error("Original critical-tip durations or persistent ordinary messages differ");
            }
            for(bool pref:{false,true})for(bool command:{false,true})for(bool animating:{false,true})for(bool oldCommand:{false,true})
                if(Allegiance::CommandGridVisible(pref,command,animating,oldCommand)!=(pref || command || (animating && oldCommand)))throw std::runtime_error("Original grid preference/camera transition visibility");
        }
        {
            Settings prefs;Frontend scale(prefs);
            auto select=[&](MenuOp op,int value=0,int delta=1){for(size_t i=0;i<scale.view().items.size();++i){auto a=scale.view().items[i].action;if(a.op==op && a.value==value && a.delta==delta)return scale.activate(i);}throw std::runtime_error("Scale lobby action missing");};
            select(MenuOp::Scenario,int(Scenario::Skirmish));
            select(MenuOp::Enemies,0,0);if(!scale.wantsText())throw std::runtime_error("Numeric team editor does not request SDL text input");scale.text("1024");scale.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
            select(MenuOp::Allies,0,0);scale.text("1024");scale.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
            if(scale.session().enemies!=1024 || scale.session().allies!=1024 || !Game::missionError(scale.session()).empty())throw std::runtime_error("Large team entry is capped or invalid");
            select(MenuOp::Enemies,0,1);select(MenuOp::Allies,0,0);scale.text("9");scale.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            if(scale.session().enemies!=1025 || scale.session().allies!=1024)throw std::runtime_error("Large team increment or cancelled edit lost the count");
            select(MenuOp::Navigate,int(Screen::Scenarios));select(MenuOp::Scenario,int(Scenario::Conquest));select(MenuOp::Navigate,int(Screen::LobbyPilots));for(int i=0;i<63;++i)select(MenuOp::AddLobbyPilot);
            if(scale.session().localPilots.size()!=63 || !Game::missionError(scale.session()).empty())throw std::runtime_error("Local pilot seats still have an artificial cap");
            SessionOptions overflow;overflow.enemies=32767;overflow.allies=1;if(Game::missionError(overflow).empty())throw std::runtime_error("Unrepresentable ship identities accepted");
        }
        {
            Settings shipped;shipped.importInputMap((argc>1?std::filesystem::path(argv[1]):Assets::steamArtwork())/"defaultinputmap.mdl");
            auto custom=shipped;custom.keys[size_t(Binding::Coast)]=SDL_SCANCODE_F10;
            Frontend mapped(custom,shipped);
            if(custom.keys[size_t(Binding::Coast)]!=SDL_SCANCODE_F10)throw std::runtime_error("Creating frontend overwrites a customized input map");
            mapped.open(Screen::Settings);
            auto invoke=[&](MenuOp op){for(size_t i=0;i<mapped.view().items.size();++i)if(mapped.view().items[i].action.op==op)return mapped.activate(i);throw std::runtime_error("Original default map settings action missing");};
            invoke(MenuOp::Defaults);invoke(MenuOp::Apply);
            if(custom.keys!=shipped.keys || custom.modifiers!=shipped.modifiers || custom.keyCommands!=shipped.keyCommands || custom.keyAliases!=shipped.keyAliases || custom.mouseButtons!=shipped.mouseButtons || custom.controller.mapButtons!=shipped.controller.mapButtons)
                throw std::runtime_error("Restore defaults must restore the actual original input map, not prototype bindings");
        }
        Settings settings;Frontend ui(settings);
        // Drain consecutive mouse actions without an intervening render. The
        // second coordinate must address the newly opened scenario screen.
        auto click=[&](glm::ivec2 size,float scale,glm::vec2 p,MenuOp expected) {
            auto hit=menuHit(ui.view(),size,scale,p);
            if(!hit || ui.view().items[*hit].action.op!=expected)throw std::runtime_error("Current menu mouse action missed after navigation/resize");
            return ui.activate(*hit);
        };
        click({1152,900},1,{476,410},MenuOp::Navigate);
        click({1152,900},1,{476,240},MenuOp::Scenario);
        if(ui.screen()!=Screen::Lobby || ui.session().scenario!=Scenario::Practice)throw std::runtime_error("Consecutive unrendered menu actions lost navigation");
        if(menuHit(ui.view(),{1920,1080},1,{1320,792}))throw std::runtime_error("Disabled start accepted a mouse hit");
        click({1920,1080},1,{960,792},MenuOp::Ready);
        if(!menuHit(ui.view(),{640,480},2,{487,357}) || menuHit(ui.view(),{640,480},2,{-1,-1}))throw std::runtime_error("Narrow resized menu hit regions use stale coordinates");
        ui.open(Screen::Home);
        {
            Settings prefs;Frontend countdown(prefs);
            auto select=[&](MenuOp op,int value=0){for(size_t i=0;i<countdown.view().items.size();++i)if(countdown.view().items[i].action.op==op && countdown.view().items[i].action.value==value)return countdown.activate(i);throw std::runtime_error("Countdown control absent");};
            select(MenuOp::Scenario,int(Scenario::Skirmish));select(MenuOp::Ready);select(MenuOp::Start);
            if(countdown.sounds().size()!=1 || countdown.sounds()[0]!=countdownStartingSound)throw std::runtime_error("Lobby countdown does not announce its start");
            countdown.clearSounds();countdown.tick(5.01);
            if(countdown.sounds().size()!=1 || countdown.sounds()[0]!=countdown10Sound)throw std::runtime_error("Lobby countdown omits original ten-second voice");
            countdown.clearSounds();countdown.tick(0);
            if(!countdown.sounds().empty())throw std::runtime_error("Countdown repeats voice every frame");
            select(MenuOp::CancelCountdown);countdown.tick(60);
            if(!countdown.sounds().empty())throw std::runtime_error("Cancelled countdown keeps announcing");
            select(MenuOp::Start);countdown.clearSounds();countdown.tick(14.5);
            if(countdown.sounds().size()!=1 || countdown.sounds()[0]!=countdown1Sound)throw std::runtime_error("Delayed countdown must select current second without replaying skipped speech");
        }
        { GameState chatState;ui.setPlaying(true);ui.open(Screen::Chat,&chatState);ui.text("Flight check");
          auto message=ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);if(message.effect!=FrontEffect::SendChat || message.value!=int(ChatChannel::Everyone) || ui.chatText()!="Flight check")throw std::runtime_error("Chat text/recipient send workflow");
          ui.key(SDLK_TAB,SDL_SCANCODE_TAB);message=ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);if(message.value!=int(ChatChannel::Team))throw std::runtime_error("Chat recipient cycling");
          for(int i=2;i<int(ChatChannel::Count);++i){ui.key(SDLK_TAB,SDL_SCANCODE_TAB);message=ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);if(message.value!=i)throw std::runtime_error("Chat cannot reach an original recipient");}
          ui.key(SDLK_TAB,SDL_SCANCODE_TAB);if(ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).value!=int(ChatChannel::Everyone))throw std::runtime_error("Expanded chat recipients do not wrap");
          if(ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE).effect!=FrontEffect::Resume)throw std::runtime_error("Chat escape resumes flight");ui.setPlaying(false);ui.open(Screen::Home); }
        {GameState chatState;chatState.targetObjectType=ContactShip;chatState.targetId=77;ui.setPlaying(true);ui.chatSector(9,chatState);ui.text("Sector message");
          auto message=ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);if(message.effect!=FrontEffect::SendChat || message.value!=int(ChatChannel::FriendlySector) || message.targetId!=9)throw std::runtime_error("Map right-click must address the picked sector, not the targeted pilot");
          ui.open(Screen::Chat,&chatState);message=ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);if(message.value!=int(ChatChannel::FriendlySector) || message.targetId!=-1)throw std::runtime_error("New chat must discard a stale remote sector recipient");
          ui.setPlaying(false);ui.open(Screen::Home);}
        {GameState chatState;ui.setPlaying(true);ui.open(Screen::Chat,&chatState);ui.text("!MuTe Radio pilot");
          if(ui.view().subtitle!="Local command" || std::any_of(ui.view().items.begin(),ui.view().items.end(),[](auto& item){return item.action.op==MenuOp::ChatChannel;}))throw std::runtime_error("Typed shell still presents a network recipient");
          auto channel=ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).value;ui.key(SDLK_TAB,SDL_SCANCODE_TAB);
          if(ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).value!=channel || ui.chatText()!="!MuTe Radio pilot")throw std::runtime_error("Shell Tab changes recipient or text");
          while(!ui.chatText().empty())ui.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);ui.text("Ordinary chat");
          if(ui.view().subtitle!="Chat")throw std::runtime_error("Deleting shell prefix does not restore ordinary composition");
          ui.setPlaying(false);ui.open(Screen::Home);}

        auto choose=[&](MenuOp op,int value=0) {
            auto& items=ui.view().items;
            for(size_t i=0;i<items.size();++i)if(items[i].action.op==op && items[i].action.value==value)return ui.activate(i);
            throw std::runtime_error("Missing menu action");
        };
        choose(MenuOp::Navigate,int(Screen::Scenarios));choose(MenuOp::Scenario,int(Scenario::Skirmish));
        if(choose(MenuOp::Start).effect!=FrontEffect::None)throw std::runtime_error("Unready lobby started");
        choose(MenuOp::Ready);if(choose(MenuOp::Start).effect!=FrontEffect::None || ui.screen()!=Screen::Countdown || ui.tick(14).effect!=FrontEffect::None || ui.tick(1).effect!=FrontEffect::Start || ui.tick(100).effect!=FrontEffect::None)throw std::runtime_error("Ready lobby countdown/start must emit once");
        ui.setPlaying(false);ui.open(Screen::Scenarios);choose(MenuOp::Scenario,int(Scenario::Conquest));choose(MenuOp::Ready);
        choose(MenuOp::Navigate,int(Screen::MissionRules));
        // row helper emits minus before plus; use the actual plus action.
        auto rule=[&](int id){for(size_t i=0;i<ui.view().items.size();++i){auto& a=ui.view().items[i].action;if(a.op==MenuOp::MissionSetting && a.value==id && a.delta==1){ui.activate(i);return;}}throw std::runtime_error("Missing mission rule control");};
        rule(2);rule(3);rule(4);rule(5);rule(6);choose(MenuOp::RulesPage,1);rule(7);rule(8);rule(9);rule(10);rule(11);
        auto& rules=ui.session().rules;
        if(rules.flags!=1 || rules.artifacts!=1 || rules.teamKills!=5 || rules.prosperity!=.5f || rules.minutes!=5 || rules.startingMoney!=1.5f || rules.miners!=2 || rules.lives!=0 || !rules.friendlyFire)throw std::runtime_error("Mission rules did not propagate from menus");
        ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);if(ui.screen()!=Screen::Lobby || choose(MenuOp::Start).effect!=FrontEffect::None)throw std::runtime_error("Mission rule edit did not invalidate ready state");
        ui.open(Screen::MissionRules);choose(MenuOp::RulesPage,2);rule(14);rule(15);rule(17);rule(18);rule(20);
        choose(MenuOp::RulesPage,3);rule(21);rule(23);rule(25);rule(27);
        choose(MenuOp::RulesPage,4);rule(28);rule(30);rule(33);rule(34);
        choose(MenuOp::RulesPage,5);rule(35);rule(38);
        if(rules.teams!=3 || rules.mapSize!=1 || !rules.showMap || !rules.showHome || rules.ejectPods || rules.homeAsteroids!=21 || rules.homeMineable!=3 || rules.homeSpecial!=2 || rules.resourceAmounts || rules.homeTreasures!=1 || !rules.alliedSight || !rules.alliedRipcord || rules.shipyard || rules.expansion)
            throw std::runtime_error("World/resource/team/path settings lost through pagination");
        ui.open(Screen::Lobby);
        for(size_t i=0;i<ui.view().items.size();++i)if(ui.view().items[i].action.op==MenuOp::JoinTeam && ui.view().items[i].action.delta==1){ui.activate(i);break;}
        if(ui.session().team!=1)throw std::runtime_error("Lobby team selection lost");
        ui.civilizations({{0,"A"},{1,"B"},{2,"C"},{3,"D"},{4,"E"}});ui.open(Screen::Teams);
        choose(MenuOp::TeamCivilization,0);choose(MenuOp::TeamAlliance,0);
        for(size_t i=0;i<ui.view().items.size();++i)if(ui.view().items[i].action.op==MenuOp::SelectTeam && ui.view().items[i].action.delta==1){ui.activate(i);break;}
        choose(MenuOp::TeamAlliance,1);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
        if(ui.screen()!=Screen::Lobby || ui.session().teamCivilizations[0]<0 || ui.session().alliances[0]!=2 || ui.session().alliances[1]!=2)throw std::runtime_error("Team civilization/alliance edits lost through lobby navigation");
        choose(MenuOp::Navigate,int(Screen::LobbyPilots));choose(MenuOp::AddLobbyPilot);choose(MenuOp::AddLobbyPilot);
        choose(MenuOp::LobbyPilotName,0);ui.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);ui.text("A");ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
        choose(MenuOp::LobbyPilotTeam,1);choose(MenuOp::LobbyPilotTeam,0);choose(MenuOp::LobbyPilotTeam,0);choose(MenuOp::LobbyPilotReady,0);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);choose(MenuOp::Ready);
        if(choose(MenuOp::Start).effect!=FrontEffect::None || ui.screen()!=Screen::Lobby)throw std::runtime_error("Unready additional seat must block mission start");
        choose(MenuOp::Navigate,int(Screen::LobbyPilots));choose(MenuOp::LobbyPilotReady,1);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);choose(MenuOp::Start);
        if(ui.screen()!=Screen::Countdown)throw std::runtime_error("All local pilots ready must enter countdown");
        choose(MenuOp::CancelCountdown);if(ui.screen()!=Screen::Lobby || ui.tick(100).effect!=FrontEffect::None)throw std::runtime_error("Cancelled countdown must not launch later");
        choose(MenuOp::Start);if(ui.tick(15).effect!=FrontEffect::Start || ui.session().localPilots.size()!=2 || ui.session().localPilots[0].name!="Pilot A" || ui.session().localPilots[1].team!=2)throw std::runtime_error("Pilot edits/readiness lost through countdown");
        ui.open(Screen::LobbyPilots);choose(MenuOp::RemoveLobbyPilot,1);if(ui.session().localPilots.size()!=1)throw std::runtime_error("Local lobby removal lost");
        ui.open(Screen::Lobby);
        {GameState accounts;accounts.economy=true;accounts.money=5000;accounts.commander=true;accounts.economyRevision=51;
            accounts.pilots={{10,0,0,5000,-1,"Commander","Scout","Home",true,true,true,false,false},{11,0,3,0,10,"Pilot","Fighter","Home",false,false,true,false,false}};
            accounts.moneyRequest={11,1000,-1,"Pilot"};ui.setPlaying(true);ui.open(Screen::Pilots,&accounts);
            if(choose(MenuOp::MoneyResponse,1).effect!=FrontEffect::MoneyResponse)throw std::runtime_error("Money request approval UI absent");
            choose(MenuOp::Account,11);if(choose(MenuOp::Donate,11).effect!=FrontEffect::Donate || choose(MenuOp::AutoDonate,11).effect!=FrontEffect::AutoDonate || choose(MenuOp::HandOver,11).effect!=FrontEffect::HandOver || choose(MenuOp::Wing,11).effect!=FrontEffect::Wing || choose(MenuOp::ControlPilot,11).effect!=FrontEffect::ControlPilot || choose(MenuOp::RemovePilot,11).effect!=FrontEffect::RemovePilot)
                throw std::runtime_error("Pilot account actions do not dispatch");
            auto mute=choose(MenuOp::MutePilot,11);if(mute.effect!=FrontEffect::MutePilot || mute.command!=1)throw std::runtime_error("Pilot mute action absent");
            accounts.pilots[1].muted=true;accounts.economyRevision++;ui.sync(accounts);
            mute=choose(MenuOp::MutePilot,11);if(mute.effect!=FrontEffect::MutePilot || mute.command!=0)throw std::runtime_error("Pilot unmute action absent");
            accounts.commander=false;accounts.economyRevision++;ui.sync(accounts);
            if(choose(MenuOp::HandOver,11).effect!=FrontEffect::None || choose(MenuOp::Wing,11).effect!=FrontEffect::None || choose(MenuOp::RemovePilot,11).effect!=FrontEffect::None)throw std::runtime_error("Pilot UI exposes commander-only actions");
            ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);if(ui.screen()!=Screen::Pilots)throw std::runtime_error("Pilot details lose roster context");
            choose(MenuOp::Account,10);if(choose(MenuOp::RequestMoney).effect!=FrontEffect::RequestMoney)throw std::runtime_error("Own-account request UI absent");
            if(choose(MenuOp::MutePilot,10).effect!=FrontEffect::None)throw std::runtime_error("Cannot mute own pilot");
            accounts.complete=true;accounts.commander=true;ui.open(Screen::Pilots,&accounts);
            if(choose(MenuOp::MoneyResponse,1).effect!=FrontEffect::None || choose(MenuOp::MoneyResponse,0).effect!=FrontEffect::None || choose(MenuOp::Crew).effect!=FrontEffect::None)throw std::runtime_error("Completed roster enables requests or crew changes");
            choose(MenuOp::Account,11);
            if(choose(MenuOp::MutePilot,11).effect!=FrontEffect::MutePilot)throw std::runtime_error("Completed results cannot change local mute preference");
            for(auto op:{MenuOp::Donate,MenuOp::AutoDonate,MenuOp::HandOver,MenuOp::Wing,MenuOp::ControlPilot,MenuOp::RemovePilot})if(choose(op,11).effect!=FrontEffect::None)throw std::runtime_error("Completed pilot account enables state mutations");
            ui.open(Screen::Pilots,&accounts);choose(MenuOp::Account,10);
            if(choose(MenuOp::RequestMoney).effect!=FrontEffect::None || choose(MenuOp::AutoDonate,-1).effect!=FrontEffect::None)throw std::runtime_error("Completed own account enables income changes");
            accounts.complete=false;accounts.sideActive=false;ui.open(Screen::Pilots,&accounts);
            if(choose(MenuOp::MoneyResponse,1).effect!=FrontEffect::None || choose(MenuOp::MoneyResponse,0).effect!=FrontEffect::None)throw std::runtime_error("Eliminated team enables financial approvals in a continuing match");
            choose(MenuOp::Account,11);
            for(auto op:{MenuOp::Donate,MenuOp::AutoDonate,MenuOp::HandOver,MenuOp::Wing,MenuOp::ControlPilot,MenuOp::RemovePilot})if(choose(op,11).effect!=FrontEffect::None)throw std::runtime_error("Eliminated team account enables gameplay mutations");
            if(choose(MenuOp::MutePilot,11).effect!=FrontEffect::MutePilot)throw std::runtime_error("Eliminated observer cannot change its local mute preference");
            accounts.sideActive=true;ui.sync(accounts);
            if(choose(MenuOp::Donate,11).effect!=FrontEffect::Donate)throw std::runtime_error("Active team account actions are not restored after changing local seats");
            ui.setPlaying(false);ui.open(Screen::Home);
        }
        {
            GameState docked;docked.docked=true;docked.station="Origin";ui.setPlaying(true);ui.open(Screen::Hangar,&docked);
            if(choose(MenuOp::Teleport).effect!=FrontEffect::Teleport)throw std::runtime_error("Hangar station destination request absent");
            std::vector<TeleportChoice> destinations{{10,"Origin","Alpha",true,true},{11,"Destination","Beta",false,true},{12,"Replacement","Gamma",false,false}};
            ui.teleports(destinations,docked);
            if(ui.screen()!=Screen::Teleport || choose(MenuOp::TeleportTo,10).effect!=FrontEffect::None)throw std::runtime_error("Current station must remain disabled for independent pilots");
            auto transfer=choose(MenuOp::TeleportTo,11);
            if(transfer.effect!=FrontEffect::TeleportTo || transfer.value!=11)throw std::runtime_error("Station destination selection does not dispatch identity");
            ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            if(ui.screen()!=Screen::Hangar)throw std::runtime_error("Teleport escape loses hangar context");
            docked.parentId=8;docked.passenger=true;ui.teleports(destinations,docked);
            if(choose(MenuOp::TeleportTo,10).effect!=FrontEffect::TeleportTo)throw std::runtime_error("Passenger must be able to leave its parent at the same station");
            docked.parentId=-1;docked.passenger=false;docked.ghost=true;ui.open(Screen::Hangar,&docked);
            if(choose(MenuOp::Teleport).effect!=FrontEffect::Teleport)throw std::runtime_error("Ghost station travel unavailable");
            docked.ghost=false;docked.trainingMission=2;ui.open(Screen::Hangar,&docked);
            if(choose(MenuOp::Teleport).effect!=FrontEffect::None)throw std::runtime_error("Authored training must not expose independent station travel");
            ui.setPlaying(false);ui.open(Screen::Home);
        }
        ui.open(Screen::Settings);choose(MenuOp::Navigate,int(Screen::Controls));choose(MenuOp::Bind,0);
        ui.key(SDLK_T,SDL_SCANCODE_T);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);choose(MenuOp::Apply);
        if(settings.keys[0]!=SDL_SCANCODE_T)throw std::runtime_error("Keyboard mapping did not apply");
        ui.open(Screen::Settings);choose(MenuOp::Navigate,int(Screen::Controls));choose(MenuOp::BindingPage,1);
        choose(MenuOp::Bind,int(Binding::Reload));ui.key(SDLK_R,SDL_SCANCODE_R);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);choose(MenuOp::Apply);
        ui.open(Screen::Settings);choose(MenuOp::Navigate,int(Screen::Controls));
        choose(MenuOp::BindingPage,0);choose(MenuOp::Bind,int(Binding::ThrottleUp));ui.key(SDLK_F10,SDL_SCANCODE_F10);
        // Reassign an existing default: the two actions exchange keys instead of
        // silently sharing a scancode or quitting from an in-game menu.
        choose(MenuOp::Unbind,int(Binding::ThrottleDown));
        ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);choose(MenuOp::Apply);
        if(settings.keys[size_t(Binding::ThrottleUp)]!=SDL_SCANCODE_F10 || settings.keys[size_t(Binding::TargetFriendly)]!=SDL_SCANCODE_T || settings.keys[size_t(Binding::ThrottleDown)]!=SDL_SCANCODE_UNKNOWN)
            throw std::runtime_error("Default interface keys cannot be rebound or cleared");
        ui.open(Screen::Pause);if(ui.key(SDLK_F10,SDL_SCANCODE_F10).effect!=FrontEffect::None)throw std::runtime_error("Friendly-target key quits from a menu");
        ui.controllers({{"virtual-stick","Virtual flight stick",false,4,8,1}},"Virtual flight stick",4,8,1,false);
        ui.open(Screen::Settings);choose(MenuOp::Navigate,int(Screen::Controls));choose(MenuOp::BindingPage,0);choose(MenuOp::Bind,int(Binding::Boost));ui.key(SDLK_LSHIFT,SDL_SCANCODE_LSHIFT,true,KeyShift);ui.key(SDLK_F10,SDL_SCANCODE_F10,true,KeyShift);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);choose(MenuOp::Apply);
        if(settings.keys[size_t(Binding::Boost)]!=SDL_SCANCODE_F10 || settings.modifiers[size_t(Binding::Boost)]!=KeyShift)throw std::runtime_error("Chord assignment finishes on modifier down or loses modifier on apply");
        ui.open(Screen::Settings);choose(MenuOp::Navigate,int(Screen::Controls));choose(MenuOp::Bind,int(Binding::Boost));ui.key(SDLK_LSHIFT,SDL_SCANCODE_LSHIFT,true,KeyShift);ui.keyReleased(SDLK_LSHIFT,SDL_SCANCODE_LSHIFT);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);choose(MenuOp::Apply);
        if(settings.keys[size_t(Binding::Boost)]!=SDL_SCANCODE_LSHIFT || settings.modifiers[size_t(Binding::Boost)]!=KeyAny)throw std::runtime_error("Standalone modifier assignment does not wait for release");
        ui.open(Screen::Settings);choose(MenuOp::Navigate,int(Screen::Controls));choose(MenuOp::Bind,int(Binding::Coast));
        ui.key(SDLK_LCTRL,SDL_SCANCODE_LCTRL,false,KeyControl);ui.keyReleased(SDLK_LCTRL,SDL_SCANCODE_LCTRL);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);choose(MenuOp::Apply);
        if(settings.keys[size_t(Binding::Coast)]!=SDL_SCANCODE_LCTRL || settings.modifiers[size_t(Binding::Coast)]!=KeyControl ||
            KeyboardBindings(settings).pressed(SDL_SCANCODE_LCTRL,SDL_KMOD_CTRL)!=Binding::Coast)
            throw std::runtime_error("Editor creates an unreachable standalone modifier press action");
        ui.open(Screen::Settings);choose(MenuOp::Navigate,int(Screen::Controller));choose(MenuOp::ControllerSetting,2);
        choose(MenuOp::Navigate,int(Screen::ControllerAxes));choose(MenuOp::ControllerInvert,1);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
        if(ui.screen()!=Screen::Controller)throw std::runtime_error("Controller axes lose configuration context");
        choose(MenuOp::Navigate,int(Screen::ControllerButtons));choose(MenuOp::ControllerButton,0);
        for(int page=0;page<8 && std::none_of(ui.view().items.begin(),ui.view().items.end(),[](auto& item){return item.action.op==MenuOp::ControllerAssign && item.action.value==int(Binding::AcceptOrder);});++page) {
            auto& items=ui.view().items;auto next=std::find_if(items.begin(),items.end(),[](auto& item){return item.action.op==MenuOp::ControllerPage && item.action.delta==1 && item.enabled;});
            if(next==items.end())throw std::runtime_error("Controller action chooser cannot reach next page");
            ui.activate(next-items.begin());
        }
        choose(MenuOp::ControllerAssign,int(Binding::AcceptOrder));ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
        choose(MenuOp::Apply);
        if(!settings.controller.linear || !settings.controller.invert[1] || settings.controller.buttons[0]!=int(Binding::AcceptOrder))throw std::runtime_error("Controller configuration edits lost on return/apply");
        {
            auto plus=[&](MenuOp op,int value=0){for(size_t i=0;i<ui.view().items.size();++i){auto& a=ui.view().items[i].action;if(a.op==op && a.value==value && a.delta==1){ui.activate(i);return;}}throw std::runtime_error("Mapped controller plus control absent");};
            ui.open(Screen::Settings);choose(MenuOp::Navigate,int(Screen::Controller));choose(MenuOp::Navigate,int(Screen::ControllerMap));choose(MenuOp::MapMode);
            choose(MenuOp::MapAxis,0);plus(MenuOp::MapAxisDevice);plus(MenuOp::MapAxisInput);choose(MenuOp::MapAxisInvert);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            if(ui.screen()!=Screen::ControllerMap)throw std::runtime_error("Axis routes lose configuration context");
            choose(MenuOp::Navigate,int(Screen::ControllerMapDevices));plus(MenuOp::MapDevice,0);choose(MenuOp::MapDevicePage);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            choose(MenuOp::Navigate,int(Screen::ControllerMapButtons));choose(MenuOp::MapButtonAdd);plus(MenuOp::MapButtonDevice);plus(MenuOp::MapButtonInput);choose(MenuOp::MapButtonAction);
            while(std::none_of(ui.view().items.begin(),ui.view().items.end(),[](auto& item){return item.action.op==MenuOp::MapButtonAssign && item.action.value==int(Binding::Screenshot);})){plus(MenuOp::ControllerPage,1);}
            choose(MenuOp::MapButtonAssign,int(Binding::Screenshot));ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            choose(MenuOp::MapButtonAdd);choose(MenuOp::MapButtonDelete);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);choose(MenuOp::Apply);
            if(!settings.controller.mapped || settings.controller.mapAxes[0]!=ControllerAxisRoute{0,0,false,true} || settings.controller.mapDevices[0]!="virtual-stick" || settings.controller.mapButtons!=std::vector<ControllerButtonRoute>{{1,1,Binding::Screenshot}})throw std::runtime_error("Controller device/axis/button routes lose draft across Back and Apply");
            ui.open(Screen::Settings);choose(MenuOp::Navigate,int(Screen::Controller));choose(MenuOp::Navigate,int(Screen::ControllerMap));choose(MenuOp::MapMode);
            ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            if(!settings.controller.mapped)throw std::runtime_error("Cancel applies controller map mode");
        }
        ui.open(Screen::Settings);choose(MenuOp::Navigate,int(Screen::Movies));choose(MenuOp::Setting,9);choose(MenuOp::Setting,10);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);choose(MenuOp::Apply);
        if(!settings.startupCredits || !settings.startupIntro)throw std::runtime_error("Startup movie preferences lose draft on return/apply");
        ui.open(Screen::Settings);choose(MenuOp::Navigate,int(Screen::Audio));choose(MenuOp::Setting,11);choose(MenuOp::Setting,12);choose(MenuOp::Setting,13);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);choose(MenuOp::Apply);
        if(settings.effectsGain!=-9 || settings.voiceGain!=-14 || settings.controller.feedbackGain!=.95f)throw std::runtime_error("Audio/feedback configuration loses draft on return/apply");
        ui.open(Screen::Settings);choose(MenuOp::Navigate,int(Screen::Interface));choose(MenuOp::Setting,14);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);choose(MenuOp::Apply);
        if(settings.cockpitRadar!=3 || settings.commandRadar!=0)throw std::runtime_error("Cockpit/command radar preferences are coupled or lose their settings context");
        ui.open(Screen::Settings);choose(MenuOp::Navigate,int(Screen::Interface));choose(MenuOp::Navigate,int(Screen::Visuals));
        for(int i=0;i<int(visualFields.size());++i)choose(MenuOp::VisualSetting,i);
        ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);if(ui.screen()!=Screen::Interface)throw std::runtime_error("Appearance preferences lose interface context");
        ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);choose(MenuOp::Apply);
        for(auto field:visualFields)if(settings.visual.*field)throw std::runtime_error("Appearance edits lose draft across Back and Apply");
        ui.open(Screen::Settings);choose(MenuOp::Navigate,int(Screen::Interface));choose(MenuOp::Setting,15);choose(MenuOp::Setting,16);choose(MenuOp::Setting,20);choose(MenuOp::Navigate,int(Screen::Mouse));
        choose(MenuOp::Setting,17);choose(MenuOp::Setting,18);choose(MenuOp::Setting,19);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
        if(ui.screen()!=Screen::Interface)throw std::runtime_error("Mouse controls lose interface context");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);choose(MenuOp::Apply);
        if(settings.cockpitRadar!=3 || settings.commandRadar!=3 || !settings.showGrid || settings.showCommandGrid || !settings.virtualJoystick || settings.joystickIndicator || settings.mouseAcceleration!=2)throw std::runtime_error("Independent radar, grid and mouse edits lose configuration draft");
        settings.mouseAliases.push_back({0,Binding::Chaff});
        ui.open(Screen::Settings);choose(MenuOp::Navigate,int(Screen::Interface));choose(MenuOp::Navigate,int(Screen::Mouse));choose(MenuOp::Navigate,int(Screen::MouseButtons));
        choose(MenuOp::MouseButton,0);choose(MenuOp::MouseAssign,int(Binding::Eject));
        ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);if(ui.screen()!=Screen::Mouse)throw std::runtime_error("Mouse assignments lose steering context");
        ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);choose(MenuOp::Apply);
        if(settings.mouseButtons[0]!=int(Binding::Eject) || !settings.mouseAliases.empty())throw std::runtime_error("Mouse reassignment loses draft or retains the old imported aliases");
        auto file=std::filesystem::temp_directory_path()/("allegiance-config-check-"+std::to_string(getpid()));
        {auto before=settings.chat;ui.open(Screen::Settings);choose(MenuOp::Navigate,int(Screen::Interface));choose(MenuOp::Navigate,int(Screen::ChatPreferences));
            for(int i=0;i<4;++i)choose(MenuOp::ChatPreference,i);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            if(ui.screen()!=Screen::Interface)throw std::runtime_error("Chat preferences lose interface context");
            ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);choose(MenuOp::Apply);
            if(settings.chat.filterAll==before.filterAll || settings.chat.filterQuick==before.filterQuick || settings.chat.filterUnknown==before.filterUnknown || settings.chat.censor==before.censor)throw std::runtime_error("Chat preferences do not apply");
            auto applied=settings.chat;ui.open(Screen::Settings);choose(MenuOp::Navigate,int(Screen::Interface));choose(MenuOp::Navigate,int(Screen::ChatPreferences));choose(MenuOp::ChatPreference,0);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            if(!(settings.chat==applied))throw std::runtime_error("Cancelled chat preferences changed live settings");
        }
        settings.pilot="Test pilot";settings.volume=.25f;settings.hudStyle=4;settings.save(file);auto restored=Settings::load(file);std::filesystem::remove(file);
        if(restored.hudStyle!=4)throw std::runtime_error("Authored HUD style does not persist");
        if(restored.showCommandGrid!=settings.showCommandGrid || restored.chat!=settings.chat || restored.chatLines!=settings.chatLines || restored.chatSize!=settings.chatSize)throw std::runtime_error("Command grid or chat preferences lost on settings reload");
        if(restored.pilot!=settings.pilot || restored.keys!=settings.keys || restored.modifiers!=settings.modifiers || restored.keyAliases!=settings.keyAliases || restored.mouseButtons!=settings.mouseButtons || restored.mouseAliases!=settings.mouseAliases || restored.virtualJoystick!=settings.virtualJoystick || restored.joystickIndicator!=settings.joystickIndicator || restored.mouseAcceleration!=settings.mouseAcceleration || restored.visual!=settings.visual || restored.volume!=settings.volume || restored.showGrid!=settings.showGrid || restored.cockpitRadar!=settings.cockpitRadar || restored.commandRadar!=settings.commandRadar || restored.effectsGain!=settings.effectsGain || restored.voiceGain!=settings.voiceGain || restored.controller.feedbackGain!=settings.controller.feedbackGain || restored.controller.mapped!=settings.controller.mapped || restored.controller.mapDevices!=settings.controller.mapDevices || restored.controller.mapAxes!=settings.controller.mapAxes || restored.controller.mapButtons!=settings.controller.mapButtons || restored.controller.buttons!=settings.controller.buttons || restored.controller.axes!=settings.controller.axes || restored.controller.invert!=settings.controller.invert || restored.controller.linear!=settings.controller.linear || restored.controller.feedback!=settings.controller.feedback || restored.startupCredits!=settings.startupCredits || restored.startupIntro!=settings.startupIntro)throw std::runtime_error("Configuration round trip failed");
        {
            auto original=file;original+=".json";
            const std::string source=R"({"Graphics.Fullscreen":false,"FlipY":1,"EnableFeedback":0,"LinearControlResponse":1,"DeadZone":31,"MouseSensitivity":"0.8","Debris":"0.8","StyleHUD":"5.0","SmokeEffects":2,"Online.CharacterName":"Imported pilot","Ui.ShowStartupCreditsMovie":true,"Ui.ShowStartupIntroMovie":true,"SFXGain":20,"VoiceOverGain":25,"FFGain":4000,"RadarLOD":2,"ShowGrid":true,"VirtualJoystick":true,"ShowJoystickIndicator":false,"MouseAcceleration":2,"WheelDelay":4,"Environment":false,"Posters":false,"Stars":false,"Strobes":false,"Trails":false,"Lens Flare":false,"BidirectionalLighting":false,"CenterHUD":false,"TargetHUD":false,"ChatHistory":false,"FilterChatsToAll":false,"FilterQuickComms":true,"FilterUnknownChats":false,"CensorChats":true,"FilterLobbyChats":2,"ChatLines":9,"ChatSize":3,"Steam.OfficerToken":"ignored"})";
            {std::ofstream out(original);out<<source;}
            auto migrated=restored;auto keys=migrated.keys;
            auto expectedVisual=settings.visual;expectedVisual.debris=3;
            if(migrated.importOriginal(original)!=38 || migrated.hudStyle!=5 || migrated.fullscreen || !migrated.invertY || migrated.controller.feedback || !migrated.controller.linear || migrated.controller.deadZone!=.04f || migrated.sensitivity!=.8f || migrated.particles!=4 || migrated.pilot!="Imported pilot" || !migrated.startupCredits || !migrated.startupIntro || migrated.keys!=keys || migrated.volume!=restored.volume || migrated.effectsGain!=-20 || migrated.voiceGain!=-25 || migrated.controller.feedbackGain!=.4f || migrated.cockpitRadar!=2 || migrated.commandRadar!=2 || !migrated.showGrid || migrated.throttleDelay!=4 || migrated.visual!=expectedVisual || !migrated.virtualJoystick || migrated.joystickIndicator || migrated.mouseAcceleration!=2 || !migrated.chat.filterAll || migrated.chat.filterQuick || migrated.chat.filterUnknown || !migrated.chat.censor || migrated.chat.filterLobby!=2 || migrated.chatLines!=9 || migrated.chatSize!=3)
                throw std::runtime_error("Original configuration mapping loses supported preferences or overwrites unrelated native settings");
            migrated.save(file);auto filterRestore=Settings::load(file);std::filesystem::remove(file);
            if(!(filterRestore.chat==migrated.chat) || filterRestore.hudStyle!=5 || filterRestore.particles!=4 || filterRestore.visual.debris!=3 || filterRestore.chatLines!=9 || filterRestore.chatSize!=3)throw std::runtime_error("Chat or original effect preferences do not persist");
            {std::ifstream in(original);std::string after((std::istreambuf_iterator<char>(in)),{});if(after!=source)throw std::runtime_error("Original configuration source was modified");}
            {std::ofstream out(original);out<<R"({"MouseSensitivity":"bad","Graphics.Fullscreen":"false","Online.CharacterName":2,"Debris":"nan","DeadZone":10})";}
            if(migrated.importOriginal(original)!=1 || migrated.controller.deadZone!=.1f || migrated.pilot!="Imported pilot" || migrated.sensitivity!=.8f)throw std::runtime_error("Original malformed fields must leave existing preferences intact");
            {std::ofstream out(original);out<<"{broken";}
            bool rejected=false;try{migrated.importOriginal(original);}catch(const std::exception&){rejected=true;}
            if(!rejected || migrated.pilot!="Imported pilot")throw std::runtime_error("Invalid original JSON partially changed settings");
            std::filesystem::remove(original);
        }
        {GameState done;done.complete=true;done.docked=true;done.result="Victory";ui.setPlaying(true);ui.open(Screen::Results,&done);
            for(auto& item:ui.view().items)if(item.action.op==MenuOp::Resume || item.action.op==MenuOp::Launch || item.action.op==MenuOp::ReturnToBase)throw std::runtime_error("Completed session exposes flight controls");
            choose(MenuOp::Navigate,int(Screen::Scores));ui.sync(done);
            if(ui.screen()!=Screen::Scores)throw std::runtime_error("Scoreboard lost on completed session sync");
            if(ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE).effect!=FrontEffect::None || ui.screen()!=Screen::Results)throw std::runtime_error("Scoreboard escapes into completed flight");
            ui.open(Screen::Settings);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            if(ui.screen()!=Screen::Results)throw std::runtime_error("Configuration loses completed session context");
            done.trainingMission=6;done.result="Training ended: ship destroyed";ui.open(Screen::Results,&done);
            auto exit=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](auto& item){return item.action.op==MenuOp::Leave && item.label=="Return to training";});
            if(exit==ui.view().items.end())throw std::runtime_error("Authored training result advertises the wrong destination");
            ui.setPlaying(false);ui.open(Screen::Home);
        }
        {GameState flight;flight.commands[0]={5,1,0,"Go to / Garrison"};flight.commands[1]={3,0,7,"Defend / Scout"};
            ui.setPlaying(true);ui.open(Screen::PilotOrders,&flight);
            for(auto pair:{std::pair{MenuOp::AcceptOrder,FrontEffect::AcceptOrder},std::pair{MenuOp::ExecuteOrder,FrontEffect::ExecuteOrder},std::pair{MenuOp::RejectOrder,FrontEffect::RejectOrder},std::pair{MenuOp::ClearOrder,FrontEffect::ClearOrder},std::pair{MenuOp::TargetOrder,FrontEffect::TargetOrder}})
                if(choose(pair.first).effect!=pair.second)throw std::runtime_error("Pilot command menu dispatch failed");
            flight.commands[0]={};flight.commands[1]={};ui.sync(flight);
            if(choose(MenuOp::AcceptOrder).effect!=FrontEffect::None || choose(MenuOp::TargetOrder).effect!=FrontEffect::None)throw std::runtime_error("Pilot command menu enables empty slots");
            flight.moneyRequest={12,2000,0,"Requester","Requester has requested $2000"};flight.economyRevision++;ui.sync(flight);
            if(choose(MenuOp::AcceptOrder).effect!=FrontEffect::AcceptOrder || choose(MenuOp::ExecuteOrder).effect!=FrontEffect::ExecuteOrder || choose(MenuOp::RejectOrder).effect!=FrontEffect::RejectOrder)throw std::runtime_error("Pilot command menu cannot approve/decline a money request");
            ui.open(Screen::Pilots,&flight);if(choose(MenuOp::MoneyResponse,1).effect!=FrontEffect::MoneyResponse)throw std::runtime_error("Partial-balance financial approval disabled in roster");
            ui.open(Screen::PilotOrders,&flight);
            flight.complete=true;ui.sync(flight);
            if(choose(MenuOp::AcceptOrder).effect!=FrontEffect::None || choose(MenuOp::ExecuteOrder).effect!=FrontEffect::None || choose(MenuOp::RejectOrder).effect!=FrontEffect::None)throw std::runtime_error("Completed queued-request menu enables money mutation");
            flight.complete=false;ui.sync(flight);
            ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);if(ui.screen()!=Screen::Pause)throw std::runtime_error("Pilot command menu loses session context");
            ui.setPlaying(false);
        }
        Assets assets(argc>1?std::filesystem::path(argv[1]):Assets::steamArtwork());
        NativeTest::checkLobbyRoster(assets);
        NativeTest::checkLobbyActions(assets);
        NativeTest::checkLobbyAllianceUi(assets);
        NativeTest::checkWings(assets);
        NativeTest::checkAuthoredHosts(assets);
        NativeTest::checkClassicUiBatch(assets);
        NativeTest::checkClassicKeyboard(assets);
        NativeTest::checkTrainingMenu(assets);
        NativeTest::checkDockedWorkflows(assets);
        NativeTest::checkDockedViewTransitions(assets);
        NativeTest::checkOriginalLocalGame(assets);
        NativeTest::checkOriginalDeviceActions(assets);
        NativeTest::checkConnectionReports(assets);
        NativeTest::checkCustomMapUi(assets);
        NativeTest::checkMissionSetupParity(assets);
        NativeTest::checkLanDialog(assets);
        NativeTest::checkListInput(assets);
        NativeTest::checkPopupInput(assets);
        NativeTest::checkPlayerSearch(assets);
        NativeTest::checkCoreIdentity(assets);
        NativeTest::checkUiFeedback(assets);
        {
            MissionParams p;p.iLives=0;p.mmMapType=c_mmPinWheel;p.iRandomEncounters=7;p.iMaxImbalance=32767;p.fHe3Density=1.5f;
            p.bAllowFriendlyFire=true;p.bAllowEmptyTeams=true;p.bAllowAlliedRip=true;p.bExperimental=true;
            p.bAllowSupremacyPath=false;p.bAllowDefections=false;p.bAllowJoiners=false;
            auto report=Allegiance::DescribeMission(p);report.name="Actual mission";report.server="Game server";report.core="Active core";
            require(report.lives==0 && report.map==c_mmPinWheel && report.resources[0]==p.nNeutralSectorMineableAsteroids && report.resources[1]==p.nPlayerSectorMineableAsteroids && report.alliedRipcord && report.experimental && !report.supremacy,"mission details snapshot maps original IGC values");
            auto paragraphs=Allegiance::MissionDetailsParagraphs(report,{"Pinwheel","Low","Normal","Normal","Unlimited"});
            require(paragraphs.front()=="Name: Actual mission" && paragraphs[1].empty() && paragraphs[2]=="Server: Game server" && paragraphs[5]=="Lives: 0" && paragraphs[21]=="Allow Supremacy: no" && paragraphs[24]=="Experimental: yes" && paragraphs.back().empty(),"original Game details field order, blank paragraphs and allowed-death wording");
            Settings preferences;Frontend pane(preferences);pane.originalInterface(assets);pane.setPlaying(true);int requests=0;
            pane.missionDetailsService([&]{++requests;return report;});
            GameState state;state.docked=true;state.ship="Scout";state.station="Home";state.sideActive=true;pane.open(Screen::Hangar,&state);
            auto open=[&]{pane.open(Screen::Pause,&state);
                require(!pane.menuPopupView().artwork.empty() && std::none_of(pane.sceneView().artwork.begin(),pane.sceneView().artwork.end(),[](const auto& c){return c.text=="Game Details";}) && std::any_of(pane.menuPopupView().artwork.begin(),pane.menuPopupView().artwork.end(),[](const auto& c){return c.text=="Game Details";}),"main menu must retain separate resident background/top layers for chat/console ordering");
                pane.key(SDLK_I,SDL_SCANCODE_I);require(pane.menuPopupView().artwork.empty(),"dismissed main menu remains in the top layer");};
            auto item=[&](MenuOp op,int delta=1){for(size_t i=0;i<pane.view().items.size();++i){auto& entry=pane.view().items[i];if(entry.action.op==op && entry.action.delta==delta && !entry.blocked)return i;}throw std::runtime_error("Missing Game details control");};
            auto textY=[&](std::string_view text){for(const auto& c:pane.view().artwork)if(c.name=="detailsText" && c.text==text)return c.bounds.y;throw std::runtime_error("Missing Game details text: "+std::string(text));};
            open();require(pane.screen()==Screen::GameDetails && pane.view().original && !pane.view().overlay && requests==1 && !pane.wantsText(),"Escape-menu Game Details must open its authored read-only popup over hangar");
            require(pane.view().items[item(MenuOp::DetailsClose)].control=="closeButton","original MML ButtonEffectPane close control is missing");
            auto background=assets.image("dialogbkgndbmp",false);auto at=glm::floor((glm::vec2(800,600)-glm::vec2(background->width,background->height))*.5f);
            require(std::any_of(pane.view().artwork.begin(),pane.view().artwork.end(),[&](const auto& c){return c.image==background && c.bounds==glm::vec4(at,background->width,background->height);}),"Game details uses the original centered unkeyed MML background");
            for(const auto& entry:pane.view().items)require(entry.blocked || entry.action.op==MenuOp::DetailsClose || entry.action.op==MenuOp::ScrollLine || entry.action.op==MenuOp::ScrollPage || entry.action.op==MenuOp::ScrollThumb,"read-only Game details exposes settings or underlying hangar actions");
            auto nameY=textY("Name: Actual mission ");pane.activate(item(MenuOp::ScrollLine));require(textY("Name: Actual mission ")==nameY-8,"original MML scroll arrow advances eight pixels");
            pane.activate(item(MenuOp::ScrollLine,-1));require(textY("Name: Actual mission ")==nameY,"Game details scroll-up restores the first line");
            auto down=item(MenuOp::ScrollLine);auto b=pane.view().items[down].bounds;pane.pointerMove(glm::vec2(b)+glm::vec2(2));pane.pointerPress(down);pane.tick(.51);pane.pointerRelease({});
            require(textY("Name: Actual mission ")==nameY-16,"held Game details scroll arrow changes to a different step size");
            pane.activate(item(MenuOp::ScrollPage));require(std::any_of(pane.view().artwork.begin(),pane.view().artwork.end(),[](const auto& c){return c.name=="detailsText" && c.text=="Experimental: yes ";}),"Game details scrollbar cannot reach the final original fields");
            require(requests==1,"scrolling re-reads live IGC or rebuilds the mission report");
            pane.displayScale(2);require(pane.view().classicScale==2 && pane.sceneView().classicScale==2 && pane.detailsView().classicScale==2 && requests==1,"Game details restores a stale display scale or reloads its report on a DPI change");
            report.name="Changed after opening";pane.sync(state);pane.key(SDLK_PAGEUP,SDL_SCANCODE_PAGEUP);require(requests==1 && pane.screen()==Screen::GameDetails,"MML input capture refreshes or leaves its snapshot");
            require(pane.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect==FrontEffect::None && pane.screen()==Screen::Hangar,"Enter closes Game details and restores its docked parent");
            state.docked=false;open();require(pane.view().overlay && requests==2 && textY("Name: Changed after opening ")>0,"reopening Game details does not refresh its value snapshot or preserve flight");
            require(pane.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE).effect==FrontEffect::Resume,"Escape from Game details does not resume flight");
            state.docked=true;state.commandView=true;open();require(pane.view().overlay && pane.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect==FrontEffect::Resume,"docked command view is replaced by the hangar when closing Game details");
            state.commandView=false;open();require(pane.activate(item(MenuOp::DetailsClose)).effect==FrontEffect::None && pane.screen()==Screen::Hangar,"authored close button does not restore the hangar");
            open();pane.pointerMove(at+glm::vec2(3));pane.pointerPress({});pane.pointerContext({});pane.text("Ignored");
            require(pane.screen()==Screen::GameDetails && !pane.wantsText(),"picking the original dialog background or typing dismisses the read-only popup");
            require(std::none_of(pane.sceneView().artwork.begin(),pane.sceneView().artwork.end(),[](const auto& c){return c.name=="detailsText";}) && std::any_of(pane.detailsView().artwork.begin(),pane.detailsView().artwork.end(),[](const auto& c){return c.name=="detailsText";}),"Game details duplicates its report in the parent and top layers");
            pane.pointerMove({0,0});require(pane.pointerPress({}).effect==FrontEffect::None && pane.screen()==Screen::Hangar,"outside left-click must close the original MML popup over hangar");
            state.commandView=true;open();pane.pointerMove({0,0});require(pane.pointerContext({}).effect==FrontEffect::Resume,"outside right-click must close the original MML popup and resume command view");
            puts("Original Game details: IGC snapshot, exact report fields, authored popup, read-only capture, eight-pixel scrolling, top-layer retention and hangar/flight/command restoration passed");
        }
        {
            Settings preferences;Frontend pane(preferences);pane.originalInterface(assets);pane.setPlaying(true);
            GameState state;state.docked=true;state.ship="Scout";state.station="Home";state.sector="Wasp";
            TeleportChoice ship{7,"Carrier","Aegis",false,false};ship.boarding=true;ship.ship="Bomber";ship.manned=ship.turrets=2;
            std::vector<TeleportChoice> rows{ship,{7,"Home","Wasp",true,true},{8,"Outpost","Aegis",false,false}};
            pane.teleportService([&]{return rows;});pane.open(Screen::Hangar,&state);pane.teleports(rows,state);
            auto require=[&](bool value,const char* message){if(!value)throw std::runtime_error(message);};
            auto index=[&](MenuOp op,int id=0,int kind=-1){for(size_t i=0;i<pane.view().items.size();++i){auto& item=pane.view().items[i];if(item.action.op==op && item.action.value==id && (kind<0 || item.action.delta==kind))return i;}throw std::runtime_error("Missing original teleport action");};
            require(pane.view().original && pane.view().slide && pane.view().slide->screenAnchor==glm::vec2(0,.5f),"teleport must be an authored console pane over hangar");
            auto image=assets.image("teleportpanebmp",true);float y=400-image->height;
            auto background=std::find_if(pane.view().artwork.begin(),pane.view().artwork.end(),[&](auto& c){return c.image==image;});
            require(background!=pane.view().artwork.end() && background->bounds==glm::vec4(36,y,image->width,image->height),"teleport background differs from dialog.mdl's original placement");
            require(pane.activate(index(MenuOp::TeleportActivate)).effect==FrontEffect::None,"empty teleport selection must be inert");
            auto station=index(MenuOp::TeleportSelect,7,0);require(pane.pointerPress(station).effect==FrontEffect::None && pane.view().items[index(MenuOp::TeleportSelect,7,0)].selected,"teleport list selection must occur on mouse-down without travel");
            pane.pointerRelease({});require(pane.activate(index(MenuOp::TeleportActivate)).effect==FrontEffect::Resume,"current independent station must only close the pane");
            auto carrier=index(MenuOp::TeleportSelect,7,1);pane.pointerPress(carrier);pane.pointerRelease(carrier);
            auto board=pane.activate(index(MenuOp::TeleportActivate));require(board.effect==FrontEffect::Board && board.value==7,"ship/station ID collision redirects boarding");
            require(pane.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect==FrontEffect::None,"Enter must not add an unauthored teleport shortcut");
            auto jump=pane.pointerPress(index(MenuOp::TeleportSelect,8,0),true);require(jump.effect==FrontEffect::TeleportTo && jump.value==8,"original list double-click must activate on mouse-down");
            pane.pointerContext(index(MenuOp::TeleportSelect,7,0));state.passenger=true;++state.economyRevision;pane.sync(state);
            require(pane.activate(index(MenuOp::TeleportActivate)).effect==FrontEffect::Disembark,"current station must disembark a passenger without teleporting");
            std::swap(rows[1],rows[2]);++state.economyRevision;pane.sync(state);pane.key(SDLK_UP,SDL_SCANCODE_UP);
            require(pane.view().items[index(MenuOp::TeleportSelect,8,0)].selected,"keyboard navigation after reorder must follow the selected stable destination");
            rows.erase(rows.begin()+1);++state.economyRevision;pane.sync(state);require(pane.activate(index(MenuOp::TeleportActivate)).effect==FrontEffect::None,"removed teleport destination retains an actionable selection");
            pane.activate(index(MenuOp::TeleportSelect,7,0));pane.pointerPress(index(MenuOp::TeleportSelect,-1,0));require(pane.activate(index(MenuOp::TeleportActivate)).effect==FrontEffect::None,"blank list space must clear selection");
            for(int i=0;i<80;++i)rows.push_back({100+i,"Distant base","Aegis",false,true});++state.economyRevision;pane.sync(state);
            for(int i=0;i<12;++i)pane.key(SDLK_PAGEDOWN,SDL_SCANCODE_PAGEDOWN);
            require(pane.view().items[index(MenuOp::TeleportSelect,179,0)].selected,"original page keys cannot reach the entire teleport list");
            for(const auto& item:pane.view().items)if(item.action.op==MenuOp::TeleportSelect && item.action.value>=0)require(item.bounds.y>=y+39 && item.bounds.y+item.bounds.w<=y+184,"teleport row hit regions escape list clipping");
            auto selected=index(MenuOp::TeleportSelect,179,0);auto b=pane.view().items[selected].bounds;
            pane.pointerMove(glm::vec2(b)+glm::vec2(b.z,b.w)*.5f+menuSlideOffset(pane.view(),{800,600},uiTime()));require(pane.pointerWheel(1),"teleport list wheel navigation is missing");
            require(pane.view().items[index(MenuOp::TeleportSelect,178,0)].selected,"teleport wheel must move and reveal the selection");
            state.docked=false;++state.economyRevision;pane.sync(state);require(pane.activate(index(MenuOp::TeleportActivate)).effect==FrontEffect::None,"flying player can still teleport");
            state.docked=true;state.passenger=false;++state.economyRevision;pane.sync(state);
            for(int scale:{1,2,3})for(glm::ivec2 size:{glm::ivec2(800*scale,600*scale),glm::ivec2(1200*scale+1,900*scale+1)}){
                auto view=pane.view();view.classicScale=scale;view.slide->started=uiTime()-1;
                auto close=index(MenuOp::Resume);auto bounds=view.items[close].bounds;auto layout=originalMenuLayout(size,scale);
                auto p=layout.pixel(glm::vec2(bounds)+glm::vec2(bounds.z,bounds.w)*.5f+menuSlideOffset(view,size,uiTime()));
                require(menuHit(view,size,1,p)==close,"teleport art/hits lose left anchoring at integer scales");
                require(std::abs((layout.pixel(glm::vec2(36,y)+menuSlideOffset(view,size,uiTime()))).x-36*scale)<.01f,"teleport console slides relative to centered menu instead of screen edge");
                view.slide->started=uiTime()-.25;
                p=layout.pixel(glm::vec2(bounds)+glm::vec2(bounds.z,bounds.w)*.5f+menuSlideOffset(view,size,uiTime()));require(menuHit(view,size,1,p)==close,"half-open teleport panel hit region does not follow its image");
            }
            require(pane.activate(index(MenuOp::Teleport)).effect==FrontEffect::Resume,"hangar teleport button cannot toggle the panel off");
            pane.open(Screen::Hangar,&state);require(pane.view().slide && pane.view().slide->closing && std::none_of(pane.view().items.begin(),pane.view().items.end(),[](auto& i){return i.action.op==MenuOp::TeleportSelect;}),"closing panel keeps active destination hit regions");
            puts("Original teleport UI: authored placement, mouse-down and double-click, stable typed IDs, live changes, full list access, clipped hits and integer-scale slide anchors passed");
        }
        {
            // Frozen ListPaneOld::RemoveItemByIdx/SetSelItemByIdx, independent
            // of the shared policy: all removal sets and initial selections.
            const std::array<int,8> originalCards{10,20,30,40,50,60,70,80};
            for(unsigned mask=0;mask<256;++mask)for(int selected=-1;selected<8;++selected){
                std::vector<int> remaining(originalCards.begin(),originalCards.end());int index=selected;
                for(size_t i=0;i<originalCards.size();++i)if(mask&(1u<<i)){
                    auto found=std::find(remaining.begin(),remaining.end(),originalCards[i]);int removed=int(found-remaining.begin());remaining.erase(found);
                    if(removed<=index)--index;index=std::clamp(index,-1,int(remaining.size())-1);
                }
                auto present=[&](int id){return std::find(remaining.begin(),remaining.end(),id)!=remaining.end();};
                int actual=Allegiance::InvestmentSelectionAfterRemoval(originalCards,selected<0?-1:originalCards[selected],present);
                if(actual!=(index<0?-1:remaining[index]))throw std::runtime_error("Live research removal selection differs from original list");
            }
            for(int price:{1,9,10,100,999,10000})for(int paid:{0,1,price/2,price-1,price})for(int wallet:{0,1,9,100,100000})for(bool partial:{false,true}){
                int expected=std::min(price-paid,wallet);
                if(partial){expected=std::min(int(price*.1),wallet);while(paid+expected>=price)--expected;expected=std::max(0,expected);}
                if(Allegiance::InvestmentAmount(price,paid,wallet,partial)!=std::max(0,expected))throw std::runtime_error("Investment payment differs from original full/10%-without-completion policy");
            }
            for(unsigned required=0;required<32;++required){
                unsigned original=(required&~c_aabmBuildable)?required&~c_aabmBuildable:required;
                if(Allegiance::InvestmentAsteroidMask(required)!=original)throw std::runtime_error("Investment sector hint subtype priority differs from original");
            }
            Settings preferences;Frontend pane(preferences);pane.originalInterface(assets);pane.setPlaying(true);
            GameState state;state.economy=true;state.money=999;state.docked=true;state.ship="Scout";state.station="Home";state.sector="Wasp";
            std::vector<ResearchChoice> choices;
            for(int group=0;group<6;++group)for(int n=0;n<15;++n){ResearchChoice row;row.id=group*100+n;row.group=group;row.type=OT_development;if(n==3)row.buildMask=c_aabmBuildable|c_aabmSpecial;row.name="Research "+std::to_string(row.id);row.model=n?"missing-card":"acs38";row.price=100;row.invested=n==0?91:0;row.bought=n==0?91:0;row.available=row.canBuy=row.predecessorReady=n!=2;row.complete=n==1;row.description="A description of this development that spans several original small-font lines.";choices.push_back(row);}
            int reads=0;pane.researchService([&]{++reads;return choices;});pane.open(Screen::Hangar,&state);pane.open(Screen::Research,&state);
            auto require=[](bool value,const char* text){if(!value)throw std::runtime_error(text);};
            auto index=[&](MenuOp op,int value=0){for(size_t i=0;i<pane.view().items.size();++i){auto& row=pane.view().items[i];if(row.action.op==op && row.action.value==value)return i;}throw std::runtime_error("Original investment control missing");};
            auto rowCount=[&]{return std::count_if(pane.view().items.begin(),pane.view().items.end(),[](auto& c){return c.action.op==MenuOp::ResearchSelect;});};
            require(reads==1 && pane.view().original && pane.view().slide && pane.view().slide->screenAnchor==glm::vec2(1,.5f),"F5 must compile the original right-side purchases pane once on entry");
            auto card=assets.image("iacs38bmp",true),blank=assets.image("conloadoutitembmp",true);
            auto contains=[&](const auto& image){return std::any_of(pane.view().artwork.begin(),pane.view().artwork.end(),[&](auto& c){return c.image==image;});};
            require(contains(card) && contains(blank),"Research cards discard real model artwork or lose the missing-model fallback");
            require(rowCount()==6 && pane.view().items[index(MenuOp::ResearchSelect,0)].selected,"default purchases tab, selection or six-card viewport differs");
            auto partial=pane.pointerContext(index(MenuOp::ResearchSelect,0));require(partial.effect==FrontEffect::Invest && partial.value==0 && partial.command==8,"right-click must select and invest 10% without completing");
            auto full=pane.pointerPress(index(MenuOp::ResearchSelect,0),true);require(full.effect==FrontEffect::Invest && full.command==9,"double-click investment must occur on mouse-down");
            pane.pointerRelease(index(MenuOp::ResearchSelect,0),true);pane.pointerPress(index(MenuOp::ResearchSelect,3));require(pane.pointerRelease(index(MenuOp::ResearchSelect,3)).effect==FrontEffect::None,"single selection invests on release");
            require(pane.activate(index(MenuOp::ResearchActivate)).command==100,"INVEST does not fund selected bucket");
            pane.pointerHover(index(MenuOp::ResearchSelect,3));require(pane.investmentAsteroid()==(c_aabmBuildable|c_aabmSpecial),"station hover omits construction-site hints");auto info=std::find_if(pane.view().artwork.begin(),pane.view().artwork.end(),[](auto& c){return c.name=="investmentInfoName";});require(info!=pane.view().artwork.end() && info->text=="Research 3" && info->font=="smallBoldFont","investment hover details differ from original");
            pane.pointerHover({});require(std::none_of(pane.view().artwork.begin(),pane.view().artwork.end(),[](auto& c){return c.name=="investmentInfoName";}),"leaving card retains stale info");require(!pane.investmentAsteroid(),"leaving purchase card retains construction-site hints");
            auto count=reads;pane.sync(state);require(reads==count,"unchanged investment view re-extracts all buckets every frame");
            choices[3].invested=60;choices[3].bought=60;choices[3].progress=30;++state.economyRevision;pane.sync(state);require(reads==count+1 && pane.activate(index(MenuOp::ResearchActivate)).command==40,"live bucket update loses selection or payment freshness");
            auto savedChoices=choices;
            auto removeCard=[&](int id){std::erase_if(choices,[&](const auto& row){return row.id==id;});++state.economyRevision;pane.sync(state);};
            removeCard(3);require(pane.view().items[index(MenuOp::ResearchSelect,1)].selected,"terminated purchase selects next card instead of original previous card");
            removeCard(1);require(pane.view().items[index(MenuOp::ResearchSelect,0)].selected,"consecutive purchase removals lose original preceding selection");
            removeCard(0);require(std::none_of(pane.view().items.begin(),pane.view().items.end(),[](const auto& row){return row.action.op==MenuOp::ResearchSelect && row.selected;}) && !pane.view().items[index(MenuOp::ResearchActivate)].enabled,"removing first selected purchase must clear selection and disable INVEST");
            choices=savedChoices;++state.economyRevision;pane.sync(state);require(!pane.view().items[index(MenuOp::ResearchActivate)].enabled,"newly available cards invent a selection after original removal cleared it");
            pane.pointerHover({});require(!pane.view().items[index(MenuOp::ResearchActivate)].enabled,"research repaint invents a cleared selection");
            pane.activate(index(MenuOp::ResearchTab,0));require(pane.view().items[index(MenuOp::ResearchSelect,0)].selected,"explicit tab reset does not restore first-card selection after removal");
            pane.activate(index(MenuOp::ResearchFilter,0));require(pane.view().items[index(MenuOp::ResearchSelect,0)].selected,"Show All must reset the current tab to its first original card");require(pane.view().items[index(MenuOp::ResearchSelect,2)].enabled,"Show All must expose locked developments for inspection");pane.activate(index(MenuOp::ResearchSelect,2));require(pane.activate(index(MenuOp::ResearchActivate)).effect==FrontEffect::None,"Show All permits purchasing unavailable technology");
            pane.activate(index(MenuOp::ResearchFilter,1));require(std::none_of(pane.view().items.begin(),pane.view().items.end(),[](auto& c){return c.action.op==MenuOp::ResearchSelect && c.action.value==1;}),"Show Complete unchecked retains complete bucket");
            pane.activate(index(MenuOp::ResearchTab,1));require(pane.view().items[index(MenuOp::ResearchSelect,100)].selected,"tab does not select its first card");
            auto b=pane.view().items[index(MenuOp::ResearchSelect,100)].bounds;pane.pointerMove(glm::vec2(b)+glm::vec2(10,10)+menuSlideOffset(pane.view(),{800,600},uiTime()));require(pane.pointerWheel(-1),"original purchases wheel paging missing");
            require(std::none_of(pane.view().items.begin(),pane.view().items.end(),[](auto& c){return c.action.op==MenuOp::ResearchSelect && c.action.value==100;}),"purchase wheel must page cards without changing selection");
            require(pane.activate(index(MenuOp::ResearchActivate)).value==100,"wheel scrolling changes selected purchase");
            for(auto& c:pane.view().items)if(c.action.op==MenuOp::ResearchSelect)require(c.bounds.y>=136 && c.bounds.y+c.bounds.w<=568 && c.bounds.w==72,"purchase rows clip at fractional card boundaries");
            pane.activate(index(MenuOp::ResearchFilter,1));require(pane.view().items[index(MenuOp::ResearchSelect,100)].selected && rowCount()==6,"Show Complete must reset selection and scroll to the first original card");
            for(auto& row:choices)if(row.group==2)row.available=row.canBuy=row.predecessorReady=false;
            ++state.economyRevision;pane.sync(state);pane.activate(index(MenuOp::ResearchTab,2));
            require(pane.view().items[index(MenuOp::ResearchSelect,200)].selected,"Show All cannot inspect a locked technology path");
            pane.activate(index(MenuOp::ResearchFilter,0));
            require(pane.view().items[index(MenuOp::ResearchTab,0)].selected && pane.view().items[index(MenuOp::ResearchSelect,0)].selected,"closing an unavailable research path does not return to the original Purchases tab");
            require(pane.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect==FrontEffect::None,"F5 must not invent Enter-to-invest");
            for(int scale:{1,2,3})for(glm::ivec2 size:{glm::ivec2(800*scale,600*scale),glm::ivec2(1200*scale+1,900*scale+1)}){
                auto view=pane.view();view.classicScale=scale;view.slide->started=uiTime()-1;auto layout=originalMenuLayout(size,scale);
                auto p=layout.pixel(glm::vec2(310,127)+menuSlideOffset(view,size,uiTime()));require(std::abs(p.x-(size.x/scale*scale-490*scale))<.01f,"investment pane is centered rather than right anchored");
                auto close=index(MenuOp::Resume);auto bounds=view.items[close].bounds;p=layout.pixel(glm::vec2(bounds)+glm::vec2(bounds.z,bounds.w)*.5f+menuSlideOffset(view,size,uiTime()));require(menuHit(view,size,1,p)==close,"investment art and hit transforms differ at integer scale");
            }
            pane.open(Screen::Hangar,&state);require(pane.view().slide && pane.view().slide->closing && pane.view().slide->screenAnchor==glm::vec2(1,.5f),"closing investment pane loses original animation");
            state.docked=false;pane.open(Screen::Research,&state);require(pane.view().overlay && std::none_of(pane.view().items.begin(),pane.view().items.end(),[](auto& c){return c.control=="launchButtonPane";}),"flight investment pane adds a hangar background");
            require(pane.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE).effect==FrontEffect::Resume,"investment Escape does not return to flight");
            require(pane.freshInvestmentPane() && pane.releaseInvestmentPointer() && !pane.freshInvestmentPane() && !pane.releaseInvestmentPointer(),"original fresh investment pointer must release once without hiding the pane");
            pane.closeConsoleToFlight();require(pane.closingConsole() && pane.view().overlay && pane.view().items.empty(),"closing flight investment pane must animate without retaining active controls");
            pane.error("Stationary acknowledgment");
            require(pane.view().slide && pane.view().slide->artworkEnd<pane.view().artwork.size() && !pane.view().slide->movesItem(0),"closing console animation captures a later modal's art or input");
            pane.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
            pane.open(Screen::Research,&state);require(!pane.closingConsole() && pane.freshInvestmentPane() && pane.view().slide && pane.view().slide->displacement(uiTime()).x>450,"reopening a closing flight console must restart the original entrance");
            puts("Original investments: six tabs/cards, filters, hover, full/partial payments, live refresh, whole-card paging, right-side integer anchors and flight overlay passed");
        }
        {
            SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.enemies=0;
            Game game(assets.root(),[&](auto& name,auto&){auto model=assets.model(name);return ModelHandle{1,model->radius,model->frames,model->lights};},0,0,options);
            auto rows=game.researchChoices();auto* side=game.playerShip()->GetSide();size_t i=0;int locked=0,available=0;
            for(auto* link=side->GetBuckets()->first();link;link=link->next(),++i){auto* bucket=link->data();if(i>=rows.size())throw std::runtime_error("investment metadata omitted original bucket");auto& row=rows[i];
                if(row.id!=bucket->GetObjectID() || row.type!=bucket->GetBucketType() || row.group!=bucket->GetGroupID() || row.name!=bucket->GetName() || row.model!=bucket->GetModelName() || row.canBuy!=side->CanBuy(bucket) || row.timeToBuild!=bucket->GetTimeToBuild())throw std::runtime_error("investment metadata differs from IGC");
                auto prerequisites=Allegiance::InvestmentPrerequisites(bucket);if(row.prerequisites.size()!=prerequisites.size())throw std::runtime_error("investment prerequisite extraction differs");for(size_t j=0;j<prerequisites.size();++j)if(row.prerequisites[j]!=prerequisites[j]->GetName())throw std::runtime_error("investment dependency order differs");
                locked+=!row.canBuy;available+=row.available;
            }
            if(i!=rows.size() || !locked || !available)throw std::runtime_error("investment fixture lacks locked/available technology");
            auto buy=std::find_if(rows.begin(),rows.end(),[](auto& row){return row.available && row.price>1;});if(buy==rows.end())throw std::runtime_error("investment fixture has no fundable bucket");
            auto before=game.state().money;int amount=Allegiance::InvestmentAmount(buy->price,buy->invested,before,true);
            if(amount<=0 || !game.invest(buy->id,amount))throw std::runtime_error("original partial purchase does not reach authority");
            auto updated=game.researchChoices();auto after=std::find_if(updated.begin(),updated.end(),[&](auto& row){return row.id==buy->id;});
            if(game.state().money!=before-amount || after==updated.end() || after->invested!=buy->invested+amount || after->bought>=100)throw std::runtime_error("original partial purchase loses credits or completes prematurely");
            auto* mission=game.playerShip()->GetMission();auto* viewer=game.playerShip()->GetSide();
            for(unsigned mask=1;mask<32;++mask){
                std::vector<int> expected;auto required=Allegiance::InvestmentAsteroidMask(mask);
                for(auto* l=mission->GetAsteroids()->first();l;l=l->next())if(l->data()->SeenBySide(viewer) && l->data()->HasCapability(required))expected.push_back(l->data()->GetCluster()->GetObjectID());
                std::sort(expected.begin(),expected.end());expected.erase(std::unique(expected.begin(),expected.end()),expected.end());
                auto actual=game.investmentSectors(mask);if(!std::equal(actual.begin(),actual.end(),expected.begin(),expected.end()))throw std::runtime_error("Investment sector hints leak unknown rocks or omit known sites");
            }
            auto* rock=mission->GetAsteroids()->first()->data();auto required=unsigned(rock->GetCapabilities());
            for(auto* l=mission->GetAsteroids()->first();l;l=l->next())l->data()->SetSideVisibility(viewer,false);
            if(!game.investmentSectors(required).empty())throw std::runtime_error("Investment hint cache retains unseen rocks");
            rock->SetSideVisibility(viewer,true);auto visible=game.investmentSectors(required);
            if(visible.size()!=1 || visible[0]!=rock->GetCluster()->GetObjectID())throw std::runtime_error("Newly discovered construction site does not update hints");
            auto* resident=visible.data();if(game.investmentSectors(required).data()!=resident)throw std::runtime_error("Unchanged construction hints are rebuilt");
            rock->Terminate();if(!game.investmentSectors(required).empty())throw std::runtime_error("Destroyed construction site remains highlighted");
            printf("Original investment metadata: %zu IGC buckets, %d locked, %d available, prerequisites and real partial-payment accounting passed\n",rows.size(),locked,available);
        }
        {
            Settings preferences;Frontend pane(preferences);pane.originalInterface(assets);pane.setPlaying(true);
            GameState state;state.docked=true;state.ship="Scout";state.station="Home";state.sector="Wasp";
            pane.open(Screen::Hangar,&state);pane.teleports({{3,"Outpost","Aegis",false,true}},state);
            auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
            pane.error("Teleportation failed.");
            require(pane.hasMessageBox() && !pane.wantsText() && pane.view().notice.empty(),"classic errors must use a modal instead of a status label");
            require(pane.view().slide && pane.view().slide->artworkEnd<pane.view().artwork.size(),"modal artwork is part of the underlying teleport animation");
            auto ok=[&]{for(size_t i=0;i<pane.view().items.size();++i)if(pane.view().items[i].action.op==MenuOp::MessageDismiss)return i;throw std::runtime_error("Modal OK missing");};
            for(size_t i=0;i<pane.view().items.size();++i)if(i!=ok())require(pane.view().items[i].blocked && pane.activate(i).effect==FrontEffect::None,"modal allows an underlying control to activate");
            auto caption=std::find_if(pane.view().artwork.begin(),pane.view().artwork.end(),[](auto& c){return c.name=="messageText";});
            require(caption!=pane.view().artwork.end() && caption->font=="largeFont" && caption->text=="Teleportation failed.","message box loses original font or text");
            auto im=assets.image("dialogbkgndbmp",true);auto origin=glm::floor((glm::vec2(800,600)-glm::vec2(im->width,im->height))*.5f);
            require(glm::vec2(caption->bounds)==origin+glm::vec2(50,50) && caption->bounds.z==403,"message text differs from original authored geometry");
            pane.text("unwanted chat");require(pane.chatText().empty(),"modal leaks text into an underlying entry");
            pane.pointerPress({});require(pane.hasMessageBox(),"outside click dismisses a modal message");
            pane.pointerWheel(-1);pane.pointerContext(ok());pane.key(SDLK_F8,SDL_SCANCODE_F8);pane.key(SDLK_SPACE,SDL_SCANCODE_SPACE);
            require(pane.hasMessageBox(),"modal leaks wheel, right-click or non-dialog keys");
            ++state.economyRevision;pane.sync(state);require(pane.hasMessageBox(),"live game refresh discards the modal");
            for(int scale:{1,2,3})for(glm::ivec2 size:{glm::ivec2(800*scale,600*scale),glm::ivec2(1200*scale+1,900*scale+1)}){
                auto view=pane.view();view.classicScale=scale;view.slide->off={-328,0};view.slide->closing=true;view.slide->started=uiTime()-1;
                auto b=view.items[ok()].bounds;auto p=originalMenuLayout(size,scale).pixel(glm::vec2(b)+glm::vec2(b.z,b.w)*.5f);
                require(!view.slide->movesItem(ok()) && menuHit(view,size,1,p)==ok(),"centered modal hits follow the sliding console");
            }
            pane.pointerPress(ok());pane.pointerRelease({});require(pane.hasMessageBox(),"OK activates after release outside its capture");
            pane.pointerPress(ok());pane.pointerRelease(ok());require(!pane.hasMessageBox() && pane.screen()==Screen::Teleport,"modal acknowledgment loses its parent console");
            pane.error("Second failure.");require(pane.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE).effect==FrontEffect::None && !pane.hasMessageBox(),"single-button Escape must acknowledge without leaving the parent");
            state.docked=false;pane.open(Screen::Pause,&state);pane.error("Flight failure.",true);
            require(pane.view().overlay && pane.view().items.size()==1,"flight error adds a menu/background underneath the original modal");
            require(pane.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect==FrontEffect::Resume && !pane.hasMessageBox(),"flight acknowledgment must resume the previous flight view");
            pane.open(Screen::Online);pane.connecting();require(pane.hasMessageBox() && std::all_of(pane.view().items.begin(),pane.view().items.end(),[](auto& row){return row.blocked;}),"connecting dialog must block the LAN dialog without adding an OK button");
            pane.key(SDLK_RETURN,SDL_SCANCODE_RETURN);pane.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);require(pane.hasMessageBox(),"connecting dialog can be dismissed before connection completes");
            pane.connectionFinished();require(!pane.hasMessageBox(),"connected mission retains its pending dialog");
            pane.connecting();pane.error("Connection timed out.");pane.connectionFinished();require(pane.hasMessageBox(),"pending-dialog cleanup hides a connection failure");pane.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
            puts("Original message boxes: authored art/font/geometry, input capture, live refresh, console-independent animation, click cancellation and flight return passed");
        }
        {
            Settings prefs;prefs.controller.mapButtons.push_back({});Frontend classic(prefs);classic.originalInterface(assets);
            for(auto [platform,expected]:{std::pair{1.f,1}, {1.25f,1}, {1.5f,1}, {1.5001f,2}, {1.75f,2}, {2.f,2}, {2.5f,2}, {2.5001f,3}, {3.f,3}, {0.f,1}, {100.f,8}}){
                classic.displayScale(platform);
                if(classic.view().classicScale!=expected || integerDisplayScale(platform)!=expected)throw std::runtime_error("Platform classic DPI scale or midpoint rounding differs");
            }
            if(integerDisplayScale(std::numeric_limits<float>::quiet_NaN())!=1 || integerDisplayScale(std::numeric_limits<float>::infinity())!=1)throw std::runtime_error("Invalid display scale does not fall back to 1x");
            classic.displayScale(1);
            for(glm::ivec2 size:{glm::ivec2(800,600),{1280,720},{1920,1080},{2560,1440},{3841,2161},{640,480}}){
                if(originalMenuLayout(size).scale!=1)throw std::runtime_error("Classic UI automatically enlarges or shrinks on resize");
                for(int requested=1;requested<=8;++requested){
                    auto layout=originalMenuLayout(size,requested);auto origin=layout.pixel({0,0});
                    if(layout.scale!=int(layout.scale) || layout.scale<1 || layout.scale>requested || origin!=glm::floor(origin))throw std::runtime_error("Classic HiDPI layout loses integer pixels");
                    auto local=glm::vec2(123,247);auto p=layout.pixel(local);
                    if(glm::length(layout.local(p)-local)>.001f)throw std::runtime_error("Classic UI hit transform differs from pixel transform");
                    if(size.x>=800 && size.y>=600 && (layout.pixel({800,600}).x>size.x || layout.pixel({800,600}).y>size.y))throw std::runtime_error("Classic integer scale exceeds drawable");
                }
            }
            auto index=[&](MenuOp op){for(size_t i=0;i<classic.view().items.size();++i)if(classic.view().items[i].action.op==op)return i;throw std::runtime_error("Missing classic UI action");};
            prefs.classicUiScale=3;classic.open(Screen::Training);
            classic.displayScale(1.5f);if(classic.view().classicScale!=3)throw std::runtime_error("Explicit classic UI scale does not override display DPI");
            for(glm::ivec2 size:{glm::ivec2(1600,1200),{2401,1801},{2560,1440}}){
                auto i=index(MenuOp::TrainingStart);const auto& item=classic.view().items[i];auto layout=originalMenuLayout(size,classic.view().classicScale);
                auto hit=menuHit(classic.view(),size,2.5f,layout.pixel(glm::vec2(item.bounds)+glm::vec2(item.bounds.z,item.bounds.w)*.5f));
                if(!hit || *hit!=i)throw std::runtime_error("Integer HiDPI menu pointer cannot hit rendered button");
            }
            for(int screen=0;screen<=int(Screen::Help);++screen){classic.open(Screen(screen));
                if(!classic.view().original)throw std::runtime_error("Default original frontend silently falls back to experimental interface");
            }
            classic.open(Screen::Home);classic.open(Screen::Online);
            require(!classic.view().artwork.empty() && classic.view().items[index(MenuOp::LanJoin)].bounds.z<=200 && !classic.view().items[index(MenuOp::LanJoin)].enabled,"empty LAN dialog lost its disabled original Join control");
            require(!classic.wantsText() && std::any_of(classic.view().artwork.begin(),classic.view().artwork.end(),[](const auto& c){return c.text=="Searching...";}),"empty LAN dialog invents an address row instead of original Searching...");
            classic.open(Screen::Chat);if(!classic.wantsText())throw std::runtime_error("Classic chat does not request SDL text input");classic.text("Classic chat test");
            if(classic.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect!=FrontEffect::SendChat || classic.chatText()!="Classic chat test")throw std::runtime_error("Classic command adapter intercepts chat text or Enter");
            classic.open(Screen::Home);classic.open(Screen::Help);
            if(classic.helpView().canvas!=glm::ivec2(1300,700) || classic.helpView().artwork.empty() || classic.screen()!=Screen::Home)throw std::runtime_error("Original Help image rescaled or replaced its underlying screen");
            classic.toggleHelp();if(classic.screen()!=Screen::Home)throw std::runtime_error("Mapped Help cannot close original Help");
            auto file=std::filesystem::temp_directory_path()/"allegiance-classic-scale-check.conf";prefs.save(file);auto saved=Settings::load(file);std::filesystem::remove(file);
            if(saved.classicUiScale!=3 || saved.uiScale!=prefs.uiScale)throw std::runtime_error("Classic integer scale does not persist independently");
            puts("Classic UI: native pixels, integer HiDPI, pointer alignment, all default screen routes and original Help/LAN checked");
        }
        {
            struct Clock {int seconds;bool countdown;const char* text;};
            for(auto sample:{Clock{-3601,true,"-1:00"},Clock{-59,true,"-0:00"},Clock{-59,false,"0:00"},Clock{-100,false,"0:00"},Clock{-101,false,"days"},Clock{3599,false,"0:59"},Clock{3600,false,"1:00"},Clock{359999,false,"99:59"},Clock{360000,false,"days"}}){char text[32];Allegiance::FormatGameListTime(text,sizeof(text),sample.seconds,sample.countdown);require(std::string_view(text)==sample.text,"Games List time formatting differs from WinTrek");}
            puts("Games List clock: original countdown, negative/skew and long-duration labels passed");
        }
        {
            Settings prefs;Frontend games(prefs);games.originalInterface(assets);games.open(Screen::Games);
            auto find=[&](MenuOp op,int value=0){for(size_t i=0;i<games.view().items.size();++i)if(games.view().items[i].action.op==op && games.view().items[i].action.value==value)return i;throw std::runtime_error("Missing games-list action");};
            auto first=[&]{if(std::none_of(games.view().items.begin(),games.view().items.end(),[](const auto& c){return c.action.op==MenuOp::GameSelect && c.selected;}))games.key(SDLK_DOWN,SDL_SCANCODE_DOWN);for(int i=0;i<2;++i)games.key(SDLK_PAGEUP,SDL_SCANCODE_PAGEUP);};
            auto last=[&]{for(int i=0;i<2;++i)games.key(SDLK_PAGEDOWN,SDL_SCANCODE_PAGEDOWN);};
            require(games.view().original && !games.view().items[find(MenuOp::GameJoin)].enabled,"empty original games list cannot join or show fixture rows");
            require(std::none_of(games.view().items.begin(),games.view().items.end(),[](const auto& i){return i.action.op==MenuOp::GameSelect || i.action.op==MenuOp::ScenarioSelect;}),"local scenarios are absent from the public games list");
            std::vector<Net::ServerListing> listings;
            for(int i=0;i<24;++i){SessionOptions s;s.scenario=Scenario::Conquest;s.rules.developments=i%2;Net::ServerInfo info{};info.session=Net::session(s);info.players=i;info.flags=Net::ServerWaiting|Net::ServerJoinable;Net::text(info.name,"Game "+std::to_string(i));Net::text(info.server,"Native");Net::text(info.core,"PCore014");listings.push_back({"127.0.0.1:"+std::to_string(28000+i),info});}
            games.gameListings(listings);games.key(SDLK_DOWN,SDL_SCANCODE_DOWN);require(games.view().items[find(MenuOp::GameJoin)].enabled,"keyboard selects live game");
            auto gameRow=find(MenuOp::GameSelect,1);games.pointerPress(gameRow);
            require(games.view().items[find(MenuOp::GameSelect,1)].selected && !games.pointerCaptured() && games.pointerRelease(gameRow).effect==FrontEffect::None,"games ListPane does not select on down without capture");
            require(games.pointerPress(find(MenuOp::GameSelect,1),true).effect==FrontEffect::Connect && games.serverAddress()=="127.0.0.1:28022","games ListPane double-click does not join on down");
            require(games.pointerRelease(find(MenuOp::GameSelect,1),true).effect==FrontEffect::None,"games list release joins twice");first();
            auto join=games.activate(find(MenuOp::GameJoin));require(join.effect==FrontEffect::Connect && games.serverAddress()=="127.0.0.1:28023","original default sorts by population and joins real endpoint");
            std::reverse(listings.begin(),listings.end());games.gameListings(listings);require(games.activate(find(MenuOp::GameJoin)).effect==FrontEffect::Connect && games.serverAddress()=="127.0.0.1:28023","server refresh preserves endpoint selection across list reordering");
            last();require(games.activate(find(MenuOp::GameJoin)).effect==FrontEffect::Connect && games.serverAddress()=="127.0.0.1:28000","scrollable original list exposes last game");
            auto edit=[&](std::string_view name)->const OriginalControl&{auto face=std::find_if(games.view().artwork.begin(),games.view().artwork.end(),[&](const auto& c){return c.name==name && c.widget==OriginalControl::Widget::Edit;});require(face!=games.view().artwork.end(),"authored edit face is absent");auto text=std::find_if(games.view().artwork.begin(),games.view().artwork.end(),[&](const auto& c){return c.textMode==OriginalControl::TextMode::Edit && c.bounds==face->bounds;});require(text!=games.view().artwork.end(),"authored edit uses an added underscore/painter instead of original edit policy");return *text;};
            games.activate(find(MenuOp::GameFilter));require(games.wantsText(),"original Filter opens text focus");require(edit("gameNameEditPane").text.empty() && edit("gameNameEditPane").editFocused && edit("gameNameEditPane").font=="largeFont","empty original edit loses its font or end-cursor focus");
            games.text("gAmE 2");auto typed=edit("gameNameEditPane").editLastTyped;require(edit("gameNameEditPane").text=="gAmE 2","edit stores an underscore as part of the authored text");
            games.key(SDLK_LEFT,SDL_SCANCODE_LEFT);games.key(SDLK_DELETE,SDL_SCANCODE_DELETE);require(edit("gameNameEditPane").text=="gAmE 2" && edit("gameNameEditPane").editLastTyped==typed,"original append-only EditPane gained caret movement/delete or blink resets");
            games.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);require(edit("gameNameEditPane").text=="gAmE " && edit("gameNameEditPane").editLastTyped==typed,"Backspace changes original end-cursor blink phase");games.text("2");games.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
            first();require(games.activate(find(MenuOp::GameJoin)).effect==FrontEffect::Connect && games.serverAddress()=="127.0.0.1:28023","game-name filter matches case insensitively");
            games.activate(find(MenuOp::GameFilter));for(int i=0;i<6;++i)games.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);games.text("absent");games.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            require(!games.wantsText() && games.view().items[find(MenuOp::GameJoin)].enabled,"cancel restores committed filters and text focus");
            games.activate(find(MenuOp::GameFilter));
            {
                auto caption=[&](int field){auto b=games.view().items[find(MenuOp::GameFilterCombo,field)].bounds;for(const auto& c:games.view().artwork)if(c.kind==OriginalControl::Kind::Text && c.visibleBounds()==b)return c.text;throw std::runtime_error("Missing original filter caption");};
                auto choice=[&](int field,int value){for(size_t i=0;i<games.view().items.size();++i){const auto& item=games.view().items[i];if(item.action.op==MenuOp::GameFilterChoice && item.action.value==field && item.action.delta==value)return i;}throw std::runtime_error("Missing original filter popup choice");};
                auto selected=[&](int field,int value){return games.view().items[choice(field,value)].selected;};
                games.displayScale(2);const glm::ivec2 drawable{2560,1440};const auto layout=originalMenuLayout(drawable,2);
                for(int field=0;field<11;++field){
                    auto combo=find(MenuOp::GameFilterCombo,field);auto bounds=games.view().items[combo].bounds;auto anchor=glm::vec2(bounds)+glm::vec2(3);
                    games.pointerMove(anchor,drawable);auto hit=menuHit(games.view(),drawable,1,layout.pixel(anchor));require(hit==combo,"integer-scaled filter combo cannot be hit");games.pointerPress(hit);
                    require(!games.wantsText() && games.view().items[find(MenuOp::GameFilterAccept)].blocked && games.view().items[find(MenuOp::GameJoin)].blocked,"filter popup does not capture dialog and Games List");
                    auto yes=games.view().items[choice(field,1)],no=games.view().items[choice(field,0)],any=games.view().items[choice(field,2)];
                    require(yes.selected && yes.bounds.y<anchor.y && no.bounds.y-yes.bounds.y==assets.font("smallFont")->height+2 && any.bounds.y-no.bounds.y==no.bounds.y-yes.bounds.y,"filter popup loses original Yes/No/DontCare order, first highlight, font row spacing or pointer anchor");
                    games.pointerRelease(menuHit(games.view(),drawable,1,layout.pixel(anchor)));require(games.view().items[choice(field,1)].selected,"opening release selects a filter from behind its popup");
                    games.text("ignored");games.key(SDLK_UP,SDL_SCANCODE_UP);require(selected(field,2),"filter popup Up fails to wrap to DontCare");games.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
                    require(games.wantsText() && !games.view().items[find(MenuOp::GameFilterAccept)].blocked && caption(field)=="DontCare","Enter accepts whole filter dialog instead of popup choice or loses edit focus");
                    games.activate(find(MenuOp::GameFilterCombo,field));require(selected(field,2),"authored combo forgets its independent menu highlight");games.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
                    require(games.wantsText() && caption(field)=="DontCare","Escape closes filter dialog rather than only the dropdown");
                }
                games.activate(find(MenuOp::GameFilterCombo,0));auto row=choice(0,0);auto rowBounds=games.view().items[row].bounds;games.pointerMove(glm::vec2(rowBounds)+glm::vec2(3),drawable);games.pointerHover(row);
                row=choice(0,0);games.pointerRelease(row);require(caption(0)=="No" && games.wantsText(),"dragging into a filter row and releasing fails to select it");
                games.activate(find(MenuOp::GameFilterCombo,0));require(selected(0,0),"reopening combo loses last hovered/selected menu row");
                games.pointerMove({0,0},drawable);games.pointerPress({});require(games.wantsText() && caption(0)=="No","outside click cancels dialog or falls through parent instead of dismissing popup");
                games.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);require(games.view().items[find(MenuOp::GameJoin)].enabled,"discarding filter draft loses previous committed list/selection");
                games.displayScale(1);
                puts("Original combo widgets: all eleven filter menus, native font geometry, integer-scale hits, first/persistent highlights, keyboard wrapping, modal capture, drag release, focus restoration and staged cancellation passed");
            }
            games.activate(find(MenuOp::GameFilter));games.activate(find(MenuOp::GameFilterCombo,0));
            auto yes=std::find_if(games.view().items.begin(),games.view().items.end(),[](const auto& item){return item.action.op==MenuOp::GameFilterChoice && item.action.value==0 && item.action.delta==1;});require(yes!=games.view().items.end(),"original Yes/No/DontCare filter choices");games.activate(size_t(yes-games.view().items.begin()));games.activate(find(MenuOp::GameFilterAccept));
            last();require(games.activate(find(MenuOp::GameJoin)).effect==FrontEffect::Connect && games.serverAddress()=="127.0.0.1:28021","goal/property filter uses actual mission rules");
            games.open(Screen::Games);games.activate(find(MenuOp::GameCreate));require(games.wantsText() && edit("createEditPane").editFocused && edit("createEditPane").text=="Cadet's game","original Create Game opens authored name edit with no added cursor character");require(games.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect==FrontEffect::None && games.screen()==Screen::Games,"original Create Game forwards Return without inventing acceptance");games.activate(find(MenuOp::GameCreateServer,0));games.activate(find(MenuOp::GameCreateCore,0));games.activate(find(MenuOp::GameCreateAccept));require(games.screen()==Screen::Lobby && games.session().scenario==Scenario::Conquest && games.session().name=="Cadet's game","original creation dialog creates explicitly local named game");
            require(edit("gameNameEditPane").editReadOnly && !edit("gameNameEditPane").editFocused && edit("gameNameEditPane").text==games.session().name,"original non-Club setup does not display its authored readonly game name");
            auto encoded=Net::session(games.session());require(Net::options(encoded,0,"Pilot").name==games.session().name,"game name round trips in native session record");
            auto cancel=std::find_if(games.view().items.begin(),games.view().items.end(),[](const auto& i){return i.action.op==MenuOp::SetupCancel;});require(cancel!=games.view().items.end(),"created game uses original settings");games.activate(size_t(cancel-games.view().items.begin()));require(games.screen()==Screen::Games,"cancelling local creation returns to games list");
            games.activate(find(MenuOp::GameFilter));games.text(std::string(100,'M'));require(edit("gameNameEditPane").text.size()>64,"original local substring filter incorrectly has a network game-name input limit");games.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            games.activate(find(MenuOp::GameCreate));games.activate(find(MenuOp::GameCreateServer,0));games.activate(find(MenuOp::GameCreateCore,0));
            {auto& field=edit("createEditPane");auto index=find(MenuOp::GameCreateName);auto b=games.view().items[index].bounds;require(b==field.visibleBounds() && b.z<field.bounds.z,"Create Game hit area ignores inherited parent clipping");
                auto layout=originalMenuLayout({1000,700});auto outside=menuHit(games.view(),{1000,700},1,layout.pixel({b.x+b.z+3,b.y+3}));require(!outside || *outside!=index,"clipped-away authored edit still accepts pointer input");}
            while(!edit("createEditPane").text.empty())games.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);
            require(games.view().items[find(MenuOp::GameCreateAccept)].enabled,"original Create button must dispatch empty-name validation");games.activate(find(MenuOp::GameCreateAccept));
            auto errorText=[&](std::string_view value){return std::any_of(games.view().artwork.begin(),games.view().artwork.end(),[&](const auto& c){return c.kind==OriginalControl::Kind::Text && c.text==value;});};
            require(errorText("Error : Game name is empty") && !games.wantsText(),"Create Game omits original empty-name message or leaks text through it");games.key(SDLK_RETURN,SDL_SCANCODE_RETURN);require(games.wantsText(),"dismissing creation error loses its name edit");
            games.text(std::string(65,'M'));require(edit("createEditPane").text.size()==65,"original Create Game must validate length on acceptance instead of silently clipping input");games.activate(find(MenuOp::GameCreateAccept));
            require(errorText("Error : Game name is too long") && games.screen()==Screen::Games,"Create Game omits original overlong-name error or accepts invalid state");games.key(SDLK_RETURN,SDL_SCANCODE_RETURN);games.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);games.activate(find(MenuOp::GameCreateAccept));
            require(games.screen()==Screen::Lobby && games.session().name==std::string(64,'M') && Net::options(Net::session(games.session()),0,"Pilot").name==games.session().name,"valid original 64-byte game name is rejected or truncated by native session records");
            require(Allegiance::GameNameCapacity==c_cbGameName,"native authored validation differs from original IGC game-name capacity");games.activate(find(MenuOp::SetupCancel));
            puts("Original game-name validation: unbounded local filter, enabled Create, exact empty/too-long modals, restored edit/draft and full 64-byte session name passed");
            puts("Original games list: empty/live states, population sort, endpoint selection, full scrolling, real joining, case-insensitive and tri-state staged filters, named local creation and cancellation passed");
            GameState remote;remote.networked=remote.networkLobby=remote.networkHost=remote.commander=true;remote.networkHostPilot=10;remote.networkCountdown=5;remote.missionStage=STAGE_STARTING;remote.lobbyFlags=3;
            games.setPlaying(true);games.open(Screen::RemoteLobby,&remote);games.setPlaying(false);games.open(Screen::Games);games.activate(find(MenuOp::GameCreate));games.activate(find(MenuOp::GameCreateServer,0));games.activate(find(MenuOp::GameCreateCore,0));games.activate(find(MenuOp::GameCreateAccept));
            auto accept=std::find_if(games.view().items.begin(),games.view().items.end(),[](const auto& i){return i.action.op==MenuOp::SetupAccept && i.enabled;});require(accept!=games.view().items.end(),"leaving network match clears authority/lobby flags before local setup");
            auto accepted=games.activate(size_t(accept-games.view().items.begin()));require(accepted.effect!=FrontEffect::NetworkConfigure && games.screen()==Screen::LobbyPilots,"local creation after leaving network session uses local settings transaction");
        }
        {
            Settings prefs;Frontend options(prefs);options.originalInterface(assets);
            auto homeArtwork=options.view().artwork.size();options.open(Screen::Settings);
            if(!options.view().original || options.view().overlay || options.view().items.size()!=4 || options.view().items[0].bounds.x!=11 || options.view().items[0].bounds.y!=11)
                throw std::runtime_error("Original options root geometry or backdrop is incorrect");
            auto press=[&](SDL_Keycode key,SDL_Scancode scan){return options.key(key,scan);};
            auto find=[&](std::string_view label){for(size_t i=0;i<options.view().items.size();++i)if(options.view().items[i].label.starts_with(label))return i;throw std::runtime_error("Original popup entry missing: "+std::string(label));};
            options.pointerHover(find("Game Options"));
            if(options.view().items.size()!=13)throw std::runtime_error("Original options hover fails to open adjacent submenu");
            auto gameBounds=options.view().items[find("Chat/Mute/Filter")].bounds;
            if(gameBounds.x<=options.view().items[0].bounds.x)throw std::runtime_error("Original options submenu replaces its parent");
            auto changed=press(SDLK_L,SDL_SCANCODE_L);
            if(changed.effect!=FrontEffect::ApplyOptions || changed.value!=1 || !prefs.controller.linear || options.screen()!=Screen::Settings)
                throw std::runtime_error("Original options do not apply immediately while retaining popup");
            press(SDLK_Z,SDL_SCANCODE_Z);if(prefs.controller.deadZone!=.3f)throw std::runtime_error("Original dead zone does not cycle Small to Large");
            press(SDLK_Z,SDL_SCANCODE_Z);if(prefs.controller.deadZone!=.04f)throw std::runtime_error("Original dead zone does not cycle Large to Smallest");
            press(SDLK_K,SDL_SCANCODE_K);if(!prefs.preferChase)throw std::runtime_error("Original default camera preference does not change");
            press(SDLK_Q,SDL_SCANCODE_Q);float sensitivity=prefs.sensitivity;press(SDLK_U,SDL_SCANCODE_U);
            if(std::abs(prefs.sensitivity-sensitivity-.01f)>1e-6f)throw std::runtime_error("Original mouse sensitivity increment is incorrect");
            press(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            if(options.view().items.size()!=13)throw std::runtime_error("Escape closes more than the current submenu");
            press(SDLK_C,SDL_SCANCODE_C);
            if(options.screen()!=Screen::Controls || !options.view().original || options.view().overlay ||
                std::any_of(options.view().items.begin(),options.view().items.end(),[](auto& i){return i.action.op==MenuOp::PopupItem;}))
                throw std::runtime_error("Input mapping retains an options menu underneath");
            press(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            if(options.screen()!=Screen::Home || options.view().artwork.size()!=homeArtwork || !prefs.controller.linear || !prefs.preferChase)
                throw std::runtime_error("Mapping Cancel loses parent or reverts already applied options");
            options.open(Screen::Settings);press(SDLK_O,SDL_SCANCODE_O);bool environment=prefs.visual.environment;
            changed=press(SDLK_E,SDL_SCANCODE_E);
            if(changed.effect!=FrontEffect::ApplyOptions || changed.value!=4 || prefs.visual.environment==environment)
                throw std::runtime_error("Graphics option unnecessarily reconfigures input or does not apply");
            int initialParticles=prefs.particles;
            for(int expected:{1,2,3,4,5,0}) {
                changed=press(SDLK_H,SDL_SCANCODE_H);
                if(changed.effect!=FrontEffect::ApplyOptions || changed.value!=4 || prefs.hudStyle!=expected || prefs.particles!=initialParticles)
                    throw std::runtime_error("Original six-slot HUD cycle does not apply immediately or alters other graphics settings");
            }
            for(auto expected:{3,0,1,2}){
                press(SDLK_D,SDL_SCANCODE_D);
                if(prefs.visual.debris!=expected || prefs.particles!=initialParticles || options.view().items[find(debrisLabels[expected])].label!=debrisLabels[expected])
                    throw std::runtime_error("Original debris cycle changes particle quality or labels");
            }
            for(auto expected:{2,3,4,0,1}){
                press(SDLK_L,SDL_SCANCODE_L);
                if(prefs.particles!=expected || prefs.visual.debris!=2 || options.view().items[find(particlePreference(expected).label)].label!=particlePreference(expected).label)
                    throw std::runtime_error("Original five-state particle cycle changes debris or labels");
            }
            press(SDLK_A,SDL_SCANCODE_A);
            for(const auto& item:options.view().items)if(item.bounds.x<0 || item.bounds.y<0 || item.bounds.x+item.bounds.z>800 || item.bounds.y+item.bounds.w>600)
                throw std::runtime_error("Original cascading popup exceeds authored canvas");
            press(SDLK_LEFT,SDL_SCANCODE_LEFT);press(SDLK_LEFT,SDL_SCANCODE_LEFT);press(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            options.open(Screen::Settings);press(SDLK_S,SDL_SCANCODE_S);float volume=prefs.effectsGain;
            {auto disabled=find("Sound Quality");auto bounds=options.view().items[disabled].bounds;auto point=originalMenuLayout({1000,700}).pixel(glm::vec2(bounds)+glm::vec2(3));auto hit=menuHit(options.view(),{1000,700},1,point);
                require(hit==disabled && !options.view().items[disabled].enabled,"original disabled menu row loses hover input");options.pointerHover(hit);
                require(options.view().items[find("Sound Quality")].selected && options.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect==FrontEffect::None && prefs.effectsGain==volume,"disabled popup row executes a command or cannot be highlighted");}
            press(SDLK_S,SDL_SCANCODE_S);
            if(prefs.effectsGain!=volume+1 || options.screen()!=Screen::Settings)throw std::runtime_error("Original sound menu volume behavior differs");
            press(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);press(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            options.open(Screen::Settings);press(SDLK_G,SDL_SCANCODE_G);press(SDLK_M,SDL_SCANCODE_M);
            for(int expected:{1,2,3}){changed=press(SDLK_L,SDL_SCANCODE_L);if(changed.effect!=FrontEffect::ApplyOptions || changed.value!=4 || prefs.chat.filterLobby!=expected)throw std::runtime_error("Original lobby filter cycle is incorrect");}
            for(int expected:{3,1,2}){press(SDLK_Z,SDL_SCANCODE_Z);if(prefs.chatSize!=expected)throw std::runtime_error("Original chat font size cycle is incorrect");}
            for(int i=0;i<20;++i)press(SDLK_I,SDL_SCANCODE_I);
            if(prefs.chatLines!=10 || options.view().items[find("Chat Lines At Maximum")].label!="Chat Lines At Maximum")throw std::runtime_error("Chat line maximum or label differs");
            for(int i=0;i<20;++i)press(SDLK_R,SDL_SCANCODE_R);
            if(prefs.chatLines!=1 || options.view().items[find("Chat Lines At Minimum")].label!="Chat Lines At Minimum")throw std::runtime_error("Chat line minimum or label differs");
            press(SDLK_I,SDL_SCANCODE_I);find("Increase To 3 Chat Lines");find("Reduce To 1 Chat Lines");
            press(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);press(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);press(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            auto path=std::filesystem::temp_directory_path()/"allegiance-original-popup-settings.conf";prefs.save(path);auto loaded=Settings::load(path);std::filesystem::remove(path);
            if(loaded.preferChase!=prefs.preferChase || loaded.visual.environment!=prefs.visual.environment || loaded.effectsGain!=prefs.effectsGain || loaded.visual.debris!=prefs.visual.debris || loaded.particles!=prefs.particles || loaded.chat!=prefs.chat || loaded.chatLines!=prefs.chatLines || loaded.chatSize!=prefs.chatSize)
                throw std::runtime_error("Immediate original options fail to persist");
            GameState flight;flight.docked=false;flight.sideActive=true;options.setPlaying(true);options.open(Screen::Pause,&flight);
            if(!options.view().overlay || press(SDLK_RETURN,SDL_SCANCODE_RETURN).effect!=FrontEffect::None)
                throw std::runtime_error("Original flight menu hides scene or activates its version header");
            press(SDLK_G,SDL_SCANCODE_G);press(SDLK_C,SDL_SCANCODE_C);
            if(!options.view().overlay || options.screen()!=Screen::Controls)throw std::runtime_error("Flight input popup loses game backdrop");
            if(press(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE).effect!=FrontEffect::Resume)throw std::runtime_error("Closing flight input popup does not resume");
            options.open(Screen::Pause,&flight);if(press(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE).effect!=FrontEffect::Resume)
                throw std::runtime_error("Closing original flight menu does not resume");
        }
        {
            Settings defaults;defaults.importInputMap(assets.root()/"defaultinputmap.mdl");
            Settings prefs=defaults;prefs.volume=.23f;prefs.controller.feedbackGain=.4f;
            Frontend editor(prefs,defaults);editor.originalInterface(assets);editor.open(Screen::Controls);
            auto item=[&](MenuOp op,int value=0,int delta=1){for(size_t i=0;i<editor.view().items.size();++i){auto& a=editor.view().items[i].action;if(a.op==op && a.value==value && a.delta==delta)return i;}throw std::runtime_error("Original mapping/list action missing");};
            auto select=[&](TrekKey command){int row=0;for(auto& c:Allegiance::CommandDescriptions){if(c.m_tk==command)break;++row;}editor.key(SDLK_DOWN,SDL_SCANCODE_DOWN);for(int i=0;i<int(std::size(Allegiance::CommandDescriptions))+6;++i)editor.key(SDLK_UP,SDL_SCANCODE_UP);for(int i=0;i<row;++i)editor.key(SDLK_DOWN,SDL_SCANCODE_DOWN);editor.activate(item(MenuOp::MappingSelect,row));};
            auto originalKeys=prefs.keys;select(TK_StartChat);
            if(editor.capturingKey())throw std::runtime_error("Single-click mapping selection enters edit");
            editor.key(SDLK_RETURN,SDL_SCANCODE_RETURN);editor.key(SDLK_LSHIFT,SDL_SCANCODE_LSHIFT,true,KeyShift);editor.keyReleased(SDLK_LSHIFT,SDL_SCANCODE_LSHIFT);
            if(!editor.capturingKey())throw std::runtime_error("Original editor binds a standalone modifier instead of awaiting a chord");
            editor.key(SDLK_F13,SDL_SCANCODE_F13);editor.activate(item(MenuOp::MappingAccept));
            if(prefs.keys!=originalKeys || KeyboardBindings(prefs).command(SDL_SCANCODE_F13,0)!=TK_StartChat || prefs.volume!=.23f)throw std::runtime_error("Adding mapping replaces existing keys or unrelated preferences");
            editor.open(Screen::Controls);select(TK_FireWeapon);editor.key(SDLK_RETURN,SDL_SCANCODE_RETURN);editor.key(SDLK_F13,SDL_SCANCODE_F13);
            if(!editor.capturingKey() || std::none_of(editor.view().artwork.begin(),editor.view().artwork.end(),[](auto& c){return c.text.find("Key in use for \"Start Chat\"")!=c.text.npos;}))throw std::runtime_error("Conflicting original mapping does not ask before overwriting");
            editor.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);editor.key(SDLK_RETURN,SDL_SCANCODE_RETURN);editor.key(SDLK_F13,SDL_SCANCODE_F13);editor.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
            editor.activate(item(MenuOp::MappingAccept));
            if(KeyboardBindings(prefs).command(SDL_SCANCODE_F13,0)!=TK_FireWeapon || prefs.keys[size_t(Binding::Chat)]!=originalKeys[size_t(Binding::Chat)])throw std::runtime_error("Confirmed mapping transfer removes another key of its old action");
            KeyboardBindings held(prefs);held.setFocus(true);held.keyDown(SDL_SCANCODE_F13,SDL_KMOD_NONE);held.keyDown(SDL_SCANCODE_LCTRL,SDL_KMOD_CTRL);
            if(!held.sample()[size_t(Binding::Fire)])throw std::runtime_error("An unmodified key without chords fails to acquire original Any behavior when saved");
            editor.open(Screen::Controls);select(TK_StartChat);editor.key(SDLK_RETURN,SDL_SCANCODE_RETURN);editor.key(SDLK_F13,SDL_SCANCODE_F13,true,KeyShift);editor.activate(item(MenuOp::MappingAccept));
            KeyboardBindings chord(prefs);chord.setFocus(true);chord.keyDown(SDL_SCANCODE_F13,SDL_KMOD_NONE);chord.keyDown(SDL_SCANCODE_LSHIFT,SDL_KMOD_SHIFT);
            if(chord.sample()[size_t(Binding::Fire)] || chord.command(SDL_SCANCODE_F13,SDL_KMOD_SHIFT)!=TK_StartChat)throw std::runtime_error("Saving a chord leaves the same key's unmodified continuous mapping active under modifiers");
            editor.open(Screen::Controls);select(TK_StartChat);editor.key(SDLK_RETURN,SDL_SCANCODE_RETURN);editor.mappingMouse(1);editor.key(SDLK_RETURN,SDL_SCANCODE_RETURN);editor.activate(item(MenuOp::MappingAccept));
            if(prefs.mouseButtons[0]!=int(Binding::Chat))throw std::runtime_error("Original input popup cannot remap a mouse button");
            auto saved=prefs;editor.open(Screen::Controls);select(TK_StartChat);editor.key(SDLK_DELETE,SDL_SCANCODE_DELETE);editor.activate(item(MenuOp::MappingCancel));
            if(prefs.keys!=saved.keys || prefs.keyAliases!=saved.keyAliases || prefs.mouseButtons!=saved.mouseButtons)throw std::runtime_error("Mapping Cancel commits deleted keyboard or mouse mappings");
            editor.open(Screen::Controls);select(TK_StartChat);editor.key(SDLK_DELETE,SDL_SCANCODE_DELETE);editor.key(SDLK_SPACE,SDL_SCANCODE_SPACE);
            if(!inputMappingText(prefs,TK_StartChat).empty() || prefs.mouseButtons[0]>=0)throw std::runtime_error("Delete fails to clear all device mappings of an action");
            editor.open(Screen::Controls);editor.activate(item(MenuOp::MappingRestore));editor.activate(item(MenuOp::MappingAccept));
            if(prefs.keys!=defaults.keys || prefs.keyAliases!=defaults.keyAliases || prefs.mouseButtons!=defaults.mouseButtons || prefs.controller.mapAxes!=defaults.controller.mapAxes || prefs.volume!=.23f || prefs.controller.feedbackGain!=.4f)throw std::runtime_error("Original Restore resets unrelated settings or fails to restore the authored input map");
            editor.open(Screen::Controls);auto down=item(MenuOp::ScrollLine,0,1);auto bounds=editor.view().items[down].bounds;editor.pointerMove(glm::vec2(bounds)+glm::vec2(3));editor.pointerPress(down);
            auto first=[&](){for(auto& a:editor.view().items)if(a.action.op==MenuOp::MappingSelect)return a.action.value;return -1;};auto afterPress=first();editor.tick(.49);
            if(first()!=afterPress)throw std::runtime_error("Scrollbar repeat fires before the original half-second delay");
            editor.tick(.02);auto afterRepeat=first();editor.pointerRelease(item(MenuOp::ScrollLine,0,1));
            if(afterPress<1 || afterRepeat<=afterPress || first()!=afterRepeat)throw std::runtime_error("Scrollbar press/repeat/release scrolls incorrectly");
            auto thumb=item(MenuOp::ScrollThumb,0);bounds=editor.view().items[thumb].bounds;editor.pointerMove(glm::vec2(bounds)+glm::vec2(3));editor.pointerPress(thumb);editor.pointerMove({bounds.x+3,10000});editor.pointerRelease({});
            if(std::none_of(editor.view().items.begin(),editor.view().items.end(),[](auto& a){return a.action.op==MenuOp::MappingSelect && a.action.value==int(std::size(Allegiance::CommandDescriptions))+5;}))throw std::runtime_error("Thumb dragging cannot reach original joystick axis rows");
            editor.activate(item(MenuOp::MappingSelect,int(std::size(Allegiance::CommandDescriptions))+5));
            auto oldFirst=first();int wheelRows=int(std::count_if(editor.view().items.begin(),editor.view().items.end(),[](auto& a){return a.action.op==MenuOp::MappingSelect;}));
            editor.pointerMove({180,200});
            for(int i=0;i<wheelRows;++i)editor.pointerWheel(1);
            if(first()>=oldFirst || !editor.view().items[item(MenuOp::MappingSelect,int(std::size(Allegiance::CommandDescriptions))+5-wheelRows)].selected)throw std::runtime_error("Original list wheel does not move selection and reveal it");
            editor.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            puts("Original input editor: multiple bindings, conflicts, device deletion, restore/cancel, modifier handling, list repeat and thumb dragging passed");
        }
        {
            if(!SDL_Init(SDL_INIT_JOYSTICK))throw std::runtime_error(SDL_GetError());
            struct JoystickCleanup{SDL_Joystick* joystick{};SDL_JoystickID id{};~JoystickCleanup(){if(joystick)SDL_CloseJoystick(joystick);if(id)SDL_DetachVirtualJoystick(id);SDL_QuitSubSystem(SDL_INIT_JOYSTICK);}} cleanup;
            SDL_VirtualJoystickDesc desc;SDL_INIT_INTERFACE(&desc);desc.type=SDL_JOYSTICK_TYPE_FLIGHT_STICK;desc.naxes=2;desc.nbuttons=2;desc.nhats=1;desc.name="Original mapping test stick";
            cleanup.id=SDL_AttachVirtualJoystick(&desc);cleanup.joystick=SDL_OpenJoystick(cleanup.id);
            if(!cleanup.id || !cleanup.joystick)throw std::runtime_error(SDL_GetError());
            int count=0,ordinal=-1;auto* ids=SDL_GetJoysticks(&count);std::vector<ControllerDevice> devices;
            for(int i=0;i<count;++i){char guid[33];SDL_GUIDToString(SDL_GetJoystickGUIDForID(ids[i]),guid,sizeof(guid));devices.push_back({guid,"Mapping test device",false,2,2,1});if(ids[i]==cleanup.id)ordinal=i;}SDL_free(ids);
            Settings prefs;prefs.importInputMap(assets.root()/"defaultinputmap.mdl");prefs.controller.mapAxes={};prefs.controller.mapAxes[0]={ordinal,0,false,true};prefs.controller.mapDevices={};prefs.controller.mapButtons.clear();
            Frontend editor(prefs,prefs);editor.originalInterface(assets);editor.controllers(devices,"Mapping test device",2,2,1,false);editor.open(Screen::Controls);
            int rows=int(std::size(Allegiance::CommandDescriptions));
            auto row=[&](int target){editor.key(SDLK_DOWN,SDL_SCANCODE_DOWN);for(int i=0;i<rows+6;++i)editor.key(SDLK_UP,SDL_SCANCODE_UP);for(int i=0;i<target;++i)editor.key(SDLK_DOWN,SDL_SCANCODE_DOWN);editor.key(SDLK_RETURN,SDL_SCANCODE_RETURN);};
            auto accept=[&](){for(size_t i=0;i<editor.view().items.size();++i)if(editor.view().items[i].action.op==MenuOp::MappingAccept){editor.activate(i);return;}throw std::runtime_error("Joystick mapping acceptance missing");};
            row(rows+1);SDL_Event axis{};axis.type=SDL_EVENT_JOYSTICK_AXIS_MOTION;axis.jaxis.which=cleanup.id;axis.jaxis.axis=0;axis.jaxis.value=10000;editor.mappingController(axis);
            if(!editor.capturingKey())throw std::runtime_error("Joystick mapper captures noise below the original axis travel threshold");
            axis.jaxis.value=32767;editor.mappingController(axis);
            if(std::none_of(editor.view().artwork.begin(),editor.view().artwork.end(),[](auto& c){return c.text.starts_with("Axis in use.");}))throw std::runtime_error("Joystick axis conflicts overwrite another logical axis silently");
            editor.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);accept();
            if(prefs.controller.mapAxes[0].device!=ordinal || !prefs.controller.mapDevices[ordinal].empty())throw std::runtime_error("Canceling joystick axis edit changes the old route or device identity");
            editor.open(Screen::Controls);row(rows+1);editor.mappingController(axis);editor.key(SDLK_RETURN,SDL_SCANCODE_RETURN);accept();
            if(prefs.controller.mapAxes[0].device!=-1 || prefs.controller.mapAxes[1].device!=ordinal || prefs.controller.mapAxes[1].index!=0 || prefs.controller.mapDevices[ordinal]!=devices[ordinal].guid)throw std::runtime_error("Confirmed joystick axis transfer loses routing or device identity");
            editor.open(Screen::Controls);int commandRow=0;for(auto& c:Allegiance::CommandDescriptions){if(c.m_tk==TK_StartChat)break;++commandRow;}row(commandRow);
            SDL_Event button{};button.type=SDL_EVENT_JOYSTICK_BUTTON_DOWN;button.jbutton.which=cleanup.id;button.jbutton.button=1;editor.mappingController(button);accept();
            if(prefs.controller.mapButtons!=std::vector<ControllerButtonRoute>{{ordinal,1,Binding::Chat}})throw std::runtime_error("Original input popup cannot capture joystick buttons");
            puts("Original joystick mapper: travel threshold, conflict confirmation, edit cancellation, stable device identity and button capture passed");
        }
        {
            SessionOptions options;options.scenario=Scenario::FreeFlight;options.enemies=0;options.startInHangar=true;
            auto loader=[&](const std::string& name,const std::string&){auto m=assets.model(name);return ModelHandle{1,m->radius,m->frames,m->lights};};
            Game game(assets.root(),loader,0,0,options);Settings prefs;Frontend original(prefs);original.originalInterface(assets);original.setPlaying(true);
            original.loadoutServices([&](const Loadout& l,bool defaults,int target){return game.previewLoadout(l,defaults,target);},[&](PartKind k,int m,int h){return game.loadoutPartChoices(k,m,h);},{},[&](int part,int h){return game.loadoutPartInfo(part,h);});
            std::vector<Loadout> remembered;original.loadoutMemory([&](const Loadout& loadout){remembered.push_back(loadout);game.rememberEditorLoadout(loadout);});
            auto index=[&](MenuOp op,int value=0){for(size_t i=0;i<original.view().items.size();++i)if(original.view().items[i].action.op==op && original.view().items[i].action.value==value)return i;throw std::runtime_error("Missing staged loadout UI action");};
            auto before=game.currentLoadout();original.open(Screen::ShipLoadout,&game.state());
            if(!original.view().original || !original.view().shipPreview || !original.stagedLoadout().valid)throw std::runtime_error("Original staged loadout or 3D preview missing");
            OriginalUi authored(assets);auto authoredSlots=authored.loadoutSlots(original.stagedLoadout().loadout.hull);int pictures=0;
            for(const auto& slot:authoredSlots){auto& preview=original.stagedLoadout();int mount=slot.type==-1?slot.mount-5:slot.type==-2?slot.mount+preview.fixedWeapons:slot.mount,type=slot.type==-2?1:slot.type;
                if((slot.type==1 && mount>=preview.fixedWeapons) || (slot.type==-2 && mount>=preview.weapons))continue;
                auto part=std::find_if(preview.slots.begin(),preview.slots.end(),[&](const auto& p){return p.mount==mount && (mount<0 || int(p.kind)==type);});if(part==preview.slots.end() || part->id<0)continue;
                auto name=slot.prefix+part->model+"bmp";if(!assets.hasImage(name))continue;auto picture=assets.image(name,true);
                auto control=std::find_if(original.view().artwork.begin(),original.view().artwork.end(),[&](const auto& c){return c.name=="loadoutSlot"+std::to_string(part-preview.slots.begin());});
                require(control!=original.view().artwork.end() && control->image==picture,"Loadout slot discards its MDL image prefix");++pictures;
            }
            require(pictures>0,"Loadout artwork check did not visit equipped slots");
            original.pointerMove({400,350});original.activate(index(MenuOp::LoadoutPopup,1));auto hull=original.stagedLoadout().hulls[1].id;
            const auto& hullChoice=original.stagedLoadout().hulls[1];auto label=hullChoice.name+(hullChoice.price?" $("+std::to_string(hullChoice.price)+")":"");
            if(original.view().items[index(MenuOp::PreviewHull,hull)].label!=label || original.view().items[index(MenuOp::PreviewHull,hull)].bounds.w>14 ||
                std::any_of(original.view().items.begin(),original.view().items.end(),[](auto& i){return i.label=="Previous" || i.label=="Next";}))
                throw std::runtime_error("Original hull popup retains paged rows, oversized text or incorrect price format");
            original.activate(index(MenuOp::PreviewHull,hull));if(original.stagedLoadout().loadout.hull!=hull || game.currentLoadout()!=before)throw std::runtime_error("Browsing a hull purchased it");
            original.activate(index(MenuOp::PreviewDefault));
            auto restored=game.previewLoadout(before,false,hull);if(!restored.valid || restored.loadout.hull!=hull || game.currentLoadout()!=before)throw std::runtime_error("Restoring another hull's preset changes the selected hull or live ship");
            if(original.key(SDLK_9,SDL_SCANCODE_9,false,KeyControl).effect!=FrontEffect::SaveLoadout || original.key(SDLK_9,SDL_SCANCODE_9).effect!=FrontEffect::UseLoadout)throw std::runtime_error("Original loadout preset save/restore chords missing");
            auto& slots=original.stagedLoadout().slots;auto gun=std::find_if(slots.begin(),slots.end(),[](auto& s){return s.kind==PartKind::Weapon && s.mount==0;});
            if(gun==slots.end())throw std::runtime_error("Staged hull has no weapon slot");
            original.activate(index(MenuOp::PreviewSlot,int(gun-slots.begin())));original.activate(index(MenuOp::PreviewPart,-1));
            auto staged=original.stagedLoadout().loadout;
            auto memories=remembered.size();original.open(Screen::Settings,&game.state());original.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            if(original.screen()!=Screen::ShipLoadout || original.stagedLoadout().loadout!=staged || remembered.size()!=memories)throw std::runtime_error("Opening options commits or discards a staged loadout");
            if(game.currentLoadout()!=before || original.activate(index(MenuOp::CommitLoadout,1)).effect!=FrontEffect::CommitLoadout)throw std::runtime_error("Equipment browsing changed live ship or staged launch missing");
            original.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);if(original.screen()!=Screen::Hangar || game.currentLoadout()!=before)throw std::runtime_error("Leaving the original loadout commits a preview");
            if(remembered.empty() || remembered.back()!=staged)throw std::runtime_error("Closing the loadout forgets its original client cache intent");
            original.open(Screen::ShipLoadout,&game.state());original.purchasing(false);
            if(!original.hasMessageBox() || original.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect!=FrontEffect::None || original.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE).effect!=FrontEffect::None)throw std::runtime_error("Pending purchase accepts input before completion");
            original.connectionFinished();if(original.hasMessageBox())throw std::runtime_error("Purchase completion leaves the input lockdown open");
            auto purchase=game.purchaseLoadout(staged);
            if(!purchase.accepted || !purchase.complete || purchase.launched || game.currentLoadout().hull!=hull)throw std::runtime_error("Staged loadout cannot be committed through original IGC purchase");
            puts("Original loadout hull/part browsing, defaults, preset chords, cancellation and commit passed");
        }
        {
            SessionOptions options;options.scenario=Scenario::FreeFlight;options.enemies=0;options.startInHangar=true;
            auto loader=[&](const std::string& name,const std::string&){auto m=assets.model(name);return ModelHandle{1,m->radius,m->frames,m->lights};};
            Game game(assets.root(),loader,0,0,options);auto live=game.currentLoadout();auto initial=game.previewLoadout();
            auto equipped=std::find_if(initial.slots.begin(),initial.slots.end(),[](auto& slot){return slot.id>=0;});
            if(equipped==initial.slots.end())throw std::runtime_error("Overflow popup fixture has no part metadata");
            auto info=game.loadoutPartInfo(equipped->id,initial.loadout.hull);if(!info)throw std::runtime_error("Read-only part metadata is unavailable");
            int previews=0,descriptions=0;Settings prefs;Frontend menu(prefs);menu.originalInterface(assets);menu.setPlaying(true);
            menu.loadoutServices([&](const Loadout& l,bool defaults,int target){++previews;return game.previewLoadout(l,defaults,target);},
                [&](PartKind,int,int){std::vector<PartChoice> choices{{-1,"-Make Slot Empty-"}};
                    for(int i=0;i<300;++i)choices.push_back({1000+i,"Overflow Primary "+std::to_string(i),100});
                    for(int i=0;i<300;++i)choices.push_back({2000+i,"Overflow Other "+std::to_string(i),200,true});return choices;},{},
                [&](int part,int)->std::optional<LoadoutSlotPreview>{++descriptions;auto copy=*info;copy.id=part;copy.name="Hover part "+std::to_string(part);return copy;});
            auto item=[&](MenuOp op,int value,int level=-1){for(size_t i=0;i<menu.view().items.size();++i){const auto& a=menu.view().items[i];if(a.enabled && a.action.op==op && a.action.value==value && (level<0 || a.popupLevel==level))return i;}throw std::runtime_error("Overflow popup control missing");};
            auto popupItem=[&](int level,int row){for(size_t i=0;i<menu.view().items.size();++i)if(menu.view().items[i].popupLevel==level && menu.view().items[i].popupRow==row)return i;throw std::runtime_error("Popup row missing");};
            menu.open(Screen::ShipLoadout,&game.state());int snapshotPreviews=previews;auto staged=menu.stagedLoadout().loadout;
            auto cargo=std::find_if(menu.stagedLoadout().slots.begin(),menu.stagedLoadout().slots.end(),[](auto& slot){return slot.mount<0;});
            if(cargo==menu.stagedLoadout().slots.end())throw std::runtime_error("Overflow popup fixture lacks cargo slot");
            auto rng=game.simulationRandomState();int ships=game.core()->GetShips()->n(),money=game.state().money;
            menu.pointerMove({780,590});menu.pointerContext(item(MenuOp::PreviewSlot,int(cargo-menu.stagedLoadout().slots.begin())));
            if(menu.view().items[popupItem(0,0)].label!="Parts For Other Ships" || menu.view().items[item(MenuOp::PreviewPart,1000,0)].label!="Overflow Primary 0 $(100)")
                throw std::runtime_error("Original cargo menu lacks first adjacent submenu or exact prices");
            menu.pointerHover(popupItem(0,0));
            auto firstOther=item(MenuOp::PreviewPart,2000,1);auto bounds=menu.view().items[firstOther].bounds;
            if(bounds.x>=menu.view().items[popupItem(0,0)].bounds.x)throw std::runtime_error("Right-edge part cascade does not flip to the left");
            menu.pointerHover(firstOther);menu.pointerHover(item(MenuOp::PreviewPart,2000,1));
            if(descriptions!=1 || previews!=snapshotPreviews)throw std::runtime_error("Unchanged hover repeats metadata work or builds a preview ship");
            menu.key(SDLK_END,SDL_SCANCODE_END);
            auto last=item(MenuOp::PreviewPart,2299,1);bounds=menu.view().items[last].bounds;
            if(!menu.view().items[last].selected)throw std::runtime_error("Overflow part popup cannot select its last entry");
            menu.pointerMove(glm::vec2(bounds)+glm::vec2(2));menu.pointerWheel(1);
            if(!menu.view().items[item(MenuOp::PreviewPart,2298,1)].selected)throw std::runtime_error("Popup wheel fails to select/reveal an item");
            menu.key(SDLK_PAGEUP,SDL_SCANCODE_PAGEUP);
            for(const auto& a:menu.view().items)if(a.popupLevel>=0 && (a.bounds.x<0 || a.bounds.y<0 || a.bounds.x+a.bounds.z>800 || a.bounds.y+a.bounds.w>600 || a.bounds.w>14))
                throw std::runtime_error("Overflow popup hit regions exceed the canvas or authored row height");
            auto thumb=item(MenuOp::ScrollThumb,3);bounds=menu.view().items[thumb].bounds;
            menu.pointerMove(glm::vec2(bounds)+glm::vec2(2));menu.pointerPress(thumb);menu.pointerMove({bounds.x+2,10000});menu.pointerRelease({});
            if(std::none_of(menu.view().items.begin(),menu.view().items.end(),[](auto& a){return a.popupLevel==1 && a.action.op==MenuOp::PreviewPart && a.action.value==2299;}))
                throw std::runtime_error("Overflow popup thumb cannot reveal last cargo part");
            menu.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            if(std::none_of(menu.view().items.begin(),menu.view().items.end(),[](auto& a){return a.popupLevel==0;}) ||
                std::any_of(menu.view().items.begin(),menu.view().items.end(),[](auto& a){return a.popupLevel==1;}))
                throw std::runtime_error("Cargo submenu Escape closes its parent");
            menu.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            if(menu.screen()!=Screen::ShipLoadout || std::any_of(menu.view().items.begin(),menu.view().items.end(),[](auto& a){return a.popupLevel>=0;}) ||
                previews!=snapshotPreviews || menu.stagedLoadout().loadout!=staged || game.currentLoadout()!=live || game.simulationRandomState()!=rng || game.state().money!=money || game.core()->GetShips()->n()!=ships || game.core()->GetShip(-2))
                throw std::runtime_error("Cargo browsing/cancellation mutates a preview or live simulation");
            puts("Original loadout cascades: 600 cargo choices, prices, hover caching, clipping, keyboard/wheel/thumb navigation, one-level close and read-only state passed");
        }
        {
            Settings prefs;Frontend original(prefs);original.originalInterface(assets);
            auto index=[&](MenuOp op,int value=0){for(size_t i=0;i<original.view().items.size();++i)if(original.view().items[i].action.op==op && original.view().items[i].action.value==value)return i;throw std::runtime_error("Missing original UI action");};
            auto training=index(MenuOp::Navigate,int(Screen::Training));
            if(!original.view().original)throw std::runtime_error("Original main menu is not active");
            original.open(Screen::Credits);if(!original.view().original || !original.view().scroll || !original.view().scroll->offset || original.view().scroll->offset->evaluate({})>=0)throw std::runtime_error("Original credits popup or authored scroll missing");
            original.activate(index(MenuOp::SecondaryClose));training=index(MenuOp::Navigate,int(Screen::Training));
            original.pointerPress(training,true);
            require(original.screen()==Screen::Home && !original.pointerCaptured() && original.pointerRelease(training,true).effect==FrontEffect::None,"ordinary ButtonPane double-click repeats its normal action or captures the pointer");
            original.pointerPress(training);
            if(original.screen()!=Screen::Home || original.view().pressed!=int(training))throw std::runtime_error("Original button activates on press instead of release");
            original.pointerRelease({});if(original.screen()!=Screen::Home)throw std::runtime_error("Releasing outside a captured original button activates it");
            original.pointerPress(training);original.pointerRelease(training);
            require(original.screen()==Screen::Home && original.hasMessageBox() && authoredControl(original.view().artwork,"messageText").text==Allegiance::TrainingWelcomeMessage,"original training button skips its welcome dialog");
            original.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
            if(original.screen()!=Screen::Training || original.hasMessageBox())throw std::runtime_error("Original training welcome does not navigate");
            auto lesson=index(MenuOp::TrainingSelect,2);
            if(original.activate(lesson).effect!=FrontEffect::None)throw std::runtime_error("Selecting a training lesson starts it before Launch Mission");
            auto launch=index(MenuOp::TrainingStart,2);
            if(original.activate(launch).effect!=FrontEffect::Start || original.session().trainingMission!=2)throw std::runtime_error("Original Launch Mission does not start the selected training");
            original.open(Screen::Training);
            if(original.view().items[index(MenuOp::TrainingStart,1)].action.value!=1)throw std::runtime_error("Launching a later lesson advances the next-lesson selection");
            original.open(Screen::Home);training=index(MenuOp::Navigate,int(Screen::Training));original.pointerPress(training);original.open(Screen::Training);
            if(original.pointerRelease(training).effect!=FrontEffect::None)throw std::runtime_error("A menu change retains mouse capture and activates another control");
            GameState docked;docked.docked=true;original.setPlaying(true);original.open(Screen::Hangar,&docked);
            original.activate(index(MenuOp::Navigate,int(Screen::ShipLoadout)));
            if(original.screen()!=Screen::ShipLoadout)throw std::runtime_error("Hangar loadout hotspot does not open equipment selection");
            original.open(Screen::Training);lesson=index(MenuOp::TrainingSelect,3);original.pointerPress(lesson);original.pointerRelease(lesson);
            require(original.pointerPress(lesson,true).effect==FrontEffect::Start && original.session().trainingMission==3 && !original.pointerCaptured(),"original training double-click does not launch on down without capture");
            require(original.pointerRelease(lesson,true).effect==FrontEffect::None,"training double-click release launches twice");
            original.open(Screen::Training);lesson=index(MenuOp::TrainingSelect,5);
            require(original.pointerPress(lesson,true).effect==FrontEffect::Start && original.session().trainingMission==1,"training double-click handler changes selection before launching the already selected lesson");
            GameState lobby;lobby.networked=true;lobby.networkLobby=true;original.open(Screen::RemoteLobby,&lobby);
            if(!original.wantsText())throw std::runtime_error("Original team lobby does not focus the chat input");
            original.activate(index(MenuOp::LobbyChatChannel,1));original.text("Lobby channel check");
            auto sent=original.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
            if(sent.effect!=FrontEffect::SendChat || sent.value!=int(ChatChannel::Team) || original.chatText()!="Lobby channel check")throw std::runtime_error("Original lobby chat/header recipient actions missing");
            original.chatSent();if(original.screen()!=Screen::RemoteLobby || !original.chatText().empty())throw std::runtime_error("Sending chat closes the original team lobby or retains the sent text");
            GameState pilots;pilots.pilots.push_back({42,0,0,0,-1,"Roster pilot"});original.open(Screen::Pilots,&pilots);original.activate(index(MenuOp::RosterSelect,0));
            original.text("Private roster message");auto recipient=original.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
            if(original.screen()!=Screen::Pilots || recipient.value!=int(ChatChannel::Target) || recipient.targetId!=42)throw std::runtime_error("Original left-click roster selection opens actions or fails to address chat to the selected pilot");
            original.activate(index(MenuOp::RosterSelect,0));recipient=original.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
            if(recipient.value!=int(ChatChannel::Team))throw std::runtime_error("Clicking selected roster pilot again does not restore channel recipient");
            original.pointerContext(index(MenuOp::RosterSelect,0));
            if(original.screen()!=Screen::Pilots || original.wantsText() || original.view().items[index(MenuOp::MutePilot,42)].label!="Mute         ")throw std::runtime_error("Original right-click roster row does not open the original mute popup or capture chat input");
            auto mute=original.key(SDLK_M,SDL_SCANCODE_M);auto chatBefore=original.chatText();original.text("m");original.keyReleased(SDLK_M,SDL_SCANCODE_M);
            if(mute.effect!=FrontEffect::MutePilot || mute.value!=42 || mute.command!=1 || original.chatText()!=chatBefore || !original.wantsText())throw std::runtime_error("Original mute accelerator leaks into chat or loses the roster context");
            original.open(Screen::Pilots,&pilots);
            for(int id=43;id<342;++id)pilots.pilots.push_back({id,id%2,0,0,-1,"Roster pilot "+std::to_string(id)});
            pilots.economyRevision++;original.sync(pilots);
            auto thumb=std::find_if(original.view().items.begin(),original.view().items.end(),[](auto& a){return a.action.op==MenuOp::ScrollThumb && a.action.value==1;});
            if(thumb==original.view().items.end())throw std::runtime_error("Original roster scrollbar missing");
            auto b=thumb->bounds;auto thumbIndex=size_t(thumb-original.view().items.begin());original.pointerMove(glm::vec2(b)+glm::vec2(3));original.pointerPress(thumbIndex);original.pointerMove({b.x+3,10000});original.pointerRelease({});
            if(std::none_of(original.view().artwork.begin(),original.view().artwork.end(),[](auto& c){return c.text=="Roster pilot 340";}))throw std::runtime_error("Original roster truncates a large team or thumb cannot reach its last pilot");
            auto last=std::find_if(original.view().items.begin(),original.view().items.end(),[](auto& a){return a.action.op==MenuOp::RosterSelect && a.action.delta==340;});
            if(last==original.view().items.end() || last->bounds.y<71 || last->bounds.y+last->bounds.w>376)throw std::runtime_error("Scrolled roster hit regions are not clipped to the authored list");
            original.activate(size_t(last-original.view().items.begin()));for(int i=0;i<21;++i)original.key(SDLK_UP,SDL_SCANCODE_UP);
            recipient=original.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
            if(recipient.targetId!=298)throw std::runtime_error("Original roster page navigation loses selected recipient");
            GameState remote;remote.networked=true;remote.networkLobby=true;remote.commander=true;remote.networkHost=true;
            remote.networkPilots={{7,0,"Duplicate",true,true,true,true,false},{91,0,"Duplicate",false,false,false,false,false},{105,1,"Opponent",true,false,true,false,false}};
            original.open(Screen::RemoteLobby,&remote);original.activate(index(MenuOp::RosterSelect,1));
            auto rosterById=[&](int id){auto& items=original.view().items;auto it=std::find_if(items.begin(),items.end(),[&](const auto& i){return i.action.op==MenuOp::RosterSelect && i.action.delta==id;});require(it!=items.end(),"Original roster test pilot missing");return size_t(it-items.begin());};
            if(original.key(SDLK_RETURN,SDL_SCANCODE_RETURN).targetId!=91)throw std::runtime_error("Duplicate lobby names redirect private chat");
            std::swap(remote.networkPilots[0],remote.networkPilots[1]);original.sync(remote);
            if(original.key(SDLK_RETURN,SDL_SCANCODE_RETURN).targetId!=91 || !original.view().items[rosterById(91)].selected)throw std::runtime_error("Reordered lobby report changes the selected pilot identity");
            original.pointerMove({500,250});original.pointerContext(rosterById(91));
            auto leaderIt=std::find_if(original.view().items.begin(),original.view().items.end(),[](const auto& item){return item.popupLevel>=0 && item.action.op==MenuOp::MakeLeader && item.action.value==91;});if(leaderIt==original.view().items.end())throw std::runtime_error("Original leader popup missing");auto leaderBounds=leaderIt->bounds;
            if(leaderBounds.y>=250 || leaderIt->label!="Make Leader  ")throw std::runtime_error("Original player menu loses its labels or above-pointer placement");
            auto transfer=original.key(SDLK_L,SDL_SCANCODE_L);original.keyReleased(SDLK_L,SDL_SCANCODE_L);
            if(transfer.effect!=FrontEffect::MakeLeader || transfer.value!=91 || original.screen()!=Screen::RemoteLobby)throw std::runtime_error("Make Leader uses the wrong identity/action or changes the lobby screen");
            original.pointerContext(rosterById(91));auto boot=original.key(SDLK_R,SDL_SCANCODE_R);original.keyReleased(SDLK_R,SDL_SCANCODE_R);
            require(boot.effect==FrontEffect::LobbyAction && boot.command==int(LobbyAction::Boot) && boot.targetId==91,"original Boot uses R and selected stable pilot identity");
            auto rules=original.session();rules.rules.lockTeamSettings=true;original.networkSession(rules);original.pointerContext(rosterById(91));
            require(std::none_of(original.view().items.begin(),original.view().items.end(),[](auto& item){return item.popupLevel>=0 && item.action.op==MenuOp::LobbyAction && item.action.value==int(LobbyAction::Boot);}),"locked original player menu omits Boot while retaining Make Leader and Mute");
            require(std::any_of(original.view().items.begin(),original.view().items.end(),[](auto& item){return item.popupLevel>=0 && item.action.op==MenuOp::MakeLeader;}) && !original.view().items[index(MenuOp::LobbyAction,int(LobbyAction::Reject))].enabled,"team-settings lock also disables physical Boot without blocking leadership transfer");
            original.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            remote.networkPilots.push_back({106,noTeam,"Applicant",true,false,false,false,false,0});original.sync(remote);original.pointerContext(index(MenuOp::RosterSelect,2));
            auto accepted=original.key(SDLK_A,SDL_SCANCODE_A);original.keyReleased(SDLK_A,SDL_SCANCODE_A);
            require(accepted.effect==FrontEffect::LobbyAction && accepted.command==int(LobbyAction::Accept) && accepted.targetId==106,"original applicant popup accepts under locked team settings");
            original.pointerContext(index(MenuOp::RosterSelect,2));auto declined=original.key(SDLK_R,SDL_SCANCODE_R);original.keyReleased(SDLK_R,SDL_SCANCODE_R);
            require(declined.effect==FrontEffect::LobbyAction && declined.command==int(LobbyAction::Reject) && declined.targetId==106,"original applicant popup rejects under locked team settings");
            remote.networkPilots.pop_back();rules.rules.lockTeamSettings=false;original.networkSession(rules);original.sync(remote);
            remote.networkPilots[0].muted=true;original.sync(remote);original.pointerContext(rosterById(91));
            auto unmute=original.key(SDLK_U,SDL_SCANCODE_U);original.keyReleased(SDLK_U,SDL_SCANCODE_U);
            if(unmute.effect!=FrontEffect::MutePilot || unmute.value!=91 || unmute.command)throw std::runtime_error("Muted lobby pilot does not expose the original UnMute accelerator");
            original.pointerContext(rosterById(91));remote.networkPilots[0].id=92;original.sync(remote);
            if(std::any_of(original.view().items.begin(),original.view().items.end(),[](const auto& item){return item.popupLevel>=0 && (item.action.op==MenuOp::MakeLeader || item.action.op==MenuOp::MutePilot);}) || !original.wantsText() || original.key(SDLK_RETURN,SDL_SCANCODE_RETURN).targetId>=0)throw std::runtime_error("Lobby replacement with the same name retains a stale popup action or private recipient");
            auto hostMarks=std::count_if(original.view().artwork.begin(),original.view().artwork.end(),[](const auto& c){return c.image && c.bounds.x==380;});
            if(hostMarks!=1)throw std::runtime_error("Network host role does not populate the original roster column");
            original.pointerContext(rosterById(7));
            if(std::any_of(original.view().items.begin(),original.view().items.end(),[](const auto& item){return item.popupLevel>=0 && (item.action.op==MenuOp::MakeLeader || item.action.op==MenuOp::MutePilot);}))throw std::runtime_error("Original self context menu permits self-mute or self leadership transfer");
            original.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            puts("Original player popups: stable duplicate-name identities, reorder/departure, host/leader roles, mute accelerators, chat capture and cancellation passed");
            puts("Original main/training/hangar/roster/chat actions, mouse-release/cancel, changed-menu capture and lesson selection passed");
        }
        {
            Settings preferences;Frontend ui(preferences);ui.originalInterface(assets);ui.setPlaying(true);
            SessionOptions options;options.team=0;ui.networkSession(options);
            GameState state;state.networked=state.networkLobby=state.commander=true;
            state.networkPilots={{7,0,"Commander",true,true,true,true,false},{8,0,"Private recipient",true,false,false,false,false}};
            auto add=[&](std::string text,int channel,bool chat=true,bool leader=false){GameMessage m{};m.serial=state.messages.empty()?1:state.messages.back().serial+1;m.text=std::move(text);m.chatTarget=channel;m.chat=m.fromPlayer=chat;m.leader=leader;state.messages.push_back(std::move(m));};
            for(int i=0;i<30;++i)add("All history "+std::to_string(i),CHAT_EVERYONE);
            add("Team-only report",CHAT_TEAM);add("Leader-only report",CHAT_LEADERS);
            add("Private report",CHAT_INDIVIDUAL);add("Urgent private report",CHAT_INDIVIDUAL_NOFILTER);add("Administrator report",CHAT_ADMIN);
            add("Unrelated sector report",CHAT_FRIENDLY_SECTOR);add("Noncritical notification",CHAT_EVERYONE,false);
            add("Commander: "+std::string(40,'x')+" A long message should wrap and indent rather than overwrite the scrollbar or chat editor.",CHAT_EVERYONE,true,true);
            ui.open(Screen::RemoteLobby,&state);
            auto visible=[&](std::string_view text){return std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](auto& c){return c.name.starts_with("lobbyChat/") && c.text.find(text)!=c.text.npos;});};
            auto item=[&](MenuOp op,int value){auto i=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](auto& v){return v.action.op==op && v.action.value==value;});require(i!=ui.view().items.end(),"original chat test control present");return size_t(i-ui.view().items.begin());};
            ui.activate(item(MenuOp::LobbyChatChannel,0));
            require(visible("Private report") && visible("Urgent private") && visible("Administrator") && !visible("Team-only") && !visible("Leader-only") && !visible("Unrelated sector") && !visible("Noncritical"),"original All tab filters channels but includes PM/nofilter/admin");
            auto bold=std::count_if(ui.view().artwork.begin(),ui.view().artwork.end(),[](auto& c){return c.name.starts_with("lobbyChat/") && c.font=="smallBoldFont";});
            require(bold>1 && std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](auto& c){return c.name.starts_with("lobbyChat/") && c.text.starts_with("  ");}),"original commander message wraps with two-space continuation and bold bitmap font");
            for(const auto& c:ui.view().artwork)if(c.name.starts_with("lobbyChat/"))require(c.clip==std::optional(glm::vec4(270,408,407,123)) && c.singleLine && c.bounds.x==273,"original lobby chat uses authored viewport, three-pixel inset and clipped lines");
            ui.text("Unsent draft");require(ui.pageChat(true) && visible("All history") && ui.chatText()=="Unsent draft","mapped Page Up action scrolls original chat without changing draft or pilot recipient");
            auto first=std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[](auto& c){return c.name.starts_with("lobbyChat/");})->text;
            add("New report while reading",CHAT_EVERYONE);ui.sync(state);
            require(std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[](auto& c){return c.name.starts_with("lobbyChat/");})->text==first && !visible("New report"),"new chat does not move a manually paged history");
            state.messages.pop_front();ui.sync(state);
            require(std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[](auto& c){return c.name.starts_with("lobbyChat/");})->text==first,"expiring older messages retains the line being read");
            add("Unread team report",CHAT_TEAM);ui.sync(state);ui.tick(0);
            auto headerState=[&]{auto i=std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[](auto& c){return c.name=="chatHeader1";});require(i!=ui.view().artwork.end(),"original team chat header present");return i->checkedState;};
            auto blink=headerState();for(int tries=0;tries<20 && headerState()==blink;++tries){std::this_thread::sleep_for(std::chrono::milliseconds(15));ui.tick(0);}require(headerState()!=blink && (headerState()==0 || headerState()==2),"unread human team chat uses original quarter-second checked2 flashing without rebuilding history");
            auto resident=ui.view().artwork.data();ui.sync(state);require(ui.view().artwork.data()==resident,"unchanged lobby reports retain resident chat layout");
            auto thumb=item(MenuOp::ScrollThumb,-6);auto b=ui.view().items[thumb].bounds;ui.pointerMove(glm::vec2(b)+glm::vec2(3));ui.pointerPress(thumb);ui.pointerMove({b.x+3,10000});ui.pointerRelease({});require(visible("New report"),"original chat thumb reaches newest message");
            ui.pointerMove({400,450});require(ui.pointerWheel(1) && !visible("New report"),"chat wheel scrolls history rather than roster selection");
            ui.activate(item(MenuOp::LobbyChatChannel,1));require(headerState()==0 && visible("Team-only") && visible("Private report") && !visible("All history") && !visible("Leader-only"),"original Team tab clears unread indication and filters retained history");
            auto recipientBefore=ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);ui.key(SDLK_TAB,SDL_SCANCODE_TAB);require(ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).value==recipientBefore.value && ui.chatText()=="Unsent draft","Tab cannot cycle original lobby chat into a flight-only channel");
            ui.activate(item(MenuOp::LobbyChatChannel,2));require(visible("Leader-only") && visible("Private report") && !visible("Team-only"),"original Leaders tab filters retained history");
            ui.chatSent();ui.text("private RECIPIENT:Hello through original name prefix");auto action=ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
            require(action.effect==FrontEffect::SendChat && action.value==int(ChatChannel::Target) && action.targetId==8 && ui.outgoingChatText()=="Hello through original name prefix","original lobby name-prefix routing is case-insensitive and strips only a recognized recipient prefix");
            ui.chatSent();ui.text("admin:Operator message");action=ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);require(action.value==int(ChatChannel::Admin) && ui.outgoingChatText()=="Operator message","original lobby admin prefix overrides current channel");
            puts("Original lobby chat: authored wrapping/fonts/clipping, channel filters/unread flashing, paging, wheel/thumb scrolling, stable draft/expired history, original Tab and recipient prefixes passed");
        }
        {
            Settings prefs;Frontend setup(prefs);setup.originalInterface(assets);OriginalUi authored(assets);
            SessionOptions initial;initial.scenario=Scenario::Conquest;initial.rules.teams=4;initial.localPilots={{"Other pilot",3,true}};initial.team=3;
            setup.networkSession(initial);setup.open(Screen::Scenarios);setup.open(Screen::Lobby);
            auto control=[&](std::string_view name){for(size_t i=0;i<setup.view().items.size();++i)if(setup.view().items[i].control==name)return i;throw std::runtime_error("Original setup control missing: "+std::string(name));};
            auto click=[&](std::string_view name){return setup.activate(control(name));};
            auto option=[&](std::string_view name,int choice){click(name);for(size_t i=0;i<setup.view().items.size();++i)if(setup.view().items[i].action.op==MenuOp::SetupChoice && setup.view().items[i].action.delta==choice)return setup.activate(i);throw std::runtime_error("Original setup choice missing");};
            if(std::count_if(setup.view().items.begin(),setup.view().items.end(),[](const auto& item){return item.action.op==MenuOp::SetupCombo;})!=21 || !setup.view().original)throw std::runtime_error("Original setup does not expose all authored combo controls");
            auto lives=control("livesComboPane");auto b=setup.view().items[lives].bounds;setup.pointerMove(glm::vec2(b)+glm::vec2(3));setup.pointerPress(lives);
            auto row=std::find_if(setup.view().items.begin(),setup.view().items.end(),[](const auto& item){return item.action.op==MenuOp::SetupChoice && item.action.delta==1;});
            if(row==setup.view().items.end() || row->bounds.y>=b.y || setup.pointerRelease(menuHit(setup.view(),{1000,700},1,originalMenuLayout({1000,700}).pixel(glm::vec2(b)+glm::vec2(3)))).effect!=FrontEffect::None || setup.session().rules.lives!=32767)throw std::runtime_error("Combo does not open above the pointer on press or its stationary opening release selects a row");
            auto behind=control("friendlyFireCheckboxPane");const auto& behindItem=setup.view().items[behind];
            if(!behindItem.enabled || !behindItem.blocked || menuHit(setup.view(),{1000,700},1,originalMenuLayout({1000,700}).pixel(glm::vec2(behindItem.bounds)+glm::vec2(3))))throw std::runtime_error("Popup changes parent enabled faces or lets clicks pass through");
            setup.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);if(setup.screen()!=Screen::Lobby)throw std::runtime_error("Closing a setup combo leaves game settings");
            option("livesComboPane",1);if(setup.session().rules.lives!=0)throw std::runtime_error("One life is not zero allowed deaths");
            option("connectivityComboPane",0);if(setup.session().rules.randomEncounters!=7)throw std::runtime_error("Low connectivity has reversed original meaning");
            option("teamCountComboPane",0);if(setup.session().rules.teams!=2 || setup.session().team!=1 || setup.session().localPilots[0].team!=1)throw std::runtime_error("Setup team count leaves pilots on removed sides");
            auto resources=authored.choices("newgamescreen","ResourcesNames",{"ResourcesNeutralMinable","ResourcesPlayerMinable","ResourcesNeutralSpecial","ResourcesPlayerSpecial"});
            option("resourcesComboPane",7);const auto& r=setup.session().rules;auto v=resources[7].values;
            if(r.neutralMineable!=int(v[0]) || r.homeMineable!=int(v[1]) || r.neutralSpecial!=int(v[2]) || r.homeSpecial!=int(v[3]))throw std::runtime_error("Resource choice updates only one of its four original fields");
            auto treasures=authored.choices("newgamescreen","TreasuresNames",{"TreasureHomeValues","TreasureNeutralValues","TreasureRateHomeValues","TreasureRateNeutralValues"});
            for(size_t i=0;i<treasures.size();++i){option("treasuresComboPane",int(i));auto& values=treasures[i].values;auto& rules=setup.session().rules;
                if(rules.homeTreasures!=int(values[0]) || rules.neutralTreasures!=int(values[1]) || rules.homeTreasureRate!=float(values[2]) || rules.neutralTreasureRate!=float(values[3]))throw std::runtime_error("Treasure counts or per-minute rates differ from original ReadControls");}
            option("mapTypeComboPane",2);if(conquestMapIds[setup.session().map]!=2)throw std::runtime_error("Authored map IDs are mistaken for native map indices");
            click("friendlyFireCheckboxPane");setup.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            if(setup.screen()!=Screen::Scenarios || setup.session().rules.friendlyFire!=initial.rules.friendlyFire || setup.session().map!=initial.map || setup.session().rules.lives!=initial.rules.lives || setup.session().localPilots[0].team!=3 || setup.session().team!=3)throw std::runtime_error("Cancelling game settings commits part of the draft");
            setup.open(Screen::Lobby);
            if(setup.view().items[control("livesComboPane")].label!="The number of lives each player has.")throw std::runtime_error("Original setup loses its authored hover explanation");
            option("teamCountComboPane",4);option("mapTypeComboPane",12);option("mapSizeComboPane",1);
            auto mapControl=control("mapPreviewPane");auto mapBounds=setup.view().items[mapControl].bounds;
            setup.pointerMove(glm::vec2(mapBounds)+glm::vec2(4));
            if(std::none_of(setup.view().artwork.begin(),setup.view().artwork.end(),[](const auto& c){return c.font=="largeBoldFont" && c.text.find(" sectors")!=std::string::npos;}))throw std::runtime_error("Map preview hover does not show original sector and aleph counts");
            if(setup.view().items[control("mapPreviewPane")].enabled){setup.pointerPress(control("mapPreviewPane"));setup.pointerMove(glm::vec2(mapBounds)+glm::vec2(-40,200));setup.pointerRelease({});}
            setup.pointerMove({0,0});
            for(const auto& c:setup.view().artwork)if(c.kind==OriginalControl::Kind::Rectangle && c.color==glm::vec4(.5,.5,.5,1) && !c.clip)throw std::runtime_error("Map preview lines escape the authored frame");
            for(size_t i=0;i<Allegiance::GameTypes.size();++i){option("gameTypeComboPane",int(i));const auto& p=Allegiance::GameTypes[i];const auto& rules=setup.session().rules;
                if(rules.conquest!=p.values.conquest || rules.territory!=p.values.territory || rules.flags!=p.values.flags || rules.artifacts!=p.values.artifacts || rules.teamKills!=p.values.kills || rules.experimental!=p.values.experimental || conquestMapIds[setup.session().map]!=p.map)throw std::runtime_error("Authored game preset differs from WinTrek");}
            option("gameTypeComboPane",0);option("initialMinersCountComboPane",10);option("maxDronesCountComboPane",0);click("createButtonPane");
            if(setup.screen()!=Screen::Lobby || !setup.hasMessageBox() || setup.session().rules.miners!=10)throw std::runtime_error("Invalid miners/drones combination silently mutates or starts the mission");
            setup.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
            option("initialMinersCountComboPane",1);option("maxDronesCountComboPane",4);option("teamCountComboPane",0);option("mapTypeComboPane",0);
            if(click("createButtonPane").effect!=FrontEffect::None || setup.screen()!=Screen::LobbyPilots)throw std::runtime_error("Accept does not stage the original team lobby before starting");
            click("sideReadyButtonPane");setup.open(Screen::Lobby);click("friendlyFireCheckboxPane");setup.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            if(setup.screen()!=Screen::LobbyPilots || !setup.view().items[control("sideReadyButtonPane")].selected)throw std::runtime_error("Cancelling a settings draft changes prior team readiness");
            setup.open(Screen::Lobby);option("gameTypeComboPane",5);option("deathWatchKillsComboPane",1);click("createButtonPane");
            if(setup.screen()!=Screen::LobbyPilots || setup.session().rules.experimental)throw std::runtime_error("A custom game inherits the Experimental flag despite original ReadControls");
            GameState remote;remote.networked=remote.networkLobby=true;setup.open(Screen::RemoteLobby,&remote);setup.open(Screen::Lobby,&remote);
            if(setup.view().items[control("friendlyFireCheckboxPane")].enabled || setup.view().items[control("livesComboPane")].enabled || std::any_of(setup.view().items.begin(),setup.view().items.end(),[](auto& item){return item.action.op==MenuOp::SetupAccept;}))throw std::runtime_error("Nonhost can edit network game settings");
            setup.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);remote.networkHost=true;setup.open(Screen::Lobby,&remote);click("livesComboPane");remote.networkHost=false;setup.sync(remote);
            if(std::any_of(setup.view().items.begin(),setup.view().items.end(),[](auto& item){return item.action.op==MenuOp::SetupChoice;}) || setup.view().items[control("livesComboPane")].enabled)throw std::runtime_error("Losing host authority retains an actionable setup menu");
            setup.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);remote.networkHost=true;setup.open(Screen::Lobby,&remote);option("livesComboPane",2);
            if(click("createButtonPane").effect!=FrontEffect::NetworkConfigure || setup.session().rules.lives!=1)throw std::runtime_error("Host settings do not produce the network configuration action");
            puts("Original game setup: 21 combos, preset policy, resource/rate mappings, draft cancellation, invalid settings and host authority passed");
        }
        {
            Settings prefs;Frontend teams(prefs);teams.originalInterface(assets);
            auto civs=Game::civilizations(assets.root());teams.civilizations(civs);
            SessionOptions options;options.scenario=Scenario::Conquest;options.team=1;teams.networkSession(options);
            GameState state;state.playerSide=1;state.networked=state.networkLobby=state.commander=state.networkReady=true;
            state.networkHost=false;state.networkHostPilot=10;state.forceReady=2;
            state.networkPilots={{10,0,"Owner",true,true,true,false,false},{20,1,"Commander",true,false,true,true,false}};
            state.teamScores.resize(2);state.teamScores[1].name="Veterans";state.teamScores[1].civilization=civs.front().id;state.teamScores[1].civilizationIcon=civs.front().icon;state.teamScores[1].color={8.f/255,184.f/255,184.f/255,1};
            teams.open(Screen::RemoteLobby,&state);
            auto icon=civs.front().icon;std::transform(icon.begin(),icon.end(),icon.begin(),[](unsigned char c){return std::tolower(c);});auto watermark=assets.image(icon+"lobbybmp",false);
            if(std::none_of(teams.view().artwork.begin(),teams.view().artwork.end(),[&](const auto& c){return c.image==watermark && c.bounds==glm::vec4(268,71,watermark->width,watermark->height);}) ||
               std::none_of(teams.view().artwork.begin(),teams.view().artwork.end(),[&](const auto& c){return c.kind==OriginalControl::Kind::Rectangle && c.name=="waterMarkPane" && c.color==glm::vec4(8.f/255,184.f/255,184.f/255,.5f);}))throw std::runtime_error("Original non-keyed faction watermark, pane placement or premultiplied tint missing");
            auto item=[&](std::string_view name){for(size_t i=0;i<teams.view().items.size();++i)if(teams.view().items[i].control==name)return i;throw std::runtime_error("Team settings control missing: "+std::string(name));};
            auto click=[&](std::string_view name){return teams.activate(item(name));};
            if(!teams.view().items[item("teamSettingsButtonPane")].enabled || click("teamSettingsButtonPane").effect!=FrontEffect::LobbyVisit || !teams.wantsText())throw std::runtime_error("Non-owner team leader cannot open the authored settings dialog or withdraw readiness");
            auto focusedEdits=[&]{return std::count_if(teams.view().artwork.begin(),teams.view().artwork.end(),[](const auto& c){return c.textMode==OriginalControl::TextMode::Edit && c.editFocused;});};
            if(focusedEdits()!=1)throw std::runtime_error("Team dialog paints simultaneous name and covered-chat cursors");
            OriginalUi authored(assets);auto dialog=authored.layout("teamscreen","TeamSettingsDialog",{{"showSquad",0}});
            auto origin=glm::floor((glm::vec2(800,600)-glm::vec2(dialog.front().bounds.z,dialog.front().bounds.w))*.5f);
            for(const auto& c:dialog)if(c.name=="teamNameEditPane" || c.name=="civComboPane" || c.name=="okButtonPane" || c.name=="cancelButtonPane")
                if(teams.view().items[item(c.name)].bounds!=c.bounds+glm::vec4(origin,0,0))throw std::runtime_error("Authored team settings artwork/control placement differs");
            if(!teams.view().items[item("teamSettingsButtonPane")].blocked)throw std::runtime_error("Team dialog leaks input into its lobby backdrop");
            teams.text(" discarded");teams.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            if(teams.session().teamNames[1]!="" || !teams.wantsText() || teams.view().items[item("teamSettingsButtonPane")].blocked)throw std::runtime_error("Cancel changes the team settings or loses lobby chat focus");
            if(focusedEdits()!=1)throw std::runtime_error("Closing team dialog fails to restore the single lobby chat cursor");
            click("teamSettingsButtonPane");click("civComboPane");
            if(teams.wantsText())throw std::runtime_error("Faction dropdown accepts chat/name text");
            auto choice=std::find_if(teams.view().items.begin(),teams.view().items.end(),[&](const auto& i){return i.action.op==MenuOp::TeamFactionChoice && i.action.value==civs[1].id;});
            if(choice==teams.view().items.end())throw std::runtime_error("Faction menu uses list indices instead of core civilization identities");
            auto nameBounds=teams.view().items[item("teamNameEditPane")].bounds;
            if(std::none_of(teams.view().artwork.begin(),teams.view().artwork.end(),[&](const auto& c){return c.textMode==OriginalControl::TextMode::Edit && c.bounds==nameBounds && c.editFocused;}))throw std::runtime_error("Faction dropdown incorrectly removes the underlying name-field focus");
            teams.activate(size_t(choice-teams.view().items.begin()));if(!teams.wantsText())throw std::runtime_error("Closing faction dropdown loses original name-field input");click("teamNameEditPane");
            for(int i=0;i<8;++i)teams.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);
            teams.text("Seasoned Pilots");teams.text("012345678901234567890123456789");
            auto accepted=teams.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
            if(accepted.effect!=FrontEffect::TeamSettings || accepted.value!=civs[1].id || teams.teamSettingsName()!="Seasoned Pilots012345678" || teams.session().teamNames[1]!="")throw std::runtime_error("Team dialog does not stage name/faction until authoritative acceptance, or rejects the fitting prefix at the original name limit");
            state.teamScores[1].name="Authoritative name";state.teamScores[1].civilization=civs[1].id;teams.sync(state);
            click("teamSettingsButtonPane");if(teams.teamSettingsName()!="Authoritative name")throw std::runtime_error("Next team edit uses stale replicated team data");
            state.commander=false;teams.sync(state);
            if(std::any_of(teams.view().items.begin(),teams.view().items.end(),[](const auto& i){return i.control=="teamNameEditPane" || i.control=="teamSettingsButtonPane";}))throw std::runtime_error("Loss of leadership leaves team settings writable");
            state.commander=state.networkHost=true;state.missionStage=STAGE_NOTSTARTED;state.networkCountdown=35;state.lobbyFlags=Allegiance::LobbyAutoRestart;
            teams.open(Screen::RemoteLobby,&state);
            require(teams.view().items[item("settingsButtonPane")].enabled && !teams.view().items[item("startButtonPane")].enabled && teams.view().items[item("teamSettingsButtonPane")].enabled,"automatic restart delay changes original mission/team settings availability");
            click("teamSettingsButtonPane");teams.text(" draft");auto draft=teams.teamSettingsName();state.networkCountdown=34;teams.sync(state);
            require(teams.teamSettingsName()==draft && std::any_of(teams.view().items.begin(),teams.view().items.end(),[](const auto& i){return i.control=="teamNameEditPane";}),"automatic delay retires the original team-settings draft before briefing");
            state.missionStage=STAGE_STARTING;state.networkCountdown=14;teams.sync(state);
            require(teams.keyboardPopup() && !teams.wantsText() && std::none_of(teams.view().items.begin(),teams.view().items.end(),[](const auto& i){return i.control=="teamNameEditPane";}),"actual briefing does not replace the original team-settings dialog");
            state.missionStage=STAGE_STARTED;state.networkLobby=false;state.networkCountdown=-1;state.lobbyFlags=4;teams.sync(state);
            require(teams.view().items[item("teamSettingsButtonPane")].enabled,"in-progress team-settings control differs from the original lock-only gate");
            click("teamSettingsButtonPane");teams.sync(state);
            require(teams.wantsText() && std::any_of(teams.view().items.begin(),teams.view().items.end(),[](const auto& i){return i.control=="teamNameEditPane";}),"live mission state prematurely closes the original team-settings dialog");
            teams.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);state.missionStage=STAGE_OVER;state.networkLobby=true;state.lobbyFlags=0;teams.open(Screen::RemoteLobby,&state);
            require(teams.view().items[item("detailsButtonPane")].enabled && std::none_of(teams.view().items.begin(),teams.view().items.end(),[](const auto& i){return i.control=="settingsButtonPane";}) && !teams.view().items[item("startButtonPane")].enabled,"ended mission exposes mutable game settings or Start");
            teams.open(Screen::Lobby,&state);require(std::none_of(teams.view().items.begin(),teams.view().items.end(),[](const auto& i){return i.action.op==MenuOp::SetupAccept;}),"direct setup navigation bypasses ended mission stage");
            state.commander=true;options.rules.lockTeamSettings=true;teams.networkSession(options);teams.open(Screen::RemoteLobby,&state);
            if(teams.view().items[item("teamSettingsButtonPane")].enabled)throw std::runtime_error("Locked team settings remain enabled");
            puts("Original team dialog: authored artwork, non-owner leader permissions, focus, cancel, staged acceptance, faction identities, name limit and live authority updates passed");
        }
        {
            Settings prefs;Frontend briefing(prefs);briefing.originalInterface(assets);QuickChatCatalog catalog(assets);briefing.quickChats(catalog);
            float winMoney{};auto civs=Game::civilizations(assets.root(),&winMoney);briefing.civilizations(civs,winMoney);
            SessionOptions options;options.scenario=Scenario::Conquest;options.rules.teams=3;options.rules.artifacts=3;options.rules.flags=2;
            options.rules.teamKills=10;options.rules.minutes=15;options.rules.prosperity=1;options.rules.conquest=75;
            for(size_t faction=0;faction<civs.size();++faction){
                briefing.networkSession(options);GameState state;state.networked=state.networkLobby=state.networkHost=state.networkReady=true;
                state.networkCountdown=9;state.missionStage=STAGE_STARTING;state.sector="Wasp";state.playerSide=0;state.teamScores.resize(3);
                for(int side=0;side<3;++side){state.teamScores[side].name=side?"Opponent":"Veterans";state.teamScores[side].civilization=civs[(faction+side)%civs.size()].id;state.teamScores[side].civilizationIcon=civs[(faction+side)%civs.size()].icon;}
                state.networkPilots={{10,0,"Commander",true,true,true,true,false}};briefing.open(Screen::RemoteLobby,&state);
                auto control=[&](const char* name)->const OriginalControl&{auto i=std::find_if(briefing.view().artwork.begin(),briefing.view().artwork.end(),[&](const auto& c){return c.name==name;});require(i!=briefing.view().artwork.end(),"authored mission briefing control missing");return *i;};
                const auto& body=control("missionBriefingTextBody");
                require(body.font=="medVerdana" && body.clip && body.bounds.z==306 && body.text.find("Veterans")!=std::string::npos && body.text.find("Wasp")!=std::string::npos && body.text.find("75%")!=std::string::npos && body.text.find(std::to_string(int(winMoney)))!=std::string::npos,"briefing must use authored faction text, objectives, location, font and clipping");
                require(control("missionBriefingCountDown").text=="9" && control("missionTipText").text!="" && control("missionBriefingTeamImage").clip && control("mapPreviewPane").image,"original countdown, tip, team/civilization icon and topology preview");
                auto map=std::find_if(briefing.view().artwork.rbegin(),briefing.view().artwork.rend(),[](const auto& c){return c.name=="mapPreviewPane";});
                auto clip=glm::vec4(glm::vec2(map->bounds)+glm::vec2(1),map->bounds.z-2,map->bounds.w-2);
                require(std::any_of(briefing.view().artwork.begin(),briefing.view().artwork.end(),[&](const auto& c){return c.kind==OriginalControl::Kind::Rectangle && c.clip && *c.clip==clip;}),"briefing topology must be composed into its own pane, not the underlying team-screen preview");
                require(!briefing.wantsText() && !briefing.lobbyQuickChat() && std::all_of(briefing.view().items.begin(),briefing.view().items.end(),[](const auto& item){return item.blocked;}),"briefing must block underlying lobby input");
                require(briefing.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE).effect==FrontEffect::None && briefing.screen()==Screen::RemoteLobby,"network briefing cannot be dismissed by Escape");
                state.networkCountdown=8;briefing.sync(state);require(control("missionBriefingCountDown").text=="8","authoritative countdown updates original briefing");
                state.networkCountdown=-1;state.missionStage=STAGE_NOTSTARTED;briefing.sync(state);require(briefing.wantsText() && std::none_of(briefing.view().artwork.begin(),briefing.view().artwork.end(),[](const auto& c){return c.name=="missionBriefingCountDown";}),"canceled countdown retires briefing and restores lobby focus");
            }
            puts("Original mission briefing: every core faction, original objectives/tips/fonts/clipping, team icon, map preview, input isolation, countdown and cancellation passed");
        }
        {
            Settings prefs;Frontend lobby(prefs);lobby.originalInterface(assets);
            SessionOptions options;options.scenario=Scenario::Conquest;lobby.networkSession(options);
            GameState state;state.networked=state.networkLobby=state.networkHost=state.commander=true;
            state.networkReady=true;state.networkHostPilot=10;state.autoAccept=3;
            state.networkPilots={{10,0,"Commander",true,true,true,true,false},{20,noTeam,"Applicant",true,false,false,false,false,0,0}};
            lobby.open(Screen::RemoteLobby,&state);
            auto item=[&](std::string_view name){for(size_t i=0;i<lobby.view().items.size();++i)if(lobby.view().items[i].control==name)return i;throw std::runtime_error("Original lobby control missing: "+std::string(name));};
            auto click=[&](std::string_view name){return lobby.activate(item(name));};
            if(!lobby.view().items[item("autoAcceptButtonPane")].selected || lobby.view().items[item("sideReadyButtonPane")].selected || lobby.view().items[item("awayFromKeyboardButtonPane")].selected)throw std::runtime_error("Original Auto Accept, force-ready and personal Away states are conflated");
            auto away=click("awayFromKeyboardButtonPane"),force=click("sideReadyButtonPane");
            if(away.effect!=FrontEffect::NetworkReady || away.value!=0 || force.effect!=FrontEffect::LobbyAction || force.command!=int(LobbyAction::ForceReady) || force.value!=1)throw std::runtime_error("Personal Away and commander force-ready route to the same state");
            auto label=[&](std::string_view name){auto i=std::find_if(lobby.view().artwork.begin(),lobby.view().artwork.end(),[&](const auto& c){return c.name==name;});if(i==lobby.view().artwork.end())throw std::runtime_error("Original lobby text missing");return i->text;};
            require(label("textStatus")=="READY TO START" && lobby.view().items[item("startButtonPane")].enabled,"original ready status and Start gate");
            auto randomize=click("randomizeButtonPane");require(randomize.effect==FrontEffect::LobbyAction && randomize.command==int(LobbyAction::Randomize),"original Randomize routes to owner-authorized server action");
            state.networkPilots.push_back({30,0,"Wingman",true,false,false,false,false});lobby.sync(state);
            require(label("textStatus")=="TEAMS ARE UNBALANCED" && !lobby.view().items[item("startButtonPane")].enabled,"unbalanced teams must disable original Start");
            state.networkPilots.back().team=1;state.networkPilots.back().ready=false;lobby.sync(state);
            require(label("textStatus2")=="IS NOT READY" && !lobby.view().items[item("startButtonPane")].enabled,"not-ready team status must use original second line");
            state.networkPilots.back().ready=true;state.networkCountdown=9;state.missionStage=STAGE_STARTING;lobby.sync(state);
            require(label("textStatus")=="COUNTDOWN IN PROGRESS" && !lobby.view().items[item("startButtonPane")].enabled && lobby.view().items[item("randomizeButtonPane")].enabled,"countdown disables Start while the original owner Randomize remains enabled");
            state.networkPilots[0].team=noTeam;lobby.sync(state);
            require(label("textTimerType")=="STARTING IN:" && label("textTimer")=="9","No Team sees original countdown label and timer");
            require(std::any_of(lobby.view().items.begin(),lobby.view().items.end(),[](const auto& i){return i.action.op==MenuOp::RosterSelect && i.action.delta==10;}),"membership change must select the original No Team roster");
            state.networkCountdown=-1;state.missionStage=STAGE_STARTED;state.lobbyFlags=4;state.time=3661;lobby.sync(state);
            require(label("textStatus")=="GAME IN PROGRESS" && label("textTimerType")=="TIME ELAPSED:" && label("textTimer")=="1:01:01","in-progress original lobby elapsed timer");
            GameMessage notification{};notification.serial=1;notification.modal=true;notification.text="You have been removed from your team because the Game Commander flushed all teams.";
            state.messages.push_back(notification);lobby.sync(state);require(lobby.hasMessageBox(),"team flush warning must use original modal");
            lobby.key(SDLK_RETURN,SDL_SCANCODE_RETURN);lobby.sync(state);require(!lobby.hasMessageBox(),"repeated snapshot must not reopen team flush modal");
            lobby.networkSession(options);lobby.open(Screen::RemoteLobby,&state);lobby.sync(state);
            require(!lobby.hasMessageBox(),"retained previous-round history must not reopen acknowledged Flush/Balance warning");
            state.messages.clear();state.networkPilots.pop_back();state.networkPilots[0].team=0;state.lobbyFlags=0;state.time=0;lobby.sync(state);
            auto applicant=std::find_if(lobby.view().items.begin(),lobby.view().items.end(),[](const auto& i){return i.action.op==MenuOp::RosterSelect && i.action.delta==20;});
            if(applicant==lobby.view().items.end() || std::none_of(lobby.view().artwork.begin(),lobby.view().artwork.end(),[](const auto& c){return c.text=="Requesting" && c.name=="request/20";}))throw std::runtime_error("Unassigned applicant is absent from the commander roster");
            lobby.activate(size_t(applicant-lobby.view().items.begin()));auto accepted=click("acceptButtonPane");
            if(accepted.effect!=FrontEffect::LobbyAction || accepted.command!=int(LobbyAction::Accept) || accepted.targetId!=20 || lobby.view().items[item("makeLeaderButtonPane")].enabled)throw std::runtime_error("Accept does not use the selected applicant identity, or requester can become commander");
            click("noTeam");auto depart=click("joinButtonPane");
            if(depart.effect!=FrontEffect::NetworkJoinTeam || depart.value!=noTeam)throw std::runtime_error("Original No Team tab does not send the actual lobby side ID");
            if(std::any_of(lobby.view().artwork.begin(),lobby.view().artwork.end(),[](const auto& c){return c.name=="sideReadyButtonPane" || c.name=="acceptButtonPane";}))throw std::runtime_error("Commander controls remain visible on another roster");
            state.networkPilots[0].commander=state.commander=state.networkHost=false;state.networkHostPilot=30;
            lobby.open(Screen::RemoteLobby,&state);
            if(std::any_of(lobby.view().artwork.begin(),lobby.view().artwork.end(),[](const auto& c){return c.name=="sideReadyButtonPane" || c.name=="autoAcceptButtonPane" || c.name=="settingsButtonPane";}))throw std::runtime_error("Nonleader/nonhost retains original privileged control artwork");
            state.networkPilots[0].team=noTeam;state.networkPilots[0].ready=false;state.networkReady=false;state.lobbyFlags=4;
            lobby.sync(state);
            if(!lobby.view().items[item("awayFromKeyboardButtonPane")].enabled || !lobby.view().items[item("awayFromKeyboardButtonPane")].selected)throw std::runtime_error("No Team cannot change Away while the mission is running");
            state.networkPilots[0].requested=1;lobby.sync(state);
            auto cancel=click("joinButtonPane");
            if(cancel.effect!=FrontEffect::LobbyAction || cancel.command!=int(LobbyAction::CancelRequest) ||
                std::none_of(lobby.view().artwork.begin(),lobby.view().artwork.end(),[](const auto& c){return c.name=="textPrompt" && c.text=="COMMANDER, YOU ARE REQUESTING TO JOIN A TEAM ";}))throw std::runtime_error("Original JOIN TEAM does not cancel a pending request or use the authored prompt");
            if(std::any_of(lobby.view().artwork.begin(),lobby.view().artwork.end(),[](const auto& c){return c.name=="startButtonPane" || c.name=="chatHeader2";}))throw std::runtime_error("Nonhost/nonleader retains original Start or commander chat artwork");
            puts("Original lobby controls: separate Away/force-ready, role visibility, pending applicants, stable acceptance identity and No Team departure passed");
        }
        {
            int valid=0;
            for(int map=0;map<int(conquestMapIds.size());++map)for(int teams=2;teams<=6;++teams)for(int size=0;size<=1;++size){
                SessionOptions options;options.scenario=Scenario::Conquest;options.map=map;options.rules.teams=teams;options.rules.mapSize=size;
                auto preview=Game::mapPreview(options);if(Game::validMap(map,teams)){
                    if(!preview.error.empty() || preview.sectors.empty() || (preview.sectors.size()>1 && preview.links.empty()))throw std::runtime_error(std::string("Original valid map has no native preview: ")+conquestMapNames[map]+" / "+std::to_string(teams)+" / "+preview.error);
                    for(const auto& sector:preview.sectors)if(!std::isfinite(sector.position.x) || !std::isfinite(sector.position.y))throw std::runtime_error("Preview contains invalid screen positions");
                    for(auto link:preview.links)for(int endpoint:link)if(std::none_of(preview.sectors.begin(),preview.sectors.end(),[&](const auto& s){return s.id==endpoint;}))throw std::runtime_error("Preview aleph refers to a missing sector");
                    ++valid;
                }else if(preview.error.empty())throw std::runtime_error("Original invalid map/team combination was accepted by preview");
            }
            ImissionPV preview(nullptr);MissionParams params;params.nTeams=2;params.mmMapType=c_mmPinWheel;
            if(ImapPVMaker::Create(&params,&preview))throw std::runtime_error("Cannot generate preview lifetime check");
            std::vector<CMapPVCluster*> sectors;std::vector<CwarpPV*> warps;
            for(auto* link=preview.GetClusters()->first();link;link=link->next()){link->data()->AddRef();sectors.push_back(link->data());}
            for(auto* link=preview.GetWarps()->first();link;link=link->next()){link->data()->AddRef();link->data()->GetDestination();warps.push_back(link->data());}
            preview.Clear();preview.Clear();
            for(auto* warp:warps){if(warp->GetCount()!=1)throw std::runtime_error("Map preview retains destination/membership references after Clear");warp->Release();}
            for(auto* sector:sectors){if(sector->GetCount()!=1)throw std::runtime_error("Map preview retains generated sector ownership after Clear");sector->Release();}
            printf("Original map preview: %d valid map/team/size combinations, invalid settings and reference-cycle teardown passed\n",valid);
        }
        QuickChatCatalog quickChat(assets);ui.quickChats(quickChat);ui.setPlaying(true);GameState chatState;
        {
            Settings preferences;Frontend lobby(preferences);lobby.originalInterface(assets);lobby.quickChats(quickChat);
            for(auto screen:{Screen::RemoteLobby,Screen::Pilots,Screen::LobbyPilots,Screen::Results}){
                GameState state;state.docked=true;state.complete=screen==Screen::Results;
                lobby.open(Screen::Research,&state);lobby.open(screen,&state);
                if(lobby.closingConsole() || lobby.view().slide)throw std::runtime_error("Lobby/postgame retained a closing flight-console animation");
                lobby.text("unsent draft");
                auto draft=lobby.chatText();
                if(!lobby.lobbyQuickChat() || lobby.wantsText())throw std::runtime_error("Original lobby quick chat fails to capture input");
                size_t rows=0;for(auto& item:lobby.view().items)if(item.popupLevel==0){
                    if(item.bounds.x!=11 || item.bounds.y<11 || item.label!=quickChat.menus[0].items[rows++].label)throw std::runtime_error("Lobby quick-chat rows differ from authored menu or original corner anchor");
                }
                if(rows!=quickChat.menus[0].items.size())throw std::runtime_error("Lobby quick-chat popup replaced its parent or lost rows");
                auto sent=lobby.key(SDLK_1,SDL_SCANCODE_1);
                if(sent.effect!=FrontEffect::QuickChat || !sent.voiceOnly || sent.command!=int(screen==Screen::Results?ChatChannel::Everyone:ChatChannel::Team) || quickChat.commands.at(sent.value).text.find("Affirmative")==std::string::npos || lobby.screen()!=screen)
                    throw std::runtime_error("Lobby quick chat changes screen, route or delivers a flight command");
                lobby.text("1");if(lobby.chatText()!=draft)throw std::runtime_error("Quick-chat accelerator leaked into unsent lobby text");
                if(!lobby.lobbyQuickChat())throw std::runtime_error("Lobby quick chat cannot reopen after sending");
                lobby.key(SDLK_A,SDL_SCANCODE_A);
                if(std::none_of(lobby.view().items.begin(),lobby.view().items.end(),[](auto& item){return item.popupLevel==1;}))throw std::runtime_error("Lobby quick-chat authored submenu did not cascade");
                lobby.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
                if(std::any_of(lobby.view().items.begin(),lobby.view().items.end(),[](auto& item){return item.popupLevel==1;}))throw std::runtime_error("Lobby quick-chat Escape closed the wrong level");
                lobby.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
                if(lobby.screen()!=screen || lobby.chatText()!=draft || !lobby.wantsText())throw std::runtime_error("Lobby quick-chat cancellation changed parent, draft or focus");
            }
            puts("Original lobby/postgame quick chat: authored cascades, numeric shortcuts, cancellation, draft preservation and voice-only routing passed");
        }
        ui.open(Screen::QuickChat,&chatState);auto quick=ui.key(SDLK_1,SDL_SCANCODE_1);
        if(quick.effect!=FrontEffect::QuickChat || quick.command!=int(ChatChannel::Team) || quickChat.commands.at(quick.value).text.find("Affirmative")==std::string::npos)throw std::runtime_error("Original quick-chat root shortcut or recipient");
        for(int i=2;i<int(ChatChannel::Count);++i){ui.key(SDLK_TAB,SDL_SCANCODE_TAB);auto recipient=ui.key(SDLK_1,SDL_SCANCODE_1);if(recipient.effect!=FrontEffect::QuickChat || recipient.command!=i)throw std::runtime_error("Quick chat cannot reach an original recipient");}
        ui.key(SDLK_A,SDL_SCANCODE_A);if(ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE).effect!=FrontEffect::None || ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE).effect!=FrontEffect::Resume)throw std::runtime_error("Quick-chat submenu back/close");
        ui.setPlaying(false);ui.open(Screen::Home);
        Settings promptSettings;promptSettings.keys[size_t(Binding::Boost)]=SDL_SCANCODE_F10;
        Settings originalPrompts;originalPrompts.importInputMap(assets.root()/"defaultinputmap.mdl");KeyboardBindings originalKeyboard(originalPrompts);
        if(originalKeyboard.command(SDL_SCANCODE_INSERT,SDL_KMOD_NONE)!=TK_AcceptCommand ||
           interfacePrompt("Requester has requested $100  Press the [Insert] key to approve it.",originalPrompts,&originalKeyboard)!="Requester has requested $100  Press the [Insert] key to approve it." ||
           trainingPrompt("Press the INSERT or '[' key to accept the command.",originalPrompts,&originalKeyboard)!="Press the INSERT or '[' key to accept the command.")throw std::runtime_error("Original Insert alias is hidden by the primary bracket binding");
        if(interfacePrompt("New orders from Commander. Press [insert] to accept.",promptSettings)!="New orders from Commander. Press [insert] to accept." ||
           interfacePrompt("Requester has requested $100  Press the [Insert] key to approve it.",promptSettings)!="Requester has requested $100  Press the [Insert] key to approve it." ||
           trainingPrompt("Press the INSERT or '[' key to accept the command.",promptSettings)!="Press the INSERT or '[' key to accept the command.")throw std::runtime_error("Default command prompts alter original text or action");
        promptSettings.keys[size_t(Binding::AcceptOrder)]=SDL_SCANCODE_END;
        promptSettings.keys[size_t(Binding::ExecuteOrder)]=SDL_SCANCODE_HOME;
        if(trainingPrompt("Press the SPACEBAR when you are ready to proceed.",promptSettings)!="Press F10 when you are ready to proceed." ||
           trainingPrompt("Press the G key to activate your cloak.",promptSettings)!="Press K to activate your cloak." ||
           trainingPrompt("Press the INSERT key to accept the command.",promptSettings)!="Press End to accept the command.")throw std::runtime_error("Training prompts do not follow current bindings");
        if(interfacePrompt("Requester has requested $100. Press [insert] to approve it.",promptSettings)!="Requester has requested $100. Press [End] to approve it." ||
           interfacePrompt("New orders from Commander. Press [insert] to accept.",promptSettings)!="New orders from Commander. Press [End] to accept.")throw std::runtime_error("Financial/order tips use the wrong configured keys or action");
        if(interfacePrompt("Requester has requested $100  Press the [Insert] key to approve it.",promptSettings)!="Requester has requested $100  Press the [End] key to approve it.")throw std::runtime_error("Original hull request prompt ignores remapped Accept key");
        promptSettings.keys[size_t(Binding::AcceptOrder)]=SDL_SCANCODE_LEFTBRACKET;
        if(interfacePrompt("New orders from Commander. Press [insert] to accept.",promptSettings)!="New orders from Commander. Press '[' to accept.")throw std::runtime_error("Bracket key prompt has ambiguous nested brackets");

        auto loader=[&](const std::string& name,const std::string& skin){auto m=assets.model(name);if(!skin.empty())assets.image(skin+"bmp");return ModelHandle{0,m->radius,m->frames,m->lights};};
        auto effects=[&](const std::string& name){auto t=assets.image(name);return EffectTexture{0,t->width,t->height,int(std::max(1u,t->width/t->height))};};
        for(int map=0;map<int(mapFiles.size());++map) {
            SessionOptions options;options.scenario=Scenario::FreeFlight;options.map=map;options.enemies=0;options.pilot="Test pilot";
            Game game(assets.root(),loader,0,0,options);game.step({},.01);
            if(game.state().sectors.empty() || game.state().dead)throw std::runtime_error("Map did not create a live session");
            for(auto& c:game.contacts())if(c.type==ContactShip)throw std::runtime_error("Free flight unexpectedly spawned drones");
            printf("Map %s: %zu sectors, %zu contacts\n",mapFiles[map],game.state().sectors.size(),game.contacts().size());
        }
        SessionOptions options;options.scenario=Scenario::Skirmish;options.enemies=3;options.allies=2;
        {SessionOptions transit;transit.scenario=Scenario::FreeFlight;transit.map=3;transit.enemies=0;
            Game game(assets.root(),loader,0,0,transit);
            auto warp=std::find_if(game.contacts().begin(),game.contacts().end(),[](auto& c){return c.type==ContactWarp;});
            if(warp==game.contacts().end())throw std::runtime_error("No aleph for sector transit check");
            std::string sector=game.state().sector;game.selectTarget(warp->type,warp->id);game.toggleAutopilot();
            for(int i=0;i<30000 && game.state().sector==sector && !game.state().dead;++i)game.step({},1./165);
            if(game.state().dead || game.state().sector==sector || !std::isfinite(glm::length(game.state().velocity)))
                throw std::runtime_error("IGC flight through aleph failed");
            puts("Actual autopilot aleph collision changed sector with finite exit kinematics");
        }
        {Game game(assets.root(),loader,0,0,options);int enemy=0,ally=0;
            for(auto& c:game.contacts())if(c.type==ContactShip){if(c.enemy)++enemy;else ++ally;}
            if(enemy!=3 || ally!=2)throw std::runtime_error("Lobby team counts did not reach simulation");
        }
        options.scenario=Scenario::Practice;options.enemies=options.allies=0;
        options.startInHangar=true;
        {Game game(assets.root(),loader,0,0,options);
            auto equipment=game.state().equipment;
            auto mounted=std::find_if(equipment.begin(),equipment.end(),[](auto& e){return e.kind==PartKind::Weapon && e.mount==0;});
            auto guns=game.partChoices(PartKind::Weapon,0);
            auto replacement=std::find_if(guns.begin(),guns.end(),[&](auto& p){return p.name!=mounted->name;});
            auto cargo=game.partChoices(PartKind::Pack,-2);
            auto ammo=std::find_if(cargo.begin(),cargo.end(),[](auto& p){return p.name=="Ammo";});
            if(replacement==guns.end() || ammo==cargo.end() || !game.replacePart(PartKind::Pack,-1,replacement->id) ||
               !game.replacePart(PartKind::Pack,-2,ammo->id) || !game.replacePart(PartKind::Pack,-5,ammo->id))throw std::runtime_error("Cannot provision flight cargo test");
            if(game.dropCargo() || game.reload() || game.swapCargo(PartKind::Weapon,0))throw std::runtime_error("Flight cargo action accepted while docked");
            game.launch();game.selectCargo(-1);
            ui.setPlaying(true);ui.open(Screen::Cargo,&game.state());
            if(choose(MenuOp::CargoMounts).effect!=FrontEffect::CargoMounts)throw std::runtime_error("Flight cargo menu lacks mount action");
            auto mounts=game.cargoMounts();if(mounts.empty())throw std::runtime_error("Cargo weapon has no mount choices");
            ui.cargoMounts(mounts,game.state());auto effect=choose(MenuOp::CargoSwap,32);
            if(effect.effect!=FrontEffect::CargoSwap || !game.swapCargo(PartKind(effect.value/32),effect.value%32))throw std::runtime_error("Flight cargo swap failed");
            auto at=[&](PartKind kind,int mount)->const EquipmentState*{for(auto& e:game.state().equipment)if(e.kind==kind && e.mount==mount)return &e;return nullptr;};
            if(!at(PartKind::Weapon,0) || at(PartKind::Weapon,0)->name!=replacement->name || !at(PartKind::Weapon,-1) || at(PartKind::Weapon,-1)->name!=mounted->name || at(PartKind::Weapon,0)->mounted>=1)
                throw std::runtime_error("Cargo swap did not preserve old part or original arming delay");
            FlightInput fire;fire.fire=true;for(int i=0;i<600;++i)game.step(fire,1./165);game.step({},1./165);
            int before=game.state().ammo;
            if(!game.reload() || game.state().ammo<=before || at(PartKind::Weapon,0)->mounted>=1)throw std::runtime_error("Manual reload did not consume reserves and reset gun mounting");
            game.selectCargo(-2);game.nextCargo();
            if(game.state().selectedCargo==-5)throw std::runtime_error("Cargo navigation repeated a stack");
            game.selectCargo(-2);
            if(!game.dropCargo() || at(PartKind::Pack,-5))throw std::runtime_error("Drop did not remove last duplicate reserve");
            game.step({},1./165);
            if(std::none_of(game.contacts().begin(),game.contacts().end(),[](auto& c){return c.type==ContactTreasure;}))throw std::runtime_error("Dropped cargo did not create original IGC treasure");
            ui.open(Screen::Cargo,&game.state());if(choose(MenuOp::Reload).effect!=FrontEffect::Reload)throw std::runtime_error("Cargo manual reload action missing");
            ui.setPlaying(false);puts("Flight cargo: swap/arming, manual reload, grouped selection and real dropped treasure passed");
        }
        {Game game(assets.root(),loader,0,0,options,effects);
            if(!game.partChoices(PartKind::Weapon,100).empty() || !game.partChoices(PartKind::Pack,-6).empty() ||
               game.replacePart(PartKind::Pack,-6,-1))throw std::runtime_error("Invalid loadout slot accepted");
            ui.setPlaying(true);ui.open(Screen::Hangar,&game.state());choose(MenuOp::Navigate,int(Screen::Cargo));
            auto slot=choose(MenuOp::EquipmentSlot,-5);
            if(slot.effect!=FrontEffect::PartOptions || slot.value!=-5)throw std::runtime_error("Cargo slot does not open its choices");
            auto choices=game.partChoices(PartKind::Pack,-5);
            auto ammo=std::find_if(choices.begin(),choices.end(),[](auto& c){return c.name=="Ammo";});
            if(ammo==choices.end())throw std::runtime_error("Missing authored ammo reserve");
            ui.parts(PartKind::Pack,-5,choices);
            // The list is paginated; keyboard escape must return to the cargo page.
            ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            if(ui.screen()!=Screen::Cargo)throw std::runtime_error("Cargo choice escape loses context");
            if(!game.replacePart(PartKind::Pack,-5,ammo->id))throw std::runtime_error("Cannot load cargo reserve");
            ui.open(Screen::Cargo,&game.state());
            auto reserve=std::find_if(game.state().equipment.begin(),game.state().equipment.end(),[](auto& e){return e.mount==-5;});
            if(reserve==game.state().equipment.end() || reserve->kind!=PartKind::Pack || reserve->amount<=0)throw std::runtime_error("Cargo reserve not mounted");
            if(!game.replacePart(PartKind::Pack,-4,ammo->id) || !game.replacePart(PartKind::Pack,-4,-1))throw std::runtime_error("Cannot clear cargo slot");
            // Use real IGC firing to empty the magazine and reload from cargo.
            auto missileChoices=game.partChoices(PartKind::Missile,0);
            if(missileChoices.empty())throw std::runtime_error("No compatible missile launcher");
            int missile=missileChoices.front().id;
            if(!game.replacePart(PartKind::Missile,0,missile) || !game.replacePart(PartKind::Pack,-1,missile))throw std::runtime_error("Cannot provision missile reserves");
            game.launch();FlightInput fire;fire.missile=true;fire.fire=true;
            bool ammoReload=false,missileReload=false,missileEmpty=false;
            int lastAmmo=game.state().ammo;
            for(int i=0;i<30000 && (!ammoReload || !missileReload || !missileEmpty);++i) {
                game.step(fire,1./60);
                if(game.state().dead)throw std::runtime_error("Reload test player died");
                ammoReload|=game.state().ammo>lastAmmo;lastAmmo=game.state().ammo;
                auto& parts=game.state().equipment;
                bool hasReserve=std::any_of(parts.begin(),parts.end(),[](auto& e){return e.mount==-1;});
                bool hasMagazine=std::any_of(parts.begin(),parts.end(),[](auto& e){return e.mount==0 && e.kind==PartKind::Missile;});
                missileReload|=!hasReserve && hasMagazine;missileEmpty|=missileReload && !hasMagazine;
            }
            if(!ammoReload || !missileReload || !missileEmpty)throw std::runtime_error("IGC depletion did not reload cargo and remove exhausted launcher");
            game.step({},1./165);if(!game.returnToBase())throw std::runtime_error("Cannot return after depletion");
            for(int i=0;i<30000 && !game.state().docked && !game.state().dead;++i)game.step({},1./165);
            if(!game.state().docked)throw std::runtime_error("Cannot dock after depletion");
            auto restored=std::find_if(game.state().equipment.begin(),game.state().equipment.end(),[](auto& e){return e.kind==PartKind::Missile && e.mount==0 && !e.empty && e.amount>0;});
            if(restored==game.state().equipment.end())throw std::runtime_error("Docking did not automatically restore the depleted launcher");
            auto stripped=game.currentLoadout();std::erase_if(stripped.parts,[&](auto& part){return part.mount==0 && game.core()->GetPartType(part.id)->GetEquipmentType()==ET_Magazine;});
            auto purchase=game.purchaseLoadout(stripped);if(!purchase.accepted || !purchase.complete)throw std::runtime_error("Cannot explicitly buy an empty launcher slot");
            auto empty=std::find_if(game.state().equipment.begin(),game.state().equipment.end(),[](auto& e){return e.kind==PartKind::Missile && e.mount==0 && e.empty;});
            if(empty==game.state().equipment.end())throw std::runtime_error("Depleted launcher slot missing in hangar");
            ui.open(Screen::Hangar,&game.state());auto choice=choose(MenuOp::EquipmentSlot,int(empty-game.state().equipment.begin()));
            if(choice.effect!=FrontEffect::PartOptions || !game.replacePart(PartKind::Missile,0,missile))throw std::runtime_error("Cannot re-equip empty launcher slot");
            puts("Cargo menu, actual ammo/missile depletion, reserve reload, empty-part lifetime and dock/refit passed");
            ui.setPlaying(false);
        }
        {Game game(assets.root(),loader,0,0,options);
            auto choices=game.partChoices(PartKind::Pack,-5);
            auto fuel=std::find_if(choices.begin(),choices.end(),[](auto& c){return c.name=="Fuel";});
            if(fuel==choices.end() || !game.replacePart(PartKind::Pack,-5,fuel->id))throw std::runtime_error("Cannot load fuel reserve");
            game.launch();FlightInput boost;boost.boost=true;boost.throttle=1;boost.yaw=.3f;
            bool reloaded=false;float last=game.state().fuel;
            for(int i=0;i<12000 && !reloaded && !game.state().dead;++i){game.step(boost,1./165);reloaded=game.state().fuel>last+.1f;last=game.state().fuel;}
            if(!reloaded || game.state().dead)throw std::runtime_error("Afterburner fuel exhaustion did not reload its reserve");
            puts("Real afterburner depletion invoked the fuel reserve reload callback");
        }
        {Game game(assets.root(),loader,0,0,options,effects);
            if(!game.state().docked || game.state().station.empty())throw std::runtime_error("Session did not start in its hangar");
            auto before=game.state().equipment.front();auto choices=game.partChoices(before.kind,before.mount);
            auto choice=std::find_if(choices.begin(),choices.end(),[&](auto& c){return c.name!=before.name;});
            if(choice==choices.end() || !game.replacePart(before.kind,before.mount,choice->id) || game.state().equipment.front().name!=choice->name)
                throw std::runtime_error("Hangar replacement did not change actual equipment");
            if(game.replacePart(PartKind::Shield,0,choice->id))throw std::runtime_error("Hangar accepted an incompatible part");
            game.launch();if(game.state().docked)throw std::runtime_error("Hangar launch failed");
            if(game.replacePart(before.kind,before.mount,choice->id))throw std::runtime_error("Refit allowed in flight");
            FlightInput input;input.throttle=1;input.boost=true;input.fire=true;
            for(int i=0;i<330;++i)game.step(input,1./165);
            auto particles=game.particles();if(particles.empty())throw std::runtime_error("Afterburner/projectile effects were not emitted");
            for(auto& p:particles)if(!std::isfinite(p.centerSize.x) || (!(p.texture&(particleRibbon|particleLine|particleOriented)) && p.uv.x>=p.uv.z))throw std::runtime_error("Invalid particle geometry");
        }
        {Game game(assets.root(),loader,0,0,options,effects);
            auto base=game.state().station;
            for(int hull=0;hull<int(hullIds.size());++hull) {
                auto position=game.state().position;game.selectHull(hull);
                if(!game.state().docked || game.state().station!=base || glm::distance(position,game.state().position)>.01f)
                    throw std::runtime_error("Changing hull moved the player out of its docked station");
                game.launch();auto ship=game.state().ship;auto launch=game.state().position;
                game.selectHull((hull+1)%hullIds.size());
                if(game.state().ship!=ship)throw std::runtime_error("Hull replacement allowed in flight");
                FlightInput boost;boost.throttle=1;boost.boost=true;
                for(int i=0;i<165;++i)game.step(boost,1./165);
                if(game.state().dead || glm::distance(launch,game.state().position)<10)throw std::runtime_error("Station launch did not clear its collision hull");
                game.step({},1./165);if(!game.returnToBase())throw std::runtime_error("Return to base failed to find station");
                for(int i=0;i<30000 && !game.state().docked && !game.state().dead;++i)game.step({},1./165);
                if(!game.state().docked || game.state().station!=base || game.state().dead || game.state().autopilot)
                    throw std::runtime_error("Launch-return-dock cycle failed for "+ship);
            }
            puts("All six hulls launched, returned, docked and refitted at the same station");
        }
        options.startInHangar=false;
        {Game game(assets.root(),loader,0,0,options,effects);FlightInput input;input.throttle=1;input.boost=true;input.yaw=.4f;input.pitch=-.2f;input.roll=.3f;
            for(int i=0;i<165;++i)game.step(input,1./165);
            const auto& state=game.state();bool found=false;
            for(auto& draw:game.scene(true))if(glm::distance(glm::vec3(draw.transform[3]),state.position)<.01f) {
                // Artwork noses are +Z; the IGC ship/camera forward is -Z.
                if(glm::dot(glm::normalize(glm::vec3(draw.transform[2])),state.forward)<.999f ||
                    glm::dot(glm::normalize(glm::vec3(draw.transform[1])),state.up)<.999f)
                    throw std::runtime_error("Rendered player model disagrees with flight orientation");found=true;
            }
            if(!found)throw std::runtime_error("Player model missing from chase view");
            for(auto& p:game.particles())for(int axis=0;axis<3;++axis)if(!std::isfinite(p.centerSize[axis]) || !std::isfinite(p.axisWidth[axis]))
                throw std::runtime_error("Turning/rolling produced an invalid particle");
        }
        {Particles particles(effects);
            particles.motionDebris(0,{}, {0,0,-1},{1,0,0},{0,1,0},.01f);
            particles.motionDebris(0,{0,0,-10},{0,0,-1},{1,0,0},{0,1,0},.01f);particles.begin(0);
            std::vector<ParticleDraw> before(particles.draws().begin(),particles.draws().end());
            if(before.empty())throw std::runtime_error("Motion debris missing");
            particles.motionDebris(0,{0,0,-10},{1,0,0},{0,0,1},{0,-1,0},.01f);particles.begin(0);
            if(particles.draws().size()!=before.size())throw std::runtime_error("Camera rotation spawned motion debris");
            for(size_t i=0;i<before.size();++i)if(glm::distance(glm::vec3(before[i].centerSize),glm::vec3(particles.draws()[i].centerSize))>1e-6f)
                throw std::runtime_error("Motion debris rotated with the player");
            particles.motionDebris(0,{0,0,-10},{1,0,0},{0,0,1},{0,-1,0},0,.25f);particles.begin(0);
            if(particles.draws().size()!=before.size())throw std::runtime_error("FOV change spawned motion debris");
            for(auto& p:particles.draws())if(std::abs(p.centerSize.w-.025f)>1e-6f)
                throw std::runtime_error("Motion debris did not retain angular size during camera zoom");
            particles.motionDebris(1,{1000,0,0},{1,0,0},{0,0,1},{0,-1,0},.01f);particles.begin(1);
            if(!particles.draws().empty())throw std::runtime_error("Jump left motion debris behind in the new sector");
            particles.quality(3);particles.motionDebris(1,{1010,0,0},{1,0,0},{0,0,1},{0,-1,0},.01f);particles.begin(1);
            if(particles.emissionLimit()!=0 || particles.draws().empty())throw std::runtime_error("Turning off smoke also disables independent motion debris");
            particles.debris(0);particles.begin(1);
            if(particles.live() || !particles.draws().empty())throw std::runtime_error("Turning off debris leaves old motion debris visible");
            particles.quality(4);
            if(!particles.performance() || particles.emissionLimit()!=1)throw std::runtime_error("Performance smoke loses its rocket-mode policy");
        }
        {Particles particles(effects);for(int i=0;i<10000;++i)particles.emit(EffectKind::Smoke,0,{}, {},1,1);
            if(particles.live()>4096)throw std::runtime_error("Particle pool is not bounded");
            particles.begin(1);if(!particles.draws().empty())throw std::runtime_error("Particles leaked across sectors");
            particles.step(1.1f);if(particles.live())throw std::runtime_error("Expired particles were retained");
        }
        auto atRate=[&](int hz){Game game(assets.root(),loader,0,0,options,effects);FlightInput input;input.throttle=1;input.boost=true;
            for(int i=0;i<hz*2;++i)game.step(input,1./hz);return game.particles().size();};
        auto lowRate=atRate(30),highRate=atRate(165);
        if(std::min(lowRate,highRate)<20 || double(std::max(lowRate,highRate))/std::min(lowRate,highRate)>1.15)
            throw std::runtime_error("Exhaust density depends on render frequency");
        printf("Exhaust instances after two seconds: %zu at 30 Hz, %zu at 165 Hz\n",lowRate,highRate);
        options.scenario=Scenario::FlightTraining;options.enemies=options.allies=0;
        {Game game(assets.root(),loader,0,0,options);FlightInput input;input.throttle=1;
            for(int i=0;i<500;++i)game.step(input,.01);
            if(game.state().objective.find("Flight 2/5")==std::string::npos)throw std::runtime_error("Flight objective failed to advance");
            input.yaw=.5f;for(int i=0;i<500 && game.state().objective.find("Flight 2/5")!=std::string::npos;++i)game.step(input,.01);
            input.yaw=0;input.right=true;for(int i=0;i<110;++i)game.step(input,.01);
            input.right=false;game.toggleVectorLock();game.step(input,.01);
            bool station=false;for(auto& c:game.contacts())if(c.type==ContactStation && !c.enemy){game.selectTarget(c.type,c.id);station=true;break;}
            if(!station)throw std::runtime_error("Flight training has no friendly station");game.toggleAutopilot();
            for(int i=0;i<6600 && !game.state().complete;++i)game.step({},.02);
            if(!game.state().complete || game.state().result.empty())throw std::runtime_error("Flight training failed to complete on docking");
        }
        options.scenario=Scenario::Skirmish;options.killLimit=1;options.enemies=1;options.allies=2;
        {Game game(assets.root(),loader,0,0,options);
            for(int i=0;i<30000 && !game.state().complete;++i)game.step({},1./165);
            if(!game.state().complete || game.state().friendlyKills+game.state().enemyKills<1 || game.state().result.empty())throw std::runtime_error("Skirmish never reached its win condition");
        }
        puts("Session lifecycle, lobby teams, objectives, bindings and configuration passed");return 0;
    }catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}
}
