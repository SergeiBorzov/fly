#include "core/assert.h"

#include "device.h"
#include "index_buffer.h"

namespace Hls
{

bool CopyDataToIndexBuffer(Device& device, const u32* indices, u32 indexCount,
                           u32 offsetCount, IndexBuffer& indexBuffer)
{
    HLS_ASSERT(indices);
    HLS_ASSERT(indexCount);

    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = sizeof(u32) * indexCount;
    createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocCreateInfo.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT;
    allocCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    VmaAllocationInfo allocationInfo;
    VkBuffer stagingBuffer;
    VmaAllocation allocation;
    if (vmaCreateBuffer(device.allocator, &createInfo, &allocCreateInfo,
                        &stagingBuffer, &allocation,
                        &allocationInfo) != VK_SUCCESS)
    {
        return false;
    }

    void* mappedPtr = nullptr;
    bool res =
        vmaMapMemory(device.allocator, allocation, &mappedPtr) == VK_SUCCESS;
    HLS_ASSERT(res);

    memcpy(static_cast<u8*>(mappedPtr), indices, indexCount * sizeof(u32));
    vmaUnmapMemory(device.allocator, allocation);

    BeginTransfer(device);
    CommandBuffer& cmd = TransferCommandBuffer(device);
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = offsetCount * sizeof(u32);
    copyRegion.size = sizeof(u32) * indexCount;
    vkCmdCopyBuffer(cmd.handle, stagingBuffer, indexBuffer.handle, 1,
                    &copyRegion);
    EndTransfer(device);

    vmaDestroyBuffer(device.allocator, stagingBuffer, allocation);

    return true;
}

bool CreateIndexBuffer(Device& device, const u32* indices, u32 indexCount,
                       IndexBuffer& indexBuffer)
{
    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = sizeof(u32) * indexCount;
    createInfo.usage =
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    allocCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (vmaCreateBuffer(device.allocator, &createInfo, &allocCreateInfo,
                        &indexBuffer.handle, &indexBuffer.allocation,
                        &indexBuffer.allocationInfo) != VK_SUCCESS)
    {
        return false;
    }

    if (indices)
    {
        CopyDataToIndexBuffer(device, indices, indexCount, 0, indexBuffer);
    }

    return true;
}

void DestroyIndexBuffer(Device& device, IndexBuffer& indexBuffer)
{
    HLS_ASSERT(indexBuffer.handle != VK_NULL_HANDLE);
    vmaDestroyBuffer(device.allocator, indexBuffer.handle,
                     indexBuffer.allocation);
    indexBuffer.handle = VK_NULL_HANDLE;
}

} // namespace Hls
