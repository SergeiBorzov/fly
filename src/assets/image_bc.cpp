#include <string.h>
#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"

#include "core/assert.h"
#include "math/functions.h"

#include "image.h"
#include "image_bc.h"

namespace Fly
{

static void CopyImageBlock1Byte(u8 block[16], const u8* data, u32 width,
                                u32 height, u32 x, u32 y)
{
    for (u32 i = 0; i < 4; i++)
    {
        for (u32 j = 0; j < 4; j++)
        {
            u32 kh = Math::Min(y + i, height - 1);
            u32 kw = Math::Min(x + j, width - 1);

            block[4 * i + j] = data[width * kh + kw];
        }
    }
}

static void CopyImageBlock2Bytes(u16 block[16], const u16* data, u32 width,
                                 u32 height, u32 x, u32 y)
{
    for (u32 i = 0; i < 4; i++)
    {
        for (u32 j = 0; j < 4; j++)
        {
            u32 kh = Math::Min(y * 2 + i, height - 1);
            u32 kw = Math::Min(x * 2 + j, width - 1);

            block[4 * i + j] = data[width * kh + kw];
        }
    }
}

static void CopyImageBlock4Bytes(u32 block[16], const u32* data, u32 width,
                                 u32 height, u32 x, u32 y)
{
    for (u32 i = 0; i < 4; i++)
    {
        for (u32 j = 0; j < 4; j++)
        {
            u32 kh = Math::Min(y * 4 + i, height - 1);
            u32 kw = Math::Min(x * 4 + j, width - 1);

            block[4 * i + j] = data[width * kh + kw];
        }
    }
}

u64 SizeBlock8(u32 width, u32 height)
{
    return static_cast<u64>(Math::Ceil(width / 4.0f) *
                            Math::Ceil(height / 4.0f) * 8);
}

u64 SizeBlock16(u32 width, u32 height)
{
    return static_cast<u64>(Math::Ceil(width / 4.0f) *
                            Math::Ceil(height / 4.0f) * 16);
}

static bool CompressImageBC1(u8* dst, u64 dstSize, const Image& image)
{
    if (image.channelCount < 3 ||
        dstSize < SizeBlock8(image.width, image.height))
    {
        return false;
    }

    const u32 blockWidth = static_cast<u32>(Math::Ceil(image.width / 4.0f));
    const u32 blockHeight = static_cast<u32>(Math::Ceil(image.height / 4.0f));

    u32 block[16] = {0};
    for (u32 i = 0; i < blockHeight; i++)
    {
        for (u32 j = 0; j < blockWidth; j++)
        {
            CopyImageBlock4Bytes(block,
                                 reinterpret_cast<const u32*>(image.data),
                                 image.width, image.height, j, i);
            stb_compress_dxt_block(dst + i * blockWidth * 8 + j * 8,
                                   reinterpret_cast<unsigned char*>(block), 0,
                                   STB_DXT_NORMAL);
        }
    }

    return true;
}

static bool CompressImageBC3(u8* dst, u64 dstSize, const Image& image)
{
    if (image.channelCount != 4 ||
        dstSize < SizeBlock16(image.width, image.height))
    {
        return false;
    }

    const u32 blockWidth = static_cast<u32>(Math::Ceil(image.width / 4.0f));
    const u32 blockHeight = static_cast<u32>(Math::Ceil(image.height / 4.0f));

    u32 block[16] = {0};
    for (u32 i = 0; i < blockHeight; i++)
    {
        for (u32 j = 0; j < blockWidth; j++)
        {
            CopyImageBlock4Bytes(block,
                                 reinterpret_cast<const u32*>(image.data),
                                 image.width, image.height, j, i);
            stb_compress_dxt_block(dst + i * blockWidth * 16 + j * 16,
                                   reinterpret_cast<unsigned char*>(block), 1,
                                   STB_DXT_NORMAL);
        }
    }

    return true;
}

static bool CompressImageBC4(u8* dst, u64 dstSize, const Image& image)
{
    if (image.channelCount != 1 ||
        dstSize < SizeBlock8(image.width, image.height))
    {
        return false;
    }

    const u32 blockWidth = static_cast<u32>(Math::Ceil(image.width / 4.0f));
    const u32 blockHeight = static_cast<u32>(Math::Ceil(image.height / 4.0f));

    u8 block[16] = {0};
    for (u32 i = 0; i < blockHeight; i++)
    {
        for (u32 j = 0; j < blockWidth; j++)
        {
            CopyImageBlock1Byte(block, image.data, image.width, image.height, j,
                                i);
            stb_compress_bc4_block(dst + i * blockWidth * 8 + j * 8,
                                   reinterpret_cast<unsigned char*>(block));
        }
    }
    return true;
}

static bool CompressImageBC5(u8* dst, u64 dstSize, const Image& image)
{
    if (image.channelCount != 2 ||
        dstSize < SizeBlock16(image.width, image.height))
    {
        return false;
    }

    const u32 blockWidth = static_cast<u32>(Math::Ceil(image.width / 4.0f));
    const u32 blockHeight = static_cast<u32>(Math::Ceil(image.height / 4.0f));

    u16 block[16] = {0};
    for (u32 i = 0; i < blockHeight; i++)
    {
        for (u32 j = 0; j < blockWidth; j++)
        {
            CopyImageBlock2Bytes(block,
                                 reinterpret_cast<const u16*>(image.data),
                                 image.width, image.height, j, i);
            stb_compress_bc5_block(dst + i * blockWidth * 16 + j * 16,
                                   reinterpret_cast<unsigned char*>(block));
        }
    }
    return true;
}

bool CompressImage(u8* dst, u64 dstSize, const Image& image, CodecType codec)
{
    FLY_ASSERT(dst);
    FLY_ASSERT(dstSize > 0);
    switch (codec)
    {
        case CodecType::BC1:
        {
            return CompressImageBC1(dst, dstSize, image);
        }
        case CodecType::BC3:
        {
            return CompressImageBC3(dst, dstSize, image);
        }
        case CodecType::BC4:
        {
            return CompressImageBC4(dst, dstSize, image);
        }
        case CodecType::BC5:
        {
            return CompressImageBC5(dst, dstSize, image);
        }
        case CodecType::Invalid:
        {
            return false;
        }
    }
    return true;
}

} // namespace Fly
