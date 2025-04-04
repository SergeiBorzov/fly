#ifndef HLS_CORE_MEMORY_H
#define HLS_CORE_MEMORY_H

#include "types.h"
#include <string.h> // for memset

namespace Hls
{

inline void* MemZero(void* p, u64 size) { return memset(p, 0, size); }

void* PlatformAlloc(u64 reserveSize, u64 commitSize);
void* PlatformCommitMemory(void* baseAddress, u64 commitSize);
bool PlatformDecommitMemory(void* baseAddress);
void PlatformFree(void* ptr, u64 size);

void* Alloc(u64 size);
void* AllocAligned(u64 size, u32 alignment);
void* Realloc(void* p, u64 newSize);
void* ReallocAligned(void* p, u64 newSize, u32 alignment);
void Free(void* p);

} // namespace Hls

#endif /* HLS_CORE_MEMORY_H */
