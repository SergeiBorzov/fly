#include <volk.h>

#include "core/string8.h"

#include "rhi/texture.h"

namespace Fly
{

namespace RHI
{
struct Shader;
}

bool LoadTexture2DFromFile(RHI::Device& device, VkImageUsageFlags flags,
                           String8 path, VkFormat format,
                           RHI::Sampler::FilterMode filterMode,
                           RHI::Sampler::WrapMode wrapMode,
                           RHI::Texture& texture);
bool LoadCompressedTexture2D(RHI::Device& device, VkImageUsageFlags,
                             String8 path, VkFormat format,
                             RHI::Sampler::FilterMode filterMode,
                             RHI::Sampler::WrapMode wrapMode,
                             RHI::Texture& texture);
bool LoadCubemap(RHI::Device& device, VkImageUsageFlags usage, String8 path,
                 VkFormat format, RHI::Sampler::FilterMode filterMode,
                 u32 mipCount, RHI::Texture& texture);
bool LoadCompressedCubemap(RHI::Device& device, VkImageUsageFlags usage,
                           String8 path, VkFormat format,
                           RHI::Sampler::FilterMode filterMode,
                           RHI::Texture& texture);
bool LoadShaderFromSpv(RHI::Device& device, String8 path, RHI::Shader& shader);

} // namespace Fly
