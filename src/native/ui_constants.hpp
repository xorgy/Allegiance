#pragma once
#include <glm/glm.hpp>
#include <optional>
#include <string_view>
#include "../ClientCore/SoundIds.h"
namespace Native {
// The original trek namespace exports every IGC sound as <name>SoundId.
inline std::optional<SoundID> originalSoundId(std::string_view name){
    if(!name.ends_with("SoundId"))return {};
#define DEFSOUND(id) if(name==#id "SoundId")return id##Sound;
#define DEFEMPTYSOUND(id) DEFSOUND(id)
#include "../Igc/sounds.h"
#undef DEFEMPTYSOUND
#undef DEFSOUND
    return {};
}
// modeler.cpp's effect namespace exports; authored definitions take precedence.
inline std::optional<glm::vec4> originalColor(std::string_view name){
    if(name=="white")return glm::vec4(1);
    if(name=="black")return glm::vec4(0,0,0,1);
    if(name=="red")return glm::vec4(1,0,0,1);
    if(name=="green")return glm::vec4(0,1,0,1);
    if(name=="blue")return glm::vec4(0,0,1,1);
    if(name=="yellow")return glm::vec4(1,1,0,1);
    if(name=="cyan")return glm::vec4(0,1,1,1);
    return {};
}
}
