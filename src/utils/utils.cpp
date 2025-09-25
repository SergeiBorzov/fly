#include "core/assert.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "math/functions.h"

#include "rhi/device.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"

#include "assets/image/image.h"
#include "assets/image/import_image.h"
#include "assets/import_spv.h"

#include "utils.h"

namespace Fly
{

bool LoadTexture2DFromFile(RHI::Device& device, const char* path,
                           VkFormat format, RHI::Sampler::FilterMode filterMode,
                           RHI::Sampler::WrapMode wrapMode,
                           RHI::Texture& texture)
{
    FLY_ASSERT(path);

    Image image;
    if (!Fly::LoadImageFromFile(path, image))
    {
        return false;
    }

    if (!RHI::CreateTexture2D(device,
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                  VK_IMAGE_USAGE_SAMPLED_BIT,
                              image.data, image.width, image.height, format,
                              filterMode, wrapMode, 0, texture))
    {
        return false;
    }
    FreeImage(image);
    return true;
}

u64 GetCompressedSize(u32 width, u32 height, VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        case VK_FORMAT_BC4_UNORM_BLOCK:
        case VK_FORMAT_BC4_SNORM_BLOCK:
        {
            return static_cast<u64>(Math::Ceil(width / 4.0f) *
                                    Math::Ceil(height / 4.0f) * 8);
            break;
        }
        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC5_UNORM_BLOCK:
        case VK_FORMAT_BC5_SNORM_BLOCK:
        {
            return static_cast<u64>(Math::Ceil(width / 4.0f) *
                                    Math::Ceil(height / 4.0f) * 16);
            break;
        }
        default:
        {
            return 0;
        }
    }
}

bool LoadCompressedCubemap(RHI::Device& device, const char* path,
                           VkFormat format, RHI::Sampler::FilterMode filterMode,
                           RHI::Texture& texture)
{
    FLY_ASSERT(format == VK_FORMAT_BC1_RGB_UNORM_BLOCK ||
               format == VK_FORMAT_BC1_RGB_SRGB_BLOCK ||
               format == VK_FORMAT_BC1_RGBA_UNORM_BLOCK ||
               format == VK_FORMAT_BC1_RGBA_SRGB_BLOCK ||
               format == VK_FORMAT_BC3_UNORM_BLOCK ||
               format == VK_FORMAT_BC3_SRGB_BLOCK ||
               format == VK_FORMAT_BC4_UNORM_BLOCK ||
               format == VK_FORMAT_BC4_SNORM_BLOCK ||
               format == VK_FORMAT_BC5_UNORM_BLOCK ||
               format == VK_FORMAT_BC5_SNORM_BLOCK);

    Image image;
    if (!Fly::LoadImageFromFile(path, image))
    {
        return false;
    }

    if (!RHI::CreateCubemap(device,
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                VK_IMAGE_USAGE_SAMPLED_BIT,
                            image.data, image.width, format, filterMode,
                            image.mipCount, texture))
    {
        return false;
    }

    if (image.mipCount == 1)
    {
        FreeImage(image);
        return true;
    }

    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    RHI::MipDesc* mips = FLY_PUSH_ARENA(arena, RHI::MipDesc, image.mipCount);
    for (u32 i = 0; i < image.mipCount; i++)
    {
        Mip mip;
        if (!GetImageMipLevel(image, 0, i, mip))
        {
            return false;
        }
        mips[i].data = mip.data;
        mips[i].width = mip.width;
        mips[i].height = mip.height;
        mips[i].size = mip.size;
    }

    if (!CopyMipsToTexture(device, texture, mips, image.mipCount))
    {
        return false;
    }
    ArenaPopToMarker(arena, marker);

    FreeImage(image);
    return true;
}

bool LoadCompressedTexture2D(RHI::Device& device, const char* path,
                             VkFormat format,
                             RHI::Sampler::FilterMode filterMode,
                             RHI::Sampler::WrapMode wrapMode,
                             RHI::Texture& texture)
{
    FLY_ASSERT(format == VK_FORMAT_BC1_RGB_UNORM_BLOCK ||
               format == VK_FORMAT_BC1_RGB_SRGB_BLOCK ||
               format == VK_FORMAT_BC1_RGBA_UNORM_BLOCK ||
               format == VK_FORMAT_BC1_RGBA_SRGB_BLOCK ||
               format == VK_FORMAT_BC3_UNORM_BLOCK ||
               format == VK_FORMAT_BC3_SRGB_BLOCK ||
               format == VK_FORMAT_BC4_UNORM_BLOCK ||
               format == VK_FORMAT_BC4_SNORM_BLOCK ||
               format == VK_FORMAT_BC5_UNORM_BLOCK ||
               format == VK_FORMAT_BC5_SNORM_BLOCK);

    Image image;
    if (!Fly::LoadImageFromFile(path, image))
    {
        return false;
    }

    if (!RHI::CreateTexture2D(device,
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                  VK_IMAGE_USAGE_SAMPLED_BIT,
                              nullptr, image.width, image.height, format,
                              filterMode, wrapMode, image.mipCount, texture))
    {
        return false;
    }

    if (image.mipCount == 1)
    {
        FreeImage(image);
        return true;
    }

    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    RHI::MipDesc* mips = FLY_PUSH_ARENA(arena, RHI::MipDesc, image.mipCount);
    for (u32 i = 0; i < image.mipCount; i++)
    {
        Mip mip;
        if (!GetImageMipLevel(image, 0, i, mip))
        {
            return false;
        }
        mips[i].data = mip.data;
        mips[i].width = mip.width;
        mips[i].height = mip.height;
        mips[i].size = mip.size;
    }

    if (!CopyMipsToTexture(device, texture, mips, image.mipCount))
    {
        return false;
    }
    ArenaPopToMarker(arena, marker);

    FreeImage(image);
    return true;
}

bool LoadShaderFromSpv(RHI::Device& device, const char* path,
                       RHI::Shader& shader)
{
    FLY_ASSERT(path);

    Arena& scratch = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(scratch);

    String8 spvSource = Fly::LoadSpvFromFile(scratch, path);
    if (!spvSource)
    {
        return false;
    }
    if (!RHI::CreateShader(device, spvSource.Data(), spvSource.Size(), shader))
    {
        ArenaPopToMarker(scratch, marker);
        return false;
    }

    ArenaPopToMarker(scratch, marker);
    return true;
}

} // namespace Fly
