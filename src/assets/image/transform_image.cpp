#include <string.h>

#include "core/assert.h"
#include "core/memory.h"
#include "core/thread_context.h"

#include "rhi/buffer.h"
#include "rhi/command_buffer.h"
#include "rhi/device.h"
#include "rhi/pipeline.h"
#include "rhi/texture.h"

#include "export_image.h"
#include "transform_image.h"

#define STBIR_DEFAULT_FILTER_UPSAMPLE STBIR_FILTER_CATMULLROM
#define STBIR_DEFAULT_FILTER_DOWNSAMPLE STBIR_FILTER_MITCHELL
#define STBIR_ASSERT(x) FLY_ASSERT(x)
#define STBIR_MALLOC(size, userData) (Fly::Alloc(size))
#define STBIR_FREE(ptr, userData) (Fly::Free(ptr))
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

using BlockCompressionFunc = void (*)(unsigned char* dst,
                                      const unsigned char* src);

static u32 Log2(u32 x)
{
    u32 result = 0;
    while (x >>= 1)
    {
        ++result;
    }
    return result;
}

static void CompressBlockBC1(unsigned char* dst, const unsigned char* src)
{
    stb_compress_dxt_block(dst, src, 0, STB_DXT_NORMAL);
}

static void CompressBlockBC3(unsigned char* dst, const unsigned char* src)
{
    stb_compress_dxt_block(dst, src, 1, STB_DXT_NORMAL);
}

static void CopyImageBlock(u8* block, const u8* data, u32 width, u32 height,
                           u32 x, u32 y, u32 elemSize)
{
    for (u32 i = 0; i < 4; i++)
    {
        for (u32 j = 0; j < 4; j++)
        {
            u32 kh = MIN(y * 4 + i, height - 1);
            u32 kw = MIN(x * 4 + j, width - 1);

            const u8* srcPtr = data + (width * kh + kw) * elemSize;
            u8* dstPtr = block + (4 * i + j) * elemSize;

            memcpy(dstPtr, srcPtr, elemSize);
        }
    }
}

namespace Fly
{

static void CompressImageLayer(u8* dst, const u8* src, u32 srcWidth,
                               u32 srcHeight, ImageStorageType codec)
{
    FLY_ASSERT(dst);
    FLY_ASSERT(src);
    FLY_ASSERT(srcWidth);
    FLY_ASSERT(srcHeight);

    static BlockCompressionFunc compressionFuncs[4] = {
        CompressBlockBC1,
        CompressBlockBC3,
        stb_compress_bc4_block,
        stb_compress_bc5_block,
    };

    static u8 channelCounts[4] = {4, 4, 1, 2};
    static u8 blockSizes[4] = {8, 16, 8, 16};

    BlockCompressionFunc compressionFunc =
        compressionFuncs[static_cast<u8>(codec)];
    const u32 elemSize = channelCounts[static_cast<u8>(codec)];
    const u32 blockSize = blockSizes[static_cast<u8>(codec)];

    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);
    u8* block = FLY_PUSH_ARENA(arena, u8, 16 * elemSize);

    const u32 blockWidth = (srcWidth + 3) / 4;
    const u32 blockHeight = (srcHeight + 3) / 4;
    for (u32 i = 0; i < blockHeight; i++)
    {
        for (u32 j = 0; j < blockWidth; j++)
        {
            CopyImageBlock(block, src, srcWidth, srcHeight, j, i, elemSize);
            compressionFunc(dst + (i * blockWidth + j) * blockSize, block);
        }
    }
    ArenaPopToMarker(arena, marker);
}

