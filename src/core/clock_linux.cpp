#include "clock.h"
#include <time.h>

namespace Hls
{

u64 ClockNow()
{
    thread_local struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    u64 now = static_cast<u64>(ts.tv_sec) * 1000000000ull +
              static_cast<u64>(ts.tv_nsec);
    return now;
}

} // namespace Hls
