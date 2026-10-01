#pragma once
#include <algorithm>
#include <array>
#include <glm/glm.hpp>
#ifdef __aarch64__
#include <arm_neon.h>
#endif

namespace Native {
// Pack across planes, rather than reducing four vector components separately
// for every sprite. Construction is once per frame; no instance data changes.
class Frustum {
public:
    explicit Frustum(const std::array<glm::vec4,6>& planes)
#ifndef __aarch64__
        : planes_(planes)
#endif
    {
#ifdef __aarch64__
        for(size_t group=0;group<2;++group) {
            std::array<std::array<float,4>,4> components;
            for(size_t lane=0;lane<4;++lane) {
                const auto& plane=planes[std::min(group*4+lane,size_t(5))];
                for(size_t axis=0;axis<4;++axis)components[axis][lane]=plane[axis];
            }
            for(size_t axis=0;axis<4;++axis)packed_[group][axis]=vld1q_f32(components[axis].data());
        }
#endif
    }
    bool outside(glm::vec4 center,float radius) const {
#ifdef __aarch64__
        auto limit=vdupq_n_f32(-radius);
        for(const auto& planes:packed_) {
            auto xy=vaddq_f32(vmulq_n_f32(planes[0],center.x),vmulq_n_f32(planes[1],center.y));
            auto zw=vaddq_f32(vmulq_n_f32(planes[2],center.z),vmulq_n_f32(planes[3],center.w));
            // Same pairwise sum as GLM's vec4 dot, with contraction disabled
            // by the native build, including at exact frustum boundaries.
            if(vmaxvq_u32(vcltq_f32(vaddq_f32(xy,zw),limit)))return true;
        }
        return false;
#else
        return std::any_of(planes_.begin(),planes_.end(),[&](auto plane){return glm::dot(plane,center)<-radius;});
#endif
    }
private:
#ifdef __aarch64__
    std::array<std::array<float32x4_t,4>,2> packed_;
#else
    std::array<glm::vec4,6> planes_;
#endif
};
}
