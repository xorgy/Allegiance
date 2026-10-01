#include "frustum.hpp"
#include <cmath>
#include <cstdio>
#include <random>
#include <stdexcept>

int main() {
    try {
        std::mt19937 random(1937);
        auto number=[&](float scale){return (float(random()%200001)-100000)*scale/100000;};
        size_t checked=0;
        for(int layout=0;layout<64;++layout) {
            std::array<glm::vec4,6> planes;
            for(auto& plane:planes) {
                plane={number(1),number(1),number(1),number(400000)};
                plane/=glm::length(glm::vec3(plane));
            }
            Native::Frustum frustum(planes);
            auto compare=[&](glm::vec4 center,float radius) {
                bool reference=std::any_of(planes.begin(),planes.end(),[&](auto plane){return glm::dot(plane,center)<-radius;});
                if(frustum.outside(center,radius)!=reference)throw std::runtime_error("packed frustum changes scalar visibility");
                ++checked;
            };
            for(int i=0;i<4096;++i)compare({number(400000),number(400000),number(400000),1},std::abs(number(10000)));
            // Tangencies and adjacent representable floats stress cancellation,
            // strict comparisons and the two planes in the final SIMD group.
            for(const auto& plane:planes)for(float radius:{0.f,1.f,500.f}) {
                glm::vec4 center(0,0,0,1);
                int axis=0;for(int a=1;a<3;++a)if(std::abs(plane[a])>std::abs(plane[axis]))axis=a;
                center[axis]=(-radius-plane.w)/plane[axis];
                compare(center,radius);
                center[axis]=std::nextafter(center[axis],INFINITY);compare(center,radius);
                center[axis]=std::nextafter(center[axis],-INFINITY);compare(center,radius);
                center[axis]=std::nextafter(center[axis],-INFINITY);compare(center,radius);
            }
        }
        printf("Frustum: %zu scalar/packed comparisons, including large coordinates and tangencies, passed\n",checked);
        return 0;
    }catch(const std::exception& error){fprintf(stderr,"Frustum check: %s\n",error.what());return 1;}
}
