#pragma once
#include "../game.hpp"
#include "../test_fixture.hpp"

namespace NativeTest {
inline void checkPredictedInvestments(Native::Assets& assets){
    using namespace Native;
    SessionOptions options;options.scenario=Scenario::Conquest;options.enemies=0;options.rules.miners=0;options.rules.startingMoney=9;options.startInHangar=true;
    auto loader=[](auto&,auto&){return ModelHandle{1,1};};
    Game authority(assets.root(),loader,0,0,options,{},GameRole::Authority),replica(assets.root(),loader,0,0,options,{},GameRole::Replica);
    int pilot=authority.state().shipId,side=authority.state().playerSide;Net::Tick tick=60;
    auto report=[&]{auto f=authority.networkFrame(pilot,++tick,0);return Net::frame(Net::packet(f));};
    auto baseline=report();replica.applyNetworkFrame(baseline);
    std::vector<Net::Action> actions;bool writable=true;
    replica.networkActions([&](const auto& action){if(!writable)return false;actions.push_back(action);return true;});
    auto choices=replica.researchChoices();std::vector<ResearchChoice> available;
    for(const auto& row:choices)if(row.available && !row.complete && row.price-row.invested>=32)available.push_back(row);
    require(available.size()>=2 && replica.state().money>64,"predicted funding fixture needs two affordable buckets and a wallet");
    auto chosen=available.front();int id=chosen.id,paid=chosen.invested,wallet=replica.state().money,cases=0;
    auto check=[&](int spent){require(replica.state().money==wallet-spent && replica.core()->GetSide(side)->GetBucket(id)->GetMoney()==paid+spent,"local funding differs from original bounded wallet/bucket update");};
    for(int amount:{-2,0,1,17,32,-1,INT_MAX}){
        replica.applyNetworkFrame(baseline);actions.clear();replica.clearSounds();
        auto time=replica.core()->GetSide(side)->GetBucket(id)->GetTime();int ships=replica.core()->GetShips()->n();
        int expected=amount==0 || amount< -1?0:std::min({wallet,chosen.price-paid,amount<0?wallet:amount});
        require(replica.invest(id,amount)==(expected>0),"predicted investment validation differs from original funding limits");check(expected);
        require(actions.size()==size_t(expected>0),"investment does not send exactly one action per valid click");
        if(expected){
            require(actions.front().op==Net::Operation::Invest && actions.front().args[0]==id && actions.front().args[1]==expected,"wire action sends a stale/unbounded amount instead of actual payment");
            const Game::InvestmentIntent intent{side,id,expected};
            for(int retry=0;retry<3;++retry){replica.applyNetworkFrame(baseline);replica.replayInvestments(std::span(&intent,1));check(expected);}
            require(actions.size()==1 && replica.sounds().empty(),"funding replay retransmits or repeats interface feedback");
            require(replica.core()->GetSide(side)->GetBucket(id)->GetTime()==time && replica.core()->GetShips()->n()==ships,"predicted funding advances production or creates authority units");
        }
        ++cases;
    }
    replica.applyNetworkFrame(baseline);actions.clear();writable=false;
    require(!replica.invest(id,17),"failed transport accepts an investment");check(0);require(actions.empty(),"failed investment retained an action");writable=true;
    for(int invalid:{-1,SHRT_MAX+1,INT_MAX})require(!replica.invest(invalid,17),"invalid bucket accepted by predicted funding");check(0);
    auto poor=baseline;auto own=std::find_if(poor.pilots.begin(),poor.pilots.end(),[&](const auto& p){return p.id==pilot;});require(own!=poor.pilots.end(),"funding fixture has no account record");own->money=23;
    replica.applyNetworkFrame(poor);actions.clear();require(replica.invest(id,17) && replica.invest(available[1].id,17),"ordered funding burst rejected");
    require(actions.size()==2 && actions[0].args[1]==17 && actions[1].args[1]==6 && replica.state().money==0 && !replica.invest(id,1),"burst clicks use stale wallet or lose the final bounded payment");
    std::array intents{Game::InvestmentIntent{side,id,17},Game::InvestmentIntent{side,available[1].id,6}};
    replica.applyNetworkFrame(poor);replica.replayInvestments(intents);require(replica.state().money==0 && replica.core()->GetSide(side)->GetBucket(id)->GetMoney()==paid+17 && actions.size()==2,"ordered replay loses funding or sends duplicate commands");
    replica.applyNetworkFrame(baseline);Game::InvestmentIntent foreign{side+1,id,17};replica.replayInvestments(std::span(&foreign,1));check(0);
    actions.clear();require(replica.invest(id,17),"canonical payment fixture");require(authority.authorityAction(pilot,actions.front()),"authority rejects bounded local payment");
    replica.applyNetworkFrame(report());check(17);require(authority.pilotBalance(pilot)==replica.state().money,"canonical report duplicates payment or loses conservation");
    printf("Original immediate investment feedback: %d amount boundaries, failed send, invalid IDs, repeated stale reports, wallet-limited bursts, faction context, production isolation and canonical conservation passed\n",cases);
}
}
