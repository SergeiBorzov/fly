#include "core/log.h"
#include "core/memory.h"

#include "allocation_callbacks.h"

namespace Hls
{

void* VKAPI_PTR VulkanAlloc(void* pUserData, size_t size, size_t alignment,
                            VkSystemAllocationScope scope)
{
    return Hls::AllocAligned(size, static_cast<u32>(alignment));
}

void* VKAPI_PTR VulkanRealloc(void* pUserData, void* pOriginal, size_t size,
                              size_t alignment,
                              VkSystemAllocationScope allocationScope)
{
    return Hls::ReallocAligned(pOriginal, size, static_cast<u32>(alignment));
}

void VKAPI_PTR VulkanFree(void* pUserData, void* pMemory)
{
    Hls::Free(pMemory);
}

const VkAllocationCallbacks* GetVulkanAllocationCallbacks()
{
    static VkAllocationCallbacks allocationCallbacks{};
    allocationCallbacks.pUserData = nullptr;
    allocationCallbacks.pfnAllocation = VulkanAlloc;
    allocationCallbacks.pfnReallocation = VulkanRealloc;
    allocationCallbacks.pfnFree = VulkanFree;
    allocationCallbacks.pfnInternalAllocation = nullptr;
    allocationCallbacks.pfnInternalFree = nullptr;

    return &allocationCallbacks;
}

} // namespace Hls
