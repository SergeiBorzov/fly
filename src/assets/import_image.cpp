#include "rhi/buffer.h"
#include "rhi/device.h"

#include "import_image.h"

#include "core/assert.h"
#include "core/memory.h"
#define STBI_ASSERT(x) HLS_ASSERT(x)
#define STBI_MALLOC(size) (Hls::Alloc(size))
#define STBI_REALLOC(p, newSize) (Hls::Realloc(p, newSize))
#define STBI_FREE(p) (Hls::Free(p))
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Hls
{

bool LoadImageFromFile(const char* filename, Image& image)
{
    int x = 0, y = 0, n = 0;
    int desiredChannelCount = 4;
    unsigned char* data = stbi_load(filename, &x, &y, &n, desiredChannelCount);
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

    RecordTransitionImageLayout(cmd, texture.handle,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    EndTransfer(device);

    UnmapBuffer(device, transferBuffer);
    DestroyBuffer(device, transferBuffer);

    return true;
}

} // namespace Hls
