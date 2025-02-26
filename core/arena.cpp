#include "arena.h"
#include "assert.h"
#include "memory.h"

// shifts given pointer upwards if necessary to
// ensure it is aligned to given number of bytes

static void* AlignPtr(void* ptr, u32 align)
{
    const uintptr_t addr = (uintptr_t)ptr;
    const u64 mask = align - 1;
    HLS_ASSERT((align & mask) == 0); // should be power of 2
    return (void*)((addr + mask) & ~mask);
}

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
    arena.ptr = static_cast<u8*>(PlatformAlloc(reservedSize, commitedSize));
    HLS_ASSERT(arena.ptr);

    arena.reservedCapacity = reservedSize;
    arena.capacity = commitedSize;
    arena.size = 0;
    arena.lastAllocSize = 0;

    return arena;
}

Arena ArenaCreateInline(u64 commitedSize, void* ptr)
{
    Arena arena;
    arena.ptr = static_cast<u8*>(ptr);
    HLS_ASSERT(ptr);

    arena.capacity = commitedSize;
    arena.size = 0;
    arena.lastAllocSize = 0;
    return arena;
}

void ArenaDestroy(Arena* arena)
{
    PlatformFree(arena->ptr, arena->reservedCapacity);
}

void ArenaPop(Arena* arena, void* alignedPtr)
{
    HLS_ASSERT(alignedPtr);
    u8* rawPtr = static_cast<u8*>(ArenaUnwrapPtr(alignedPtr));
    HLS_ASSERT(arena->ptr + arena->size - arena->lastAllocSize == rawPtr,
                "Arena popped not the latest allocation: %p vs %p",
                arena->ptr + arena->size - arena->lastAllocSize, rawPtr);

    ArenaAllocHeader* header = reinterpret_cast<ArenaAllocHeader*>(rawPtr);

    arena->size = arena->size - arena->lastAllocSize;
    arena->lastAllocSize = header->prevAllocSize;
}

ArenaMarker ArenaGetMarker(const Arena* arena) { return {arena->size}; }

void ArenaPopToMarker(Arena* arena, ArenaMarker marker)
{
    arena->size = marker.value;
}

void* ArenaPushAligned(Arena* arena, u64 size, u32 align)
{
    HLS_ASSERT(size > 0);

    u64 allocSize = size + align + sizeof(ArenaAllocHeader);

    u64 total = arena->size + allocSize;
    if (total > arena->capacity)
    {
        // TODO: Handle arena out of memory situation
        HLS_ASSERT(false);
    }

    u8* unalignedPtr = arena->ptr + arena->size;
    ArenaAllocHeader* header =
        reinterpret_cast<ArenaAllocHeader*>(unalignedPtr);
    header->prevAllocSize = arena->lastAllocSize;

    u8* alignedPtr = static_cast<u8*>(
        AlignPtr(unalignedPtr + sizeof(ArenaAllocHeader), align));
    if (alignedPtr == unalignedPtr + sizeof(ArenaAllocHeader))
    {
        alignedPtr += align;
    }

    ptrdiff_t shift = alignedPtr - (unalignedPtr + sizeof(ArenaAllocHeader));
    HLS_ASSERT(shift > 0 && shift <= 256, "Supported alignment is up to 256");

    alignedPtr[-1] = static_cast<u8>(shift & 0xFF);

    arena->lastAllocSize = size + shift + sizeof(ArenaAllocHeader);
    arena->size += arena->lastAllocSize;
    return alignedPtr;
}

void ArenaReset(Arena* arena) { arena->size = 0; }
