#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "math/vec.h"

using namespace Hls::Math;

TEST(Vec4, Constructors)
{
    Vec4 zeroVector;
    EXPECT_FLOAT_EQ(0.0f, zeroVector.x);
    EXPECT_FLOAT_EQ(0.0f, zeroVector.y);
    EXPECT_FLOAT_EQ(0.0f, zeroVector.z);
    EXPECT_FLOAT_EQ(0.0f, zeroVector.z);

    Vec4 singleValueVector(12345.0f);
    EXPECT_FLOAT_EQ(12345.0f, singleValueVector.x);
    EXPECT_FLOAT_EQ(12345.0f, singleValueVector.y);
    EXPECT_FLOAT_EQ(12345.0f, singleValueVector.z);
    EXPECT_FLOAT_EQ(12345.0f, singleValueVector.w);

    Vec4 a({1.0f, 2.0f, 3.0f}, 4.0f);
    EXPECT_FLOAT_EQ(1.0f, a.x);
    EXPECT_FLOAT_EQ(2.0f, a.y);
    EXPECT_FLOAT_EQ(3.0f, a.z);
    EXPECT_FLOAT_EQ(4.0f, a.w);

    Vec4 b(4.0f, {5.0f, 6.0f, 7.0f});
    EXPECT_FLOAT_EQ(4.0f, b.x);
    EXPECT_FLOAT_EQ(5.0f, b.y);
    EXPECT_FLOAT_EQ(6.0f, b.z);
    EXPECT_FLOAT_EQ(7.0f, b.w);

    Vec4 c({-1.0f, 1.0f}, {1.0f, -1.0f});
    EXPECT_FLOAT_EQ(-1.0f, c.x);
    EXPECT_FLOAT_EQ(1.0f, c.y);
    EXPECT_FLOAT_EQ(1.0f, c.z);
    EXPECT_FLOAT_EQ(-1.0f, c.w);

    Vec4 d(1.0f, 2.0f, {3.0f, 4.0f});
    EXPECT_FLOAT_EQ(1.0f, d.x);
    EXPECT_FLOAT_EQ(2.0f, d.y);
    EXPECT_FLOAT_EQ(3.0f, d.z);
    EXPECT_FLOAT_EQ(4.0f, d.w);

    Vec4 e(1.0f, {2.0f, 3.0f}, 4.0f);
    EXPECT_FLOAT_EQ(1.0f, e.x);
    EXPECT_FLOAT_EQ(2.0f, e.y);
    EXPECT_FLOAT_EQ(3.0f, e.z);
    EXPECT_FLOAT_EQ(4.0f, e.w);

    Vec4 f({1.0f, 2.0f}, 3.0f, 4.0f);
    EXPECT_FLOAT_EQ(1.0f, f.x);
    EXPECT_FLOAT_EQ(2.0f, f.y);
    EXPECT_FLOAT_EQ(3.0f, f.z);
    EXPECT_FLOAT_EQ(4.0f, f.w);

    Vec4 g(1.0f, 2.0f, 3.0f, 4.0f);
    EXPECT_FLOAT_EQ(1.0f, g.x);
    EXPECT_FLOAT_EQ(2.0f, g.y);
    EXPECT_FLOAT_EQ(3.0f, g.z);
    EXPECT_FLOAT_EQ(4.0f, g.w);
}

TEST(Vec4, Add)
{
    Vec4 a = {1.0f, 2.0f, 3.0f, 4.0f};
    Vec4 b = {2.0f, 1.0f, 3.0f, 4.0f};
    Vec4 c = a + b;
    EXPECT_FLOAT_EQ(3.0f, c.x);
    EXPECT_FLOAT_EQ(3.0f, c.y);
    EXPECT_FLOAT_EQ(6.0f, c.z);
    EXPECT_FLOAT_EQ(8.0f, c.w);

    a += b;
    EXPECT_FLOAT_EQ(3.0f, a.x);
    EXPECT_FLOAT_EQ(3.0f, a.y);
    EXPECT_FLOAT_EQ(6.0f, c.z);
    EXPECT_FLOAT_EQ(8.0f, c.w);
}

TEST(Vec4, Subtract)
{
    Vec4 a = {1.0f, 2.0f, 4.0f, 2.0f};
    Vec4 b = {3.0f, 4.0f, 2.0f, -2.0f};
    Vec4 c = b - a;
    EXPECT_FLOAT_EQ(2.0f, c.x);
    EXPECT_FLOAT_EQ(2.0f, c.y);
    EXPECT_FLOAT_EQ(-2.0f, c.z);
    EXPECT_FLOAT_EQ(-4.0f, c.w);

    c -= c;
    EXPECT_FLOAT_EQ(0.0f, c.x);
    EXPECT_FLOAT_EQ(0.0f, c.y);
    EXPECT_FLOAT_EQ(0.0f, c.z);
    EXPECT_FLOAT_EQ(0.0f, c.w);
}

TEST(Vec4, Multiply)
{
    Vec4 a = {2.0f, 0.0f, 4.0f, 2.0f};
    Vec4 b = {9.0f, 127.0f, 0.5f, -1.0f};
    Vec4 c = a * b;
    EXPECT_FLOAT_EQ(18.0f, c.x);
    EXPECT_FLOAT_EQ(0.0f, c.y);
    EXPECT_FLOAT_EQ(2.0f, c.z);
    EXPECT_FLOAT_EQ(-2.0f, c.w);

    c *= c;
    EXPECT_FLOAT_EQ(324.0f, c.x);
    EXPECT_FLOAT_EQ(0.0f, c.y);
    EXPECT_FLOAT_EQ(4.0f, c.z);
    EXPECT_FLOAT_EQ(4.0f, c.w);
}

TEST(Vec4, Divide)
{
    Vec4 a = {8.0f, 4.0f, 0.5f, 1.0f};
    Vec4 b = {2.0f, 0.5f, 0.25f, 1.0f};
    Vec4 c = a / b;
    EXPECT_FLOAT_EQ(4.0f, c.x);
    EXPECT_FLOAT_EQ(8.0f, c.y);
    EXPECT_FLOAT_EQ(2.0f, c.z);
    EXPECT_FLOAT_EQ(1.0f, c.w);

    c /= c;
    EXPECT_FLOAT_EQ(1.0f, c.x);
    EXPECT_FLOAT_EQ(1.0f, c.y);
    EXPECT_FLOAT_EQ(1.0f, c.z);
    EXPECT_FLOAT_EQ(1.0f, c.w);
}

TEST(Vec4, Subscript)
{
    Vec4 a = {1.0f, 2.0f, 3.0f, 4.0f};
    EXPECT_FLOAT_EQ(a.x, a[0]);
    EXPECT_FLOAT_EQ(a.y, a[1]);
    EXPECT_FLOAT_EQ(a.z, a[2]);
    EXPECT_FLOAT_EQ(a.w, a[3]);
}

TEST(Vec4, Dot)
{
    Vec4 a = {3.0f, -6.0f, 1.0f, 0.0f};
    Vec4 b = {6.0f, 3.0f, -1.0f, 5.0f};
    Vec4 c = {1.0f, 1.0f, 2.0f, 1.0f};

    EXPECT_FLOAT_EQ(-1.0f, Dot(a, b));
    EXPECT_FLOAT_EQ(-1.0f, Dot(a, c));
    EXPECT_FLOAT_EQ(12.0f, Dot(b, c));
}
