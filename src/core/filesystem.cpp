#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "filesystem.h"
#include "platform.h"

#ifdef HLS_PLATFORM_OS_WINDOWS
#include <windows.h>
#endif

#ifdef HLS_PLATFORM_OS_WINDOWS
#define HLS_PATH_SEPARATOR '\\'
#define HLS_PATH_SEPARATOR_STRING "\\"
#elif defined(HLS_PLATFORM_OS_LINUX) || defined(HLS_PLATFORM_OS_MAC_OSX)
#define HLS_PATH_SEPARATOR '/'
#define HLS_PATH_SEPARATOR_STRING "/"
#else
#error "Not implemented"
#endif

namespace Hls
{

String8 GetParentDirectoryPath(String8 path)
{
    String8 lastSeparator = String8::FindLast(path, HLS_PATH_SEPARATOR);
    if (!lastSeparator)
    {
        return HLS_STRING8_LITERAL("." HLS_PATH_SEPARATOR_STRING);
    }

    return String8(path.Data(), path.Size() - lastSeparator.Size() + 1);
}

String8 AppendPaths(Arena& arena, String8* paths, u32 pathCount)
{
    u64 totalSize = 0;
    for (u32 i = 0; i < pathCount; i++)
    {
        totalSize += paths[i].Size();
    }

    char* totalData = HLS_ALLOC(arena, char, totalSize);

    u64 offset = 0;
    for (u32 i = 0; i < pathCount; i++)
    {
        memcpy(totalData + offset, paths[i].Data(), paths[i].Size());
        offset += paths[i].Size();
    }

    return String8(totalData, totalSize);
}

String8 ReadFileToString(Arena& arena, const char* filename, u32 align,
                         bool binaryMode)
{
    const char* mode = "rb";
    if (!binaryMode)
    {
        mode = "r";
    }

    FILE* file = fopen(filename, mode);
    if (!file)
    {
        return String8();
    }

    // Move the file pointer to the end of the file to determine its size
    fseek(file, 0, SEEK_END);
    i64 fileSize = ftell(file);
    fseek(file, 0, SEEK_SET); // Move back to the beginning of the file

    // Allocate memory for the string
    char* content = HLS_ALLOC_ALIGNED(arena, char, fileSize + 1, align);

    if (!content)
    {
        fclose(file);
        return String8();
    }

    // Read the file into the string
    fread(content, 1, fileSize, file);

    fclose(file);

    return String8(content, fileSize);
}

} // namespace Hls
