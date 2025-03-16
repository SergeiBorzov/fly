#ifndef HLS_MATH_FUNCTIONS_H
#define HLS_MATH_FUNCTIONS_H

#include "core/types.h"

#define HLS_MATH_PI (3.1415926f)
#define HLS_MATH_TWO_PI (6.2831853f)
#define HLS_MATH_HALF_PI (1.5707963f)

namespace Hls
{
namespace Math
{

inline f32 Degrees(f32 radians) { return radians / HLS_MATH_PI * 180.0f; }
inline f32 Radians(f32 degrees) { return degrees / 180.0f * HLS_MATH_PI; }

f32 Sin(f32 radians);
f32 Cos(f32 radians);

} // namespace Math
} // namespace Hls

#endif /* End of HLS_MATH_MATH_H */
