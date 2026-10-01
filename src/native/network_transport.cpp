#include "network_transport.hpp"
#include "hostname_transport.hpp"
#include "thread_affinity.hpp"
#include "network_records.hpp"
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#if __has_include(<steam/steamnetworkingsharedpayload.h>)
#include <steam/steamnetworkingsharedpayload.h>
#define ALLEGIANCE_GNS_SHARED_PAYLOAD 1
#endif
#include <unordered_map>
#include <mutex>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <utility>
namespace Native::Net {
namespace {
std::mutex initMutex;
unsigned users{};
class Sockets;
std::unordered_map<HSteamListenSocket, Sockets *> listeners;
std::unordered_map<HSteamNetConnection, Sockets *> connections;
class Sockets final : public Transport {
    ISteamNetworkingSockets *api{};
    HSteamListenSocket listener{};
    HSteamNetPollGroup group{};
    std::vector<ConnectionEvent> changes;
    struct Pressure {int64_t state{},world{};int stateBytes{},worldBytes{};};
    std::unordered_map<Connection,Pressure> pressure;
    std::mutex pressureMutex;
    static void changed(SteamNetConnectionStatusChangedCallback_t *event) {
        Sockets *owner = nullptr;
        if (auto i = connections.find(event->m_hConn); i != connections.end())
            owner = i->second;
        else if (auto l = listeners.find(event->m_info.m_hListenSocket); l != listeners.end()) {
            owner = l->second;
            connections[event->m_hConn] = owner;
        }
        if (owner)
            owner->change(*event);
    }
    void change(const SteamNetConnectionStatusChangedCallback_t &event) {
        auto peer = event.m_hConn;
        switch (event.m_info.m_eState) {
        case k_ESteamNetworkingConnectionState_Connecting:
            if (event.m_info.m_hListenSocket) {
                if (api->AcceptConnection(peer) != k_EResultOK) {
                    close(peer, "Connection rejected");
                    return;
                }
                api->SetConnectionPollGroup(peer, group);
            }
            break;
        case k_ESteamNetworkingConnectionState_Connected: {
            int priorities[]{0, 1, 4, 2, 3};
            uint16_t weights[]{1, 1, 1, 1, 1};
            api->ConfigureConnectionLanes(peer, 5, priorities, weights);
            changes.push_back({peer, true, {}});
            break;
        }
        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
            changes.push_back({peer, false, event.m_info.m_szEndDebug, event.m_info.m_eEndReason != 1001});
            api->CloseConnection(peer, 0, nullptr, false);
            connections.erase(peer);
            break;
        default:
            break;
        }
    }

