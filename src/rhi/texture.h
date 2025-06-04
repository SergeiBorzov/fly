#ifndef FLY_RHI_IMAGE_H
#define FLY_RHI_IMAGE_H

#include <volk.h>

#include "vma.h"

namespace Fly
{
namespace RHI
{

struct Device;
struct Buffer;

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

struct SwapchainTexture
{
    VkImage handle = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    u32 width = 0;
    u32 height = 0;
};

struct DepthTexture
{
    VmaAllocationInfo allocationInfo;
    VkImage handle = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    VmaAllocation allocation;
    u32 width = 0;
    u32 height = 0;
};
bool CreateDepthTexture(Device& device, u32 width, u32 height, VkFormat format,
                        DepthTexture& depthTexture);
void DestroyDepthTexture(Device& device, DepthTexture& depthTexture);

struct Texture
{
    VmaAllocationInfo allocationInfo;
    Sampler sampler;
    VkImage handle = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VmaAllocation allocation;
    u32 width = 0;
    u32 height = 0;
    u32 mipLevelCount = 0;
    u32 bindlessHandle = FLY_MAX_U32;
};

bool CreateTexture(Device& device, u8* data, u32 width, u32 height,
                   u32 channelCount, VkFormat format,
                   Sampler::FilterMode filterMode, Sampler::WrapMode wrapMode,
                   Texture& texture);
bool ModifyTextureSampler(Device& device, Sampler::FilterMode filterMode,
                          Sampler::WrapMode wrapMode);
void DestroyTexture(Device& device, Texture& texture);
void CopyImageToBuffer(Device& device, const Texture& texture, Buffer& buffer);

u32 GetTexelSize(VkFormat format);

} // namespace RHI
} // namespace Fly

#endif /* FLY_RHI_IMAGE_H */
