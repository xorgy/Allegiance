// Original client event sound policy; adapters retain playback and routing.
#pragma once
#include "SoundIds.h"

namespace Allegiance {
inline constexpr int ClientEventSoundIds[]={mouseoverSound,mouseclickSound,positiveButtonClickSound,negativeButtonClickSound,changeLoadoutSound,
    chatKeySound,countdownStartingSound,countdown10minSound,countdown5minSound,countdown1minSound,countdown30Sound,
    countdown10Sound,countdown9Sound,countdown8Sound,countdown7Sound,countdown6Sound,countdown5Sound,countdown4Sound,countdown3Sound,countdown2Sound,countdown1Sound,
    newCommandMsgSound,newChatMsgFromCommanderSound,newPersonalMsgSound,newChatMsgSound,newOfflineChatMsgSound,newShipSound,newEnemySound};
inline SoundID CountdownSound(int seconds) {
    switch(seconds) {
    case 600:return countdown10minSound;
    case 300:return countdown5minSound;
    case 60:return countdown1minSound;
    case 30:return countdown30Sound;
    case 10:return countdown10Sound;
    case 9:return countdown9Sound;
    case 8:return countdown8Sound;
    case 7:return countdown7Sound;
    case 6:return countdown6Sound;
    case 5:return countdown5Sound;
    case 4:return countdown4Sound;
    case 3:return countdown3Sound;
    case 2:return countdown2Sound;
    case 1:return countdown1Sound;
    default:return -1;
    }
}
class CountdownSounds {
    int previous_{1000000};
public:
    void Reset(){previous_=1000000;}
    SoundID Observe(double timeLeft,bool starting=true) {
        // Preserve WinTrek's truncation and one-second lag. A delayed frame
        // announces only the current threshold, never a backlog of speech.
        int seconds=timeLeft< -1?0:starting?int(timeLeft)+1:int(timeLeft+1);
        if(seconds==previous_)return -1;
        previous_=seconds;return CountdownSound(seconds);
    }
};
inline SoundID ChatSound(SoundID voice,bool command,bool individualEcho,
                         bool personal,bool training,bool connected,bool remoteLeader) {
    if(voice!=-1)return voice;
    if(command && !individualEcho)return newCommandMsgSound;
    if(personal)return training?newChatMsgFromCommanderSound:newPersonalMsgSound;
    return connected?(remoteLeader?newChatMsgFromCommanderSound:newChatMsgSound):newOfflineChatMsgSound;
}
inline SoundID ShipContactSound(bool cached,bool hostile) {
    // These identifiers are intentionally as used by the original fast path;
    // renaming/reversing them would change its authored audio selection.
    return cached && !hostile?newEnemySound:newShipSound;
}
template<class StationType>
SoundID StationCaptureSound(StationType* type,bool lostFriendly,bool gainedFriendly) {
    return lostFriendly?type->GetCapturedSound():gainedFriendly?type->GetEnemyCapturedSound():-1;
}
template<class StationType>
SoundID StationDestroyedSound(StationType* type,bool ownSide) {
    return ownSide?type->GetDestroyedSound():type->GetEnemyDestroyedSound();
}
}
