// Frozen HEAD 39ef2c072b1eac0e9f97ab1428bc96572cbd5854 ClusterSiteImpl behavior.
// Only platform calls/fields are replaced with explicit arguments and callbacks.
#pragma once
#include "../../Igc/igc.h"
namespace OriginalWarnings {
template<class Threat,class Secured,class Lost,class Sound>
void changes(AssetMask previous,AssetMask current,bool inGame,bool invulnerable,WingID wing,Threat threat,Secured secured,Lost lost,Sound sound) {

            if (previous == current)
                return; // nothing to do

            bool bInvulnerableStations = invulnerable;
            ClusterWarning  cwOld = GetClusterWarning(previous, bInvulnerableStations);
            ClusterWarning  cwNew = GetClusterWarning(current, bInvulnerableStations);

            if ((cwNew != cwOld) && (cwNew > c_cwNoThreat))
            {
                static const SoundID c_vAlertSounds[c_cwMax] = 
                {
                    NA, 
                    NA,
                    NA,
                    NA,
                    NA,
                    minerThreatenedSound, 
                    constructorThreatenedSound, 
                    carrierThreatenedSound, 
                    bomberDetectedSound,
                    carrierDetectedSound, 
                    capitalDetectedSound,
                    teleportDetectedSound,
                    transportDetectedSound,
                    stationThreatenedSound,
                    stationThreatenedSound,
                    stationThreatenedSound
                };

                if ((cwNew >= c_cwMinerThreatened) || (wing == 0))
                    threat(cwNew > cwOld, GetClusterWarningText(cwNew));

                if (c_vAlertSounds[cwNew] != NA)
                    sound(c_vAlertSounds[cwNew]);
            }

            if (inGame)
            {
                bool bNewSectorSecured = (current & c_amStation) 
                    && !(current & c_amEnemyStation);
                bool bOldSectorSecured = (previous & c_amStation)
                    && !(previous & c_amEnemyStation);
            
                if (bNewSectorSecured && !bOldSectorSecured)
                {
                    secured();
                    sound(sectorSecuredSound);
                }
                else if (!(current & c_amStation) 
                    && (previous & c_amStation))
                {
                    lost();
                    sound(sectorLostSound);
                }
            }

            
        
}
}
