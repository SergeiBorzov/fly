#include <string.h>

#include "image.h"
#include "import_image.h"

#include "core/assert.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/memory.h"
#include "core/string8.h"

#define STBI_ASSERT(x) FLY_ASSERT(x)
#define STBI_MALLOC(size) (Fly::Alloc(size))
#define STBI_REALLOC(p, newSize) (Fly::Realloc(p, newSize))
#define STBI_FREE(p) (Fly::Free(p))
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <tinyexr.h>

namespace Fly
{

bool LoadCompressedImageFromFile(String8 path, Image& image)
{
    FLY_ASSERT(path);

    u64 size = 0;
    u8* bytes = ReadFileToByteArray(path.Data(), size);
    image.mem = bytes;

    ImageHeader* header = reinterpret_cast<ImageHeader*>(bytes);
    image.storageType = header->storageType;
    image.channelCount = header->channelCount;
    image.mipCount = header->mipCount;
    image.layerCount = header->layerCount;

    ImageLayerRow* layerRow =
        reinterpret_cast<ImageLayerRow*>(bytes + sizeof(ImageHeader));
    image.width = layerRow->width;
    image.height = layerRow->height;
    image.data = bytes + layerRow->offset;

    return true;
}

bool LoadExrImageFromFile(String8 path, Image& image)
{
    FLY_ASSERT(path);

    EXRVersion exrVersion;

    int ret = ParseEXRVersionFromFile(&exrVersion, path.Data());
    if (ret != 0)
    {
        return false;
    }

    FLY_ASSERT(exrVersion.multipart == 0);

    EXRHeader exrHeader;
    InitEXRHeader(&exrHeader);

    const char* err = nullptr;
    ret = ParseEXRHeaderFromFile(&exrHeader, &exrVersion, path.Data(), &err);
    if (ret != 0)
    {
        FreeEXRErrorMessage(err); // free's buffer for an error message
        return false;
    }

    // Channels can be in any order in exr
    i32 channelMap[] = {-1, -1, -1, -1};
    for (i32 i = 0; i < exrHeader.num_channels; i++)
    {
        if (strcmp(exrHeader.channels[i].name, "R") == 0)
        {
            channelMap[0] = i;
        }
        else if (strcmp(exrHeader.channels[i].name, "G") == 0)
        {
            channelMap[1] = i;
        }
        else if (strcmp(exrHeader.channels[i].name, "B") == 0)
        {
            channelMap[2] = i;
        }
        else if (strcmp(exrHeader.channels[i].name, "A") == 0)
        {
            channelMap[3] = i;
        }
    }

    for (int i = 0; i < exrHeader.num_channels; i++)
    {
        if (exrHeader.pixel_types[i] != TINYEXR_PIXELTYPE_HALF)
        {
            exrHeader.requested_pixel_types[i] = TINYEXR_PIXELTYPE_HALF;
        }
    }

    EXRImage exrImage;
    InitEXRImage(&exrImage);
    ret = LoadEXRImageFromFile(&exrImage, &exrHeader, path.Data(), &err);
    if (ret != 0)
    {
        FreeEXRHeader(&exrHeader);
        FreeEXRErrorMessage(err);
        return false;
    }

    FLY_ASSERT(exrImage.images);

    u16* data =
        static_cast<u16*>(Fly::Alloc(sizeof(u16) * exrImage.width *
                                     exrImage.height * exrImage.num_channels));
    image.mem = reinterpret_cast<u8*>(data);
    image.data = reinterpret_cast<u8*>(data);
    image.channelCount = exrImage.num_channels;
    image.width = exrImage.width;
    image.height = exrImage.height;
    image.mipCount = 1;
    image.layerCount = 1;
    image.storageType = ImageStorageType::Half;

    // Change channel layout
    image.mem = reinterpret_cast<u8*>(data);
    for (u32 j = 0; j < image.height; j++)
    {
        for (u32 i = 0; i < image.width; i++)
        {
            for (u32 k = 0; k < image.channelCount; k++)
            {
                data[j * image.width * image.channelCount +
                     i * image.channelCount + k] =
                    reinterpret_cast<u16*>(
                        exrImage.images[channelMap[k]])[j * exrImage.width + i];
            }
        }
    }

    FreeEXRImage(&exrImage);
    FreeEXRHeader(&exrHeader);

    return true;
}

bool LoadImageFromFile(const char* path, Image& image, u8 desiredChannelCount)
{
    FLY_ASSERT(path);

    String8 pathStr = Fly::String8(path, strlen(path));

    String8 extension = String8::FindLast(pathStr, '.');

    if (extension.StartsWith(FLY_STRING8_LITERAL(".fbc")))
    {
        return LoadCompressedImageFromFile(pathStr, image);
    }
    else if (extension.StartsWith(FLY_STRING8_LITERAL(".exr")))
    {
        return LoadExrImageFromFile(pathStr, image);
    }

    int x = 0, y = 0, n = 0;
    if (stbi_is_hdr(path))
    {
        image.mem = reinterpret_cast<u8*>(
            stbi_loadf(path, &x, &y, &n, desiredChannelCount));
        image.storageType = ImageStorageType::Float;
    }
    else
    {
        image.mem = stbi_load(path, &x, &y, &n, desiredChannelCount);
        image.storageType = ImageStorageType::Byte;
    }
    image.data = image.mem;

    image.mipCount = 1;
    image.layerCount = 1;
    image.width = static_cast<u32>(x);
    image.height = static_cast<u32>(y);

    if (desiredChannelCount != 0)
    {
        image.channelCount = static_cast<u8>(desiredChannelCount);
    }
    else
    {
        image.channelCount = static_cast<u8>(n);
    }

    return image.data;
}

bool LoadImageFromFile(const Path& path, Image& image, u8 desiredChannelCount)
{
    return Fly::LoadImageFromFile(path.ToCStr(), image, desiredChannelCount);
}

void FreeImage(Image& image)
{
    FLY_ASSERT(image.mem);
    FLY_ASSERT(image.data);

    Fly::Free(image.mem);

    image.mem = nullptr;
    image.data = nullptr;
    image.channelCount = 0;
    image.width = 0;
    image.height = 0;
}

u32 GetImageStorageTypeSize(ImageStorageType type)
{
    switch (type)
    {
        case ImageStorageType::Byte:
        {
            return sizeof(u8);
        }
        case ImageStorageType::Half:
        {
            return sizeof(u16);
        }
        case ImageStorageType::Float:
        {
            return sizeof(f32);
        }
        case ImageStorageType::Block8:
        {
            return 8;
        }
        case ImageStorageType::Block16:
        {
            return 16;
        }
    }
    return 0;
}

} // namespace Fly
