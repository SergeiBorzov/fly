#include "assert.h"
#include "thread_context.h"

#include <windows.h>

static thread_local ThreadContext stThreadContext;

bool SetEnv(const char* name, const char* value)
{
    return _putenv_s(name, value) == 0;
}

static const char* GetBinaryDirectoryPath(Arena& arena)
{
    char buffer[MAX_PATH] = {0};
    DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);

    if (length == 0)
    {
        return nullptr;
    }

    char* lastSlash = strrchr(buffer, '\\'); // Find the last backslash
    i64 actualLength = lastSlash - buffer + 1;

    char* exeDirPath = HLS_ALLOC(arena, char, actualLength + 1);
    memcpy(exeDirPath, buffer, actualLength);
    exeDirPath[actualLength] = '\0';

    return exeDirPath;
}

void InitThreadContext()
{
    for (i32 i = 0; i < 2; i++)
    {
        stThreadContext.arenas[i] = ArenaCreate(HLS_SIZE_GB(2), HLS_SIZE_MB(1));
    }

    Arena& scratch = stThreadContext.arenas[0];
    const char* binaryDirectoryPath = GetBinaryDirectoryPath(scratch);
    HLS_ASSERT(binaryDirectoryPath);
    bool res = SetEnv("VK_LAYER_PATH", binaryDirectoryPath);
    HLS_ASSERT(res);
    res = SetCurrentDirectory(binaryDirectoryPath);
    HLS_ASSERT(res);
}

void ReleaseThreadContext()
{
    for (i32 i = 0; i < 2; i++)
    {
        ArenaDestroy(stThreadContext.arenas[i]);
    }
}

ThreadContext& GetThreadContext() { return stThreadContext; }

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
