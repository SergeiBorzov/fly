#include <string.h>
#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"

#include "core/assert.h"
#include "math/functions.h"

#include "image.h"
#include "image_bc.h"

// BC1, BC4
static u64 SizeBlock8(u32 width, u32 height)
{
    return static_cast<u64>(Fly::Math::Ceil(width / 4.0f) *
                            Fly::Math::Ceil(height / 4.0f) * 8);
}

// BC3, BC5
static u64 SizeBlock16(u32 width, u32 height)
{
    return static_cast<u64>(Fly::Math::Ceil(width / 4.0f) *
                            Fly::Math::Ceil(height / 4.0f) * 8);
}

namespace Fly
{

void CompressImageBC1(u8* dest, u64 destSize, const Image& image)
{
    FLY_ASSERT(dest);
    FLY_ASSERT(image.channelCount >= 3);
    FLY_ASSERT(destSize >= SizeBlock8(image.width, image.height));
}

void CompressImageBC3(u8* dest, u64 destSize, const Image& image)
{
    FLY_ASSERT(dest);
    FLY_ASSERT(image.channelCount == 4);
    FLY_ASSERT(destSize >= SizeBlock16(image.width, image.height));
}

void CompressImageBC4(u8* dest, u64 destSize, const Image& image)
{
    FLY_ASSERT(dest);
    FLY_ASSERT(image.channelCount == 1);
    FLY_ASSERT(destSize >= SizeBlock8(image.width, image.height));
}

void CompressImageBC5(u8* dest, u64 destSize, const Image& image)
{
    FLY_ASSERT(dest);
    FLY_ASSERT(image.channelCount == 2);
    FLY_ASSERT(destSize >= SizeBlock16(image.width, image.height));
}

} // namespace Fly
