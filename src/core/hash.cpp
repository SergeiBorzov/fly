#include "assert.h"
#include <xxhash.h>

#define HASH_SEED 2025

namespace Fly
{

u64 Hash64(const void* data, u64 size)
{
    FLY_ASSERT(data);
    FLY_ASSERT(size > 0);
    return XXH64(data, size, HASH_SEED);
}

} // namespace Fly
