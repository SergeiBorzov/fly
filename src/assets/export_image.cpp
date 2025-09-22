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

namespace Fly
{

bool ExportPNG(const char* path, const Image& image)
{
    return stbi_write_png(path, image.width, image.height, image.channelCount,
                          image.data,
                          image.width * image.channelCount * sizeof(u8));
}

bool ExportBMP(const char* path, const Image& image)
{
    return stbi_write_bmp(path, image.width, image.height, image.channelCount,
                          image.data);
}

bool ExportTGA(const char* path, const Image& image)
{
    return stbi_write_tga(path, image.width, image.height, image.channelCount,
                          image.data);
}

bool ExportJPG(const char* path, const Image& image)
{
    return stbi_write_jpg(path, image.width, image.height, image.channelCount,
                          image.data, 90);
}

bool ExportHDR(const char* path, const Image& image)
{
    return stbi_write_hdr(path, image.width, image.height, image.channelCount,
                          reinterpret_cast<const float*>(image.data));
}

bool ExportFBC1(const char* path, const Image& image, bool generateMips)
{
    u64 size = 0;
    u8* data = CompressImage(image, CodecType::BC1, generateMips, size);
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

bool ExportFBC3(const char* path, const Image& image, bool generateMips)
{
    u64 size = 0;
    u8* data = CompressImage(image, CodecType::BC3, generateMips, size);
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

bool ExportFBC4(const char* path, const Image& image, bool generateMips)
{
    u64 size = 0;
    u8* data = CompressImage(image, CodecType::BC4, generateMips, size);
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

bool ExportFBC5(const char* path, const Image& image, bool generateMips)
{
    u64 size = 0;
    u8* data = CompressImage(image, CodecType::BC5, generateMips, size);
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

bool ExportImage(const char* path, const Image& image, bool generateMips)
{
    String8 pathStr = Fly::String8(path, strlen(path));

    if (pathStr.EndsWith(FLY_STRING8_LITERAL(".png")))
    {
        return ExportPNG(path, image);
    }
    else if (pathStr.EndsWith(FLY_STRING8_LITERAL(".jpg")) ||
             pathStr.EndsWith(FLY_STRING8_LITERAL(".jpeg")))
    {
        return ExportJPG(path, image);
    }
    else if (pathStr.EndsWith(FLY_STRING8_LITERAL(".bmp")))
    {
        return ExportBMP(path, image);
    }
    else if (pathStr.EndsWith(FLY_STRING8_LITERAL(".tga")))
    {
        return ExportTGA(path, image);
    }
    else if (pathStr.EndsWith(FLY_STRING8_LITERAL(".hdr")))
    {
        return ExportHDR(path, image);
    }
    else if (pathStr.EndsWith(FLY_STRING8_LITERAL(".fbc1")))
    {
        return ExportFBC1(path, image, generateMips);
    }
    else if (pathStr.EndsWith(FLY_STRING8_LITERAL(".fbc3")))
    {
        return ExportFBC3(path, image, generateMips);
    }
    else if (pathStr.EndsWith(FLY_STRING8_LITERAL(".fbc4")))
    {
        return ExportFBC4(path, image, generateMips);
    }
    else if (pathStr.EndsWith(FLY_STRING8_LITERAL(".fbc5")))
    {
        return ExportFBC5(path, image, generateMips);
    }

    return false;
}

} // namespace Fly
