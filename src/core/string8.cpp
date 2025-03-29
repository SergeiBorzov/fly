#include <string.h>

#include "string8.h"

namespace Hls
{

bool CharIsAlpha(i32 c)
{
    return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
}

bool CharIsDigit(i32 c) { return (c >= '0' && c <= '9'); }

bool CharIsWhitespace(i32 c)
{
    return c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '\v' ||
           c == '\f';
}

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

    char* start = str.data_;
    char* end = str.data_ + str.size_;
    char* point = start;
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

} // namespace Hls
