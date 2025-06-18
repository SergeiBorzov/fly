#ifndef FLY_DEMO_OCEAN_SKY_RENDERER_H
#define FLY_DEMO_OCEAN_SKY_RENDERER_H

#include "rhi/buffer.h"
#include "rhi/device.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"
#include "rhi/texture.h"

namespace Fly
{

struct SkyBoxRenderer
{
    RHI::Cubemap skyBoxes[FLY_FRAME_IN_FLIGHT_COUNT];
    RHI::GraphicsPipeline skyBoxPipeline;
    u32 resolution;
};

bool CreateSkyBoxRenderer(RHI::Device& device, u32 resolution,
                          SkyBoxRenderer& renderer);
void DestroySkyBoxRenderer(RHI::Device& device, SkyBoxRenderer& renderer);
void RecordSkyBoxRendererCommands(RHI::Device& device,
                                  SkyBoxRenderer& renderer);

} // namespace Fly

#endif /* FLY_DEMO_OCEAN_SKY_RENDERER_H */
