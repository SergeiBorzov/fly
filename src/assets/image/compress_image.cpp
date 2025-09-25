#include <stdio.h>
#include <string.h>

#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"

#include "core/assert.h"
#include "core/memory.h"

#include "compress_image.h"
#include "image.h"
#include "transform_image.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

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
            u32 kh = MIN(y + i, height - 1);
            u32 kw = MAX(x + j, width - 1);

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
            u32 kh = MIN(y * 2 + i, height - 1);
            u32 kw = MIN(x * 2 + j, width - 1);

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
            u32 kh = MIN(y * 4 + i, height - 1);
            u32 kw = MIN(x * 4 + j, width - 1);

            block[4 * i + j] = data[width * kh + kw];
        }
    }
}

static u64 SizeBlock8(u32 width, u32 height)
{
    return ((width + 3) / 4) * ((height + 3) / 4) * 8;
}

static u64 SizeBlock16(u32 width, u32 height)
{
    return ((width + 3) / 4) * ((height + 3) / 4) * 16;
}

static bool CompressImageBC1(u8* dst, u64 dstSize, const Image& image, u8 layer)
{
    if (image.channelCount < 3 ||
        dstSize < SizeBlock8(image.width, image.height))
    {
        return false;
    }

    const u8* srcData = reinterpret_cast<const u8*>(image.mem) +
                        image.width * image.height * image.channelCount * layer;

    const u32 blockWidth = (image.width + 3) / 4;
    const u32 blockHeight = (image.height + 3) / 4;

    u32 block[16] = {0};
    for (u32 i = 0; i < blockHeight; i++)
    {
        for (u32 j = 0; j < blockWidth; j++)
        {
            CopyImageBlock4Bytes(block, reinterpret_cast<const u32*>(srcData),
                                 image.width, image.height, j, i);
            stb_compress_dxt_block(dst + (i * blockWidth + j) * 8,
                                   reinterpret_cast<unsigned char*>(block), 0,
                                   STB_DXT_NORMAL);
        }
    }

    return true;
}

static bool CompressImageBC3(u8* dst, u64 dstSize, const Image& image, u8 layer)
{
    if (image.channelCount != 4 ||
        dstSize < SizeBlock16(image.width, image.height))
    {
        return false;
    }

    const u8* srcData = reinterpret_cast<const u8*>(image.data) +
                        image.width * image.height * image.channelCount * layer;

    const u32 blockWidth = (image.width + 3) / 4;
    const u32 blockHeight = (image.height + 3) / 4;

    u32 block[16] = {0};
    for (u32 i = 0; i < blockHeight; i++)
    {
        for (u32 j = 0; j < blockWidth; j++)
        {
            CopyImageBlock4Bytes(block, reinterpret_cast<const u32*>(srcData),
                                 image.width, image.height, j, i);
            stb_compress_dxt_block(dst + i * blockWidth * 16 + j * 16,
                                   reinterpret_cast<unsigned char*>(block), 1,
                                   STB_DXT_NORMAL);
        }
    }

    return true;
}

static bool CompressImageBC4(u8* dst, u64 dstSize, const Image& image, u8 layer)
{
    if (image.channelCount != 1 ||
        dstSize < SizeBlock8(image.width, image.height))
    {
        return false;
    }

    const u8* srcData = reinterpret_cast<const u8*>(image.data) +
                        image.width * image.height * image.channelCount * layer;

    const u32 blockWidth = (image.width + 3) / 4;
    const u32 blockHeight = (image.height + 3) / 4;

    u8 block[16] = {0};
    for (u32 i = 0; i < blockHeight; i++)
    {
        for (u32 j = 0; j < blockWidth; j++)
        {
            CopyImageBlock1Byte(block, srcData, image.width, image.height, j,
                                i);
            stb_compress_bc4_block(dst + i * blockWidth * 8 + j * 8,
                                   reinterpret_cast<unsigned char*>(block));
        }
    }
    return true;
}

