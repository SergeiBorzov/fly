#ifndef HLS_CORE_THREAD_CONTEXT_H
#define HLS_CORE_THREAD_CONTEXT_H

#include "arena.h"

struct ThreadContext
{
    Arena arenas[2];
};

void InitThreadContext();
void ReleaseThreadContext();
ThreadContext& GetThreadContext();

Arena& GetScratchArena(Arena* conflict = nullptr);

#endif /* HLS_CORE_THREAD_CONTEXT */
