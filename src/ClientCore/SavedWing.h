#pragma once
#include <optional>

namespace Allegiance {
inline constexpr int WingCount=10;
inline int SavedWingAssignment(int saved) {
    // TeamScreen maps an explicitly unassigned preference to Attack. The
    // original configuration's absent preference defaults to Command (zero).
    return saved==-1?1:saved>=0 && saved<WingCount?saved:0;
}
class WingEntry {
    int pilot_{-1},side_{-1};
public:
    void reset(){pilot_=side_=-1;}
    std::optional<int> enter(int pilot,int side,bool commander,int saved) {
        if(pilot_==pilot && side_==side)return {};
        pilot_=pilot;side_=side;
        // Only faction entry restores the local preference. Later reports and
        // commander assignments must not continually overwrite each other.
        if(pilot<0 || side<0 || commander)return {};
        return SavedWingAssignment(saved);
    }
};
}
