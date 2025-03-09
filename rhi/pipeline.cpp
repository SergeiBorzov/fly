#include "core/assert.h"

#include "pipeline.h"

namespace Hls
{

bool CreateShaderModule(VkDevice device, const char* spvSource, u64 codeSize,
                        VkShaderModule& shaderModule)
{
    HLS_ASSERT(spvSource);
    HLS_ASSERT((reinterpret_cast<uintptr_t>(spvSource) % 4) == 0);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = codeSize;
    createInfo.pCode = reinterpret_cast<const u32*>(spvSource);

    return vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) ==
           VK_SUCCESS;
}

void DestroyShaderModule(VkDevice device, VkShaderModule shaderModule)
{
    vkDestroyShaderModule(device, shaderModule, nullptr);
}

} // namespace Hls
