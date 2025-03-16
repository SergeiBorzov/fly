#ifndef HLS_MATH_VEC_H
#define HLS_MATH_VEC_H

#include "core/assert.h"

namespace Hls
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

    inline f32& operator[](i32 i)
    {
        HLS_ASSERT(i >= 0 && i < 2);
        return data[i];
    }

    inline const f32& operator[](i32 i) const
    {
        HLS_ASSERT(i >= 0 && i < 2);
        return data[i];
    }
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return Vec2(a.x + b.x, a.y + b.y); }
inline Vec2 operator-(Vec2 a, Vec2 b) { return Vec2(a.x - b.x, a.y - b.y); }
inline Vec2 operator*(Vec2 a, Vec2 b) { return Vec2(a.x * b.x, a.y * b.y); }
inline Vec2 operator/(Vec2 a, Vec2 b) { return Vec2(a.x / b.x, a.y / b.y); }

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

    inline f32& operator[](i32 i)
    {
        HLS_ASSERT(i >= 0 && i < 3);
        return data[i];
    }
    inline const f32& operator[](i32 i) const
    {
        HLS_ASSERT(i >= 0 && i < 3);
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

    inline f32& operator[](i32 i)
    {
        HLS_ASSERT(i >= 0 && i < 4);
        return data[i];
    }
    inline const f32& operator[](i32 i) const
    {
        HLS_ASSERT(i >= 0 && i < 4);
        return data[i];
    }
};

inline Vec4 operator+(Vec4 a, Vec4 b)
{
    return Vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.z);
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

inline Vec2::Vec2(const Vec3& vec3) : x(vec3.x), y(vec3.y) {}
inline Vec2::Vec2(const Vec4& vec4) : x(vec4.x), y(vec4.y) {}
inline Vec3::Vec3(const Vec4& vec4) : x(vec4.x), y(vec4.y), z(vec4.z) {}

inline f32 Dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline f32 Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline f32 Dot(Vec4 a, Vec4 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

} // namespace Math
} // namespace Hls

#endif /* End of HLS_MATH_VEC_H */
