#include "core/assert.h"
#include "core/log.h"

#include "allocation_callbacks.h"
#include "buffer.h"
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

static void GenerateMipmaps(Fly::RHI::CommandBuffer& cmd,
                            Fly::RHI::Cubemap& cubemap,
                            VkImageLayout targetLayout)
{
    return;
}

static void GenerateMipmaps(Fly::RHI::CommandBuffer& cmd,
                            Fly::RHI::Texture& texture,
                            VkImageLayout targetLayout)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = texture.image;
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
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
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

        vkCmdBlitImage(cmd.handle, texture.image,
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture.image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                       VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = targetLayout;
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
    barrier.newLayout = targetLayout;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);
}

static VkImage CreateVulkanImage2D(Fly::RHI::Device& device,
                                   VmaAllocationInfo& allocationInfo,
                                   VmaAllocation& allocation,
                                   VkImageUsageFlags usage, VkFormat format,
                                   VkImageTiling tiling, u32 width, u32 height,
                                   u32 mipLevelCount = 1)
{
    VkImageCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.format = format;
    createInfo.extent.width = width;
    createInfo.extent.height = height;
    createInfo.extent.depth = 1;
    createInfo.mipLevels = mipLevelCount;
    createInfo.arrayLayers = 1;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = usage;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocCreateInfo.requiredFlags =
        VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImage result = VK_NULL_HANDLE;
    if (vmaCreateImage(device.allocator, &createInfo, &allocCreateInfo, &result,
                       &allocation, &allocationInfo) != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }
    return result;
}

static VkImageView CreateVulkanImageView2D(Fly::RHI::Device& device,
                                           VkImage image, VkFormat format,
                                           u32 mipLevelCount,
                                           VkImageAspectFlags aspectMask)
{
    VkImageViewCreateInfo viewCreateInfo{};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image = image;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = format;
    viewCreateInfo.subresourceRange.aspectMask = aspectMask;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = mipLevelCount;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;

    VkImageView result;
    if (vkCreateImageView(device.logicalDevice, &viewCreateInfo,
                          Fly::RHI::GetVulkanAllocationCallbacks(),
                          &result) != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }

    return result;
}

