#include "memory.h"
#include <mach/mach.h>
#include <sys/mman.h>


#ifndef VM_MADVISE_FREE
#define VM_MADVISE_FREE 5
#endif

namespace Fly
{

void* PlatformAlloc(u64 reserveSize, u64 commitSize)
{
    vm_address_t address = 0;
    kern_return_t kr = vm_allocate(mach_task_self(), &address, reserveSize,
                                   VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS)
    {
        return nullptr;
    }

    kr = vm_protect(mach_task_self(), address, commitSize, FALSE,
                    VM_PROT_READ | VM_PROT_WRITE);
    if (kr != KERN_SUCCESS)
    {
        vm_deallocate(mach_task_self(), address, reserveSize);
        return nullptr;
    }

    return (void*)address;
}

void* PlatformCommitMemory(void* baseAddress, u64 commitSize)
{
    kern_return_t kr =
	vm_protect(mach_task_self(), (vm_address_t)baseAddress,
		   commitSize, FALSE, VM_PROT_READ | VM_PROT_WRITE);
    if (kr != KERN_SUCCESS)
    {
        return nullptr;
    }

    return (void*)baseAddress;
}

bool PlatformDecommitMemory(void* baseAddress, u64 length)
{
    return madvise(baseAddress, length, MADV_DONTNEED) == 0;
}

void PlatformFree(void* ptr, u64 size)
{
    vm_deallocate(mach_task_self(), (vm_address_t)ptr, size);
}

} // namespace Fly
