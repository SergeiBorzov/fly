#include "core/assert.h"

#include "device.h"
#include "storage_buffer.h"

namespace Hls
{
namespace RHI
{

bool CopyDataToStorageBuffer(Device& device, const void* data, u64 size,
                             u64 offset, StorageBuffer& storageBuffer)
{
    HLS_ASSERT(data);
    HLS_ASSERT(size);

    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = size;
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

    memcpy(static_cast<u8*>(mappedPtr), data, size);
    vmaUnmapMemory(device.allocator, allocation);

    BeginTransfer(device);
    CommandBuffer& cmd = TransferCommandBuffer(device);
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = offset;
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd.handle, stagingBuffer, storageBuffer.handle, 1,
                    &copyRegion);
    EndTransfer(device);

    vmaDestroyBuffer(device.allocator, stagingBuffer, allocation);

    return true;
}

bool CreateStorageBuffer(Device& device, const void* data, u64 size,
                         StorageBuffer& storageBuffer)
{
    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = size;
    createInfo.usage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    allocCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (vmaCreateBuffer(device.allocator, &createInfo, &allocCreateInfo,
                        &storageBuffer.handle, &storageBuffer.allocation,
                        &storageBuffer.allocationInfo) != VK_SUCCESS)
    {
        return false;
    }

    if (data)
    {
        CopyDataToStorageBuffer(device, data, size, 0, storageBuffer);
    }

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = storageBuffer.handle;
    bufferInfo.offset = 0;
    bufferInfo.range = size;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = device.bindlessDescriptorSet;
    descriptorWrite.dstBinding = HLS_STORAGE_BUFFER_BINDING_INDEX;
    descriptorWrite.dstArrayElement = device.bindlessStorageBufferHandleCount;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(device.logicalDevice, 1, &descriptorWrite, 0,
                           nullptr);
    storageBuffer.bindlessHandle = device.bindlessStorageBufferHandleCount++;

    return true;
}

void DestroyStorageBuffer(Device& device, StorageBuffer& storageBuffer)
{
    HLS_ASSERT(storageBuffer.handle != VK_NULL_HANDLE);
    vmaDestroyBuffer(device.allocator, storageBuffer.handle,
                     storageBuffer.allocation);
    storageBuffer.handle = VK_NULL_HANDLE;
}

} // namespace RHI
} // namespace Hls
