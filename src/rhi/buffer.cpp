#include <string.h>

#include "core/assert.h"

#include "buffer.h"

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

    return vmaCreateBuffer(device.allocator, &createInfo, &allocCreateInfo,
                           &buffer.handle, &buffer.allocation,
                           &buffer.allocationInfo) == VK_SUCCESS;
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

void CopyDataToBuffer(Device& device, Buffer& buffer, u64 offset,
                      const void* source, u64 size)
{
    HLS_ASSERT(source);
    HLS_ASSERT(size > 0);
    HLS_ASSERT(size + offset <= buffer.allocationInfo.size);
    memcpy(static_cast<u8*>(buffer.mappedPtr) + offset, source, size);
}

void UnmapBuffer(Device& device, Buffer& buffer)
{
    HLS_ASSERT(buffer.mappedPtr != nullptr);
    vmaUnmapMemory(device.allocator, buffer.allocation);
}

bool TransferDataToBuffer(Device& device, const void* data, u64 size,
                          Buffer& buffer)
{
    Buffer transferBuffer;

    if (!CreateBuffer(device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                      size, transferBuffer))
    {
        return false;
    }

    if (!MapBuffer(device, transferBuffer))
    {
        return false;
    }
    memcpy(transferBuffer.mappedPtr, data, size);

    BeginTransfer(device);
    CommandBuffer& cmd = TransferCommandBuffer(device);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd.handle, transferBuffer.handle, buffer.handle, 1, &copyRegion);

    EndTransfer(device);

    UnmapBuffer(device, transferBuffer);
    DestroyBuffer(device, transferBuffer);

    return true;
}

} // namespace Hls
