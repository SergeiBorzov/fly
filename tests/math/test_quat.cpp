#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/math/quat.h"

using namespace Fly::Math;

TEST(Quat, Constructors)
{
    Quat identity;
    EXPECT_NEAR(0.0f, identity.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(0.0f, identity.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(0.0f, identity.z, FLY_MATH_EPSILON);
    EXPECT_NEAR(1.0f, identity.w, FLY_MATH_EPSILON);

    Quat q(-1.0f, 2.0f, 3.0f, -4.0f);
    EXPECT_NEAR(-1.0f, q.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(2.0f, q.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(3.0f, q.z, FLY_MATH_EPSILON);
    EXPECT_NEAR(-4.0f, q.w, FLY_MATH_EPSILON);
}

TEST(Quat, Normalize)
{
    Quat q = Normalize(Quat(1.0f, 4.0f, 4.0f, 4.0f));
    EXPECT_NEAR(1.0f / 7.0f, q.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(4.0f / 7.0f, q.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(4.0f / 7.0f, q.z, FLY_MATH_EPSILON);
    EXPECT_NEAR(4.0f / 7.0f, q.w, FLY_MATH_EPSILON);
}

TEST(Quat, AngleAxis)
{
    Vec3 forward(0.0f, 0.0f, 1.0f);
    Vec3 right(1.0f, 0.0f, 0.0f);

    Vec3 b = AngleAxis(Vec3(0.0f, 1.0f, 0.0f), 90.0f) * right;
    EXPECT_NEAR(0.0f, b.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(0.0f, b.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(-1.0f, b.z, FLY_MATH_EPSILON);

    Vec3 u = AngleAxis(Vec3(0.0f, 0.0f, 1.0f), 90.0f) * right;
    EXPECT_NEAR(0.0f, u.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(1.0f, u.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(0.0f, u.z, FLY_MATH_EPSILON);

    Vec3 d = AngleAxis(Vec3(1.0f, 0.0f, 0.0f), 90.0f) * forward;
    EXPECT_NEAR(0.0f, d.x, FLY_MATH_EPSILON);
    EXPECT_NEAR(-1.0f, d.y, FLY_MATH_EPSILON);
    EXPECT_NEAR(0.0f, d.z, FLY_MATH_EPSILON);
}
