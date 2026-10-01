#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace Native {
struct LoadoutPart {
    int16_t id{};int8_t mount{};int16_t amount{32767};
    bool operator==(const LoadoutPart&)const=default;
};
struct Loadout {
    int16_t hull{-1};std::vector<LoadoutPart> parts;
    bool operator==(const Loadout&)const=default;
};
struct PurchaseResult {bool accepted{},complete{},launched{};};
// Original ten preset banks, each with a most-recent-first list of hulls.
// No IGC pointers survive a mission or static-core change.
class Loadouts {
public:
    static constexpr size_t count=10;
    std::array<std::vector<Loadout>,count> banks;
    void remember(size_t bank,Loadout loadout);
    const Loadout* findExact(size_t bank,int hull)const;
    const Loadout* find(size_t bank,int hull)const;
    static Loadouts load(const std::filesystem::path& path);
    void save(const std::filesystem::path& path)const;
};
}
