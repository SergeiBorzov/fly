#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "math/vec.h"

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
