#pragma once
#include "frontend.hpp"
#include "ui_clock.hpp"
#include "../ClientCore/ConsoleTips.h"
namespace Native {
// Compiled dialog.mdl order/tip overlays. Retained strings and controls change
// only on notices/layout/expiry; evaluation never traverses the MDL graph.
class ConsolePane {
public:
    explicit ConsolePane(Assets& assets);
    void reset();
    const MenuView& view(glm::ivec2 size,int scale,const GameState& game,int chatLines,bool loadout,bool shown,double now=uiTime());
private:
    bool consume(const GameState& game);
    void rebuild(const GameState& game,int chatLines,bool loadout,bool shown,unsigned visible);
    std::vector<HudProgram> programs_;
    std::unique_ptr<UiOverlayLayout> overlays_;std::array<size_t,6> paintOrder_{};
    float criticalWidth_{};uint32_t overlayModes_{};
    std::shared_ptr<const BitmapFont> criticalFont_;
    Allegiance::ConsoleTips tips_;
    MenuView view_;glm::ivec2 size_{};int scale_{},chatLines_{};bool loadout_{},shown_{};
    std::array<std::string,2> commandText_;
    std::string voteText_;
    uint64_t lastMessage_{},displayRevision_{};unsigned visible_{};
    glm::vec4 color_{};bool invalid_{true};
};
}
