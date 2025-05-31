#ifndef FLY_MATH_VEC_H
#define FLY_MATH_VEC_H

#include "core/assert.h"
#include "functions.h"

namespace Fly
{
namespace Math
{

struct Vec2;
struct Vec3;
struct Vec4;

struct Vec2
{
    union
    {
        struct
        {
            f32 x;
            f32 y;
        };
        f32 data[2];
    };

    inline Vec2(f32 v = 0.0f) : x(v), y(v) {}
    inline Vec2(f32 inX, f32 inY) : x(inX), y(inY) {}

    explicit Vec2(const Vec3& vec3);
    explicit Vec2(const Vec4& vec4);

    inline Vec2& operator+=(Vec2 rhs)
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }
    inline Vec2& operator-=(Vec2 rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }
    inline Vec2& operator*=(Vec2 rhs)
    {
        x *= rhs.x;
        y *= rhs.y;
        return *this;
    }
    inline Vec2& operator/=(Vec2 rhs)
    {
        x /= rhs.x;
        y /= rhs.y;
        return *this;
    }

    inline Vec2& operator*=(f32 rhs)
    {
        x *= rhs;
        y *= rhs;
        return *this;
    }
    inline Vec2& operator/=(f32 rhs)
    {
        x /= rhs;
        y /= rhs;
        return *this;
    }

    inline f32& operator[](i32 i)
    {
        FLY_ASSERT(i >= 0 && i < 2);
        return data[i];
    }
    inline const f32& operator[](i32 i) const
    {
        FLY_ASSERT(i >= 0 && i < 2);
        return data[i];
    }
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return Vec2(a.x + b.x, a.y + b.y); }
inline Vec2 operator-(Vec2 a, Vec2 b) { return Vec2(a.x - b.x, a.y - b.y); }
inline Vec2 operator*(Vec2 a, Vec2 b) { return Vec2(a.x * b.x, a.y * b.y); }
inline Vec2 operator/(Vec2 a, Vec2 b) { return Vec2(a.x / b.x, a.y / b.y); }

inline Vec2 operator*(Vec2 a, f32 b) { return Vec2(a.x * b, a.y * b); }
inline Vec2 operator*(f32 a, Vec2 b) { return Vec2(a * b.x, a * b.y); }
inline Vec2 operator/(Vec2 a, f32 b) { return Vec2(a.x / b, a.y / b); }

struct Vec3
{
    union
    {
        struct
        {
            f32 x;
            f32 y;
            f32 z;
        };
        f32 data[3];
    };

    inline Vec3(f32 v = 0.0f) : x(v), y(v), z(v) {}
    inline Vec3(f32 inX, f32 inY, f32 inZ) : x(inX), y(inY), z(inZ) {}
    inline Vec3(Vec2 xy, f32 inZ) : x(xy.x), y(xy.y), z(inZ) {}
    inline Vec3(f32 inX, Vec2 yz) : x(inX), y(yz.x), z(yz.y) {}
    inline explicit Vec3(const Vec4& vec4);

    inline Vec3& operator+=(Vec3 rhs)
    {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }
    inline Vec3& operator-=(Vec3 rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }
    inline Vec3& operator*=(Vec3 rhs)
    {
        x *= rhs.x;
        y *= rhs.y;
        z *= rhs.z;
        return *this;
    }
    inline Vec3& operator/=(Vec3 rhs)
    {
        x /= rhs.x;
        y /= rhs.y;
        z /= rhs.z;
        return *this;
    }

    inline Vec3& operator*=(f32 rhs)
    {
        x *= rhs;
        y *= rhs;
        z *= rhs;
        return *this;
    }
    inline Vec3& operator/=(f32 rhs)
    {
        x /= rhs;
        y /= rhs;
        z /= rhs;
        return *this;
    }

    inline f32& operator[](i32 i)
    {
        FLY_ASSERT(i >= 0 && i < 3);
        return data[i];
    }
    inline const f32& operator[](i32 i) const
    {
        FLY_ASSERT(i >= 0 && i < 3);
        return data[i];
    }
};

inline Vec3 operator+(Vec3 a, Vec3 b)
{
    return Vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}
inline Vec3 operator-(Vec3 a, Vec3 b)
{
    return Vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}
inline Vec3 operator*(Vec3 a, Vec3 b)
{
    return Vec3(a.x * b.x, a.y * b.y, a.z * b.z);
}
inline Vec3 operator/(Vec3 a, Vec3 b)
{
    return Vec3(a.x / b.x, a.y / b.y, a.z / b.z);
}

inline Vec3 operator*(Vec3 a, f32 b) { return Vec3(a.x * b, a.y * b, a.z * b); }
inline Vec3 operator*(f32 a, Vec3 b) { return Vec3(a * b.x, a * b.y, a * b.z); }
inline Vec3 operator/(Vec3 a, f32 b) { return Vec3(a.x / b, a.y / b, a.z / b); }

struct Vec4
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

    inline Vec4(f32 v = 0.0f) : x(v), y(v), z(v), w(v) {}
    inline Vec4(f32 inX, f32 inY, f32 inZ, f32 inW)
        : x(inX), y(inY), z(inZ), w(inW)
    {
    }
    inline Vec4(Vec2 xy, f32 inZ, f32 inW) : x(xy.x), y(xy.y), z(inZ), w(inW) {}
    inline Vec4(f32 inX, Vec2 yz, f32 inW) : x(inX), y(yz.x), z(yz.y), w(inW) {}
    inline Vec4(f32 inX, f32 inY, Vec2 zw) : x(inX), y(inY), z(zw.x), w(zw.y) {}
    inline Vec4(Vec2 xy, Vec2 zw) : x(xy.x), y(xy.y), z(zw.x), w(zw.y) {}
    inline Vec4(Vec3 xyz, f32 inW) : x(xyz.x), y(xyz.y), z(xyz.z), w(inW) {}
    inline Vec4(f32 inX, Vec3 yzw) : x(inX), y(yzw.x), z(yzw.y), w(yzw.z) {}

