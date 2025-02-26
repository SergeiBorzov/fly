#ifndef HLS_MEMORY_ARENA_H
#define HLS_MEMORY_ARENA_H

#include "types.h"

#define HLS_ALLOC(arena, T, count)                                             \
    static_cast<T*>(ArenaPushAligned(arena, sizeof(T) * count, alignof(T)))
#define HLS_ALLOC_ALIGNED(arena, T, count, align)                              \
    static_cast<T*>(ArenaPushAligned(arena, sizeof(T) * count, align))

#define HLS_SIZE_KB(i) (1024ull * i)
#define HLS_SIZE_MB(i) (1024ull * 1024ull * i)
#define HLS_SIZE_GB(i) (1024ull * 1024ull * 1024ull * i)

struct ArenaMarker
{
    u64 value;
};

struct ArenaAllocHeader
{
    u64 prevAllocSize = 0;
};

struct Arena
{
    u8* ptr;
    u64 lastAllocSize;
    u64 size;
    u64 capacity;
    u64 reservedCapacity;
};

Arena ArenaCreate(u64 reservedSize, u64 commitedSize);
Arena ArenaCreateInline(u64 commitedSize, void* ptr);
void ArenaDestroy(Arena* arena);
void* ArenaPushAligned(Arena* arena, u64 size, u32 align);
ArenaMarker ArenaGetMarker(const Arena* arena);
void ArenaPop(Arena* arena, void* ptr);
void ArenaPopToMarker(Arena* arena, ArenaMarker marker);
void ArenaReset(Arena* arena);
void* ArenaUnwrapPtr(void* ptr);

#endif /* HLS_MEMORY_ARENA_H */
