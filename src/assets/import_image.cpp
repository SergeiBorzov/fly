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

struct MipRow
{
    u32 width;
    u32 height;
    u64 offset;
};

namespace Fly
{

bool LoadCompressedImageFromFile(String8 path, Image& image)
{
    FLY_ASSERT(path);

    u8 channelCount = 0;

    if (path.EndsWith(FLY_STRING8_LITERAL(".fbc1")))
    {
        channelCount = 3;
    }
    else if (path.EndsWith(FLY_STRING8_LITERAL(".fbc3")))
    {
        channelCount = 4;
    }
    else if (path.EndsWith(FLY_STRING8_LITERAL(".fbc4")))
    {
        channelCount = 1;
    }
    else if (path.EndsWith(FLY_STRING8_LITERAL(".fbc5")))
    {
        channelCount = 2;
    }

    u64 size = 0;
    u8* bytes = ReadFileToByteArray(path.Data(), size);
    image.channelCount = channelCount;
    image.mem = bytes;
    image.mipLevelCount = *(reinterpret_cast<u32*>(bytes));
    MipRow* mipRow = reinterpret_cast<MipRow*>(bytes + sizeof(u32));
    image.width = mipRow->width;
    image.height = mipRow->height;
    image.data = bytes + mipRow->offset;

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
    image.mipLevelCount = 1;
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
