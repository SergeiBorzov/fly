#include "assert.h"
#include "thread_context.h"

static thread_local ThreadContext stThreadContext;

void InitThreadContext()
{
    for (i32 i = 0; i < 2; i++)
    {
        stThreadContext.arenas[i] =
            ArenaCreate(HLS_SIZE_MB(256), HLS_SIZE_MB(8));
    }
}

void ReleaseThreadContext()
{
    for (i32 i = 0; i < 2; i++)
    {
        ArenaDestroy(stThreadContext.arenas[i]);
    }
}

ThreadContext& GetThreadContext()
{
    return stThreadContext;
}

Arena& GetScratchArena(Arena* conflict)
{
    if (!conflict)
    {
        return stThreadContext.arenas[0];
    }

    i32 index = -1;
    for (i32 i = 0; i < 2; i++)
    {
        if (&stThreadContext.arenas[i] != conflict)
        {
            index = i;
            break;
        }
    }

    HLS_ASSERT(index != -1);
    return stThreadContext.arenas[index];
}
