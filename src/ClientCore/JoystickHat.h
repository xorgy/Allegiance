#pragma once

namespace Allegiance {
// TrekInputImpl's four virtual buttons. Exact boundaries matter for diagonals;
// a centered DirectInput POV is -2 and activates no button.
inline int JoystickHatButton(float value) {
    if(value>.75f)return 2;
    if(value>.25f)return 1;
    if(value>-.25f)return 0;
    if(value>-.75f)return 3;
    if(value>-1.25f)return 2;
    return -1;
}
}
