#ifndef HLS_RHI_SHADER_PROGRAM_H
#define HLS_RHI_SHADER_PROGRAM_H

#include "core/assert.h"
#include "core/types.h"
#include <volk.h>

namespace Hls
{
namespace RHI
{

struct Device;

struct Shader
{
    enum class Type
    {
        Vertex = 0,
        Fragment = 1,
        Geometry = 2,
        Task = 3,
        Mesh = 4,
        Compute = 5,
        Count,
        Invalid,
    };

    VkShaderModule handle = VK_NULL_HANDLE;
};
bool CreateShader(Device& device, const char* spvSource, u64 codeSize,
                  Shader& shaderModule);
void DestroyShader(Device& device, Shader& shaderModule);

struct ShaderProgram
{
    inline Shader& operator[](Shader::Type type)
    {
        u32 index = static_cast<u32>(type);
        HLS_ASSERT(index < static_cast<u32>(Shader::Type::Count));
        return shaders[index];
    }

    inline const Shader& operator[](Shader::Type type) const
    {
        u32 index = static_cast<u32>(type);
        HLS_ASSERT(index < static_cast<u32>(Shader::Type::Count));
        return shaders[index];
    }

private:
    Shader shaders[static_cast<u32>(Shader::Type::Count)] = {};
};

} // namespace RHI
} // namespace Hls

#endif /* End of HLS_RHI_SHADER_PROGRAM_H */
