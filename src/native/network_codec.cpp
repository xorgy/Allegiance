#include "network_codec.hpp"
#include <lz4.h>
namespace Native::Net {
namespace {
// Publication indices are built and discarded together. A flat table avoids
// allocating/freeing a node for every projectile on every delta; iteration
// order never participates in the wire format, and duplicate keys keep the
// original first-record semantics.
template <class Value> class RecordIndex {
    struct Slot {uint64_t key{};Value value{};bool used{};};
    std::vector<Slot> slots_;
    static uint64_t hash(uint64_t x) {
        x^=x>>30;x*=0xbf58476d1ce4e5b9ull;
        x^=x>>27;x*=0x94d049bb133111ebull;
        return x^(x>>31);
    }
public:
    explicit RecordIndex(size_t count) {
        size_t capacity=2;while(capacity<count*2)capacity*=2;
        slots_.resize(capacity);
    }
    void emplace(uint64_t key,const Value& value) {
        size_t at=hash(key)&(slots_.size()-1);
        while(slots_[at].used) {
            if(slots_[at].key==key)return;
            at=(at+1)&(slots_.size()-1);
        }
        slots_[at]={key,value,true};
    }
    const Value* find(uint64_t key)const {
        size_t at=hash(key)&(slots_.size()-1);
        while(slots_[at].used) {
            if(slots_[at].key==key)return &slots_[at].value;
            at=(at+1)&(slots_.size()-1);
        }
        return nullptr;
    }
};
template <Record T> T at(std::span<const std::byte> bytes, size_t offset) {
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
        throw std::runtime_error("Delta record bounds");
    T record;
    std::memcpy(&record, bytes.data() + offset, sizeof(record));
    return record;
}
bool packable(Kind kind, size_t size) {
    switch (kind) {
    case Kind::Action: return size == sizeof(Action);
    case Kind::Configure: return size == sizeof(Configuration);
    case Kind::Welcome: return size == sizeof(Welcome);
    default: return false;
    }
}
struct Layout {
    Snapshot status;
    size_t entities, parts, pilots, members, teams, buckets, notices, events, highlights, resultPlayers, resultTeams, shipIntel, payload;
    explicit Layout(std::span<const std::byte> bytes) : status(at<Snapshot>(bytes, sizeof(Header))) {
        size_t offset = sizeof(Header) + sizeof(Snapshot);
        auto array = [&](size_t count, size_t size) {
            if (count > maxEntities || offset > bytes.size() || count > (bytes.size() - offset) / size)
                throw std::runtime_error("Delta array bounds");
            auto begin = offset;
            offset += count * size;
            return begin;
        };
        entities = array(status.entities, sizeof(Entity));
        parts = array(status.parts, sizeof(Part));
        pilots = array(status.pilots, sizeof(Pilot));
        members = array(status.members, sizeof(Member));
        teams = array(status.teams, sizeof(Team));
        buckets = array(status.buckets, sizeof(Bucket));
        notices = array(status.notices, sizeof(Notice));
        events = array(status.events, sizeof(Event));
        highlights = array(status.highlights,sizeof(SectorHighlight));
        resultPlayers=array(status.resultPlayers,sizeof(ResultPlayer));resultTeams=array(status.resultTeams,sizeof(ResultTeam));
        shipIntel=array(status.shipIntel,sizeof(ShipIntel));
        payload = offset;
        if (bytes.size() - offset != status.payloadBytes)
            throw std::runtime_error("Delta payload bounds");
    }
};
uint64_t key(Identity id) {
    return (uint64_t(id.serial) << 32) | (uint32_t(uint16_t(id.type)) << 16) | uint16_t(id.id);
}
std::vector<CopyRange> relocations(std::span<const std::byte> current, std::span<const std::byte> previous) {
    Layout c(current), p(previous);
    std::vector<CopyRange> copies;
    auto copy = [&](size_t source, size_t destination, size_t size) {
        if (!size || source == destination)
            return;
        if (source > previous.size() || size > previous.size() - source ||
            destination > current.size() || size > current.size() - destination)
            throw std::runtime_error("Delta copy bounds");
        if (!copies.empty()) {
            auto &last = copies.back();
            if (last.source + last.size == source && last.destination + last.size == destination) {
                last.size += size;
                return;
            }
        }
        copies.push_back({uint32_t(source), uint32_t(destination), uint32_t(size)});
    };
    bool sameObjects = c.status.entities == p.status.entities && c.status.payloadBytes == p.status.payloadBytes;
    if (sameObjects)
        for (size_t i = 0; i < c.status.entities && sameObjects; ++i) {
            auto a = at<Entity>(current, c.entities + i * sizeof(Entity));
            auto b = at<Entity>(previous, p.entities + i * sizeof(Entity));
            sameObjects = a.identity == b.identity && a.payloadBytes == b.payloadBytes;
        }
    if (sameObjects) {
        copy(p.entities, c.entities, c.status.entities * sizeof(Entity));
        copy(p.payload, c.payload, c.status.payloadBytes);
    } else {
        struct Object {size_t record, payload, bytes;};
        RecordIndex<Object> objects(p.status.entities);
        size_t offset = p.payload;
        for (size_t i = 0; i < p.status.entities; ++i) {
            size_t record = p.entities + i * sizeof(Entity);
            auto e = at<Entity>(previous, record);
            if (offset > previous.size() || e.payloadBytes > previous.size() - offset)
                throw std::runtime_error("Delta object payload bounds");
            objects.emplace(key(e.identity), Object{record, offset, e.payloadBytes});
            offset += e.payloadBytes;
        }
        if (offset != previous.size())
            throw std::runtime_error("Delta object payload size");
        offset = c.payload;
        for (size_t i = 0; i < c.status.entities; ++i) {
            size_t record = c.entities + i * sizeof(Entity);
            auto e = at<Entity>(current, record);
            if (offset > current.size() || e.payloadBytes > current.size() - offset)
                throw std::runtime_error("Delta object payload bounds");
            if (auto* found = objects.find(key(e.identity))) {
                copy(found->record, record, sizeof(Entity));
                if (found->bytes == e.payloadBytes)
                    copy(found->payload, offset, e.payloadBytes);
            }
            offset += e.payloadBytes;
        }
        if (offset != current.size())
            throw std::runtime_error("Delta object payload size");
    }
    auto array = [&]<Record T>(size_t begin, size_t count, size_t oldBegin, size_t oldCount, auto identify) {
        if (!count || !oldCount)
            return;
        // Stable arrays need no index or relocation allocation.
        bool same = count == oldCount;
        if (same)
            for (size_t i = 0; i < count && same; ++i)
                same = identify(at<T>(current, begin + i * sizeof(T))) ==
                       identify(at<T>(previous, oldBegin + i * sizeof(T)));
        if (same) {
            copy(oldBegin, begin, count * sizeof(T));
            return;
        }
        RecordIndex<size_t> index(oldCount);
        for (size_t i = 0; i < oldCount; ++i)
            index.emplace(identify(at<T>(previous, oldBegin + i * sizeof(T))), oldBegin + i * sizeof(T));
        for (size_t i = 0; i < count; ++i) {
            size_t record = begin + i * sizeof(T);
            if (auto* found = index.find(identify(at<T>(current, record))))
                copy(*found, record, sizeof(T));
        }
    };
    array.template operator()<Part>(c.parts, c.status.parts, p.parts, p.status.parts, [](const Part &v) {
        return (uint64_t(v.ship.serial) << 32) | (uint32_t(uint16_t(v.mount)) << 16) | uint16_t(v.id);
    });
    array.template operator()<Pilot>(c.pilots, c.status.pilots, p.pilots, p.status.pilots,
                                    [](const Pilot &v) { return uint64_t(uint16_t(v.id)); });
    array.template operator()<Member>(c.members,c.status.members,p.members,p.status.members,
                                      [](const Member& v){return uint64_t(uint16_t(v.id));});
    array.template operator()<ShipIntel>(c.shipIntel,c.status.shipIntel,p.shipIntel,p.status.shipIntel,
                                        [](const ShipIntel& v){return (uint64_t(v.serial)<<16)|uint16_t(v.id);});
    array.template operator()<ResultPlayer>(c.resultPlayers,c.status.resultPlayers,p.resultPlayers,p.status.resultPlayers,[](const ResultPlayer& v){return uint64_t(uint16_t(v.id));});
    array.template operator()<ResultTeam>(c.resultTeams,c.status.resultTeams,p.resultTeams,p.status.resultTeams,[](const ResultTeam& v){return uint64_t(uint16_t(v.id));});
    array.template operator()<Team>(c.teams, c.status.teams, p.teams, p.status.teams,
                                   [](const Team &v) { return uint64_t(uint16_t(v.id)); });
    array.template operator()<Bucket>(c.buckets, c.status.buckets, p.buckets, p.status.buckets,
                                     [](const Bucket &v) { return uint64_t(uint16_t(v.side)) << 16 | uint16_t(v.id); });
    array.template operator()<Notice>(c.notices, c.status.notices, p.notices, p.status.notices,
                                     [](const Notice &v) { return v.number; });
    array.template operator()<Event>(c.events, c.status.events, p.events, p.status.events,
                                    [](const Event &v) { return v.number; });
    array.template operator()<SectorHighlight>(c.highlights,c.status.highlights,p.highlights,p.status.highlights,[](const SectorHighlight& v){return uint64_t(uint32_t(v.sector));});
    std::sort(copies.begin(), copies.end(), [](auto a, auto b) { return a.destination < b.destination; });
    size_t used = 0;
    for (auto c : copies) {
        if (used && copies[used - 1].source + copies[used - 1].size == c.source &&
            copies[used - 1].destination + copies[used - 1].size == c.destination)
            copies[used - 1].size += c.size;
        else
            copies[used++] = c;
    }
    copies.resize(used);
    return copies;
}
} // namespace
std::vector<std::byte> compactMessage(std::vector<std::byte> bytes) {
    auto h = at<Header>(bytes, 0);
    size_t size = bytes.size() - sizeof(Header);
    if (!packable(h.kind, size))
        return bytes;
    std::vector<std::byte> out(sizeof(Header) + sizeof(Packed) + LZ4_compressBound(size));
    int n = LZ4_compress_default(reinterpret_cast<const char *>(bytes.data() + sizeof(Header)),
                                reinterpret_cast<char *>(out.data() + sizeof(Header) + sizeof(Packed)), size,
                                out.size() - sizeof(Header) - sizeof(Packed));
    // Avoid paying another envelope for incompressible controls/chat.
    if (n <= 0 || sizeof(Packed) + n + 8 >= size)
        return bytes;
    Packed packed{h.kind, uint32_t(size)};
    h.kind = Kind::Packed;
    std::memcpy(out.data(), &h, sizeof(h));
    std::memcpy(out.data() + sizeof(h), &packed, sizeof(packed));
    out.resize(sizeof(h) + sizeof(packed) + n);
    return out;
}
void unpackMessage(Header &h, Reader &r, std::vector<std::byte> &storage) {
    if (h.kind != Kind::Packed)
        return;
    auto p = r.take<Packed>();
    if (!packable(p.kind, p.size))
        throw std::runtime_error("Invalid packed record layout");
    storage.resize(p.size);
    if (LZ4_decompress_safe(reinterpret_cast<const char *>(r.rest().data()),
                            reinterpret_cast<char *>(storage.data()), r.rest().size(), storage.size()) != int(p.size))
        throw std::runtime_error("Invalid packed record compression");
    h.kind = p.kind;
    r = Reader(storage);
}
std::vector<std::byte> compressed(std::span<const std::byte> full, uint64_t baseline,
                                  std::span<const std::byte> previous) {
    std::vector<CopyRange> copies;
    if (baseline)
        copies = relocations(full, previous);
    // Pathological churn can always use the ordinary positional XOR basis.
    if (copies.size() * sizeof(CopyRange) > full.size())
        copies.clear();
    std::vector<std::byte> aligned;
    std::span<const std::byte> basis = previous;
    if (!copies.empty() || (!previous.empty() && previous.size() != full.size())) {
        aligned.resize(full.size());
        std::memcpy(aligned.data(), previous.data(), std::min(full.size(), previous.size()));
        for (auto c : copies)
            std::memcpy(aligned.data() + c.destination, previous.data() + c.source, c.size);
        basis = aligned;
    }
    std::vector<std::byte> xorBytes(full.begin(), full.end());
    if (!basis.empty())
        for (size_t i = 0; i < full.size(); ++i)
            xorBytes[i] ^= basis[i];
    std::vector<std::byte> patch;
    if (baseline) {
        // A dense zero block still costs about one LZ4 length byte per 255
        // bytes. Send only changed word runs, so unchanged positions, names,
        // parts and creation data occupy no network space even in huge worlds.
        auto changed = [&](size_t offset) {
            uint64_t word = 0;
            std::memcpy(&word, xorBytes.data() + offset, std::min(sizeof(word), xorBytes.size() - offset));
            return word != 0;
        };
        size_t begin = 0;
        while (begin < xorBytes.size()) {
            if (!changed(begin)) {
                begin += std::min(size_t(8), xorBytes.size() - begin);
                continue;
            }
            size_t end = std::min(begin + 8, xorBytes.size()), scan = end;
            while (scan < xorBytes.size() && scan - end <= 16) {
                if (changed(scan))
                    end = std::min(scan + 8, xorBytes.size());
                scan += std::min(size_t(8), xorBytes.size() - scan);
            }
            append(patch, XorRange{uint32_t(begin), uint32_t(end - begin)});
            append(patch, std::span(xorBytes).subspan(begin, end - begin));
            begin = scan;
        }
        if (patch.size() >= xorBytes.size())
            patch.clear();
    }
    size_t prefix = copies.size() * sizeof(CopyRange);
    std::vector<std::byte> difference;
    size_t patchSize = patch.size();
    if (copies.empty())
        difference = patch.empty() ? std::move(xorBytes) : std::move(patch);
    else {
        difference.reserve(prefix + (patch.empty() ? xorBytes.size() : patch.size()));
        append(difference, std::span(copies));
        append(difference, patch.empty() ? std::span(xorBytes) : std::span(patch));
    }
    std::vector<std::byte> out;
    auto h = at<Header>(full, 0);
    h.kind = Kind::Delta;
    append(out, h);
    append(out, Delta{baseline, full.size(), uint32_t(copies.size()), uint32_t(patchSize)});
    auto offset = out.size();
    out.resize(offset + LZ4_compressBound(difference.size()));
    int n = LZ4_compress_default(reinterpret_cast<const char *>(difference.data()),
                                reinterpret_cast<char *>(out.data() + offset), difference.size(), out.size() - offset);
    if (n <= 0)
        throw std::runtime_error("Snapshot compression");
    out.resize(offset + n);
    return out;
}
std::vector<std::byte> expanded(std::span<const std::byte> compressed,
                               const std::map<uint64_t, std::vector<std::byte>> &history) {
    Reader reader(compressed);reader.take<Header>();auto delta=reader.take<Delta>();
    auto basis=history.find(delta.baseline);
    if(delta.baseline && basis==history.end())return {};
    return expandedAgainst(compressed,delta.baseline?std::span(basis->second):std::span<const std::byte>{});
}
std::vector<std::byte> expandedAgainst(std::span<const std::byte> compressed,
                                     std::span<const std::byte> baseline) {
    Reader r(compressed);
    r.take<Header>();
    auto d = r.take<Delta>();
    if (d.size > maxFrame || d.size < sizeof(Header) + sizeof(Snapshot) || d.patchBytes > d.size ||
        d.copies > d.size / sizeof(CopyRange) || (!d.baseline && (d.copies || d.patchBytes)))
        throw std::runtime_error("Snapshot expansion limit");
    if(bool(d.baseline)!=!baseline.empty())throw std::runtime_error("Invalid snapshot basis");
    size_t prefix = d.copies * sizeof(CopyRange);
    std::vector<std::byte> difference(prefix + (d.patchBytes ? d.patchBytes : d.size)), full(d.size);
    if (LZ4_decompress_safe(reinterpret_cast<const char *>(r.rest().data()),
                            reinterpret_cast<char *>(difference.data()), r.rest().size(), difference.size()) !=
        int(difference.size()))
        throw std::runtime_error("Invalid snapshot compression");
    if (!baseline.empty())
        std::memcpy(full.data(), baseline.data(), std::min(full.size(), baseline.size()));
    for (size_t i = 0; i < d.copies; ++i) {
        auto c = at<CopyRange>(difference, i * sizeof(CopyRange));
        if (!c.size || c.source > baseline.size() || c.size > baseline.size() - c.source ||
            c.destination > full.size() || c.size > full.size() - c.destination)
            throw std::runtime_error("Invalid snapshot relocation");
        std::memcpy(full.data() + c.destination, baseline.data() + c.source, c.size);
    }
    if (!d.patchBytes)
        for (size_t i = 0; i < full.size(); ++i)
            full[i] ^= difference[prefix + i];
    else {
        size_t offset = prefix, last = 0;
        while (offset < difference.size()) {
            auto x = at<XorRange>(difference, offset);
            offset += sizeof(x);
            if (!x.size || x.offset < last || x.offset > full.size() || x.size > full.size() - x.offset ||
                x.size > difference.size() - offset)
                throw std::runtime_error("Invalid snapshot XOR range");
            for (size_t i = 0; i < x.size; ++i)
                full[x.offset + i] ^= difference[offset + i];
            offset += x.size;
            last = x.offset + x.size;
        }
    }
    return full;
}
} // namespace Native::Net
