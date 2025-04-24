#ifndef HLS_RHI_IMAGE_H
#define HLS_RHI_IMAGE_H

#include <volk.h>

#include "vma.h"

namespace Hls
{
struct Device;
}

namespace Hls
{

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
    VkImage handle = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VmaAllocation allocation;
    u32 width = 0;
    u32 height = 0;
    u32 mipLevelCount = 0;
    u32 bindlessHandle = HLS_MAX_U32;
};

bool CreateTexture(Device& device, u32 width, u32 height, VkFormat format,
                   Texture& texture, bool generateMipMaps = true,
                   u32 maxAnisotropy = 8);
void DestroyTexture(Device& device, Texture& texture);

} // namespace Hls

#endif /* HLS_RHI_IMAGE_H */
