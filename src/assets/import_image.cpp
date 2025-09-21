#include <string.h>

#include "image.h"
#include "import_image.h"

#include "core/assert.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/memory.h"

#define STBI_ASSERT(x) FLY_ASSERT(x)
#define STBI_MALLOC(size) (Fly::Alloc(size))
#define STBI_REALLOC(p, newSize) (Fly::Realloc(p, newSize))
#define STBI_FREE(p) (Fly::Free(p))
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Fly
{

bool LoadImageFromFile(const char* path, Image& image, u8 desiredChannelCount)
{
    FLY_ASSERT(path);

    const char* lastDot = strrchr(path, '.');
    if (!lastDot)
    {
        return false;
    }

    if (strncmp(lastDot, ".bc1", strlen(".bc1")) == 0)
    {
        u64 size = 0;
        u8* bytes = ReadFileToByteArray(path, size);
        if (!bytes)
        {
            return false;
        }

        image.mem = bytes;
        image.width = *(reinterpret_cast<u32*>(bytes));
        image.height = *(reinterpret_cast<u32*>(bytes) + 1);
        image.channelCount = 3;
        image.data = reinterpret_cast<u8*>(reinterpret_cast<u32*>(bytes) + 2);
        return true;
    }
    else if (strncmp(lastDot, ".bc3", strlen(".bc3")) == 0)
    {
        u64 size = 0;
        u8* bytes = ReadFileToByteArray(path, size);
        if (!bytes)
        {
            return false;
        }

        image.mem = bytes;
        image.width = *(reinterpret_cast<u32*>(bytes));
        FLY_LOG("Width read %u", image.width);
        image.height = *(reinterpret_cast<u32*>(bytes) + 1);
        FLY_LOG("Height read %u", image.height);
        image.channelCount = 4;
        image.data = reinterpret_cast<u8*>(reinterpret_cast<u32*>(bytes) + 2);
        return true;
    }
    else if (strncmp(lastDot, ".bc4", strlen(".bc4")) == 0)
    {
        u64 size = 0;
        u8* bytes = ReadFileToByteArray(path, size);
        if (!bytes)
        {
            return false;
        }

        image.mem = bytes;
        image.width = *(reinterpret_cast<u32*>(bytes));
        image.height = *(reinterpret_cast<u32*>(bytes) + 1);
        image.channelCount = 1;
        image.data = reinterpret_cast<u8*>(reinterpret_cast<u32*>(bytes) + 2);
        return true;
    }
    else if (strncmp(lastDot, ".bc5", strlen(".bc5")) == 0)
    {
        u64 size = 0;
        u8* bytes = ReadFileToByteArray(path, size);
        if (!bytes)
        {
            return false;
        }

        image.width = *(reinterpret_cast<u32*>(bytes));
        image.height = *(reinterpret_cast<u32*>(bytes) + 1);
        image.channelCount = 1;
        image.data = reinterpret_cast<u8*>(reinterpret_cast<u32*>(bytes) + 2);
        return true;
    }

    int x = 0, y = 0, n = 0;
    image.mem = stbi_load(path, &x, &y, &n, desiredChannelCount);
    image.data = image.mem;
    image.width = static_cast<u32>(x);
    image.height = static_cast<u32>(y);
    image.channelCount = static_cast<u32>(desiredChannelCount);

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

} // namespace Fly
