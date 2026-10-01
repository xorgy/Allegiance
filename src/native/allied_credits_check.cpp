// Original MONEY_CHANGE audiences and compact F6 accounts; no scene/rendering.
#include "game.hpp"
#include "network_arena.hpp"
#include "test_fixture.hpp"
#include "../ClientCore/ObservedCredits.h"
#include <cstdio>
using namespace Native;
using NativeTest::require;
namespace {
int money(Game& game,int id) {
    auto pane=game.playerPaneData();auto row=std::find_if(pane.rows.begin(),pane.rows.end(),[&](auto& p){return p.id==id;});
    require(row!=pane.rows.end(),"public credit row missing");return row->money;
}
void records(const std::filesystem::path& art) {
    auto loader=[](auto&,auto&){return ModelHandle{1,1};};
    SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.enemies=0;
    options.rules.miners=0;options.rules.teams=4;options.rules.startingMoney=9;options.alliances={0,0,0,-1,-1,-1};
    Game host(art,loader,0,0,options,{},GameRole::Authority);
    int donor=host.state().shipId,recipient=host.addLocalPilot("Allied recipient",1),teammate=host.addLocalPilot("Recipient teammate",1),third=host.addLocalPilot("Third ally",2),enemy=host.addLocalPilot("Hostile pilot",3);
    require(recipient>=0 && teammate>=0 && third>=0 && enemy>=0,"allied credit participants");
    std::vector<int> ids{donor,recipient,teammate,third,enemy};Net::Tick tick=60;
    auto snapshot=[&](int id){
        auto frame=host.networkFrame(id,tick,0);
        for(int pilot:ids)if(auto* ship=host.core()->GetShip(pilot)) {Net::Member member;member.id=pilot;member.side=ship->GetSide()->GetObjectID();Net::text(member.name,ship->GetName());member.flags=host.teamLeader(member.side)==pilot?4:0;frame.members.push_back(member);}
        return Net::frame(Net::packet(frame));
    };
    std::vector<std::unique_ptr<Game>> views;
    for(int id:ids){views.push_back(std::make_unique<Game>(art,loader,0,0,options,EffectLoader{},GameRole::Replica));views.back()->applyNetworkFrame(snapshot(id));}
    auto publish=[&]{tick+=Net::snapshotInterval;host.beginNetworkPublication(tick);for(size_t i=0;i<ids.size();++i)views[i]->applyNetworkFrame(snapshot(ids[i]));};
    auto donate=[&](int by,int to,int amount){Net::Action action;action.op=Net::Operation::Donate;action.args[0]=to;action.args[1]=amount;require(host.authorityAction(by,action),"original cross-faction donation accepted");};
    require(money(*views[0],recipient)==0 && money(*views[1],donor)==0 && money(*views[3],recipient)==0 && money(*views[4],donor)==-1,"initial allied accounts reveal balances or hostile accounts");
    // Deliberately omit the first report. Only the donor/recipient factions
    // receive MONEY_CHANGE in FedSrv; the third ally gets no account changes.
    donate(donor,recipient,100);donate(donor,recipient,200);publish();
    require(money(*views[0],recipient)==300 && money(*views[1],donor)==-300 && money(*views[2],donor)==-300 && money(*views[3],recipient)==0,"skipped reports lose signed donations or expose changes to a third ally");
    require(money(*views[0],donor)==host.pilotBalance(donor) && money(*views[1],recipient)==host.pilotBalance(recipient),"partial observations replace own authoritative balances");
    auto raw=snapshot(donor);auto account=std::find_if(raw.pilots.begin(),raw.pilots.end(),[&](auto& p){return p.id==recipient;});
    require(account!=raw.pilots.end() && account->money==300 && account->identity && account->money!=host.pilotBalance(recipient),"foreign report contains an actual account or lacks lifetime identity");
    views[0]->applyNetworkFrame(raw);require(money(*views[0],recipient)==300,"duplicate report replays a donation");
    Game late(art,loader,0,0,options,{},GameRole::Replica);late.applyNetworkFrame(raw);require(money(late,recipient)==0,"new subscriber inherits unseen donation history");
    donate(donor,recipient,50);publish();late.applyNetworkFrame(snapshot(donor));
    require(money(*views[0],recipient)==350 && money(late,recipient)==50,"different subscriber baselines require private reports or lose a live donation");
    donate(recipient,donor,30);publish();late.applyNetworkFrame(snapshot(donor));
    require(money(*views[0],recipient)==320 && money(*views[1],donor)==-320 && money(late,recipient)==20,"reverse donation loses signed partial credit accounting");
    donate(recipient,teammate,500);publish();late.applyNetworkFrame(snapshot(donor));
    require(money(*views[0],recipient)==320 && money(*views[0],teammate)==0 && money(*views[3],donor)==0,"foreign same-faction transfers leak actual money changes");
    auto policy=host.missionPolicy();auto original=policy;policy.alliances[1]=NA;
    require(host.applyMissionPolicy(policy),"remove credit audience alliance");for(auto& view:views)require(view->applyMissionPolicy(policy),"replica removes alliance");publish();
    require(money(*views[0],recipient)==-1,"former ally retains a visible credit account");
    require(host.applyMissionPolicy(original),"restore credit audience alliance");for(auto& view:views)require(view->applyMissionPolicy(original),"replica restores alliance");publish();
    require(money(*views[0],recipient)==320 && money(*views[1],donor)==-320,"alliance changes discard observed credit memory");
    // Both membership changes can occur between publications. A physical
    // ship/callsign survives, but its account and observation baseline do not.
    require(host.moveNetworkPilotToTeam(recipient,noTeam) && host.moveNetworkPilotToTeam(recipient,1),"account reset preserves the pilot through No Team");publish();late.applyNetworkFrame(snapshot(donor));
    require(money(*views[0],recipient)==0 && money(late,recipient)==0 && money(*views[1],donor)==0 && money(*views[2],donor)==-320,"unpublished leave/rejoin resets another pilot's observation or inherits a former account's baseline");
    donate(donor,recipient,25);publish();late.applyNetworkFrame(snapshot(donor));require(money(late,recipient)==25,"new account donation is lost after baseline retirement");
    // Account identity, not callsign or physical hull, owns a baseline.
    auto replacement=snapshot(donor);for(auto& p:replacement.pilots)if(p.id==recipient){++p.identity;p.money=900;}
    views[0]->applyNetworkFrame(replacement);require(money(*views[0],recipient)==0,"reused account identity inherits the previous pilot's credits");
    auto capture=host.networkReport(donor,tick,0);auto world=capture.worlds.front().frame;
    Net::ArenaEncoder encoder;for(int subscriber=0;subscriber<1024;++subscriber)encoder.publishWorld(0,tick,1,*world);
    require(encoder.encodes()==1,"credit subscriber baselines trigger per-client encoding");
    static_assert(sizeof(Net::Pilot)==48,"Observed credits must not grow pilot records");
    puts("Allied credits: original signed faction audiences, skipped/duplicate reports, own balances, newcomer baselines, reverse/same-team transfers, alliance memory, account identity and one encoding for 1024 subscribers passed");
}
void lifetimes() {
    Allegiance::CreditObservations observed;observed.audience(0);
    require(observed.receive(7,1,3,100)==0 && observed.receive(7,1,3,130)==30,"partial account baseline");
    observed.retain([](int){return false;});require(observed.receive(7,1,3,140)==0,"departed public account retains a baseline");
    observed.audience(2);require(observed.receive(7,1,3,160)==0,"changed viewer faction inherits private donation history");
    require(observed.receive(7,2,3,180)==0 && observed.receive(7,2,4,190)==0,"changed owner or generation retains old credits");
    observed.audience(2,10);require(observed.receive(7,1,4,200)==0 && observed.receive(7,1,4,220)==20,"viewer account baseline");
    observed.audience(2,11);require(observed.receive(7,1,4,230)==0,"unchanged faction hides a reset viewer account");
    observed.audience(0);require(observed.receive(8,1,9,0xfffffff0u)==0 && observed.receive(8,1,9,20)==36,"cumulative transfer wrap loses compact signed record semantics");
    puts("Observed credit lifetimes: public departure, viewer/owner/generation changes and 32-bit wrap passed");
}
}
int main(int argc,char** argv)try{auto art=argc>1?std::filesystem::path(argv[1]):Assets::steamArtwork();lifetimes();records(art);return 0;}catch(const std::exception& e){fprintf(stderr,"Allied credits: %s\n",e.what());return 1;}
