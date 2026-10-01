#pragma once
#include "network_codec.hpp"
#include <unordered_map>

namespace Native::Net {
using SharedBytes = std::shared_ptr<const std::vector<std::byte>>;
struct ArenaRevision {
    struct Block { uint64_t revision{}; SharedBytes bytes; };
    uint32_t domain{};
    uint64_t revision{}, checkpoint{}, chainBase{};
    SharedBytes raw, delta, checkpointRaw, checkpointBytes;
    std::vector<Block> chain, history;
};
// Owned by the publication worker. Required world revisions are independent
// deltas shared by baseline cohort. Stalled peers jump to the latest state.
// Projectile membership uses shared chains.
// Neither is encoded again for a recipient's different receipt/admission state.
class ArenaEncoder {
    std::unordered_map<uint32_t,std::shared_ptr<const ArenaRevision>> current_;
    std::unordered_map<uint32_t,Tick> capturedTicks_;
    struct Partition { Tick tick{}; std::vector<std::shared_ptr<const ArenaRevision>> arenas; };
    std::unordered_map<uint32_t,Partition> partitions_;
    struct Repairs {uint64_t revision{};std::unordered_map<uint64_t,SharedBytes> bases;};
    std::unordered_map<uint32_t,Repairs> repairs_;
    uint64_t match_{};
    uint64_t encodes_{};
public:
    std::shared_ptr<const ArenaRevision> publish(uint32_t domain, Tick tick,
        uint64_t match, const Frame& capture);
    std::span<const std::shared_ptr<const ArenaRevision>> publishWorld(uint32_t domain,
        Tick tick, uint64_t match, const Frame& capture);
    SharedBytes stateUpdate(const ArenaRevision&,uint64_t sentRevision);
    uint64_t encodes() const { return encodes_; }
    size_t retainedBytes() const;
};
std::vector<std::byte> reportPacket(Frame personal, std::span<const ArenaRef>);
} // namespace Native::Net
