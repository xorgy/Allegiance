#pragma once
#include "network_records.hpp"
#include "network_transport.hpp"
#include <map>

namespace Native::Net {
int64_t directoryClock();
// A bounded table owns only IDs and endpoints. Heartbeats refresh leases without
// re-encoding a shared reply; membership changes produce one immutable buffer.
class RegistryTable {
    struct Lease {std::string address;int64_t expires{};};
    std::map<Connection,Lease> leases_;
    std::map<std::string,size_t,std::less<>> owners_;
    uint64_t revision_{};
    std::shared_ptr<const std::vector<std::byte>> encoded_;
  public:
    void publish(Connection,std::string,int64_t now);
    void remove(Connection);
    void expire(int64_t now);
    size_t size()const{return owners_.size();}
    size_t publishers()const{return leases_.size();}
    std::shared_ptr<const std::vector<std::byte>> report();
};
class PublicDirectory {
    struct Peer {int64_t expires{},nextRegistration{};bool browsing{};};
    struct Probe {Connection publisher{};std::string address;AuthorityRegistration registration{};int64_t expires{};};
    std::unique_ptr<Transport> transport_;
    RegistryTable table_;
    std::map<Connection,Peer> peers_;
    std::map<Connection,Probe> probes_;
    void retire(Connection,std::string_view);
    void finishProbe(Connection,bool,int64_t now);
  public:
    static constexpr uint16_t defaultPort=27599;
    explicit PublicDirectory(std::unique_ptr<Transport> transport):transport_(std::move(transport)){}
    explicit PublicDirectory(uint16_t port=defaultPort):PublicDirectory(listen(port)){}
    void poll(int64_t now=directoryClock());
    size_t authorities()const{return table_.size();}
};
// Registration is optional and does not participate in simulation or reports.
// Network/directory outages retry independently of the game listener and pilots.
class DirectoryRegistration {
    std::string address_;
    std::string error_;
    AuthorityRegistration registration_;
    std::unique_ptr<Transport> transport_;
    Connection connection_{};
    int64_t nextPoll_{},started_{},nextSend_{},lastAck_{},retry_{};
    bool connected_{};
    void disconnect(int64_t now,std::string_view reason);
  public:
    DirectoryRegistration(std::string address,uint16_t port,Digest content,std::unique_ptr<Transport> transport={});
    void poll(int64_t now=directoryClock());
    bool registered()const{return connected_ && lastAck_!=0;}
    const std::string& error()const{return error_;}
    const std::string& address()const{return address_;}
};
}
