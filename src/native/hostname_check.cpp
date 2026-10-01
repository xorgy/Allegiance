#include "hostname_transport.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <map>
#include <set>
#include <thread>

using namespace Native::Net;
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct FakeState {
    std::map<Connection,std::string> opened;std::set<Connection> closed;std::set<std::string> refuse;
    std::vector<ConnectionEvent> status;std::vector<Message> incoming;std::vector<OutgoingMessage> batch;
    Connection next=9000;int released{};
};
class FakeSocket final:public Transport {
    std::shared_ptr<FakeState> state_;
  public:
    explicit FakeSocket(std::shared_ptr<FakeState> state):state_(std::move(state)){}
    Connection open(std::string_view address)override{if(state_->refuse.contains(std::string(address)))throw std::runtime_error("Cannot open route");auto id=++state_->next;state_->opened[id]=address;return id;}
    bool writable(Connection id)override{return state_->opened.contains(id) && !state_->closed.contains(id);}
    bool send(Connection id,std::span<const std::byte>,Lane)override{return writable(id);}
    std::vector<bool> sendBatch(std::span<const OutgoingMessage> messages)override{state_->batch.assign(messages.begin(),messages.end());return std::vector<bool>(messages.size(),true);}
    void close(Connection id,std::string_view,bool=false)override{state_->closed.insert(id);}
    std::vector<Message> receive()override{return std::exchange(state_->incoming,{});}
    std::vector<ConnectionEvent> events()override{return std::exchange(state_->status,{});}
    ConnectionQuality quality(Connection)override{return {12,.9f};}
    std::string remoteAddress(Connection id)override{return state_->opened.at(id);}
    std::string remoteHost(Connection)override{return "127.0.0.1";}
};
struct Fixture {
    std::shared_ptr<FakeState> state=std::make_shared<FakeState>();
    std::vector<std::shared_ptr<EndpointResolution>> requests;
    int64_t time=1000000;std::unique_ptr<Transport> transport;
    Fixture(){transport=hostnameTransport(std::make_unique<FakeSocket>(state),[&](const Endpoint&){auto r=std::make_shared<EndpointResolution>();requests.push_back(r);return r;},[&]{return time;});}
    void complete(ResolvedEndpoints result){requests.back()->complete(std::move(result));}
    std::vector<ConnectionEvent> events(){return transport->events();}
};
void parser(){
    for(auto input:{"localhost","example.net:65535","EXAMPLE.NET.:27500","127.0.0.1:27500","[::1]:27500","::1","[::ffff:127.0.0.1]:27500"}){
        auto endpoint=parseEndpoint(input);require(endpoint.port>0 && !endpoint.host.empty() && parseEndpoint(endpoint.address()).address()==endpoint.address(),"endpoint canonical/default-port round trip");
    }
    require(!parseEndpoint("localhost").numeric && !parseEndpoint("123.example.net").numeric && parseEndpoint("127.0.0.1").numeric && parseEndpoint("::1").numeric,"DNS/numeric classification");
    for(auto input:{"",":27500","host:","host:0","host:-1","host:65536","host:1x","host:1:xyz","[]:27500","[::1]tail","[localhost]:27500","host/path","host name","host..name","-host","host-",".","[fe80::1%eth0]:27500"}){
        bool rejected=false;try{parseEndpoint(input);}catch(const std::exception&){rejected=true;}require(rejected,"malformed endpoint accepted");
    }
    bool rejected=false;try{parseEndpoint(std::string_view("host\0suffix",11));}catch(const std::exception&){rejected=true;}require(rejected,"embedded-NUL hostname accepted");
    puts("Endpoint parser: hostnames/default ports, numeric IPv4/IPv6, canonical round trips, labels, strict ports and malformed/scoped/NUL rejection passed");
}
void races(){
    Fixture f;auto id=f.transport->open("example.net:27500");require(f.state->opened.empty() && !f.transport->writable(id) && !f.transport->send(id,{},Lane::Control) && f.events().empty(),"pending DNS blocks or exposes an unconnected socket");
    f.complete({{"[::1]:27500","127.0.0.1:27500"},{}});require(f.events().empty() && f.state->opened.size()==1,"resolved first route not started independently");auto first=f.state->opened.begin()->first;
    f.time+=249999;f.events();require(f.state->opened.size()==1,"fallback launches before stagger deadline");++f.time;f.events();require(f.state->opened.size()==2,"broken preferred route stalls fallback");auto winner=f.state->opened.rbegin()->first;
    f.state->status.push_back({winner,true});auto events=f.events();require(events.size()==1 && events[0].peer==id && events[0].connected && f.state->closed.contains(first),"winning route changes logical identity or retains loser");
    require(f.transport->remoteAddress(id)=="127.0.0.1:27500" && f.transport->remoteHost(id)=="127.0.0.1" && f.transport->quality(id).ping==12 && f.transport->writable(id),"selected numeric route/quality/admission not forwarded");
    struct Payload {std::shared_ptr<FakeState> state;std::array<std::byte,2> bytes{std::byte{1},std::byte{2}};};
    for(auto peer:{first,winner}){auto* data=new Payload{f.state};f.state->incoming.emplace_back(peer,std::span<const std::byte>(data->bytes),data,[](const void* p){auto* data=static_cast<const Payload*>(p);++data->state->released;delete data;});}
    auto messages=f.transport->receive();require(messages.size()==1 && messages[0].peer==id && messages[0].data[1]==std::byte{2} && f.state->released==1,"loser packets leak or winning borrowed message is copied/lost");messages.clear();require(f.state->released==2,"winning message ownership not released");
    require(f.events().empty(),"receive/events repeats a connection notification");
    auto pending=f.transport->open("pending.net");auto data=std::make_shared<const std::vector<std::byte>>(3);std::array<OutgoingMessage,3> batch{{{pending,data,Lane::Control},{id,data,Lane::World,true},{999,data,Lane::State}}};
    auto sent=f.transport->sendBatch(batch);require(sent==std::vector<bool>({false,true,false}) && f.state->batch.size()==1 && f.state->batch[0].peer==winner && f.state->batch[0].data==data && f.state->batch[0].discardIfQueued && f.state->batch[0].lane==Lane::World,"batched logical translation loses shared payload/lane/admission/results");
    f.state->status.push_back({first,true});require(f.events().empty(),"late losing callback reselects a route");f.transport->close(id,"Closed");require(f.state->closed.contains(winner) && !f.transport->writable(id),"logical close retains winning route");
    f.state->status.push_back({winner,true});require(f.events().empty(),"closed logical peer resurrects from a late callback");
    puts("Hostname transport: asynchronous start, staggered fallback, one winner/stable peer, loser retirement, numeric routing, borrowed receive ownership and shared batch translation passed");
}
void retirement(){
    Fixture f;auto failed=f.transport->open("missing.net");f.complete({{},"No such host",false});auto events=f.events();require(events.size()==1 && events[0].peer==failed && !events[0].connected && !events[0].retryable && events[0].reason=="No such host" && f.state->opened.empty(),"DNS refusal allocates a socket or loses final error semantics");
    auto timed=f.transport->open("slow.net");auto slow=f.requests.back();f.time+=5000000;events=f.events();require(events.size()==1 && events[0].peer==timed && events[0].retryable && slow->cancelled(),"DNS timeout waits for a worker or fails to cancel request");slow->complete({{"127.0.0.1:27500"},{}});require(f.events().empty() && f.state->opened.empty(),"late DNS result resurrects a timed-out peer");
    auto cancelled=f.transport->open("cancelled.net");auto request=f.requests.back();f.transport->close(cancelled,"Cancelled");request->complete({{"127.0.0.1:27500"},{}});require(request->cancelled() && f.events().empty() && f.state->opened.empty(),"closed pending lookup retains a callback target");
    f.transport->open("teardown.net");auto teardown=f.requests.back();f.transport.reset();require(teardown->cancelled(),"transport teardown leaves its pending resolver live");teardown->complete({{"127.0.0.1:27500"},{}});
    Fixture g;auto id=g.transport->open("routes.net");g.complete({{"127.0.0.1:27500","127.0.0.2:27500"},{}});g.events();auto first=g.state->opened.begin()->first;g.state->status.push_back({first,false,"First failed",true});g.events();g.events();require(g.state->opened.size()==2,"early route failure waits for the stagger deadline");
    auto second=g.state->opened.rbegin()->first;g.state->status.push_back({second,false,"Last failed",false});events=g.events();require(events.size()==1 && events[0].peer==id && !events[0].retryable && events[0].reason=="Last failed" && g.events().empty(),"all-route failure repeats/losses final reason or peer identity");
    Fixture h;h.state->refuse.insert("127.0.0.1:27500");auto viable=h.transport->open("viable.net");h.complete({{"127.0.0.1:27500","127.0.0.2:27500"},{}});h.events();require(h.state->opened.size()==1 && h.state->opened.begin()->second=="127.0.0.2:27500","immediate route-open failure prevents later candidate");h.state->status.push_back({h.state->opened.begin()->first,true});require(h.events()[0].peer==viable,"fallback after open exception changes peer identity");
    puts("Hostname retirement: permanent errors, transient timeout, cancellation/teardown, late results/callbacks, early fallback, all-route failure and open exceptions passed");
}
void systemResolver(){
    auto result=resolveEndpoint(parseEndpoint("localhost:27850"));std::optional<ResolvedEndpoints> resolved;auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    do{resolved=result->take();if(resolved)break;std::this_thread::sleep_for(std::chrono::milliseconds(2));}while(std::chrono::steady_clock::now()<deadline);
    require(resolved && resolved->error.empty() && !resolved->addresses.empty() && resolved->addresses.size()<=8,"system localhost resolution failed");std::set<std::string> unique;
    for(const auto& address:resolved->addresses)require(parseEndpoint(address).numeric && parseEndpoint(address).port==27850 && unique.insert(address).second,"system resolver returns duplicate/non-numeric/wrong-port endpoints");
    puts("System resolver: real asynchronous localhost lookup, IPv4/IPv6 numeric UDP endpoints, deduplication and bounded results passed");
}
void numericPeers(){
    Fixture f;std::set<Connection> peers;
    for(int i=0;i<1024;++i)require(peers.insert(f.transport->open("127.0.0.1:27500")).second,"numeric client peer identity collision");
    require(f.requests.empty() && f.state->opened.size()==1024,"numeric client peers incur DNS work or an unrelated hostname cap");
    for(const auto& [id,_]:f.state->opened)f.state->status.push_back({id,true});
    auto events=f.events();require(events.size()==1024,"numeric client peers lose connection notifications");for(const auto& event:events)require(event.connected && peers.contains(event.peer),"numeric peer notification uses an underlying handle");
    f.transport.reset();require(f.state->closed.size()==1024,"client transport teardown retains numeric peers");
    puts("Numeric client transport: 1,024 stable peers, no DNS work/cap, correct notifications and complete teardown passed");
}
}
int main(){try{parser();races();retirement();systemResolver();numericPeers();return 0;}catch(const std::exception& error){fprintf(stderr,"Hostname check: %s\n",error.what());return 1;}}
