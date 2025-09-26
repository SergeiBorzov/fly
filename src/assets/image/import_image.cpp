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
#include "stb_image.h"

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

bool LoadImageFromFile(const char* path, Image& image, u8 desiredChannelCount)
{
    FLY_ASSERT(path);

    String8 pathStr = Fly::String8(path, strlen(path));

    if (String8::FindLast(pathStr, '.').StartsWith(FLY_STRING8_LITERAL(".fbc")))
    {
        return LoadCompressedImageFromFile(pathStr, image);
    }

    int x = 0, y = 0, n = 0;
    image.mem = stbi_load(path, &x, &y, &n, desiredChannelCount);
    image.data = image.mem;

    image.storageType = ImageStorageType::Byte;
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
