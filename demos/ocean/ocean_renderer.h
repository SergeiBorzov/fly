#ifndef FLY_DEMO_OCEAN_OCEAN_RENDERER_H
#define FLY_DEMO_OCEAN_OCEAN_RENDERER_H

#include "core/arena.h"

#include "math/vec.h"

#include "rhi/buffer.h"
#include "rhi/device.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"
#include "rhi/texture.h"

namespace Fly
{

class SimpleCameraFPS;

struct OceanRenderer
{
    struct Node
    {
        float centerX;
        float centerY;
        float size;
    };

    RHI::Buffer uniformBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
    RHI::Buffer vertexBuffer;
    RHI::Buffer indexBuffer;
    RHI::GraphicsPipeline wireframePipeline;
    RHI::GraphicsPipeline oceanPipeline;
    RHI::GraphicsPipeline skyPipeline;
    u32 indexCount = 0;
};

struct OceanRendererInputs
{
    u32 heightMaps[4];
    u32 diffDisplacementMaps[4];
    u32 skyBox;
};

bool CreateOceanRenderer(RHI::Device& device, OceanRenderer& renderer);
void DestroyOceanRenderer(RHI::Device& device, OceanRenderer& renderer);
void RecordOceanRendererCommands(RHI::Device& device,
                                 const OceanRendererInputs& inputs,
                                 OceanRenderer& renderer);
void UpdateOceanRendererUniforms(RHI::Device& device,
                                 const SimpleCameraFPS& camera, u32 width,
                                 u32 height, OceanRenderer& renderer);

} // namespace Fly

#endif