    inline Vec4& operator+=(Vec4 rhs)
    {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        w += rhs.w;
        return *this;
    }
    inline Vec4& operator-=(Vec4 rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        w -= rhs.w;
        return *this;
    }
    inline Vec4& operator*=(Vec4 rhs)
    {
        x *= rhs.x;
        y *= rhs.y;
        z *= rhs.z;
        w *= rhs.w;
        return *this;
    }
    inline Vec4& operator/=(Vec4 rhs)
    {
        x /= rhs.x;
        y /= rhs.y;
        z /= rhs.z;
        w /= rhs.w;
        return *this;
    }

    inline Vec4& operator*=(f32 rhs)
    {
        x *= rhs;
        y *= rhs;
        z *= rhs;
        w *= rhs;
        return *this;
    }
    inline Vec4& operator/=(f32 rhs)
    {
        x /= rhs;
        y /= rhs;
        z /= rhs;
        w /= rhs;
        return *this;
    }

    inline f32& operator[](i32 i)
    {
        FLY_ASSERT(i >= 0 && i < 4);
        return data[i];
    }
    inline const f32& operator[](i32 i) const
    {
        FLY_ASSERT(i >= 0 && i < 4);
        return data[i];
    }
};

inline Vec4 operator+(Vec4 a, Vec4 b)
{
    return Vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}
inline Vec4 operator-(Vec4 a, Vec4 b)
{
    return Vec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}
inline Vec4 operator*(Vec4 a, Vec4 b)
{
    return Vec4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}
inline Vec4 operator/(Vec4 a, Vec4 b)
{
    return Vec4(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w);
}

inline Vec4 operator*(Vec4 a, f32 b)
{
    return Vec4(a.x * b, a.y * b, a.z * b, a.w * b);
}
inline Vec4 operator*(f32 a, Vec4 b)
{
    return Vec4(a * b.x, a * b.y, a * b.z, a * b.w);
}
inline Vec4 operator/(Vec4 a, f32 b)
{
    return Vec4(a.x / b, a.y / b, a.z / b, a.w / b);
}

inline Vec2::Vec2(const Vec3& vec3) : x(vec3.x), y(vec3.y) {}
inline Vec2::Vec2(const Vec4& vec4) : x(vec4.x), y(vec4.y) {}
inline Vec3::Vec3(const Vec4& vec4) : x(vec4.x), y(vec4.y), z(vec4.z) {}

inline f32 LengthSqr(Vec2 v) { return v.x * v.x + v.y * v.y; }
inline f32 LengthSqr(Vec3 v) { return v.x * v.x + v.y * v.y + v.z * v.z; }
inline f32 LengthSqr(Vec4 v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
}

inline f32 Length(Vec2 v) { return Sqrt(v.x * v.x + v.y * v.y); }
inline f32 Length(Vec3 v) { return Sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
inline f32 Length(Vec4 v)
{
    return Sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
}

inline Vec2 Normalize(Vec2 v)
{
    if (Abs(v.x) < FLY_MATH_EPSILON && Abs(v.y) < FLY_MATH_EPSILON)
    {
        return Vec2(0.0f);
    }

    f32 invLength = InvSqrt(v.x * v.x + v.y * v.y);
    return invLength * v;
}

inline Vec3 Normalize(Vec3 v)
{
    if (Abs(v.x) < FLY_MATH_EPSILON && Abs(v.y) < FLY_MATH_EPSILON &&
        Abs(v.z) < FLY_MATH_EPSILON)
    {
        return Vec3(0.0f);
    }

    f32 invLength = InvSqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return invLength * v;
}

inline Vec4 Normalize(Vec4 v)
{
    if (Abs(v.x) < FLY_MATH_EPSILON && Abs(v.y) < FLY_MATH_EPSILON &&
        Abs(v.z) < FLY_MATH_EPSILON && Abs(v.w) < FLY_MATH_EPSILON)
    {
        return Vec4(0.0f);
    }
    f32 invLength = InvSqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
    return invLength * v;
}

inline f32 Dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline f32 Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline f32 Dot(Vec4 a, Vec4 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

inline Vec3 Cross(Vec3 a, Vec3 b)
{
    return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x);
}

inline Vec2 Min(Vec2 a, Vec2 b) { return Vec2(Min(a.x, b.x), Min(a.y, b.y)); }
inline Vec3 Min(Vec3 a, Vec3 b)
{
    return Vec3(Min(a.x, b.x), Min(a.y, b.y), Min(a.z, b.z));
}
inline Vec4 Min(Vec4 a, Vec4 b)
{
    return Vec4(Min(a.x, b.x), Min(a.y, b.y), Min(a.z, b.z), Min(a.w, b.w));
}

inline Vec2 Max(Vec2 a, Vec2 b) { return Vec2(Max(a.x, b.x), Max(a.y, b.y)); }
inline Vec3 Max(Vec3 a, Vec3 b)
{
    return Vec3(Max(a.x, b.x), Max(a.y, b.y), Max(a.z, b.z));
}
inline Vec4 Max(Vec4 a, Vec4 b)
{
    return Vec4(Max(a.x, b.x), Max(a.y, b.y), Max(a.z, b.z), Max(a.w, b.w));
}

} // namespace Math
} // namespace Fly

#endif /* End of FLY_MATH_VEC_H */
