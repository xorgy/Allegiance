#pragma once
#include <bit>
#include <cstdint>
#include <unordered_map>

namespace Allegiance {
// PlayerInfo starts foreign accounts at zero. MONEY_CHANGE exposes only the
// signed donations involving the viewer's faction, not foreign paydays or
// actual balances. Unsigned arithmetic preserves the original 32-bit records.
struct ObservedCreditTotal {
    uint32_t identity{}, amount{};
    void add(uint32_t generation,int32_t delta) {
        if(identity!=generation){identity=generation;amount=0;}
        amount+=uint32_t(delta);
    }
};
inline int32_t CreditDifference(uint32_t total,uint32_t baseline=0) {
    return std::bit_cast<int32_t>(total-baseline);
}
// A cumulative faction total survives skipped snapshots without keeping an
// event history. Each client subtracts the first total it actually received;
// a newcomer must not inherit donations made before joining. Retain baselines
// across reconnect/alliance changes, and retire them with public membership.
class CreditObservations {
    struct Entry {uint32_t identity{}, baseline{};int owner{-1};int32_t money{};};
    int audience_{-2};
    uint32_t audienceIdentity_{};
    std::unordered_map<int,Entry> entries_;
public:
    void audience(int side,uint32_t identity=0) {if(audience_!=side || audienceIdentity_!=identity){audience_=side;audienceIdentity_=identity;entries_.clear();}}
    int32_t receive(int id,int owner,uint32_t identity,uint32_t total) {
        auto [it,added]=entries_.try_emplace(id,Entry{identity,total,owner,0});auto& entry=it->second;
        if(!added && (entry.identity!=identity || entry.owner!=owner))entry={identity,total,owner,0};
        entry.money=CreditDifference(total,entry.baseline);return entry.money;
    }
    int32_t money(int id,int owner)const {
        auto found=entries_.find(id);return found==entries_.end() || found->second.owner!=owner?0:found->second.money;
    }
    template<class Present>void retain(Present present){std::erase_if(entries_,[&](const auto& entry){return !present(entry.first);});}
};
}
