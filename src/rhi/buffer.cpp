#include "core/assert.h"
#include "core/log.h"

#include "buffer.h"
#include "device.h"

namespace Hls
{
namespace RHI
{

bool CopyDataToBuffer(Device& device, const void* data, u64 size, u64 offset,
                      Buffer& buffer)
{
    HLS_ASSERT(buffer.handle != VK_NULL_HANDLE);
    HLS_ASSERT(data);
    HLS_ASSERT(size > 0);

    if (buffer.allocationInfo.pMappedData)
    {
        memcpy(static_cast<u8*>(buffer.allocationInfo.pMappedData) + offset,
               data, size);
        return true;
    }

    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = size;
    createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocCreateInfo.flags =
        VMA_ALLOCATION_CREATE_MAPPED_BIT |
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

    HLS_ASSERT(allocationInfo.pMappedData);
    memcpy(static_cast<u8*>(allocationInfo.pMappedData), data, size);

    BeginTransfer(device);
    CommandBuffer& cmd = TransferCommandBuffer(device);
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = offset;
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd.handle, stagingBuffer, buffer.handle, 1, &copyRegion);
    EndTransfer(device);
    vmaDestroyBuffer(device.allocator, stagingBuffer, allocation);

    return true;
}

bool CreateBuffer(Device& device, bool hostVisible, VkBufferUsageFlags usage,
                  const void* data, u64 size, Buffer& buffer)
{
    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = size;
    createInfo.usage = usage;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    allocCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (hostVisible)
    {
        allocCreateInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

        allocCreateInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
        allocCreateInfo.flags |=
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    if (vmaCreateBuffer(device.allocator, &createInfo, &allocCreateInfo,
                        &buffer.handle, &buffer.allocation,
                        &buffer.allocationInfo) != VK_SUCCESS)
    {
        return false;
    }

    if (data)
    {
        CopyDataToBuffer(device, data, size, 0, buffer);
    }

    bool storageUsage = usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bool uniformUsage = usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (!storageUsage && !uniformUsage)
    {
        HLS_DEBUG_LOG("Buffer[%llu] created with size %f MB", buffer.handle,
                      buffer.allocationInfo.size / 1024.0 / 1024.0);
        return true;
    }
    HLS_ASSERT(!storageUsage || !uniformUsage);

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer.handle;
    bufferInfo.offset = 0;
    bufferInfo.range = size;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = device.bindlessDescriptorSet;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    u32* pBindlessHandle = nullptr;
    if (uniformUsage)
    {
        pBindlessHandle = &device.bindlessUniformBufferHandleCount;
        descriptorWrite.dstBinding = HLS_UNIFORM_BUFFER_BINDING_INDEX;
        descriptorWrite.dstArrayElement =
            device.bindlessUniformBufferHandleCount;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
    else
    {
        pBindlessHandle = &device.bindlessStorageBufferHandleCount;
        descriptorWrite.dstBinding = HLS_STORAGE_BUFFER_BINDING_INDEX;
        descriptorWrite.dstArrayElement =
            device.bindlessStorageBufferHandleCount;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    vkUpdateDescriptorSets(device.logicalDevice, 1, &descriptorWrite, 0,
                           nullptr);
    buffer.bindlessHandle = (*pBindlessHandle)++;

    HLS_DEBUG_LOG("Buffer[%llu] created - alloc size %f MB", buffer.handle,
                  buffer.allocationInfo.size / 1024.0 / 1024.0);
    return true;
}

void DestroyBuffer(Device& device, Buffer& buffer)
{
    HLS_DEBUG_LOG("Buffer[%llu] destroyed - dealloc size: %f MB", buffer.handle,
                  buffer.allocationInfo.size / 1024.0 / 1024.0);
    HLS_ASSERT(buffer.handle != VK_NULL_HANDLE);
    vmaDestroyBuffer(device.allocator, buffer.handle, buffer.allocation);
    buffer.handle = VK_NULL_HANDLE;
}

bool CreateUniformBuffer(Device& device, const void* data, u64 size,
                         Buffer& buffer)
{
    return CreateBuffer(device, true,
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        data, size, buffer);
}

bool CreateStorageBuffer(Device& device, const void* data, u64 size,
                         Buffer& buffer)
{
    return CreateBuffer(device, false,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        data, size, buffer);
}

bool CreateIndexBuffer(Device& device, const void* data, u64 size,
                       Buffer& buffer)
{
    return CreateBuffer(device, false,
                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        data, size, buffer);
}

bool CreateIndirectBuffer(Device& device, bool hostVisible, const void* data,
                          u64 size, Buffer& buffer)
{
    return CreateBuffer(device, hostVisible,
                        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        data, size, buffer);
}

} // namespace RHI
} // namespace Hls
