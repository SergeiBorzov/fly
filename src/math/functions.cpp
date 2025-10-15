#include "functions.h"
#include "core/assert.h"

#include <math.h>

#define LUT_TABLE_SIZE 1024
#define LUT_VALUE_SCALE LUT_TABLE_SIZE / FLY_MATH_TWO_PI

thread_local u32 stSeed = 2025;

namespace Fly
{
namespace Math
{

f32 Ceil(f32 value) { return ceilf(value); }
f32 Floor(f32 value) { return floorf(value); }

f32 Abs(f32 value) { return fabsf(value); }
f32 Sqrt(f32 value) { return sqrtf(value); }

// John Carmack's Quake 3 Fast Inverse Sqrt
f32 InvSqrt(f32 value)
{
    if (value < MinF32())
    {
        return InfinityF32();
    }

    union
    {
        f32 f;
        u32 u;
    } i, v;

    v.f = value;
    i.u = 0x5F375A86 - (v.u >> 1);

    i.f = (0.5f * i.f) * (3.0f - value * i.f * i.f);
    i.f = (0.5f * i.f) * (3.0f - value * i.f * i.f);
    return i.f;
}

f32 Exp(f32 value) { return expf(value); }
f32 Sin(f32 radians) { return sinf(radians); }

f32 Cos(f32 radians) { return cosf(radians); }

f32 Tan(f32 radians) { return tanf(radians); }

f32 Atan2(f32 y, f32 x) { return atan2f(y, x); }

void SetRandomSeed(u32 seed) { stSeed = seed; }

u32 Rand()
{
    // xorshift32
    u32 x = stSeed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return stSeed = x;
}

i8 RandomI8(i8 min, i8 max)
{
    FLY_ASSERT(min <= max);
    return Rand() % (max - min + 1) + min;
}

i16 RandomI16(i16 min, i16 max)
{
    FLY_ASSERT(min <= max);
    return Rand() % (max - min + 1) + min;
}

i32 RandomI32(i32 min, i32 max)
{
    FLY_ASSERT(min <= max);
    return Rand() % (max - min + 1) + min;
}

i64 RandomI64(i64 min, i64 max)
{
    FLY_ASSERT(min <= max);
    return Rand() % (max - min + 1) + min;
}

u8 RandomU8(u8 min, u8 max)
{
    FLY_ASSERT(min <= max);
    return Rand() % (max - min + 1) + min;
}

u16 RandomU16(u16 min, u16 max)
{
    FLY_ASSERT(min <= max);
    return Rand() % (max - min + 1) + min;
}

u32 RandomU32(u32 min, u32 max)
{
    FLY_ASSERT(min <= max);
    return Rand() % (max - min + 1) + min;
}

u64 RandomU64(u64 min, u64 max)
{
    FLY_ASSERT(min <= max);
    return Rand() % (max - min + 1) + min;
}

f32 RandomF32(f32 min, f32 max)
{
    FLY_ASSERT(min <= max);
    return (max - min) * (static_cast<f32>(Rand()) / 0xFFFFFFFFu) + min;
}

f64 RandomF64(f64 min, f64 max)
{
    FLY_ASSERT(min <= max);
    return (max - min) * (static_cast<f32>(Rand()) / 0xFFFFFFFFu) + min;
}

} // namespace Math
} // namespace Fly
