#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include "lan_discovery.hpp"
#include "network_records.hpp"
#include "crypto.hpp"
#include <chrono>
#include <cstdio>

namespace Native::Net {
namespace {
constexpr uint32_t signature=0x4c414746; // FGAL, distinct from game records.
constexpr uint32_t localGroup=0xefff1b32; // 239.255.27.50, host-local fanout only.
enum : uint16_t {Query=1,Answer=2};
struct Probe {uint32_t signature{};uint16_t kind{},port{};Digest build{};uint64_t cookie{},instance{};};
static_assert(sizeof(Probe)==56 && std::has_unique_object_representations_v<Probe> && std::is_trivially_copyable_v<Probe>);
int64_t clockUs(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
#ifdef _WIN32
using Socket=SOCKET;constexpr auto invalid=INVALID_SOCKET;
void closeSocket(Socket socket){closesocket(socket);}
#else
using Socket=int;constexpr Socket invalid=-1;
void closeSocket(Socket socket){close(socket);}
#endif
}
struct LanDiscovery::Impl {
    Role role;Socket socket{invalid};Probe record{};int64_t nextPoll{},nextQuery{};bool loopbackOnly{};
    std::vector<sockaddr_in> destinations;
#ifdef _WIN32
    bool winsock{};
#endif
    void reset(){if(socket!=invalid){closeSocket(socket);socket=invalid;}}
    ~Impl(){reset();
#ifdef _WIN32
        if(winsock)WSACleanup();
#endif
    }
    explicit Impl(Role role,uint16_t gamePort,uint16_t port,bool loopbackOnly):role(role),loopbackOnly(loopbackOnly){
#ifdef _WIN32
        WSADATA data{};winsock=WSAStartup(MAKEWORD(2,2),&data)==0;if(!winsock)return;
#endif
        socket=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(socket==invalid)return;
        auto option=[&](int name,int value){return setsockopt(socket,SOL_SOCKET,name,reinterpret_cast<const char*>(&value),sizeof(value))==0;};
        if(role==Role::Server){
            if(!option(SO_REUSEADDR,1)){reset();return;}
#ifdef SO_REUSEPORT
            if(!option(SO_REUSEPORT,1)){reset();return;}
#endif
        }else if(!option(SO_BROADCAST,1)){reset();return;}
#ifdef _WIN32
        u_long nonblocking=1;if(ioctlsocket(socket,FIONBIO,&nonblocking)){reset();return;}
#else
        if(fcntl(socket,F_SETFL,O_NONBLOCK)<0){reset();return;}
#endif
        int ttl=1;if(setsockopt(socket,IPPROTO_IP,IP_TTL,reinterpret_cast<const char*>(&ttl),sizeof(ttl))){reset();return;}
        // BSD loopback has no broadcast fanout. Multicast on the loopback
        // interface reaches every local authority sharing the discovery port.
        // A zero multicast TTL prevents this query from leaving the host.
        in_addr loopback{};loopback.s_addr=htonl(INADDR_LOOPBACK);
        bool localFanout=false;
        if(role==Role::Server){
            ip_mreq membership{};membership.imr_multiaddr.s_addr=htonl(localGroup);membership.imr_interface=loopback;
            localFanout=setsockopt(socket,IPPROTO_IP,IP_ADD_MEMBERSHIP,reinterpret_cast<const char*>(&membership),sizeof(membership))==0;
        }else{
            int localTtl=0,localLoop=1;
            localFanout=setsockopt(socket,IPPROTO_IP,IP_MULTICAST_IF,reinterpret_cast<const char*>(&loopback),sizeof(loopback))==0
                && setsockopt(socket,IPPROTO_IP,IP_MULTICAST_TTL,reinterpret_cast<const char*>(&localTtl),sizeof(localTtl))==0
                && setsockopt(socket,IPPROTO_IP,IP_MULTICAST_LOOP,reinterpret_cast<const char*>(&localLoop),sizeof(localLoop))==0;
        }
        if(!localFanout)fprintf(stderr,"LAN host-local fanout unavailable; unicast and LAN broadcast remain usable.\n");
        sockaddr_in bindTo{};bindTo.sin_family=AF_INET;bindTo.sin_addr.s_addr=htonl(role==Role::Browser && loopbackOnly?INADDR_LOOPBACK:INADDR_ANY);bindTo.sin_port=htons(role==Role::Server?port:0);
        if(bind(socket,reinterpret_cast<const sockaddr*>(&bindTo),sizeof(bindTo))){reset();return;}
        record={signature,role==Role::Server?Answer:Query,role==Role::Server?gamePort:uint16_t(0),buildDigest(),0,0};
        auto& random=role==Role::Server?record.instance:record.cookie;
        Crypto::random({reinterpret_cast<uint8_t*>(&random),sizeof(random)});if(!random)random=1;
        if(role==Role::Server)return;
        auto add=[&](uint32_t address){
            if(std::any_of(destinations.begin(),destinations.end(),[&](const auto& d){return d.sin_addr.s_addr==address;}))return;
            sockaddr_in d{};d.sin_family=AF_INET;d.sin_addr.s_addr=address;d.sin_port=htons(port);destinations.push_back(d);
        };
        add(htonl(INADDR_LOOPBACK));if(localFanout)add(htonl(localGroup));if(loopbackOnly){add(htonl(0x7fffffff));return;}
        add(htonl(INADDR_BROADCAST));
#ifdef _WIN32
        ULONG bytes=0;constexpr ULONG flags=GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER;
        if(GetAdaptersAddresses(AF_INET,flags,nullptr,nullptr,&bytes)==ERROR_BUFFER_OVERFLOW && bytes<=1024*1024){
            std::vector<std::byte> storage(bytes);auto* adapters=reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data());
            if(GetAdaptersAddresses(AF_INET,flags,nullptr,adapters,&bytes)==NO_ERROR)for(auto* a=adapters;a;a=a->Next)if(a->OperStatus==IfOperStatusUp && a->IfType!=IF_TYPE_SOFTWARE_LOOPBACK)
                for(auto* u=a->FirstUnicastAddress;u;u=u->Next)if(u->Address.lpSockaddr->sa_family==AF_INET && u->OnLinkPrefixLength<31){
                    auto ip=reinterpret_cast<sockaddr_in*>(u->Address.lpSockaddr)->sin_addr.s_addr;
                    uint32_t mask=u->OnLinkPrefixLength?UINT32_MAX<<(32-u->OnLinkPrefixLength):0;add(ip|htonl(~mask));
                }
        }
#else
        ifaddrs* interfaces=nullptr;
        if(!getifaddrs(&interfaces)){
            for(auto* i=interfaces;i;i=i->ifa_next)if(i->ifa_addr && i->ifa_broadaddr && i->ifa_addr->sa_family==AF_INET && (i->ifa_flags&(IFF_UP|IFF_BROADCAST))==(IFF_UP|IFF_BROADCAST))
                add(reinterpret_cast<sockaddr_in*>(i->ifa_broadaddr)->sin_addr.s_addr);
            freeifaddrs(interfaces);
        }
#endif
    }
    std::vector<Endpoint> poll(){
        std::vector<Endpoint> endpoints;if(socket==invalid)return endpoints;
        const auto now=clockUs();if(now<nextPoll)return endpoints;nextPoll=now+100000;
        if(role==Role::Browser && now>=nextQuery){
            nextQuery=now+1000000;
            for(const auto& d:destinations)sendto(socket,reinterpret_cast<const char*>(&record),sizeof(record),0,reinterpret_cast<const sockaddr*>(&d),sizeof(d));
        }
        // Bounded work even on a noisy LAN; discovery never blocks simulation.
        for(int count=0;count<32;++count){
            std::array<char,128> bytes{};sockaddr_in source{};
#ifdef _WIN32
            int size=sizeof(source);
#else
            socklen_t size=sizeof(source);
#endif
            auto length=recvfrom(socket,bytes.data(),bytes.size(),0,reinterpret_cast<sockaddr*>(&source),&size);if(length<0)break;
            if(length!=sizeof(Probe) || source.sin_family!=AF_INET)continue;
            if(loopbackOnly && (ntohl(source.sin_addr.s_addr)>>24)!=127)continue;
            Probe p;std::memcpy(&p,bytes.data(),sizeof(p));if(p.signature!=signature || p.build!=record.build || !p.cookie)continue;
            if(role==Role::Server){
                if(p.kind!=Query || p.port || p.instance)continue;
                auto response=record;response.cookie=p.cookie;
                sendto(socket,reinterpret_cast<const char*>(&response),sizeof(response),0,reinterpret_cast<const sockaddr*>(&source),size);
            }else if(p.kind==Answer && p.port && p.instance && p.cookie==record.cookie){
                char ip[INET_ADDRSTRLEN];if(!inet_ntop(AF_INET,&source.sin_addr,ip,sizeof(ip)))continue;
                auto endpoint=std::string(ip)+":"+std::to_string(p.port);
                auto known=std::find_if(endpoints.begin(),endpoints.end(),[&](const auto& e){return e.instance==p.instance;});
                if(known==endpoints.end())endpoints.push_back({std::move(endpoint),p.instance});
                else if((ntohl(source.sin_addr.s_addr)>>24)==127)known->address=std::move(endpoint);
            }
        }
        return endpoints;
    }
};
LanDiscovery::LanDiscovery(Role role,uint16_t gamePort,uint16_t port,bool loopbackOnly):impl_(std::make_unique<Impl>(role,gamePort,port,loopbackOnly)){
    if(!available())fprintf(stderr,"LAN discovery socket unavailable; explicit endpoints remain usable.\n");
}
LanDiscovery::~LanDiscovery()=default;
bool LanDiscovery::available()const{return impl_->socket!=invalid;}
std::vector<LanDiscovery::Endpoint> LanDiscovery::poll(){return impl_->poll();}
}
