#pragma once
#include "network_records.hpp"
#include "network_transport.hpp"
#include "lan_discovery.hpp"
#include <optional>
#include <utility>
#include <map>

namespace Native::Net {
struct ServerListing {std::string address;ServerInfo info{};int64_t observedAt{};};
struct AuthorityListing {std::string address;DirectoryInfo info{};int ping{-1};std::vector<std::string> cores;int64_t observedAt{};
    bool supports(std::string_view file)const{return Allegiance::FindCore(cores,file)!=nullptr;}};
struct GameCreationResult {std::string address,error;std::array<uint64_t,2> token{};};
struct PlayerSearchResult {std::optional<ServerListing> game;};
std::vector<ServerListing> coalesceGames(std::vector<ServerListing>);
std::vector<AuthorityListing> coalesceAuthorities(std::vector<AuthorityListing>);
// One GNS poll group services all probes. Bound concurrency and refresh work;
// the browser is idle outside the entry screens and never runs game simulation.
class ServerBrowser {
    struct Endpoint {std::string address,error;std::vector<ServerListing> games;std::optional<AuthorityListing> authority;Connection connection{};int64_t started{},next{},expires{},directoryExpires{};uint64_t discovery{},search{};bool explicitAddress{};};
    Digest content_;
    std::unique_ptr<Transport> transport_;
    std::unique_ptr<LanDiscovery> discovery_;
    std::vector<Endpoint> endpoints_;
    struct RegistryCatalog {int64_t expires{};std::vector<std::string> addresses;};
    std::map<std::string,RegistryCatalog,std::less<>> registries_;
    uint64_t revision_{};
    uint64_t searchSerial_{},activeSearch_{};std::string playerName_;std::optional<PlayerSearchResult> searchResult_;
    struct Creation {std::string address;CreateGameRequest request{};Connection connection{};int64_t started{};};
    std::optional<Creation> creation_;std::optional<GameCreationResult> creationResult_;
    void finishCreation(GameCreationResult);
    void finish(Endpoint&,std::string error={});
  public:
    explicit ServerBrowser(Digest content):content_(content){}
    bool add(std::string_view address);
    void poll(bool discoverLan=false);
    void stop();
    void findPlayer(std::string_view name);
    bool createGame(std::string_view address,std::string_view name,std::string_view pilot,std::string_view core);
    std::optional<GameCreationResult> takeCreation(){return std::exchange(creationResult_,{});}
    std::vector<AuthorityListing> authorities()const;
    std::optional<PlayerSearchResult> takePlayerSearch(){return std::exchange(searchResult_,{});}
    uint64_t revision()const{return revision_;}
    std::vector<ServerListing> listings()const;
};
}
