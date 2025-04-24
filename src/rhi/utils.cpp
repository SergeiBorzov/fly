#include <string.h>

#include "core/filesystem.h"
#include "core/platform.h"
#include "core/thread_context.h"

#include "context.h"
#include "texture.h"
#include "utils.h"

namespace Hls
{

bool LoadProgrammableStage(Arena& arena, Device& device,
                           const ShaderPathMap& shaderPathMap,
                           GraphicsPipelineProgrammableStage& programmableStage)
{
    Arena& scratch = GetScratchArena(&arena);
    ArenaMarker marker = ArenaGetMarker(scratch);

    for (u32 i = 0; i < static_cast<u32>(ShaderType::Count); i++)
    {
        ArenaMarker loopMarker = ArenaGetMarker(scratch);

        ShaderType shaderType = static_cast<ShaderType>(i);

        if (!shaderPathMap[shaderType])
        {
            continue;
        }

        String8 spvSource =
            ReadFileToString(scratch, shaderPathMap[shaderType], sizeof(u32));
        if (!spvSource)
        {
            ArenaPopToMarker(scratch, marker);
            return false;
        }
        if (!Hls::CreateShaderModule(arena, device, spvSource.Data(), spvSource.Size(),
                                     programmableStage[shaderType]))
        {
            ArenaPopToMarker(scratch, marker);
            return false;
        }

        ArenaPopToMarker(scratch, loopMarker);
    }

    ArenaPopToMarker(scratch, marker);
    return true;
}

} // namespace Hls
