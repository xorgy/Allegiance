#pragma once
#include <array>
namespace Native {
// Original ForceEffectID values; repeated shots coalesce within one frame.
struct Feedback {
    unsigned mask{};
    std::array<int,3> direction{};
    void add(int id,int degrees=0) {if(id>=0 && id<3){mask|=1u<<id;direction[id]=degrees;}}
};
}
