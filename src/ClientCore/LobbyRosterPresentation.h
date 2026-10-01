#pragma once
#include "EndgamePresentation.h"
#include <algorithm>
#include <vector>
#include <cstddef>
#include <cstdint>

namespace Allegiance {
enum class LobbyRosterSort { Leader, Name, Rank, Status, Wing };
inline bool LobbyRosterFlash(double seconds){return (uint64_t(std::max(0.,seconds)*4)&1)!=0;} // TeamScreen's 0.25-second timer.
constexpr LobbyRosterSort LobbyRosterColumn(LobbyRosterSort current,int column) {
    switch(column){case 0:return LobbyRosterSort::Name;case 3:return LobbyRosterSort::Rank;
    case 4:return LobbyRosterSort::Status;case 5:return LobbyRosterSort::Wing;default:return current;}
}
// TeamScreen uses TList::InsertSorted. Its leader comparator and the inverted
// status/wing comparators are not strict weak orders. Do not pass them to a
// standard sorting algorithm. Preserve their actual insertion results with
// linear leader insertion and strict, stable key sorts for the other columns.
template<class Row>
void SortLobbyRoster(std::vector<Row>& rows,LobbyRosterSort mode,bool reverse,
                     std::vector<Row>& scratch,std::vector<std::size_t>& links) {
    if(mode==LobbyRosterSort::Leader){
        auto end=rows.size();links.resize(end+1);links[end]=end;
        auto tail=end,anchor=end;
        for(std::size_t i=0;i<end;++i){
            auto parent=reverse?(rows[i].host?tail:anchor):(rows[i].commander?anchor:tail);
            links[i]=links[parent];links[parent]=i;
            if(links[i]==end)tail=i;
            if(reverse?!rows[i].commander:rows[i].host)anchor=i;
        }
        scratch.clear();scratch.reserve(end);
        for(auto i=links[end];i!=end;i=links[i])scratch.push_back(std::move(rows[i]));
        rows.swap(scratch);return;
    }
    // The original reverse status/wing comparisons invert the entire boolean,
    // including equality: later equal entries precede earlier equal entries.
    if(reverse && (mode==LobbyRosterSort::Status || mode==LobbyRosterSort::Wing))std::reverse(rows.begin(),rows.end());
    std::stable_sort(rows.begin(),rows.end(),[&](const Row& a,const Row& b){
        int order=0;
        auto compare=[&](auto x,auto y){order=x<y?-1:x>y?1:0;};
        switch(mode){
        case LobbyRosterSort::Name:order=EndgameNameCompare(a.name,b.name);break;
        case LobbyRosterSort::Rank:compare(a.rank,b.rank);break;
        case LobbyRosterSort::Status:
        case LobbyRosterSort::Wing:
            compare(!a.requesting,!b.requesting);
            if(!order){if(mode==LobbyRosterSort::Status)compare(a.ready,b.ready);else compare(a.wingID,b.wingID);}
            break;
        default:break;
        }
        return reverse?order>0:order<0;
    });
}
}
