#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/clock.h"
#include "math/functions.h"

#define COUNT 400000

void BenchmarkRand()
{
    u64 hlsStart = Hls::ClockNow();
    for (i32 i = 0; i < COUNT; i++)
    {
        volatile u32 r = Hls::Math::Rand();
    }
    u64 hlsEnd = Hls::ClockNow();

    u64 stdlibStart = Hls::ClockNow();
    for (i32 i = 0; i < COUNT; i++)
    {
        volatile int r = rand();
    }
    u64 stdlibEnd = Hls::ClockNow();

    u64 hlsTime = hlsEnd - hlsStart;
    u64 stdlibTime = stdlibEnd - stdlibStart;

    printf("Hls - Rand() - %f s\n", Hls::ToSeconds(hlsTime));
    printf("stdlib - rand() - %f s\n", Hls::ToSeconds(stdlibTime));

    if (hlsTime < stdlibTime)
    {
        printf("Hls Rand() is %f times faster than stdlib one\n\n",
               Hls::ToSeconds(stdlibTime) / Hls::ToSeconds(hlsTime));
    }
    else
    {
        printf("Stdlib rand() is %f times faster than Hls one\n\n",
               Hls::ToSeconds(hlsTime) / Hls::ToSeconds(stdlibTime));
    }
}

void BenchmarkSin()
{
    u64 hlsStart = Hls::ClockNow();
    for (i32 i = 0; i < COUNT; i++)
    {
        volatile f32 s = Hls::Math::Sin(static_cast<f32>(i));
    }
    u64 hlsEnd = Hls::ClockNow();

    u64 stdlibStart = Hls::ClockNow();
    for (i32 i = 0; i < COUNT; i++)
    {
        volatile f32 s = sinf(static_cast<f32>(i));
    }
    u64 stdlibEnd = Hls::ClockNow();

    u64 hlsTime = hlsEnd - hlsStart;
    u64 stdlibTime = stdlibEnd - stdlibStart;

    printf("Hls - Sin() - %f s\n", Hls::ToSeconds(hlsTime));
    printf("cmath - sin() - %f s\n", Hls::ToSeconds(stdlibTime));

    if (hlsTime < stdlibTime)
    {
        printf("Hls Sin() is %f times faster than cmath one\n\n",
               Hls::ToSeconds(stdlibTime) / Hls::ToSeconds(hlsTime));
    }
    else
    {
        printf("cmath sin() is %f times faster than Hls one\n\n",
               Hls::ToSeconds(hlsTime) / Hls::ToSeconds(stdlibTime));
    }
}

void BenchmarkCos()
{
    u64 hlsStart = Hls::ClockNow();
    for (i32 i = 0; i < COUNT; i++)
    {
        volatile f32 c = Hls::Math::Cos(static_cast<f32>(i));
    }
    u64 hlsEnd = Hls::ClockNow();

    u64 stdlibStart = Hls::ClockNow();
    for (i32 i = 0; i < COUNT; i++)
    {
        volatile f32 c = cosf(static_cast<f32>(i));
    }
    u64 stdlibEnd = Hls::ClockNow();

    u64 hlsTime = hlsEnd - hlsStart;
    u64 stdlibTime = stdlibEnd - stdlibStart;

    printf("Hls - Cos() - %f s\n", Hls::ToSeconds(hlsTime));
    printf("cmath - cos() - %f s\n", Hls::ToSeconds(stdlibTime));

    if (hlsTime < stdlibTime)
    {
        printf("Hls Cos() is %f times faster than cmath one\n\n",
               Hls::ToSeconds(stdlibTime) / Hls::ToSeconds(hlsTime));
    }
    else
    {
        printf("cmath cos() is %f times faster than Hls one\n\n",
               Hls::ToSeconds(hlsTime) / Hls::ToSeconds(stdlibTime));
    }
}

void BenchmarkInvSqrt()
{
    u64 hlsStart = Hls::ClockNow();
    for (i32 i = 1; i < COUNT; i++)
    {
        volatile f32 c = Hls::Math::InvSqrt(static_cast<f32>(i));
    }
    u64 hlsEnd = Hls::ClockNow();

    u64 stdlibStart = Hls::ClockNow();
    for (i32 i = 1; i < COUNT; i++)
    {
        volatile f32 c = 1.0f/sqrtf(static_cast<f32>(i));
    }
    u64 stdlibEnd = Hls::ClockNow();

    u64 hlsTime = hlsEnd - hlsStart;
    u64 stdlibTime = stdlibEnd - stdlibStart;

    printf("Hls - InvSqrt() - %f s\n", Hls::ToSeconds(hlsTime));
    printf("cmath - 1.0f/sqrtf() - %f s\n", Hls::ToSeconds(stdlibTime));

    if (hlsTime < stdlibTime)
    {
        printf("Hls InvSqrt() is %f times faster than cmath one\n\n",
               Hls::ToSeconds(stdlibTime) / Hls::ToSeconds(hlsTime));
    }
    else
    {
        printf("cmath 1.0f/sqrtf() is %f times faster than Hls one\n\n",
               Hls::ToSeconds(hlsTime) / Hls::ToSeconds(stdlibTime));
    }
}

int main()
{
    BenchmarkRand();
    BenchmarkSin();
    BenchmarkCos();
    BenchmarkInvSqrt();
    return 0;
}
