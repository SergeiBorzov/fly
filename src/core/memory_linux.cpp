#include "memory.h"
#include <sys/mman.h>

namespace Hls
{

void* PlatformAlloc(u64 reserveSize, u64 commitSize)
{
    // reserve memory
    void* reserved = mmap(nullptr, reserveSize, PROT_NONE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (reserved == MAP_FAILED)
    {
        return nullptr;
    }

    void* commited = mmap(reserved, commitSize, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (commited == MAP_FAILED)
    {
        munmap(reserved, reserveSize);
        return nullptr;
    }

    return reserved;
}

void* PlatformCommitMemory(void* baseAddress, u64 commitSize)
{
    void* commited = mmap(baseAddress, commitSize, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (commited == MAP_FAILED)
    {
        return nullptr;
    }

    return commited;
}

bool PlatformDecommitMemory(void* baseAddress, u64 length)
{
    return madvise(baseAddress, length, MADV_DONTNEED) == 0;
}

void PlatformFree(void* reserved, u64 reserveSize)
{
    munmap(reserved, reserveSize);
}

} // namespace Hls
