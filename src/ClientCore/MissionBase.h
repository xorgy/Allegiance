#pragma once
#include <type_traits>

namespace Allegiance {
// CFSMission::GetBase order and capability policy, also used for native briefing
// data. The original station list remains authoritative; no duplicate index.
template<class Side,class Capability>
auto MissionRestartBase(Side* side,Capability restart)->typename std::remove_reference<decltype(side->GetStations()->first()->data())>::type{
    if(side)for(auto* link=side->GetStations()->first();link;link=link->next())
        if(link->data()->GetStationType()->HasCapability(restart))return link->data();
    return nullptr;
}
}
