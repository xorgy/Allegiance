#pragma once
#include "network_session.hpp"
#include <chrono>

namespace Native::Net {
constexpr std::array<uint16_t,maxHostedGames> gameListenerPorts(uint16_t entry){
    std::array<uint16_t,maxHostedGames> ports{};if(!entry)return ports;
    ports[0]=entry;size_t slot=1;
    for(uint32_t candidate=uint32_t(entry)+1;candidate<=65535 && slot<ports.size();++candidate)
        if(candidate!=LanDiscovery::discoveryPort)ports[slot++]=uint16_t(candidate);
    return ports;
}
// Entry socket and its original mission stay stable. Each created mission has
// its own listener, simulation and publication domains, sharing the asset cache.
class ServerHost {
  public:
    using Factory=std::function<std::unique_ptr<Game>(const SessionOptions&)>;
  private:
    using Clock=std::chrono::steady_clock;
    struct Mission {
        std::unique_ptr<Game> game;std::unique_ptr<Server> server;
        uint16_t port{};Clock::time_point created{};bool joined{};
    };
    struct Creation {CreateGameRequest request;GameCreated result;Clock::time_point created,expires;};
    Server& entry_;Digest content_;std::array<uint16_t,maxHostedGames> ports_;uint32_t capacity_;Factory factory_;std::vector<std::string> cores_;
    std::vector<Mission> missions_;
    std::map<std::array<uint64_t,2>,Creation> creations_;
    std::vector<std::byte> directory(std::string_view);
    GameCreated create(const CreateGameRequest&);
  public:
    ServerHost(Server&,Digest,uint16_t,Factory,std::vector<std::string> cores={"static_core"});
    ~ServerHost();
    void poll(std::chrono::steady_clock::time_point now=std::chrono::steady_clock::now());
    void step();
    size_t games()const{return 1+missions_.size();}
};
}
