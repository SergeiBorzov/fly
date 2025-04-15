#include <string.h>

#include "arena.h"
#include "string8.h"

namespace Hls
{

bool CharIsAlpha(i32 c)
{
    return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
}

bool CharIsDigit(i32 c) { return (c >= '0' && c <= '9'); }

bool CharIsWhitespace(i32 c) { return c == ' ' || c == '\t' || c == '\r'; }

bool CharIsExponent(i32 c) { return c == 'e' || c == 'E'; }

bool CharIsNewline(i32 c) { return c == '\n'; }

bool String8::operator==(String8 rhs)
{
    return size_ == rhs.size_ && (!size_ || !memcmp(data_, rhs.data_, size_));
}

bool String8::Cut(String8 str, char sep, String8CutPair& cutPair)
{
    if (!str.data_ || str.size_ == 0)
    {
        return false;
    }

    const char* start = str.data_;
    const char* end = str.data_ + str.size_;
    const char* point = start;
    while (point < end && *point != sep)
    {
        point++;
    }
    bool success = point < end;
    if (success)
    {
        cutPair.head = String8(start, static_cast<u64>(point - start));
        cutPair.tail = String8(point + 1, static_cast<u64>(end - point - 1));
    }
    return success;
}

String8 String8::TrimLeft(String8 str)
{
    if (!str.size_ || !str.data_)
    {
        return str;
    }

    while (str.size_ > 0 && CharIsWhitespace(str.data_[0]))
    {
        str.data_ += 1;
        str.size_ -= 1;
    }

    return {str.data_, str.size_};
}

String8 String8::TrimRight(String8 str)
{
    if (!str.size_ || !str.data_)
    {
        return str;
    }

    while (str.size_ > 0 && CharIsWhitespace(str.data_[str.size_ - 1]))
    {
        str.size_ -= 1;
    }
    return {str.data_, str.size_};
}

String8 String8::Find(String8 str, i32 character)
{
    for (u64 i = 0; i < str.Size(); i++)
    {
        if (str[i] == character)
        {
            return String8(str.Data() + i, str.Size() - i);
        }
    }
    return String8();
}

String8 String8::FindLast(String8 str, i32 character)
{
    for (i64 i = str.Size() - 1; i >= 0; i--)
    {
        if (str[i] == character)
        {
            return String8(str.Data() + i, str.Size() - i);
        }
    }
    return String8();
}

char* String8::CopyNullTerminate(Arena& arena, String8 str)
{
    bool isNullTerminated = str[str.Size() - 1] == '\0';
    char* copyData = HLS_ALLOC(arena, char, str.Size() + !isNullTerminated);
    memcpy(copyData, str.Data(), str.Size());
    if (!isNullTerminated)
    {
        copyData[str.Size()] = '\0';
    }
    return copyData;
}

bool String8::ParseF64(String8 str, f64& res)
{
    if (!str.size_ || !str.data_)
    {
        return false;
    }

    str = TrimLeft(TrimRight(str));

    // Handle sign
    i32 sign = 1;
    if (str.data_[0] == '-' || str.data_[0] == '+')
    {
        sign = (str.data_[0] == '+') * 2 - 1;
        ++str.data_;
        --str.size_;
    }

    String8CutPair cutPair{};
    cutPair.head = str;
    Cut(str, '.', cutPair);

    // Process integer part
    f64 result = 0.0;
    for (u64 i = 0; i < cutPair.head.size_; i++)
    {
        if (CharIsDigit(cutPair.head.data_[i]))
        {
            result = result * 10 + (cutPair.head.data_[i] - '0');
        }
        else
        {
            return false;
        }
    }

    // Process fraction part
    f64 fraction = 0.1;
    for (u64 i = 0; i < cutPair.tail.size_; i++)
    {
        if (CharIsDigit(cutPair.tail.data_[i]))
        {
            result += (cutPair.tail.data_[i] - '0') * fraction;
            fraction *= 0.1;
        }
        else
        {
            return false;
        }
    }

    res = sign * result;
    return true;
}

bool String8::ParseF32(String8 str, f32& res)
{
    f64 value = 0.0;
    if (!ParseF64(str, value))
    {
        return false;
    }
    res = static_cast<f32>(value);
    return true;
}

bool String8::ParseI64(String8 str, i64& res)
{
    if (!str.size_ || !str.data_)
    {
        return false;
    }

    // Handle sign
    i64 sign = 1;
    if (str.data_[0] == '+' || str.data_[0] == '-')
    {
        ++str.data_;
        --str.size_;
        sign = (str.data_[0] == '+') * 2 - 1;
    }

    str = TrimLeft(TrimRight(str));

    // Process integer part
    i64 result = 0;
    for (u64 i = 0; i < str.size_; i++)
    {
        if (CharIsDigit(str.data_[i]))
        {
            result = result * 10 + (str.data_[i] - '0');
        }
        else
        {
            return false;
        }
    }

    res = sign * result;
    return true;
}

bool String8::ParseI32(String8 str, i32& res)
{
    if (!str.size_ || !str.data_)
    {
        return false;
    }

    // Handle sign
    i32 sign = 1;
    if (str.data_[0] == '+' || str.data_[0] == '-')
    {
        ++str.data_;
        --str.size_;
        sign = (str.data_[0] == '+') * 2 - 1;
    }

    str = TrimLeft(TrimRight(str));

    // Process integer part
    i32 result = 0;
    for (u64 i = 0; i < str.size_; i++)
    {
        if (CharIsDigit(str.data_[i]))
        {
            result = result * 10 + (str.data_[i] - '0');
        }
        else
        {
            return false;
        }
    }

    res = sign * result;
    return true;
}

bool String8::ParseU64(String8 str, u64& res)
{
    if (!str.size_ || !str.data_)
    {
        return false;
    }

    // Handle sign
    if (str.data_[0] == '+')
    {
        ++str.data_;
        --str.size_;
    }

    str = TrimLeft(TrimRight(str));

    // Process integer part
    u64 result = 0;
    for (u64 i = 0; i < str.size_; i++)
    {
        if (CharIsDigit(str.data_[i]))
        {
            result = result * 10 + (str.data_[i] - '0');
        }
        else
        {
            return false;
        }
    }

    res = result;
    return true;
}

} // namespace Hls
