#include <stdarg.h>
#include <stdio.h>

#include "assert.h"

void AssertImpl(const char* cond, const char* file, int line, const char* msg,
                ...)
{
    printf("Assertion failed %s:%d %s\n", file, line, cond);

    if (*msg)
    {
        va_list arg;
        va_start(arg, msg);
        char* data = va_arg(arg, char*);
        vprintf(data, arg);
    }

    if (*msg)
    {
        printf("\n");
    }

    FLY_ASSERT_BREAK();
}
