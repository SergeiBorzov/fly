#include "mat.h"
#include "functions.h"

namespace Hls
{
namespace Math
{

Mat4::Mat4(f32 value)
{
    for (i32 i = 0; i < 16; i++)
    {
        data[i] = 0.0f;
    }
    data[0] = value;
    data[5] = value;
    data[10] = value;
    data[15] = value;
}

Mat4::Mat4(const f32* values, u32 valueCount)
{
    HLS_ASSERT(valueCount == 16);
    for (i32 i = 0; i < 16; i++)
    {
        data[i] = values[i];
    }
}

Mat4& Mat4::operator+=(const Mat4& rhs)
{
    for (i32 i = 0; i < 16; i++)
    {
        data[i] += rhs.data[i];
    }
    return *this;
}

Mat4& Mat4::operator-=(const Mat4& rhs)
{
    for (i32 i = 0; i < 16; i++)
    {
        data[i] -= rhs.data[i];
    }
    return *this;
}

Mat4& Mat4::operator*=(const Mat4& rhs)
{
    Mat4 tmp(0.0f);
    for (i32 c = 0; c < 4; c++)
    {
        for (i32 r = 0; r < 4; r++)
        {
            tmp.data[c * 4 + r] = data[0 * 4 + r] * rhs.data[c * 4 + 0] +
                                  data[1 * 4 + r] * rhs.data[c * 4 + 1] +
                                  data[2 * 4 + r] * rhs.data[c * 4 + 2] +
                                  data[3 * 4 + r] * rhs.data[c * 4 + 3];
        }
    }
    *this = tmp;
    return *this;
}

Mat4 operator+(const Mat4& lhs, const Mat4& rhs)
{
    Mat4 res;
    for (int i = 0; i < 16; i++)
    {
        res.data[i] = lhs.data[i] + rhs.data[i];
    }
    return res;
}

Mat4 operator-(const Mat4& lhs, const Mat4& rhs)
{
    Mat4 res;
    for (int i = 0; i < 16; i++)
    {
        res.data[i] = lhs.data[i] - rhs.data[i];
    }
    return res;
}

Mat4 operator*(const Mat4& lhs, const Mat4& rhs)
{
    Mat4 result(0.0f);
    for (i32 c = 0; c < 4; c++)
    {
        for (i32 r = 0; r < 4; r++)
        {
            result.data[c * 4 + r] = lhs.data[0 * 4 + r] * rhs.data[c * 4 + 0] +
                                     lhs.data[1 * 4 + r] * rhs.data[c * 4 + 1] +
                                     lhs.data[2 * 4 + r] * rhs.data[c * 4 + 2] +
                                     lhs.data[3 * 4 + r] * rhs.data[c * 4 + 3];
        }
    }
    return result;
}

Vec4 operator*(const Mat4& lhs, Vec4 rhs)
{
    Vec4 res;
    for (i32 r = 0; r < 4; r++)
    {
        f32 sum = 0.0f;
        for (i32 c = 0; c < 4; c++)
        {
            sum += lhs.data[4 * c + r] * rhs.data[c];
        }
        res.data[r] = sum;
    }
    return res;
}

Mat4 ScaleMatrix(f32 x, f32 y, f32 z)
{
    Mat4 res;
    res.data[0] = x;
    res.data[5] = y;
    res.data[10] = z;
    return res;
}

Mat4 TranslationMatrix(f32 x, f32 y, f32 z)
{
    Mat4 res;
    res.data[12] = x;
    res.data[13] = y;
    res.data[14] = z;
    return res;
}

Mat4 RotateX(f32 angle)
{
    f32 c = Math::Cos(Math::Radians(angle));
    f32 s = Math::Sin(Math::Radians(angle));

    Mat4 res;
    res.data[0] = 1.0f;
    res.data[5] = c;
    res.data[6] = s;
    res.data[9] = -s;
    res.data[10] = c;

    return res;
}

Mat4 RotateY(f32 angle)
{
    f32 c = Math::Cos(Math::Radians(angle));
    f32 s = Math::Sin(Math::Radians(angle));

    Mat4 res;
    res.data[0] = c;
    res.data[2] = -s;
    res.data[8] = s;
    res.data[10] = c;

    return res;
}

Mat4 RotateZ(f32 angle)
{
    f32 c = Math::Cos(Math::Radians(angle));
    f32 s = Math::Sin(Math::Radians(angle));

    Mat4 res;
    res.data[0] = c;
    res.data[1] = s;
    res.data[4] = -s;
    res.data[5] = c;

    return res;
}

Mat4 Perspective(f32 fovx_degrees, f32 aspect, float near, float far)
{
    Mat4 res(0.0f);

    f32 hTanH = Tan(Radians(fovx_degrees) * 0.5f);
    f32 f = 1.0f / hTanH;
    f32 a = far / (far - near);

    res.data[0] = f;
    res.data[5] = -f * aspect;
    res.data[10] = a;
    res.data[11] = 1.0f;
    res.data[14] = -near * a;

    return res;
}

Mat4 LookAt(Vec3 eye, Vec3 target, Vec3 worldUp)
{
    Mat4 res(0.0f);

    Vec3 z = Normalize(target - eye);
    Vec3 y = worldUp;
    Vec3 x = Normalize(Cross(y, z));
    y = Normalize(Cross(z, x));

    res.data[0] = x.x;
    res.data[1] = y.x;
    res.data[2] = z.x;

    res.data[4] = x.y;
    res.data[5] = y.y;
    res.data[6] = z.y;

    res.data[8] = x.z;
    res.data[9] = y.z;
    res.data[10] = z.z;

    res.data[12] = -Dot(x, eye);
    res.data[13] = -Dot(y, eye);
    res.data[14] = -Dot(z, eye);
    res.data[15] = 1.0f;

    return res;
}

} // namespace Math
} // namespace Hls
