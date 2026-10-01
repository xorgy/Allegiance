#pragma once
#include <string_view>

namespace Allegiance {
inline constexpr char MissionChatHelp[]="#draw | #resign | #autoaccept | #autobalance [1|2|a|n] | #ban | #mutiny";
constexpr unsigned char ChatLower(unsigned char ch){return ch>='A' && ch<='Z'?ch+('a'-'A'):ch;}
inline bool ChatStarts(std::string_view text,std::string_view prefix) {
    if(text.size()<prefix.size())return false;
    for(size_t i=0;i<prefix.size();++i)if(ChatLower(text[i])!=ChatLower(prefix[i]))return false;
    return true;
}
inline bool ChatNameEqual(std::string_view a,std::string_view b){return a.size()==b.size() && ChatStarts(a,b);}
constexpr bool ValidTeamImbalance(int value){return value==1 || value==2 || value==32766 || value==32767;}
enum class MissionChatKind {None,AutoAccept,AutoBalance,List,Ban};
struct MissionChatContext {bool owner{},commander{},onTeam{},started{};};
struct MissionChatCommand {MissionChatKind kind{};int imbalance{-1};std::string_view target;};
// FedSrv.CPP compares case-insensitive prefixes, including suffixes, and
// cbMessage includes the NUL. Retain spacing, length and role fallthrough.
inline MissionChatCommand ParseMissionChat(std::string_view text,MissionChatContext context) {
    if(context.owner && ChatStarts(text,"#ban "))return {MissionChatKind::Ban,-1,text.substr(5)};
    if(!context.onTeam || !context.started)return {};
    if(context.commander && ChatStarts(text,"#autoaccept"))return {MissionChatKind::AutoAccept};
    if(context.owner && text.size()>=std::string_view("#autobalance  ").size() && ChatStarts(text,"#autobalance")){
        int value=ChatStarts(text,"#autobalance 1")?1:ChatStarts(text,"#autobalance 2")?2:
            ChatStarts(text,"#autobalance a")?32766:ChatStarts(text,"#autobalance n")?32767:-1;
        return {MissionChatKind::AutoBalance,value};
    }
    if(ChatStarts(text,"#list"))return {MissionChatKind::List};
    return {};
}
}
