#include "memory.h"
#include <mimalloc.h>

namespace Fly
{
void* Alloc(u64 size) { return mi_malloc(size); }

void* Realloc(void* p, u64 newSize) { return mi_realloc(p, newSize); }

void* AllocAligned(u64 size, u32 alignment)
{
    return mi_malloc_aligned(size, static_cast<u64>(alignment));
}

void* ReallocAligned(void* p, u64 newSize, u32 alignment)
{
    return mi_realloc_aligned(p, newSize, static_cast<u64>(alignment));
}

void Free(void* p) { mi_free(p); }
} // namespace Fly
