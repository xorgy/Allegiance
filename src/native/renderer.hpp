#pragma once
#include "assets.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <span>
#include <SDL3/SDL.h>

namespace Native {
// Select the bundled macOS Vulkan loader/driver before creating a window.
void configureVulkanPlatform();
enum class ModelBlend { Opaque,Alpha,Additive };
struct DrawItem { uint32_t model;glm::mat4 transform{1};glm::vec4 color{1};bool unlit{};ModelBlend blend{};float textureOffset{},textureScale{1};bool aleph{};uint8_t background{}; };
// One world-space quad instance. axisWidth.xyz is an optional longitudinal
// half-axis; zero selects a camera-facing square. Texture bit 31 is additive.
struct ParticleDraw {glm::vec4 centerSize,axisWidth,color,uv{0,0,1,1};uint32_t texture;float angle{};};
static_assert(sizeof(ParticleDraw)==72);
// Ribbons reuse the sprite UV/size/angle payload for their two edge vectors.
inline constexpr uint32_t particleRibbon=0x40000000u;
inline constexpr uint32_t particleLine=0x20000000u;
inline constexpr uint32_t particleOriented=0x10000000u;
// One instance per quad; the vertex shader generates the six corners. The high
// texture bit selects additive art within the same ordered, premultiplied batch.
struct HudQuad { glm::vec4 bounds,uv,color;uint32_t texture; };
static_assert(sizeof(HudQuad)==52);
inline constexpr uint32_t hudAdditive=0x80000000u;
inline constexpr uint32_t hudLine=0x40000000u; // bounds=endpoints; uv.xy=half-width normal in NDC
struct ModelPreview {
    DrawItem item;glm::mat4 viewProjection{1};glm::ivec4 bounds{};
    uint32_t overlayOffset{}; // Insert the model between background and foreground UI.
    const ModelPreview* next{}; // Small bounded chain for authored dual target views.
    std::optional<glm::ivec4> clip; // Parent pane clip; preserve the viewport/projection.
};
struct RenderLighting {glm::vec4 direction{.4f,-.7f,-.5f,.22f},primary{.78f,.78f,.78f,0},alternate{0};};
struct HudBackground {std::span<const HudQuad> quads;uint32_t posterOffset{};};
inline constexpr uint32_t hudOriented=0x20000000u; // endpoints and half-height normal; full sprite UV
struct RenderResources {uint32_t vertices{},indices{},models{},textures{};};
struct RenderStats { double latencyMs{},refreshHz{};uint64_t missed{};uint32_t draws{},triangles{}; };
class Renderer {
public:
    Renderer(SDL_Window* window,bool validation);
    ~Renderer();
    Renderer(const Renderer&)=delete;
    Renderer& operator=(const Renderer&)=delete;
    uint32_t addModel(const std::shared_ptr<ModelData>& model,const std::shared_ptr<ImageData>& skin={});
    // Sampling variants share one GPU image/upload and have resident bindless
    // descriptors. UI sampling must not change a world material's filtering.
    uint32_t addTexture(const std::shared_ptr<ImageData>& image,bool nearest=false);
    void flushUploads();
    void updateTexture(uint32_t texture,const ImageData& image);
    void gamma(float level);
    std::vector<std::string> deviceInfo() const;
    std::vector<int> antialiasingOptions() const;
    int antialiasing(int samples); // Returns an actually supported count (0=off).
    void textureLimit(int level);
    // Wait for resource reuse, acquire and pace BEFORE polling/sampling input.
    bool beginFrame();
    // CLOCK_MONOTONIC nanoseconds for this frame's expected display, or zero
    // while presentation feedback has not established a schedule.
    uint64_t targetPresentTime() const;
    void draw(const glm::mat4& viewProjection,std::span<const DrawItem> items,
        std::span<const HudQuad> hud={},const std::string& screenshot={},const ModelPreview* preview=nullptr,std::span<const ParticleDraw> particles={},const RenderLighting& lighting={},HudBackground background={});
    glm::ivec2 size() const;
    RenderStats stats() const;
    RenderResources resources() const;
    void traceTiming(const std::string& file);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
