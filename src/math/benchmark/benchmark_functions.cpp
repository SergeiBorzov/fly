#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/clock.h"
#include "math/functions.h"

#define COUNT 400000

void BenchmarkRand()
{
    u64 flyStart = Fly::ClockNow();
    for (i32 i = 0; i < COUNT; i++)
    {
        volatile u32 r = Fly::Math::Rand();
    }
    u64 flyEnd = Fly::ClockNow();

    u64 stdlibStart = Fly::ClockNow();
    for (i32 i = 0; i < COUNT; i++)
    {
        volatile int r = rand();
    }
    u64 stdlibEnd = Fly::ClockNow();

    u64 flyTime = flyEnd - flyStart;
    u64 stdlibTime = stdlibEnd - stdlibStart;

    printf("Fly - Rand() - %f s\n", Fly::ToSeconds(flyTime));
    printf("stdlib - rand() - %f s\n", Fly::ToSeconds(stdlibTime));

    if (flyTime < stdlibTime)
    {
        printf("Fly Rand() is %f times faster than stdlib one\n\n",
               Fly::ToSeconds(stdlibTime) / Fly::ToSeconds(flyTime));
    }
    else
    {
        printf("Stdlib rand() is %f times faster than Fly one\n\n",
               Fly::ToSeconds(flyTime) / Fly::ToSeconds(stdlibTime));
    }
}

void BenchmarkSin()
{
    u64 flyStart = Fly::ClockNow();
    for (i32 i = 0; i < COUNT; i++)
    {
        volatile f32 s = Fly::Math::Sin(static_cast<f32>(i));
    }
    u64 flyEnd = Fly::ClockNow();

    u64 stdlibStart = Fly::ClockNow();
    for (i32 i = 0; i < COUNT; i++)
    {
        volatile f32 s = sinf(static_cast<f32>(i));
    }
    u64 stdlibEnd = Fly::ClockNow();

    u64 flyTime = flyEnd - flyStart;
    u64 stdlibTime = stdlibEnd - stdlibStart;

    printf("Fly - Sin() - %f s\n", Fly::ToSeconds(flyTime));
    printf("cmath - sin() - %f s\n", Fly::ToSeconds(stdlibTime));

    if (flyTime < stdlibTime)
    {
        printf("Fly Sin() is %f times faster than cmath one\n\n",
               Fly::ToSeconds(stdlibTime) / Fly::ToSeconds(flyTime));
    }
    else
    {
        printf("cmath sin() is %f times faster than Fly one\n\n",
               Fly::ToSeconds(flyTime) / Fly::ToSeconds(stdlibTime));
    }
}

void BenchmarkCos()
{
    u64 flyStart = Fly::ClockNow();
    for (i32 i = 0; i < COUNT; i++)
    {
        volatile f32 c = Fly::Math::Cos(static_cast<f32>(i));
    }
    u64 flyEnd = Fly::ClockNow();

    u64 stdlibStart = Fly::ClockNow();
    for (i32 i = 0; i < COUNT; i++)
    {
        volatile f32 c = cosf(static_cast<f32>(i));
    }
    u64 stdlibEnd = Fly::ClockNow();

    u64 flyTime = flyEnd - flyStart;
    u64 stdlibTime = stdlibEnd - stdlibStart;

    printf("Fly - Cos() - %f s\n", Fly::ToSeconds(flyTime));
    printf("cmath - cos() - %f s\n", Fly::ToSeconds(stdlibTime));

    if (flyTime < stdlibTime)
    {
        printf("Fly Cos() is %f times faster than cmath one\n\n",
               Fly::ToSeconds(stdlibTime) / Fly::ToSeconds(flyTime));
    }
    else
    {
        printf("cmath cos() is %f times faster than Fly one\n\n",
               Fly::ToSeconds(flyTime) / Fly::ToSeconds(stdlibTime));
    }
}

void BenchmarkInvSqrt()
{
    u64 flyStart = Fly::ClockNow();
    for (i32 i = 1; i < COUNT; i++)
    {
        volatile f32 c = Fly::Math::InvSqrt(static_cast<f32>(i));
    }
    u64 flyEnd = Fly::ClockNow();

    u64 stdlibStart = Fly::ClockNow();
    for (i32 i = 1; i < COUNT; i++)
    {
        volatile f32 c = 1.0f/sqrtf(static_cast<f32>(i));
    }
    u64 stdlibEnd = Fly::ClockNow();

    u64 flyTime = flyEnd - flyStart;
    u64 stdlibTime = stdlibEnd - stdlibStart;

    printf("Fly - InvSqrt() - %f s\n", Fly::ToSeconds(flyTime));
    printf("cmath - 1.0f/sqrtf() - %f s\n", Fly::ToSeconds(stdlibTime));

    if (flyTime < stdlibTime)
    {
        printf("Fly InvSqrt() is %f times faster than cmath one\n\n",
               Fly::ToSeconds(stdlibTime) / Fly::ToSeconds(flyTime));
    }
    else
    {
        printf("cmath 1.0f/sqrtf() is %f times faster than Fly one\n\n",
               Fly::ToSeconds(flyTime) / Fly::ToSeconds(stdlibTime));
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
