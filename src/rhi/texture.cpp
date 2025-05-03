#include <string.h>

#include "core/assert.h"

#include "allocation_callbacks.h"
#include "command_buffer.h"
#include "device.h"
#include "texture.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

static u32 Log2(u32 x)
{
    u32 result = 0;
    while (x >>= 1)
    {
        ++result;
    }
    return result;
}

namespace Hls
{
namespace RHI
{

static void GenerateMipmaps(RHI::CommandBuffer& cmd, RHI::Texture& texture)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = texture.handle;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    i32 mipWidth = static_cast<i32>(texture.width);
    i32 mipHeight = static_cast<i32>(texture.height);

    for (u32 i = 1; i < texture.mipLevelCount; i++)
    {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1,
                              mipHeight > 1 ? mipHeight / 2 : 1, 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(cmd.handle, texture.handle,
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture.handle,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                       VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &barrier);

        if (mipWidth > 1)
        {
            mipWidth /= 2;
        }
        if (mipHeight > 1)
        {
            mipHeight /= 2;
        }
    }

    barrier.subresourceRange.baseMipLevel = texture.mipLevelCount - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);
}

bool CreateSampler(Device& device, Sampler::FilterMode filterMode,
                   Sampler::WrapMode wrapMode, u32 mipLevelCount,
                   Sampler& sampler)
{
    VkSamplerCreateInfo samplerCreateInfo{};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    u32 anisotropy = 0;
    switch (filterMode)
    {
        case Sampler::FilterMode::Nearest:
        {
            samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
            samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        }
        case Sampler::FilterMode::Bilinear:
        {
            samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        }
        case Sampler::FilterMode::Trilinear:
        {
            samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
        }
        case Sampler::FilterMode::Anisotropy2x:
        {
            samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            anisotropy = 2;
            break;
        }
        case Sampler::FilterMode::Anisotropy4x:
        {
            samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            anisotropy = 4;
            break;
        }
        case Sampler::FilterMode::Anisotropy8x:
        {
            samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            anisotropy = 8;
            break;
        }
        case Sampler::FilterMode::Anisotropy16x:
        {
            samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            anisotropy = 16;
            break;
        }
        default:
        {
            return false;
        }
    }

    switch (wrapMode)
    {
        case Sampler::WrapMode::Repeat:
        {
            samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            break;
        }
        case Sampler::WrapMode::Mirrored:
        {
            samplerCreateInfo.addressModeU =
                VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            samplerCreateInfo.addressModeV =
                VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            samplerCreateInfo.addressModeW =
                VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            break;
        }
        case Sampler::WrapMode::Clamp:
        {
            samplerCreateInfo.addressModeU =
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.addressModeV =
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.addressModeW =
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            break;
        }
        default:
        {
            return false;
        }
    }

    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = static_cast<f32>(mipLevelCount);

    samplerCreateInfo.anisotropyEnable = anisotropy > 0;
    samplerCreateInfo.maxAnisotropy = static_cast<f32>(anisotropy);

    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    samplerCreateInfo.compareEnable = VK_FALSE;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    if (vkCreateSampler(device.logicalDevice, &samplerCreateInfo,
                        GetVulkanAllocationCallbacks(),
                        &sampler.handle) != VK_SUCCESS)
    {
        return false;
    }

    sampler.filterMode = filterMode;
    sampler.wrapMode = wrapMode;
    return true;
}

void DestroySampler(Device& device, Sampler& sampler)
{
    HLS_ASSERT(sampler.handle != VK_NULL_HANDLE);
    vkDestroySampler(device.logicalDevice, sampler.handle,
                     GetVulkanAllocationCallbacks());
    sampler.handle = VK_NULL_HANDLE;
}

