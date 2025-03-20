#include <string.h>

#include "core/thread_context.h"
#include "core/filesystem.h"
#include "core/platform.h"

#include "context.h"
#include "utils.h"

namespace Hls
{

bool LoadProgrammableStage(Device& device,
                           const ShaderPathMap& shaderPathMap,
                           GraphicsPipelineProgrammableStage& programmableStage)
{
    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);
    const char* binDirectoryPath = GetBinaryDirectoryPath(arena);
    u64 binDirectoryPathStrLength = strlen(binDirectoryPath);

    char* buffer = HLS_ALLOC(arena, char, 4096);

    for (u32 i = 0; i < static_cast<u32>(ShaderType::Count); i++)
    {
        ArenaMarker loopMarker = ArenaGetMarker(arena);

        ShaderType shaderType = static_cast<ShaderType>(i);

        if (!shaderPathMap[shaderType])
        {
            continue;
        }

        strncpy(buffer, binDirectoryPath, binDirectoryPathStrLength);
        strncpy(buffer + binDirectoryPathStrLength, shaderPathMap[shaderType],
                strlen(shaderPathMap[shaderType]));

        u64 codeSize = 0;
        const char* spvSource =
            ReadFileToString(arena, buffer, &codeSize, sizeof(u32));
        if (!spvSource)
        {
            ArenaPopToMarker(arena, marker);
            return false;
        }
        if (!Hls::CreateShaderModule(device, spvSource, codeSize,
                                     programmableStage[shaderType]))
        {
            ArenaPopToMarker(arena, marker);
            return false;
        }

        ArenaPopToMarker(arena, loopMarker);
    }

    ArenaPopToMarker(arena, marker);
    return true;
}

} // namespace Hls
