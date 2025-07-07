#ifndef FLY_CORE_HASH_H
#define FLY_CORE_HASH_H

#include "types.h"

namespace Fly
{

u64 Hash64(void* data, u64 size);

template <typename T>
struct Hash;

template <>
struct Hash<i8>
{
    inline u64 operator()(i8 key) { return static_cast<u64>(key); }
};

template <>
struct Hash<i16>
{
    inline u64 operator()(i16 key) { return static_cast<u64>(key); }
};

template <>
struct Hash<i32>
{
    inline u64 operator()(i32 key) { return static_cast<u64>(key); }
};

template <>
struct Hash<i64>
{
    inline u64 operator()(i64 key) { return static_cast<u64>(key); }
};

template <>
struct Hash<u8>
{
    inline u64 operator()(u8 key) { return static_cast<u64>(key); }
};

template <>
struct Hash<u16>
{
    inline u64 operator()(u16 key) { return static_cast<u64>(key); }
};

template <>
struct Hash<u32>
{
    inline u64 operator()(u32 key) { return static_cast<u64>(key); }
};

template <>
struct Hash<u64>
{
    inline u64 operator()(u64 key) { return key; }
};

template <>
struct Hash<char>
{
    inline u64 operator()(char key) { return static_cast<u64>(key); }
};

template <>
struct Hash<bool>
{
    inline u64 operator()(bool key) { return static_cast<u64>(key); }
};

template <typename T>
struct Hash<T*>
{
    inline u64 operator()(T* ptr) const { return reinterpret_cast<u64>(ptr); }
};

} // namespace Fly

#endif /* FLY_CORE_HASH_H */
