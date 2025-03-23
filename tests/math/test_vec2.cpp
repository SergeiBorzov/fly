#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/math/vec.h"

using namespace Hls::Math;

TEST(Vec2, Constructors)
{
    Vec2 zeroVector;
    EXPECT_FLOAT_EQ(0.0f, zeroVector.x);
    EXPECT_FLOAT_EQ(0.0f, zeroVector.y);

    Vec2 singleValueVector(12345.0f);
    EXPECT_FLOAT_EQ(12345.0f, singleValueVector.x);
    EXPECT_FLOAT_EQ(12345.0f, singleValueVector.y);

    Vec2 vector2(12.0f, -34.0f);
    EXPECT_FLOAT_EQ(12.0f, vector2.x);
    EXPECT_FLOAT_EQ(-34.0f, vector2.y);

    Vec3 vec3(1.0f, 2.0f, 3.0f);
    Vec4 vec4(-1.0f, -2.0f, -3.0f, -4.0f);

    Vec2 a = Vec2(vec3);
    EXPECT_FLOAT_EQ(1.0f, a.x);
    EXPECT_FLOAT_EQ(2.0f, a.y);

    Vec2 b = Vec2(vec4);
    EXPECT_FLOAT_EQ(-1.0f, b.x);
    EXPECT_FLOAT_EQ(-2.0f, b.y);
}

TEST(Vec2, Add)
{
    Vec2 a = {1.0f, 2.0f};
    Vec2 b = {2.0f, 1.0f};
    Vec2 c = a + b;
    EXPECT_FLOAT_EQ(3.0f, c.x);
    EXPECT_FLOAT_EQ(3.0f, c.y);

    a += b;
    EXPECT_FLOAT_EQ(3.0f, a.x);
    EXPECT_FLOAT_EQ(3.0f, a.y);
}

TEST(Vec2, Subtract)
{
    Vec2 a = {1.0f, 2.0f};
    Vec2 b = {3.0f, 4.0f};
    Vec2 c = b - a;
    EXPECT_FLOAT_EQ(2.0f, c.x);
    EXPECT_FLOAT_EQ(2.0f, c.y);

    c -= c;
    EXPECT_FLOAT_EQ(0.0f, c.x);
    EXPECT_FLOAT_EQ(0.0f, c.y);
}

TEST(Vec2, Multiply)
{
    Vec2 a = {2.0f, 0.0f};
    Vec2 b = {9.0f, 127.0f};
    Vec2 c = a * b;
    EXPECT_FLOAT_EQ(18.0f, c.x);
    EXPECT_FLOAT_EQ(0.0f, c.y);

    c *= c;
    EXPECT_FLOAT_EQ(324.0f, c.x);
    EXPECT_FLOAT_EQ(0.0f, c.y);

    a *= 3.0f;
    EXPECT_FLOAT_EQ(6.0f, a.x);
    EXPECT_FLOAT_EQ(0.0f, a.y);

    Vec2 d = 3.0f * a;
    EXPECT_FLOAT_EQ(18.0f, d.x);
    EXPECT_FLOAT_EQ(0.0f, d.y);

    d = d * 0.5f;
    EXPECT_FLOAT_EQ(9.0f, d.x);
    EXPECT_FLOAT_EQ(0.0f, d.y);
}

TEST(Vec2, Divide)
{
    Vec2 a = {8.0f, 4.0f};
    Vec2 b = {2.0f, 0.5f};
    Vec2 c = a / b;
    EXPECT_FLOAT_EQ(4.0f, c.x);
    EXPECT_FLOAT_EQ(8.0f, c.y);

    c /= c;
    EXPECT_FLOAT_EQ(1.0f, c.x);
    EXPECT_FLOAT_EQ(1.0f, c.y);

    a /= 2.0f;
    EXPECT_FLOAT_EQ(4.0f, a.x);
    EXPECT_FLOAT_EQ(2.0f, a.y);

    Vec2 d = a / 2.0f;
    EXPECT_FLOAT_EQ(2.0f, d.x);
    EXPECT_FLOAT_EQ(1.0f, d.y);
}

TEST(Vec2, Subscript)
{
    Vec2 a = {1.0f, 2.0f};
    EXPECT_FLOAT_EQ(a.x, a[0]);
    EXPECT_FLOAT_EQ(a.y, a[1]);
}

TEST(Vec2, Dot)
{
    Vec2 a = {3.0f, -6.0f};
    Vec2 b = {6.0f, 3.0f};
    Vec2 c = {1.0f, 1.0f};

    EXPECT_FLOAT_EQ(0.0f, Dot(a, b));
    EXPECT_FLOAT_EQ(-3.0f, Dot(a, c));
    EXPECT_FLOAT_EQ(9.0f, Dot(b, c));
}

TEST(Vec2, Length)
{
    Vec2 a(23.0f, 0.0f);
    EXPECT_FLOAT_EQ(23.0f, Length(a));
    EXPECT_FLOAT_EQ(23.0f * 23.0f, LengthSqr(a));

    Vec2 b(3.0f, 4.0f);
    EXPECT_FLOAT_EQ(5.0f, Length(b));
    EXPECT_FLOAT_EQ(25.0f, LengthSqr(b));
}

TEST(Vec2, Normalize)
{
    Vec2 a = Normalize(Vec2(23.0f, 0.0f));
    EXPECT_FLOAT_EQ(1.0f, a.x);
    EXPECT_FLOAT_EQ(0.0f, a.y);

    Vec2 b = Normalize(Vec2(3.0f, 4.0f));
    EXPECT_FLOAT_EQ(0.6f, b.x);
    EXPECT_FLOAT_EQ(0.8f, b.y);
}
