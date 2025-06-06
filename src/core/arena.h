#ifndef FLY_MEMORY_ARENA_H
#define FLY_MEMORY_ARENA_H

#include "types.h"

#define FLY_PUSH_ARENA(arena, T, count)                                        \
    static_cast<T*>(ArenaPushAligned(arena, sizeof(T) * count, alignof(T)))
#define FLY_PUSH_ARENA_ALIGNED(arena, T, count, align)                         \
    static_cast<T*>(ArenaPushAligned(arena, sizeof(T) * count, align))

#define FLY_SIZE_KB(i) (1024ull * i)
#define FLY_SIZE_MB(i) (1024ull * 1024ull * i)
#define FLY_SIZE_GB(i) (1024ull * 1024ull * 1024ull * i)

#define FLY_ARENA_MIN_CAPACITY FLY_SIZE_MB(1)

namespace Fly
{

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
    u8* ptr = nullptr;
    u64 lastAllocSize = 0;
    u64 size = 0;
    u64 capacity = 0;
    u64 reservedCapacity = 0;
};

Arena ArenaCreate(u64 reservedSize, u64 commitedSize);
void ArenaDestroy(Arena& arena);
void* ArenaPushAligned(Arena& arena, u64 size, u32 align);
ArenaMarker ArenaGetMarker(const Arena& arena);
void ArenaPopToMarker(Arena& arena, ArenaMarker marker);
void ArenaReset(Arena& arena);
void* ArenaUnwrapPtr(void* ptr);

} // namespace FLy

#endif /* FLY_MEMORY_ARENA_H */
