#pragma once
#include "../frontend.hpp"
#include "../test_fixture.hpp"
#include <bit>
#include <functional>

namespace NativeTest {
inline void checkRemoteTeleport(Native::Assets& assets){
    using namespace Native;using Status=Allegiance::PlayerShipStatus;
    SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.enemies=0;options.rules.miners=0;options.rules.teams=3;
    auto loader=[](auto&,auto&){return ModelHandle{1,1};};
    Game authority(assets.root(),loader,0,0,options,{},GameRole::Authority),replica(assets.root(),loader,0,0,options,{},GameRole::Replica);
    auto policy=authority.missionPolicy(1);policy.alliances[0]=policy.alliances[1]=0;
    require(authority.applyMissionPolicy(policy) && replica.applyMissionPolicy(policy),"remote boarding allied faction fixture");
    auto* core=authority.core();auto* scout=authority.playerShip()->GetBaseHullType();IhullTypeIGC* gunship=nullptr;
    for(auto* link=core->GetHullTypes()->first();link;link=link->next())if(link->data()->GetMaxWeapons()-link->data()->GetMaxFixedWeapons()>=2){gunship=link->data();break;}
    require(gunship,"remote boarding multi-turret hull missing");
    constexpr int target=30000,crew=30001;int self=authority.state().shipId;uint32_t tick=60;int cases=0;
    auto* sector=core->GetClusters()->last()->data();
    auto frame=[&](int side,int state,bool stale,int hull,bool membership,int manned=1){
        auto report=authority.networkFrame(self,++tick,0);
        Net::Member own;own.id=self;own.side=0;Net::text(own.name,authority.playerShip()->GetName());report.members.push_back(own);
        if(membership){Net::Member member;member.id=target;member.side=side;Net::text(member.name,"Current public captain");report.members.push_back(member);}
        Net::ShipIntel intel;intel.id=target;intel.side=side;intel.serial=100001;intel.pilotType=c_ptPlayer;Net::text(intel.name,"Old reported captain");
        intel.status.state=state;intel.status.flags=stale?Status::Unknown:0;intel.status.hull=hull;intel.status.sector=sector->GetObjectID();report.shipIntel.push_back(intel);
        for(int i=0;i<manned+1;++i){
            Net::Member member;member.id=crew+i;member.side=side;Net::text(member.name,i<manned?"Reported gunner":"Reported observer");report.members.push_back(member);
            Net::ShipIntel child;child.id=member.id;child.side=side;child.serial=100002+i;child.pilotType=c_ptPlayer;Net::text(child.name,member.name);
            child.status.state=i<manned?Status::Turret:Status::Observer;child.status.flags=0;child.status.parent=target;child.status.sector=sector->GetObjectID();report.shipIntel.push_back(child);
        }
        return report;
    };
    for(int side=0;side<3;++side)for(int state=Status::Dead;state<=Status::Turret;++state)
    for(bool stale:{false,true})for(int hull:{NA,int(scout->GetObjectID()),int(gunship->GetObjectID())})for(bool membership:{false,true}){
        auto report=frame(side,state,stale,hull,membership);replica.applyNetworkFrame(Net::frame(Net::packet(report)));
        require(!replica.core()->GetShip(target) && !replica.core()->GetShip(crew),"remote boarding fixture created physical target/crew replicas");
        auto rows=replica.teleportDestinations();auto found=std::find_if(rows.begin(),rows.end(),[](const auto& row){return row.boarding && row.id==target;});
        bool expected=side==0 && state==Status::Docked && !stale && hull==gunship->GetObjectID() && membership;
        require((found!=rows.end())==expected,"original teleport pane loses a reported remote-sector boarding target or lists an ineligible one");
        if(expected)require(found->name=="Current public captain" && found->ship==gunship->GetName() && found->sector==sector->GetName() && found->manned==1 && found->turrets==gunship->GetMaxWeapons()-gunship->GetMaxFixedWeapons() && !found->current && !found->keepsShip,"reported boarding target loses original name/hull/sector/occupancy/painter state");++cases;
    }
    for(int accountSide:{NA,0,1}){
        auto retained=frame(0,Status::Docked,false,gunship->GetObjectID(),false);
        for(auto& intel:retained.shipIntel)if(intel.id==target)Net::text(intel.name,"Retained captain");
        if(accountSide>=0){Net::Pilot account;account.id=target;account.side=accountSide;account.identity=7;Net::text(account.name,"Retained captain");retained.pilots.push_back(account);}
        // Current human crew status includes active seats without live peers.
        std::erase_if(retained.members,[](const auto& member){return member.id==crew;});
        Net::Pilot gunner;gunner.id=crew;gunner.side=0;gunner.identity=8;retained.pilots.push_back(gunner);
        replica.applyNetworkFrame(Net::frame(Net::packet(retained)));auto destinations=replica.teleportDestinations();
        auto row=std::find_if(destinations.begin(),destinations.end(),[](const auto& row){return row.boarding && row.id==target;});
        require((row!=destinations.end())==(accountSide==0),"retained boarding target loses its active own account or trusts foreign/retired identity");
        if(accountSide==0)require(row->name=="Retained captain" && row->manned==1 && replica.pilotName(target)==row->name,"retained target caption/occupancy requires live target and crew peers");
    }
    auto report=frame(0,Status::Docked,false,gunship->GetObjectID(),true);replica.applyNetworkFrame(Net::frame(Net::packet(report)));
    require(replica.pilotName(target)=="Current public captain" && replica.pilotName(-1).empty() && replica.pilotName(32768).empty(),"boarding caption requires a physical ship or ignores public identity");
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.setPlaying(true);ui.teleportService([&]{return replica.teleportDestinations();});ui.teleports(replica.teleportDestinations(),replica.state());
    auto row=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& item){return item.action.op==MenuOp::TeleportSelect && item.action.value==target && item.action.delta==1;});
    require(row!=ui.view().items.end() && row->enabled,"remote boarding target cannot be selected in original teleport pane");ui.activate(size_t(row-ui.view().items.begin()));
    auto transfer=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& item){return item.action.op==MenuOp::TeleportActivate;});require(transfer!=ui.view().items.end(),"original teleport action missing");
    auto selected=ui.activate(size_t(transfer-ui.view().items.begin()));require(selected.effect==FrontEffect::Board && selected.value==target,"original remote boarding action lost its typed ship ID");
    Net::Action action;int submissions=0;replica.networkActions([&](const Net::Action& value){action=value;++submissions;return true;});
    submissions=0; // Installing the sink publishes the current chat preferences.
    require(replica.boardShip(selected.value,false,true) && submissions==1 && action.op==Net::Operation::Board && action.args[0]==target && action.args[1]==1,"remote boarding requires a physical target replica or loses disembark-first intent");
    ui.transferring(Allegiance::CrewTransfer::Board,replica.pilotName(target),19);
    require(std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& control){return control.name=="messageText" && control.text=="Boarding Current public captain's ship....";}),"status-only boarding lockdown loses original caption");ui.transferFinished(19);
    require(replica.boardingFailure(target)=="Teleportation canceled - no turret positions were open.","remote boarding rejection loses original occupancy-dependent feedback");
    auto full=frame(0,Status::Docked,false,gunship->GetObjectID(),true,gunship->GetMaxWeapons()-gunship->GetMaxFixedWeapons());replica.applyNetworkFrame(Net::frame(Net::packet(full)));
    ui.sync(replica.state());require(replica.boardingFailure(target)=="Teleportation failed.","full reported turrets use incorrect rejection feedback");
    auto retired=frame(0,Status::Flying,false,gunship->GetObjectID(),true);replica.applyNetworkFrame(Net::frame(Net::packet(retired)));ui.sync(replica.state());
    require(std::none_of(ui.view().items.begin(),ui.view().items.end(),[](const auto& item){return item.action.op==MenuOp::TeleportSelect && item.action.value==target;}),"live launch does not retire the selected remote teleport target");
    transfer=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& item){return item.action.op==MenuOp::TeleportActivate;});require(transfer!=ui.view().items.end() && ui.activate(size_t(transfer-ui.view().items.begin())).effect==FrontEffect::None,"retired remote boarding selection still dispatches an action");
    // A full human roster: one carrier, two gunners and five observers per
    // group. No physical target replicas or artificial extra wire records.
    auto roster=authority.networkFrame(self,++tick,0);Net::Member own;own.id=self;own.side=0;Net::text(own.name,authority.playerShip()->GetName());roster.members.push_back(own);
    for(int group=0;group<128;++group)for(int seat=0;seat<8;++seat){
        int id=target+group*8+seat;Net::Member member;member.id=id;member.side=0;Net::text(member.name,seat?"Crew":"Carrier "+std::to_string(group));roster.members.push_back(member);
        Net::ShipIntel intel;intel.id=id;intel.side=0;intel.serial=200000+id;intel.pilotType=c_ptPlayer;Net::text(intel.name,member.name);intel.status.flags=0;intel.status.sector=sector->GetObjectID();
        intel.status.state=seat==0?Status::Docked:seat<=2?Status::Turret:Status::Observer;intel.status.hull=seat==0?gunship->GetObjectID():NA;intel.status.parent=seat?target+group*8:NA;roster.shipIntel.push_back(intel);
    }
    replica.applyNetworkFrame(Net::frame(Net::packet(roster)));auto destinations=replica.teleportDestinations();
    require(std::count_if(destinations.begin(),destinations.end(),[](const auto& row){return row.boarding;})==128,"full reported crew roster loses remote carriers");
    for(const auto& row:destinations)if(row.boarding)require(row.manned==2,"full reported crew roster counts observers as gunners");
    ui.teleports(destinations,replica.state());
    int pageRows=std::max(1,int(authoredControlBounds(ui.view().artwork,"teleportPaneDestinationListPane").w/14));
    for(size_t i=0;i<destinations.size()+pageRows;i+=pageRows)ui.key(SDLK_PAGEDOWN,SDL_SCANCODE_PAGEDOWN);
    int stations=int(destinations.size())-128;for(int i=0;i<stations;++i)ui.key(SDLK_UP,SDL_SCANCODE_UP);
    require(std::any_of(ui.view().items.begin(),ui.view().items.end(),[](const auto& item){return item.action.op==MenuOp::TeleportSelect && item.action.value==target+127*8 && item.action.delta==1 && item.selected;}),"full remote boarding roster cannot reach its last carrier");
    auto departed=frame(0,Status::Docked,false,gunship->GetObjectID(),false);replica.applyNetworkFrame(Net::frame(Net::packet(departed)));ui.sync(replica.state());require(replica.pilotName(target).empty(),"departed boarding caption retains old report identity");
    replica.networkActions({});
    printf("Original remote teleport: %d reported state/faction/hull/membership combinations, no physical target/crew replicas, live occupancy/selection retirement and canonical boarding intent passed\n",cases);
    puts("Original remote teleport: 1024 reported pilots, 128 carriers, observer-free gunner counts and full keyboard reachability passed");
    puts("Original remote teleport: active own-account targets and occupied seats without live peers, foreign-account exclusion and retired identity passed");
}
inline void checkTeleportHeaders(Native::Assets& assets,const std::function<void(const Native::MenuView&,const Native::MenuView&,std::string_view)>& paint={}){
    using namespace Native;
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.setPlaying(true);GameState state;state.docked=state.economy=true;
    std::vector<TeleportChoice> choices{{7,"Captain","Remote sector",false,false,true,"Gunship",1,2},{9,"Home","Home sector",true,true}};ui.teleports(choices,state);
    auto item=[&](MenuOp op,int value){auto i=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& item){return item.action.op==op && item.action.value==value;});require(i!=ui.view().items.end(),"teleport header fixture missing control");return size_t(i-ui.view().items.begin());};
    ui.activate(item(MenuOp::TeleportSelect,7));auto* cached=ui.view().artwork.data();auto started=ui.view().slide->started;
    OriginalUi source(assets);auto authored=source.layout("teleportpane","teleportPaneDestinationListHeader");
    for(int column=0;column<4;++column)for(bool context:{false,true}){
        auto index=item(MenuOp::TeleportHeader,column);ui.clearSounds();if(context)ui.pointerContext(index);else ui.pointerPress(index);
        require(ui.pointerCaptured() && bool(ui.view().capturedButton()) && ui.contextButtonCaptured()==context,"teleport header does not retain original left/right capture/pressed face");
        if(paint && column==0){
            MenuView actual;actual.original=actual.overlay=true;actual.canvas=ui.view().canvas;actual.classicScale=ui.view().classicScale;
            MenuView expected=actual;expected.artwork=authored;
            // Original unchecked ButtonFacePane down/up slices. Reference
            // images bypass the native ButtonPane state-selection code.
            for(size_t i=0;i<expected.artwork.size();++i){
                auto& c=expected.artwork[i];float frames=float(std::popcount(c.faces)),frame=i==size_t(column)?1.f:0.f;
                c.kind=OriginalControl::Kind::Image;c.faces=0;c.uv.y=frame/frames;c.uv.w=(frame+1)/frames;
            }
            glm::vec2 origin{};bool first=true;
            for(const auto& control:ui.view().artwork)if(control.name.starts_with("teleportPaneDestinationListHeader")){
                if(first){origin=glm::vec2(control.bounds);first=false;}auto c=control;c.translate(-origin);actual.artwork.push_back(std::move(c));
            }
            for(size_t i=0;i<actual.artwork.size();++i){
                const auto& c=actual.artwork[i];MenuItem header;header.bounds=c.bounds;header.control=c.name;header.enabled=header.captureOnPress=true;actual.items.push_back(header);
            }
            actual.pressed=column;std::string name=context?"teleport-header-right":"teleport-header-left";paint(actual,expected,name);
            // Installed normal/down glyphs are identical. A test-only copy
            // with green down-frame glyphs proves actual frame selection;
            // neither the asset cache nor any installed file is changed.
            auto distinct=std::make_shared<ImageData>(*actual.artwork.front().image);distinct->name+="-teleport-down-diagnostic";
            auto height=distinct->height/std::popcount(authored.front().faces);
            for(uint32_t y=height;y<2*height;++y)for(uint32_t x=0;x<distinct->width;++x){
                auto offset=(size_t(y)*distinct->width+x)*4;auto* pixel=distinct->pixels.data()+offset;
                if(pixel[3] && (pixel[0] || pixel[1] || pixel[2])){pixel[0]=pixel[2]=0;pixel[1]=255;}
            }
            for(auto* view:{&actual,&expected})for(auto& control:view->artwork)control.image=distinct;
            paint(actual,expected,name+"-distinct");
        }
        require(ui.pointerRelease(index,false,context?SDL_BUTTON_RIGHT:SDL_BUTTON_LEFT).effect==FrontEffect::None && !ui.pointerCaptured(),"teleport header dispatches a screen change or retains capture");
        require(ui.view().artwork.data()==cached && ui.view().slide->started==started && ui.screen()==Screen::Teleport,"teleport header rebuilds/reopens its pane");
        require(ui.view().items[item(MenuOp::TeleportSelect,7)].selected,"teleport header changes its destination selection");
        for(const auto& control:ui.view().artwork)if(control.name.starts_with("teleportPaneDestinationListHeader"))require(control.checkedState==0,"teleport header invents tab/checkbox state");
        require(std::count(ui.sounds().begin(),ui.sounds().end(),mouseclickSound)==(context?0:1),"teleport header forwards right event to the left sound sink");
        if(context)ui.pointerContext(index);else ui.pointerPress(index);ui.clearSounds();ui.pointerRelease({});require(ui.sounds().empty() && !ui.pointerCaptured() && ui.view().artwork.data()==cached,"canceled teleport header capture fires a callback or changes its root");
    }
    puts("Original teleport headers: four inert columns, left/right pressed faces/capture, distinct sound sinks, canceled release and unchanged selection/root passed");
}
}