bool CreateTexture(Device& device, u8* data, u32 width, u32 height,
                   u32 channelCount, VkFormat format,
                   Sampler::FilterMode filterMode, Sampler::WrapMode wrapMode,
                   Texture& texture)
{
    HLS_ASSERT(width > 0);
    HLS_ASSERT(height > 0);

    u32 mipLevelCount = 1;
    VkImageUsageFlags usage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (filterMode != Sampler::FilterMode::Nearest)
    {
        mipLevelCount = Log2(MAX(width, height)) + 1;
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    VkImageCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.extent.width = width;
    createInfo.extent.height = height;
    createInfo.extent.depth = 1;
    createInfo.mipLevels = mipLevelCount;
    createInfo.arrayLayers = 1;
    createInfo.format = format;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.usage = usage;
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

    VkImageViewCreateInfo viewCreateInfo{};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image = texture.handle;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = format;
    viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = mipLevelCount;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device.logicalDevice, &viewCreateInfo,
                          GetVulkanAllocationCallbacks(),
                          &texture.imageView) != VK_SUCCESS)
    {
        vmaDestroyImage(device.allocator, texture.handle, texture.allocation);
        return false;
    }

    if (!CreateSampler(device, filterMode, wrapMode, mipLevelCount,
                       texture.sampler))
    {
        vkDestroyImageView(device.logicalDevice, texture.imageView,
                           GetVulkanAllocationCallbacks());
        vmaDestroyImage(device.allocator, texture.handle, texture.allocation);
        return false;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = texture.imageView;
    imageInfo.sampler = texture.sampler.handle;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = device.bindlessDescriptorSet;
    descriptorWrite.dstBinding = HLS_TEXTURE_BINDING_INDEX;
    descriptorWrite.dstArrayElement = device.bindlessTextureHandleCount;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device.logicalDevice, 1, &descriptorWrite, 0,
                           nullptr);

    texture.bindlessHandle = device.bindlessTextureHandleCount++;
    texture.width = width;
    texture.height = height;
    texture.format = format;
    texture.mipLevelCount = mipLevelCount;

    if (data)
    {
        HLS_ASSERT(width);
        HLS_ASSERT(height);
        HLS_ASSERT(channelCount);

        u64 allocSize = sizeof(u8) * width * height * channelCount;
        VkBufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfo.size = allocSize;
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
            DestroySampler(device, texture.sampler);
            vkDestroyImageView(device.logicalDevice, texture.imageView,
                               GetVulkanAllocationCallbacks());
            vmaDestroyImage(device.allocator, texture.handle,
                            texture.allocation);
            return false;
        }

        void* mappedPtr = nullptr;
        bool res = vmaMapMemory(device.allocator, allocation, &mappedPtr) ==
                   VK_SUCCESS;
        HLS_ASSERT(res);
        memcpy(mappedPtr, data, allocSize);
        vmaUnmapMemory(device.allocator, allocation);

        BeginTransfer(device);
        RHI::CommandBuffer& cmd = TransferCommandBuffer(device);

        RHI::RecordTransitionImageLayout(cmd, texture.handle,
                                         VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent.width = width;
        copyRegion.imageExtent.height = height;
        copyRegion.imageExtent.depth = 1;

        vkCmdCopyBufferToImage(cmd.handle, stagingBuffer, texture.handle,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &copyRegion);

        GenerateMipmaps(cmd, texture);

        EndTransfer(device);

        vmaDestroyBuffer(device.allocator, stagingBuffer, allocation);
    }

    return true;
}

void DestroyTexture(Device& device, Texture& texture)
{
    DestroySampler(device, texture.sampler);
    vkDestroyImageView(device.logicalDevice, texture.imageView,
                       GetVulkanAllocationCallbacks());
    vmaDestroyImage(device.allocator, texture.handle, texture.allocation);
    texture.imageView = VK_NULL_HANDLE;
    texture.format = VK_FORMAT_UNDEFINED;
    texture.handle = VK_NULL_HANDLE;
}

bool ModifyTextureSampler(Device& device, Sampler::FilterMode filterMode,
                          Sampler::WrapMode wrapMode, u32 anisotropy,
                          Texture& texture)
{
    HLS_ASSERT(texture.handle != VK_NULL_HANDLE);
    HLS_ASSERT(texture.sampler.handle != VK_NULL_HANDLE);

    if (texture.sampler.filterMode == filterMode &&
        texture.sampler.wrapMode == wrapMode)
    {
        return true;
    }

    DeviceWaitIdle(device);

    DestroySampler(device, texture.sampler);
    return CreateSampler(device, filterMode, wrapMode, texture.mipLevelCount,
                         texture.sampler);
}

bool CreateDepthTexture(Device& device, u32 width, u32 height, VkFormat format,
                        DepthTexture& depthTexture)
{
    VkImageCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.extent.width = static_cast<u32>(width);
    createInfo.extent.height = static_cast<u32>(height);
    createInfo.extent.depth = 1;
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.format = format;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.flags = 0;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocCreateInfo.requiredFlags =
        VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vmaCreateImage(device.allocator, &createInfo, &allocCreateInfo,
                       &depthTexture.handle, &depthTexture.allocation,
                       &depthTexture.allocationInfo) != VK_SUCCESS)
    {
        return false;
    }

    depthTexture.format = format;
    depthTexture.width = width;
    depthTexture.height = height;

    VkImageViewCreateInfo viewCreateInfo{};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image = depthTexture.handle;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = format;
    viewCreateInfo.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device.logicalDevice, &viewCreateInfo,
                          GetVulkanAllocationCallbacks(),
                          &device.depthTexture.imageView) != VK_SUCCESS)
    {
        return false;
    }

    return true;
}

void DestroyDepthTexture(Device& device, DepthTexture& depthTexture)
{
    vkDestroyImageView(device.logicalDevice, depthTexture.imageView,
                       GetVulkanAllocationCallbacks());
    vmaDestroyImage(device.allocator, depthTexture.handle,
                    depthTexture.allocation);
    depthTexture.handle = VK_NULL_HANDLE;
}

} // namespace RHI
} // namespace Hls
