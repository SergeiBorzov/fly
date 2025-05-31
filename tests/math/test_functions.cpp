#include <math.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/math/functions.h"

TEST(Trigonometry, Sin)
{
    for (i32 i = 0; i < 2048; i++)
    {
        f32 value = (FLY_MATH_TWO_PI * 125.0f) + FLY_MATH_TWO_PI * i / 2048;
        EXPECT_NEAR(sinf(value), Fly::Math::Sin(value), 1e-4);
    }
}

TEST(Trigonometry, Cos)
{
    for (i32 i = 0; i < 2048; i++)
    {
        f32 value = FLY_MATH_TWO_PI * i / 2048;
        EXPECT_NEAR(cosf(value), Fly::Math::Cos(value), 1e-4);
    }
}

TEST(Trigonometry, Tan)
{
    for (i32 i = 0; i < 2048; i++)
    {
        f32 value = FLY_MATH_TWO_PI * i / 2048;
        EXPECT_NEAR(tanf(value), Fly::Math::Tan(value), 1e-4);
    }
}

TEST(Basic, InvSqrt)
{
    for (i32 i = 1; i < 2048; i++)
    {
        f32 value = static_cast<f32>(i);
        EXPECT_NEAR(1.0f / sqrtf(value), Fly::Math::InvSqrt(value), 1e-4);
    }
}
