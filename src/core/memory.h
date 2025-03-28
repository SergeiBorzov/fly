#ifndef HLS_MEMORY_H
#define HLS_MEMORY_H

#include "types.h"
#include <string.h> // for memset

inline void* MemZero(void* p, u64 size) { return memset(p, 0, size); }

void* PlatformAlloc(u64 reserveSize, u64 commitSize);
void* PlatformCommitMemory(void* baseAddress, u64 commitSize);
bool PlatformDecommitMemory(void* baseAddress);
void PlatformFree(void* ptr, u64 size);

#endif /* HLS_MEMORY_H */
