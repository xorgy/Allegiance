#pragma once
namespace Training {
#ifdef ALLEGIANCE_NATIVE
// Authored goals use the same simulation clock as IGC. The native main loop
// advances it only while the session is running, including audio progression.
struct Clock {
    static Time Now();
    static bool IsPaused();
    static void Pause();
    static void Continue();
};
#else
using Clock = Time;
#endif
}
