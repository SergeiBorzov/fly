#include "clock.h"
#include <mach/mach_time.h>

namespace Hls
{

u64 ClockNow()
{
    thread_local bool isInitialized = false;
    thread_local mach_timebase_info_data_t info;
    if (!isInitialized)
    {
        mach_timebase_info(&info);
        isInitialized = 1;
    }
    u64 now = mach_absolute_time();
    now *= info.numer;
    now /= info.denom;
    return now;
}

} // namespace Hls
