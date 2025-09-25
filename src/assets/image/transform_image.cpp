#include "core/assert.h"
#include "core/memory.h"
#include <stdio.h>

#include "rhi/buffer.h"
#include "rhi/command_buffer.h"
#include "rhi/device.h"
#include "rhi/pipeline.h"
#include "rhi/texture.h"

#include "image.h"
#include "transform_image.h"

#define STBIR_DEFAULT_FILTER_UPSAMPLE STBIR_FILTER_CATMULLROM
#define STBIR_DEFAULT_FILTER_DOWNSAMPLE STBIR_FILTER_MITCHELL
#define STBIR_ASSERT(x) FLY_ASSERT(x)
#define STBIR_MALLOC(size, userData) (Fly::Alloc(size))
#define STBIR_FREE(ptr, userData) (Fly::Free(ptr))
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

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

    return stbir_resize_uint8_srgb(
        srcImage.data, srcImage.width, srcImage.height, 0, dstImage.data,
        dstImage.width, dstImage.height, 0,
        static_cast<stbir_pixel_layout>(dstImage.channelCount));
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

    return stbir_resize_uint8_linear(
        srcImage.data, srcImage.width, srcImage.height, 0, dstImage.data,
        dstImage.width, dstImage.height, 0,
        static_cast<stbir_pixel_layout>(dstImage.channelCount));
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
    RHI::Buffer& stagingBuffer = *(bufferInput->buffers[0]);
    RHI::CopyTextureToBuffer(cmd, cubemap, 0, stagingBuffer);
}

bool Eq2Cube(RHI::Device& device, RHI::GraphicsPipeline& eq2cubePipeline,
             const Image& srcImage, Image& dstImage, bool generateMips)
{
    FLY_ASSERT(srcImage.storageType == ImageStorageType::Byte);
    FLY_ASSERT(srcImage.channelCount >= 3);

    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    if (srcImage.channelCount == 3)
    {
        format = VK_FORMAT_R8G8B8_SRGB;
    }

    u32 size = srcImage.width / 4;

    u32 mipCount = generateMips ? 0 : 1;

    RHI::Texture texture2D;
    RHI::Texture cubemap;

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
                            nullptr, size, VK_FORMAT_R8G8B8A8_SRGB,
                            RHI::Sampler::FilterMode::Nearest, mipCount,
                            cubemap))
    {
        return false;
    }

    VkRenderingAttachmentInfo colorAttachment =
        RHI::ColorAttachmentInfo(cubemap.arrayImageView);
    VkRenderingInfo renderingInfo = RHI::RenderingInfo(
        {{0, 0}, {size, size}}, &colorAttachment, 1, nullptr, nullptr, 6, 0x3F);

    RHI::RecordBufferInput bufferInput;
    VkAccessFlagBits2 bufferAccessMask;
    RHI::RecordTextureInput textureInput;
    RHI::Texture* textures[2];
    RHI::ImageLayoutAccess imageLayoutsAccesses[2];
    textureInput.imageLayoutsAccesses = imageLayoutsAccesses;
    textureInput.textures = textures;

    printf("kalakakalk\n");
    RHI::BeginTransfer(device);
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

        RHI::ExecuteGraphics(device, TransferCommandBuffer(device),
                             renderingInfo, RecordConvertFromEquirectangular,
                             nullptr, &textureInput, &eq2cubePipeline);
        if (generateMips)
        {
            RHI::ExecuteTransfer(device, TransferCommandBuffer(device),
                                 RecordGenerateMipmaps, nullptr, &textureInput);
        }
    }
    u64 dstSize = size * size * srcImage.channelCount * 6;
    RHI::Buffer stagingBuffer;
    {

        if (!RHI::CreateBuffer(device, true, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               nullptr, dstSize, stagingBuffer))
        {
            return false;
        }

        bufferInput.bufferCount = 1;
        RHI::Buffer* pStagingBuffer = &stagingBuffer;
        bufferInput.buffers = &pStagingBuffer;
        bufferAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        bufferInput.bufferAccesses = &bufferAccessMask;

        textureInput.textureCount = 1;
        textures[0] = &cubemap;
        imageLayoutsAccesses[0].imageLayout =
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imageLayoutsAccesses[0].accessMask = VK_ACCESS_2_TRANSFER_READ_BIT;

        RHI::ExecuteTransfer(device, TransferCommandBuffer(device),
                             RecordReadbackCubemap, &bufferInput, &textureInput,
                             &dstImage);
    }
    RHI::EndTransfer(device);

    dstImage.mem = static_cast<u8*>(Fly::Alloc(dstSize));
    dstImage.data = dstImage.mem;
    dstImage.width = size;
    dstImage.height = size;
    dstImage.channelCount = 4;
    dstImage.mipCount = 1;
    dstImage.layerCount = 6;
    dstImage.storageType = ImageStorageType::Byte;
    memcpy(dstImage.mem, RHI::BufferMappedPtr(stagingBuffer), dstSize);

    RHI::DestroyBuffer(device, stagingBuffer);
    RHI::DestroyTexture(device, texture2D);
    RHI::DestroyTexture(device, cubemap);
    return true;
}

} // namespace Fly
