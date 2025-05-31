#ifndef FLY_RHI_ALLOCATION_CALLBACKS_H
#define FLY_RHI_ALLOCATION_CALLBACKS_H

#include <volk.h>

namespace Fly
{
namespace RHI
{

void* VKAPI_PTR VulkanAlloc(void* pUserData, size_t size, size_t alignment,
                            VkSystemAllocationScope scope);
void* VKAPI_PTR VulkanRealloc(void* pUserData, void* pOriginal, size_t size,
                              size_t alignment,
                              VkSystemAllocationScope allocationScope);
void VKAPI_PTR VulkanFree(void* pUserData, void* pMemory);

const VkAllocationCallbacks* GetVulkanAllocationCallbacks();

} // namespace RHI
} // namespace Fly

#endif /* FLY_RHI_ALLOCATION_CALLBACKS_H */
