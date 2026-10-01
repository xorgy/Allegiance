#pragma once
#include <array>
#include <algorithm>
namespace Native {
// Retain the first port's persisted low/medium/high values; the original
// WinTrek smoke values are 1/3/5, with 0 off and 2 the rocket performance mode.
struct ParticlePreference {int limit,next,original;bool performance;const char* label;};
inline constexpr std::array<ParticlePreference,5> particlePreferences{{
    {1,1,1,false,"Particles On - Low Quality"},
    {3,2,3,false,"Particles On - Medium Quality"},
    {5,3,5,false,"Particles On - High Quality"},
    {0,4,0,false,"Particles Off"},
    {1,0,2,true,"Particles On - Performance"}
}};
inline constexpr std::array<float,4> debrisSpacing{0,1.5f,1.f,.8f};
inline constexpr std::array<const char*,4> debrisLabels{"Debris Off","Debris Low","Debris Medium","Debris High"};
inline const ParticlePreference& particlePreference(int mode){return particlePreferences[std::clamp(mode,0,4)];}
}
