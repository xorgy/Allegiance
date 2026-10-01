// Copyright (c) 2026 Aaron. SPDX-License-Identifier: MIT
// Adapted from the author's 0 A.D. thread affinity code with permission.
#pragma once
#include <thread>
#include <utility>
#include <vector>
#ifdef __linux__
#include <sched.h>
#endif

namespace Native::Affinity {
enum class Lane { Simulation, Publication, Render };
struct Cpu {
    int id{}, ranking{};
    std::vector<int> siblings, cache;
};
struct Selection {
    int simulation=-1, publication=-1, render=-1;
    std::vector<int> workers;
};
// Pure topology selection, also used by the synthetic topology checks.
Selection select(const std::vector<Cpu>& allowed);
// Called before creating platform services; a disabled plan leaves masks alone.
void initialize();
bool pinCurrentThread(Lane lane);
bool restoreCurrentThread();

// Temporarily restore the pre-pin mask for thread creation, or use its worker
// subset for libraries whose internally created threads inherit our mask.
class ScopedUnpin {
public:
    explicit ScopedUnpin(bool workers=false);
    ~ScopedUnpin();
    void restore();
    ScopedUnpin(const ScopedUnpin&)=delete;
    ScopedUnpin& operator=(const ScopedUnpin&)=delete;
private:
#ifdef __linux__
    cpu_set_t previous_{};
#endif
    bool changed_{};
};
template<class Callable> std::thread createThread(Callable&& callable) {
    const ScopedUnpin unpin;
    return std::thread(std::forward<Callable>(callable));
}
} // namespace Native::Affinity
