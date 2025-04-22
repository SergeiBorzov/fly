#include "assert.h"
#include <xxhash.h>

#define HASH_SEED 2025

namespace Hls
{

u64 Hash64(void* data, u64 size)
{
    HLS_ASSERT(data);
    HLS_ASSERT(size > 0);
    return XXH64(data, size, HASH_SEED);
}

} // namespace Hls
