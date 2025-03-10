#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "filesystem.h"
#include "platform.h"

#ifdef HLS_PLATFORM_OS_WINDOWS
#include <windows.h>
#endif

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

char* ReadFileToString(Arena& arena, const char* filename, u64* size, u32 align)
{
    const char* mode = "rb";

    FILE* file = fopen(filename, mode);
    if (!file)
    {
        printf("shit %s!\n", filename);
        return nullptr;
    }

    // Move the file pointer to the end of the file to determine its size
    fseek(file, 0, SEEK_END);
    i64 fileSize = ftell(file);
    fseek(file, 0, SEEK_SET); // Move back to the beginning of the file

    // Allocate memory for the string (plus one for the null terminator)
    char* content = HLS_ALLOC_ALIGNED(arena, char, fileSize + 1, align);

    if (!content)
    {
        fclose(file);
        return nullptr;
    }

    // Read the file into the string
    fread(content, 1, fileSize, file);
    content[fileSize] = '\0'; // Null-terminate the string

    fclose(file);
    if (size)
    {
        *size = fileSize;
    }
    return content;
}
