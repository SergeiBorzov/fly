#include <string.h>

#include "core/assert.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/memory.h"

#include "compress_image.h"
#include "export_image.h"
#include "image.h"

#define STBIW_ASSERT(x) FLY_ASSERT(x)
#define STBIW_MALLOC(size) (Fly::Alloc(size))
#define STBIW_REALLOC(p, newSize) (Fly::Realloc(p, newSize))
#define STBIW_FREE(p) (Fly::Free(p))
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define HALF_EXPONENT_MASK (0x7C00u)
#define HALF_MANTISSA_MASK (0x3FFu)
#define HALF_SIGN_MASK (0x8000u)

#define MIN(a, b) ((a) < (b) ? (a) : (b))

union u32f32
{
    u32 bits;
    f32 value;
};

static f32 HalfToFloat(u16 half)
{
    u16 sign = (half & HALF_SIGN_MASK) >> 15;
    u16 exponent = (half & HALF_EXPONENT_MASK) >> 10;
    u16 significand = half & HALF_MANTISSA_MASK;

    u32 f32Exponent = 0;
    u32 f32Significand = 0;
    u32 f32Sign = static_cast<u32>(sign) << 31;
    if (exponent == 0)
    {
        if (significand == 0)
        {
            f32Exponent = 0;
            f32Significand = 0;
        }
        else
        {
            // Subnormal
            exponent = 1;
            while ((significand & 0x0400u) == 0)
            {
                significand <<= 1;
                --exponent;
            }
            significand &= 0x03FFu;
            f32Exponent = static_cast<u32>(exponent + (127 - 15)) << 23;
            f32Significand = static_cast<u32>(significand) << 13;
        }
    }
    else if (exponent == 0x1F)
    {
        // NaN case
        f32Exponent = 0xFFu << 23;
        f32Significand =
            (significand != 0) ? (static_cast<u32>(significand) << 13) : 0;
    }
    else
    {
        f32Exponent = static_cast<u32>((exponent + (127 - 15))) << 23;
        f32Significand = static_cast<u32>(significand) << 13;
    }

    u32f32 res;
    res.bits = f32Sign | f32Exponent | f32Significand;

    return res.value;
}

static u8 ReinhardTonemap(f32 value)
{
    value = value / (1.0f + value);
    return static_cast<u8>(value * 255.0f + 0.5f);
}