  public:
    Sockets() {
        const Affinity::ScopedUnpin workerMask(true);
        std::lock_guard lock(initMutex);
        if (users++ == 0) {
            SteamDatagramErrMsg error;
            if (!GameNetworkingSockets_Init(nullptr, error)) {
                --users;
                throw std::runtime_error(error);
            }
        }
        api = SteamNetworkingSockets();
        // Let congestion control use a fast link for large visible battles.
        // A 2 MiB/s ceiling starves their fragmented state publications even
        // on loopback, leaving incomplete frames and older delta baselines.
        SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_SendRateMax, 32 * 1024 * 1024);
        // GNS defaults to a 512 KiB send queue. One admitted publication can
        // exceed that, so its later application fragments were being rejected.
        // These are allocation limits, not preallocated buffers; writable()
        // still admits only one state burst while its queue is nearly empty.
        SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_SendBufferSize, 4 * maxFrame);
        SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_RecvBufferSize, 4 * maxFrame);
        SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_RecvBufferMessages, 4 * maxFrame / fragmentBytes + 256);
        group = api->CreatePollGroup();
        if (!group) {
            if (--users == 0)
                GameNetworkingSockets_Kill();
            throw std::runtime_error("GNS poll group");
        }
    }
    ~Sockets() override {
        if (listener) {
            listeners.erase(listener);
            api->CloseListenSocket(listener);
        }
        std::vector<Connection> own;
        for (auto &[peer, owner] : connections)
            if (owner == this)
                own.push_back(peer);
        for (auto peer : own)
            close(peer, listener ? "Server shut down" : "Connection replaced", !listener);
        api->DestroyPollGroup(group);
        std::lock_guard lock(initMutex);
        if (--users == 0)
            GameNetworkingSockets_Kill();
    }
    void bind(uint16_t port) {
        SteamNetworkingIPAddr address;
        address.Clear();
        address.m_port = port;
        SteamNetworkingConfigValue_t callback;
        callback.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, reinterpret_cast<void *>(&changed));
        listener = api->CreateListenSocketIP(address, 1, &callback);
        if (!listener)
            throw std::runtime_error("Cannot listen on UDP port " + std::to_string(port));
        listeners[listener] = this;
    }
    Connection open(std::string_view destination) override {
        SteamNetworkingIPAddr address;
        address.Clear();
        if (!address.ParseString(std::string(destination).c_str()))
            throw std::runtime_error("Use a numeric address with port (for example 127.0.0.1:27500)");
        if (!address.m_port)
            address.m_port = 27500;
        SteamNetworkingConfigValue_t callback;
        callback.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, reinterpret_cast<void *>(&changed));
        auto peer = api->ConnectByIPAddress(address, 1, &callback);
        if (!peer)
            throw std::runtime_error("Cannot connect");
        connections[peer] = this;
        api->SetConnectionPollGroup(peer, group);
        return peer;
    }
    bool writable(Connection peer) override {
        SteamNetConnectionRealTimeLaneStatus_t lanes[5]{};
        if (api->GetConnectionRealTimeStatus(peer, nullptr, 5, lanes) != k_EResultOK)
            return false;
        const auto& state=lanes[int(Lane::State)];const auto& world=lanes[int(Lane::World)];
        // A rate estimate is not the service capacity available to this peer.
        // Admit a new pose only after the previous burst has substantially
        // drained; 50 ms at a nominal 32 MiB/s allowed megabytes of stale state.
        bool ready=state.m_usecQueueTime<50000 && world.m_cbPendingReliable<4096;
        if(!ready){std::lock_guard lock(pressureMutex);pressure[peer]={state.m_usecQueueTime,world.m_usecQueueTime,
            state.m_cbPendingUnreliable+state.m_cbPendingReliable,world.m_cbPendingReliable};}
        return ready;
    }
    std::string diagnostic(Connection peer) override {
        char text[8192];
        if(api->GetDetailedConnectionStatus(peer,text,sizeof(text))!=0)return {};
        std::lock_guard lock(pressureMutex);
        if(auto p=pressure.find(peer);p!=pressure.end()) {
            const auto& s=p->second;
            return "Last backpressure: state "+std::to_string(s.state)+" us/"+std::to_string(s.stateBytes)+
                " bytes, world "+std::to_string(s.world)+" us/"+std::to_string(s.worldBytes)+" bytes\n"+text;
        }
        return text;
    }
    ConnectionQuality quality(Connection peer) override {
        SteamNetConnectionRealTimeStatus_t status{};
        if(api->GetConnectionRealTimeStatus(peer,&status,0,nullptr)!=k_EResultOK)return {};
        return {std::max(0,status.m_nPing),std::isfinite(status.m_flConnectionQualityRemote) && status.m_flConnectionQualityRemote>=0?std::clamp(status.m_flConnectionQualityRemote,0.f,1.f):1.f};
    }
    std::string remoteHost(Connection peer) override {
        SteamNetConnectionInfo_t info{};if(!api->GetConnectionInfo(peer,&info))return {};
        char address[SteamNetworkingIPAddr::k_cchMaxString]{};info.m_addrRemote.ToString(address,sizeof(address),false);return address;
    }
    std::string remoteAddress(Connection peer) override {
        SteamNetConnectionInfo_t info{};if(!api->GetConnectionInfo(peer,&info))return {};
        char address[SteamNetworkingIPAddr::k_cchMaxString]{};info.m_addrRemote.ToString(address,sizeof(address),true);return address;
    }
    bool send(Connection peer, std::span<const std::byte> bytes, Lane lane) override {
        auto *message = SteamNetworkingUtils()->AllocateMessage(bytes.size());
        if (!message)
            return false;
        std::memcpy(message->m_pData, bytes.data(), bytes.size());
        message->m_cbSize = bytes.size();
        message->m_conn = peer;
        message->m_idxLane = int(lane);
        // Small input/status messages can expire before sending. Large state
        // messages span packets: dropping their queued fragments independently
        // wastes the entire publication. Publication admission is already gated
        // by writable(), so enqueue that bounded burst without retransmission.
        message->m_nFlags = lane == Lane::State     ? (bytes.size()>1024?k_nSteamNetworkingSend_UnreliableNoNagle:k_nSteamNetworkingSend_UnreliableNoDelay)
                            : lane == Lane::Control ? k_nSteamNetworkingSend_ReliableNoNagle
                                                    : k_nSteamNetworkingSend_Reliable;
        int64 result;
        api->SendMessages(1, &message, &result, true);
        return result > 0;
    }
    std::vector<bool> sendBatch(std::span<const OutgoingMessage> outgoing) override {
        std::vector<bool> accepted(outgoing.size());
        std::vector<SteamNetworkingMessage_t*> messages;messages.reserve(outgoing.size());
#ifdef ALLEGIANCE_GNS_SHARED_PAYLOAD
        struct ReleasePayload {void operator()(SteamNetworkingSharedPayload* p)const{if(p)p->Release();}};
        using Payload=std::unique_ptr<SteamNetworkingSharedPayload,ReleasePayload>;
        std::unordered_map<const void*,Payload> payloads;
#endif
        for(const auto& out:outgoing) {
            int flags=out.lane==Lane::State ? (out.discardIfQueued?k_nSteamNetworkingSend_UnreliableNoDelay:k_nSteamNetworkingSend_UnreliableNoNagle)
                :(out.lane==Lane::Control || out.lane==Lane::World || out.lane==Lane::Projectiles)?k_nSteamNetworkingSend_ReliableNoNagle:k_nSteamNetworkingSend_Reliable;
            SteamNetworkingMessage_t* message=nullptr;
#ifdef ALLEGIANCE_GNS_SHARED_PAYLOAD
            auto& payload=payloads[out.data.get()];
            if(!payload) {
                auto* ownership=new std::shared_ptr<const std::vector<std::byte>>(out.data);
                payload.reset(SteamNetworkingSharedPayload::Adopt(out.data->data(),out.data->size(),
                    [](void* p){delete static_cast<std::shared_ptr<const std::vector<std::byte>>*>(p);},ownership));
                if(!payload)delete ownership;
            }
            if(payload)message=payload->CreateMessage(SteamNetworkingUtils(),out.peer,flags,int(out.lane));
#else
            message=SteamNetworkingUtils()->AllocateMessage(out.data->size());
            if(message){std::memcpy(message->m_pData,out.data->data(),out.data->size());
                message->m_cbSize=out.data->size();message->m_conn=out.peer;message->m_idxLane=int(out.lane);message->m_nFlags=flags;}
#endif
            messages.push_back(message);
        }
        // Bound GNS's sorting scratch and amortize connection/service locking.
        constexpr size_t batchSize=4096;
        for(size_t begin=0;begin<messages.size();begin+=batchSize) {
            size_t count=std::min(batchSize,messages.size()-begin);std::vector<int64> results(count);
            api->SendMessages(count,messages.data()+begin,results.data(),true);
            for(size_t i=0;i<count;++i)accepted[begin+i]=results[i]>0;
        }
        return accepted;
    }
    void close(Connection peer, std::string_view reason, bool retryable = false) override {
        api->CloseConnection(peer, retryable ? 2001 : 1001, std::string(reason).c_str(), false);
        connections.erase(peer);
    }
    std::vector<Message> receive() override {
        api->RunCallbacks();
        std::vector<Message> out;
        SteamNetworkingMessage_t *messages[64];
        for (;;) {
            int count = api->ReceiveMessagesOnPollGroup(group, messages, 64);
            if (count <= 0)
                break;
            for (int i = 0; i < count; ++i) {
                auto *m = messages[i];
                auto *p = static_cast<const std::byte *>(m->m_pData);
                out.emplace_back(m->m_conn,std::span(p,size_t(m->m_cbSize)),m,
                    [](const void* owner){const_cast<SteamNetworkingMessage_t*>(static_cast<const SteamNetworkingMessage_t*>(owner))->Release();});
            }
            if (count < 64)
                break;
        }
        return out;
    }
    std::vector<ConnectionEvent> events() override {
        api->RunCallbacks();
        return std::exchange(changes, {});
    }
};
} // namespace
std::unique_ptr<Transport> listen(uint16_t port) {
    auto t = std::make_unique<Sockets>();
    t->bind(port);
    return t;
}
std::pair<std::unique_ptr<Transport>, Connection> connect(std::string_view address) {
    auto endpoint=parseEndpoint(address);
    std::unique_ptr<Transport> t=endpoint.numeric?std::make_unique<Sockets>():clientTransport();
    auto peer = t->open(endpoint.address());
    return {std::move(t), peer};
}
std::unique_ptr<Transport> clientTransport() {return hostnameTransport(std::make_unique<Sockets>());}
void impairment(float loss, int delay, int reorder, int rate) {
    auto *u = SteamNetworkingUtils();
    u->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketLoss_Send, loss);
    u->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketLag_Send, delay);
    u->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketReorder_Send, float(reorder));
    if (rate > 0) {
        u->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_SendRateMin, rate);
        u->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_SendRateMax, rate);
    }
}
} // namespace Native::Net
