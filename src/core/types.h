#ifndef FLY_TYPES_H
#define FLY_TYPES_H

#include <stdint.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

#define FLY_MAX_U8 0xFFu
#define FLY_MAX_U16 0xFFFFu
#define FLY_MAX_U32 0xFFFFFFFFu
#define FLY_MAX_U64 0xFFFFFFFFFFFFFFFFu

struct f16
{
    static u16 QuantizeHalf(f32 f);
    static f32 DequantizeHalf(f16 h);

    inline explicit f16(u16 bits) : data(bits) {}
    inline f16(f32 value = 0.0f) : data(QuantizeHalf(value)) {}
    operator f32() const { return DequantizeHalf(*this); }

    u16 data = 0;

    inline f16& operator+=(f16 rhs)
    {
        f32 tmp = DequantizeHalf(*this) + DequantizeHalf(rhs);
        data = QuantizeHalf(tmp);
        return *this;
    }

    inline f16& operator+=(f32 rhs)
    {
        f32 tmp = DequantizeHalf(data);
        tmp += rhs;
        data = QuantizeHalf(tmp);
        return *this;
    }

    inline f16& operator-=(f16 rhs)
    {
        f32 tmp = DequantizeHalf(*this) - DequantizeHalf(rhs);
        data = QuantizeHalf(tmp);
        return *this;
    }

    inline f16& operator-=(f32 rhs)
    {
        f32 tmp = DequantizeHalf(data);
        tmp -= rhs;
        data = QuantizeHalf(tmp);
        return *this;
    }

    inline f16& operator*=(f16 rhs)
    {
        f32 tmp = DequantizeHalf(*this) * DequantizeHalf(rhs);
        data = QuantizeHalf(tmp);
        return *this;
    }

    inline f16& operator*=(f32 rhs)
    {
        f32 tmp = DequantizeHalf(data);
        tmp *= rhs;
        data = QuantizeHalf(tmp);
        return *this;
    }

    inline f16& operator/=(f16 rhs)
    {
        f32 tmp = DequantizeHalf(*this) / DequantizeHalf(rhs);
        data = QuantizeHalf(tmp);
        return *this;
    }

    inline f16& operator/=(f32 rhs)
    {
        f32 tmp = DequantizeHalf(data);
        tmp /= rhs;
        data = QuantizeHalf(tmp);
        return *this;
    }
};

inline f16 operator+(f16 a) { return a; }
inline f16 operator-(f16 a)
{
    return f16(static_cast<u16>(a.data ^ (1u << 15)));
}
inline f16 operator+(f16 a, f16 b)
{
    return f16(
        f16::QuantizeHalf(f16::DequantizeHalf(a) + f16::DequantizeHalf(b)));
}
inline f32 operator+(f16 a, f32 b) { return f16::DequantizeHalf(a) + b; }
inline f32 operator+(f32 a, f16 b) { return a + f16::DequantizeHalf(b); }

inline f16 operator-(f16 a, f16 b)
{
    return f16(
        f16::QuantizeHalf(f16::DequantizeHalf(a) - f16::DequantizeHalf(b)));
}
inline f32 operator-(f16 a, f32 b) { return f16::DequantizeHalf(a) - b; }
inline f32 operator-(f32 a, f16 b) { return a - f16::DequantizeHalf(b); }

inline f16 operator*(f16 a, f16 b)
{
    return f16(
        f16::QuantizeHalf(f16::DequantizeHalf(a) * f16::DequantizeHalf(b)));
}
inline f32 operator*(f16 a, f32 b) { return f16::DequantizeHalf(a) * b; }
inline f32 operator*(f32 a, f16 b) { return a * f16::DequantizeHalf(b); }

inline f16 operator/(f16 a, f16 b)
{
    return f16(
        f16::QuantizeHalf(f16::DequantizeHalf(a) / f16::DequantizeHalf(b)));
}
inline f32 operator/(f16 a, f32 b) { return f16::DequantizeHalf(a) / b; }
inline f32 operator/(f32 a, f16 b) { return a / f16::DequantizeHalf(b); }

inline f32 MinF32()
{
    union
    {
        f32 f;
        u32 u;
    } v;
    v.u = 0x00800000;
    return v.f;
}

inline f32 MaxF32()
{
    union
    {
        f32 f;
        u32 u;
    } v;
    v.u = 0x7f7fffff;
    return v.f;
}

inline f32 InfinityF32()
{
    union
    {
        f32 f;
        u32 u;
    } v;
    v.u = 0x7f800000;
    return v.f;
}

inline f32 MinusInfinityF32()
{
    union
    {
        f32 f;
        u32 u;
    } v;
    v.u = 0xff800000;
    return v.f;
}

#define STACK_ARRAY_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))

#endif /* End of FLY_TYPES_H */
