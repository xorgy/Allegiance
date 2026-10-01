#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include <stdexcept>
#include <utility>
namespace Native::Net {
using Connection = uint32_t;
enum class Lane { Control, State, Bulk, World, Projectiles };
struct OutgoingMessage {
    Connection peer{};
    std::shared_ptr<const std::vector<std::byte>> data;
    Lane lane{Lane::State};
    bool discardIfQueued{}; // Latest input/receipt samples, not report dependencies.
};
class Message {
    std::vector<std::byte> owned_;
    using Release = void (*)(const void*);
    std::unique_ptr<const void,Release> borrowed_{nullptr,nullptr};
  public:
    Connection peer{};
    std::span<const std::byte> data;
    Message()=default;
    Message(Connection connection,std::vector<std::byte> bytes)
        :owned_(std::move(bytes)),peer(connection),data(owned_){}
    // A received GNS message already owns its plaintext. Adopt that ownership
    // until consumption instead of allocating and copying it a second time.
    Message(Connection connection,std::span<const std::byte> bytes,const void* owner,Release release)
        :borrowed_(owner,release),peer(connection),data(bytes){}
    Message(const Message& other):Message(other.peer,std::vector<std::byte>(other.data.begin(),other.data.end())){}
    Message& operator=(const Message& other){if(this!=&other)*this=Message(other);return *this;}
    Message(Message&& other)noexcept
        :owned_(std::move(other.owned_)),borrowed_(std::move(other.borrowed_)),peer(other.peer),data(std::exchange(other.data,{})){}
    Message& operator=(Message&& other)noexcept {
        if(this!=&other){owned_=std::move(other.owned_);borrowed_=std::move(other.borrowed_);peer=other.peer;data=std::exchange(other.data,{});}
        return *this;
    }
};
struct ConnectionEvent {
    Connection peer{};
    bool connected{};
    std::string reason;
    bool retryable{};
};
// Game code depends on this boundary, never on a GNS connection or callback.
struct ConnectionQuality {int ping{};float outgoingDelivery{1};};
class Transport {
  public:
    virtual ~Transport() = default;
    virtual bool writable(Connection) = 0;
    virtual std::string diagnostic(Connection) {return {};}
    virtual ConnectionQuality quality(Connection) {return {};}
    // Numeric address observed by the transport; directory registration must
    // never trust a host name supplied in an application record.
    virtual std::string remoteHost(Connection) {return {};}
    virtual std::string remoteAddress(Connection) {return {};}
    virtual Connection open(std::string_view) {throw std::logic_error("Transport does not open connections");}
    virtual bool send(Connection, std::span<const std::byte>, Lane) = 0;
    virtual std::vector<bool> sendBatch(std::span<const OutgoingMessage> messages) {
        std::vector<bool> accepted;accepted.reserve(messages.size());
        for(const auto& m:messages)accepted.push_back(send(m.peer,*m.data,m.lane));
        return accepted;
    }
    virtual void close(Connection, std::string_view, bool retryable = false) = 0;
    virtual std::vector<Message> receive() = 0;
    virtual std::vector<ConnectionEvent> events() = 0;
};
std::unique_ptr<Transport> listen(uint16_t port);
std::unique_ptr<Transport> clientTransport();
std::pair<std::unique_ptr<Transport>, Connection> connect(std::string_view address);
void impairment(float loss, int delay, int reorder = 0, int rate = 0);
} // namespace Native::Net
