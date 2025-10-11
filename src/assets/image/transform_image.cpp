#include "core/assert.h"
#include "core/memory.h"
#include "core/thread_context.h"

#include "rhi/buffer.h"
#include "rhi/command_buffer.h"
#include "rhi/device.h"
#include "rhi/pipeline.h"
#include "rhi/texture.h"

#include "export_image.h"
#include "image.h"
#include "transform_image.h"

#define STBIR_DEFAULT_FILTER_UPSAMPLE STBIR_FILTER_CATMULLROM
#define STBIR_DEFAULT_FILTER_DOWNSAMPLE STBIR_FILTER_MITCHELL
#define STBIR_ASSERT(x) FLY_ASSERT(x)
#define STBIR_MALLOC(size, userData) (Fly::Alloc(size))
#define STBIR_FREE(ptr, userData) (Fly::Free(ptr))
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

static u32 Log2(u32 x)
{
    u32 result = 0;
    while (x >>= 1)
    {
        ++result;
    }
    return result;
}

namespace Fly
{

bool ResizeImageSRGB(const Image& srcImage, u32 width, u32 height,
                     Image& dstImage)
{
    FLY_ASSERT(srcImage.storageType == ImageStorageType::Byte);
    FLY_ASSERT(width > 0);
    FLY_ASSERT(height > 0);

    dstImage.channelCount = srcImage.channelCount;
    dstImage.mipCount = srcImage.mipCount;
    dstImage.width = width;
    dstImage.height = height;
    dstImage.mem = static_cast<u8*>(Fly::Alloc(
        sizeof(u8) * dstImage.width * dstImage.height * dstImage.channelCount));
    dstImage.data = dstImage.mem;

    u8* res = stbir_resize_uint8_srgb(
        srcImage.data, srcImage.width, srcImage.height, 0, dstImage.data,
        dstImage.width, dstImage.height, 0,
        static_cast<stbir_pixel_layout>(dstImage.channelCount));
    if (!res)
    {
        Fly::Free(dstImage.mem);
        dstImage.mem = nullptr;
        dstImage.data = nullptr;
    }
    return res;
}

bool ResizeImageLinear(const Image& srcImage, u32 width, u32 height,
                       Image& dstImage)
{
    FLY_ASSERT(srcImage.storageType == ImageStorageType::Byte);
    FLY_ASSERT(width > 0);
    FLY_ASSERT(height > 0);

    dstImage.channelCount = srcImage.channelCount;
    dstImage.mipCount = srcImage.mipCount;
    dstImage.width = width;
    dstImage.height = height;
    dstImage.mem = static_cast<u8*>(Fly::Alloc(
        sizeof(u8) * dstImage.width * dstImage.height * dstImage.channelCount));
    dstImage.data = dstImage.mem;

    u8* res = stbir_resize_uint8_linear(
        srcImage.data, srcImage.width, srcImage.height, 0, dstImage.data,
        dstImage.width, dstImage.height, 0,
        static_cast<stbir_pixel_layout>(dstImage.channelCount));
    if (!res)
    {
        Fly::Free(dstImage.mem);
        dstImage.mem = nullptr;
        dstImage.data = nullptr;
    }
    return res;
}

static void RecordConvertFromEquirectangular(
    RHI::CommandBuffer& cmd, const RHI::RecordBufferInput* bufferInput,
    const RHI::RecordTextureInput* textureInput, void* pUserData)
{
    RHI::GraphicsPipeline* graphicsPipeline =
        static_cast<RHI::GraphicsPipeline*>(pUserData);

    RHI::Texture& equirectangular = *(textureInput->textures[0]);
    RHI::Texture& cubemap = *(textureInput->textures[1]);
    RHI::BindGraphicsPipeline(cmd, *graphicsPipeline);

    u32 indices[] = {equirectangular.bindlessHandle};
    RHI::PushConstants(cmd, indices, sizeof(indices));

    RHI::SetViewport(cmd, 0, 0, cubemap.width, cubemap.width, 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, cubemap.width, cubemap.width);
    RHI::Draw(cmd, 6, 1, 0, 0);
}

static void RecordGenerateMipmaps(RHI::CommandBuffer& cmd,
                                  const RHI::RecordBufferInput* bufferInput,
                                  const RHI::RecordTextureInput* textureInput,
                                  void* pUserData)
{
    RHI::Texture& cubemap = *(textureInput->textures[1]);
    RHI::GenerateMipmaps(cmd, cubemap);
}

static void RecordReadbackCubemap(RHI::CommandBuffer& cmd,
                                  const RHI::RecordBufferInput* bufferInput,
                                  const RHI::RecordTextureInput* textureInput,
                                  void* pUserData)
{
    RHI::Texture& cubemap = *(textureInput->textures[0]);
    for (u32 i = 0; i < cubemap.mipCount; i++)
    {
        RHI::Buffer& stagingBuffer = *(bufferInput->buffers[i]);
        RHI::CopyTextureToBuffer(cmd, stagingBuffer, cubemap, i);
    }
}

bool GenerateMips(const Image& srcImage, Image& dstImage)
{
    FLY_ASSERT(srcImage.storageType == ImageStorageType::Byte);

    dstImage = srcImage;
    u32 mipCount = Log2(MAX(srcImage.width, srcImage.height)) + 1;
    u64 totalSize = 0;
    for (u32 i = 0; i < mipCount; i++)
    {
        u32 mipWidth = MAX(srcImage.width >> i, 1);
        u32 mipHeight = MAX(srcImage.height >> i, 1);
        totalSize +=
            mipWidth * mipHeight * srcImage.channelCount * srcImage.layerCount;
    }

    dstImage.mem = static_cast<u8*>(Fly::Alloc(totalSize));
    dstImage.data = dstImage.mem;

    u64 offset = srcImage.width * srcImage.height * srcImage.channelCount *
                 srcImage.layerCount;
    memcpy(dstImage.data, srcImage.data, offset);
    for (u32 i = 1; i < mipCount; i++)
    {
        u32 mipWidth = MAX(srcImage.width >> i, 1);
        u32 mipHeight = MAX(srcImage.height >> i, 1);

        Image resized;
        if (!ResizeImageSRGB(srcImage, mipWidth, mipHeight, resized))
        {
            return false;
        }
        u64 size =
            mipWidth * mipHeight * srcImage.channelCount * srcImage.layerCount;
        memcpy(dstImage.data + offset, resized.data, size);
        offset += size;
        FreeImage(resized);
    }
    return true;
}

VkFormat ImageVulkanFormat(const Image& image)
{
    switch (image.storageType)
    {
        case ImageStorageType::Byte:
        {
            if (image.channelCount == 1)
            {
                return VK_FORMAT_R8_SRGB;
            }
            else if (image.channelCount == 2)
            {
                return VK_FORMAT_R8G8_SRGB;
            }
            else if (image.channelCount == 3)
            {
                return VK_FORMAT_R8G8B8_SRGB;
            }
            else if (image.channelCount == 4)
            {
                return VK_FORMAT_R8G8B8A8_SRGB;
            }
            break;
        }
        case ImageStorageType::Half:
        {
            if (image.channelCount == 1)
            {
                return VK_FORMAT_R16_SFLOAT;
            }
            else if (image.channelCount == 2)
            {
                return VK_FORMAT_R16G16_SFLOAT;
            }
            else if (image.channelCount == 3)
            {
                return VK_FORMAT_R16G16B16_SFLOAT;
            }
            else if (image.channelCount == 4)
            {
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            }
            break;
        }
        case ImageStorageType::Float:
        {
            if (image.channelCount == 1)
            {
                return VK_FORMAT_R32_SFLOAT;
            }
            else if (image.channelCount == 2)
            {
                return VK_FORMAT_R32G32_SFLOAT;
            }
            else if (image.channelCount == 3)
            {
                return VK_FORMAT_R32G32B32_SFLOAT;
            }
            else if (image.channelCount == 4)
            {
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            }
            break;
        }
    }

    return VK_FORMAT_UNDEFINED;
}

bool Eq2Cube(RHI::Device& device, RHI::GraphicsPipeline& eq2cubePipeline,
             const Image& srcImage, Image& dstImage, bool generateMips)
{
    FLY_ASSERT(srcImage.storageType == ImageStorageType::Byte);
    FLY_ASSERT(srcImage.channelCount == 4);

    u32 side = srcImage.width / 4;
    u32 mipCount = generateMips ? 0 : 1;

    RHI::Texture texture2D;
    RHI::Texture cubemap;

    VkFormat format = ImageVulkanFormat(srcImage);

    if (!RHI::CreateTexture2D(device,
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                  VK_IMAGE_USAGE_SAMPLED_BIT,
                              srcImage.data, srcImage.width, srcImage.height,
                              format, RHI::Sampler::FilterMode::Nearest,
                              RHI::Sampler::WrapMode::Clamp, 1, texture2D))
    {
        return false;
    }

    if (!RHI::CreateCubemap(device,
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                            nullptr, side, format,
                            RHI::Sampler::FilterMode::Nearest, mipCount,
                            cubemap))
    {
        return false;
    }

    VkRenderingAttachmentInfo colorAttachment =
        RHI::ColorAttachmentInfo(cubemap.arrayImageView);
    VkRenderingInfo renderingInfo = RHI::RenderingInfo(
        {{0, 0}, {side, side}}, &colorAttachment, 1, nullptr, nullptr, 6, 0x3F);

    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    RHI::RecordBufferInput bufferInput;
    RHI::RecordTextureInput textureInput;
    RHI::Texture* textures[2];
    RHI::ImageLayoutAccess imageLayoutsAccesses[2];
    textureInput.imageLayoutsAccesses = imageLayoutsAccesses;
    textureInput.textures = textures;

    RHI::Buffer* stagingBuffers =
        FLY_PUSH_ARENA(arena, RHI::Buffer, cubemap.mipCount);
    VkAccessFlagBits2* bufferAccesses =
        FLY_PUSH_ARENA(arena, VkAccessFlagBits2, cubemap.mipCount);
    RHI::Buffer** buffers =
        FLY_PUSH_ARENA(arena, RHI::Buffer*, cubemap.mipCount);
    bufferInput.buffers = buffers;
    bufferInput.bufferAccesses = bufferAccesses;
    bufferInput.bufferCount = cubemap.mipCount;

    u64 totalSize = 0;
    for (u32 i = 0; i < cubemap.mipCount; i++)
    {
        u32 mipSide = MAX(side >> i, 1);
        u64 size = mipSide * mipSide * srcImage.channelCount *
                   cubemap.layerCount *
                   GetImageStorageTypeSize(srcImage.storageType);
        if (!RHI::CreateBuffer(device, true, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               nullptr, size, stagingBuffers[i]))
        {
            return false;
        }
        buffers[i] = &(stagingBuffers[i]);
        bufferAccesses[i] = VK_ACCESS_2_TRANSFER_READ_BIT;
        totalSize += size;
    }

    RHI::BeginOneTimeSubmit(device);
    RHI::CommandBuffer& cmd = OneTimeSubmitCommandBuffer(device);
    {

        textureInput.textureCount = 2;
        textures[0] = &texture2D;
        imageLayoutsAccesses[0].imageLayout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageLayoutsAccesses[0].accessMask = VK_ACCESS_2_SHADER_READ_BIT;

        textures[1] = &cubemap;
        imageLayoutsAccesses[1].imageLayout =
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        imageLayoutsAccesses[1].accessMask =
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

        RHI::ExecuteGraphics(cmd, renderingInfo,
                             RecordConvertFromEquirectangular, nullptr,
                             &textureInput, &eq2cubePipeline);
        if (generateMips)
        {
            RHI::ExecuteTransfer(cmd, RecordGenerateMipmaps, nullptr,
                                 &textureInput);
        }
    }

    {
        textureInput.textureCount = 1;
        textures[0] = &cubemap;
        imageLayoutsAccesses[0].imageLayout =
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imageLayoutsAccesses[0].accessMask = VK_ACCESS_2_TRANSFER_READ_BIT;

        RHI::ExecuteTransfer(cmd, RecordReadbackCubemap, &bufferInput,
                             &textureInput, nullptr);
    }
    RHI::EndOneTimeSubmit(device);

    u64 offset = 0;
    dstImage.mem = static_cast<u8*>(Fly::Alloc(totalSize));
    dstImage.data = dstImage.mem;
    dstImage.width = side;
    dstImage.height = side;
    dstImage.channelCount = srcImage.channelCount;
    dstImage.mipCount = cubemap.mipCount;
    dstImage.layerCount = cubemap.layerCount;
    dstImage.storageType = srcImage.storageType;
    for (u32 i = 0; i < cubemap.mipCount; i++)
    {
        u32 mipSide = MAX(side >> i, 1);
        u64 size = mipSide * mipSide * srcImage.channelCount *
                   cubemap.layerCount *
                   GetImageStorageTypeSize(srcImage.storageType);
        memcpy(dstImage.mem + offset, RHI::BufferMappedPtr(stagingBuffers[i]),
               size);
        offset += size;
    }

    for (u32 i = 0; i < cubemap.mipCount; i++)
    {
        RHI::DestroyBuffer(device, stagingBuffers[i]);
    }
    RHI::DestroyTexture(device, texture2D);
    RHI::DestroyTexture(device, cubemap);

    ArenaPopToMarker(arena, marker);
    return true;
}

} // namespace Fly
