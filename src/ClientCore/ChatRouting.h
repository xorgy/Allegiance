#pragma once
#include "../Igc/igc.h"

namespace Allegiance {
// FedSrv::ForwardChatMessage gates public broadcast commands. Text and voice
// without commands still use these channels; the sender's client echoes its
// own outgoing message independently of server delivery.
inline bool ChatCarriesOrders(ChatTarget channel) {
    switch(channel){
    case CHAT_EVERYONE:case CHAT_ALLIES:case CHAT_ALL_SECTOR:
    case CHAT_LEADERS:case CHAT_ADMIN:case CHAT_INDIVIDUAL_ECHO:
    case CHAT_NOSELECTION:return false;
    case CHAT_SHIP:case CHAT_TEAM:case CHAT_INDIVIDUAL:case CHAT_INDIVIDUAL_NOFILTER:
    case CHAT_WING:case CHAT_FRIENDLY_SECTOR:case CHAT_GROUP:case CHAT_GROUP_NOECHO:return true;
    default:return false;
    }
}
inline bool ChatOrderAuthorized(ChatTarget channel,IsideIGC* sender,IsideIGC* recipient) {
    if(!ChatCarriesOrders(channel))return false;
    if(channel==CHAT_TEAM)return !sender || sender==recipient;
    if(channel==CHAT_INDIVIDUAL || channel==CHAT_INDIVIDUAL_NOFILTER || channel==CHAT_GROUP || channel==CHAT_GROUP_NOECHO)
        return !sender || sender==recipient || IsideIGC::AlliedSides(sender,recipient);
    return true; // Wing/ship/friendly-sector membership is checked by the route.
}
}
