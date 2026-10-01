#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Native::Net {
struct Endpoint {
    std::string host;uint16_t port{27500};bool numeric{};
    std::string address()const;
};
Endpoint parseEndpoint(std::string_view);
struct ResolvedEndpoints {
    std::vector<std::string> addresses;std::string error;bool retryable{};
};
// Workers own only this result, never a transport, client or callback target.
// Cancelling/closing an entry cannot wait for an uncancellable OS DNS lookup.
class EndpointResolution {
    std::atomic<bool> cancelled_{};
    std::mutex mutex_;std::optional<ResolvedEndpoints> result_;
  public:
    void cancel(){cancelled_.store(true,std::memory_order_relaxed);}
    bool cancelled()const{return cancelled_.load(std::memory_order_relaxed);}
    void complete(ResolvedEndpoints);
    std::optional<ResolvedEndpoints> take();
};
std::shared_ptr<EndpointResolution> resolveEndpoint(const Endpoint&);
}
