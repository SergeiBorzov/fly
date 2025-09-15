#include "core/assert.h"
#include "core/log.h"

#include "allocation_callbacks.h"
#include "buffer.h"
#include "command_buffer.h"
#include "device.h"
#include "texture.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

namespace Fly
{
namespace RHI
{

VkImageAspectFlags GetImageAspectMask(VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT:
        {
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
        {
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        default:
        {
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }
}

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
                            Fly::RHI::Texture& texture, u32 layerCount)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = texture.image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layerCount;
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
        blit.srcSubresource.layerCount = layerCount;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1,
                              mipHeight > 1 ? mipHeight / 2 : 1, 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = layerCount;

        vkCmdBlitImage(cmd.handle, texture.image,
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture.image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                       VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
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
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);
}

static VkImage
CreateVulkanImage(Fly::RHI::Device& device, VmaAllocationInfo& allocationInfo,
                  VmaAllocation& allocation, VkImageCreateFlags flags,
                  VkImageType imageType, VkImageUsageFlags usage,
                  VkFormat format, VkImageTiling tiling, u32 width, u32 height,
                  u32 depth, u32 layerCount, u32 mipLevelCount = 1)
{
    VkImageCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.flags = flags;
    createInfo.imageType = imageType;
    createInfo.format = format;
    createInfo.extent.width = width;
    createInfo.extent.height = height;
    createInfo.extent.depth = depth;
    createInfo.mipLevels = mipLevelCount;
    createInfo.arrayLayers = layerCount;
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

static VkImageView
CreateVulkanImageView(Fly::RHI::Device& device, VkImage image, VkFormat format,
                      u32 mipLevelCount, VkImageViewType imageViewType,
                      VkImageAspectFlags aspectMask, u32 layerCount)
{
    VkImageViewCreateInfo viewCreateInfo{};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image = image;
    viewCreateInfo.viewType = imageViewType;
    viewCreateInfo.format = format;
    viewCreateInfo.subresourceRange.aspectMask = GetImageAspectMask(format);
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = mipLevelCount;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = layerCount;

    VkImageView result;
    if (vkCreateImageView(device.logicalDevice, &viewCreateInfo,
                          Fly::RHI::GetVulkanAllocationCallbacks(),
                          &result) != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }

    return result;
}

static void CreateDescriptors(Fly::RHI::Device& device,
                              Fly::RHI::Texture& texture)
{
    u32 count = (texture.usage & VK_IMAGE_USAGE_STORAGE_BIT) ? 2 : 1;

    VkImageLayout layouts[2] = {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo imageInfo[2];

    VkWriteDescriptorSet descriptorWrites[2];
    for (u32 i = 0; i < count; i++)
    {
        imageInfo[i].imageLayout = layouts[i];
        imageInfo[i].imageView = texture.imageView;
        imageInfo[i].sampler = texture.sampler.handle;

        descriptorWrites[i] = {};
        descriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[i].dstSet = device.bindlessDescriptorSet;
        descriptorWrites[i].descriptorCount = 1;
        descriptorWrites[i].pImageInfo = &(imageInfo[i]);
    }

    descriptorWrites[0].dstBinding = FLY_TEXTURE_BINDING_INDEX;
    descriptorWrites[0].dstArrayElement = device.bindlessTextureHandleCount;
    descriptorWrites[0].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    descriptorWrites[1].dstBinding = FLY_STORAGE_TEXTURE_BINDING_INDEX;
    descriptorWrites[1].dstArrayElement =
        device.bindlessWriteTextureHandleCount;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

    vkUpdateDescriptorSets(device.logicalDevice, count, descriptorWrites, 0,
                           nullptr);

    texture.bindlessHandle = device.bindlessTextureHandleCount++;
    texture.bindlessStorageHandle = device.bindlessWriteTextureHandleCount++;
}

static bool InitializeWithData(Fly::RHI::Device& device, const void* data,
                               u64 dataSize, Fly::RHI::Texture& texture,
                               u32 layerCount)
{
    if (data)
    {
        Fly::RHI::Buffer stagingBuffer;
        if (!CreateBuffer(device, true, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, data,
                          dataSize, stagingBuffer))
        {
            DestroySampler(device, texture.sampler);
            vkDestroyImageView(device.logicalDevice, texture.imageView,
                               Fly::RHI::GetVulkanAllocationCallbacks());
            vmaDestroyImage(device.allocator, texture.image,
                            texture.allocation);
            return false;
        }

        BeginTransfer(device);
        Fly::RHI::CommandBuffer& cmd = TransferCommandBuffer(device);

        // Also sets all mip-map levels to transfer dst optimal
        Fly::RHI::RecordTransitionImageLayout(
            cmd, texture.image, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = layerCount;
        copyRegion.imageExtent.width = texture.width;
        copyRegion.imageExtent.height = texture.height;
        copyRegion.imageExtent.depth = 1;

        vkCmdCopyBufferToImage(cmd.handle, stagingBuffer.handle, texture.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &copyRegion);

        GenerateMipmaps(cmd, texture, layerCount);
        EndTransfer(device);
        DestroyBuffer(device, stagingBuffer);
    }
    return true;
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
    if (sampler.handle == VK_NULL_HANDLE)
    {
        return;
    }
    vkDestroySampler(device.logicalDevice, sampler.handle,
                     GetVulkanAllocationCallbacks());
    sampler.handle = VK_NULL_HANDLE;
}

bool CreateTexture2D(Device& device, VkImageUsageFlags usage, const void* data,
                     u32 width, u32 height, VkFormat format,
                     Sampler::FilterMode filterMode, Sampler::WrapMode wrapMode,
                     Texture& texture)
{
    FLY_ASSERT(width > 0);
    FLY_ASSERT(height > 0);

    u32 dataSize = GetTexelSize(format) * width * height;

    u32 mipLevelCount = 1;
    if ((usage & VK_IMAGE_USAGE_SAMPLED_BIT) &&
        filterMode != Sampler::FilterMode::Nearest)
    {
        mipLevelCount = Log2(MAX(width, height)) + 1;
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    texture.accessMask = VK_ACCESS_2_NONE;
    texture.pipelineStageMask = VK_PIPELINE_STAGE_2_NONE;
    texture.image = CreateVulkanImage(device, texture.allocationInfo,
                                      texture.allocation, 0, VK_IMAGE_TYPE_2D,
                                      usage, format, VK_IMAGE_TILING_OPTIMAL,
                                      width, height, 1, 1, mipLevelCount);
    if (texture.image == VK_NULL_HANDLE)
    {
        return false;
    }

    texture.imageView = CreateVulkanImageView(
        device, texture.image, format, mipLevelCount, VK_IMAGE_VIEW_TYPE_2D,
        GetImageAspectMask(format), 1);
    if (texture.imageView == VK_NULL_HANDLE)
    {
        vmaDestroyImage(device.allocator, texture.image, texture.allocation);
        return false;
    }

    texture.arrayImageView = CreateVulkanImageView(
        device, texture.image, format, mipLevelCount,
        VK_IMAGE_VIEW_TYPE_2D_ARRAY, GetImageAspectMask(format), 1);
    if (texture.arrayImageView == VK_NULL_HANDLE)
    {
        vkDestroyImageView(device.logicalDevice, texture.imageView,
                           GetVulkanAllocationCallbacks());
        vmaDestroyImage(device.allocator, texture.image, texture.allocation);
        return false;
    }

    texture.usage = usage;
    texture.sampler.handle = VK_NULL_HANDLE;
    texture.bindlessHandle = FLY_MAX_U32;
    texture.bindlessStorageHandle = FLY_MAX_U32;
    if (usage & VK_IMAGE_USAGE_SAMPLED_BIT)
    {
        if (!CreateSampler(device, filterMode, wrapMode, mipLevelCount,
                           texture.sampler))
        {
            vkDestroyImageView(device.logicalDevice, texture.imageView,
                               GetVulkanAllocationCallbacks());
            vmaDestroyImage(device.allocator, texture.image,
                            texture.allocation);
            return false;
        }

        CreateDescriptors(device, texture);
    }

    texture.width = width;
    texture.height = height;
    texture.format = format;
    texture.mipLevelCount = mipLevelCount;
    texture.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (!InitializeWithData(device, data, dataSize, texture, 1))
    {
        DestroySampler(device, texture.sampler);
        vkDestroyImageView(device.logicalDevice, texture.imageView,
                           GetVulkanAllocationCallbacks());
        vmaDestroyImage(device.allocator, texture.image, texture.allocation);
        return false;
    }

    FLY_DEBUG_LOG(
        "Texture2D [%llu] created with size %f MB: bindless %u storage "
        "bindless handle %u",
        texture.image, texture.allocationInfo.size / 1024.0 / 1024.0,
        texture.bindlessHandle, texture.bindlessStorageHandle);
    return true;
}

bool CreateCubemap(Device& device, VkImageUsageFlags usage, const void* data,
                   u32 size, VkFormat format, Sampler::FilterMode filterMode,
                   Texture& texture)
{
    FLY_ASSERT(size > 0);

    u32 dataSize = GetTexelSize(format) * size * size * 6;

    u32 mipLevelCount = 1;
    if ((usage & VK_IMAGE_USAGE_SAMPLED_BIT) &&
        filterMode != Sampler::FilterMode::Nearest)
    {
        mipLevelCount = Log2(size) + 1;
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    texture.accessMask = VK_ACCESS_2_NONE;
    texture.pipelineStageMask = VK_PIPELINE_STAGE_2_NONE;
    texture.image = CreateVulkanImage(
        device, texture.allocationInfo, texture.allocation,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, VK_IMAGE_TYPE_2D, usage, format,
        VK_IMAGE_TILING_OPTIMAL, size, size, 1, 6, mipLevelCount);
    if (texture.image == VK_NULL_HANDLE)
    {
        return false;
    }

    texture.imageView = CreateVulkanImageView(
        device, texture.image, format, mipLevelCount, VK_IMAGE_VIEW_TYPE_CUBE,
        GetImageAspectMask(format), 6);
    if (texture.imageView == VK_NULL_HANDLE)
    {
        vmaDestroyImage(device.allocator, texture.image, texture.allocation);
        return false;
    }

    texture.arrayImageView = CreateVulkanImageView(
        device, texture.image, format, mipLevelCount,
        VK_IMAGE_VIEW_TYPE_2D_ARRAY, GetImageAspectMask(format), 6);
    if (texture.arrayImageView == VK_NULL_HANDLE)
    {
        vkDestroyImageView(device.logicalDevice, texture.imageView,
                           GetVulkanAllocationCallbacks());
        vmaDestroyImage(device.allocator, texture.image, texture.allocation);
        return false;
    }

    texture.usage = usage;
    texture.sampler.handle = VK_NULL_HANDLE;
    texture.bindlessHandle = FLY_MAX_U32;
    texture.bindlessStorageHandle = FLY_MAX_U32;
    if (usage & VK_IMAGE_USAGE_SAMPLED_BIT)
    {
        if (!CreateSampler(device, filterMode, Sampler::WrapMode::Clamp,
                           mipLevelCount, texture.sampler))
        {
            vkDestroyImageView(device.logicalDevice, texture.imageView,
                               GetVulkanAllocationCallbacks());
            vmaDestroyImage(device.allocator, texture.image,
                            texture.allocation);
            return false;
        }

        CreateDescriptors(device, texture);
    }

    texture.width = size;
    texture.height = size;
    texture.format = format;
    texture.mipLevelCount = mipLevelCount;
    texture.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (!InitializeWithData(device, data, dataSize, texture, 6))
    {
        DestroySampler(device, texture.sampler);
        vkDestroyImageView(device.logicalDevice, texture.imageView,
                           GetVulkanAllocationCallbacks());
        vmaDestroyImage(device.allocator, texture.image, texture.allocation);
        return false;
    }

    FLY_DEBUG_LOG("Cubemap [%llu] created with size %f MB: bindless %u storage "
                  "bindless handle %u",
                  texture.image, texture.allocationInfo.size / 1024.0 / 1024.0,
                  texture.bindlessHandle, texture.bindlessStorageHandle);
    return true;
}

void DestroyTexture(Device& device, Texture& texture)
{
    DestroySampler(device, texture.sampler);
    vkDestroyImageView(device.logicalDevice, texture.arrayImageView,
                       GetVulkanAllocationCallbacks());
    vkDestroyImageView(device.logicalDevice, texture.imageView,
                       GetVulkanAllocationCallbacks());
    vmaDestroyImage(device.allocator, texture.image, texture.allocation);
    texture.image = VK_NULL_HANDLE;
    texture.imageView = VK_NULL_HANDLE;
    texture.arrayImageView = VK_NULL_HANDLE;
    texture.format = VK_FORMAT_UNDEFINED;
    texture.width = 0;
    texture.height = 0;
    texture.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    FLY_DEBUG_LOG("Texture [%llu] destroyed: bindless handle %u dealloc size "
                  "%f MB",
                  texture.image, texture.bindlessHandle,
                  texture.allocationInfo.size / 1024.0 / 1024.0);
}

} // namespace RHI
} // namespace Fly
