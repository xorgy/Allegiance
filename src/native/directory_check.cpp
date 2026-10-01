#include "public_directory.hpp"
#include <cstdio>
#include <deque>

using namespace Native::Net;
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<Record T> std::vector<std::byte> packet(Kind kind,const T& value){Header h{};h.kind=kind;std::vector<std::byte> bytes;append(bytes,h);append(bytes,value);return bytes;}
class FakeTransport final:public Transport {
  public:
    struct Sent {Connection peer;std::vector<std::byte> bytes;};
    std::vector<Message> incoming;std::vector<ConnectionEvent> status;std::vector<Sent> sent;
    std::map<Connection,std::string> hosts,opened;std::vector<Connection> closed;Connection next=10000;
    bool writable(Connection)override{return true;}
    bool send(Connection peer,std::span<const std::byte> bytes,Lane)override{sent.push_back({peer,{bytes.begin(),bytes.end()}});return true;}
    Connection open(std::string_view address)override{auto id=next++;opened[id]=address;status.push_back({id,true});return id;}
    void close(Connection peer,std::string_view,bool=false)override{closed.push_back(peer);}
    std::string remoteHost(Connection peer)override{return hosts[peer];}
    std::vector<Message> receive()override{return std::exchange(incoming,{});}
    std::vector<ConnectionEvent> events()override{return std::exchange(status,{});}
    template<Record T> void message(Connection peer,Kind kind,const T& value){incoming.emplace_back(peer,packet(kind,value));}
    void accept(Connection peer,std::string host){hosts[peer]=std::move(host);status.push_back({peer,true});}
};
RegistryInfo info(const std::shared_ptr<const std::vector<std::byte>>& bytes){Reader r(*bytes);require(r.take<Header>().kind==Kind::Registry,"registry reply kind");return r.take<RegistryInfo>();}
void table(){
    RegistryTable table;auto empty=table.report();require(info(empty).count==0 && table.report()==empty,"empty registry reply or cached identity");
    for(unsigned i=1;i<=registryCapacity;++i)table.publish(i,"127.0.0.1:"+std::to_string(20000+i),1000000);
    auto full=table.report();require(info(full).count==registryCapacity && table.size()==registryCapacity,"full registry membership");
    Reader r(*full);r.take<Header>();r.take<RegistryInfo>();std::string last;
    for(unsigned i=0;i<registryCapacity;++i){auto endpoint=r.take<RegistryEndpoint>();require(terminated(endpoint.address) && std::string(endpoint.address)>last,"deterministic unique endpoint ordering");last=endpoint.address;}
    require(r.rest().empty(),"registry wire record boundary");
    for(unsigned i=1;i<=registryCapacity;++i)table.publish(i,"127.0.0.1:"+std::to_string(20000+i),5000000);
    require(table.report()==full,"heartbeats re-encode identical registry replies");
    table.publish(1000,"127.0.0.1:20001",6000000);require(table.report()==full && table.publishers()==registryCapacity+1,"duplicate publisher duplicates/re-encodes an authority");
    bool rejected=false;try{table.publish(1001,"127.0.0.2:22222",7000000);}catch(const std::exception&){rejected=true;}require(rejected,"directory capacity not enforced");
    rejected=false;try{table.publish(1000,"127.0.0.2:22222",7000000);}catch(const std::exception&){rejected=true;}require(rejected,"publisher silently changes its endpoint");
    table.remove(1);require(table.report()==full,"disconnect removes an endpoint with another live publisher");
    table.expire(35000000);require(table.size()==1 && info(table.report()).count==1 && table.report()!=full,"lease expiry or retained duplicate publisher");
    table.expire(36000000);require(table.size()==0 && info(table.report()).count==0,"last publisher lease expiry");
    require(info(full).count==registryCapacity,"mutation invalidates an in-flight immutable report");
    puts("Registry table: 256 unique authorities, deterministic raw records, shared encoding across heartbeats/duplicate publishers, bounded capacity, ownership and lease/disconnect retirement passed");
}
void service(){
    auto transport=std::make_unique<FakeTransport>();auto* io=transport.get();PublicDirectory directory(std::move(transport));
    io->accept(1,"127.0.0.2");directory.poll(1000000);
    AuthorityRegistration registration{{buildDigest(),{}},27500};
    io->message(1,Kind::RegisterAuthority,registration);directory.poll(1100000);
    require(directory.authorities()==0 && io->opened.size()==1 && io->opened.begin()->second=="127.0.0.2:27500","unverified endpoint published or advertised host trusted");
    auto probe=io->opened.begin()->first;directory.poll(1200000);
    Reader query(io->sent.back().bytes);require(io->sent.back().peer==probe && query.take<Header>().kind==Kind::Browse && query.take<BrowseQuery>().build==registration.identity.build && query.rest().empty(),"authority proof does not use the normal build/content-gated browse request");
    ServerInfo authority{};authority.instance={1,2,3,4};io->message(probe,Kind::ServerInfo,authority);directory.poll(1300000);
    require(directory.authorities()==1 && io->sent.back().peer==1,"verified authority not published/acknowledged");
    io->accept(2,"127.0.0.3");io->message(2,Kind::Browse,BrowseQuery{buildDigest(),{}});directory.poll(1400000);
    Reader listing(io->sent.back().bytes);require(listing.take<Header>().kind==Kind::Registry && listing.take<RegistryInfo>().count==1 && std::string(listing.take<RegistryEndpoint>().address)=="127.0.0.2:27500" && listing.rest().empty(),"public reply does not contain only the observed verified endpoint");
    io->message(2,Kind::Browse,BrowseQuery{buildDigest(),{}});directory.poll(1500000);
    require(std::find(io->closed.begin(),io->closed.end(),2)!=io->closed.end(),"browser reuses an expired read-only handshake");
    io->status.push_back({1,false});directory.poll(1600000);require(directory.authorities()==0,"registration disconnect leaves an advertised authority");
    io->accept(3,"::1");io->message(3,Kind::RegisterAuthority,registration);directory.poll(1700000);
    require(io->opened.rbegin()->second=="[::1]:27500","IPv6 registration endpoint formatting");
    auto ipv6Probe=io->opened.rbegin()->first;io->status.push_back({3,false});directory.poll(1800000);
    io->message(ipv6Probe,Kind::ServerInfo,ServerInfo{});directory.poll(1900000);require(directory.authorities()==0,"late proof republishes a disconnected authority");
    io->accept(4,"127.0.0.2");auto bad=registration;bad.identity.build[0]^=1;io->message(4,Kind::RegisterAuthority,bad);directory.poll(2000000);
    require(std::find(io->closed.begin(),io->closed.end(),4)!=io->closed.end(),"incompatible authority registration accepted");
    io->accept(5,"127.0.0.2");io->message(5,Kind::RegisterAuthority,registration);directory.poll(2100000);
    auto pending=io->opened.rbegin()->first;directory.poll(2200000);io->message(pending,Kind::Registry,RegistryInfo{});directory.poll(2300000);
    require(directory.authorities()==0 && std::find(io->closed.begin(),io->closed.end(),5)!=io->closed.end(),"registry forwarding loop accepted as an authority proof");
    io->accept(6,"127.0.0.2");io->message(6,Kind::RegisterAuthority,registration);directory.poll(2400000);directory.poll(7400000);
    require(directory.authorities()==0 && std::find(io->closed.begin(),io->closed.end(),6)!=io->closed.end(),"unreachable registration probe does not expire");
    puts("Directory service: observed IPv4/IPv6 source, proof before publication, normal compatibility gate, anonymous browse, incompatible/unreachable authorities, late proof and forwarding-loop rejection passed");
}
void registration(){
    auto transport=std::make_unique<FakeTransport>();auto* io=transport.get();DirectoryRegistration registration("127.0.0.1:27599",27500,{},std::move(transport));
    registration.poll(1000000);require(io->opened.size()==1 && !registration.registered(),"registration state is optimistic before proof acknowledgement");
    registration.poll(1030000);Reader request(io->sent.back().bytes);require(request.take<Header>().kind==Kind::RegisterAuthority && request.take<AuthorityRegistration>().port==27500 && request.rest().empty(),"registration request port/raw record");
    auto connection=io->opened.begin()->first;io->message(connection,Kind::AuthorityRegistered,AuthorityRegistered{});registration.poll(1060000);require(registration.registered(),"registration acknowledgement not applied");
    auto sent=io->sent.size();registration.poll(1070000);require(io->sent.size()==sent,"directory polling ignores idle pacing");
    registration.poll(11030000);require(io->sent.size()==sent+1,"registration lease heartbeat missing");
    registration.poll(32000000);require(!registration.registered() && std::find(io->closed.begin(),io->closed.end(),connection)!=io->closed.end(),"lost directory acknowledgements leave stale registration");
    registration.poll(37000000);require(io->opened.size()==2,"directory outage does not retry independently");
    puts("Authority registrar: acknowledgement-based state, paced polling, periodic renewal, lost-ack expiry and independent retry passed");
}
}
int main(){try{table();service();registration();return 0;}catch(const std::exception& e){fprintf(stderr,"Directory check: %s\n",e.what());return 1;}}
