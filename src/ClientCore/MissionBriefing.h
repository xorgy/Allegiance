#pragma once
#include <string>

namespace Allegiance {
// FedSrv supplies either a home-sector name for the asset-authored faction
// briefing, or literal server-authored story text. This is UI state, not a wire
// record; immutable shared instances avoid copying stories during pilot views.
struct MissionBriefing {
    std::string text;
    bool generateCivilization{true};
};
}