namespace Fly
{
namespace RHI
{

u32 GetTexelSize(VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_R8_UNORM:
        case VK_FORMAT_R8_SNORM:
        case VK_FORMAT_R8_UINT:
        case VK_FORMAT_R8_SINT:
        case VK_FORMAT_R8_SRGB:
        case VK_FORMAT_S8_UINT:
        {
            return 1;
        }

        case VK_FORMAT_R8G8_UNORM:
        case VK_FORMAT_R8G8_SNORM:
        case VK_FORMAT_R8G8_UINT:
        case VK_FORMAT_R8G8_SINT:
        case VK_FORMAT_R8G8_SRGB:
        case VK_FORMAT_R16_UNORM:
        case VK_FORMAT_R16_SNORM:
        case VK_FORMAT_R16_UINT:
        case VK_FORMAT_R16_SINT:
        case VK_FORMAT_R16_SFLOAT:
        case VK_FORMAT_D16_UNORM:
        {
            return 2;
        }

        case VK_FORMAT_R8G8B8_UNORM:
        case VK_FORMAT_R8G8B8_SNORM:
        case VK_FORMAT_R8G8B8_UINT:
        case VK_FORMAT_R8G8B8_SINT:
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_B8G8R8_UNORM:
        case VK_FORMAT_B8G8R8_SNORM:
        case VK_FORMAT_B8G8R8_UINT:
        case VK_FORMAT_B8G8R8_SINT:
        case VK_FORMAT_B8G8R8_SRGB:
        {
            return 3;
        }

        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SNORM:
        case VK_FORMAT_R8G8B8A8_UINT:
        case VK_FORMAT_R8G8B8A8_SINT:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SNORM:
        case VK_FORMAT_B8G8R8A8_UINT:
        case VK_FORMAT_B8G8R8A8_SINT:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
        case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
        case VK_FORMAT_A8B8G8R8_UINT_PACK32:
        case VK_FORMAT_A8B8G8R8_SINT_PACK32:
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
        case VK_FORMAT_R32_UINT:
        case VK_FORMAT_R32_SINT:
        case VK_FORMAT_R32_SFLOAT:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT:
        {
            return 4;
        }

        case VK_FORMAT_D32_SFLOAT_S8_UINT:
        {
            return 5;
        }

        case VK_FORMAT_R16G16B16_UNORM:
        case VK_FORMAT_R16G16B16_SNORM:
        case VK_FORMAT_R16G16B16_UINT:
        case VK_FORMAT_R16G16B16_SINT:
        case VK_FORMAT_R16G16B16_SFLOAT:
        {
            return 6;
        }

        case VK_FORMAT_R16G16B16A16_UNORM:
        case VK_FORMAT_R16G16B16A16_SNORM:
        case VK_FORMAT_R16G16B16A16_UINT:
        case VK_FORMAT_R16G16B16A16_SINT:
        case VK_FORMAT_R16G16B16A16_SFLOAT:
        case VK_FORMAT_R32G32_UINT:
        case VK_FORMAT_R32G32_SINT:
        case VK_FORMAT_R32G32_SFLOAT:
        {
            return 8;
        }

        case VK_FORMAT_R32G32B32_UINT:
        case VK_FORMAT_R32G32B32_SINT:
        case VK_FORMAT_R32G32B32_SFLOAT:
        {
            return 12;
        }

        case VK_FORMAT_R32G32B32A32_UINT:
        case VK_FORMAT_R32G32B32A32_SINT:
        case VK_FORMAT_R32G32B32A32_SFLOAT:
        {
            return 16;
        }

        default:
        {
            FLY_ASSERT(false);
            return 0;
        }
    }
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
    FLY_ASSERT(sampler.handle != VK_NULL_HANDLE);
    vkDestroySampler(device.logicalDevice, sampler.handle,
                     GetVulkanAllocationCallbacks());
    sampler.handle = VK_NULL_HANDLE;
}

bool CreateTexture(Device& device, void* data, u64 dataSize, u32 width,
                   u32 height, VkFormat format, Sampler::FilterMode filterMode,
                   Sampler::WrapMode wrapMode, Texture& texture)
{
    FLY_ASSERT(width > 0);
    FLY_ASSERT(height > 0);
    FLY_ASSERT(dataSize == GetTexelSize(format) * width * height);

    u32 mipLevelCount = 1;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                              VK_IMAGE_USAGE_SAMPLED_BIT |
                              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (filterMode != Sampler::FilterMode::Nearest)
    {
        mipLevelCount = Log2(MAX(width, height)) + 1;
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    texture.image = CreateVulkanImage2D(
        device, texture.allocationInfo, texture.allocation, usage, format,
        VK_IMAGE_TILING_OPTIMAL, width, height, mipLevelCount);
    if (texture.image == VK_NULL_HANDLE)
    {
        return false;
    }

    texture.imageView =
        CreateVulkanImageView2D(device, texture.image, format, mipLevelCount,
                                VK_IMAGE_ASPECT_COLOR_BIT);
    if (!texture.imageView)
    {
        vmaDestroyImage(device.allocator, texture.image, texture.allocation);
        return false;
    }

    if (!CreateSampler(device, filterMode, wrapMode, mipLevelCount,
                       texture.sampler))
    {
        vkDestroyImageView(device.logicalDevice, texture.imageView,
                           GetVulkanAllocationCallbacks());
        vmaDestroyImage(device.allocator, texture.image, texture.allocation);
        return false;
    }
    VkImageLayout targetImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = targetImageLayout;
    imageInfo.imageView = texture.imageView;
    imageInfo.sampler = texture.sampler.handle;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = device.bindlessDescriptorSet;
    descriptorWrite.dstBinding = FLY_TEXTURE_BINDING_INDEX;
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
        RHI::Buffer stagingBuffer;
        if (!CreateBuffer(device, true, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, data,
                          dataSize, stagingBuffer))
        {
            DestroySampler(device, texture.sampler);
            vkDestroyImageView(device.logicalDevice, texture.imageView,
                               GetVulkanAllocationCallbacks());
            vmaDestroyImage(device.allocator, texture.image,
                            texture.allocation);
            return false;
        }

        BeginTransfer(device);
        RHI::CommandBuffer& cmd = TransferCommandBuffer(device);

        RHI::RecordTransitionImageLayout(cmd, texture.image,
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

        vkCmdCopyBufferToImage(cmd.handle, stagingBuffer.handle, texture.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &copyRegion);

        GenerateMipmaps(cmd, texture, targetImageLayout);
        EndTransfer(device);

        DestroyBuffer(device, stagingBuffer);
    }
    else
    {
        BeginTransfer(device);
        RHI::CommandBuffer& cmd = TransferCommandBuffer(device);
        RHI::RecordTransitionImageLayout(
            cmd, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, targetImageLayout);
        EndTransfer(device);
    }

    texture.imageLayout = targetImageLayout;

    FLY_DEBUG_LOG("Texture [%llu] created with size %f MB: bindless handle %u",
                  texture.image, texture.allocationInfo.size / 1024.0 / 1024.0,
                  texture.bindlessHandle);
    return true;
}

bool CreateReadWriteTexture(Device& device, void* data, u64 dataSize, u32 width,
                            u32 height, VkFormat format,
                            Sampler::FilterMode filterMode,
                            Sampler::WrapMode wrapMode, Texture& texture)
{
    FLY_ASSERT(width > 0);
    FLY_ASSERT(height > 0);
    FLY_ASSERT(dataSize == GetTexelSize(format) * width * height);

    u32 mipLevelCount = 1;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                              VK_IMAGE_USAGE_SAMPLED_BIT |
                              VK_IMAGE_USAGE_STORAGE_BIT;
    if (filterMode != Sampler::FilterMode::Nearest)
    {
        mipLevelCount = Log2(MAX(width, height)) + 1;
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    texture.image = CreateVulkanImage2D(
        device, texture.allocationInfo, texture.allocation, usage, format,
        VK_IMAGE_TILING_OPTIMAL, width, height, mipLevelCount);
    if (texture.image == VK_NULL_HANDLE)
    {
        return false;
    }

    texture.imageView =
        CreateVulkanImageView2D(device, texture.image, format, mipLevelCount,
                                VK_IMAGE_ASPECT_COLOR_BIT);
    if (!texture.imageView)
    {
        vmaDestroyImage(device.allocator, texture.image, texture.allocation);
        return false;
    }

    if (!CreateSampler(device, filterMode, wrapMode, mipLevelCount,
                       texture.sampler))
    {
        vkDestroyImageView(device.logicalDevice, texture.imageView,
                           GetVulkanAllocationCallbacks());
        vmaDestroyImage(device.allocator, texture.image, texture.allocation);
        return false;
    }

    VkImageLayout targetImageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = targetImageLayout;
    imageInfo.imageView = texture.imageView;
    imageInfo.sampler = texture.sampler.handle;

    VkWriteDescriptorSet descriptorWrites[2];
    for (u32 i = 0; i < 2; i++)
    {
        descriptorWrites[i] = {};
        descriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[i].dstSet = device.bindlessDescriptorSet;
        descriptorWrites[i].descriptorCount = 1;
        descriptorWrites[i].pImageInfo = &imageInfo;
    }
    descriptorWrites[0].dstBinding = FLY_TEXTURE_BINDING_INDEX;
    descriptorWrites[0].dstArrayElement = device.bindlessTextureHandleCount;
    descriptorWrites[0].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].dstBinding = FLY_STORAGE_TEXTURE_BINDING_INDEX;
    descriptorWrites[1].dstArrayElement =
        device.bindlessWriteTextureHandleCount;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    vkUpdateDescriptorSets(device.logicalDevice,
                           STACK_ARRAY_COUNT(descriptorWrites),
                           descriptorWrites, 0, nullptr);

    texture.bindlessHandle = device.bindlessTextureHandleCount++;
    texture.bindlessStorageHandle = device.bindlessWriteTextureHandleCount++;

    texture.width = width;
    texture.height = height;
    texture.format = format;
    texture.mipLevelCount = mipLevelCount;

    if (data)
    {
        RHI::Buffer stagingBuffer;
        if (!CreateBuffer(device, true, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, data,
                          dataSize, stagingBuffer))
        {
            DestroySampler(device, texture.sampler);
            vkDestroyImageView(device.logicalDevice, texture.imageView,
                               GetVulkanAllocationCallbacks());
            vmaDestroyImage(device.allocator, texture.image,
                            texture.allocation);
            return false;
        }

        BeginTransfer(device);
        RHI::CommandBuffer& cmd = TransferCommandBuffer(device);

        RHI::RecordTransitionImageLayout(cmd, texture.image,
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

        vkCmdCopyBufferToImage(cmd.handle, stagingBuffer.handle, texture.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &copyRegion);

        GenerateMipmaps(cmd, texture, targetImageLayout);
        EndTransfer(device);

        DestroyBuffer(device, stagingBuffer);
    }
    else
    {
        BeginTransfer(device);
        RHI::CommandBuffer& cmd = TransferCommandBuffer(device);
        RHI::RecordTransitionImageLayout(
            cmd, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, targetImageLayout);
        EndTransfer(device);
    }
    texture.imageLayout = targetImageLayout;

    FLY_DEBUG_LOG("Texture [%llu] created with size %f MB: bindless %u storage "
                  "bindless handle %u",
                  texture.image, texture.allocationInfo.size / 1024.0 / 1024.0,
                  texture.bindlessHandle, texture.bindlessStorageHandle);
    return true;
}

void DestroyTexture(Device& device, Texture& texture)
{
    DestroySampler(device, texture.sampler);
    vkDestroyImageView(device.logicalDevice, texture.imageView,
                       GetVulkanAllocationCallbacks());
    vmaDestroyImage(device.allocator, texture.image, texture.allocation);
    texture.image = VK_NULL_HANDLE;
    texture.imageView = VK_NULL_HANDLE;
    texture.format = VK_FORMAT_UNDEFINED;
    texture.width = 0;
    texture.height = 0;
    texture.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

bool ModifyTextureSampler(Device& device, Sampler::FilterMode filterMode,
                          Sampler::WrapMode wrapMode, u32 anisotropy,
                          Texture& texture)
{
    FLY_ASSERT(texture.image != VK_NULL_HANDLE);
    FLY_ASSERT(texture.sampler.handle != VK_NULL_HANDLE);

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

void CopyTextureToBuffer(Device& device, const Texture& texture, Buffer& buffer)
{
    FLY_ASSERT(texture.image != VK_NULL_HANDLE);
    FLY_ASSERT(buffer.handle != VK_NULL_HANDLE);

    BeginTransfer(device);
    CommandBuffer& cmd = TransferCommandBuffer(device);

    RHI::RecordTransitionImageLayout(cmd, texture.image, texture.imageLayout,
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    VkBufferImageCopy copyRegion{};
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = {texture.width, texture.height, 1};

    vkCmdCopyImageToBuffer(cmd.handle, texture.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer.handle,
                           1, &copyRegion);

    RHI::RecordTransitionImageLayout(cmd, texture.image,
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     texture.imageLayout);
    EndTransfer(device);
}

bool CreateCubemap(Device& device, void* data, u64 dataSize, u32 size,
                   VkFormat format, Sampler::FilterMode filterMode,
                   Cubemap& cubemap)
{
    FLY_ASSERT(size > 0);
    FLY_ASSERT(dataSize == GetTexelSize(format) * size * size * 6);

    VkImageCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.format = format;
    createInfo.extent = {size, size, 1};
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 6;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT |
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocCreateInfo.requiredFlags =
        VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vmaCreateImage(device.allocator, &createInfo, &allocCreateInfo,
                       &cubemap.image, &cubemap.allocation,
                       &cubemap.allocationInfo) != VK_SUCCESS)
    {
        return false;
    }

    VkImageViewCreateInfo viewCreateInfo{};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image = cubemap.image;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewCreateInfo.format = format;
    viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 6;
    if (vkCreateImageView(device.logicalDevice, &viewCreateInfo,
                          RHI::GetVulkanAllocationCallbacks(),
                          &cubemap.imageView) != VK_SUCCESS)
    {
        vmaDestroyImage(device.allocator, cubemap.image, cubemap.allocation);
        return false;
    }

    VkImageViewCreateInfo arrayViewCreateInfo{};
    arrayViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    arrayViewCreateInfo.image = cubemap.image;
    arrayViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    arrayViewCreateInfo.format = format;
    arrayViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    arrayViewCreateInfo.subresourceRange.baseMipLevel = 0;
    arrayViewCreateInfo.subresourceRange.levelCount = 1;
    arrayViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    arrayViewCreateInfo.subresourceRange.layerCount = 6;
    if (vkCreateImageView(device.logicalDevice, &arrayViewCreateInfo,
                          RHI::GetVulkanAllocationCallbacks(),
                          &cubemap.arrayImageView))
    {
        vkDestroyImageView(device.logicalDevice, cubemap.imageView,
                           GetVulkanAllocationCallbacks());
        vmaDestroyImage(device.allocator, cubemap.image, cubemap.allocation);
        return false;
    }

    if (!CreateSampler(device, filterMode, Sampler::WrapMode::Clamp, 1,
                       cubemap.sampler))
    {
        vkDestroyImageView(device.logicalDevice, cubemap.arrayImageView,
                           GetVulkanAllocationCallbacks());
        vkDestroyImageView(device.logicalDevice, cubemap.imageView,
                           GetVulkanAllocationCallbacks());
        vmaDestroyImage(device.allocator, cubemap.image, cubemap.allocation);
        return false;
    }

    VkImageLayout targetImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo imageInfos[2];
    imageInfos[0].imageLayout = targetImageLayout;
    imageInfos[0].imageView = cubemap.imageView;
    imageInfos[0].sampler = cubemap.sampler.handle;

    imageInfos[1].imageLayout = targetImageLayout;
    imageInfos[1].imageView = cubemap.arrayImageView;
    imageInfos[1].sampler = cubemap.sampler.handle;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = device.bindlessDescriptorSet;
    descriptorWrite.dstBinding = FLY_TEXTURE_BINDING_INDEX;
    descriptorWrite.dstArrayElement = device.bindlessTextureHandleCount;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 2;
    descriptorWrite.pImageInfo = imageInfos;

    vkUpdateDescriptorSets(device.logicalDevice, 1, &descriptorWrite, 0,
                           nullptr);
    cubemap.bindlessHandle = device.bindlessTextureHandleCount++;
    cubemap.bindlessArrayHandle = device.bindlessTextureHandleCount++;
    cubemap.size = size;
    cubemap.format = format;
    cubemap.mipLevelCount = 1;

    if (data)
    {
        RHI::Buffer stagingBuffer;
        if (!CreateBuffer(device, true, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, data,
                          dataSize, stagingBuffer))
        {
            DestroySampler(device, cubemap.sampler);
            vkDestroyImageView(device.logicalDevice, cubemap.arrayImageView,
                               GetVulkanAllocationCallbacks());
            vkDestroyImageView(device.logicalDevice, cubemap.imageView,
                               GetVulkanAllocationCallbacks());
            vmaDestroyImage(device.allocator, cubemap.image,
                            cubemap.allocation);
            return false;
        }

        BeginTransfer(device);
        RHI::CommandBuffer& cmd = TransferCommandBuffer(device);

        RHI::RecordTransitionImageLayout(cmd, cubemap.image,
                                         VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 6;
        copyRegion.imageExtent.width = size;
        copyRegion.imageExtent.height = size;
        copyRegion.imageExtent.depth = 1;

        vkCmdCopyBufferToImage(cmd.handle, stagingBuffer.handle, cubemap.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &copyRegion);

        GenerateMipmaps(cmd, cubemap, targetImageLayout);
        EndTransfer(device);

        DestroyBuffer(device, stagingBuffer);
    }
    else
    {
        BeginTransfer(device);
        RHI::CommandBuffer& cmd = TransferCommandBuffer(device);
        RHI::RecordTransitionImageLayout(
            cmd, cubemap.image, VK_IMAGE_LAYOUT_UNDEFINED, targetImageLayout);
        EndTransfer(device);
    }

    cubemap.imageLayout = targetImageLayout;

    FLY_DEBUG_LOG("Cubemap [%llu] created with size %f MB: bindless handle %u",
                  cubemap.image, cubemap.allocationInfo.size / 1024.0 / 1024.0,
                  cubemap.bindlessHandle);
    return true;
}

void DestroyCubemap(Device& device, Cubemap& cubemap)
{
    DestroySampler(device, cubemap.sampler);
    vkDestroyImageView(device.logicalDevice, cubemap.arrayImageView,
                       GetVulkanAllocationCallbacks());
    vkDestroyImageView(device.logicalDevice, cubemap.imageView,
                       GetVulkanAllocationCallbacks());
    vmaDestroyImage(device.allocator, cubemap.image, cubemap.allocation);
    cubemap.image = VK_NULL_HANDLE;
    cubemap.imageView = VK_NULL_HANDLE;
    cubemap.arrayImageView = VK_NULL_HANDLE;
    cubemap.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    cubemap.format = VK_FORMAT_UNDEFINED;
    cubemap.size = 0;
}

} // namespace RHI
} // namespace Fly
