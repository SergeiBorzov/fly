#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/math/vec.h"

using namespace Hls::Math;

TEST(Vec4, Constructors)
{
    Vec4 zeroVector;
    EXPECT_NEAR(0.0f, zeroVector.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, zeroVector.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, zeroVector.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, zeroVector.z, HLS_MATH_EPSILON);

    Vec4 singleValueVector(12345.0f);
    EXPECT_NEAR(12345.0f, singleValueVector.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(12345.0f, singleValueVector.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(12345.0f, singleValueVector.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(12345.0f, singleValueVector.w, HLS_MATH_EPSILON);

    Vec4 a({1.0f, 2.0f, 3.0f}, 4.0f);
    EXPECT_NEAR(1.0f, a.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(2.0f, a.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(3.0f, a.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(4.0f, a.w, HLS_MATH_EPSILON);

    Vec4 b(4.0f, {5.0f, 6.0f, 7.0f});
    EXPECT_NEAR(4.0f, b.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(5.0f, b.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(6.0f, b.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(7.0f, b.w, HLS_MATH_EPSILON);

    Vec4 c({-1.0f, 1.0f}, {1.0f, -1.0f});
    EXPECT_NEAR(-1.0f, c.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(1.0f, c.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(1.0f, c.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(-1.0f, c.w, HLS_MATH_EPSILON);

    Vec4 d(1.0f, 2.0f, {3.0f, 4.0f});
    EXPECT_NEAR(1.0f, d.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(2.0f, d.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(3.0f, d.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(4.0f, d.w, HLS_MATH_EPSILON);

    Vec4 e(1.0f, {2.0f, 3.0f}, 4.0f);
    EXPECT_NEAR(1.0f, e.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(2.0f, e.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(3.0f, e.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(4.0f, e.w, HLS_MATH_EPSILON);

    Vec4 f({1.0f, 2.0f}, 3.0f, 4.0f);
    EXPECT_NEAR(1.0f, f.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(2.0f, f.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(3.0f, f.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(4.0f, f.w, HLS_MATH_EPSILON);

    Vec4 g(1.0f, 2.0f, 3.0f, 4.0f);
    EXPECT_NEAR(1.0f, g.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(2.0f, g.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(3.0f, g.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(4.0f, g.w, HLS_MATH_EPSILON);
}

TEST(Vec4, Add)
{
    Vec4 a = {1.0f, 2.0f, 3.0f, 4.0f};
    Vec4 b = {2.0f, 1.0f, 3.0f, 4.0f};
    Vec4 c = a + b;
    EXPECT_NEAR(3.0f, c.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(3.0f, c.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(6.0f, c.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(8.0f, c.w, HLS_MATH_EPSILON);

    a += b;
    EXPECT_NEAR(3.0f, a.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(3.0f, a.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(6.0f, c.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(8.0f, c.w, HLS_MATH_EPSILON);
}

TEST(Vec4, Subtract)
{
    Vec4 a = {1.0f, 2.0f, 4.0f, 2.0f};
    Vec4 b = {3.0f, 4.0f, 2.0f, -2.0f};
    Vec4 c = b - a;
    EXPECT_NEAR(2.0f, c.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(2.0f, c.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(-2.0f, c.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(-4.0f, c.w, HLS_MATH_EPSILON);

    c -= c;
    EXPECT_NEAR(0.0f, c.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, c.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, c.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, c.w, HLS_MATH_EPSILON);
}

TEST(Vec4, Multiply)
{
    Vec4 a = {2.0f, 0.0f, 4.0f, 2.0f};
    Vec4 b = {9.0f, 127.0f, 0.5f, -1.0f};
    Vec4 c = a * b;
    EXPECT_NEAR(18.0f, c.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, c.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(2.0f, c.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(-2.0f, c.w, HLS_MATH_EPSILON);

    c *= c;
    EXPECT_NEAR(324.0f, c.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, c.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(4.0f, c.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(4.0f, c.w, HLS_MATH_EPSILON);

    a *= 3.0f;
    EXPECT_NEAR(6.0f, a.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, a.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(12.0f, a.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(6.0f, a.w, HLS_MATH_EPSILON);

    Vec4 d = 3.0f * a;
    EXPECT_NEAR(18.0f, d.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, d.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(36.0f, d.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(18.0f, d.w, HLS_MATH_EPSILON);

    d = d * 0.5f;
    EXPECT_NEAR(9.0f, d.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, d.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(18.0f, d.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(9.0f, d.w, HLS_MATH_EPSILON);
}

TEST(Vec4, Divide)
{
    Vec4 a = {8.0f, 4.0f, 0.5f, 1.0f};
    Vec4 b = {2.0f, 0.5f, 0.25f, 1.0f};
    Vec4 c = a / b;
    EXPECT_NEAR(4.0f, c.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(8.0f, c.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(2.0f, c.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(1.0f, c.w, HLS_MATH_EPSILON);

    c /= c;
    EXPECT_NEAR(1.0f, c.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(1.0f, c.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(1.0f, c.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(1.0f, c.w, HLS_MATH_EPSILON);

    a /= 2.0f;
    EXPECT_NEAR(4.0f, a.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(2.0f, a.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.25f, a.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.5f, a.w, HLS_MATH_EPSILON);

    Vec4 d = a / 2.0f;
    EXPECT_NEAR(2.0f, d.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(1.0f, d.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.125f, d.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.25f, d.w, HLS_MATH_EPSILON);
}

TEST(Vec4, Subscript)
{
    Vec4 a = {1.0f, 2.0f, 3.0f, 4.0f};
    EXPECT_NEAR(a.x, a[0], HLS_MATH_EPSILON);
    EXPECT_NEAR(a.y, a[1], HLS_MATH_EPSILON);
    EXPECT_NEAR(a.z, a[2], HLS_MATH_EPSILON);
    EXPECT_NEAR(a.w, a[3], HLS_MATH_EPSILON);
}

TEST(Vec4, Dot)
{
    Vec4 a = {3.0f, -6.0f, 1.0f, 0.0f};
    Vec4 b = {6.0f, 3.0f, -1.0f, 5.0f};
    Vec4 c = {1.0f, 1.0f, 2.0f, 1.0f};

    EXPECT_NEAR(-1.0f, Dot(a, b), HLS_MATH_EPSILON);
    EXPECT_NEAR(-1.0f, Dot(a, c), HLS_MATH_EPSILON);
    EXPECT_NEAR(12.0f, Dot(b, c), HLS_MATH_EPSILON);
}

TEST(Vec4, Length)
{
    Vec4 a(23.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_NEAR(23.0f, Length(a), HLS_MATH_EPSILON);
    EXPECT_NEAR(23.0f * 23.0f, LengthSqr(a), HLS_MATH_EPSILON);

    Vec4 b(1.0f, 4.0f, 4.0f, 4.0f);
    EXPECT_NEAR(7.0f, Length(b), HLS_MATH_EPSILON);
    EXPECT_NEAR(49.0f, LengthSqr(b), HLS_MATH_EPSILON);
}

TEST(Vec4, Normalize)
{
    Vec4 a = Normalize(Vec4(23.0f, 0.0f, 0.0f, 0.0f));
    EXPECT_NEAR(1.0f, a.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, a.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, a.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(0.0f, a.w, HLS_MATH_EPSILON);

    Vec4 b = Normalize(Vec4(1.0f, 4.0f, 4.0f, 4.0f));
    EXPECT_NEAR(1.0f / 7.0f, b.x, HLS_MATH_EPSILON);
    EXPECT_NEAR(4.0f / 7.0f, b.y, HLS_MATH_EPSILON);
    EXPECT_NEAR(4.0f / 7.0f, b.z, HLS_MATH_EPSILON);
    EXPECT_NEAR(4.0f / 7.0f, b.w, HLS_MATH_EPSILON);
}
