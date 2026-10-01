#pragma once
#include "original_ui.hpp"
#include <SDL3/SDL.h>
#include <array>

namespace Native {
inline constexpr std::array<std::string_view,15> originalCursorNames{
    "default","drop","drag","select","illegal","attack","capture","defend",
    "pickup","goto","repair","join","mine","build","wait"};
// Original MDL hotspots/artwork, supplied to the platform cursor plane. Images
// and whole-scale variants are cached; cursor movement never uploads a texture.
class OriginalCursor {
public:
    explicit OriginalCursor(Assets& assets);
    ~OriginalCursor();
    OriginalCursor(const OriginalCursor&)=delete;
    OriginalCursor& operator=(const OriginalCursor&)=delete;
    bool set(std::string_view name,int scale,float pixelDensity=1);
    const OriginalControl& artwork(std::string_view name)const;
private:
    static size_t index(std::string_view name);
    std::array<OriginalControl,originalCursorNames.size()> art_;
    std::array<std::array<SDL_Cursor*,64>,originalCursorNames.size()> cursors_{};
    std::array<std::array<bool,64>,originalCursorNames.size()> attempted_{};
    SDL_Cursor* selected_{};
};
}
