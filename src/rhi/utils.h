#ifndef HLS_RHI_UTILS_H
#define HLS_RHI_UTILS_H

#include "core/string8.h"

#include "pipeline.h"

struct Arena;
struct Image;

namespace Hls
{

struct Device;
struct Buffer;
struct Texture;

struct ShaderPathMap
{
    inline String8& operator[](ShaderType type)
    {
        u32 index = static_cast<size_t>(type);
        HLS_ASSERT(index < static_cast<size_t>(ShaderType::Count));
        return paths[index];
    }

    inline const String8& operator[](ShaderType type) const
    {
        u32 index = static_cast<u32>(type);
        HLS_ASSERT(index < static_cast<u32>(ShaderType::Count));
        return paths[static_cast<size_t>(index)];
    }

private:
    String8 paths[ShaderType::Count];
};

bool LoadProgrammableStage(
    Arena& arena, Device& device, const ShaderPathMap& shaderPathMap,
    GraphicsPipelineProgrammableStage& programmableStage);

} // namespace Hls

#endif /* End of HLS_RHI_UTILS_H */
