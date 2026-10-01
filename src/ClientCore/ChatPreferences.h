#pragma once

namespace Allegiance {
struct ChatPreferences {
    bool filterAll{},filterQuick{},filterUnknown{true},censor{true};
    int filterLobby{3}; // 1: public lobby chats, 2: includes PMs, 3: no filter.
    bool operator==(const ChatPreferences& other)const {
        return filterAll==other.filterAll && filterQuick==other.filterQuick && filterUnknown==other.filterUnknown && censor==other.censor && filterLobby==other.filterLobby;
    }
};
struct ChatDisposition {bool discard{},quietVoice{};};
inline unsigned ChatPreferenceBits(const ChatPreferences& p) {
    return unsigned(p.filterAll)|(unsigned(p.filterQuick)<<1)|(unsigned(p.filterUnknown)<<2)|(unsigned(p.censor)<<3);
}
// WinTrek::ReceiveChat: muted pilots and filtered public/unknown messages
// cannot issue orders. Filtering known human quick comms keeps text/orders
// and substitutes the normal notification for the voice recording.
inline ChatDisposition FilterChat(const ChatPreferences& prefs,bool sender,bool human,
    bool muted,bool everyone,bool hasText,bool hasVoice,bool knownVoice,bool quick=false,
    bool fromLobby=false,bool individual=false,bool inGame=true,bool privileged=false) {
    return {sender && (muted || (prefs.filterAll && everyone && inGame) ||
                (fromLobby && inGame && !privileged && (prefs.filterLobby==2 || (prefs.filterLobby==1 && !individual))) ||
                (prefs.filterUnknown && human && !hasText && hasVoice && !knownVoice)),
            prefs.filterQuick && sender && human && hasVoice && (!hasText || quick)};
}
}
