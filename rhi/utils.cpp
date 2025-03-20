#include <string.h>

#include "core/filesystem.h"
#include "core/platform.h"
#include "core/thread_context.h"

#include "context.h"
#include "utils.h"

namespace Hls
{

bool LoadProgrammableStage(Arena& arena, Device& device,
                           const ShaderPathMap& shaderPathMap,
                           GraphicsPipelineProgrammableStage& programmableStage)
{
    Arena& scratch = GetScratchArena(&arena);
    ArenaMarker marker = ArenaGetMarker(scratch);
    const char* binDirectoryPath = GetBinaryDirectoryPath(scratch);
    u64 binDirectoryPathStrLength = strlen(binDirectoryPath);

    char* buffer = HLS_ALLOC(scratch, char, 4096);

    for (u32 i = 0; i < static_cast<u32>(ShaderType::Count); i++)
    {
        ArenaMarker loopMarker = ArenaGetMarker(scratch);

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
            ReadFileToString(scratch, buffer, &codeSize, sizeof(u32));
        if (!spvSource)
        {
            ArenaPopToMarker(scratch, marker);
            return false;
        }
        if (!Hls::CreateShaderModule(arena, device, spvSource, codeSize,
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
