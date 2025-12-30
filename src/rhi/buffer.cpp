#include <string.h>

#include "core/assert.h"
#include "core/log.h"

#include "buffer.h"
#include "device.h"

namespace Fly
{
namespace RHI
{

static bool CreateBufferImpl(Device& device, bool hostVisible,
                             VkBufferUsageFlags usage, u64 size, Buffer& buffer)
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
        allocCreateInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

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

    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        VkBufferDeviceAddressInfoKHR addrInfo{};
        addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addrInfo.buffer = buffer.handle;
        buffer.address =
            vkGetBufferDeviceAddress(device.logicalDevice, &addrInfo);
    }

    return true;
}

bool CopyDataToBuffer(Device& device, const void* data, u64 size, u64 offset,
                      Buffer& buffer)
{
    FLY_ASSERT(buffer.handle != VK_NULL_HANDLE);
    FLY_ASSERT(data);
    FLY_ASSERT(size > 0);

    if (buffer.allocationInfo.pMappedData)
    {
        memcpy(static_cast<u8*>(buffer.allocationInfo.pMappedData) + offset,
               data, size);
        return true;
    }

    Buffer stagingBuffer;
    if (!CreateBufferImpl(device, true, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size,
                          stagingBuffer))
    {
        return false;
    }
    FLY_ASSERT(stagingBuffer.allocationInfo.pMappedData);
    memcpy(static_cast<u8*>(stagingBuffer.allocationInfo.pMappedData), data,
           size);

    BeginOneTimeSubmit(device);
    {
        CommandBuffer& cmd = OneTimeSubmitCommandBuffer(device);
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = offset;
        copyRegion.size = size;
        vkCmdCopyBuffer(cmd.handle, stagingBuffer.handle, buffer.handle, 1,
                        &copyRegion);
    }
    EndOneTimeSubmit(device);

    DestroyBuffer(device, stagingBuffer);

    return true;
}

bool CreateBuffer(Device& device, bool hostVisible, VkBufferUsageFlags usage,
                  const void* data, u64 size, Buffer& buffer)
{
    if (!CreateBufferImpl(device, hostVisible, usage, size, buffer))
    {
        return false;
    }

    if (data)
    {
        CopyDataToBuffer(device, data, size, 0, buffer);
    }

    buffer.accessMask = VK_ACCESS_2_NONE;
    buffer.pipelineStageMask = VK_PIPELINE_STAGE_2_NONE;
    buffer.hostVisible = hostVisible;
    buffer.usage = usage;
    buffer.bindlessHandle = FLY_MAX_U32;

    bool storageUsage = usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bool uniformUsage = usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (storageUsage || uniformUsage)
    {

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
            descriptorWrite.dstBinding = FLY_UNIFORM_BUFFER_BINDING_INDEX;
            descriptorWrite.dstArrayElement =
                device.bindlessUniformBufferHandleCount;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        else
        {
            pBindlessHandle = &device.bindlessStorageBufferHandleCount;
            descriptorWrite.dstBinding = FLY_STORAGE_BUFFER_BINDING_INDEX;
            descriptorWrite.dstArrayElement =
                device.bindlessStorageBufferHandleCount;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
        vkUpdateDescriptorSets(device.logicalDevice, 1, &descriptorWrite, 0,
                               nullptr);
        buffer.bindlessHandle = (*pBindlessHandle)++;
    }

    // FLY_DEBUG_LOG("Buffer[%llu] created: bindless handle %u, alloc size %f MB",
    //               buffer.handle, buffer.bindlessHandle,
    //               buffer.allocationInfo.size / 1024.0 / 1024.0);
    return true;
}

void DestroyBuffer(Device& device, Buffer& buffer)
{
    // FLY_DEBUG_LOG(
    //     "Buffer[%llu] destroyed: bindless handle %u, dealloc size: %f MB",
    //     buffer.handle, buffer.bindlessHandle,
    //     buffer.allocationInfo.size / 1024.0 / 1024.0);
    FLY_ASSERT(buffer.handle != VK_NULL_HANDLE);
    vmaDestroyBuffer(device.allocator, buffer.handle, buffer.allocation);
    buffer.handle = VK_NULL_HANDLE;
}

} // namespace RHI
} // namespace Fly
