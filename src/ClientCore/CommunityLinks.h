#pragma once
namespace Allegiance {
inline constexpr int WikiLink=10;
// IntroScreen and TeamScreen::OnButtonDiscord destinations. Team voice
// invites are distinct from the main menu's community invitation.
inline const char* CommunityLink(int index) {
    static constexpr const char* links[]={
        "https://discord.gg/WcEJ9VH",
        "https://steamcommunity.com/stats/700480/leaderboards/2212084",
        "https://www.freeallegiance.org/forums",
        "https://discord.gg/qv3cEa7", "https://discord.gg/6JWrRdZ",
        "https://discord.gg/wytSK2Q", "https://discord.gg/5QWGrSa",
        "https://discord.gg/WrHj7AS", "https://discord.gg/eUrSxek",
        "https://discord.gg/43jn6Ab",
        "http://www.freeallegiance.org/FAW/index.php/Quick_Crash_Course"};
    return index>=0 && index<int(sizeof(links)/sizeof(*links))?links[index]:nullptr;
}
}
