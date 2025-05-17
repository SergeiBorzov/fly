#include "thread_context.h"
#include "assert.h"

#ifdef HLS_PLATFORM_OS_WINDOWS
#include <windows.h>
#else
#error "Not implemented"
#endif

static thread_local ThreadContext stThreadContext;

bool SetEnv(const char* name, const char* value)
{
#ifdef HLS_PLATFORM_OS_WINDOWS
    return _putenv_s(name, value) == 0;
#elif defined HLS_PLATFORM_POSIX
    return setenv(name, value, true) == 0;
#else
    return false;
#endif
}

static const char* GetBinaryDirectoryPath(Arena& arena)
{
#ifdef HLS_PLATFORM_OS_WINDOWS
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
#else
    return nullptr;
#endif
}

void InitThreadContext()
{
    for (i32 i = 0; i < 2; i++)
    {
        stThreadContext.arenas[i] =
            ArenaCreate(HLS_SIZE_GB(2), HLS_SIZE_MB(1));
    }

    Arena& scratch = stThreadContext.arenas[0];
    const char* binaryDirectoryPath = GetBinaryDirectoryPath(scratch);
    HLS_ASSERT(binaryDirectoryPath);
    bool res = SetEnv("VK_LAYER_PATH", binaryDirectoryPath);
    HLS_ASSERT(res);
#ifdef HLS_PLATFORM_OS_WINDOWS
    res = SetCurrentDirectory(binaryDirectoryPath);
    HLS_ASSERT(res);
#else
    HLS_ASSERT(false);
#endif
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
