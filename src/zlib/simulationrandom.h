// Native simulation's Windows CRT sequence. Presentation owns separate RNGs.
// A scope selects the mission stream without changing the process libc RNG.
#pragma once
#include <cstdint>

namespace Allegiance {
inline thread_local uint32_t fallbackSimulationRandom=1;
inline thread_local uint32_t* activeSimulationRandom=nullptr;
class SimulationRandomScope {
    uint32_t* previous_;
public:
    explicit SimulationRandomScope(uint32_t& state):previous_(activeSimulationRandom){activeSimulationRandom=&state;}
    ~SimulationRandomScope(){activeSimulationRandom=previous_;}
    SimulationRandomScope(const SimulationRandomScope&)=delete;
    SimulationRandomScope& operator=(const SimulationRandomScope&)=delete;
};
inline int SimulationRandomRaw() {
    auto& state=activeSimulationRandom?*activeSimulationRandom:fallbackSimulationRandom;
    state=state*214013u+2531011u;return int((state>>16)&32767u);
}
}
