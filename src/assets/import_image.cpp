#include "import_image.h"

#include "core/assert.h"
#include "core/filesystem.h"
#include "core/memory.h"

#define STBI_ASSERT(x) HLS_ASSERT(x)
#define STBI_MALLOC(size) (Hls::Alloc(size))
#define STBI_REALLOC(p, newSize) (Hls::Realloc(p, newSize))
#define STBI_FREE(p) (Hls::Free(p))
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Hls
{

bool LoadImageFromFile(const char* path, Image& image)
{
    HLS_ASSERT(path);

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
    return Hls::LoadImageFromFile(path.ToCStr(), image);
}

void FreeImage(Image& image)
{
    HLS_ASSERT(image.data);
    stbi_image_free(image.data);
}

} // namespace Hls
