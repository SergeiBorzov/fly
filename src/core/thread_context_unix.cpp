#include "assert.h"
#include "platform.h"
#include "thread_context.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(FLY_PLATFORM_OS_MAC_OSX)
#include <mach-o/dyld.h>
#endif

namespace Fly
{

static thread_local ThreadContext stThreadContext;

static bool SetEnv(const char* name, const char* value)
{
    return setenv(name, value, true) == 0;
}

#ifdef FLY_PLATFORM_OS_LINUX
static const char* GetBinaryDirectoryPath(Arena& arena)
{
    char buffer[PATH_MAX] = {0};
    ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);

    if (length == -1)
    {
        return nullptr;
    }

    char* lastSlash = strrchr(buffer, '/');
    if (!lastSlash)
    {
        return nullptr;
    }

    i64 actualLength = lastSlash - buffer + 1;

    char* exeDirPath = FLY_PUSH_ARENA(arena, char, actualLength + 1);
    memcpy(exeDirPath, buffer, actualLength);
    exeDirPath[actualLength] = '\0';

    return exeDirPath;
}
#elif defined(FLY_PLATFORM_OS_MAC_OSX)
static const char* GetBinaryDirectoryPath(Arena& arena)
{
    char pathBuffer[PATH_MAX] = {0};
    u32 size = sizeof(pathBuffer);

    if (_NSGetExecutablePath(pathBuffer, &size) != 0)
    {
        return nullptr;
    }

    char* lastSlash = strrchr(pathBuffer, '/');
    if (!lastSlash)
    {
        return nullptr;
    }

    size_t actualLength = lastSlash - pathBuffer + 1;

    char* exeDirPath = FLY_PUSH_ARENA(arena, char, actualLength + 1);
    memcpy(exeDirPath, pathBuffer, actualLength);
    exeDirPath[actualLength] = '\0';

    return exeDirPath;
}
#endif

void InitThreadContext()
{
    for (i32 i = 0; i < 2; i++)
    {
        stThreadContext.arenas[i] =
            ArenaCreate(FLY_SIZE_GB(2), FLY_ARENA_MIN_CAPACITY);
    }

    Arena& scratch = stThreadContext.arenas[0];
    const char* binaryDirectoryPath = GetBinaryDirectoryPath(scratch);
    FLY_ASSERT(binaryDirectoryPath);
    bool res = SetEnv("VK_LAYER_PATH", binaryDirectoryPath);
    (void)res;
    FLY_ASSERT(res);
    res = chdir(binaryDirectoryPath) == 0;
    FLY_ASSERT(res);
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

    FLY_ASSERT(index != -1);
    return stThreadContext.arenas[index];
}

} // namespace Fly
