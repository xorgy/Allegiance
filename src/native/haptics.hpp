#pragma once
#include <SDL3/SDL_haptic.h>
#include <filesystem>
#include <span>
#include <vector>
namespace Native {
struct HapticPreset {SDL_HapticEffect effect{};bool directional{};};
std::vector<HapticPreset> decodeForceFile(std::span<const unsigned char> data);
std::vector<HapticPreset> loadForceFile(const std::filesystem::path& path);
HapticPreset defaultForceEffect(int id);
}
