#include "clock.h"
#include "platform.h"

#ifdef HLS_PLATFORM_OS_MAC_OSX
#include <mach/mach_time.h>
#elif defined(HLS_PLATFORM_OS_WINDOWS)
#include <Windows.h>
#endif

namespace Hls
{

u64 ClockNow()
{
#ifdef HLS_PLATFORM_OS_MAC_OSX
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
#elif defined(HLS_PLATFORM_OS_WINDOWS)
    thread_local bool isInitialized = false;
    thread_local LARGE_INTEGER frequency;
    if (!isInitialized)
    {
        QueryPerformanceFrequency(&frequency);
        isInitialized = true;
    }

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    u64 now = counter.QuadPart;
    now *= 1000000000;
    now /= frequency.QuadPart;
    return now;
#endif
}

} // namespace Hls
