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

u32 GetTexelSize(VkFormat format);

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

struct Texture2D
{
    VmaAllocationInfo allocationInfo;
    Sampler sampler;
    VkAccessFlagBits2 accessMask;
    VkPipelineStageFlagBits2 pipelineStageMask;
    VmaAllocation allocation;
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageUsageFlags usage;
    u32 width = 0;
    u32 height = 0;
    u32 mipLevelCount = 0;
    u32 bindlessHandle = FLY_MAX_U32;
    u32 bindlessStorageHandle = FLY_MAX_U32;
};

bool CreateTexture2D(Device& device, VkImageUsageFlags usage, const void* data,
                     u32 width, u32 height, VkFormat format,
                     Sampler::FilterMode filterMode, Sampler::WrapMode wrapMode,
                     Texture2D& texture);
void DestroyTexture2D(Device& device, Texture2D& texture);
bool ModifySampler(Device& device, Sampler::FilterMode filterMode,
                   Sampler::WrapMode wrapMode);

struct Cubemap
{
    VmaAllocationInfo allocationInfo;
    Sampler sampler;
    VmaAllocation allocation;
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkImageView arrayImageView = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    u32 size = 0;
    u32 mipLevelCount = 0;
    u32 bindlessHandle = FLY_MAX_U32;
    u32 bindlessArrayHandle = FLY_MAX_U32;
    u32 bindlessStorageHandle = FLY_MAX_U32;
};
bool CreateCubemap(Device& device, void* data, u64 dataSize, u32 size,
                   VkFormat format, Sampler::FilterMode filterMode,
                   Cubemap& cubemap);
void DestroyCubemap(Device& device, Cubemap& cubemap);

} // namespace RHI
} // namespace Fly

#endif /* FLY_RHI_TEXTURE_H */
