#pragma once
#include "../player_pane.hpp"
#include "../test_fixture.hpp"
#include <chrono>
#include <thread>

namespace NativeTest {
inline void checkRosterStates(Native::Assets& assets){
    using namespace Native;
    using Status=Allegiance::PlayerShipStatus;
    SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;
    options.enemies=0;options.rules.miners=0;
    auto loader=[](auto&,auto&){return ModelHandle{1,1};};
    Game authority(assets.root(),loader,0,0,options,{},GameRole::Authority);
    Game replica(assets.root(),loader,0,0,options,{},GameRole::Replica);
    auto* home=authority.playerShip()->GetStation();auto* hull=authority.playerShip()->GetBaseHullType();
    const int self=authority.state().shipId,parent=30000,passenger=30001;
    uint32_t tick=60;int cases=0;
    PlayerPane pane(assets);bool opened=false;
    constexpr glm::ivec2 size{1280,720};
    for(int faction=0;faction<3;++faction)
    for(int state=Status::Dead;state<=Status::Turret;++state)
    for(bool unknown:{false,true})for(int membership=0;membership<3;++membership){
        int side=faction==0?0:1;bool allied=faction!=2;
        auto policy=authority.missionPolicy(1);policy.alliances.fill(NA);
        if(allied)policy.alliances[0]=policy.alliances[1]=0;
        require(authority.applyMissionPolicy(policy) && replica.applyMissionPolicy(policy),"original ordered alliance policy fixture");
        auto frame=authority.networkFrame(self,++tick,0);
        auto member=[&](int id,const char* name,int team){Net::Member m;m.id=id;m.side=team;Net::text(m.name,name);frame.members.push_back(m);};
        member(self,authority.playerShip()->GetName(),0);frame.members.back().flags=4;
        const char* parentName=membership==2?"Renamed public captain":"Public captain";
        if(membership)member(parent,parentName,side);
        member(passenger,"Reported crew",side);
        Net::ShipIntel intel;intel.id=passenger;intel.serial=100001;intel.side=side;intel.pilotType=c_ptPlayer;
        Net::text(intel.name,"Reported crew");intel.status.state=state;
        intel.status.flags=unknown?Status::Unknown:0;intel.status.stateTime=1000;
        bool crew=state==Status::Observer || state==Status::Turret;
        intel.status.hull=crew || state==Status::Dead?NA:hull->GetObjectID();
        intel.status.sector=home->GetCluster()->GetObjectID();
        intel.status.station=state==Status::Docked || crew?home->GetObjectID():NA;
        intel.status.parent=crew?parent:NA;frame.shipIntel.push_back(intel);
        if(faction==0){
            Net::Pilot account;account.id=passenger;account.side=side;account.wing=7;
            account.money=1234;account.autoDonate=parent;Net::text(account.name,"Reported crew");frame.pilots.push_back(account);
        }
        replica.applyNetworkFrame(Net::frame(Net::packet(frame)));
        auto data=replica.playerPaneData();
        auto find=[&](int id)->const PlayerPaneRow&{auto i=std::find_if(data.rows.begin(),data.rows.end(),[&](auto& row){return row.id==id;});require(i!=data.rows.end(),"public status row missing");return *i;};
        const auto& row=find(passenger);
        require(!replica.core()->GetShip(passenger) && !replica.core()->GetShip(parent),"roster fixture unexpectedly has physical ship replicas");
        // Frozen TeamPane/ExpandedPlayerPainter semantics: PlayerInfo supplies
        // public names/alliances and the existing private money/donation state;
        // LastSeenState supplies the dock/observer/turret presentation.
        require(row.human && row.current==!unknown && row.docked==(state==Status::Docked),"reported human/current/docked state differs from original PlayerInfo");
        if(row.allied!=allied || (membership!=0 && find(parent).allied!=allied))throw std::runtime_error("Roster alliance: faction="+std::to_string(faction)+" state="+std::to_string(state)+" membership="+std::to_string(membership)+" viewer="+std::to_string(data.side)+" owner="+std::to_string(row.side)+" allied="+std::to_string(row.allied));
        require(row.location==(crew && membership?std::string("aboard ")+parentName:home->GetCluster()->GetName()),"passenger label loses the current public parent name without parent ship intel");
        require(row.ship==(crew?"":state==Status::Dead?"<none>":hull->GetName()),"observer/turret/dead/hull caption differs from original ShipName");
        require(row.money==(faction==0?1234:faction==1?0:-1) && row.autoDonate==(faction==0?parent:NA) && row.wing==(faction==0?7:NA),"remote roster loses known private money/donation/wing state or reveals another faction's account");
        if(!opened){pane.toggle(replica);pane.view(size,1);std::this_thread::sleep_for(std::chrono::milliseconds(550));opened=true;}
        pane.sync(replica,true);
        const auto& tabs=pane.view(size,1);auto tab=std::find_if(tabs.items.begin(),tabs.items.end(),[&](auto& item){return item.action.op==MenuOp::SelectTeam && item.action.value==side && item.action.delta==1;});
        require(tab!=tabs.items.end(),"expanded original faction tab missing");
        auto point=originalMenuLayout(size,1,tabs.canvas).pixel(glm::vec2(tab->bounds)+glm::vec2(3)+menuSlideOffset(tabs,size,uiTime()));
        pane.pointer(size,point,SDL_BUTTON_LEFT,true);pane.pointer(size,point,SDL_BUTTON_LEFT,false);
        auto inspect=[&](bool compact){
            const auto& view=pane.view(size,1);
            auto name=std::find_if(view.artwork.begin(),view.artwork.end(),[&](auto& c){return c.name=="player/"+std::to_string(passenger);});
            require(name!=view.artwork.end(),"original status painter drops a public crew row");
            require(name->font==(state==Status::Docked?"smallBoldFont":"smallFont") && name->color==(state==Status::Docked?glm::vec4(.617f,0,.156f,1):glm::vec4(1)),"original docked name font/color differs for observers or stale reports");
            float y=name->bounds.y-2;
            auto text=[&](std::string_view symbol)->const OriginalControl*{for(auto& c:view.artwork)if(c.name==symbol && c.bounds.y>=y && c.bounds.y<y+15)return &c;return nullptr;};
            if(compact){
                float brightness=!unknown || faction==0?1.f:.75f;
                for(auto label:{"playerKills","playerDeaths"}){auto* c=text(label);require(c && c->color==glm::vec4(brightness,brightness,brightness,1),"compact original stale-status tint incorrectly dims own faction");}
            }else {auto* location=text("playerLocation");require(location && location->text==row.location,"original expanded painter loses the resolved public parent caption");}
            if(faction==0){
                auto* credits=text("playerCredits");require(credits && (compact?state==Status::Docked?credits->text.empty():credits->text=="$: 1234":credits->text=="1234"),"known remote credits disappear from original expanded/compact painter");
                require(std::any_of(view.artwork.begin(),view.artwork.end(),[&](auto& c){return c.name=="icondonateyoubmp" && c.bounds.y==y;}),"original donation icon loses a route whose pilot/recipient have no physical replicas");
            }
            else if(faction==1){
                auto* credits=text("playerCredits");
                require(compact?credits && credits->text==Allegiance::PlayerPaneCompactAccount(data.commander,row.commander,row.docked,row.dockedSeconds,0):!credits,"original compact allied credits or expanded own-only account gate differs");
            }
        };
        inspect(false);
        // Select the wholly unknown public parent when available. Donate and
        // commander routing depend on membership, not scanner/ship reports.
        int target=membership?parent:passenger;
        const auto& roster=pane.view(size,1);auto selected=std::find_if(roster.items.begin(),roster.items.end(),[&](auto& item){return item.action.op==MenuOp::RosterSelect && item.action.value==target;});
        require(selected!=roster.items.end(),"public roster selection missing");
        point=originalMenuLayout(size,1,roster.canvas).pixel(glm::vec2(selected->bounds)+glm::vec2(3)+menuSlideOffset(roster,size,uiTime()));
        pane.pointer(size,point,SDL_BUTTON_LEFT,true);pane.pointer(size,point,SDL_BUTTON_LEFT,false);
        const auto& controls=pane.view(size,1);
        auto enabled=[&](MenuOp op){auto i=std::find_if(controls.items.begin(),controls.items.end(),[&](auto& item){return item.action.op==op && item.action.value==target;});require(i!=controls.items.end(),"original player action control missing");return i->enabled;};
        require(enabled(MenuOp::Donate)==(allied && data.money>0 && !data.closed),"original allied Donate gate requires a physical replica or reveals an enemy action");
        require(enabled(MenuOp::AutoDonate)==(faction==0 && data.commander && !data.closed),"original commander donation-route gate depends on a physical ship or accepts an allied/enemy account");
        pane.toggle(replica,true);inspect(true);pane.toggle(replica,false);++cases;
    }
    // A subsequent full faction report retires private metadata even when the
    // public membership remains: no inherited donation after departure/change.
    auto retired=authority.networkFrame(self,++tick,0);
    Net::Member stillPublic;stillPublic.id=passenger;stillPublic.side=0;Net::text(stillPublic.name,"Reported crew");retired.members.push_back(stillPublic);
    replica.applyNetworkFrame(Net::frame(Net::packet(retired)));auto final=replica.playerPaneData();
    auto row=std::find_if(final.rows.begin(),final.rows.end(),[&](auto& p){return p.id==passenger;});
    require(row!=final.rows.end() && row->money==-1 && row->autoDonate==NA && row->wing==NA,"later faction report retains an absent private account/donation/wing record");
    printf("Original roster: %d own/allied/hostile, five reported states, current/stale and absent/renamed-parent combinations; expanded/compact painters, private metadata without physical replicas and retirement passed\n",cases);
}
}
