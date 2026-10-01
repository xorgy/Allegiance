#pragma once
#include <algorithm>
#include <cstddef>

namespace Allegiance {
// ChatListPane::UpdateSize and ListPaneOld's integral visible slots. Geometry,
// cropping and anchoring belong to the authored pane/image expressions.
struct CommPanePresentation {
    int lines{},lineHeight{},listHeight{};
    std::size_t slots{};
    CommPanePresentation(int requestedLines,int fontHeight,bool loadout) {
        lines=std::max(1,loadout?std::min(requestedLines,6):requestedLines);
        lineHeight=std::max(1,fontHeight);listHeight=lines*lineHeight+8;
        slots=std::size_t(listHeight/lineHeight);
    }
};
}
