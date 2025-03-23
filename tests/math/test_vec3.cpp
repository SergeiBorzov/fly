#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/math/vec.h"

using namespace Hls::Math;

TEST(Vec3, Constructors)
{
    Vec3 zeroVector;
    EXPECT_FLOAT_EQ(0.0f, zeroVector.x);
    EXPECT_FLOAT_EQ(0.0f, zeroVector.y);
    EXPECT_FLOAT_EQ(0.0f, zeroVector.z);

    Vec3 singleValueVector(12345.0f);
    EXPECT_FLOAT_EQ(12345.0f, singleValueVector.x);
    EXPECT_FLOAT_EQ(12345.0f, singleValueVector.y);
    EXPECT_FLOAT_EQ(12345.0f, singleValueVector.z);

    Vec3 a({1.0f, 2.0f}, 3.0f);
    EXPECT_FLOAT_EQ(1.0f, a.x);
    EXPECT_FLOAT_EQ(2.0f, a.y);
    EXPECT_FLOAT_EQ(3.0f, a.z);

    Vec3 b(4.0f, {5.0f, 6.0f});
    EXPECT_FLOAT_EQ(4.0f, b.x);
    EXPECT_FLOAT_EQ(5.0f, b.y);
    EXPECT_FLOAT_EQ(6.0f, b.z);

    Vec3 c(-1.0f, 1.0f, -1.0f);
    EXPECT_FLOAT_EQ(-1.0f, c.x);
    EXPECT_FLOAT_EQ(1.0f, c.y);
    EXPECT_FLOAT_EQ(-1.0f, c.z);

    Vec3 d = Vec3(Vec4(4.0f, 3.0f, 2.0f, 1.0f));
    EXPECT_FLOAT_EQ(4.0f, d.x);
    EXPECT_FLOAT_EQ(3.0f, d.y);
    EXPECT_FLOAT_EQ(2.0f, d.z);
}

TEST(Vec3, Add)
{
    Vec3 a = {1.0f, 2.0f, 3.0f};
    Vec3 b = {2.0f, 1.0f, 3.0f};
    Vec3 c = a + b;
    EXPECT_FLOAT_EQ(3.0f, c.x);
    EXPECT_FLOAT_EQ(3.0f, c.y);
    EXPECT_FLOAT_EQ(6.0f, c.z);

    a += b;
    EXPECT_FLOAT_EQ(3.0f, a.x);
    EXPECT_FLOAT_EQ(3.0f, a.y);
    EXPECT_FLOAT_EQ(6.0f, c.z);
}

TEST(Vec3, Subtract)
{
    Vec3 a = {1.0f, 2.0f, 4.0f};
    Vec3 b = {3.0f, 4.0f, 2.0f};
    Vec3 c = b - a;
    EXPECT_FLOAT_EQ(2.0f, c.x);
    EXPECT_FLOAT_EQ(2.0f, c.y);
    EXPECT_FLOAT_EQ(-2.0f, c.z);

    c -= c;
    EXPECT_FLOAT_EQ(0.0f, c.x);
    EXPECT_FLOAT_EQ(0.0f, c.y);
    EXPECT_FLOAT_EQ(0.0f, c.z);
}

TEST(Vec3, Multiply)
{
    Vec3 a = {2.0f, 0.0f, 4.0f};
    Vec3 b = {9.0f, 127.0f, 0.5f};
    Vec3 c = a * b;
    EXPECT_FLOAT_EQ(18.0f, c.x);
    EXPECT_FLOAT_EQ(0.0f, c.y);
    EXPECT_FLOAT_EQ(2.0f, c.z);

    c *= c;
    EXPECT_FLOAT_EQ(324.0f, c.x);
    EXPECT_FLOAT_EQ(0.0f, c.y);
    EXPECT_FLOAT_EQ(4.0f, c.z);

    a *= 3.0f;
    EXPECT_FLOAT_EQ(6.0f, a.x);
    EXPECT_FLOAT_EQ(0.0f, a.y);
    EXPECT_FLOAT_EQ(12.0f, a.z);

    Vec3 d = 3.0f * a;
    EXPECT_FLOAT_EQ(18.0f, d.x);
    EXPECT_FLOAT_EQ(0.0f, d.y);
    EXPECT_FLOAT_EQ(36.0f, d.z);

    d = d * 0.5f;
    EXPECT_FLOAT_EQ(9.0f, d.x);
    EXPECT_FLOAT_EQ(0.0f, d.y);
    EXPECT_FLOAT_EQ(18.0f, d.z);
}

