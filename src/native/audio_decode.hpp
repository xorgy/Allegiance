#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>
namespace Native {
struct DecodedAudio {std::vector<int16_t> samples;int channels{},rate{};};
DecodedAudio decodeAudio(const std::filesystem::path& path,bool mono=false);
}
