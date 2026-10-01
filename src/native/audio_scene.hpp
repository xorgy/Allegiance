#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <memory>
namespace Native {
struct SoundEmitter {glm::vec3 position{},velocity{};glm::mat3 orientation{1};bool relative{},active{true};};
struct SoundEvent {int id;glm::vec3 position,velocity;bool relative{};glm::vec3 direction{0,0,-1};std::shared_ptr<SoundEmitter> emitter;glm::vec3 offset{};};
struct SoundLoop {
    int id{-1};float gain{1},pitch{1};glm::vec3 position{},velocity{};bool relative{};
    uint64_t key{};glm::vec3 direction{0,0,-1};
};
}
