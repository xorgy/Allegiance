#include "network_arena.hpp"
namespace Native::Net {
namespace {
SharedBytes arenaPacket(uint32_t domain, Tick tick, uint64_t match,
                        std::vector<std::byte> payload) {
    std::vector<std::byte> bytes;
    bytes.reserve(sizeof(Header)+sizeof(ArenaRecord)+payload.size());
    Header header;header.kind=Kind::Arena;header.tick=tick;
    // Domain keys occupy 24 bits; the remaining 40 cover centuries at 60 Hz.
    header.sequence=(tick<<24)|domain;
    append(bytes,header);append(bytes,ArenaRecord{match,domain});
    append(bytes,std::span(payload));
    return std::make_shared<const std::vector<std::byte>>(std::move(bytes));
}
}
std::shared_ptr<const ArenaRevision> ArenaEncoder::publish(uint32_t domain,
        Tick tick,uint64_t match,const Frame& capture) {
    if(domain>=0x1000000 || !tick || tick>=uint64_t(1)<<40)
        throw std::runtime_error("Arena identity range");
    if(match_!=match){current_.clear();capturedTicks_.clear();partitions_.clear();repairs_.clear();match_=match;}
    auto& old=current_[domain];
    auto& capturedTick=capturedTicks_[domain];
    if(old && capturedTick==tick)return old;
    if(old && capturedTick>tick)throw std::runtime_error("Arena revision reversed");
    Frame world=capture;
    world.header.kind=Kind::Snapshot;world.header.tick=tick;world.header.sequence=tick;
    world.status.match=match;
    auto next=std::make_shared<ArenaRevision>();
    next->domain=domain;next->revision=tick;
    next->raw=std::make_shared<const std::vector<std::byte>>(packet(world));
    capturedTick=tick;
    if(old && old->raw->size()==next->raw->size() &&
       std::equal(next->raw->begin()+sizeof(Header),next->raw->end(),old->raw->begin()+sizeof(Header)))
        return old; // Reference unchanged state; no new encoding, delta or receipt.
    if(!(domain&0x800000)) {
        if(!old || tick-old->checkpoint>=3*tickRate) {
            next->checkpoint=tick;next->checkpointRaw=next->raw;
            next->checkpointBytes=arenaPacket(domain,tick,match,compressed(*next->raw,0,{}));
        }else {
            next->checkpoint=old->checkpoint;next->checkpointRaw=old->checkpointRaw;
            next->checkpointBytes=old->checkpointBytes;
        }
        if(old){
            next->delta=arenaPacket(domain,tick,match,compressed(*next->raw,old->revision,*old->raw));
            next->history=old->history;next->history.push_back({old->revision,old->raw});
            // Common repair baselines are shared, bounded independently of the
            // recipient count, and never consulted on the authority thread.
            while(next->history.size()>8)next->history.erase(next->history.begin());
        }else next->delta=next->checkpointBytes;
        next->chainBase=next->checkpoint;
    }else if(!old || tick-old->checkpoint>=3*tickRate) {
        next->checkpoint=tick;next->checkpointRaw=next->raw;
        next->checkpointBytes=arenaPacket(domain,tick,match,compressed(*next->raw,0,{}));
        if(old) {
            next->chainBase=old->revision;
            next->delta=arenaPacket(domain,tick,match,compressed(*next->raw,old->revision,*old->raw));
            next->chain.push_back({tick,next->delta});
        }else {next->chainBase=tick;next->delta=next->checkpointBytes;}
    }else {
        next->checkpoint=old->checkpoint;next->checkpointRaw=old->checkpointRaw;
        next->chainBase=old->chainBase;
        next->checkpointBytes=old->checkpointBytes;
        next->delta=arenaPacket(domain,tick,match,
            compressed(*next->raw,old->revision,*old->raw));
        next->chain=old->chain;
        next->chain.push_back({tick,next->delta});
    }
    ++encodes_;old=next;return next;
}
std::span<const std::shared_ptr<const ArenaRevision>> ArenaEncoder::publishWorld(
        uint32_t domain,Tick tick,uint64_t match,const Frame& capture) {
    if(match_!=match){current_.clear();capturedTicks_.clear();partitions_.clear();repairs_.clear();match_=match;}
    auto& partition=partitions_[domain];
    if(!partition.arenas.empty() && partition.tick==tick)return partition.arenas;
    if(!partition.arenas.empty() && partition.tick>tick)throw std::runtime_error("Partition revision reversed");
    partition.tick=tick;partition.arenas.clear();
    if(!(domain&0x800000)) {
        partition.arenas.push_back(publish(domain,tick,match,capture));
        return partition.arenas;
    }
    // Partition once per sector publication, independently of its recipients.
    // Stable identity buckets confine creation/removal churn and packet loss.
    if(domain>=0x804000 || !capture.entities.empty() || !capture.events.empty())
        throw std::runtime_error("Invalid projectile capture domain");
    std::array<Frame,projectileArenaBuckets> buckets;
    for(auto shot:capture.capturedProjectiles) {
        auto& bucket=buckets[shot.identity.serial%projectileArenaBuckets];
        shot.ordinal=uint32_t(bucket.capturedProjectiles.size());
        bucket.capturedProjectiles.push_back(shot);
    }
    for(uint32_t i=0;i<projectileArenaBuckets;++i)
        partition.arenas.push_back(publish(0x800000|((domain&0x3fff)<<8)|i,tick,match,buckets[i]));
    return partition.arenas;
}
SharedBytes ArenaEncoder::stateUpdate(const ArenaRevision& arena,uint64_t sentRevision) {
    if(arena.domain&0x800000)throw std::logic_error("Trajectory used as flight state");
    if(sentRevision>=arena.revision)return {};
    if(!arena.history.empty() && sentRevision==arena.history.back().revision)return arena.delta;
    auto previous=std::find_if(arena.history.begin(),arena.history.end(),[&](const auto& b){return b.revision==sentRevision;});
    uint64_t basis=previous==arena.history.end()?0:sentRevision;
    if(!basis && arena.checkpoint==arena.revision)return arena.checkpointBytes;
    auto& repairs=repairs_[arena.domain];
    if(repairs.revision!=arena.revision){repairs.bases.clear();repairs.revision=arena.revision;}
    auto& bytes=repairs.bases[basis];
    if(!bytes){
        bytes=arenaPacket(arena.domain,arena.revision,match_,compressed(*arena.raw,basis,
            basis?std::span(*previous->bytes):std::span<const std::byte>{}));
        ++encodes_;
    }
    return bytes;
}
size_t ArenaEncoder::retainedBytes()const {
    size_t bytes=0;
    for(const auto& [_,r]:current_){
        bytes+=r->raw->size();
        if(r->checkpointRaw!=r->raw)bytes+=r->checkpointRaw->size();
        bytes+=r->checkpointBytes->size();
        if(r->chain.empty() && r->delta!=r->checkpointBytes)bytes+=r->delta->size();
        for(const auto& h:r->history)if(h.bytes!=r->raw && h.bytes!=r->checkpointRaw)bytes+=h.bytes->size();
        for(const auto& b:r->chain)bytes+=b.bytes->size();
    }
    for(const auto& [_,cache]:repairs_)for(const auto& [__,repair]:cache.bases)bytes+=repair->size();
    return bytes;
}
std::vector<std::byte> reportPacket(Frame personal,std::span<const ArenaRef> references) {
    auto header=personal.header;header.kind=Kind::Report;
    auto bytes=packet(personal);
    auto payload=compressed(bytes,0,{});
    std::vector<std::byte> result;
    result.reserve(sizeof(Header)+sizeof(ArenaManifest)+references.size_bytes()+payload.size());
    append(result,header);append(result,ArenaManifest{uint32_t(references.size())});
    append(result,references);append(result,std::span(payload));
    return result;
}
} // namespace Native::Net
