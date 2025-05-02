#ifndef HLS_RHI_BUFFER_H
#define HLS_RHI_BUFFER_H

#include <volk.h>

#include "core/types.h"
#include "vma.h"

namespace Hls
{
namespace RHI
{

struct Device;

struct Buffer
{
    VmaAllocationInfo allocationInfo;
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation;
    u32 bindlessHandle = HLS_MAX_U32;
};

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
} // namespace Hls

#endif /* HLS_RHI_BUFFER_H */
