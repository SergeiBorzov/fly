#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "filesystem.h"
#include "platform.h"

#ifdef HLS_PLATFORM_OS_WINDOWS
#include <windows.h>
#endif

HlsResult SetEnv(const char* name, const char* value)
{
#ifdef HLS_PLATFORM_OS_WINDOWS
    return _putenv_s(name, value);
#elif defined HLS_PLATFORM_POSIX
    return setenv(name, value, true);
#else
    return -1;
#endif
}

const char* GetBinaryDirectoryPath(Arena& arena)
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