static VkFormat ImageVulkanFormat(ImageStorageType storageType, u8 channelCount)
{
    switch (storageType)
    {
        case ImageStorageType::Byte:
        {
            if (channelCount == 1)
            {
                return VK_FORMAT_R8_SRGB;
            }
            else if (channelCount == 2)
            {
                return VK_FORMAT_R8G8_SRGB;
            }
            else if (channelCount == 3)
            {
                return VK_FORMAT_R8G8B8_SRGB;
            }
            else if (channelCount == 4)
            {
                return VK_FORMAT_R8G8B8A8_SRGB;
            }
            break;
        }
        case ImageStorageType::Half:
        {
            if (channelCount == 1)
            {
                return VK_FORMAT_R16_SFLOAT;
            }
            else if (channelCount == 2)
            {
                return VK_FORMAT_R16G16_SFLOAT;
            }
            else if (channelCount == 3)
            {
                return VK_FORMAT_R16G16B16_SFLOAT;
            }
            else if (channelCount == 4)
            {
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            }
            break;
        }
        case ImageStorageType::Float:
        {
            if (channelCount == 1)
            {
                return VK_FORMAT_R32_SFLOAT;
            }
            else if (channelCount == 2)
            {
                return VK_FORMAT_R32G32_SFLOAT;
            }
            else if (channelCount == 3)
            {
                return VK_FORMAT_R32G32B32_SFLOAT;
            }
            else if (channelCount == 4)
            {
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            }
            break;
        }
        default:
        {
            break;
        }
    }

    return VK_FORMAT_UNDEFINED;
}

static void RecordConvertFromEquirectangular(
    RHI::CommandBuffer& cmd, const RHI::RecordBufferInput* bufferInput,
    u32 bufferInputCount, const RHI::RecordTextureInput* textureInput,
    u32 textureInputCount, void* pUserData)
{
    RHI::GraphicsPipeline* graphicsPipeline =
        static_cast<RHI::GraphicsPipeline*>(pUserData);

    RHI::Texture& equirectangular = *(textureInput[0].pTexture);
    RHI::Texture& cubemap = *(textureInput[1].pTexture);
    RHI::BindGraphicsPipeline(cmd, *graphicsPipeline);

    u32 indices[] = {equirectangular.bindlessHandle};
    RHI::PushConstants(cmd, indices, sizeof(indices));

    RHI::SetViewport(cmd, 0, 0, cubemap.width, cubemap.width, 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, cubemap.width, cubemap.width);
    RHI::Draw(cmd, 6, 1, 0, 0);
}

static void RecordReadbackCubemap(RHI::CommandBuffer& cmd,
                                  const RHI::RecordBufferInput* bufferInput,
                                  u32 bufferInputCount,
                                  const RHI::RecordTextureInput* textureInput,
                                  u32 textureInputCount, void* pUserData)
{
    RHI::Texture& cubemap = *(textureInput[0].pTexture);
    for (u32 i = 0; i < cubemap.mipCount; i++)
    {
        RHI::Buffer& stagingBuffer = *(bufferInput[i].pBuffer);
        RHI::CopyTextureToBuffer(cmd, stagingBuffer, cubemap, i);
    }
}

bool ResizeImageSRGB(u32 width, u32 height, Image& image)
{
    FLY_ASSERT(width);
    FLY_ASSERT(height);
    FLY_ASSERT(image.data);
    FLY_ASSERT(image.width);
    FLY_ASSERT(image.height);
    FLY_ASSERT(image.channelCount);
    FLY_ASSERT(image.layerCount);
    FLY_ASSERT(image.mipCount == 1);
    FLY_ASSERT(image.storageType == ImageStorageType::Byte);

    u64 dataSize =
        GetImageSize(width, height, image.channelCount, image.layerCount,
                     image.mipCount, image.storageType);
    u8* data = static_cast<u8*>(Alloc(dataSize));

    u64 srcLayerSize = image.width * image.height * image.channelCount;
    u64 dstLayerSize = width * height * image.channelCount;
    for (u8 i = 0; i < image.layerCount; i++)
    {
        u8* res = stbir_resize_uint8_srgb(
            image.data + srcLayerSize * i, image.width, image.height, 0,
            data + dstLayerSize * i, width, height, 0,
            static_cast<stbir_pixel_layout>(image.channelCount));
        if (!res)
        {
            Free(data);
            return false;
        }
    }

    Free(image.data);
    image.data = data;
    image.width = width;
    image.height = height;
    return true;
}

