#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/core/string8.h"

using namespace Hls;

TEST(String8, Constructors)
{
    String8 empty;
    EXPECT_EQ(nullptr, empty.Data());
    EXPECT_EQ(0, empty.Size());

    String8 hello = HLS_STRING8_LITERAL("Hello");
    EXPECT_TRUE(hello.Data() != nullptr);
    EXPECT_EQ(5, hello.Size());
}

TEST(String8, Trim)
{
    String8 a = HLS_STRING8_LITERAL("        a and b  ");
    String8 aTrimmedLeft = String8::TrimLeft(a);

    String8 b = HLS_STRING8_LITERAL("a and b  ");
    EXPECT_EQ(a, b);

    String8 aTrimmed = String8::TrimRight(aTrimmedLeft);
    String8 c = HLS_STRING8_LITERAL("a and b");
    EXPECT_EQ(aTrimmed, c);
}

TEST(String8, Parse)
{
    String8 str = HLS_STRING8_LITERAL("-12.345");
    f64 a = 0.0;
    bool res = String8::ParseF64(str, a);
    EXPECT_TRUE(res);
    EXPECT_DOUBLE_EQ(-12.345, a);

    String8 str2 = HLS_STRING8_LITERAL("+348");
    f32 b = 0.0f;
    res = String8::ParseF32(str2, b);
    EXPECT_TRUE(res);
    EXPECT_FLOAT_EQ(348.0f, b);
}

