#pragma once
#include <chrono>
namespace Native {
inline double uiTime() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
}
