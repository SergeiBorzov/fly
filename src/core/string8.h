#ifndef FLY_CORE_STRING8_H
#define FLY_CORE_STRING8_H

#include "assert.h"
#include "hash.h"

namespace Fly
{
struct Arena;

bool CharIsAlpha(i32 c);
bool CharIsDigit(i32 c);
bool CharIsNewline(i32 c);
bool CharIsWhitespace(i32 c);
bool CharIsExponent(i32 c);

struct String8CutPair;

struct String8
{
    inline String8() : data_(nullptr), size_(0) {}
    inline String8(const char* data, u64 size) : data_(data), size_(size) {}

    inline char operator[](u64 index) const
    {
        FLY_ASSERT(index < size_);
        return data_[index];
    }

    inline operator bool() const { return data_ && size_; }

    bool operator==(String8 rhs);
    bool operator!=(String8 rhs);

    inline const char* Data() const { return data_; }
    inline u64 Size() const { return size_; }
    bool StartsWith(String8 str);
    bool EndsWith(String8 str);

    static bool Cut(String8, char sep, String8CutPair& cut);
    static String8 TrimLeft(String8 str);
    static String8 TrimRight(String8 str);
    static bool ParseF64(String8 str, f64& res);
    static bool ParseF32(String8 str, f32& res);
    static bool ParseU64(String8 str, u64& res);
    static bool ParseI64(String8 str, i64& res);
    static bool ParseI32(String8 str, i32& res);

    static String8 Find(String8 str, i32 character);
    static String8 FindLast(String8 str, i32 character);
    static char* CopyNullTerminate(Arena& arena, String8 str);

private:
    const char* data_ = nullptr;
    u64 size_ = 0;
};

template <>
struct Hash<String8>
{
    u64 operator()(String8 key) { return Hash64(key.Data(), key.Size()); }
};

struct String8CutPair
{
    String8 head = {};
    String8 tail = {};
};

#define FLY_STRING8_LITERAL(str) Fly::String8(str, sizeof(str) - 1)

} // namespace Fly

#endif /* FLY_CORE_STRING8_H */
