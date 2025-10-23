#include "core/assert.h"
#include "core/filesystem.h"

namespace Fly
{

String8 LoadSpvFromFile(Arena& arena, String8 path)
{
    return ReadFileToString(arena, path, sizeof(u32));
}

} // namespace Fly
