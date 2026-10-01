#pragma once
#include <algorithm>
#include <cstdint>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

namespace Native {
#ifdef _WIN32
inline uint64_t performanceFrequency() {
    static const uint64_t frequency=[] { LARGE_INTEGER value;QueryPerformanceFrequency(&value);return uint64_t(value.QuadPart); }();
    return frequency;
}
inline uint64_t performanceNs(uint64_t ticks) {
    auto frequency=performanceFrequency();
    return ticks/frequency*1'000'000'000 + ticks%frequency*1'000'000'000/frequency;
}
#endif
// Shared by display feedback and networking; absolute timestamps must use the
// same monotonic clock, independently of simulation time or server clock offset.
inline uint64_t monotonicNs() {
#ifdef _WIN32
    LARGE_INTEGER value;QueryPerformanceCounter(&value);return performanceNs(uint64_t(value.QuadPart));
#else
    timespec t{};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return uint64_t(t.tv_sec) * 1'000'000'000 + t.tv_nsec;
#endif
}
struct FrameTiming {
    uint64_t sampledNs{}, targetPresentNs{};
    // An unavailable or already missed display target falls back to sampling.
    // Bound presentation-only projection after stalls; never advance input ticks.
    uint64_t evaluatedNs() const {
        return sampledNs + (sampledNs && targetPresentNs > sampledNs
                                ? std::min(targetPresentNs - sampledNs, uint64_t(100'000'000))
                                : 0);
    }
    double leadSeconds() const { return double(evaluatedNs() - sampledNs) * 1e-9; }
};
} // namespace Native
