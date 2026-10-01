// Original ClusterSiteImpl::UpdateClusterWarnings. The adapter supplies text
// and audio; the IGC mask classification remains GetClusterWarning.
#pragma once
#include "../Igc/igc.h"

namespace Allegiance {
template<class Threat,class Secured,class Lost,class Sound>
inline void ClusterWarningChanges(AssetMask previous,AssetMask current,bool inGame,
    bool invulnerableStations,WingID wing,Threat threat,Secured secured,Lost lost,Sound sound) {
    if(previous==current)return;
    const auto oldWarning=GetClusterWarning(previous,invulnerableStations);
    const auto newWarning=GetClusterWarning(current,invulnerableStations);
    if(newWarning!=oldWarning && newWarning>c_cwNoThreat) {
        static constexpr SoundID sounds[c_cwMax]={NA,NA,NA,NA,NA,minerThreatenedSound,
            constructorThreatenedSound,carrierThreatenedSound,bomberDetectedSound,carrierDetectedSound,
            capitalDetectedSound,teleportDetectedSound,transportDetectedSound,stationThreatenedSound,
            stationThreatenedSound,stationThreatenedSound};
        if(newWarning>=c_cwMinerThreatened || wing==0)threat(newWarning>oldWarning,GetClusterWarningText(newWarning));
        if(sounds[newWarning]!=NA)sound(sounds[newWarning]);
    }
    if(inGame) {
        bool nowSecured=(current&c_amStation) && !(current&c_amEnemyStation);
        bool wasSecured=(previous&c_amStation) && !(previous&c_amEnemyStation);
        if(nowSecured && !wasSecured){secured();sound(sectorSecuredSound);}
        else if(!(current&c_amStation) && (previous&c_amStation)){lost();sound(sectorLostSound);}
    }
}
}
