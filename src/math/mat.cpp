#include "mat.h"
#include "functions.h"
#include "quat.h"

namespace Fly
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
    FLY_ASSERT(valueCount == 16);
    for (i32 i = 0; i < 16; i++)
    {
        data[i] = values[i];
    }
}

Mat4::Mat4(Math::Quat quat)
{
    f32 xx = quat.x * quat.x;
    f32 yy = quat.y * quat.y;
    f32 zz = quat.z * quat.z;
    f32 xy = quat.x * quat.y;
    f32 xz = quat.x * quat.z;
    f32 yz = quat.y * quat.z;
    f32 wx = quat.w * quat.x;
    f32 wy = quat.w * quat.y;
    f32 wz = quat.w * quat.z;

    data[0] = 1.0f - 2.0f * (yy + zz);
    data[1] = 2.0f * (xy + wz);
    data[2] = 2.0f * (xz - wy);
    data[3] = 0.0f;

    data[4] = 2.0f * (xy - wz);
    data[5] = 1.0f - 2.0f * (xx + zz);
    data[6] = 2.0f * (yz + wx);
    data[7] = 0.0f;

    data[8] = 2.0f * (xz + wy);
    data[9] = 2.0f * (yz - wx);
    data[10] = 1.0f - 2.0f * (xx + yy);
    data[11] = 0.0f;

    data[12] = 0.0f;
    data[13] = 0.0f;
    data[14] = 0.0f;
    data[15] = 1.0f;
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

Mat4 ScaleMatrix(Math::Vec3 v)
{
    Mat4 res;
    res.data[0] = v.x;
    res.data[5] = v.y;
    res.data[10] = v.z;
    return res;
}

Mat4 TranslationMatrix(Math::Vec3 v)
{
    Mat4 res;
    res.data[12] = v.x;
    res.data[13] = v.y;
    res.data[14] = v.z;
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
    res.data[6] = -s;
    res.data[9] = s;
    res.data[10] = c;

    return res;
}

Mat4 RotateY(f32 angle)
{
    f32 c = Math::Cos(Math::Radians(angle));
    f32 s = Math::Sin(Math::Radians(angle));

    Mat4 res;
    res.data[0] = c;
    res.data[2] = s;
    res.data[8] = -s;
    res.data[10] = c;

    return res;
}

Mat4 RotateZ(f32 angle)
{
    f32 c = Math::Cos(Math::Radians(angle));
    f32 s = Math::Sin(Math::Radians(angle));

    Mat4 res;
    res.data[0] = c;
    res.data[1] = -s;
    res.data[4] = s;
    res.data[5] = c;

    return res;
}

Mat4 Perspective(f32 fovxDegrees, f32 aspect, float near, float far)
{
    // Near and far are swapped to create a reverse-z buffer
    float rNear = far;
    float rFar = near;

    Mat4 res(0.0f);

    f32 hTanH = Tan(Radians(fovxDegrees) * 0.5f);
    f32 f = 1.0f / hTanH;
    f32 a = rFar / (rNear - rFar);

    res.data[0] = f;
    res.data[5] = -f * aspect;
    res.data[10] = a;
    res.data[11] = -1.0f;
    res.data[14] = rNear * a;

    return res;
}

Mat4 LookAt(Vec3 eye, Vec3 target, Vec3 worldUp)
{
    Mat4 res(0.0f);

    Vec3 z = Normalize(eye - target);
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

Mat4 Inverse(const Mat4& mat)
{
    f32 inv[16];
    const f32* m = mat.data;

    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
             m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];

    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] +
             m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] +
             m[12] * m[7] * m[10];

    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
             m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];

    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] +
              m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] +
              m[12] * m[6] * m[9];

    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] +
             m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] +
             m[13] * m[3] * m[10];

    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
             m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];

    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
             m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];

    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
              m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];

    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
             m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];

    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
             m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];

    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
              m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];

    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
              m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
             m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
             m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
              m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
              m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    f32 det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0)
    {
        return Mat4(0.0f);
    }

    det = 1.0f / det;

    for (int i = 0; i < 16; i++)
    {
        inv[i] *= det;
    }

    return Mat4(inv, 16);
}

} // namespace Math
} // namespace Fly
