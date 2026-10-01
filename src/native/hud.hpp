#pragma once
#include "game.hpp"
#include "frontend.hpp"
#include <optional>
namespace Native {
struct TrainingSlide;struct CommandGesture;
struct MessageLayoutStats {size_t rows{},offset{};uint64_t updates{};size_t pageRows{};float lineHeight{};glm::vec4 listBounds{};bool following{};};
enum class HudCommand { Target,Weapon,Autopilot,Respawn,Launch,NextHull,Menu,Cargo,Sector,CommandOrder,Team,Scores,SessionMenu,SectorChat,SectorHighlight,CommandRecipient,CommandWing };
struct HudAction { HudCommand command;int value{},type{}; };
struct HudPointerResult {bool handled{},capture{};std::optional<HudAction> action;};

// Native presentation only: no legacy Image/Pane/Context objects or MDL evaluator.
// Artwork coordinates are translated into anchored logical panels here. GameState
// is the boundary for both the classic skin and future vector/layout replacements.
class Hud {
public:
    Hud(Renderer& renderer,Assets& assets,float uiScale=1);
    ~Hud();
    void preload(std::span<const Contact> contacts);
    void preloadSlides(std::span<const TrainingSlide> slides);
    std::span<const HudQuad> buildSlide(glm::ivec2 size,TrainingSlide& slide,size_t index,size_t count);
    void preloadIcons(std::span<const std::string_view> icons);
    void preloadInventory(std::span<const std::string_view> scripts);
    void setScale(float scale);
    void displayScale(float scale);
    void setDiagnostics(bool enabled);
    void modernInterface(bool enabled);
    void configure(const Settings& settings);
    int cycleRadar(bool commandView);
    void resetMessages();
    MessageLayoutStats messageLayoutStats()const;
    void pageMessages(const GameState& game,bool older);
    bool key(SDL_Keycode key); // diagnostic replay compatibility
    bool binding(Binding binding);
    std::optional<HudAction> click(glm::vec2 framebufferPosition);
    void sectorInfoService(std::function<const SectorInfo&(int)> service);
    HudPointerResult mapPointerButton(glm::vec2 framebufferPosition,int button,bool down,bool control=false);
    bool mapPointerMove(glm::vec2 framebufferPosition);
    bool pointerCaptured()const;
    void cancelPointer();
    std::optional<std::string_view> mapCursor(glm::vec2 framebufferPosition,bool canOrder)const;
    std::span<const HudQuad> appendPointer(glm::ivec2 size,glm::vec2 pointer,const Contact* hovered,const OriginalControl* fallbackCursor=nullptr,glm::vec4 color=glm::vec4(1));
    void consoleControls(int recipient,std::array<bool,9> verbs);
    std::span<const HudQuad> build(glm::ivec2 size,const GameState& game,
        std::span<const Contact> contacts,const glm::mat4& viewProjection,const RenderStats& stats,
        bool paused,bool help,bool cursor=false,glm::vec2 pointer={-1,-1},const CommandGesture* gesture=nullptr,const CameraState* camera=nullptr,std::span<const int> investmentSectors={},uint32_t consoleOverlays=0);
    std::span<const HudQuad> buildSectorMap(glm::ivec2 size,const GameState& game,glm::vec2 pointer,uint32_t consoleOverlays,std::span<const int> investmentSectors={});
    std::span<const HudQuad> appendMessages(glm::ivec2 size,const GameState& game,uint32_t consoleOverlays=0);
    std::span<const HudQuad> appendStatus(glm::ivec2 size,const GameState& game,uint32_t consoleOverlays=0);
    HudBackground buildBackground(glm::ivec2 size,const GameState& game,const CameraState& camera);
    const ModelPreview* preview() const;
    std::span<const HudQuad> buildMenu(glm::ivec2 size,const MenuView& menu,glm::vec2 pointer,bool append=false);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
