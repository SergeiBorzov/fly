#ifndef FLY_DEMO_OCEAN_OCEAN_RENDERER_H
#define FLY_DEMO_OCEAN_OCEAN_RENDERER_H

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
    RHI::GraphicsPipeline wireframePipeline;
    RHI::GraphicsPipeline oceanPipeline;
    RHI::GraphicsPipeline skyPipeline;
    RHI::Buffer uniformBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
};

struct OceanRendererInputs
{
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
