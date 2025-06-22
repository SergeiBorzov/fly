#include "cascades.h"
#include "core/log.h"
#include "demos/common/scene.h"

namespace Fly
{

struct OceanFrequencyVertex
{
    Math::Vec2 value;
    Math::Vec2 displacement;
    Math::Vec2 dx;
    Math::Vec2 dy;
    Math::Vec2 dxDisplacement;
    Math::Vec2 dyDisplacement;
};

struct JonswapData
{
    Math::Vec4 fetchSpeedDirSpread;
    Math::Vec4 timeScale;
    Math::Vec4 domainMinMax;
};

bool CreateJonswapCascadesRenderer(RHI::Device& device, u32 resolution,
                                   JonswapCascadesRenderer& renderer)
{
    renderer.resolution = resolution;
    renderer.fetch = 500000.0f;
    renderer.windSpeed = 3.0f;
    renderer.windDirection = -2.4f;
    renderer.spread = 18.0f;
    renderer.time = 0.0f;

    RHI::ComputePipeline* computePipelines[] = {
        &renderer.jonswapPipeline,
        &renderer.ifftPipeline,
        &renderer.transposePipeline,
        &renderer.copyPipeline,
    };

    const char* computeShaderPaths[] = {"jonswap.comp.spv", "ifft.comp.spv",
                                        "transpose.comp.spv", "copy.comp.spv"};

    for (u32 i = 0; i < STACK_ARRAY_COUNT(computeShaderPaths); i++)
    {
        RHI::Shader shader;
        if (!Fly::LoadShaderFromSpv(device, computeShaderPaths[i], shader))
        {
            return false;
        }
        if (!RHI::CreateComputePipeline(device, shader, *computePipelines[i]))
        {
            return false;
        }
        RHI::DestroyShader(device, shader);
    }

    for (u32 i = 0; i < DEMO_OCEAN_CASCADE_COUNT; i++)
    {
        JonswapCascade& cascade = renderer.cascades[i];
        for (u32 j = 0; j < FLY_FRAME_IN_FLIGHT_COUNT; j++)
        {
            for (u32 k = 0; k < 2; k++)
            {
                if (!RHI::CreateStorageBuffer(
                        device, false, nullptr,
                        sizeof(OceanFrequencyVertex) * renderer.resolution *
                            renderer.resolution,
                        cascade.frequencyBuffers[j * 2 + k]))
                {
                    return false;
                }
            }

            if (!RHI::CreateUniformBuffer(device, nullptr, sizeof(JonswapData),
                                          cascade.uniformBuffers[j]))
            {
                return false;
            }

            if (!RHI::CreateReadWriteTexture(
                    device, nullptr, 256 * 256 * 4 * sizeof(u16), 256, 256,
                    VK_FORMAT_R16G16B16A16_SFLOAT,
                    RHI::Sampler::FilterMode::Anisotropy4x,
                    RHI::Sampler::WrapMode::Repeat,
                    cascade.diffDisplacementMaps[j]))
            {
                return false;
            }

            if (!RHI::CreateReadWriteTexture(
                    device, nullptr, 256 * 256 * 4 * sizeof(u16), 256, 256,
                    VK_FORMAT_R16G16B16A16_SFLOAT,
                    RHI::Sampler::FilterMode::Anisotropy4x,
                    RHI::Sampler::WrapMode::Repeat, cascade.heightMaps[j]))
            {
                return false;
            }
            FLY_LOG("height map - %u, handle - %u", i,
                    cascade.heightMaps[j].bindlessHandle);
        }
    }
    return true;
}

void DestroyJonswapCascadesRenderer(RHI::Device& device,
                                    JonswapCascadesRenderer& renderer)
{
    for (u32 i = 0; i < DEMO_OCEAN_CASCADE_COUNT; i++)
    {
        JonswapCascade& cascade = renderer.cascades[i];
        for (u32 j = 0; j < FLY_FRAME_IN_FLIGHT_COUNT; j++)
        {
            RHI::DestroyTexture(device, cascade.heightMaps[j]);
            RHI::DestroyTexture(device, cascade.diffDisplacementMaps[j]);
            RHI::DestroyBuffer(device, cascade.uniformBuffers[j]);
            for (u32 k = 0; k < 2; k++)
            {
                RHI::DestroyBuffer(device, cascade.frequencyBuffers[j * 2 + k]);
            }
        }
    }

    RHI::DestroyComputePipeline(device, renderer.copyPipeline);
    RHI::DestroyComputePipeline(device, renderer.transposePipeline);
    RHI::DestroyComputePipeline(device, renderer.ifftPipeline);
    RHI::DestroyComputePipeline(device, renderer.jonswapPipeline);
}

static void JonswapPass(RHI::Device& device, JonswapCascadesRenderer& renderer)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    RHI::BindComputePipeline(device, cmd, renderer.jonswapPipeline);
    for (u32 i = 0; i < DEMO_OCEAN_CASCADE_COUNT; i++)
    {
        JonswapCascade& cascade = renderer.cascades[i];
        u32 pushConstants[] = {
            cascade.frequencyBuffers[2 * device.frameIndex].bindlessHandle,
            cascade.uniformBuffers[device.frameIndex].bindlessHandle};
        RHI::SetPushConstants(device, cmd, &pushConstants,
                              sizeof(pushConstants));

        vkCmdDispatch(cmd.handle, renderer.resolution, 1, 1);
    }

