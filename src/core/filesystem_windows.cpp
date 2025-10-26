#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filesystem.h"
#include "memory.h"
#include "platform.h"
#include "thread_context.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#include <windows.h>

#define FLY_PATH_SEPARATOR '\\'

namespace Fly
{

bool CreateDirectories(String8 path)
{
    char tmp[1024];
    strncpy(tmp, path.Data(), sizeof(tmp));
    tmp[sizeof(tmp) - 1] = '\0';

    for (char* p = tmp + 1; *p; p++)
    {
        if (*p == '/' || *p == '\\')
        {
            *p = '\0';
            if (!CreateDirectoryA(tmp, nullptr))
            {
                DWORD err = GetLastError();
                if (err != ERROR_ALREADY_EXISTS)
                {
                    return false;
                }
            }
            *p = '\\';
        }
    }

    return true;
}

} // namespace Fly
