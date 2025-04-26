#ifndef HLS_RHI_ALLOCATION_CALLBACKS_H
#define HLS_RHI_ALLOCATION_CALLBACKS_H

#include <volk.h>

namespace Hls
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
} // namespace Hls

#endif /* HLS_RHI_ALLOCATION_CALLBACKS_H */
