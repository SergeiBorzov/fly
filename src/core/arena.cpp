#include "arena.h"
#include "assert.h"
#include "memory.h"

// shifts given pointer upwards if necessary to
// ensure it is aligned to given number of bytes

static void* AlignPtr(void* ptr, u32 align)
{
    const uintptr_t addr = (uintptr_t)ptr;
    const u64 mask = align - 1;
    FLY_ASSERT((align & mask) == 0); // should be power of 2
    return (void*)((addr + mask) & ~mask);
}

namespace Fly
{

void* ArenaUnwrapPtr(void* alignedPtr)
{
    u8* byteAlignedPtr = static_cast<u8*>(alignedPtr);
    ptrdiff_t shift = byteAlignedPtr[-1];
    if (shift == 0)
    {
        shift = 256;
    }
    return byteAlignedPtr - shift - sizeof(ArenaAllocHeader);
}

Arena ArenaCreate(u64 reservedSize, u64 commitedSize)
{
    Arena arena;

    // TODO: Add checks for size value
    // on some platforms allocation might fail
    // if size is not a multiple of a page size
    arena.ptr =
        static_cast<u8*>(Fly::PlatformAlloc(reservedSize, commitedSize));
    FLY_ASSERT(arena.ptr);
    FLY_ASSERT(commitedSize >= FLY_ARENA_MIN_CAPACITY);

    arena.reservedCapacity = reservedSize;
    arena.capacity = commitedSize;
    arena.size = 0;
    arena.lastAllocSize = 0;

    return arena;
}

void ArenaDestroy(Arena& arena)
{
    Fly::PlatformFree(arena.ptr, arena.reservedCapacity);
    arena.ptr = nullptr;
    arena.lastAllocSize = 0;
    arena.size = 0;
    arena.capacity = 0;
    arena.reservedCapacity = 0;
}

ArenaMarker ArenaGetMarker(const Arena& arena) { return {arena.size}; }

void ArenaPopToMarker(Arena& arena, ArenaMarker marker)
{
    arena.size = marker.value;

    if (arena.capacity > FLY_ARENA_MIN_CAPACITY &&
        arena.size < arena.capacity / 4)
    {
        u64 newCapacity = arena.capacity / 2;
        Fly::PlatformDecommitMemory(arena.ptr + newCapacity,
                                    arena.capacity - newCapacity);
        arena.capacity = newCapacity;
    }
}

void* ArenaPushAligned(Arena& arena, u64 size, u32 align)
{
    FLY_ASSERT(size > 0);

    u64 allocSize = size + align + sizeof(ArenaAllocHeader);

    u64 total = arena.size + allocSize;
    while (total > arena.capacity)
    {
        u64 newCapacity = 2 * arena.capacity;
        if (newCapacity <= arena.reservedCapacity)
        {
            void* res = Fly::PlatformCommitMemory(arena.ptr + arena.capacity,
                                                  arena.capacity);
            (void)res;
            FLY_ASSERT(res);
            arena.capacity = newCapacity;
        }
        else
        {
            // Out of reserved space of addresses
            FLY_ASSERT(false);
        }
    }

    u8* unalignedPtr = arena.ptr + arena.size;
    ArenaAllocHeader* header =
        reinterpret_cast<ArenaAllocHeader*>(unalignedPtr);
    header->prevAllocSize = arena.lastAllocSize;

    u8* alignedPtr = static_cast<u8*>(
        AlignPtr(unalignedPtr + sizeof(ArenaAllocHeader), align));
    if (alignedPtr == unalignedPtr + sizeof(ArenaAllocHeader))
    {
        alignedPtr += align;
    }

    ptrdiff_t shift = alignedPtr - (unalignedPtr + sizeof(ArenaAllocHeader));
    FLY_ASSERT(shift > 0 && shift <= 256, "Supported alignment is up to 256");

    alignedPtr[-1] = static_cast<u8>(shift & 0xFF);

    arena.lastAllocSize = size + shift + sizeof(ArenaAllocHeader);
    arena.size += arena.lastAllocSize;
    return alignedPtr;
}

void ArenaReset(Arena& arena)
{
    arena.size = 0;
    Fly::PlatformDecommitMemory(arena.ptr + FLY_ARENA_MIN_CAPACITY,
                                arena.capacity - FLY_ARENA_MIN_CAPACITY);
    arena.capacity = FLY_ARENA_MIN_CAPACITY;
}

} // namespace Fly
