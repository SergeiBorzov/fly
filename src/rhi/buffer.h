#ifndef FLY_RHI_BUFFER_H
#define FLY_RHI_BUFFER_H

#include <volk.h>

#include "core/types.h"

#include "vma.h"

namespace Fly
{
namespace RHI
{

struct Device;

struct Buffer
{
    VmaAllocationInfo allocationInfo;
    VkAccessFlagBits2 accessMask;
    VkPipelineStageFlagBits2 pipelineStageMask;
    VmaAllocation allocation;
    VkBuffer handle = VK_NULL_HANDLE;
    VkBufferUsageFlags usage;
    u32 bindlessHandle = FLY_MAX_U32;
    bool hostVisible = false;
};

inline void* BufferMappedPtr(Buffer& buffer)
{
    return buffer.allocationInfo.pMappedData;
}

bool CreateBuffer(Device& device, bool hostVisible, VkBufferUsageFlags usage,
                  const void* data, u64 size, Buffer& buffer);
void DestroyBuffer(Device& device, Buffer& buffer);
bool CopyDataToBuffer(Device& device, const void* data, u64 size, u64 offset,
                      Buffer& buffer);
bool CreateUniformBuffer(Device& device, const void* data, u64 size,
                         Buffer& buffer);
bool CreateStorageBuffer(Device& device, bool hostVisible, const void* data,
                         u64 size, Buffer& buffer);
bool CreateIndexBuffer(Device& device, const void* data, u64 size,
                       Buffer& buffer);
bool CreateIndirectBuffer(Device& device, bool hostVisible, const void* data,
                          u64 size, Buffer& buffer);

} // namespace RHI
} // namespace Fly

#endif /* FLY_RHI_BUFFER_H */
