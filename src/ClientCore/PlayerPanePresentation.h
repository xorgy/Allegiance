#pragma once
#include <algorithm>
#include <array>
#include <string_view>
#include <string>

namespace Allegiance {
inline int PlayerPaneNameCompare(std::string_view a,std::string_view b) {
    auto lower=[](unsigned char c){return c>='A' && c<='Z'?c+('a'-'A'):int(c);};
    for(size_t i=0;i<std::min(a.size(),b.size());++i) {
        int difference=lower(a[i])-lower(b[i]);if(difference)return difference;
    }
    return a.size()<b.size()?-1:a.size()>b.size()?1:0;
}
// TeamPane's header callbacks and TList::InsertSorted ("first is after second").
// Numeric kills/credits and wing keys are ascending; objective uses legal
// command text, and sector ignores the passenger's displayed "aboard" label.
// Its default LeaderCompare actually groups humans, with stable member order.
template<class Row> bool PlayerPaneBefore(const Row& a,const Row& b,int column) {
    switch(column) {
    case 0:if(a.human!=b.human)return a.human;return PlayerPaneNameCompare(a.name,b.name)<0;
    case 1:return false; // All native pilots have the same unauthenticated rank.
    case 2:return a.kills<b.kills;
    case 3:return PlayerPaneNameCompare(a.sectorName,b.sectorName)<0;
    case 4:return PlayerPaneNameCompare(a.ship,b.ship)<0;
    case 5:return a.wing<b.wing;
    case 6:return a.money<b.money;
    case 7:return PlayerPaneNameCompare(a.objective,b.objective)<0;
    default:return a.human && !b.human;
    }
}
constexpr std::array<float,3> PlayerSideColor(int side) {
    constexpr std::array<std::array<float,3>,7> colors{{{152/256.f,108/256.f,15/256.f},{11/256.f,26/256.f,119/256.f},{130/256.f,15/256.f,103/256.f},
        {27/256.f,143/256.f,16/256.f},{146/256.f,13/256.f,18/256.f},{57/256.f,207/256.f,132/256.f},{131/256.f,131/256.f,140/256.f}}};
    return colors[side>=0 && side<6?side:6];
}
// WinTrek::TK_ConModeTeam / TK_ConModeMiniTeam: the shortcuts choose
// their own size, switch from the other size, and close on a second press.
enum class PlayerPaneMode { Hidden, Compact, Expanded };
constexpr PlayerPaneMode TogglePlayerPane(PlayerPaneMode current,bool compact) {
    auto desired=compact?PlayerPaneMode::Compact:PlayerPaneMode::Expanded;
    return current==desired?PlayerPaneMode::Hidden:desired;
}
constexpr int PlayerPaneDonation(int balance,bool right) {
    return std::clamp(balance,0,right?500:100);
}
inline std::string PlayerPaneCompactAccount(bool commander,bool leader,bool docked,int seconds,int money) {
    if(commander && !leader && docked) {
        if(seconds<=0)return {};
        if(seconds<60)return "Docked for "+std::to_string(seconds)+"sec";
        return "Docked for "+std::to_string(seconds/60)+"min "+std::to_string(seconds%60)+"sec";
    }
    return "$: "+std::to_string(money);
}
constexpr std::string_view PlayerPaneDonationIcon(bool leader,int pilot,int to,int from,int self) {
    if(to==pilot)return leader?"icondonatetocombmp":"icondonatetobmp";
    if(from>=0) {
        if(from==self || from==to)return leader?"icondonatefromcombmp":"icondonatefrombmp";
        return leader?"icondonateyoucombmp":"icondonateyoubmp";
    }
    return leader?"icondonateselfcombmp":"icondonateselfbmp";
}
inline constexpr std::array<std::string_view,10> PlayerPaneOrderIcons{
    "","ackillbmp","accptbmp","acdefendbmp","acpickupbmp","acgotobmp",
    "acrepairbmp","acjoinbmp","acminebmp","acbuildbmp"};
}
