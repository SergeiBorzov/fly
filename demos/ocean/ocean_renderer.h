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
    RHI::Buffer uniformBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
    RHI::Buffer shadeParamsBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
    RHI::Buffer vertexBuffer;
    RHI::Buffer indexBuffer;
    RHI::GraphicsPipeline wireframePipeline;
    RHI::GraphicsPipeline oceanPipeline;
    RHI::GraphicsPipeline skyPipeline;
    
    Math::Vec3 lightColor;
    Math::Vec3 waterScatterColor;
    Math::Vec3 bubbleColor;
    f32 ss1;
    f32 ss2;
    f32 a1;
    f32 a2;
    f32 reflectivity;
    f32 bubbleDensity;
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
