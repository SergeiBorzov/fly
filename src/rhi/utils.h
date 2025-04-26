#ifndef HLS_RHI_UTILS_H
#define HLS_RHI_UTILS_H

#include "core/filesystem.h"

#include "pipeline.h"

struct Arena;
struct Image;

namespace Hls
{
namespace RHI
{

struct Device;
struct Buffer;
struct Texture;
struct GraphicsPipelineProgrammableStage;

struct ShaderPathMap
{
    inline Path& operator[](ShaderType type)
    {
        u32 index = static_cast<size_t>(type);
        HLS_ASSERT(index < static_cast<size_t>(ShaderType::Count));
        return paths[index];
    }

    inline const Path& operator[](ShaderType type) const
    {
        u32 index = static_cast<u32>(type);
        HLS_ASSERT(index < static_cast<u32>(ShaderType::Count));
        return paths[static_cast<size_t>(index)];
    }

private:
    Path paths[ShaderType::Count];
};

bool LoadProgrammableStage(
    Arena& arena, Device& device, const ShaderPathMap& shaderPathMap,
    GraphicsPipelineProgrammableStage& programmableStage);

} // namespace RHI
} // namespace Hls

#endif /* End of HLS_RHI_UTILS_H */
