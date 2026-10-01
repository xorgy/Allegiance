#pragma once
#include "frontend.hpp"
#include "../ClientCore/PlayerPanePresentation.h"

namespace Native {
// The original F6 pane is a non-modal console overlay. Keep it independent
// of full-screen navigation, as with the inline chat composer.
class PlayerPane {
public:
    explicit PlayerPane(Assets& assets);
    void reset();
    void toggle(Game& game,bool compact=false);
    void close();
    bool active()const{return mode_!=Allegiance::PlayerPaneMode::Hidden;}
    bool capturing()const{return !popup_.empty();}
    bool pointerCaptured()const{return bool(pressed_);}
    void cancelPointer(){pressed_.reset();drag_=false;dirty_=true;}
    Allegiance::PlayerPaneMode mode()const{return mode_;}
    int selected()const{return selected_;}
    int team()const{return team_;}
    void sync(Game& game,bool force=false);
    const MenuView& view(glm::ivec2 size,int scale);
    std::optional<FrontResult> pointer(glm::ivec2 size,glm::vec2 pixel,int button,bool down,bool doubleClick=false);
    void hover(glm::ivec2 size,glm::vec2 pixel);
    bool wheel(float amount);
    std::optional<FrontResult> key(SDL_Keycode key);
    std::optional<FrontResult> tick(double seconds);
    std::span<const SoundID> sounds()const{return feedback_.sounds();}
    void clearSounds(){feedback_.clear();}
private:
    struct PopupRow {std::string text;char key{};MenuAction action;};
    void rebuild();
    void sort();
    void context();
    void popupLayout();
    void selectPopup(int row);
    void scrollbar();
    FrontResult activate(MenuAction action,bool controlEvent=false);
    glm::vec2 local(glm::ivec2 size,glm::vec2 pixel)const;
    const PlayerPaneRow* selection()const;
    void text(std::string name,glm::vec2 at,std::string value,glm::vec4 clip,const char* font="smallFont",glm::vec4 color=glm::vec4(1));
    void image(std::string name,glm::vec2 at,bool keyed=true,std::optional<glm::vec4> clip={});
    void rectangle(glm::vec4 bounds,glm::vec4 color,std::optional<glm::vec4> clip={});
    void hit(glm::vec4 bounds,MenuAction action,bool enabled=true,bool selected=false,std::string control={});
    Assets& assets_;OriginalUi original_;UiOverlayLayout overlays_;std::array<std::vector<OriginalControl>,2> authored_;
    struct AuthoredLists {glm::vec4 players{},bar{},teams{},selected{},icons{};OriginalControl combo,wing;std::vector<int> columns;};
    std::array<AuthoredLists,2> layouts_;uint32_t overlayModes_{};bool expandedLayout_{};
    PlayerPaneData data_;std::vector<const PlayerPaneRow*> rows_;
    Allegiance::PlayerPaneMode mode_{Allegiance::PlayerPaneMode::Hidden};
    int selected_{-1},team_{-1},sort_{-1};ScrollList list_;float teamOffset_{};glm::vec4 teamViewport_{};
    std::vector<PopupRow> popup_;glm::vec4 popupBounds_{};int popupSelected_{};
    enum class PopupKind {None,Context,Teams,Wings};PopupKind popupKind_{};
    // The original compact/expanded ComboPanes retain their own IMenu highlight.
    std::array<std::array<int,2>,2> comboSelections_{};
    MenuView view_;glm::ivec2 size_{};int scale_{1};glm::vec2 pointer_{},popupAt_{};glm::vec4 paneBounds_{};
    std::optional<MenuAction> pressed_;int pressedButton_{};float grab_{};double repeat_{},entered_{},refreshAt_{};
    bool dirty_{true},closing_{},drag_{},focus_{},repeatTimer_{};
    UiSoundFeedback feedback_;
};
}