    VkBufferMemoryBarrier barriers[DEMO_OCEAN_CASCADE_COUNT] = {};
    for (u32 i = 0; i < DEMO_OCEAN_CASCADE_COUNT; i++)
    {
        JonswapCascade& cascade = renderer.cascades[i];
        barriers[i] = RHI::BufferMemoryBarrier(
            cascade.frequencyBuffers[2 * device.frameIndex],
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    }

    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                         STACK_ARRAY_COUNT(barriers), barriers, 0, nullptr);
}

static void IFFTPass(RHI::Device& device, JonswapCascadesRenderer& renderer)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    // IFFT - FIRST AXIS
    {
        RHI::BindComputePipeline(device, cmd, renderer.ifftPipeline);
        for (u32 i = 0; i < DEMO_OCEAN_CASCADE_COUNT; i++)
        {
            JonswapCascade& cascade = renderer.cascades[i];
            u32 pushConstants[] = {
                cascade.frequencyBuffers[2 * device.frameIndex].bindlessHandle};
            RHI::SetPushConstants(device, cmd, pushConstants,
                                  sizeof(pushConstants));
            vkCmdDispatch(cmd.handle, 256, 1, 1);
        }
        VkBufferMemoryBarrier barriers[DEMO_OCEAN_CASCADE_COUNT] = {};
        for (u32 i = 0; i < DEMO_OCEAN_CASCADE_COUNT; i++)
        {
            JonswapCascade& cascade = renderer.cascades[i];
            barriers[i] = RHI::BufferMemoryBarrier(
                cascade.frequencyBuffers[2 * device.frameIndex],
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT);
        }
        vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                             nullptr, STACK_ARRAY_COUNT(barriers), barriers, 0,
                             nullptr);
    }

    // Transpose
    {
        RHI::BindComputePipeline(device, cmd, renderer.transposePipeline);
        for (u32 i = 0; i < DEMO_OCEAN_CASCADE_COUNT; i++)
        {
            JonswapCascade& cascade = renderer.cascades[i];
            u32 pushConstants[] = {
                cascade.frequencyBuffers[2 * device.frameIndex].bindlessHandle,
                cascade.frequencyBuffers[2 * device.frameIndex + 1]
                    .bindlessHandle,
                renderer.resolution};
            RHI::SetPushConstants(device, cmd, pushConstants,
                                  sizeof(pushConstants));
            vkCmdDispatch(cmd.handle, renderer.resolution / 16,
                          renderer.resolution / 16, 1);
        }
        VkBufferMemoryBarrier barriers[DEMO_OCEAN_CASCADE_COUNT] = {};
        for (u32 i = 0; i < DEMO_OCEAN_CASCADE_COUNT; i++)
        {
            JonswapCascade& cascade = renderer.cascades[i];
            barriers[i] = RHI::BufferMemoryBarrier(
                cascade.frequencyBuffers[2 * device.frameIndex + 1],
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        }
        vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                             nullptr, STACK_ARRAY_COUNT(barriers), barriers, 0,
                             nullptr);
    }

    // IFFT - SECOND AXIS
    {
        RHI::BindComputePipeline(device, cmd, renderer.ifftPipeline);
        for (u32 i = 0; i < DEMO_OCEAN_CASCADE_COUNT; i++)
        {
            JonswapCascade& cascade = renderer.cascades[i];
            u32 pushConstants[] = {
                cascade.frequencyBuffers[2 * device.frameIndex + 1]
                    .bindlessHandle};
            RHI::SetPushConstants(device, cmd, pushConstants,
                                  sizeof(pushConstants));
            vkCmdDispatch(cmd.handle, 256, 1, 1);
        }
        VkBufferMemoryBarrier barriers[DEMO_OCEAN_CASCADE_COUNT] = {};
        for (u32 i = 0; i < DEMO_OCEAN_CASCADE_COUNT; i++)
        {
            JonswapCascade& cascade = renderer.cascades[i];
            barriers[i] = RHI::BufferMemoryBarrier(
                cascade.frequencyBuffers[2 * device.frameIndex + 1],
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT);
        }
        vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                             nullptr, STACK_ARRAY_COUNT(barriers), barriers, 0,
                             nullptr);
    }
}

