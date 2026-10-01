#pragma once
// Frozen from WinTrekClient::ReceiveChat at
// 39ef2c072b1eac0e9f97ab1428bc96572cbd5854. Independent of ClientCore.
namespace OriginalChatPresentation {
inline bool record(bool command,bool echo,bool forMe,bool individual,bool self) {
    return !command || echo || ((forMe && individual)==!self);
}
inline float tint(bool self,bool forMe,bool command) {
    if(self && !forMe)return .75f;
    else if(!command)return .85f;
    return 1.f;
}
}
