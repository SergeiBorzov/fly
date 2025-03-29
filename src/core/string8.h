#ifndef HLS_CORE_STRING8_H
#define HLS_CORE_STRING8_H

#include "assert.h"

namespace Hls
{

bool CharIsAlpha(i32 c);
bool CharIsDigit(i32 c);
bool CharIsWhitespace(i32 c);

struct String8CutPair;

struct String8
{
    inline String8(char* data = nullptr, u64 size = 0)
        : data_(data), size_(size)
    {
    }

    inline char operator[](u64 index)
    {
        HLS_ASSERT(index < size_);
        return data_[index];
    }

    bool operator==(String8 rhs);

    inline const char* Data() const { return data_; }
    inline u64 Size() const { return size_; }

    static bool Cut(String8, char sep, String8CutPair& cut);
    static String8 TrimLeft(String8 str);
    static String8 TrimRight(String8 str);
    static bool ParseF64(String8 str, f64& res);
    static bool ParseF32(String8 str, f32& res);
    static bool ParseU64(String8 str, u64& res);

private:
    char* data_ = nullptr;
    u64 size_ = 0;
};

struct String8CutPair
{
    String8 head = {};
    String8 tail = {};
};

#define HLS_STRING8_LITERAL(str) String8((char*)str, sizeof(str) - 1)

} // namespace Hls

#endif /* HLS_CORE_STRING8_H */
