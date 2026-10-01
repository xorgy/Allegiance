#include "hostname_transport.hpp"
#include <algorithm>
#include <chrono>
#include <unordered_map>

namespace Native::Net {
namespace {
int64_t now(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
class HostnameTransport final:public Transport {
    struct Peer {
        std::shared_ptr<EndpointResolution> resolution;
        std::vector<std::string> addresses;size_t next{};
        std::vector<Connection> attempts;Connection selected{};
        int64_t started{},lastAttempt{};std::string error;bool retryable{true};
    };
    std::unique_ptr<Transport> socket_;EndpointLookup lookup_;EndpointClock clock_;
    std::unordered_map<Connection,Peer> peers_;
    std::unordered_map<Connection,Connection> attempts_;
    std::vector<ConnectionEvent> changes_;Connection serial_{};bool received_{};
    Connection selected(Connection peer)const{auto entry=peers_.find(peer);return entry==peers_.end()?0:entry->second.selected;}
    void release(Peer& peer,std::string_view reason){
        if(peer.resolution)peer.resolution->cancel();
        for(auto attempt:peer.attempts){attempts_.erase(attempt);socket_->close(attempt,reason);}
    }
    void fail(Connection id,std::string reason,bool retryable){
        auto entry=peers_.find(id);if(entry==peers_.end())return;release(entry->second,reason);peers_.erase(entry);changes_.push_back({id,false,std::move(reason),retryable});
    }
    void start(Connection id,Peer& peer,int64_t time){
        while(peer.next<peer.addresses.size()){
            auto address=peer.addresses[peer.next++];peer.lastAttempt=time;
            try{auto connection=socket_->open(address);if(!connection)throw std::runtime_error("Cannot connect");attempts_.emplace(connection,id);peer.attempts.push_back(connection);return;}
            catch(const std::exception& error){peer.error=error.what();peer.retryable=true;}
        }
    }
    void advance(){
        auto time=clock_();std::vector<Connection> failed;
        for(auto& [id,peer]:peers_){
            if(peer.selected)continue;
            if(peer.resolution){
                if(auto result=peer.resolution->take()){
                    peer.resolution.reset();peer.addresses=std::move(result->addresses);peer.error=std::move(result->error);peer.retryable=result->retryable;
                    if(peer.addresses.empty() || !peer.error.empty()){failed.push_back(id);continue;}
                    if(peer.addresses.size()>8){peer.error="Too many resolved server addresses";peer.retryable=false;failed.push_back(id);continue;}
                    // OS ordering remains primary. Race later addresses at
                    // 250 ms intervals so a broken IPv6 route cannot stall IPv4.
                    start(id,peer,time);
                }else if(time-peer.started>=5000000){peer.error="Name resolution timed out";peer.retryable=true;failed.push_back(id);continue;}
            }else if(peer.next<peer.addresses.size() && (peer.attempts.empty() || time-peer.lastAttempt>=250000))start(id,peer,time);
            if(!peer.resolution && peer.attempts.empty() && peer.next==peer.addresses.size())failed.push_back(id);
        }
        for(auto id:failed){auto& peer=peers_.at(id);fail(id,peer.error.empty()?"Cannot connect":peer.error,peer.retryable);}
    }
    void collect(){
        for(auto event:socket_->events()){
            auto attempt=attempts_.find(event.peer);if(attempt==attempts_.end())continue;auto id=attempt->second;auto entry=peers_.find(id);if(entry==peers_.end())continue;auto& peer=entry->second;
            if(event.connected){
                if(peer.selected)continue;peer.selected=event.peer;
                for(auto other:peer.attempts)if(other!=event.peer){attempts_.erase(other);socket_->close(other,"Address race completed");}
                peer.attempts={event.peer};peer.addresses.clear();peer.resolution.reset();event.peer=id;changes_.push_back(std::move(event));
            }else{
                attempts_.erase(attempt);std::erase(peer.attempts,event.peer);
                if(peer.selected || (peer.next==peer.addresses.size() && peer.attempts.empty()))fail(id,std::move(event.reason),event.retryable);
                else{peer.error=std::move(event.reason);peer.retryable=event.retryable;}
            }
        }
    }
  public:
    HostnameTransport(std::unique_ptr<Transport> socket,EndpointLookup lookup,EndpointClock clock)
        :socket_(std::move(socket)),lookup_(std::move(lookup)),clock_(clock?std::move(clock):EndpointClock(now)){}
    ~HostnameTransport()override{for(auto& [_,peer]:peers_)release(peer,"Connection replaced");}
    Connection open(std::string_view address)override{
        auto endpoint=parseEndpoint(address);
        do{++serial_;}while(!serial_ || peers_.contains(serial_));auto id=serial_;Peer peer;peer.started=clock_();
        if(endpoint.numeric)peer.addresses.push_back(endpoint.address());else peer.resolution=lookup_(endpoint);
        auto entry=peers_.emplace(id,std::move(peer)).first;
        if(endpoint.numeric){start(id,entry->second,clock_());if(entry->second.attempts.empty()){auto error=entry->second.error;peers_.erase(entry);throw std::runtime_error(error);}}
        return id;
    }
    bool writable(Connection peer)override{auto actual=selected(peer);return actual && socket_->writable(actual);}
    bool send(Connection peer,std::span<const std::byte> bytes,Lane lane)override{auto actual=selected(peer);return actual && socket_->send(actual,bytes,lane);}
    std::vector<bool> sendBatch(std::span<const OutgoingMessage> outgoing)override{
        std::vector<bool> accepted(outgoing.size());std::vector<OutgoingMessage> translated;std::vector<size_t> indices;translated.reserve(outgoing.size());indices.reserve(outgoing.size());
        for(size_t i=0;i<outgoing.size();++i)if(auto actual=selected(outgoing[i].peer)){auto message=outgoing[i];message.peer=actual;translated.push_back(std::move(message));indices.push_back(i);}
        if(!translated.empty()){auto result=socket_->sendBatch(translated);for(size_t i=0;i<result.size();++i)accepted[indices[i]]=result[i];}return accepted;
    }
    std::string diagnostic(Connection peer)override{auto actual=selected(peer);return actual?socket_->diagnostic(actual):"Resolving or connecting";}
    ConnectionQuality quality(Connection peer)override{auto actual=selected(peer);return actual?socket_->quality(actual):ConnectionQuality{};}
    std::string remoteHost(Connection peer)override{auto actual=selected(peer);return actual?socket_->remoteHost(actual):std::string{};}
    std::string remoteAddress(Connection peer)override{auto actual=selected(peer);return actual?socket_->remoteAddress(actual):std::string{};}
    void close(Connection id,std::string_view reason,bool retryable=false)override{
        auto entry=peers_.find(id);if(entry==peers_.end())return;
        if(entry->second.resolution)entry->second.resolution->cancel();
        for(auto attempt:entry->second.attempts){attempts_.erase(attempt);socket_->close(attempt,reason,retryable);}peers_.erase(entry);
        std::erase_if(changes_,[&](const auto& event){return event.peer==id;});
    }
    std::vector<Message> receive()override{
        advance();auto incoming=socket_->receive();collect();size_t kept=0;
        for(size_t i=0;i<incoming.size();++i){auto& message=incoming[i];auto attempt=attempts_.find(message.peer);if(attempt==attempts_.end() || selected(attempt->second)!=message.peer)continue;
            message.peer=attempt->second;if(kept!=i)incoming[kept]=std::move(message);++kept;}
        incoming.resize(kept);received_=true;return incoming;
    }
    std::vector<ConnectionEvent> events()override{if(!received_){advance();collect();}received_=false;return std::exchange(changes_,{});}
};
}
std::unique_ptr<Transport> hostnameTransport(std::unique_ptr<Transport> socket,EndpointLookup lookup,EndpointClock clock){return std::make_unique<HostnameTransport>(std::move(socket),std::move(lookup),std::move(clock));}
}
