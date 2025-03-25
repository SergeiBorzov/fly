#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/math/vec.h"

using namespace Hls::Math;

TEST(Vec2, Constructors)
{
    Vec2 zeroVector;
    EXPECT_NEAR(0.0f, zeroVector.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, zeroVector.y, HLS_MATH_EPSILON);

    Vec2 singleValueVector(12345.0f);
    EXPECT_NEAR(12345.0f, singleValueVector.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(12345.0f, singleValueVector.y, HLS_MATH_EPSILON);

    Vec2 vector2(12.0f, -34.0f);
    EXPECT_NEAR(12.0f, vector2.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(-34.0f, vector2.y, HLS_MATH_EPSILON);

    Vec3 vec3(1.0f, 2.0f, 3.0f);
    Vec4 vec4(-1.0f, -2.0f, -3.0f, -4.0f);

    Vec2 a = Vec2(vec3);
    EXPECT_NEAR(1.0f, a.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(2.0f, a.y, HLS_MATH_EPSILON);

    Vec2 b = Vec2(vec4);
    EXPECT_NEAR(-1.0f, b.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(-2.0f, b.y, HLS_MATH_EPSILON);
}

TEST(Vec2, Add)
{
    Vec2 a = {1.0f, 2.0f};
    Vec2 b = {2.0f, 1.0f};
    Vec2 c = a + b;
    EXPECT_NEAR(3.0f, c.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(3.0f, c.y, HLS_MATH_EPSILON);

    a += b;
    EXPECT_NEAR(3.0f, a.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(3.0f, a.y, HLS_MATH_EPSILON);
}

TEST(Vec2, Subtract)
{
    Vec2 a = {1.0f, 2.0f};
    Vec2 b = {3.0f, 4.0f};
    Vec2 c = b - a;
    EXPECT_NEAR(2.0f, c.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(2.0f, c.y, HLS_MATH_EPSILON);

    c -= c;
    EXPECT_NEAR(0.0f, c.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, c.y, HLS_MATH_EPSILON);
}

TEST(Vec2, Multiply)
{
    Vec2 a = {2.0f, 0.0f};
    Vec2 b = {9.0f, 127.0f};
    Vec2 c = a * b;
    EXPECT_NEAR(18.0f, c.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, c.y, HLS_MATH_EPSILON);

    c *= c;
    EXPECT_NEAR(324.0f, c.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, c.y, HLS_MATH_EPSILON);

    a *= 3.0f;
    EXPECT_NEAR(6.0f, a.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, a.y, HLS_MATH_EPSILON);

    Vec2 d = 3.0f * a;
    EXPECT_NEAR(18.0f, d.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, d.y, HLS_MATH_EPSILON);

    d = d * 0.5f;
    EXPECT_NEAR(9.0f, d.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, d.y, HLS_MATH_EPSILON);
}

TEST(Vec2, Divide)
{
    Vec2 a = {8.0f, 4.0f};
    Vec2 b = {2.0f, 0.5f};
    Vec2 c = a / b;
    EXPECT_NEAR(4.0f, c.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(8.0f, c.y, HLS_MATH_EPSILON);

    c /= c;
    EXPECT_NEAR(1.0f, c.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(1.0f, c.y, HLS_MATH_EPSILON);

    a /= 2.0f;
    EXPECT_NEAR(4.0f, a.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(2.0f, a.y, HLS_MATH_EPSILON);

    Vec2 d = a / 2.0f;
    EXPECT_NEAR(2.0f, d.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(1.0f, d.y, HLS_MATH_EPSILON);
}

TEST(Vec2, Subscript)
{
    Vec2 a = {1.0f, 2.0f};
    EXPECT_NEAR(a.x, a[0], HLS_MATH_EPSILON);
    EXPECT_NEAR(a.y, a[1], HLS_MATH_EPSILON);
}

TEST(Vec2, Dot)
{
    Vec2 a = {3.0f, -6.0f};
    Vec2 b = {6.0f, 3.0f};
    Vec2 c = {1.0f, 1.0f};

    EXPECT_NEAR(0.0f, Dot(a, b), HLS_MATH_EPSILON);
    EXPECT_NEAR(-3.0f, Dot(a, c), HLS_MATH_EPSILON);
    EXPECT_NEAR(9.0f, Dot(b, c), HLS_MATH_EPSILON);
}

TEST(Vec2, Length)
{
    Vec2 a(23.0f, 0.0f);
    EXPECT_NEAR(23.0f, Length(a), HLS_MATH_EPSILON);
    EXPECT_NEAR(23.0f * 23.0f, LengthSqr(a), HLS_MATH_EPSILON);

    Vec2 b(3.0f, 4.0f);
    EXPECT_NEAR(5.0f, Length(b), HLS_MATH_EPSILON);
    EXPECT_NEAR(25.0f, LengthSqr(b), HLS_MATH_EPSILON);
}

TEST(Vec2, Normalize)
{
    Vec2 a = Normalize(Vec2(23.0f, 0.0f));
    EXPECT_NEAR(1.0f, a.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, a.y, HLS_MATH_EPSILON);

    Vec2 b = Normalize(Vec2(3.0f, 4.0f));
    EXPECT_NEAR(0.6f, b.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.8f, b.y, HLS_MATH_EPSILON);
}
