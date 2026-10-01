#pragma once
#include "network_records.hpp"
#include <map>
namespace Native::Net {
// Lossless compression only: native wire records and exact authority floats
// remain the state. No quantization, schema or dependency on floating math.
std::vector<std::byte> compactMessage(std::vector<std::byte>);
void unpackMessage(Header &, Reader &, std::vector<std::byte> &);
std::vector<std::byte> compressed(std::span<const std::byte>, uint64_t,
                                  std::span<const std::byte>);
std::vector<std::byte> expanded(std::span<const std::byte>,
                               const std::map<uint64_t, std::vector<std::byte>> &);
std::vector<std::byte> expandedAgainst(std::span<const std::byte>, std::span<const std::byte> baseline);
constexpr size_t maxDeltaData = 2 * maxFrame;
} // namespace Native::Net
