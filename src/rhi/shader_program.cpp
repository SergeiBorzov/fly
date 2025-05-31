#include "allocation_callbacks.h"
#include "device.h"
#include "shader_program.h"

namespace Fly
{
namespace RHI
{

bool CreateShader(Device& device, const char* spvSource, u64 codeSize,
                  Shader& shader)
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

    return true;
}

void DestroyShader(Device& device, Shader& shader)
{
    FLY_ASSERT(shader.handle != VK_NULL_HANDLE);
    vkDestroyShaderModule(device.logicalDevice, shader.handle,
                          GetVulkanAllocationCallbacks());
    shader.handle = VK_NULL_HANDLE;
}

} // namespace RHI
} // namespace Fly
