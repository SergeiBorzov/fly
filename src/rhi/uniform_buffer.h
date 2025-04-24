#ifndef HLS_RHI_UNIFORM_BUFFER_H
#define HLS_RHI_UNIFORM_BUFFER_H

#include <volk.h>

#include "core/types.h"
#include "vma.h"

namespace Hls
{

struct Device;

struct UniformBuffer
{
    VmaAllocationInfo allocationInfo;
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation;
    void* mappedPtr = nullptr;
    u32 bindlessHandle = HLS_MAX_U32;
};

bool CreateUniformBuffer(Device& device, const void* data, u64 size,
                         UniformBuffer& uniformBuffer);
void DestroyUniformBuffer(Device& device, UniformBuffer& uniformBuffer);
bool CopyDataToUniformBuffer(Device& device, const void* data, u64 size,
                             u64 offset, UniformBuffer& uniformBuffer);

} // namespace Hls

#endif /* HLS_RHI_UNIFORM_BUFFER_H */
