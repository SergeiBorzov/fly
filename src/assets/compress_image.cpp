#include <string.h>

#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"

#include "core/assert.h"
#include "core/memory.h"
#include "math/functions.h"

#include "compress_image.h"
#include "image.h"
#include "transform_image.h"

static u32 Log2(u32 x)
{
    u32 result = 0;
    while (x >>= 1)
    {
        ++result;
    }
    return result;
}

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

static u64 SizeBlock8(u32 width, u32 height)
{
    return static_cast<u64>(Math::Ceil(width / 4.0f) *
                            Math::Ceil(height / 4.0f) * 8);
}

static u64 SizeBlock16(u32 width, u32 height)
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

const char* CodecToExtension(CodecType codec)
{
    switch (codec)
    {
        case CodecType::BC1:
        {
            return ".fbc1";
        }
        case CodecType::BC3:
        {
            return ".fbc3";
        }
        case CodecType::BC4:
        {
            return ".fbc4";
        }
        case CodecType::BC5:
        {
            return ".fbc5";
        }
        default:
        {
            return nullptr;
        }
    }
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

u64 GetCompressedImageSize(u32 width, u32 height, CodecType codec)
{
    switch (codec)
    {
        case CodecType::BC1:
        case CodecType::BC4:
        {
            return SizeBlock8(width, height);
            break;
        }
        case CodecType::BC3:
        case CodecType::BC5:
        {
            return SizeBlock16(width, height);
            break;
        }
        default:
        {
            return 0;
        }
    }
}

u8* CompressImage(const Image& image, CodecType codec, bool generateMips,
                  u64& totalSize)
{
    u32 mipCount = 1;
    if (generateMips)
    {
        mipCount = Log2(Math::Max(image.width, image.height)) + 1;
    }

    u32 width = static_cast<u32>(Math::Ceil(image.width / 4.0f)) * 4;
    u32 height = static_cast<u32>(Math::Ceil(image.height / 4.0f)) * 4;

    u64 imagesSize = 0;
    for (u32 i = 0; i < mipCount; i++)
    {
        imagesSize += GetCompressedImageSize(width, height, codec);
        width = (width > 1) ? width / 2 : 1;
        height = (height > 1) ? height / 2 : 1;
    }

    totalSize =
        sizeof(u32) + mipCount * sizeof(MipRow) + sizeof(u8) * imagesSize;
    u8* data = static_cast<u8*>(Fly::Alloc(totalSize));

    *(reinterpret_cast<u32*>(data)) = mipCount;
    u64 offset = sizeof(u32) + mipCount * sizeof(MipRow);
    width = static_cast<u32>(Math::Ceil(image.width / 4.0f)) * 4;
    height = static_cast<u32>(Math::Ceil(image.height / 4.0f)) * 4;
    for (u32 i = 0; i < mipCount; i++)
    {
        MipRow* mipRow =
            reinterpret_cast<MipRow*>(data + sizeof(u32) + sizeof(MipRow) * i);
        mipRow->width = width;
        mipRow->height = height;
        mipRow->offset = offset;

        const Image* target = &image;
        Image targetImage;
        if (i > 0)
        {
            targetImage.width = width;
            targetImage.height = height;
            targetImage.channelCount = image.channelCount;
            targetImage.mem = static_cast<u8*>(Fly::Alloc(
                sizeof(u8) * width * height * targetImage.channelCount));
            targetImage.data = targetImage.mem;

            if (!ResizeImageSRGB(image, targetImage))
            {
                FreeImage(targetImage);
                return nullptr;
            }
            target = &targetImage;
        }

        u8* dst = data + offset;
        u64 dstSize = GetCompressedImageSize(width, height, codec);
        if (!CompressImage(dst, dstSize, *target, codec))
        {
            if (i > 0)
            {
                FreeImage(targetImage);
            }
            Free(data);
            return nullptr;
        }
        if (i > 0)
        {
            FreeImage(targetImage);
        }

        offset += dstSize;
        width = (width > 1) ? width / 2 : 1;
        height = (height > 1) ? height / 2 : 1;
    }

    return data;
}

} // namespace Fly
