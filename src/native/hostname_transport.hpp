#pragma once
#include "endpoint_resolver.hpp"
#include "network_transport.hpp"
#include <functional>
namespace Native::Net {
using EndpointLookup=std::function<std::shared_ptr<EndpointResolution>(const Endpoint&)>;
using EndpointClock=std::function<int64_t()>;
// Client-side connection IDs remain stable through resolution and raced address
// attempts. Numeric direct joins and all server traffic use raw GNS unchanged.
std::unique_ptr<Transport> hostnameTransport(std::unique_ptr<Transport>,EndpointLookup=resolveEndpoint,EndpointClock={});
}
