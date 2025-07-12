#include "core/assert.h"
#include "core/thread_context.h"

#include "rhi/device.h"
#include "rhi/shader_program.h"

#include "assets/import_image.h"
#include "assets/import_spv.h"

#include "utils.h"

namespace Fly
{

bool LoadTexture2DFromFile(RHI::Device& device, const char* path,
                           VkFormat format, RHI::Sampler::FilterMode filterMode,
                           RHI::Sampler::WrapMode wrapMode,
                           RHI::Texture2D& texture)
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
                              filterMode, wrapMode, texture))
    {
        return false;
    }
    FreeImage(image);

    RHI::BeginTransfer(device);
    RHI::CommandBuffer& cmd = TransferCommandBuffer(device);
    RHI::ChangeTexture2DLayout(cmd, texture,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    RHI::EndTransfer(device);

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
