#pragma once
#include "lobby_roster.hpp"
#include "authored_hosts.hpp"
#include <unordered_set>

namespace NativeTest {
// Exercise authored viewport/list behavior at the port's intended pilot scale,
// independently of world rendering and ship replicas.
inline void checkFullLobby(Native::Assets& assets,std::function<void(const Native::MenuView&,const Native::MenuView&,std::string_view)> paint={}){
    using namespace Native;
    constexpr int factions=6,members=256,applicants=8;
    Settings preferences;Frontend ui(preferences);ui.originalInterface(assets);ui.setPlaying(true);OriginalUi authored(assets);
    auto columns=authored.integers("teamscreen","playerColumns");require(columns.size()==6,"full lobby column imports are missing");
    SessionOptions options;options.scenario=Scenario::Conquest;options.rules.teams=factions;ui.networkSession(options);
    GameState state;state.networked=state.networkLobby=state.networkHost=state.commander=state.docked=true;
    state.playerSide=0;state.shipId=100;state.networkHostPilot=100;state.teamScores.resize(factions);
    for(int side=0;side<factions;++side){
        state.teamScores[side].active=true;state.teamScores[side].name="Faction "+std::to_string(side);
        // Requests precede members on the wire, but the original concatenated
        // lobby list starts with members before applying its selected sort.
        for(int i=0;i<applicants;++i)state.networkPilots.push_back({10000+side*applicants+i,noTeam,"Applicant "+std::to_string(i),bool(i&1),true,true,false,false,side,i});
        for(int i=0;i<members;++i)state.networkPilots.push_back({100+side*members+i,side,(i%61==0?std::string(80,'W'):(i&1?"pilot ":"PILOT "))+std::to_string(i%31),i%7!=0,side==0 && i==0,i==0,side==0 && i==0,i%9==0,noTeamRequest,i%13-1});
    }
    ui.open(Screen::RemoteLobby,&state);
    auto item=[&](std::string_view control){const auto& rows=ui.view().items;auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.control==control;});require(found!=rows.end(),"full lobby authored control is missing");return size_t(found-rows.begin());};
    auto selected=[&]{for(const auto& row:ui.view().items)if(row.action.op==MenuOp::RosterSelect && row.selected)return row.action.delta;return -1;};
    auto menu=[&]{return std::count_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& row){return row.popupLevel>=0;});};
    size_t matrices=0,visibleRows=0,refreshes=0;
    auto mode=Allegiance::LobbyRosterSort::Leader;bool reverse=false;
    for(int side=0;side<=factions;++side){
        int viewed=side==factions?noTeam:side;ui.activate(item(side==factions?"noTeam":"team"+std::to_string(side)));
        std::vector<ReferenceLobbyRow> input;
        for(bool requests:{false,true})for(const auto& pilot:state.networkPilots)if(requests?(pilot.team!=viewed && pilot.requested==viewed):pilot.team==viewed)
            input.push_back({pilot.id,pilot.name,0,pilot.wing,pilot.ready,requests,pilot.host,pilot.commander});
        for(int column:{-1,0,0,3,3,4,4,5,5}){
            if(column>=0){auto next=Allegiance::LobbyRosterColumn(mode,column);reverse=next==mode?!reverse:false;mode=next;ui.activate(item("playerListHeader"+std::to_string(column)));}
            auto expected=referenceLobbySort(input,mode,reverse);std::unordered_set<int> seen;
            auto blank=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& row){return row.action.op==MenuOp::ListClear && row.action.value==int(MenuOp::RosterSelect);});
            require(blank!=ui.view().items.end(),"full lobby list viewport is not actionable");ui.activate(size_t(blank-ui.view().items.begin()));
            ui.key(SDLK_DOWN,SDL_SCANCODE_DOWN);int index=0;
            while(true){
                auto viewport=authoredListBounds(ui.view().artwork,"playerListPane",float(columns.back()));
                require(selected()==expected[size_t(index)].id,"full lobby navigation differs from original sorted list");
                for(const auto& row:ui.view().items)if(row.action.op==MenuOp::RosterSelect){
                    require(row.action.value>=0 && row.action.value<int(expected.size()) && expected[size_t(row.action.value)].id==row.action.delta,"full lobby virtual row maps to the wrong identity");
                    require(row.bounds.w>0 && row.bounds.x>=viewport.x && row.bounds.y>=viewport.y && row.bounds.x+row.bounds.z<=viewport.x+viewport.z && row.bounds.y+row.bounds.w<=viewport.y+viewport.w,"full lobby row hit region escapes authored clipping");
                    const auto& face=authoredControl(ui.view().artwork,row.control);
                    for(int x:{0,columns[2]+2,columns[3]+2}){
                        auto text=std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& control){return control.kind==OriginalControl::Kind::Text && control.bounds.x==viewport.x+x && control.bounds.y==face.bounds.y;});
                        require(text!=ui.view().artwork.end() && text->clip && text->clip->y==row.bounds.y && text->clip->w==row.bounds.w,"original lobby name/rank/status clip must follow its 14-pixel row, including partial scroll rows");
                    }
                    seen.insert(row.action.delta);++visibleRows;
                }
                if(index==int(expected.size())-1)break;
                index=std::min(int(expected.size())-1,index+std::max(1,int(viewport.w/14)));ui.key(SDLK_PAGEDOWN,SDL_SCANCODE_PAGEDOWN);
            }
            require(seen.size()==expected.size(),"full lobby scrollbar/keyboard path cannot reach every pilot and request");
            if(paint && column==0 && !reverse){
                MenuView actual,reference;actual.original=reference.original=true;actual.canvas=reference.canvas=ui.view().canvas;actual.classicScale=reference.classicScale=ui.view().classicScale;
                auto begin=std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& control){return control.kind==OriginalControl::Kind::Rectangle && control.name.starts_with("roster");});
                auto scroll=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& row){return row.action.op==MenuOp::ScrollLine && row.action.value==1 && row.action.delta==-1;});require(scroll!=ui.view().items.end(),"full lobby pixel fixture lacks original scrollbar");
                auto end=std::find_if(begin,ui.view().artwork.end(),[&](const auto& control){return control.name==scroll->control;});require(begin!=ui.view().artwork.end() && end!=ui.view().artwork.end(),"full lobby pixel fixture cannot isolate row artwork");actual.artwork.assign(begin,end);
                auto viewport=authoredListBounds(ui.view().artwork,"playerListPane",float(columns.back()));float offset=std::max(0.f,expected.size()*14.f-viewport.w);
                for(int i=int(offset/14);i<int(expected.size());++i){
                    auto& row=expected[size_t(i)];float y=viewport.y+i*14-offset;if(y>=viewport.y+viewport.w)break;
                    OriginalControl background;background.kind=OriginalControl::Kind::Rectangle;background.bounds={viewport.x,y,viewport.z,14};background.clip=viewport;background.color=i==index?glm::vec4(100.f/255,0,0,1):glm::vec4(0);reference.artwork.push_back(background);
                    auto drawText=[&](int x,std::string value,int width=0){OriginalControl control;control.kind=OriginalControl::Kind::Text;control.font="smallFont";control.text=std::move(value);control.bounds={viewport.x+x,y,0,14};control.singleLine=true;control.clip=viewport;if(width)control.intersectClip({viewport.x+x,y,width,14});reference.artwork.push_back(std::move(control));};
                    drawText(0,row.name,columns[0]);drawText(columns[2]+2," (0) ",columns[3]-columns[2]-2);drawText(columns[3]+2,row.requesting?"Requesting":row.ready?"Ready":"AFK",columns[4]-columns[3]-2);
                    auto pilot=std::find_if(state.networkPilots.begin(),state.networkPilots.end(),[&](const auto& value){return value.id==row.id;});require(pilot!=state.networkPilots.end(),"full lobby pixel fixture loses membership");drawText(columns[4]+2,pilot->team==state.playerSide && pilot->wing>=0?std::string(Game::wingName(pilot->wing)):std::string{});
                    auto check=assets.image("lobbycheckbmp",true);auto mark=[&](int x){OriginalControl control;control.kind=OriginalControl::Kind::Image;control.image=check;control.bounds={viewport.x+x,y+(14-int(check->height))/2,check->width,check->height};control.clip=viewport;reference.artwork.push_back(std::move(control));};
                    if(!row.requesting){if(row.commander)mark(columns[1]+2);if(row.host)mark(columns[0]+2);}
                }
                paint(actual,reference,"full-lobby-"+std::to_string(viewed));
            }
            ui.text("Full-roster private draft");auto target=ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
            require(target.effect==FrontEffect::SendChat && target.targetId==(index>=0 && expected[size_t(index)].id==state.shipId?-1:expected[size_t(index)].id),"full lobby scroll selection sends chat to the wrong pilot");
            while(!ui.chatText().empty())ui.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);
            ++matrices;
        }
        // Retain an offscreen selection and private recipient through live
        // report reordering, then retire both when that exact pilot leaves.
        int target=selected();require(target>=0 && target!=state.shipId,"full lobby refresh fixture selects self");
        std::reverse(state.networkPilots.begin(),state.networkPilots.end());ui.sync(state);
        ui.text("Retained refresh draft");require(ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).targetId==target,"full lobby report permutation changes its private recipient");++refreshes;
        auto row=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& candidate){return candidate.action.op==MenuOp::RosterSelect && candidate.action.delta==target;});
        // A tied sort can move the selection offscreen; one Down/Up pair
        // reveals it without relying on the old virtual row's array index.
        if(row==ui.view().items.end()){ui.key(SDLK_UP,SDL_SCANCODE_UP);ui.key(SDLK_DOWN,SDL_SCANCODE_DOWN);row=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& candidate){return candidate.action.op==MenuOp::RosterSelect && candidate.action.delta==target;});}
        require(row!=ui.view().items.end(),"full lobby selected pilot cannot be revealed after refresh");
        ui.pointerContext(size_t(row-ui.view().items.begin()));require(menu()>0,"full lobby context menu does not open");
        auto removed=std::find_if(state.networkPilots.begin(),state.networkPilots.end(),[&](const auto& pilot){return pilot.id==target;});require(removed!=state.networkPilots.end(),"full lobby departing pilot fixture is absent");auto saved=*removed;state.networkPilots.erase(removed);ui.sync(state);
        require(menu()==0 && !ui.keyboardPopup() && selected()<0 && ui.wantsText() && ui.chatText()=="Retained refresh draft" && ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).targetId==-1,"full lobby departure leaves a stale context action, capture, private recipient or draft loss");++refreshes;
        state.networkPilots.push_back(std::move(saved));ui.sync(state);while(!ui.chatText().empty())ui.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);
    }
    printf("Original full lobby: %d pilots and %d applicants, %zu faction/No-Team sort matrices, %zu clipped virtual rows, complete keyboard access and %zu live private-selection/context retirements passed without a Game\n",factions*members,factions*applicants,matrices,visibleRows,refreshes);
}
}
