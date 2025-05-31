#ifndef FLY_MATH_QUAT_H
#define FLY_MATH_QUAT_H

#include "core/assert.h"

#include "vec.h"

namespace Fly
{

namespace Math
{

struct Quat
{
    union
    {
        struct
        {
            f32 x;
            f32 y;
            f32 z;
            f32 w;
        };
        f32 data[4];
    };

    inline Quat() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
    inline Quat(f32 inX, f32 inY, f32 inZ, f32 inW)
        : x(inX), y(inY), z(inZ), w(inW)
    {
    }

    inline Quat operator*(Quat rhs)
    {
        f32 nx = w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y;
        f32 ny = w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x;
        f32 nz = w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w;
        f32 nw = w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z;

        return Quat(nx, ny, nz, nw);
    }

    inline Vec3 operator*(Vec3 rhs)
    {
        Quat v = Quat(rhs.x, rhs.y, rhs.z, 0.0);
        Quat conj = Quat(-x, -y, -z, w);
        Quat res = (*this) * v * conj;
        return Vec3(res.x, res.y, res.z);
    }
};

inline Quat operator*(Quat a, f32 b)
{
    return Quat(a.x * b, a.y * b, a.z * b, a.w * b);
}

inline Quat operator*(f32 a, Quat b)
{
    return Quat(a * b.x, a * b.y, a * b.z, a * b.w);
}

inline Quat Conjugate(Quat q) { return Quat(-q.x, -q.y, -q.z, q.w); }

inline Quat Normalize(Quat v)
{
    if (Abs(v.x) < FLY_MATH_EPSILON && Abs(v.y) < FLY_MATH_EPSILON &&
        Abs(v.z) < FLY_MATH_EPSILON && Abs(v.w) < FLY_MATH_EPSILON)
    {
        return Quat(0.0f, 0.0f, 0.0f, 0.0f);
    }
    f32 invLength = InvSqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
    return invLength * v;
}

inline Quat AngleAxis(Vec3 axis, f32 degrees)
{
    Vec3 normAxis = Normalize(axis);
    f32 hAngle = Radians(degrees) * 0.5f;
    f32 s = Sin(hAngle);
    f32 c = Cos(hAngle);

    return Quat(normAxis.x * s, normAxis.y * s, normAxis.z * s, c);
}

} // namespace Math

} // namespace Fly

#endif /* FLY_MATH_QUAT_H */
