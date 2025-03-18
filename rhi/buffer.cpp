#include <string.h>

#include "buffer.h"
#include "core/assert.h"

namespace Hls
{

bool CreateBuffer(Device& device, VkBufferUsageFlags usage,
                  VmaMemoryUsage memoryUsage,
                  VmaAllocationCreateFlagBits allocationFlags, u64 size,
                  Buffer& buffer)
{
    HLS_ASSERT(size > 0);

    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.size = size;
    createInfo.usage = usage;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = memoryUsage;
    allocCreateInfo.flags = allocationFlags;

    VkResult res = vmaCreateBuffer(device.allocator, &createInfo,
                                   &allocCreateInfo, &buffer.handle,
                                   &buffer.allocation, &buffer.allocationInfo);
    return res == VK_SUCCESS;
}

void DestroyBuffer(Device& device, Buffer& buffer)
{
    HLS_ASSERT(buffer.handle != VK_NULL_HANDLE);
    vmaDestroyBuffer(device.allocator, buffer.handle, buffer.allocation);
    buffer.handle = VK_NULL_HANDLE;
}

bool MapBuffer(Device& device, Buffer& buffer)
{
    HLS_ASSERT(buffer.mappedPtr == nullptr);
    return vmaMapMemory(device.allocator, buffer.allocation,
                        &buffer.mappedPtr) == VK_SUCCESS;
}

void CopyDataToBuffer(Device& device, Buffer& buffer, const void* source,
                      u64 size)
{
    HLS_ASSERT(source);
    HLS_ASSERT(size > 0);
    HLS_ASSERT(size <= buffer.allocationInfo.size);
    memcpy(buffer.mappedPtr, source, size);
}

void UnmapBuffer(Device& device, Buffer& buffer)
{
    HLS_ASSERT(buffer.mappedPtr != nullptr);
    vmaUnmapMemory(device.allocator, buffer.allocation);
}

} // namespace Hls
