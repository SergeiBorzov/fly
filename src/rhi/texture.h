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

struct Texture
{
    VmaAllocationInfo allocationInfo;
    Sampler sampler;
    VmaAllocation allocation;
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    u32 width = 0;
    u32 height = 0;
    u32 mipLevelCount = 0;
    u32 bindlessHandle = FLY_MAX_U32;
};
bool CreateTexture(Device& device, void* data, u64 dataSize, u32 width,
                   u32 height, VkFormat format, Sampler::FilterMode filterMode,
                   Sampler::WrapMode wrapMode, Texture& texture);
void DestroyTexture(Device& device, Texture& texture);
bool ModifyTextureSampler(Device& device, Sampler::FilterMode filterMode,
                          Sampler::WrapMode wrapMode);
void CopyTextureToBuffer(Device& device, const Texture& texture,
                         Buffer& buffer);

struct ReadWriteTexture
{
    VmaAllocationInfo allocationInfo;
    Sampler sampler;
    VmaAllocation allocation;
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    u32 width = 0;
    u32 height = 0;
    u32 mipLevelCount = 0;
    u32 bindlessReadHandle = FLY_MAX_U32;
    u32 bindlessWriteHandle = FLY_MAX_U32;
};

} // namespace RHI
} // namespace Fly

#endif /* FLY_RHI_TEXTURE_H */
