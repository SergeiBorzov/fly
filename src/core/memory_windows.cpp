#include "memory.h"
#include <windows.h>

void* PlatformAlloc(u64 reserveSize, u64 commitSize)
{
    // reserve memory
    void* reserved =
        VirtualAlloc(nullptr, reserveSize, MEM_RESERVE, PAGE_NOACCESS);
    if (!reserved)
    {
        return nullptr;
    }

    // commit memory
    reserved = VirtualAlloc(reserved, commitSize, MEM_COMMIT, PAGE_READWRITE);
    if (!reserved)
    {
        VirtualFree(reserved, 0, MEM_RELEASE);
    }

    return reserved;
}

void PlatformFree(void* ptr, u64 size) { VirtualFree(ptr, 0, MEM_RELEASE); }
