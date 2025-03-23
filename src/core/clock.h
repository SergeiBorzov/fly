#ifndef HLS_CLOCK_H
#define HLS_CLOCK_H

#include "types.h"

namespace Hls
{

u64 ClockNow(); // Current system time in nanoseconds

inline f64 ToSeconds(u64 nanoseconds) { return nanoseconds / 1000000000.0; }

} // namespace Hls

#endif /* End of HLS_CLOCK_H */
