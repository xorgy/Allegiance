#pragma once
#include "frontend.hpp"

namespace Native {
// Original victory-condition console, independent of full-screen navigation.
class GameStatePane {
public:
    explicit GameStatePane(Assets& assets):assets_(assets),original_(assets),overlays_(original_,{"gameStateImage"}){titleFont_=original_.font("dialog","gameStateContainer",0);clockFont_=original_.font("dialog","gameStateContainer",1);}
    void reset();
    void toggle(Game& game);
    void close();
    bool active()const{return active_;}
    bool pointerCaptured()const{return pressedButton_!=0;}
    void cancelPointer(){pressedButton_=0;dirty_=true;}
    void sync(Game& game,bool force=false);
    const MenuView& view(glm::ivec2 size,int scale);
    // Owns both edges of a click begun on the panel, including a release
    // outside it, so dismissing it cannot fire a gun or launch a hangar button.
    bool pointer(glm::ivec2 size,glm::vec2 pixel,int button,bool down);
    void hover(glm::ivec2 size,glm::vec2 pixel);
    std::span<const SoundID> sounds()const{return feedback_.sounds();}
    void clearSounds(){feedback_.clear();}
    bool key(SDL_Keycode key);
private:
    void rebuild();
    OriginalControl titleFont_,clockFont_;
    Assets& assets_;OriginalUi original_;UiOverlayLayout overlays_;GameStatePanelData data_;MenuView view_;
    glm::ivec2 size_{};int scale_{1},pressedButton_{};glm::vec4 bounds_{};
    double entered_{},refreshAt_{};bool active_{},closing_{},dirty_{true},pressedClose_{};
    uint32_t overlayModes_{};
    UiSoundFeedback feedback_;
};
}
