#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

bool CreateDirectories(String8 path)
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

String8 CurrentWorkingDirectory(Arena& arena)
{
    char* buffer = FLY_PUSH_ARENA(arena, char, PATH_MAX);
    if (getcwd(buffer, PATH_MAX) != nullptr)
    {
        return String8(buffer, strlen(buffer));
    }

    return String8();
}

String8 ParentDirectory(String8 path)
{
    String8 last = String8::FindLast(path, FLY_PATH_SEPARATOR);
    if (!last)
    {
        return path;
    }

    return String8(path.Data(), path.Size() - last.Size() + 1);
}

} // namespace Fly
