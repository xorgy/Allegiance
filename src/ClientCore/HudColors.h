#pragma once
#include <cstdint>
namespace Allegiance {
// WinTrek's shadow uses Color(45,36,59), followed by 0.65 brightness and
// D3DRGBA's unmasked integer packing. Preserve the resulting displayed bytes;
// interpreting those constants as normalized RGB produces a different HUD.
inline constexpr uint32_t HudShadowArgb=(uint32_t(.55f*255)<<24)|(uint32_t((45.f*.65f)*255)<<16)|
    (uint32_t((36.f*.65f)*255)<<8)|uint32_t((59.f*.65f)*255);
}
