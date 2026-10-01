#pragma once
#include "Alliances.h"
#include <string>

namespace Allegiance {
// TeamScreen::OnTeamRightClicked and CFSMission::UpdateAlliances. Alliances
// are available for conquest before play with at least three active sides.
template<class Active>
bool LobbyAlliancesAvailable(bool owner,bool beforeStart,bool otherGoals,int count,Active active) {
    if(!owner || !beforeStart || otherGoals || count<3 || count>c_cSidesMax)return false;
    int available=0;for(int side=0;side<count;++side)available+=active(SideID(side));
    return available>=3;
}
template<class Active,class Add>
void LobbyAllianceChoices(const char* allies,int count,int selected,Active active,Add add) {
    if(selected<0 || selected>=count || !active(SideID(selected)))return;
    if(allies[selected]!=NA)add(0,std::string("Remove allies"));
    else {
        bool groups[c_cAlliancesMax]{};
        for(int side=0;side<count;++side)if(side!=selected && active(SideID(side))) {
            int group=allies[side];
            if(group>=0 && group<c_cAlliancesMax)groups[group]=true;
            else add(side+1,"Ally with Team "+std::to_string(side+1));
        }
        for(int group=0;group<c_cAlliancesMax;++group)if(groups[group]) {
            std::string name;int representative=NA;
            for(int side=0;side<count;++side)if(allies[side]==group) {
                if(!name.empty())name+=",";name+=std::to_string(side+1);representative=side;
            }
            add(representative+1,"Ally with Teams "+name);
        }
    }
    add(NA,std::string("Cancel"));
}
struct AllianceMissionRules {bool defections{},sight{},ripcord{};int imbalance{32766};};
inline AllianceMissionRules LobbyAllianceRules(bool allied,bool ripcordDisabled) {
    // TeamScreen::OnMenuCommand / OnTeamAlliancesChange. Preserve a deliberate
    // allied-ripcord opt-out; the final removal returns to automatic balance.
    return {allied,allied,allied && !ripcordDisabled,allied?32767:32766};
}
}
