#pragma once
#include <string>
#include <string_view>
namespace Native {
std::string censorChat(std::string_view text,bool enabled=true);
}
