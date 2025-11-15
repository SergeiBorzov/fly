#include "shader_program.h"
#include "allocation_callbacks.h"
#include "device.h"

namespace Fly
{
namespace RHI
{

bool CreateShader(Device& device, Shader::Type type, const char* spvSource,
                  u64 codeSize, Shader& shader)
{
    FLY_ASSERT(spvSource);
    FLY_ASSERT((reinterpret_cast<uintptr_t>(spvSource) % 4) == 0);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = codeSize;
    createInfo.pCode = reinterpret_cast<const u32*>(spvSource);

    if (vkCreateShaderModule(device.logicalDevice, &createInfo,
                             GetVulkanAllocationCallbacks(),
                             &shader.handle) != VK_SUCCESS)
    {
        return false;
    }

    shader.type = type;

    return true;
}

void DestroyShader(Device& device, Shader& shader)
{
    FLY_ASSERT(shader.handle != VK_NULL_HANDLE);
    vkDestroyShaderModule(device.logicalDevice, shader.handle,
                          GetVulkanAllocationCallbacks());
    shader.handle = VK_NULL_HANDLE;
    shader.type = Shader::Type::Invalid;
}

} // namespace RHI
} // namespace Fly
