#ifndef HLS_RHI_IMAGE_H
#define HLS_RHI_IMAGE_H

#include "context.h"

namespace Hls
{
struct Image
{
    u8* data = nullptr;
    u32 width = 0;
    u32 height = 0;
    u32 channelCount = 0;
};

struct Texture
{
    VmaAllocationInfo allocationInfo;
    VkImage handle = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VmaAllocation allocation;
    u32 width = 0;
    u32 height = 0;
};

bool CreateTexture(Device& device, u32 width, u32 height, VkFormat format,
                   Texture& texture);
void DestroyTexture(Device& device, Texture& texture);
bool TransferImageDataToTexture(Device& device, const Image& image,
                                Texture& texture);

} // namespace Hls

#endif /* HLS_RHI_IMAGE_H */
