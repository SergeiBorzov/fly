#ifndef HLS_RHI_BUFFER_H
#define HLS_RHI_BUFFER_H

#include "context.h"

namespace Hls
{

struct Buffer
{
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;
    void* mappedPtr = nullptr;
};

bool CreateBuffer(Device& device, VkBufferUsageFlags usage,
                  VmaMemoryUsage memoryUsage,
                  VmaAllocationCreateFlagBits allocationFlags, u64 size,
                  Buffer& buffer);
void DestroyBuffer(Device& device, Buffer& buffer);

bool MapBuffer(Device& device, Buffer& buffer);
void UnmapBuffer(Device& device, Buffer& buffer);

void CopyDataToBuffer(Device& device, Buffer& buffer, const void* source, u64 size);

} // namespace Hls

#endif /* HLS_RHI_BUFFER_H */