bool ResizeImageLinear(u32 width, u32 height, Image& image)
{
    FLY_ASSERT(width);
    FLY_ASSERT(height);
    FLY_ASSERT(image.data);
    FLY_ASSERT(image.width);
    FLY_ASSERT(image.height);
    FLY_ASSERT(image.channelCount);
    FLY_ASSERT(image.layerCount);
    FLY_ASSERT(image.mipCount == 1);
    FLY_ASSERT(image.storageType == ImageStorageType::Byte);

    u64 dataSize =
        GetImageSize(width, height, image.channelCount, image.layerCount,
                     image.mipCount, image.storageType);
    u8* data = static_cast<u8*>(Alloc(dataSize));

    u64 srcLayerSize = image.width * image.height * image.channelCount;
    u64 dstLayerSize = width * height * image.channelCount;
    for (u8 i = 0; i < image.layerCount; i++)
    {
        u8* res = stbir_resize_uint8_linear(
            image.data + srcLayerSize * i, image.width, image.height, 0,
            data + dstLayerSize * i, width, height, 0,
            static_cast<stbir_pixel_layout>(image.channelCount));
        if (!res)
        {
            Free(data);
            return false;
        }
    }

    Free(image.data);
    image.data = data;
    image.width = width;
    image.height = height;
    return true;
}

void CopyImage(const Image& srcImage, Image& dstImage)
{
    FLY_ASSERT(srcImage.data);
    FLY_ASSERT(srcImage.width);
    FLY_ASSERT(srcImage.height);
    FLY_ASSERT(srcImage.channelCount);
    FLY_ASSERT(srcImage.layerCount);
    FLY_ASSERT(srcImage.mipCount);

    if (dstImage.data)
    {
        Free(dstImage.data);
    }

    dstImage.width = srcImage.width;
    dstImage.height = srcImage.height;
    dstImage.channelCount = srcImage.channelCount;
    dstImage.layerCount = srcImage.layerCount;
    dstImage.mipCount = srcImage.mipCount;
    dstImage.storageType = srcImage.storageType;

    u64 srcImageSize = GetImageSize(srcImage);
    dstImage.data = static_cast<u8*>(Alloc(srcImageSize));
    memcpy(dstImage.data, srcImage.data, srcImageSize);
}

bool GenerateMips(Image& image, bool linearResize)
{
    FLY_ASSERT(image.data);
    FLY_ASSERT(image.width);
    FLY_ASSERT(image.height);
    FLY_ASSERT(image.channelCount);
    FLY_ASSERT(image.layerCount);
    FLY_ASSERT(image.mipCount);
    FLY_ASSERT(image.storageType == ImageStorageType::Byte);

    u32 mipCount = Log2(MAX(image.width, image.height)) + 1;
    u64 totalSize = 0;
    for (u32 i = 0; i < mipCount; i++)
    {
        u32 mipWidth = MAX(image.width >> i, 1);
        u32 mipHeight = MAX(image.height >> i, 1);
        totalSize +=
            mipWidth * mipHeight * image.channelCount * image.layerCount;
    }

    u8* data = static_cast<u8*>(Alloc(totalSize));

    u64 offset =
        image.width * image.height * image.channelCount * image.layerCount;
    memcpy(data, image.data, offset);

    for (u32 i = 1; i < mipCount; i++)
    {
        u32 mipWidth = MAX(image.width >> i, 1);
        u32 mipHeight = MAX(image.height >> i, 1);

        Image resized{};
        CopyImage(image, resized);
        if (linearResize)
        {
            if (!ResizeImageLinear(mipWidth, mipHeight, resized))
            {
                Free(data);
                Free(resized.data);
                return false;
            }
        }
        else
        {
            if (!ResizeImageSRGB(mipWidth, mipHeight, resized))
            {
                Free(data);
                Free(resized.data);
                return false;
            }
        }

        u64 resizedSize = GetImageSize(resized);
        memcpy(data + offset, resized.data, resizedSize);
        offset += resizedSize;

        Free(resized.data);
    }

    Free(image.data);
    image.data = data;
    image.mipCount = mipCount;

    return true;
}

