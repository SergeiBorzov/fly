
#include "rhi/texture.h"

namespace Fly
{

bool LoadTexture2DFromFile(RHI::Device& device, const char* path,
                           VkFormat format, RHI::Sampler::FilterMode filterMode,
                           RHI::Sampler::WrapMode wrapMode,
                           RHI::Texture2D& texture);
bool LoadShaderFromSpv(RHI::Device& device, const char* path,
                       RHI::Shader& shader);

}
