#ifndef FLY_CLOCK_H
#define FLY_CLOCK_H

#include "types.h"

namespace Fly
{

u64 ClockNow(); // Current system time in nanoseconds

inline f64 ToSeconds(u64 nanoseconds) { return nanoseconds / 1000000000.0; }
inline f64 ToMilliseconds(u64 nanoseconds) { return nanoseconds / 1000000.0; }

} // namespace Fly

#endif /* End of FLY_CLOCK_H */
