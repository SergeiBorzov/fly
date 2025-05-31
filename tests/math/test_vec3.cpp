#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/math/vec.h"

using namespace Fly::Math;

TEST(Vec3, Constructors)
{
    Vec3 zeroVector;
    EXPECT_NEAR(0.0f, zeroVector.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(0.0f, zeroVector.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(0.0f, zeroVector.z, FLY_MATH_EPSILON);

    Vec3 singleValueVector(12345.0f);
    EXPECT_NEAR(12345.0f, singleValueVector.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(12345.0f, singleValueVector.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(12345.0f, singleValueVector.z, FLY_MATH_EPSILON);

    Vec3 a({1.0f, 2.0f}, 3.0f);
    EXPECT_NEAR(1.0f, a.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(2.0f, a.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(3.0f, a.z, FLY_MATH_EPSILON);

    Vec3 b(4.0f, {5.0f, 6.0f});
    EXPECT_NEAR(4.0f, b.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(5.0f, b.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(6.0f, b.z, FLY_MATH_EPSILON);

    Vec3 c(-1.0f, 1.0f, -1.0f);
    EXPECT_NEAR(-1.0f, c.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(1.0f, c.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(-1.0f, c.z, FLY_MATH_EPSILON);

    Vec3 d = Vec3(Vec4(4.0f, 3.0f, 2.0f, 1.0f));
    EXPECT_NEAR(4.0f, d.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(3.0f, d.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(2.0f, d.z, FLY_MATH_EPSILON);
}

TEST(Vec3, Add)
{
    Vec3 a = {1.0f, 2.0f, 3.0f};
    Vec3 b = {2.0f, 1.0f, 3.0f};
    Vec3 c = a + b;
    EXPECT_NEAR(3.0f, c.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(3.0f, c.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(6.0f, c.z, FLY_MATH_EPSILON);

    a += b;
    EXPECT_NEAR(3.0f, a.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(3.0f, a.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(6.0f, c.z, FLY_MATH_EPSILON);
}

TEST(Vec3, Subtract)
{
    Vec3 a = {1.0f, 2.0f, 4.0f};
    Vec3 b = {3.0f, 4.0f, 2.0f};
    Vec3 c = b - a;
    EXPECT_NEAR(2.0f, c.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(2.0f, c.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(-2.0f, c.z, FLY_MATH_EPSILON);

    c -= c;
    EXPECT_NEAR(0.0f, c.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(0.0f, c.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(0.0f, c.z, FLY_MATH_EPSILON);
}

TEST(Vec3, Multiply)
{
    Vec3 a = {2.0f, 0.0f, 4.0f};
    Vec3 b = {9.0f, 127.0f, 0.5f};
    Vec3 c = a * b;
    EXPECT_NEAR(18.0f, c.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(0.0f, c.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(2.0f, c.z, FLY_MATH_EPSILON);

    c *= c;
    EXPECT_NEAR(324.0f, c.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(0.0f, c.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(4.0f, c.z, FLY_MATH_EPSILON);

    a *= 3.0f;
    EXPECT_NEAR(6.0f, a.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(0.0f, a.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(12.0f, a.z, FLY_MATH_EPSILON);

    Vec3 d = 3.0f * a;
    EXPECT_NEAR(18.0f, d.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(0.0f, d.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(36.0f, d.z, FLY_MATH_EPSILON);

    d = d * 0.5f;
    EXPECT_NEAR(9.0f, d.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(0.0f, d.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(18.0f, d.z, FLY_MATH_EPSILON);
}

TEST(Vec3, Divide)
{
    Vec3 a = {8.0f, 4.0f, 0.5f};
    Vec3 b = {2.0f, 0.5f, 0.25f};
    Vec3 c = a / b;
    EXPECT_NEAR(4.0f, c.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(8.0f, c.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(2.0f, c.z, FLY_MATH_EPSILON);

    c /= c;
    EXPECT_NEAR(1.0f, c.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(1.0f, c.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(1.0f, c.z, FLY_MATH_EPSILON);

    a /= 2.0f;
    EXPECT_NEAR(4.0f, a.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(2.0f, a.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(0.25f, a.z, FLY_MATH_EPSILON);

    Vec3 d = a / 2.0f;
    EXPECT_NEAR(2.0f, d.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(1.0f, d.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(0.125f, d.z, FLY_MATH_EPSILON);
}

TEST(Vec3, Subscript)
{
    Vec3 a = {1.0f, 2.0f, 3.0f};
    EXPECT_NEAR(a.x, a[0], FLY_MATH_EPSILON);
    EXPECT_NEAR(a.y, a[1], FLY_MATH_EPSILON);
    EXPECT_NEAR(a.z, a[2], FLY_MATH_EPSILON);
}

TEST(Vec3, Dot)
{
    Vec3 a = {3.0f, -6.0f, 1.0f};
    Vec3 b = {6.0f, 3.0f, -1.0f};
    Vec3 c = {1.0f, 1.0f, 2.0f};

    EXPECT_NEAR(-1.0f, Dot(a, b), FLY_MATH_EPSILON);
    EXPECT_NEAR(-1.0f, Dot(a, c), FLY_MATH_EPSILON);
    EXPECT_NEAR(7.0f, Dot(b, c), FLY_MATH_EPSILON);
}

TEST(Vec3, Cross)
{
    Vec3 x(1.0f, 0.0f, 0.0f);
    Vec3 y(0.0f, 1.0f, 0.0f);
    Vec3 z = Cross(x, y);

    EXPECT_NEAR(0.0f, z.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(0.0f, z.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(1.0f, z.z, FLY_MATH_EPSILON);

    Vec3 a(2, 3, 4);
    Vec3 b(5, 6, 7);
    Vec3 c = Cross(a, b);

    EXPECT_NEAR(-3.0f, c.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(6.0f, c.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(-3.0f, c.z, FLY_MATH_EPSILON);
}

TEST(Vec3, Length)
{
    Vec3 a(23.0f, 0.0f, 0.0f);
    EXPECT_NEAR(23.0f, Length(a), FLY_MATH_EPSILON);
    EXPECT_NEAR(23.0f * 23.0f, LengthSqr(a), FLY_MATH_EPSILON);

    Vec3 b(1.0f, 2.0f, 2.0f);
    EXPECT_NEAR(3.0f, Length(b), FLY_MATH_EPSILON);
    EXPECT_NEAR(9.0f, LengthSqr(b), FLY_MATH_EPSILON);
}

TEST(Vec3, Normalize)
{
    Vec3 a = Normalize(Vec3(23.0f, 0.0f, 0.0f));
    EXPECT_NEAR(1.0f, a.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(0.0f, a.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(0.0f, a.z, FLY_MATH_EPSILON);

    Vec3 b = Normalize(Vec3(1.0f, 2.0f, 2.0f));
    EXPECT_NEAR(1.0f / 3.0f, b.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(2.0f / 3.0f, b.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(2.0f / 3.0f, b.z, FLY_MATH_EPSILON);
}
