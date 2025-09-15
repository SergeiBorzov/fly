#include <volk.h>

#include "rhi/texture.h"

namespace Fly
{

bool LoadTexture2DFromFile(RHI::Device& device, const char* path,
                           VkFormat format, RHI::Sampler::FilterMode filterMode,
                           RHI::Sampler::WrapMode wrapMode,
                           RHI::Texture& texture);
bool LoadCubemapEquirectangularFromFile(RHI::Device& device, const char* path,
                                        VkFormat format,
                                        RHI::Sampler::FilterMode filterMode,
                                        RHI::Texture& texture);
bool LoadShaderFromSpv(RHI::Device& device, const char* path,
                       RHI::Shader& shader);

} // namespace Fly
