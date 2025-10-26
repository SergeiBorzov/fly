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

} // namespace Fly
