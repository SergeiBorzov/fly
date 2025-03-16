#include <math.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "math/functions.h"

TEST(Trigonometry, Sin)
{
    for (int i = 0; i < 2048; i++)
    {
        f32 value = (HLS_MATH_TWO_PI * 125.0f) + HLS_MATH_TWO_PI * i / 2048;
        EXPECT_NEAR(sinf(value), Hls::Math::Sin(value), 1e-4);
    }
}

TEST(Trigonometry, Cos)
{
    for (int i = 0; i < 2048; i++)
    {
        f32 value = HLS_MATH_TWO_PI * i / 2048;
        EXPECT_NEAR(cosf(value), Hls::Math::Cos(value), 1e-4);
    }
}
