#ifndef FLY_CORE_THREAD_CONTEXT_H
#define FLY_CORE_THREAD_CONTEXT_H

#include "arena.h"

namespace Fly
{

struct ThreadContext
{
    Arena arenas[2];
};

void InitArenas();
void InitThreadContext();
void ReleaseThreadContext();
ThreadContext& GetThreadContext();

Arena& GetScratchArena(Arena* conflict = nullptr);

} // namespace Fly

#endif /* FLY_CORE_THREAD_CONTEXT */