static bool CompressImageBC5(u8* dst, u64 dstSize, const Image& image, u8 layer)
{
    if (image.channelCount != 2 ||
        dstSize < SizeBlock16(image.width, image.height))
    {
        return false;
    }

    const u8* srcData = reinterpret_cast<const u8*>(image.data) +
                        image.width * image.height * image.channelCount * layer;

    const u32 blockWidth = (image.width + 3) / 4;
    const u32 blockHeight = (image.height + 3) / 4;

    u16 block[16] = {0};
    for (u32 i = 0; i < blockHeight; i++)
    {
        for (u32 j = 0; j < blockWidth; j++)
        {
            CopyImageBlock2Bytes(block, reinterpret_cast<const u16*>(srcData),
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

ImageStorageType CodecToImageStorageType(CodecType codec)
{
    switch (codec)
    {
        case CodecType::BC1:
        {
            return ImageStorageType::Block8;
        }
        case CodecType::BC3:
        {
            return ImageStorageType::Block16;
        }
        case CodecType::BC4:
        {
            return ImageStorageType::Block8;
        }
        case CodecType::BC5:
        {
            return ImageStorageType::Block16;
        }
        default:
        {
            FLY_ASSERT(false);
            return ImageStorageType::Block16;
        }
    }
}

bool CompressImage(u8* dst, u64 dstSize, const Image& image, u8 layer,
                   CodecType codec)
{
    FLY_ASSERT(dst);
    FLY_ASSERT(dstSize > 0);
    switch (codec)
    {
        case CodecType::BC1:
        {
            return CompressImageBC1(dst, dstSize, image, layer);
        }
        case CodecType::BC3:
        {
            return CompressImageBC3(dst, dstSize, image, layer);
        }
        case CodecType::BC4:
        {
            return CompressImageBC4(dst, dstSize, image, layer);
        }
        case CodecType::BC5:
        {
            return CompressImageBC5(dst, dstSize, image, layer);
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
        }
        case CodecType::BC3:
        case CodecType::BC5:
        {
            return SizeBlock16(width, height);
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
    ImageStorageType storageType = CodecToImageStorageType(codec);
    u32 mipCount = 1;
    if (generateMips)
    {
        mipCount = Log2(MAX(image.width, image.height)) + 1;
    }

    const u32 width = ((image.width + 3) / 4) * 4;
    const u32 height = ((image.height + 3) / 4) * 4;

    u64 imagesSize = 0;
    u32 mipWidth = width;
    u32 mipHeight = height;
    for (u32 i = 0; i < mipCount; i++)
    {
        imagesSize += GetCompressedImageSize(mipWidth, mipHeight, codec);
        mipWidth = (mipWidth > 1) ? mipWidth >> 1 : 1;
        mipHeight = (mipHeight > 1) ? mipHeight >> 1 : 1;
    }
    imagesSize *= image.layerCount;

    totalSize =
        sizeof(ImageHeader) + mipCount * sizeof(ImageLayerRow) + imagesSize;
    u8* data = static_cast<u8*>(Fly::Alloc(totalSize));

    ImageHeader* header = reinterpret_cast<ImageHeader*>(data);
    header->storageType = storageType;
    header->channelCount = image.channelCount;
    header->mipCount = mipCount;
    header->layerCount = image.layerCount;

    u64 offset = sizeof(ImageHeader) + mipCount * sizeof(ImageLayerRow);
    mipWidth = width;
    mipHeight = height;
    for (u32 j = 0; j < mipCount; j++)
    {
        ImageLayerRow* layerRow = reinterpret_cast<ImageLayerRow*>(
            data + sizeof(ImageHeader) + j * sizeof(ImageLayerRow));
        layerRow->offset = offset;
        layerRow->width = mipWidth;
        layerRow->height = mipHeight;
        layerRow->size = GetCompressedImageSize(mipWidth, mipHeight, codec);

        const Image* target = &image;
        Image targetImage;
        if (j > 0)
        {
            Image targetImage;
            if (!ResizeImageSRGB(image, mipWidth, mipHeight, targetImage))
            {
                FreeImage(targetImage);
                return nullptr;
            }
            target = &targetImage;
        }

        for (u32 i = 0; i < image.layerCount; i++)
        {
            u8* dst = data + offset;
            u64 dstSize = GetCompressedImageSize(mipWidth, mipHeight, codec);
            if (!CompressImage(dst, dstSize, *target, i, codec))
            {
                if (j > 0)
                {
                    FreeImage(targetImage);
                }
                FreeImage(targetImage);
                return nullptr;
            }
            if (j > 0)
            {
                FreeImage(targetImage);
            }

            offset += dstSize;
        }

        mipWidth = (mipWidth > 1) ? mipWidth >> 1 : 1;
        mipHeight = (mipHeight > 1) ? mipHeight >> 1 : 1;
    }

    return data;
}

} // namespace Fly
