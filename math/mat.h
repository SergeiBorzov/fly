#ifndef HLS_MATH_MAT_H
#define HLS_MATH_MAT_H

#include "core/assert.h"
#include "vec.h"

namespace Hls
{
namespace Math
{

struct Mat4
{
    union
    {
        Vec4 columns[4];
        f32 data[16];
    };

    Mat4(f32 value = 1.0f);
    Mat4(const f32* values, u32 valueCount);

    Mat4& operator+=(const Mat4& rhs);
    Mat4& operator-=(const Mat4& rhs);
    Mat4& operator*=(const Mat4& rhs);

    inline Vec4& operator[](i32 i)
    {
        HLS_ASSERT(i >= 0 && i < 4);
        return columns[i];
    }

    inline const Vec4& operator[](i32 i) const
    {
        HLS_ASSERT(i >= 0 && i < 4);
        return columns[i];
    }
};

Mat4 operator+(const Mat4& lhs, const Mat4& rhs);
Mat4 operator-(const Mat4& lhs, const Mat4& rhs);
Mat4 operator*(const Mat4& lhs, const Mat4& rhs);

Vec4 operator*(const Mat4& lhs, const Vec4& rhs);

} // namespace Math
} // namespace Hls

#endif /* HLS_MATH_MAT_H */
