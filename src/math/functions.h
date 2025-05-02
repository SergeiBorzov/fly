#ifndef HLS_MATH_FUNCTIONS_H
#define HLS_MATH_FUNCTIONS_H

#include "core/types.h"

#define HLS_MATH_EPSILON (1e-5)
#define HLS_MATH_PI (3.1415926f)
#define HLS_MATH_TWO_PI (6.2831853f)
#define HLS_MATH_HALF_PI (1.5707963f)

namespace Hls
{
namespace Math
{

inline f32 Degrees(f32 radians) { return radians / HLS_MATH_PI * 180.0f; }
inline f32 Radians(f32 degrees) { return degrees / 180.0f * HLS_MATH_PI; }
inline f32 Min(f32 a, f32 b) { return (a < b) ? a : b; }
inline f32 Max(f32 a, f32 b) { return (a > b) ? a : b; }
inline f32 Clamp(f32 value, f32 min, f32 max)
{
    return Max(min, Min(value, max));
}

f32 Ceil(f32 value);
f32 Floor(f32 value);

f32 Abs(f32 value);
f32 Sqrt(f32 value);
f32 InvSqrt(f32 value);

f32 Sin(f32 radians);
f32 Cos(f32 radians);
f32 Tan(f32 radians);

void SetRandomSeed(u32 seed);
u32 Rand();

i8 RandomI8(i8 min, i8 max);
i16 RandomI16(i16 min, i16 max);
i32 RandomI32(i32 min, i32 max);
i64 RandomI64(i64 min, i64 max);

u8 RandomU8(u8 min, u8 max);
u16 RandomU16(u16 min, u16 max);
u32 RandomU32(u32 min, u32 max);
u64 RandomU64(u64 min, u64 max);

f32 RandomF32(f32 min, f32 max);
f64 RandomF64(f64 min, f64 max);

} // namespace Math
} // namespace Hls

#endif /* HLS_MATH_MATH_H */
