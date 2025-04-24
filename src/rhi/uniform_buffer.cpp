#include "core/assert.h"

#include "device.h"
#include "uniform_buffer.h"

namespace Hls
{

bool CopyDataToUniformBuffer(Device& device, const void* data, u64 size,
                             u64 offset, UniformBuffer& uniformBuffer)
{
    HLS_ASSERT(data);
    HLS_ASSERT(size);
    HLS_ASSERT(uniformBuffer.mappedPtr);

    memcpy(static_cast<u8*>(uniformBuffer.mappedPtr) + offset, data, size);

    return true;
}

bool CreateUniformBuffer(Device& device, const void* data, u64 size,
                         UniformBuffer& uniformBuffer)
{
    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = size;
    createInfo.usage =
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocCreateInfo.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    allocCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    if (vmaCreateBuffer(device.allocator, &createInfo, &allocCreateInfo,
                        &uniformBuffer.handle, &uniformBuffer.allocation,
                        &uniformBuffer.allocationInfo) != VK_SUCCESS)
    {
        return false;
    }

    bool res = vmaMapMemory(device.allocator, uniformBuffer.allocation,
                            &uniformBuffer.mappedPtr) == VK_SUCCESS;
    HLS_ASSERT(res);

    if (data)
    {
        CopyDataToUniformBuffer(device, data, size, 0, uniformBuffer);
    }

    return true;
}

void DestroyUniformBuffer(Device& device, UniformBuffer& uniformBuffer)
{
    HLS_ASSERT(uniformBuffer.handle != VK_NULL_HANDLE);
    vmaUnmapMemory(device.allocator, uniformBuffer.allocation);
    vmaDestroyBuffer(device.allocator, uniformBuffer.handle,
                     uniformBuffer.allocation);
    uniformBuffer.handle = VK_NULL_HANDLE;
}

} // namespace Hls
