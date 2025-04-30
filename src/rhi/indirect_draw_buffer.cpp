#include "core/assert.h"

#include "device.h"
#include "indirect_draw_buffer.h"

namespace Hls
{
namespace RHI
{

bool CopyDataToIndirectDrawBuffer(Device& device, const void* data, u64 size,
                                  u64 offset,
                                  IndirectDrawBuffer& indirectDrawBuffer)
{
    HLS_ASSERT(data);
    HLS_ASSERT(size);
    HLS_ASSERT(indirectDrawBuffer.mappedPtr);

    memcpy(static_cast<u8*>(indirectDrawBuffer.mappedPtr) + offset, data, size);

    return true;
}

bool CreateIndirectDrawBuffer(Device& device, const void* data, u64 size,
                              IndirectDrawBuffer& indirectDrawBuffer)
{
    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = size;
    createInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                       VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocCreateInfo.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    allocCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    if (vmaCreateBuffer(device.allocator, &createInfo, &allocCreateInfo,
                        &indirectDrawBuffer.handle,
                        &indirectDrawBuffer.allocation,
                        &indirectDrawBuffer.allocationInfo) != VK_SUCCESS)
    {
        return false;
    }

    bool res = vmaMapMemory(device.allocator, indirectDrawBuffer.allocation,
                            &indirectDrawBuffer.mappedPtr) == VK_SUCCESS;
    HLS_ASSERT(res);

    if (data)
    {
        CopyDataToIndirectDrawBuffer(device, data, size, 0, indirectDrawBuffer);
    }

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = indirectDrawBuffer.handle;
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

    indirectDrawBuffer.bindlessHandle =
        device.bindlessStorageBufferHandleCount++;

    return true;
}

void DestroyIndirectDrawBuffer(Device& device,
                               IndirectDrawBuffer& indirectDrawBuffer)
{
    HLS_ASSERT(indirectDrawBuffer.handle != VK_NULL_HANDLE);
    vmaUnmapMemory(device.allocator, indirectDrawBuffer.allocation);
    vmaDestroyBuffer(device.allocator, indirectDrawBuffer.handle,
                     indirectDrawBuffer.allocation);
    indirectDrawBuffer.handle = VK_NULL_HANDLE;
}

} // namespace RHI
} // namespace Hls
