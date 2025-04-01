#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "filesystem.h"
#include "platform.h"

#ifdef HLS_PLATFORM_OS_WINDOWS
#include <windows.h>
#endif

namespace Hls
{

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

    // Allocate memory for the string (plus one for the null terminator)
    char* content = HLS_ALLOC_ALIGNED(arena, char, fileSize + 1, align);

    if (!content)
    {
        fclose(file);
        return String8();
    }

    // Read the file into the string
    fread(content, 1, fileSize, file);
    content[fileSize] = '\0'; // Null-terminate the string

    fclose(file);

    return String8(content, fileSize);
}

} // namespace Hls
