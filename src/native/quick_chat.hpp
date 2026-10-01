#pragma once
#include "assets.hpp"
namespace Native {
struct QuickChatVoice {int sound{-1};std::string text,soundName;};
struct QuickChatSonic {int id{-1};std::vector<QuickChatVoice> voices;};
struct QuickChatCommand {int sound{-1},command{-1},targetMask{},abilities{};std::string text,soundName;int sonic{-1};};
struct QuickChatItem {char key{};std::string label;int menu{-1},command{-1};};
struct QuickChatMenu {std::vector<QuickChatItem> items;};
// Immutable artwork data, parsed and warmed before flight.
struct QuickChatCatalog {
    explicit QuickChatCatalog(Assets& assets);
    std::vector<QuickChatMenu> menus;
    std::vector<QuickChatCommand> commands;
    std::vector<int> soundIds;
    std::vector<QuickChatSonic> sonicChats; // authored IDs, never OpenAL/template IDs
    const QuickChatSonic* sonicChat(int id)const;
    const QuickChatVoice* voiceForChat(int id,size_t variant=0)const;
    void refreshSounds(); // startup audio registration/fallback; keeps logical IDs intact
};
}
