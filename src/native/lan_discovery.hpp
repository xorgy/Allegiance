#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Native::Net {
// Local discovery carries only a compatible server's endpoint. Names, rules,
// occupancy and admission still come from the existing encrypted GNS query.
class LanDiscovery {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    static constexpr uint16_t discoveryPort=27501;
    enum class Role { Browser,Server };
    struct Endpoint {std::string address;uint64_t instance{};};
    explicit LanDiscovery(Role role,uint16_t gamePort=0,uint16_t port=discoveryPort,bool loopbackOnly=false);
    ~LanDiscovery();
    std::vector<Endpoint> poll();
    bool available()const;
};
}
