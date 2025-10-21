#include "core/assert.h"
#include "core/log.h"
#include "core/thread_context.h"

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

u32 GetImageSize(u32 width, u32 height, u32 depth, VkFormat format)
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
            return width * height * depth;
        }
        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC5_UNORM_BLOCK:
        case VK_FORMAT_BC5_SNORM_BLOCK:
        {
            return (((width + 3) / 4) * ((height + 3) / 4) * 16) * depth;
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
            return 2 * width * height * depth;
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
            return 3 * width * height * depth;
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
            return 4 * width * height * depth;
        }

        case VK_FORMAT_D32_SFLOAT_S8_UINT:
        {
            return 5 * width * height * depth;
        }

        case VK_FORMAT_R16G16B16_UNORM:
        case VK_FORMAT_R16G16B16_SNORM:
        case VK_FORMAT_R16G16B16_UINT:
        case VK_FORMAT_R16G16B16_SINT:
        case VK_FORMAT_R16G16B16_SFLOAT:
        {
            return 6 * width * height * depth;
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
            return 8 * width * height * depth;
        }

        case VK_FORMAT_R32G32B32_UINT:
        case VK_FORMAT_R32G32B32_SINT:
        case VK_FORMAT_R32G32B32_SFLOAT:
        {
            return 12 * width * height * depth;
        }

        case VK_FORMAT_R32G32B32A32_UINT:
        case VK_FORMAT_R32G32B32A32_SINT:
        case VK_FORMAT_R32G32B32A32_SFLOAT:
        {
            return 16 * width * height * depth;
        }

        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        case VK_FORMAT_BC4_UNORM_BLOCK:
        case VK_FORMAT_BC4_SNORM_BLOCK:
        {
            return ((width + 3) / 4) * ((height + 3) / 4) * 8 * depth;
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

static VkImage
CreateVulkanImage(Fly::RHI::Device& device, VmaAllocationInfo& allocationInfo,
                  VmaAllocation& allocation, VkImageCreateFlags flags,
                  VkImageType imageType, VkImageUsageFlags usage,
                  VkFormat format, VkImageTiling tiling, u32 width, u32 height,
                  u32 depth, u32 layerCount, u32 mipCount = 1)
{
    VkImageCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.flags = flags;
    createInfo.imageType = imageType;
    createInfo.format = format;
    createInfo.extent.width = width;
    createInfo.extent.height = height;
    createInfo.extent.depth = depth;
    createInfo.mipLevels = mipCount;
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
                      u32 mipCount, VkImageViewType imageViewType,
                      VkImageAspectFlags aspectMask, u32 layerCount)
{
    VkImageViewCreateInfo viewCreateInfo{};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image = image;
    viewCreateInfo.viewType = imageViewType;
    viewCreateInfo.format = format;
    viewCreateInfo.subresourceRange.aspectMask = GetImageAspectMask(format);
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = mipCount;
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

static bool CopyDataToTexture(Fly::RHI::Device& device, const u8* data,
                              bool generateMips, Fly::RHI::Texture& texture)
{
    if (data)
    {
        u64 dataSize = GetImageSize(texture.width, texture.height,
                                    texture.depth, texture.format) *
                       texture.layerCount;
        Arena& arena = GetScratchArena();
        ArenaMarker marker = ArenaGetMarker(arena);

        Fly::RHI::Buffer stagingBuffer;
        if (!CreateBuffer(device, true, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, data,
                          dataSize, stagingBuffer))
        {
            return false;
        }

        BeginOneTimeSubmit(device);
        Fly::RHI::CommandBuffer& cmd = OneTimeSubmitCommandBuffer(device);
        RHI::ChangeTextureAccessLayout(cmd, texture,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       VK_ACCESS_2_TRANSFER_WRITE_BIT);
        RHI::CopyBufferToTexture(cmd, texture, stagingBuffer, 0);

        RHI::Buffer* mipStagingBuffers = nullptr;
        if (generateMips)
        {
            RHI::GenerateMipmaps(cmd, texture);
        }
        else if (texture.mipCount > 1)
        {
            u64 offset = dataSize;
            mipStagingBuffers =
                FLY_PUSH_ARENA(arena, RHI::Buffer, texture.mipCount - 1);
            for (u32 i = 1; i < texture.mipCount; i++)
            {
                u32 mipWidth = MAX(texture.width >> i, 1);
                u32 mipHeight = MAX(texture.height >> i, 1);
                u32 mipDepth = MAX(texture.depth >> i, 1);
                u64 mipSize = GetImageSize(mipWidth, mipHeight, mipDepth,
                                           texture.format) *
                              texture.layerCount;
                const u8* mipData = data + offset;
                if (!CreateBuffer(device, true,
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT, mipData,
                                  mipSize, mipStagingBuffers[i - 1]))
                {
                    return false;
                }
                offset += mipSize;
            }

            for (u32 i = 0; i < texture.mipCount - 1; i++)
            {

                RHI::CopyBufferToTexture(cmd, texture, mipStagingBuffers[i],
                                         i + 1);
            }
        }
        RHI::ChangeTextureAccessLayout(cmd, texture,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                       VK_ACCESS_2_SHADER_READ_BIT);
        EndOneTimeSubmit(device);
        DestroyBuffer(device, stagingBuffer);

        if (mipStagingBuffers)
        {
            for (u32 i = 0; i < texture.mipCount - 1; i++)
            {
                DestroyBuffer(device, mipStagingBuffers[i]);
            }
        }
        ArenaPopToMarker(arena, marker);
    }
    return true;
}

bool CreateSampler(Device& device, Sampler::FilterMode filterMode,
                   Sampler::WrapMode wrapMode, u32 mipCount, Sampler& sampler)
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
    samplerCreateInfo.maxLod = static_cast<f32>(mipCount);

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
                     u32 mipCount, Texture& texture)
{
    FLY_ASSERT(width > 0);
    FLY_ASSERT(height > 0);

    bool generateMips = mipCount == 0;
    if (generateMips)
    {
        mipCount = Log2(MAX(width, height)) + 1;
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    texture.depth = 1;
    texture.layerCount = 1;
    texture.accessMask = VK_ACCESS_2_NONE;
    texture.pipelineStageMask = VK_PIPELINE_STAGE_2_NONE;
    texture.image = CreateVulkanImage(
        device, texture.allocationInfo, texture.allocation, 0, VK_IMAGE_TYPE_2D,
        usage, format, VK_IMAGE_TILING_OPTIMAL, width, height, texture.depth,
        texture.layerCount, mipCount);
    if (texture.image == VK_NULL_HANDLE)
    {
        return false;
    }

    texture.imageView = CreateVulkanImageView(
        device, texture.image, format, mipCount, VK_IMAGE_VIEW_TYPE_2D,
        GetImageAspectMask(format), texture.layerCount);
    if (texture.imageView == VK_NULL_HANDLE)
    {
        vmaDestroyImage(device.allocator, texture.image, texture.allocation);
        return false;
    }

    texture.arrayImageView = VK_NULL_HANDLE;

    texture.usage = usage;
    texture.sampler.handle = VK_NULL_HANDLE;
    texture.bindlessHandle = FLY_MAX_U32;
    texture.bindlessStorageHandle = FLY_MAX_U32;
    if (usage & VK_IMAGE_USAGE_SAMPLED_BIT)
    {
        if (!CreateSampler(device, filterMode, wrapMode, mipCount,
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
    texture.mipCount = mipCount;
    texture.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (!CopyDataToTexture(device, static_cast<const u8*>(data), generateMips,
                           texture))
    {
        DestroySampler(device, texture.sampler);
        vkDestroyImageView(device.logicalDevice, texture.imageView,
                           GetVulkanAllocationCallbacks());
        vmaDestroyImage(device.allocator, texture.image, texture.allocation);
        return false;
    }

    FLY_DEBUG_LOG(
        "Texture2D [%llu] created with size %f MB: bindless handle %u storage "
        "bindless handle %u",
        texture.image, texture.allocationInfo.size / 1024.0 / 1024.0,
        texture.bindlessHandle, texture.bindlessStorageHandle);
    return true;
}

bool CreateCubemap(Device& device, VkImageUsageFlags usage, const void* data,
                   u32 size, VkFormat format, Sampler::FilterMode filterMode,
                   u32 mipCount, Texture& texture)
{
    FLY_ASSERT(size > 0);

    bool generateMips = mipCount == 0;
    if (generateMips)
    {
        mipCount = Log2(size) + 1;
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    texture.depth = 1;
    texture.layerCount = 6;
    texture.accessMask = VK_ACCESS_2_NONE;
    texture.pipelineStageMask = VK_PIPELINE_STAGE_2_NONE;
    texture.image = CreateVulkanImage(
        device, texture.allocationInfo, texture.allocation,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, VK_IMAGE_TYPE_2D, usage, format,
        VK_IMAGE_TILING_OPTIMAL, size, size, 1, 6, mipCount);
    if (texture.image == VK_NULL_HANDLE)
    {
        return false;
    }

    texture.imageView = CreateVulkanImageView(device, texture.image, format,
                                              mipCount, VK_IMAGE_VIEW_TYPE_CUBE,
                                              GetImageAspectMask(format), 6);
    if (texture.imageView == VK_NULL_HANDLE)
    {
        vmaDestroyImage(device.allocator, texture.image, texture.allocation);
        return false;
    }

    texture.arrayImageView = CreateVulkanImageView(
        device, texture.image, format, mipCount, VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        GetImageAspectMask(format), 6);
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
                           mipCount, texture.sampler))
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
    texture.mipCount = mipCount;
    texture.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (!CopyDataToTexture(device, static_cast<const u8*>(data), generateMips,
                           texture))
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

bool CreateTexture3D(Device& device, VkImageUsageFlags usage, const void* data,
                     u32 width, u32 height, u32 depth, VkFormat format,
                     Sampler::FilterMode filterMode, Sampler::WrapMode wrapMode,
                     u32 mipCount, Texture& texture)
{
    FLY_ASSERT(width > 0);
    FLY_ASSERT(height > 0);
    FLY_ASSERT(depth > 0);

    bool generateMips = mipCount == 0;
    if (generateMips)
    {
        mipCount = Log2(MAX(width, height)) + 1;
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    texture.arrayImageView = VK_NULL_HANDLE;
    texture.sampler.handle = VK_NULL_HANDLE;
    texture.bindlessHandle = FLY_MAX_U32;
    texture.bindlessStorageHandle = FLY_MAX_U32;
    texture.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    texture.format = format;
    texture.layerCount = 1;
    texture.width = width;
    texture.height = height;
    texture.depth = depth;
    texture.accessMask = VK_ACCESS_2_NONE;
    texture.pipelineStageMask = VK_PIPELINE_STAGE_2_NONE;
    texture.mipCount = mipCount;
    texture.usage = usage;

    texture.image = CreateVulkanImage(
        device, texture.allocationInfo, texture.allocation, 0, VK_IMAGE_TYPE_3D,
        usage, format, VK_IMAGE_TILING_OPTIMAL, width, height, depth,
        texture.layerCount, mipCount);
    if (texture.image == VK_NULL_HANDLE)
    {
        return false;
    }

    texture.imageView = CreateVulkanImageView(
        device, texture.image, format, mipCount, VK_IMAGE_VIEW_TYPE_3D,
        GetImageAspectMask(format), texture.layerCount);
    if (texture.imageView == VK_NULL_HANDLE)
    {
        vmaDestroyImage(device.allocator, texture.image, texture.allocation);
        return false;
    }

    if (usage & VK_IMAGE_USAGE_SAMPLED_BIT)
    {
        if (!CreateSampler(device, filterMode, wrapMode, mipCount,
                           texture.sampler))
        {
            vkDestroyImageView(device.logicalDevice, texture.imageView,
                               GetVulkanAllocationCallbacks());
            vmaDestroyImage(device.allocator, texture.image,
                            texture.allocation);
        }
        CreateDescriptors(device, texture);
    }

    if (!CopyDataToTexture(device, static_cast<const u8*>(data), generateMips,
                           texture))
    {
        DestroySampler(device, texture.sampler);
        vkDestroyImageView(device.logicalDevice, texture.imageView,
                           GetVulkanAllocationCallbacks());
        vmaDestroyImage(device.allocator, texture.image, texture.allocation);
        return false;
    }

    FLY_DEBUG_LOG(
        "Texture3D [%llu] created with size %f MB: bindless handle %u storage "
        "bindless handle %u",
        texture.image, texture.allocationInfo.size / 1024.0 / 1024.0,
        texture.bindlessHandle, texture.bindlessStorageHandle);
    return true;
}

void DestroyTexture(Device& device, Texture& texture)
{
    DestroySampler(device, texture.sampler);
    if (texture.arrayImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device.logicalDevice, texture.arrayImageView,
                           GetVulkanAllocationCallbacks());
    }
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
