// Original WinTrek receive/history and BaseClient local shell policies.
#pragma once
#include <cstddef>
namespace Allegiance {
constexpr std::size_t ChatHistoryCapacity=600;
inline bool RecordChat(bool hasCommand,bool individualEcho,bool forMe,bool individual,bool fromSelf) {
    return !hasCommand || individualEcho || ((forMe && individual)==!fromSelf);
}
inline float ChatTintScale(bool fromSelf,bool forMe,bool hasCommand) {
    return fromSelf && !forMe?.75f:!hasCommand?.85f:1.f;
}
inline unsigned char ChatAsciiLower(unsigned char c){return c>='A' && c<='Z'?c+('a'-'A'):c;}
inline bool ChatNamesEqual(const char* a,const char* b) {
    for(;*a && *b;++a,++b)if(ChatAsciiLower(*a)!=ChatAsciiLower(*b))return false;
    return *a==*b;
}
inline bool ChatCommandPrefix(const char* text,const char* prefix) {
    for(;*prefix;++text,++prefix)if(ChatAsciiLower(*text)!=*prefix)return false;
    return true;
}
struct MuteChatCommand {bool recognized{},muted{};const char* name{};};
inline MuteChatCommand ParseMuteChatCommand(const char* text) {
    if(ChatCommandPrefix(text,"!mute "))return {true,true,text+6};
    if(ChatCommandPrefix(text,"!unmute "))return {true,false,text+8};
    return {};
}
}
