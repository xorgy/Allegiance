#pragma once
#include <string_view>

namespace Allegiance {
inline constexpr const char* LanLogonPrompt="Enter a call sign to use for this game.";
// LogonPopup enables Logon for nonempty text, but refuses a leading space
// when accepting. Enter with an empty name follows the original Abort path.
inline bool LogonNameAcceptable(std::string_view name){return !name.empty() && name.front()!=' ';}
}
