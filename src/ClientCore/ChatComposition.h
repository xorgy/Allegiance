#pragma once
#include <cctype>
#include <string_view>

namespace Allegiance {
inline constexpr int badChatMatch = 0x7fffffff;
// ConsoleData::MatchName: a prefix matches by the number of letters left.
// Strictly smaller scores retain the original table/list order on ties.
inline int ChatPrefixScore(std::string_view typed, std::string_view name) {
    if (typed.empty() || typed.size() > name.size()) return badChatMatch;
    for (size_t i=0; i<typed.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(typed[i])) !=
            std::tolower(static_cast<unsigned char>(name[i]))) return badChatMatch;
    return int(name.size()-typed.size());
}
enum class ComposeState { Idle, Mouse, Chat, Command, Shell, Quick };
enum class ChatRecipient { Everyone, Team, Allies, Sector, Wing, Individual, Ship, Other };
// ConsoleData::CycleChatTarget. Text chat skips self; commands do not. An
// empty picked sector is skipped, and allies follow Everyone in the cycle.
inline ChatRecipient NextChatRecipient(ChatRecipient current, bool textChat,
    bool allies, bool crew, bool sectorOccupied) {
    switch (current) {
    case ChatRecipient::Allies: return ChatRecipient::Wing;
    case ChatRecipient::Wing:
        if (!textChat) return ChatRecipient::Individual;
        [[fallthrough]];
    case ChatRecipient::Individual:
        if (crew) return ChatRecipient::Ship;
        [[fallthrough]];
    case ChatRecipient::Ship:
        if (sectorOccupied) return ChatRecipient::Sector;
        [[fallthrough]];
    case ChatRecipient::Sector: return ChatRecipient::Team;
    case ChatRecipient::Team: return ChatRecipient::Everyone;
    default: return allies ? ChatRecipient::Allies : ChatRecipient::Wing;
    }
}
inline constexpr std::string_view chatVerbs[] = {
    "", "attack", "capture", "defend", "pickup", "goto", "repair", "join", "mine", "build"
};
}
