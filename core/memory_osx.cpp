#include "memory.h"
#include <mach/mach.h>

void* PlatformAlloc(u64 reserveSize, u64 commitSize)
{
    // reserve memory
    vm_address_t address = 0;
    kern_return_t kr = vm_allocate(mach_task_self(), &address, reserveSize,
                                   VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS)
    {
        // mach_error_string(kr) to get error
        return nullptr;
    }

    // Commit memory
    kr = vm_protect(mach_task_self(), address, commitSize, FALSE,
                    VM_PROT_READ | VM_PROT_WRITE);
    if (kr != KERN_SUCCESS)
    {
        vm_deallocate(mach_task_self(), address, reserveSize);
        return nullptr;
    }

    return (void*)address;
}

void PlatformFree(void* ptr, u64 size)
{
    vm_deallocate(mach_task_self(), (vm_address_t)ptr, size);
}
