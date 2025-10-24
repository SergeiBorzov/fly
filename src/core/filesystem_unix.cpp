#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filesystem.h"
#include "memory.h"
#include "platform.h"
#include "thread_context.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define FLY_PATH_SEPARATOR '/'

namespace Fly
{

String8 ReadFileToString(Arena& arena, String8 path, u32 align)
{
    const char* mode = "rb";

    FILE* file = fopen(path.Data(), mode);
    if (!file)
    {
        return String8();
    }

    // Move the file pointer to the end of the file to determine its size
    fseek(file, 0, SEEK_END);
    i64 fileSize = ftell(file);
    fseek(file, 0, SEEK_SET); // Move back to the beginning of the file

    // Allocate memory for the string
    char* content = FLY_PUSH_ARENA_ALIGNED(arena, char, fileSize + 1, align);
    content[fileSize] = '\0';

    if (!content)
    {
        fclose(file);
        return String8();
    }

    // Read the file into the string
    if (fread(content, 1, fileSize, file) != static_cast<size_t>(fileSize))
    {
        fclose(file);
        return String8();
    }

    fclose(file);

    return String8(content, fileSize);
}

u8* ReadFileToByteArray(String8 path, u64& size, u32 align)
{
    const char* mode = "rb";

    FILE* file = fopen(path.Data(), mode);
    if (!file)
    {
        return nullptr;
    }

    // Move the file pointer to the end of the file to determine its size
    fseek(file, 0, SEEK_END);
    i64 fileSize = ftell(file);
    fseek(file, 0, SEEK_SET); // Move back to the beginning of the file

    // Allocate memory for the string
    u8* content =
        static_cast<u8*>(Fly::AllocAligned(sizeof(u8) * (fileSize + 1), align));
    content[fileSize] = '\0';

    if (!content)
    {
        fclose(file);
        return nullptr;
    }

    // Read the file into the string
    if (fread(content, 1, fileSize, file) != static_cast<size_t>(fileSize))
    {
        fclose(file);
        Fly::Free(content);
        return nullptr;
    }

    fclose(file);

    size = fileSize;
    return content;
}

static bool CreateDirectories(String8 path)
{
    char tmp[PATH_MAX];
    strncpy(tmp, path.Data(), sizeof(tmp));
    tmp[sizeof(tmp) - 1] = '\0';

    for (char* p = tmp + 1; *p; p++)
    {
        if (*p == '/' || *p == '\\')
        {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0)
            {
                if (errno == EEXIST)
                {
                    continue;
                }
                else
                {
                    return false;
                }
            }
            *p = '/';
        }
    }

    return true;
}

bool WriteStringToFile(String8 str, String8 path, bool append)
{
    if (!CreateDirectories(path))
    {
        return false;
    }

    const char* mode = append ? "ab" : "wb";
    FILE* f = fopen(path.Data(), mode);
    if (!f)
    {
        return false;
    }

    if (!str)
    {
        fclose(f);
        return true;
    }

    if (fwrite(str.Data(), 1, str.Size(), f) != str.Size())
    {
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

} // namespace Fly
