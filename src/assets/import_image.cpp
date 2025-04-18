#include "rhi/buffer.h"
#include "rhi/device.h"

#include "import_image.h"

#include "core/assert.h"
#include "core/filesystem.h"
#include "core/memory.h"

#define STBI_ASSERT(x) HLS_ASSERT(x)
#define STBI_MALLOC(size) (Hls::Alloc(size))
#define STBI_REALLOC(p, newSize) (Hls::Realloc(p, newSize))
#define STBI_FREE(p) (Hls::Free(p))
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Hls
{

static void GenerateMipmaps(CommandBuffer& cmd, Texture& texture)
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

bool ImportImageFromFile(const char* path, Image& image)
{
    int x = 0, y = 0, n = 0;
    int desiredChannelCount = 4;
    unsigned char* data = stbi_load(path, &x, &y, &n, desiredChannelCount);
    if (!data)
    {
        return nullptr;
    }

    image.data = data;
    image.width = static_cast<u32>(x);
    image.height = static_cast<u32>(y);
    image.channelCount = static_cast<u32>(desiredChannelCount);

    return data;
}

bool ImportImageFromFile(const Path& path, Image& image)
{
    return ImportImageFromFile(path.ToCStr(), image);
}

void FreeImage(Image& image)
{
    HLS_ASSERT(image.data);
    stbi_image_free(image.data);
}

bool TransferImageDataToTexture(Device& device, const Image& image,
                                Texture& texture)
{
    Buffer transferBuffer;

    u64 allocSize =
        sizeof(u8) * image.width * image.height * image.channelCount;
    if (!CreateBuffer(device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                      allocSize, transferBuffer))
    {
        return false;
    }

    if (!MapBuffer(device, transferBuffer))
    {
        return false;
    }
    memcpy(transferBuffer.mappedPtr, image.data, allocSize);

    BeginTransfer(device);
    CommandBuffer& cmd = TransferCommandBuffer(device);

    RecordTransitionImageLayout(cmd, texture.handle, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent.width = image.width;
    copyRegion.imageExtent.height = image.height;
    copyRegion.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(cmd.handle, transferBuffer.handle, texture.handle,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copyRegion);

    GenerateMipmaps(cmd, texture);

    EndTransfer(device);

    UnmapBuffer(device, transferBuffer);
    DestroyBuffer(device, transferBuffer);

    return true;
}

} // namespace Hls
