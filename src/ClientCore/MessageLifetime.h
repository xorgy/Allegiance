#pragma once
namespace Allegiance {
// ConsoleData expires critical tips; ordinary tips and chat history remain
// until replaced. The display clock is independent of paused simulation time.
inline float CriticalTipDuration(bool training){return training?30.0f:7.5f;}
inline bool MessageVisible(bool critical,bool training,double age){return !critical || age<CriticalTipDuration(training);}
}