bool Eq2Cube(RHI::Device& device, RHI::GraphicsPipeline& eq2cubePipeline,
             Image& image)
{
    FLY_ASSERT(image.data);
    FLY_ASSERT(image.width);
    FLY_ASSERT(image.height);
    FLY_ASSERT(image.channelCount);
    FLY_ASSERT(image.layerCount);
    FLY_ASSERT(image.mipCount);
    FLY_ASSERT(image.storageType == ImageStorageType::Byte);

    u32 side = image.width / 4;
    VkFormat inputImageFormat =
        ImageVulkanFormat(image.storageType, image.channelCount);

    u32 channelCount = (image.channelCount == 3) ? 4 : image.channelCount;
    VkFormat cubemapFormat = ImageVulkanFormat(image.storageType, channelCount);

    RHI::Texture texture2D{};
    if (!RHI::CreateTexture2D(
            device,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            image.data, image.width, image.height, inputImageFormat,
            RHI::Sampler::FilterMode::Nearest, RHI::Sampler::WrapMode::Clamp, 1,
            texture2D))
    {
        return false;
    }

    RHI::Texture cubemap{};
    if (!RHI::CreateCubemap(device,
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                            nullptr, side, cubemapFormat,
                            RHI::Sampler::FilterMode::Nearest, 1, cubemap))
    {
        return false;
    }

    RHI::Buffer stagingBuffer{};
    u64 bufferSize = GetImageSize(side, side, channelCount, cubemap.layerCount,
                                  1, image.storageType);
    if (!RHI::CreateBuffer(device, true, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           nullptr, bufferSize, stagingBuffer))
    {
        return false;
    }

    RHI::BeginOneTimeSubmit(device);
    RHI::CommandBuffer& cmd = OneTimeSubmitCommandBuffer(device);
    {
        RHI::RecordTextureInput textureInput[2] = {
            {&texture2D, VK_ACCESS_2_SHADER_READ_BIT,
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {&cubemap, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

        VkRenderingAttachmentInfo colorAttachment =
            RHI::ColorAttachmentInfo(cubemap.arrayImageView);
        VkRenderingInfo renderingInfo =
            RHI::RenderingInfo({{0, 0}, {side, side}}, &colorAttachment, 1,
                               nullptr, nullptr, 6, 0x3F);

        RHI::ExecuteGraphics(cmd, renderingInfo,
                             RecordConvertFromEquirectangular, nullptr, 0,
                             textureInput, 2, &eq2cubePipeline);
    }

    {
        RHI::RecordBufferInput bufferInput = {&stagingBuffer,
                                              VK_ACCESS_TRANSFER_WRITE_BIT};
        RHI::RecordTextureInput textureInput = {
            &cubemap, VK_ACCESS_2_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL};

        RHI::ExecuteTransfer(cmd, RecordReadbackCubemap, &bufferInput, 1,
                             &textureInput, 1, nullptr);
    }
    RHI::EndOneTimeSubmit(device);

    u8* data = static_cast<u8*>(Alloc(bufferSize));
    memcpy(data, RHI::BufferMappedPtr(stagingBuffer), bufferSize);

    Free(image.data);
    image.data = data;
    image.width = side;
    image.height = side;
    image.channelCount = channelCount;
    image.layerCount = cubemap.layerCount;
    image.mipCount = 1;

    RHI::DestroyBuffer(device, stagingBuffer);
    RHI::DestroyTexture(device, texture2D);
    RHI::DestroyTexture(device, cubemap);

    return true;
}

bool CompressImage(ImageStorageType codec, Image& image)
{
    FLY_ASSERT(image.data);
    FLY_ASSERT(image.width);
    FLY_ASSERT(image.height);
    FLY_ASSERT(image.channelCount == 4);
    FLY_ASSERT(image.layerCount);
    FLY_ASSERT(image.mipCount);
    FLY_ASSERT(image.storageType == ImageStorageType::Byte);

    if (codec != ImageStorageType::BC1 && codec != ImageStorageType::BC3 &&
        codec != ImageStorageType::BC4 && codec != ImageStorageType::BC5)
    {
        return false;
    }

    if ((codec == ImageStorageType::BC1 || codec == ImageStorageType::BC3) &&
        image.channelCount != 4)
    {
        return false;
    }
    else if (codec == ImageStorageType::BC4 && image.channelCount != 1)
    {
        return false;
    }
    else if (codec == ImageStorageType::BC5 && image.channelCount != 2)
    {
        return false;
    }

    u64 dataSize = GetImageSize(image.width, image.height, image.channelCount,
                                image.layerCount, image.mipCount, codec);
    u8* data = static_cast<u8*>(Alloc(dataSize));

    u32 mipWidth = image.width;
    u32 mipHeight = image.height;
    u64 dataOffset = 0;
    u64 imageOffset = 0;

    for (u32 i = 0; i < image.mipCount; i++)
    {
        for (u32 j = 0; j < image.layerCount; j++)
        {
            CompressImageLayer(data + dataOffset, image.data + imageOffset,
                               mipWidth, mipHeight, codec);
            imageOffset += GetImageSize(mipWidth, mipHeight, image.channelCount,
                                        1, 1, image.storageType);
            dataOffset += GetImageSize(mipWidth, mipHeight, image.channelCount,
                                       1, 1, codec);
        }

        mipWidth = MAX(mipWidth >> 1, 1);
        mipHeight = MAX(mipHeight >> 1, 1);
    }

    if (codec == ImageStorageType::BC1)
    {
        image.channelCount = 3;
    }

    Free(image.data);
    image.data = data;
    image.storageType = codec;

    return true;
}

static u8 Reinhard(f32 value)
{
    value = value / (1.0f + value);
    return static_cast<u8>(value * 255.0f + 0.5f);
}

void TonemapHalf(Image& image)
{
    FLY_ASSERT(image.data);
    FLY_ASSERT(image.width);
    FLY_ASSERT(image.height);
    FLY_ASSERT(image.channelCount);
    FLY_ASSERT(image.layerCount);
    FLY_ASSERT(image.mipCount == 1);
    FLY_ASSERT(image.storageType == ImageStorageType::Half);

    u64 dataSize = GetImageSize(image.width, image.height, image.channelCount,
                                image.layerCount, 1, ImageStorageType::Byte);
    u8* data = static_cast<u8*>(Alloc(dataSize));
    f16* imageData = reinterpret_cast<f16*>(image.data);

    for (u32 n = 0; n < image.layerCount; n++)
    {
        for (u32 i = 0; i < image.height; i++)
        {
            for (u32 j = 0; j < image.width; j++)
            {
                for (u32 k = 0; k < MIN(image.channelCount, 3); k++)
                {
                    f32 value = imageData[n * image.height * image.width *
                                              image.channelCount +
                                          image.width * image.channelCount * i +
                                          image.channelCount * j + k];

                    data[n * image.height * image.width * image.channelCount +
                         image.width * image.channelCount * i +
                         image.channelCount * j + k] = Reinhard(value);
                }

                if (image.channelCount == 4)
                {
                    f32 value = imageData[n * image.height * image.width *
                                              image.channelCount +
                                          image.width * image.channelCount * i +
                                          image.channelCount * j + 3];

                    data[n * image.height * image.width * image.channelCount +
                         image.width * image.channelCount * i +
                         image.channelCount * j + 3] =
                        static_cast<u8>(value * 255.0f + 0.5f);
                }
            }
        }
    }

    Free(image.data);
    image.data = data;
    image.storageType = ImageStorageType::Byte;
}

void TonemapFloat(Image& image)
{
    FLY_ASSERT(image.data);
    FLY_ASSERT(image.width);
    FLY_ASSERT(image.height);
    FLY_ASSERT(image.channelCount);
    FLY_ASSERT(image.layerCount);
    FLY_ASSERT(image.mipCount == 1);
    FLY_ASSERT(image.storageType == ImageStorageType::Float);

    u64 dataSize = GetImageSize(image.width, image.height, image.channelCount,
                                image.layerCount, 1, ImageStorageType::Byte);
    u8* data = static_cast<u8*>(Alloc(dataSize));
    f32* imageData = reinterpret_cast<f32*>(image.data);

    for (u32 n = 0; n < image.layerCount; n++)
    {
        for (u32 i = 0; i < image.height; i++)
        {
            for (u32 j = 0; j < image.width; j++)
            {
                for (u32 k = 0; k < MIN(image.channelCount, 3); k++)
                {
                    f32 value = imageData[n * image.height * image.width *
                                              image.channelCount +
                                          image.width * image.channelCount * i +
                                          image.channelCount * j + k];

                    data[n * image.height * image.width * image.channelCount +
                         image.width * image.channelCount * i +
                         image.channelCount * j + k] = Reinhard(value);
                }

                if (image.channelCount == 4)
                {
                    f32 value = imageData[n * image.height * image.width *
                                              image.channelCount +
                                          image.width * image.channelCount * i +
                                          image.channelCount * j + 3];

                    data[n * image.height * image.width * image.channelCount +
                         image.width * image.channelCount * i +
                         image.channelCount * j + 3] =
                        static_cast<u8>(value * 255.0f + 0.5f);
                }
            }
        }
    }

    Free(image.data);
    image.data = data;
    image.storageType = ImageStorageType::Byte;
}

} // namespace Fly