namespace Fly
{

static Image TonemapHalf(const Image& image)
{
    FLY_ASSERT(image.storageType == ImageStorageType::Half);

    Image output;
    output.storageType = ImageStorageType::Byte;

    output.mem =
        static_cast<u8*>(Fly::Alloc(sizeof(u8) * image.width * image.height *
                                    image.channelCount * image.layerCount));
    output.data = output.mem;
    output.width = image.width;
    output.height = image.height;
    output.mipCount = 1;
    output.layerCount = image.layerCount;
    output.channelCount = image.channelCount;

    u16* imageData = reinterpret_cast<u16*>(image.data);

    for (u32 n = 0; n < image.layerCount; n++)
    {
        for (u32 i = 0; i < image.height; i++)
        {
            for (u32 j = 0; j < image.width; j++)
            {
                for (u32 k = 0; k < MIN(image.channelCount, 3); k++)
                {
                    f32 value = HalfToFloat(
                        imageData[n * image.height * image.width *
                                      image.channelCount +
                                  image.width * image.channelCount * i +
                                  image.channelCount * j + k]);

                    output.data[n * image.height * image.width *
                                    image.channelCount +
                                image.width * image.channelCount * i +
                                image.channelCount * j + k] =
                        ReinhardTonemap(value);
                }

                if (image.channelCount == 4)
                {
                    f32 value = HalfToFloat(
                        imageData[n * image.height * image.width *
                                      image.channelCount +
                                  image.width * image.channelCount * i +
                                  image.channelCount * j + 3]);

                    output.data[n * image.height * image.width *
                                    image.channelCount +
                                image.width * image.channelCount * i +
                                image.channelCount * j + 3] =
                        static_cast<u8>(value * 255.0f + 0.5f);
                }
            }
        }
    }

    return output;
}

bool ExportPNG(const char* path, const Image& image)
{
    return stbi_write_png(path, image.width, image.height * image.layerCount,
                          image.channelCount, image.data,
                          image.width * image.channelCount * sizeof(u8));
}

bool ExportBMP(const char* path, const Image& image)
{
    return stbi_write_bmp(path, image.width, image.height * image.layerCount,
                          image.channelCount, image.data);
}

bool ExportTGA(const char* path, const Image& image)
{
    return stbi_write_tga(path, image.width, image.height * image.layerCount,
                          image.channelCount, image.data);
}

bool ExportJPG(const char* path, const Image& image)
{
    return stbi_write_jpg(path, image.width, image.height * image.layerCount,
                          image.channelCount, image.data, 90);
}

bool ExportHDR(const char* path, const Image& image)
{
    return stbi_write_hdr(path, image.width, image.height * image.layerCount,
                          image.channelCount,
                          reinterpret_cast<const float*>(image.data));
}

bool ExportFBC1(const char* path, const Image& image)
{
    u64 size = 0;
    u8* data = CompressImage(image, CodecType::BC1, size);
    if (!data)
    {
        return false;
    }
    String8 str(reinterpret_cast<const char*>(data), size);
    if (!WriteStringToFile(str, path))
    {
        Free(data);
        return false;
    }
    Free(data);
    return true;
}

bool ExportFBC3(const char* path, const Image& image)
{
    u64 size = 0;
    u8* data = CompressImage(image, CodecType::BC3, size);
    if (!data)
    {
        return false;
    }
    String8 str(reinterpret_cast<const char*>(data), size);
    if (!WriteStringToFile(str, path))
    {
        Free(data);
        return false;
    }
    Free(data);
    return true;
}

bool ExportFBC4(const char* path, const Image& image)
{
    u64 size = 0;
    u8* data = CompressImage(image, CodecType::BC4, size);
    if (!data)
    {
        return false;
    }
    String8 str(reinterpret_cast<const char*>(data), size);
    if (!WriteStringToFile(str, path))
    {
        Free(data);
        return false;
    }
    Free(data);
    return true;
}

bool ExportFBC5(const char* path, const Image& image)
{
    u64 size = 0;
    u8* data = CompressImage(image, CodecType::BC5, size);
    if (!data)
    {
        return false;
    }
    String8 str(reinterpret_cast<const char*>(data), size);
    if (!WriteStringToFile(str, path))
    {
        Free(data);
        return false;
    }
    Free(data);
    return true;
}

bool ExportImage(const char* path, const Image& image)
{
    String8 pathStr = Fly::String8(path, strlen(path));

    Image exportImage = image;
    if (image.storageType == ImageStorageType::Half)
    {
        exportImage = TonemapHalf(image);
    }
    else if (image.storageType == ImageStorageType::Float)
    {
        if (pathStr.EndsWith(FLY_STRING8_LITERAL(".hdr")))
        {
            return ExportHDR(path, image);
        }
    }

    if (pathStr.EndsWith(FLY_STRING8_LITERAL(".png")))
    {
        return ExportPNG(path, exportImage);
    }
    else if (pathStr.EndsWith(FLY_STRING8_LITERAL(".jpg")) ||
             pathStr.EndsWith(FLY_STRING8_LITERAL(".jpeg")))
    {
        return ExportJPG(path, exportImage);
    }
    else if (pathStr.EndsWith(FLY_STRING8_LITERAL(".bmp")))
    {
        return ExportBMP(path, exportImage);
    }
    else if (pathStr.EndsWith(FLY_STRING8_LITERAL(".tga")))
    {
        return ExportTGA(path, exportImage);
    }
    else if (pathStr.EndsWith(FLY_STRING8_LITERAL(".fbc1")))
    {
        return ExportFBC1(path, exportImage);
    }
    else if (pathStr.EndsWith(FLY_STRING8_LITERAL(".fbc3")))
    {
        return ExportFBC3(path, exportImage);
    }
    else if (pathStr.EndsWith(FLY_STRING8_LITERAL(".fbc4")))
    {
        return ExportFBC4(path, exportImage);
    }
    else if (pathStr.EndsWith(FLY_STRING8_LITERAL(".fbc5")))
    {
        return ExportFBC5(path, exportImage);
    }

    if (image.storageType == ImageStorageType::Half)
    {
        FreeImage(exportImage);
    }

    return false;
}

} // namespace Fly
