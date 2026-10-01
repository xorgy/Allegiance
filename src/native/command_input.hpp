#pragma once
#include "game.hpp"
#include "../ClientCore/CommandPicking.h"
#include <optional>
#include <limits>

namespace Native {
struct CommandGesture {
    std::optional<glm::vec2> selectionStart,orderStart;
    std::optional<glm::vec3> anchor;
    glm::vec2 pointer{};
    void clear(){selectionStart.reset();orderStart.reset();anchor.reset();}
    static glm::vec3 ray(glm::vec2 point,glm::ivec2 size,const glm::mat4& vp,glm::vec3 eye) {
        auto clip=point/glm::vec2(size)*2.f-1.f;
        auto p=glm::inverse(vp)*glm::vec4(clip,.0001f,1);
        return glm::normalize(glm::vec3(p)/p.w-eye);
    }
    void beginOrder(glm::vec2 point,glm::ivec2 size,const glm::mat4& vp,const CameraState& camera) {
        orderStart=pointer=point;anchor.reset();auto direction=ray(point,size,vp,camera.position);
        if(std::abs(direction.z)>1.e-5f){float t=-camera.position.z/direction.z;if(t>0)anchor=camera.position+t*direction;}
    }
    std::optional<glm::vec3> destination(glm::ivec2 size,const glm::mat4& vp,const CameraState& camera)const {
        if(!anchor)return {};auto p=*anchor;auto d=ray(pointer,size,vp,camera.position);
        p.z=Allegiance::CommandPickAltitude(p.x,p.y,camera.position.x,camera.position.y,camera.position.z,-camera.forward.z,d.x,d.y,d.z);
        if(!std::isfinite(p.z))return {};return p;
    }
};
// ConsoleData::ResolvePick chooses the nearest projected center whose screen
// radius contains the pointer, retaining list order for ties. Eligibility comes
// from IGC; radar labels and offscreen arrows are never pick regions here.
template<class Eligible>
const Contact* commandHover(std::span<const Contact> contacts,glm::vec2 point,
    glm::ivec2 size,const glm::mat4& vp,Eligible&& eligible) {
    const Contact* picked=nullptr;float nearest=std::numeric_limits<float>::max();
    float focal=glm::length(glm::vec3(vp[0][0],vp[1][0],vp[2][0]))*size.x*.5f;
    for(const auto& c:contacts) {
        auto clip=vp*glm::vec4(c.position,1);if(clip.w<=0 || clip.z<0 || clip.z>clip.w)continue;
        auto p=(glm::vec2(clip)/clip.w+1.f)*.5f*glm::vec2(size),delta=p-point;
        float radius=c.radius*focal/clip.w,distance=glm::dot(delta,delta);
        if(std::abs(delta.x)<=radius && std::abs(delta.y)<=radius && distance<=nearest && eligible(c)) {
            picked=&c;nearest=distance;
        }
    }
    return picked;
}
// On-demand rectangle query, independent of HUD label placement. Visible world
// contacts supply ownership; off-screen arrows must never select remote ships.
inline std::vector<int> commandRectangle(std::span<const Contact> contacts,glm::vec2 start,glm::vec2 stop,
    glm::ivec2 size,const glm::mat4& vp) {
    std::vector<int> result;auto low=glm::min(start,stop),high=glm::max(start,stop);
    float focal=glm::length(glm::vec3(vp[0][0],vp[1][0],vp[2][0]))*size.x*.5f;
    for(auto& c:contacts)if(c.type==ContactShip && (c.own || c.ally)) {
        auto clip=vp*glm::vec4(c.position,1);if(clip.w<=0 || clip.z<0 || clip.z>clip.w)continue;
        auto p=(glm::vec2(clip)/clip.w+1.f)*.5f*glm::vec2(size);
        if(Allegiance::CommandSelectionOverlap(p.x,p.y,c.radius*focal/clip.w,low.x,low.y,high.x,high.y))result.push_back(c.id);
    }
    return result;
}
}
