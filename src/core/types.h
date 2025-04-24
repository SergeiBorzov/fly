#ifndef HLS_TYPES_H
#define HLS_TYPES_H

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

#define HLS_MAX_U8 0xFFu
#define HLS_MAX_U16 0xFFFFu
#define HLS_MAX_U32 0xFFFFFFFFu
#define HLS_MAX_U64 0xFFFFFFFFFFFFFFFFu

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

#endif /* End of HLS_TYPES_H */
