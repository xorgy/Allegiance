#pragma once
#include "../frontend.hpp"
#include "../test_fixture.hpp"
#include <numeric>
#include <chrono>

namespace NativeTest {
// Frozen TeamScreen comparisons plus TList::InsertSorted's backwards walk.
// Deliberately separate from the modern key sort and indexed leader insertion.
struct ReferenceLobbyRow {
    int id{};std::string name;int rank{},wingID{};
    bool ready{},requesting{},host{},commander{};
};
inline std::vector<ReferenceLobbyRow> referenceLobbySort(std::span<const ReferenceLobbyRow> rows,Allegiance::LobbyRosterSort mode,bool reverse) {
    auto compareName=[](const std::string& a,const std::string& b){
        auto fold=[](unsigned char c){return c>='A' && c<='Z'?c+32:c;};
        for(size_t i=0;i<std::min(a.size(),b.size());++i)if(fold(a[i])!=fold(b[i]))return fold(a[i])>fold(b[i]);
        return a.size()>b.size();
    };
    auto predicate=[&](const auto& a,const auto& b){
        using enum Allegiance::LobbyRosterSort;
        if(mode==Leader)return reverse?a.commander && !b.host:b.commander && !a.host;
        if(mode==Name)return reverse?compareName(b.name,a.name):compareName(a.name,b.name);
        if(mode==Rank)return reverse?b.rank>a.rank:a.rank>b.rank;
        bool result;
        if(a.requesting && !b.requesting)result=false;
        else if(!a.requesting && b.requesting)result=true;
        else if(mode==Status)result=a.ready && !b.ready;
        else result=a.wingID>b.wingID;
        return reverse?!result:result;
    };
    std::vector<ReferenceLobbyRow> output;
    for(auto& row:rows){size_t index=output.size();while(index && predicate(output[index-1],row))--index;output.insert(output.begin()+index,row);}
    return output;
}
inline void checkLobbyRoster(Native::Assets& assets) {
    using namespace Native;using Allegiance::LobbyRosterSort;
    require(!Allegiance::LobbyRosterFlash(0) && !Allegiance::LobbyRosterFlash(.249) && Allegiance::LobbyRosterFlash(.25) &&
        Allegiance::LobbyRosterFlash(.499) && !Allegiance::LobbyRosterFlash(.5) && Allegiance::LobbyRosterFlash(.75),"Original lobby flash does not use the quarter-second timer");
    uint32_t random=1;auto next=[&]{random=random*1664525+1013904223;return random;};
    std::vector<ReferenceLobbyRow> scratch;std::vector<size_t> links;size_t comparisons=0;
    for(int sample=0;sample<4096;++sample){
        std::vector<ReferenceLobbyRow> input;
        for(int i=0,count=sample%13;i<count;++i){auto bits=next();ReferenceLobbyRow row;
            row.id=i;row.name=std::array<const char*,6>{"Alpha","alpha","Bravo","bRAVO","Zulu",""}[(bits>>8)%6];row.rank=(bits>>16)%4;row.wingID=int((bits>>20)%5)-1;
            row.ready=bits&1;row.requesting=bits&2;row.host=bits&4;row.commander=bits&8;input.push_back(std::move(row));
        }
        for(auto mode:{LobbyRosterSort::Leader,LobbyRosterSort::Name,LobbyRosterSort::Rank,LobbyRosterSort::Status,LobbyRosterSort::Wing})for(bool reverse:{false,true}){
            auto expected=referenceLobbySort(input,mode,reverse),actual=input;
            Allegiance::SortLobbyRoster(actual,mode,reverse,scratch,links);
            require(actual.size()==expected.size(),"Lobby sort loses pilots");
            for(size_t i=0;i<actual.size();++i)require(actual[i].id==expected[i].id,"Lobby roster sort differs from frozen WinTrek insertion semantics");
            ++comparisons;
        }
    }
    Settings preferences;Frontend ui(preferences);ui.originalInterface(assets);ui.setPlaying(true);
    SessionOptions options;options.scenario=Scenario::Conquest;ui.networkSession(options);
    GameState state;state.networked=state.networkLobby=state.networkHost=state.commander=true;
    // An applicant appears before members in the raw report and has role flags.
    // It must still follow the original member/request concatenation and omit
    // the role marks while shown as an applicant.
    state.networkPilots={{80,noTeam,"alpha",false,true,true,false,false,0,3},
        {40,0,"Zulu",true,true,true,true,false,noTeamRequest,2},
        {10,0,"aLPHa",false,false,false,false,false,noTeamRequest,-1},
        {20,0,"Bravo",true,false,false,false,false,noTeamRequest,1}};
    ui.open(Screen::RemoteLobby,&state);
    auto item=[&](std::string_view name){auto& v=ui.view().items;auto i=std::find_if(v.begin(),v.end(),[&](auto& p){return p.control==name;});require(i!=v.end(),"Lobby control missing");return size_t(i-v.begin());};
    auto pilot=[&](int id){auto& v=ui.view().items;auto i=std::find_if(v.begin(),v.end(),[&](auto& p){return p.action.op==MenuOp::RosterSelect && p.action.delta==id;});require(i!=v.end(),"Lobby pilot disappeared");return size_t(i-v.begin());};
    auto ordered=[&]{std::vector<int> ids;for(auto& p:ui.view().items)if(p.action.op==MenuOp::RosterSelect)ids.push_back(p.action.delta);return ids;};
    require(ui.view().items[item("chatHeader1")].selected,"First faction lobby entry must default to Team chat");
    auto applicant=ui.view().items[pilot(80)].bounds;auto check=assets.image("lobbycheckbmp",true);
    require(std::none_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](auto& c){return c.image==check && c.bounds.y>=applicant.y && c.bounds.y<applicant.y+14;}),"Applicant displays host/leader marks");
    require(std::count_if(ui.view().artwork.begin(),ui.view().artwork.end(),[](auto& c){return c.text==" (0) ";})==4,"Unranked pilots lack the original numeric rank text");
    auto wingBlank=ui.view().items[pilot(10)].bounds;OriginalUi authored(assets);auto columns=authored.integers("teamscreen","playerColumns");
    float wingX=authoredControl(ui.view().artwork,"playerListPane").bounds.x+columns[4]+2;
    require(std::none_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](auto& c){return c.kind==OriginalControl::Kind::Text && c.bounds.y==wingBlank.y && c.bounds.x==wingX && !c.text.empty();}),"Unassigned negative wing ID is painted as a wing");
    ui.activate(pilot(20));ui.text("Retained private draft");
    std::vector<ReferenceLobbyRow> members{{40,"Zulu",0,2,true,false,true,true},{10,"aLPHa",0,-1,false,false,false,false},{20,"Bravo",0,1,true,false,false,false},{80,"alpha",0,3,false,true,true,true}};
    auto mode=LobbyRosterSort::Leader;bool reverse=false;
    for(int column:{0,0,3,3,4,4,5,5,1,2}){
        auto changed=Allegiance::LobbyRosterColumn(mode,column);reverse=changed==mode?!reverse:false;mode=changed;
        ui.activate(item("playerListHeader"+std::to_string(column)));
        auto expected=referenceLobbySort(members,mode,reverse);std::vector<int> ids;for(auto& row:expected)ids.push_back(row.id);
        require(ordered()==ids,"Authored lobby header does not sort the real frontend roster");
        require(authoredControl(ui.view().artwork,"playerListHeader"+std::to_string(column)).checkedState==(reverse?2:1),"Lobby sort header uses the wrong checked face");
        auto recipient=ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
        require(ui.view().items[pilot(20)].selected && recipient.targetId==20 && ui.chatText()=="Retained private draft" && ui.wantsText(),"Header sort loses selected identity, private recipient, draft or text focus");
    }
    ui.activate(item("chatHeader0"));ui.open(Screen::Results);ui.open(Screen::RemoteLobby,&state);
    require(ui.view().items[item("chatHeader0")].selected,"Original lobby chat tab does not survive postgame navigation");
    ui.activate(item("chatHeader2"));ui.open(Screen::Home);state.commander=false;state.networkPilots[1].commander=false;ui.open(Screen::RemoteLobby,&state);
    require(ui.view().items[item("chatHeader1")].selected,"Unavailable remembered Leaders tab does not fall back to the original faction default");
    ui.setPlaying(false);state.networkPilots[1].team=noTeam;state.networkPilots[1].wing=2;ui.open(Screen::Home);ui.open(Screen::RemoteLobby,&state);
    require(ui.view().items[item("chatHeader0")].selected,"First No Team lobby entry must default to All chat");
    require(std::none_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](auto& c){return c.kind==OriginalControl::Kind::Text && c.bounds.x==wingX && !c.text.empty();}),"No Team roster exposes a faction wing");
    state.teamScores.resize(2);state.teamScores[0].active=false;state.teamScores[1].active=true;
    ui.sync(state);require(std::none_of(ui.view().items.begin(),ui.view().items.end(),[](auto& p){return p.control=="team0";}),"Inactive lobby faction seat is still drawn and selectable");
    state.lobbyFlags=4;state.networkPilots.erase(std::remove_if(state.networkPilots.begin(),state.networkPilots.end(),[](auto& p){return p.team==0;}),state.networkPilots.end());ui.sync(state);
    ui.activate(item("team0"));require(std::any_of(ui.view().items.begin(),ui.view().items.end(),[](auto& p){return p.action.op==MenuOp::RosterSelect && p.action.delta==40;}),"Empty destroyed faction does not fall back to No Team");
    std::vector<ReferenceLobbyRow> large(2048);for(size_t i=0;i<large.size();++i){large[i].id=i;large[i].commander=i%127==0;large[i].host=i==1024;}
    auto start=std::chrono::steady_clock::now();for(int i=0;i<64;++i)Allegiance::SortLobbyRoster(large,LobbyRosterSort::Leader,i&1,scratch,links);
    auto micros=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/64;
    std::printf("Original lobby roster: %zu frozen sorting cases; all six headers/directions, stable private selection, unranked text, applicant roles, negative/No Team wings, chat entry/return and inactive seats passed; 2048-row leader sort %.2f us average\n",comparisons,micros);
}
}
