// Frozen WinTrek::ReceiveChat filter branches from
// 39ef2c072b1eac0e9f97ab1428bc96572cbd5854, with platform lookups as inputs.
#pragma once
namespace OriginalChatFilters {
struct Result {bool discard{},quietVoice{};};
inline Result receive(bool filterAll,bool filterQuick,bool filterUnknown,bool sender,
    bool human,bool muted,bool everyone,bool text,bool voice,bool known) {
    if(sender) {
        if(muted || (filterAll && everyone) ||
           (filterUnknown && human && !text && voice && !known))return {true,false};
    }
    bool quiet=false;
    if(voice && !text) {
        if(filterQuick && sender && human)quiet=true;
    }
    return {false,quiet};
}
}
