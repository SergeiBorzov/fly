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
    u8* data = ReadFileToByteArray(path, size);
    const ImageHeader* header = reinterpret_cast<const ImageHeader*>(data);

    image.width = header->width;
    image.height = header->height;
    image.channelCount = header->channelCount;
    image.layerCount = header->layerCount;
    image.mipCount = header->mipCount;
    image.storageType = header->storageType;

    u64 imageSize = GetImageSize(image);
    u8* imageData = static_cast<u8*>(Alloc(imageSize));
    memcpy(imageData, data + sizeof(ImageHeader), imageSize);

    Free(data);
    image.data = imageData;

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
    image.data = reinterpret_cast<u8*>(data);
    image.channelCount = exrImage.num_channels;
    image.width = exrImage.width;
    image.height = exrImage.height;
    image.mipCount = 1;
    image.layerCount = 1;
    image.storageType = ImageStorageType::Half;

    // Change channel layout
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

bool LoadImageFromFile(String8 path, Image& image, u8 desiredChannelCount)
{
    FLY_ASSERT(path);

    String8 extension = String8::FindLast(path, '.');

    if (extension.StartsWith(FLY_STRING8_LITERAL(".fbc")))
    {
        return LoadCompressedImageFromFile(path, image);
    }
    else if (extension.StartsWith(FLY_STRING8_LITERAL(".exr")))
    {
        return LoadExrImageFromFile(path, image);
    }

    int x = 0, y = 0, n = 0;
    if (stbi_is_hdr(path.Data()))
    {
        image.data = reinterpret_cast<u8*>(
            stbi_loadf(path.Data(), &x, &y, &n, desiredChannelCount));
        image.storageType = ImageStorageType::Float;
    }
    else
    {
        image.data = stbi_load(path.Data(), &x, &y, &n, desiredChannelCount);
        image.storageType = ImageStorageType::Byte;
    }

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

static u64 GetImageLayerSize(u32 width, u32 height, u32 channelCount,
                             ImageStorageType storageType)
{
    switch (storageType)
    {
        case ImageStorageType::BC1:
        case ImageStorageType::BC4:
        {
            return ((width + 3) / 4) * ((height + 3) / 4) * 8;
        }
        case ImageStorageType::BC3:
        case ImageStorageType::BC5:
        {
            return ((width + 3) / 4) * ((height + 3) / 4) * 16;
        }
        case ImageStorageType::Byte:
        {
            return width * height * channelCount * sizeof(u8);
        }
        case ImageStorageType::Half:
        {
            return width * height * channelCount * sizeof(f16);
        }
        case ImageStorageType::Float:
        {
            return width * height * channelCount * sizeof(f32);
        }
    }

    return 0;
}

u64 GetImageSize(const Image& image)
{
    u64 imageSize = 0;
    for (u32 i = 0; i < image.mipCount; i++)
    {
        imageSize += GetImageLayerSize(image.width, image.height,
                                       image.channelCount, image.storageType) *
                     image.layerCount;
    }
    return imageSize;
}

u64 GetImageSize(u32 width, u32 height, u8 channelCount, u8 layerCount,
                 u8 mipCount, ImageStorageType storageType)
{
    u64 imageSize = 0;
    for (u32 i = 0; i < mipCount; i++)
    {
        imageSize +=
            GetImageLayerSize(width, height, channelCount, storageType) *
            layerCount;
    }
    return imageSize;
}

void DestroyImage(Image& image)
{
    if (image.data)
    {
        Free(image.data);
    }
    image = {};
}

} // namespace Fly
