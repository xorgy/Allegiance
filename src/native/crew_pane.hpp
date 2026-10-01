#pragma once
#include "frontend.hpp"
#include "../ClientCore/CrewPresentation.h"
namespace Native {
// Compiled equivalent of dialog::TurretOfImage/InTurretPaneImage/BoardTextImage.
// It is display-only and never captures controls or holds IGC object pointers.
class CrewPane {
public:
    explicit CrewPane(Assets& assets):ui_(assets){}
    const MenuView& view(glm::ivec2 size,int scale,const GameState& game,bool loadout,bool shown,int style=0);
private:
    void rebuild();
    struct Authored {std::unique_ptr<HudProgram> turret,board,inTurret;std::unique_ptr<UiOverlayLayout> overlays;};
    OriginalUi ui_;std::array<Authored,6> authored_;MenuView view_;glm::ivec2 size_{};int scale_{1};
    Allegiance::CrewBoardState board_{Allegiance::CrewBoardState::Pilot};
    std::string pilot_;glm::vec4 color_{1};bool loadout_{},shown_{};
    int style_{};
};
}
