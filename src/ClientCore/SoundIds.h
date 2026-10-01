// Original sound identifiers, usable without the IGC/platform interface.
#pragma once
typedef short   SoundID;
typedef short   VoiceID;

#define DEFSOUND(id) id##Sound,

//removed sounds should still create an entry in the enum
#define DEFEMPTYSOUND(id) id##Sound,
enum {
    lastDynamicSound = 1000,
#include "../Igc/sounds.h"
};
#undef DEFSOUND

//further mentions of DEFEMPTYSOUND should do nothing
#undef DEFEMPTYSOUND
#define DEFEMPTYSOUND(id)