TEST(Vec3, Divide)
{
    Vec3 a = {8.0f, 4.0f, 0.5f};
    Vec3 b = {2.0f, 0.5f, 0.25f};
    Vec3 c = a / b;
    EXPECT_FLOAT_EQ(4.0f, c.x);
    EXPECT_FLOAT_EQ(8.0f, c.y);
    EXPECT_FLOAT_EQ(2.0f, c.z);

    c /= c;
    EXPECT_FLOAT_EQ(1.0f, c.x);
    EXPECT_FLOAT_EQ(1.0f, c.y);
    EXPECT_FLOAT_EQ(1.0f, c.z);

    a /= 2.0f;
    EXPECT_FLOAT_EQ(4.0f, a.x);
    EXPECT_FLOAT_EQ(2.0f, a.y);
    EXPECT_FLOAT_EQ(0.25f, a.z);

    Vec3 d = a / 2.0f;
    EXPECT_FLOAT_EQ(2.0f, d.x);
    EXPECT_FLOAT_EQ(1.0f, d.y);
    EXPECT_FLOAT_EQ(0.125f, d.z);
}

TEST(Vec3, Subscript)
{
    Vec3 a = {1.0f, 2.0f, 3.0f};
    EXPECT_FLOAT_EQ(a.x, a[0]);
    EXPECT_FLOAT_EQ(a.y, a[1]);
    EXPECT_FLOAT_EQ(a.z, a[2]);
}

TEST(Vec3, Dot)
{
    Vec3 a = {3.0f, -6.0f, 1.0f};
    Vec3 b = {6.0f, 3.0f, -1.0f};
    Vec3 c = {1.0f, 1.0f, 2.0f};

    EXPECT_FLOAT_EQ(-1.0f, Dot(a, b));
    EXPECT_FLOAT_EQ(-1.0f, Dot(a, c));
    EXPECT_FLOAT_EQ(7.0f, Dot(b, c));
}

TEST(Vec3, Cross)
{
    Vec3 x(1.0f, 0.0f, 0.0f);
    Vec3 y(0.0f, 1.0f, 0.0f);
    Vec3 z = Cross(x, y);

    EXPECT_FLOAT_EQ(0.0f, z.x);
    EXPECT_FLOAT_EQ(0.0f, z.y);
    EXPECT_FLOAT_EQ(1.0f, z.z);

    Vec3 a(2, 3, 4);
    Vec3 b(5, 6, 7);
    Vec3 c = Cross(a, b);

    EXPECT_FLOAT_EQ(-3.0f, c.x);
    EXPECT_FLOAT_EQ(6.0f, c.y);
    EXPECT_FLOAT_EQ(-3.0f, c.z);
}

TEST(Vec3, Length)
{
    Vec3 a(23.0f, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(23.0f, Length(a));
    EXPECT_FLOAT_EQ(23.0f * 23.0f, LengthSqr(a));

    Vec3 b(1.0f, 2.0f, 2.0f);
    EXPECT_FLOAT_EQ(3.0f, Length(b));
    EXPECT_FLOAT_EQ(9.0f, LengthSqr(b));
}

TEST(Vec3, Normalize)
{
    Vec3 a = Normalize(Vec3(23.0f, 0.0f, 0.0f));
    EXPECT_FLOAT_EQ(1.0f, a.x);
    EXPECT_FLOAT_EQ(0.0f, a.y);
    EXPECT_FLOAT_EQ(0.0f, a.z);

    Vec3 b = Normalize(Vec3(1.0f, 2.0f, 2.0f));
    EXPECT_FLOAT_EQ(1.0f/3.0f, b.x);
    EXPECT_FLOAT_EQ(2.0f/3.0f, b.y);
    EXPECT_FLOAT_EQ(2.0f/3.0f, b.z);
}
