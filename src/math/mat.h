#ifndef FLY_MATH_MAT_H
#define FLY_MATH_MAT_H

#include "core/assert.h"
#include "vec.h"

namespace Fly
{
namespace Math
{

struct Quat;

struct Mat4
{
    union
    {
        Vec4 columns[4];
        f32 data[16];
    };

    Mat4(f32 value = 1.0f);
    Mat4(const f32* values, u32 valueCount);
    Mat4(Quat quat);

    Mat4& operator+=(const Mat4& rhs);
    Mat4& operator-=(const Mat4& rhs);
    Mat4& operator*=(const Mat4& rhs);

    inline Vec4& operator[](i32 i)
    {
        FLY_ASSERT(i >= 0 && i < 4);
        return columns[i];
    }

    inline const Vec4& operator[](i32 i) const
    {
        FLY_ASSERT(i >= 0 && i < 4);
        return columns[i];
    }
};

Mat4 operator+(const Mat4& lhs, const Mat4& rhs);
Mat4 operator-(const Mat4& lhs, const Mat4& rhs);
Mat4 operator*(const Mat4& lhs, const Mat4& rhs);

Vec4 operator*(const Mat4& lhs, Vec4 rhs);

Mat4 ScaleMatrix(f32 x, f32 y, f32 z);
Mat4 ScaleMatrix(Math::Vec3 scale);
Mat4 TranslationMatrix(f32 x, f32 y, f32 z);
Mat4 TranslationMatrix(Math::Vec3 translate);
Mat4 RotateX(f32 degrees);
Mat4 RotateY(f32 degrees);
Mat4 RotateZ(f32 degrees);

Mat4 Perspective(f32 fovx, f32 aspect, f32 near, f32 far);
Mat4 LookAt(Vec3 eye, Vec3 target, Vec3 worldUp);

Mat4 Inverse(const Mat4& mat);

} // namespace Math
} // namespace Fly

#endif /* FLY_MATH_MAT_H */