static void CopyPass(RHI::Device& device, JonswapCascadesRenderer& renderer)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    RHI::BindComputePipeline(device, cmd, renderer.copyPipeline);

    for (u32 i = 0; i < DEMO_OCEAN_CASCADE_COUNT; i++)
    {
        JonswapCascade& cascade = renderer.cascades[i];
        u32 pushConstants[] = {
            cascade.frequencyBuffers[2 * device.frameIndex + 1].bindlessHandle,
            cascade.diffDisplacementMaps[device.frameIndex]
                .bindlessStorageHandle,
            cascade.heightMaps[device.frameIndex].bindlessStorageHandle};
        RHI::SetPushConstants(device, cmd, pushConstants,
                              sizeof(pushConstants));
        vkCmdDispatch(cmd.handle, renderer.resolution, 1, 1);
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkImageMemoryBarrier barriers[2 * DEMO_OCEAN_CASCADE_COUNT];
    for (u32 i = 0; i < DEMO_OCEAN_CASCADE_COUNT; i++)
    {
        JonswapCascade& cascade = renderer.cascades[i];
        barriers[2 * i + 0] = barrier;
        barriers[2 * i + 1] = barrier;
        barriers[2 * i + 0].image = cascade.heightMaps[device.frameIndex].image;
        barriers[2 * i + 1].image =
            cascade.diffDisplacementMaps[device.frameIndex].image;
    }

    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, STACK_ARRAY_COUNT(barriers), barriers);
}

void RecordJonswapCascadesRendererCommands(RHI::Device& device,
                                           JonswapCascadesRenderer& renderer)
{
    JonswapPass(device, renderer);
    IFFTPass(device, renderer);
    CopyPass(device, renderer);
}

void UpdateJonswapCascadesRendererUniforms(RHI::Device& device,
                                           JonswapCascadesRenderer& renderer)
{
    JonswapData jonswapData[DEMO_OCEAN_CASCADE_COUNT];
    for (u32 i = 0; i < DEMO_OCEAN_CASCADE_COUNT; i++)
    {
        JonswapCascade& cascade = renderer.cascades[i];
        jonswapData[i].fetchSpeedDirSpread =
            Math::Vec4(renderer.fetch, renderer.windSpeed,
                       renderer.windDirection, renderer.spread);
        jonswapData[i].timeScale = Math::Vec4(renderer.time, 1.0f, 0.0f, 0.0f);
        jonswapData[i].domainMinMax =
            Math::Vec4(cascade.domain, cascade.kMin, cascade.kMax, 0.0f);

        RHI::CopyDataToBuffer(device, &(jonswapData[i]), sizeof(JonswapData), 0,
                              cascade.uniformBuffers[device.frameIndex]);
    }
}

} // namespace Fly
