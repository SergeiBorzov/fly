#include "core/assert.h"
#include "core/filesystem.h"

namespace Hls
{

String8 LoadSpvFromFile(Arena& arena, const char* path)
{
    HLS_ASSERT(path);
    return ReadFileToString(arena, path, sizeof(u32));
}

String8 LoadSpvFromFile(Arena& arena, const Path& path)
{
    return LoadSpvFromFile(arena, path.ToCStr());
}

} // namespace Hls
