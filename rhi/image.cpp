#include <string.h>

#include "core/assert.h"

#include "buffer.h"
#include "command_buffer.h"
#include "image.h"

namespace Hls
{

bool TransferImageDataToTexture(Device& device, const Image& image,
                                Texture& texture)
{
    Buffer transferBuffer;

    u64 allocSize =
        sizeof(u8) * image.width * image.height * image.channelCount;
    if (!CreateBuffer(device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                      allocSize, transferBuffer))
    {
        return false;
    }

    if (!MapBuffer(device, transferBuffer))
    {
        return false;
    }
    memcpy(transferBuffer.mappedPtr, image.data, allocSize);

    BeginTransfer(device);
    CommandBuffer& cmd = TransferCommandBuffer(device);

    RecordTransitionImageLayout(cmd, texture.handle, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent.width = image.width;
    copyRegion.imageExtent.height = image.height;
    copyRegion.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(cmd.handle, transferBuffer.handle, texture.handle,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copyRegion);

    RecordTransitionImageLayout(cmd, texture.handle,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    EndTransfer(device);

    UnmapBuffer(device, transferBuffer);
    DestroyBuffer(device, transferBuffer);

    return true;
}

bool CreateTexture(Device& device, u32 width, u32 height, VkFormat format,
                   Texture& texture)
{
    HLS_ASSERT(width > 0);
    HLS_ASSERT(height > 0);

    VkImageCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.extent.width = width;
    createInfo.extent.height = height;
    createInfo.extent.depth = 1;
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.format = format;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.usage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.flags = 0;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocCreateInfo.requiredFlags =
        VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vmaCreateImage(device.allocator, &createInfo, &allocCreateInfo,
                       &texture.handle, &texture.allocation,
                       &texture.allocationInfo) != VK_SUCCESS)
    {
        return false;
    }

    texture.width = width;
    texture.height = height;

    return true;
}

void DestroyTexture(Device& device, Texture& texture)
{
    vmaDestroyImage(device.allocator, texture.handle, texture.allocation);
    texture.handle = VK_NULL_HANDLE;
}

} // namespace Hls
