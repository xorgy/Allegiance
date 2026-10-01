#include "public_directory.hpp"
#include <chrono>

namespace Native::Net {
namespace {
constexpr int64_t second=1000000,lease=registryLeaseSeconds*second;
template<Record T> std::vector<std::byte> record(Kind kind,const T& value){
    Header h{};h.kind=kind;std::vector<std::byte> result;append(result,h);append(result,value);return result;
}
// A probe reads only enough of a normal authority reply to prove an actual,
// compatible listener. Browsers still validate the complete listing before use.
bool authorityProof(std::span<const std::byte> bytes){
    Reader reader(bytes);auto h=reader.take<Header>();if(h.signature!=magic)return false;
    if(h.kind==Kind::ServerInfo){auto info=reader.take<ServerInfo>();return validInstance(info.instance) && reader.rest().empty();}
    if(h.kind!=Kind::Directory)return false;
    auto info=reader.take<DirectoryInfo>();
    if(!validInstance(info.instance) || !info.count || info.count>info.capacity || info.capacity>maxHostedGames || info.cores>Allegiance::CoreCatalogCapacity || info.reserved ||
        reader.rest().size()!=size_t(info.cores)*sizeof(CoreInfo)+size_t(info.count)*sizeof(HostedGame))return false;
    for(uint32_t i=0;i<info.cores;++i)reader.take<CoreInfo>();
    return reader.take<HostedGame>().info.instance==info.instance;
}
}
int64_t directoryClock(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
void RegistryTable::publish(Connection peer,std::string address,int64_t now){
    if(!peer || address.empty() || address.size()>=sizeof(RegistryEndpoint::address) || address.find('\0')!=address.npos)throw std::runtime_error("Invalid authority endpoint");
    if(auto found=leases_.find(peer);found!=leases_.end()){
        if(found->second.address!=address)throw std::runtime_error("Registration listener changed");
        found->second.expires=now+lease;return;
    }
    if(leases_.size()>=registryCapacity*2 || (!owners_.contains(address) && owners_.size()>=registryCapacity))throw std::runtime_error("Public directory is full");
    leases_.emplace(peer,Lease{address,now+lease});
    if(++owners_[address]==1){++revision_;encoded_.reset();}
}
void RegistryTable::remove(Connection peer){
    auto found=leases_.find(peer);if(found==leases_.end())return;
    auto owner=owners_.find(found->second.address);
    if(--owner->second==0){owners_.erase(owner);++revision_;encoded_.reset();}
    leases_.erase(found);
}
void RegistryTable::expire(int64_t now){
    for(auto i=leases_.begin();i!=leases_.end();){auto peer=i->first;bool expired=now>=i->second.expires;++i;if(expired)remove(peer);}
}
std::shared_ptr<const std::vector<std::byte>> RegistryTable::report(){
    if(!encoded_){
        auto bytes=std::make_shared<std::vector<std::byte>>();bytes->reserve(sizeof(Header)+sizeof(RegistryInfo)+owners_.size()*sizeof(RegistryEndpoint));
        Header h{};h.kind=Kind::Registry;append(*bytes,h);append(*bytes,RegistryInfo{revision_,uint32_t(owners_.size())});
        for(const auto& [address,count]:owners_){RegistryEndpoint endpoint{};text(endpoint.address,address);append(*bytes,endpoint);}
        encoded_=std::move(bytes);
    }
    return encoded_;
}
void PublicDirectory::retire(Connection peer,std::string_view reason){
    table_.remove(peer);peers_.erase(peer);transport_->close(peer,reason);
    for(auto i=probes_.begin();i!=probes_.end();){if(i->second.publisher==peer){transport_->close(i->first,"Registration ended");i=probes_.erase(i);}else ++i;}
}
void PublicDirectory::finishProbe(Connection probe,bool valid,int64_t now){
    auto i=probes_.find(probe);if(i==probes_.end())return;
    auto value=std::move(i->second);probes_.erase(i);transport_->close(probe,"Authority probe complete");
    if(!peers_.contains(value.publisher))return;
    if(!valid){retire(value.publisher,"Authority listener is not reachable or compatible");return;}
    try{
        table_.publish(value.publisher,std::move(value.address),now);
        auto reply=record(Kind::AuthorityRegistered,AuthorityRegistered{});
        if(!transport_->send(value.publisher,reply,Lane::Control))retire(value.publisher,"Registration acknowledgement failed");
    }catch(const std::exception& e){retire(value.publisher,e.what());}
}
void PublicDirectory::poll(int64_t now){
    table_.expire(now);
    auto incoming=transport_->receive();
    for(const auto& event:transport_->events()){
        if(auto probe=probes_.find(event.peer);probe!=probes_.end()){
            if(!event.connected)finishProbe(event.peer,false,now);
            else if(!transport_->send(event.peer,record(Kind::Browse,probe->second.registration.identity),Lane::Control))finishProbe(event.peer,false,now);
        }else if(event.connected){
            if(peers_.size()>=registryCapacity*4)transport_->close(event.peer,"Public directory is busy");
            else peers_.emplace(event.peer,Peer{now+5*second});
        }else retire(event.peer,"Directory peer disconnected");
    }
    // Messages retain their GNS ownership while being inspected. No wire object
    // or peer pointer survives table/probe mutations or a transport callback.
    for(const auto& message:incoming){
        if(probes_.contains(message.peer)){
            bool valid=false;try{valid=authorityProof(message.data);}catch(const std::exception&){}
            finishProbe(message.peer,valid,now);continue;
        }
        auto peer=peers_.find(message.peer);if(peer==peers_.end())continue;
        try{
            Reader r(message.data);auto h=r.take<Header>();if(h.signature!=magic || peer->second.browsing)throw std::runtime_error("Invalid directory request");
            if(h.kind==Kind::Browse || h.kind==Kind::FindPlayer){
                auto query=h.kind==Kind::FindPlayer?r.take<PlayerQuery>():PlayerQuery{r.take<BrowseQuery>()};
                if(!r.rest().empty() || !terminated(query.player) || query.identity.build!=buildDigest())throw std::runtime_error("Incompatible directory browser");
                peer->second.browsing=true;auto reply=table_.report();
                if(!transport_->send(message.peer,*reply,Lane::Control))retire(message.peer,"Directory reply failed");
            }else if(h.kind==Kind::RegisterAuthority){
                auto registration=r.take<AuthorityRegistration>();
                if(!r.rest().empty() || registration.reserved || registration.identity.build!=buildDigest() || !registration.port || registration.port>65535)throw std::runtime_error("Invalid authority registration");
                if(now<peer->second.nextRegistration)continue;
                if(std::any_of(probes_.begin(),probes_.end(),[&](const auto& p){return p.second.publisher==message.peer;}))continue;
                if(probes_.size()>=64)throw std::runtime_error("Authority verification is busy");
                auto address=transport_->remoteHost(message.peer);
                if(address.empty())throw std::runtime_error("Authority source address is unknown");
                if(std::count(address.begin(),address.end(),':')>1)address="["+address+"]";
                address+=":"+std::to_string(registration.port);
                auto connection=transport_->open(address);
                probes_.emplace(connection,Probe{message.peer,std::move(address),registration,now+5*second});
                peer->second.nextRegistration=now+5*second;peer->second.expires=now+lease;
            }else throw std::runtime_error("Unknown directory request");
        }catch(const std::exception& e){retire(message.peer,e.what());}
    }
    for(auto i=probes_.begin();i!=probes_.end();){auto peer=i->first;bool expired=now>=i->second.expires;++i;if(expired)finishProbe(peer,false,now);}
    for(auto i=peers_.begin();i!=peers_.end();){auto peer=i->first;bool expired=now>=i->second.expires;++i;if(expired)retire(peer,"Directory lease expired");}
}
DirectoryRegistration::DirectoryRegistration(std::string address,uint16_t port,Digest content,std::unique_ptr<Transport> transport):address_(std::move(address)),registration_{{buildDigest(),content},port},transport_(std::move(transport)){
    if(address_.empty() || address_.size()>128 || !port)throw std::runtime_error("Invalid directory registration address");
}
void DirectoryRegistration::disconnect(int64_t now,std::string_view reason){
    if(connection_)transport_->close(connection_,"Directory registration retry");
    connection_=0;connected_=false;lastAck_=0;retry_=now+5*second;error_=reason;
}
void DirectoryRegistration::poll(int64_t now){
    if(now<nextPoll_)return;nextPoll_=now+25000;
    if(!transport_)transport_=clientTransport();
    auto incoming=transport_->receive();
    for(const auto& event:transport_->events())if(event.peer==connection_){
        if(event.connected){connected_=true;nextSend_=now;}else disconnect(now,event.reason.empty()?"Directory unavailable":event.reason);
    }
    for(const auto& message:incoming)if(message.peer==connection_){
        try{Reader r(message.data);auto h=r.take<Header>();auto reply=r.take<AuthorityRegistered>();
            if(h.signature!=magic || h.kind!=Kind::AuthorityRegistered || !r.rest().empty() || reply.reserved || reply.lease!=registryLeaseSeconds)throw std::runtime_error("Invalid registration acknowledgement");
            lastAck_=now;error_.clear();
        }catch(const std::exception& e){disconnect(now,e.what());}
    }
    if(connection_ && ((!connected_ && now-started_>5*second) || (connected_ && now-(lastAck_?lastAck_:started_)>lease)))disconnect(now,"Directory acknowledgement timed out");
    if(!connection_ && now>=retry_){try{connection_=transport_->open(address_);started_=now;}catch(const std::exception& e){retry_=now+5*second;error_=e.what();}}
    if(connection_ && connected_ && now>=nextSend_){
        if(!transport_->send(connection_,record(Kind::RegisterAuthority,registration_),Lane::Control))disconnect(now,"Directory registration could not be sent");
        else nextSend_=now+10*second;
    }
}
}
