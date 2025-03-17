#include "mat.h"

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

Vec4 operator*(const Mat4& lhs, const Vec4& rhs)
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

} // namespace Math
} // namespace Hls
