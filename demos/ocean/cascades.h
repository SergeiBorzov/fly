#ifndef FLY_DEMO_OCEAN_CASCADES_H
#define FLY_DEMO_OCEAN_CASCADES_H

#include "rhi/buffer.h"
#include "rhi/device.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"
#include "rhi/texture.h"

#define DEMO_OCEAN_CASCADE_COUNT 4

namespace Fly
{

struct JonswapCascade
{
    RHI::Buffer frequencyBuffers[2 * FLY_FRAME_IN_FLIGHT_COUNT];
    RHI::Buffer uniformBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
    RHI::Texture heightMaps[FLY_FRAME_IN_FLIGHT_COUNT];
    RHI::Texture diffDisplacementMaps[FLY_FRAME_IN_FLIGHT_COUNT];

    f32 domain;
    f32 kMin;
    f32 kMax;
};

struct JonswapCascadesRenderer
{
    JonswapCascade cascades[DEMO_OCEAN_CASCADE_COUNT];

    RHI::ComputePipeline jonswapPipeline;
    RHI::ComputePipeline ifftPipeline;
    RHI::ComputePipeline transposePipeline;
    RHI::ComputePipeline copyPipeline;

    u32 resolution;
    f32 fetch;
    f32 windSpeed;
    f32 windDirection;
    f32 spread;
    f32 time;
    f32 amplitudeScale;
};

bool CreateJonswapCascadesRenderer(RHI::Device& device, u32 resolution,
                                   JonswapCascadesRenderer& renderer);
void DestroyJonswapCascadesRenderer(RHI::Device& device,
                                    JonswapCascadesRenderer& renderer);

void RecordJonswapCascadesRendererCommands(RHI::Device& device,
                                           JonswapCascadesRenderer& renderer);

void UpdateJonswapCascadesRendererUniforms(RHI::Device& device,
                                           JonswapCascadesRenderer& renderer);

} // namespace Fly

#endif /* FLY_DEMO_OCEAN_CASCADES_H */
