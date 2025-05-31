#include "import_image.h"

#include "core/assert.h"
#include "core/filesystem.h"
#include "core/memory.h"

#define STBI_ASSERT(x) FLY_ASSERT(x)
#define STBI_MALLOC(size) (Fly::Alloc(size))
#define STBI_REALLOC(p, newSize) (Fly::Realloc(p, newSize))
#define STBI_FREE(p) (Fly::Free(p))
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Fly
{

bool LoadImageFromFile(const char* path, Image& image)
{
    FLY_ASSERT(path);

    int x = 0, y = 0, n = 0;
    int desiredChannelCount = 4;
    image.data = stbi_load(path, &x, &y, &n, desiredChannelCount);
    image.width = static_cast<u32>(x);
    image.height = static_cast<u32>(y);
    image.channelCount = static_cast<u32>(desiredChannelCount);

    return image.data;
}

bool LoadImageFromFile(const Path& path, Image& image)
{
    return Fly::LoadImageFromFile(path.ToCStr(), image);
}

void FreeImage(Image& image)
{
    FLY_ASSERT(image.data);
    stbi_image_free(image.data);
}

} // namespace Fly
