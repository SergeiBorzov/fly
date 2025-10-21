#ifndef FLY_RHI_TEXTURE_H
#define FLY_RHI_TEXTURE_H

#include "core/types.h"

#include "vma.h"
#include <volk.h>

namespace Fly
{
namespace RHI
{

struct Device;

u32 GetImageSize(u32 width, u32 height, VkFormat format);
VkImageAspectFlags GetImageAspectMask(VkFormat format);

struct Sampler
{
    enum class FilterMode
    {
        Nearest,
        Bilinear,
        Trilinear,
        Anisotropy2x,
        Anisotropy4x,
        Anisotropy8x,
        Anisotropy16x
    };

    enum class WrapMode
    {
        Repeat,
        Mirrored,
        Clamp
    };

    VkSampler handle = VK_NULL_HANDLE;
    FilterMode filterMode = FilterMode::Bilinear;
    WrapMode wrapMode = WrapMode::Repeat;
};
bool CreateSampler(Device& device, Sampler::FilterMode filterMode,
                   Sampler::WrapMode wrapMode, u32 mipLevelCount,
                   Sampler& sampler);
void DestroySampler(Device& device, Sampler& sampler);

struct Texture
{
    VmaAllocationInfo allocationInfo;
    Sampler sampler;
    VkAccessFlagBits2 accessMask;
    VkPipelineStageFlagBits2 pipelineStageMask;
    VmaAllocation allocation;
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkImageView arrayImageView = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageUsageFlags usage;
    u32 width = 0;
    u32 height = 0;
    u32 depth = 0;
    u32 layerCount = 0;
    u32 mipCount = 0;
    u32 bindlessHandle = FLY_MAX_U32;
    u32 bindlessStorageHandle = FLY_MAX_U32;
    u32 bindlessArrayHandle = FLY_MAX_U32;
};

bool CreateTexture2D(Device& device, VkImageUsageFlags usage, const void* data,
                     u32 width, u32 height, VkFormat format,
                     Sampler::FilterMode filterMode, Sampler::WrapMode wrapMode,
                     u32 mipCount, Texture& texture);
bool CreateTexture3D(Device& device, VkImageUsageFlags usage, const void* data,
                     u32 width, u32 height, u32 depth, VkFormat format,
                     Sampler::FilterMode filterMode, Sampler::WrapMode wrapMode,
                     u32 mipCount, Texture& texture);
void DestroyTexture(Device& device, Texture& texture);

bool CreateCubemap(Device& device, VkImageUsageFlags usage, const void* data,
                   u32 size, VkFormat format, Sampler::FilterMode filterMode,
                   u32 mipCount, Texture& texture);

} // namespace RHI
} // namespace Fly

#endif /* FLY_RHI_TEXTURE_H */
