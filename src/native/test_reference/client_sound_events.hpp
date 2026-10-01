// Frozen original 39ef2c072b1eac0e9f97ab1428bc96572cbd5854 countdown and chat branches.
// Platform lookups are explicit inputs; this is independent of ClientCore.
#pragma once
#include "../../ClientCore/SoundIds.h"
namespace OriginalClientSounds {
inline int countdown(int seconds){
                switch (seconds)
                {
                case 10 * 60:
                    return countdown10minSound;
                    break;

                case 5 * 60:
                    return countdown5minSound;
                    break;

                case 1 * 60:
                    return countdown1minSound;
                    break;

                case 30:
                    return countdown30Sound;
                    break;

                case 10:
                    return countdown10Sound;
                    break;

                case 9:
                    return countdown9Sound;
                    break;

                case 8:
                    return countdown8Sound;
                    break;

                case 7:
                    return countdown7Sound;
                    break;

                case 6:
                    return countdown6Sound;
                    break;

                case 5:
                    return countdown5Sound;
                    break;

                case 4:
                    return countdown4Sound;
                    break;

                case 3:
                    return countdown3Sound;
                    break;

                case 2:
                    return countdown2Sound;
                    break;

                case 1:
                    return countdown1Sound;
                    break;
                }
return -1;
}
inline int chat(int voice,bool command,bool echo,bool personal,bool training,bool connected,bool remoteLeader){
        if (voice != NA)
        {
            return voice;
        }
        else if (command && !echo)
        {
            return newCommandMsgSound;
        }
        else if (personal)
        {
            if (training)
                return newChatMsgFromCommanderSound;
            else
                return newPersonalMsgSound;
        }
        else
        {
            if (connected)
            {
                if (remoteLeader)
                    return newChatMsgFromCommanderSound;
                else
                    return newChatMsgSound;
            }
            else
                return newOfflineChatMsgSound;
        }
}
}
