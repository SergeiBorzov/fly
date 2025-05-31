#include "clock.h"
#include <windows.h>

namespace Fly
{

u64 ClockNow()
{
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
}

} // namespace Fly
