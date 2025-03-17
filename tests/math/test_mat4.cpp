#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "math/mat.h"

using namespace Hls::Math;

TEST(Mat4, Constructors)
{
    Mat4 identity;
    f32 expected[16] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    for (i32 i = 0; i < 16; i++)
    {
        EXPECT_FLOAT_EQ(expected[i], identity.data[i]);
    }

    Mat4 a(3.0f);
    f32 aExpected[16] = {3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 3.0f, 0.0f, 0.0f,
                         0.0f, 0.0f, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 3.0f};
    for (i32 i = 0; i < 16; i++)
    {
        EXPECT_FLOAT_EQ(aExpected[i], a.data[i]);
    }

    f32 values[16] = {1.0f, 2.0f,  3.0f,  4.0f,  5.0f,  6.0f,  7.0f,  8.0f,
                      9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f};
    Mat4 b(values, 16);
    for (i32 i = 0; i < 16; i++)
    {
        EXPECT_FLOAT_EQ(values[i], b.data[i]);
    }
}

TEST(Mat4, Add)
{
    Mat4 a;
    f32 values[16] = {1.0f, 2.0f,  3.0f,  4.0f,  5.0f,  6.0f,  7.0f,  8.0f,
                      9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f};
    Mat4 b(values, 16);

    Mat4 c = a + b;
    for (i32 i = 0; i < 16; i++)
    {
        EXPECT_FLOAT_EQ(a.data[i] + b.data[i], c.data[i]);
    }

    a += b;
    for (i32 i = 0; i < 16; i++)
    {
        EXPECT_FLOAT_EQ(c.data[i], a.data[i]);
    }
}

TEST(Mat4, Subtract)
{
    Mat4 a;
    f32 values[16] = {1.0f, 2.0f,  3.0f,  4.0f,  5.0f,  6.0f,  7.0f,  8.0f,
                      9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f};
    Mat4 b(values, 16);

    Mat4 c = b - a;
    for (i32 i = 0; i < 16; i++)
    {
        EXPECT_FLOAT_EQ(b.data[i] - a.data[i], c.data[i]);
    }

    c -= c;
    for (i32 i = 0; i < 16; i++)
    {
        EXPECT_FLOAT_EQ(0.0f, c.data[i]);
    }
}

TEST(Mat4, Multiply)
{
    f32 aValues[16] = {1.0f, 5.0f, 9.0f,  13.0f, 2.0f, 6.0f, 10.0f, 14.0f,
                       3.0f, 7.0f, 11.0f, 15.0f, 4.0f, 8.0f, 12.0f, 16.0f};
    Mat4 a(aValues, 16);

    f32 bValues[16] = {17.0f, 21.0f, 25.0f, 29.0f, 18.0f, 22.0f, 26.0f, 30.0f,
                       19.0f, 23.0f, 27.0f, 31.0f, 20.0f, 24.0f, 28.0f, 32.0f};
    Mat4 b(bValues, 16);

    Mat4 c = a * b;

    f32 expectedValues[16] = {
        250.0f, 618.0f, 986.0f,  1354.0f, 260.0f, 644.0f, 1028.0f, 1412.0f,
        270.0f, 670.0f, 1070.0f, 1470.0f, 280.0f, 696.0f, 1112.0f, 1528.0f};

    for (i32 i = 0; i < 16; i++)
    {
        EXPECT_FLOAT_EQ(expectedValues[i], c.data[i]);
    }

    a *= b;
    for (i32 i = 0; i < 16; i++)
    {
        EXPECT_FLOAT_EQ(expectedValues[i], a.data[i]);
    }
}
