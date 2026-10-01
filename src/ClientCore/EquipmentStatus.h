// Original PartWrapper mounting/arming states. The caller supplies its clock's
// armed fraction; WinTrek uses Time::Now, native sessions use the IGC clock.
#pragma once
#include "../Igc/igc.h"
namespace Allegiance {
inline float LauncherArmingFraction(float remaining,float loadTime) {
    return 1.0f-std::max(0.0f,remaining/loadTime);
}
template<class Armed>
int EquipmentReadyState(IpartIGC* part,Armed armed) {
    if(!part || part->GetMountID()<0)return 0;
    if(part->GetMountedFraction()<1.0f)return 1;
    if(IlauncherIGC::IsLauncher(part->GetObjectType()) && armed(static_cast<IlauncherIGC*>(part))<1.0f)return 2;
    return 3;
}
}
