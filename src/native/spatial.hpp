#pragma once
#include <glm/glm.hpp>
namespace Native {
// IGC orientations use local -Z as forward. Artwork loaded with options == 0
// has the same half-turn used by ThingGeo::RenderTargetGeo/TransformOffset.
inline glm::mat4 modelTransform(glm::vec3 right,glm::vec3 up,glm::vec3 backward,
    glm::vec3 position,float scale,bool artworkFlip) {
    float xz=artworkFlip?-scale:scale;
    return {glm::vec4(right*xz,0),glm::vec4(up*scale,0),glm::vec4(backward*xz,0),glm::vec4(position,1)};
}
}
