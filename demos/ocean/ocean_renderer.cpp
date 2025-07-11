#include "ocean_renderer.h"

#include "core/log.h"
#include "core/thread_context.h"

#include "rhi/allocation_callbacks.h"

#include "demos/common/scene.h"
#include "demos/common/simple_camera_fps.h"

namespace Fly
{

struct UniformData
{
    Math::Mat4 projection;
    Math::Mat4 view;
    Math::Vec4 screenSize;
};

struct ShadeParams
{
    Math::Vec4 lightColorReflectivity;
    Math::Vec4 waterScatterColor;
    Math::Vec4 coefficients;
    Math::Vec4 bubbleColorDensity;
};

struct Vertex
{
    Math::Vec2 position;
    Math::Vec2 uv;
};

static RHI::GraphicsPipeline* sCurrentPipeline;

static bool CreateGrid(RHI::Device& device, OceanRenderer& renderer)
{
    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    f32 tileSize = 256.0f;
    f32 quadSize = 0.5f;
    i32 quadPerSide = static_cast<i32>(tileSize / quadSize);
    i32 offset = quadPerSide / 2;
    u32 vertexPerSide = quadPerSide + 1;

    // Generate vertices
    Vertex* vertices =
        FLY_PUSH_ARENA(arena, Vertex, vertexPerSide * vertexPerSide);
    u32 count = 0;
    for (i32 z = -offset; z <= offset; z++)
    {
        for (i32 x = -offset; x <= offset; x++)
        {
            Vertex& vertex = vertices[count++];
            vertex.position = Math::Vec2(static_cast<f32>(x) * quadSize,
                                         static_cast<f32>(z) * quadSize);
            // FLY_LOG("%f %f", vertex.position.x, vertex.position.y);
            vertex.uv =
                Math::Vec2((x + offset) / static_cast<f32>(quadPerSide),
                           (z + offset) / static_cast<f32>(quadPerSide));
        }
    }

    if (!CreateStorageBuffer(device, false, vertices,
                             vertexPerSide * vertexPerSide * sizeof(Vertex),
                             renderer.vertexBuffer))
    {
        ArenaPopToMarker(arena, marker);
        return false;
    }
    ArenaPopToMarker(arena, marker);

    // Generate indices
    renderer.indexCount = static_cast<u32>(6 * quadPerSide * quadPerSide);
    u32* indices = FLY_PUSH_ARENA(arena, u32, renderer.indexCount);
    for (i32 z = 0; z < quadPerSide; z++)
    {
        for (i32 x = 0; x < quadPerSide; x++)
        {
            u32 indexBase = 6 * (quadPerSide * z + x);
            indices[indexBase + 0] = vertexPerSide * z + x;
            indices[indexBase + 1] = vertexPerSide * z + x + 1;
            indices[indexBase + 2] = vertexPerSide * (z + 1) + x;
            indices[indexBase + 3] = vertexPerSide * (z + 1) + x;
            indices[indexBase + 4] = vertexPerSide * z + x + 1;
            indices[indexBase + 5] = vertexPerSide * (z + 1) + x + 1;
        }
    }
    if (!CreateIndexBuffer(device, indices, sizeof(u32) * renderer.indexCount,
                           renderer.indexBuffer))
    {
        ArenaPopToMarker(arena, marker);
        return false;
    }

    ArenaPopToMarker(arena, marker);

    return true;
}

bool CreateOceanRenderer(RHI::Device& device, u32 resolution,
                         OceanRenderer& renderer)
{
    RHI::GraphicsPipelineFixedStateStage fixedState{};
    fixedState.pipelineRendering.colorAttachments[0] =
        device.surfaceFormat.format;
    fixedState.pipelineRendering.depthAttachmentFormat =
        device.depthTexture.format;
    fixedState.depthStencilState.depthTestEnable = true;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.colorBlendState.attachmentCount = 1;
    // fixedState.rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    fixedState.rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    fixedState.rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    fixedState.inputAssemblyState.topology =
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    RHI::ShaderProgram shaderProgram{};
    if (!LoadShaderFromSpv(device, "ocean.vert.spv",
                           shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }
    if (!LoadShaderFromSpv(device, "ocean.frag.spv",
                           shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
    }
    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     renderer.oceanPipeline))
    {
        return false;
    }

    fixedState.rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     renderer.wireframePipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    fixedState.rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    fixedState.depthStencilState.depthTestEnable = false;
    fixedState.pipelineRendering.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    if (!LoadShaderFromSpv(device, "screen_quad.vert.spv",
                           shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }
    if (!LoadShaderFromSpv(device, "sky.frag.spv",
                           shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
    }
    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     renderer.skyPipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    if (!LoadShaderFromSpv(device, "foam.frag.spv",
                           shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
    }
    fixedState.pipelineRendering.colorAttachments[0] = VK_FORMAT_R8_UNORM;
    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     renderer.foamPipeline))
    {
        return false;
    }

    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateUniformBuffer(device, nullptr, sizeof(UniformData),
                                      renderer.uniformBuffers[i]))
        {
            return false;
        }

        if (!RHI::CreateUniformBuffer(device, nullptr, sizeof(ShadeParams),
                                      renderer.shadeParamsBuffers[i]))
        {
            return false;
        }
    }

    for (u32 i = 0; i < 2; i++)
    {
        if (!RHI::CreateTexture2D(
                device,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                nullptr, resolution * resolution * sizeof(u8), resolution,
                resolution, VK_FORMAT_R8_UNORM,
                RHI::Sampler::FilterMode::Anisotropy8x,
                RHI::Sampler::WrapMode::Repeat, renderer.foamTextures[i]))
        {
            return false;
        }
    }

    if (!CreateGrid(device, renderer))
    {
        return false;
    }

    VkSemaphoreTypeCreateInfo timelineCreateInfo{};
    timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineCreateInfo.initialValue = 0;

    VkSemaphoreCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    createInfo.pNext = &timelineCreateInfo;

    vkCreateSemaphore(device.logicalDevice, &createInfo,
                      RHI::GetVulkanAllocationCallbacks(),
                      &renderer.foamSemaphore);

    renderer.waterScatterColor = Math::Vec3(0.059f, 0.3725f, 0.349f);
    renderer.lightColor = Math::Vec3(0.961f, 0.945f, 0.89f);
    renderer.bubbleColor = Math::Vec3(1.0f);
    renderer.ss1 = 5.92f;
    renderer.ss2 = 0.17f;
    renderer.a1 = 0.01f;
    renderer.a2 = 0.1f;
    renderer.reflectivity = 1.0f;
    renderer.bubbleDensity = 0.14f;
    renderer.foamDecay = 1.46f;
    renderer.foamGain = 1.49f;
    renderer.waveChopiness = 1.0f;

    sCurrentPipeline = &renderer.oceanPipeline;
    return true;
}

void DestroyOceanRenderer(RHI::Device& device, OceanRenderer& renderer)
{
    vkDestroySemaphore(device.logicalDevice, renderer.foamSemaphore,
                       RHI::GetVulkanAllocationCallbacks());

    RHI::DestroyGraphicsPipeline(device, renderer.wireframePipeline);
    RHI::DestroyGraphicsPipeline(device, renderer.oceanPipeline);
    RHI::DestroyGraphicsPipeline(device, renderer.skyPipeline);
    RHI::DestroyGraphicsPipeline(device, renderer.foamPipeline);

    RHI::DestroyBuffer(device, renderer.vertexBuffer);
    RHI::DestroyBuffer(device, renderer.indexBuffer);

    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        RHI::DestroyBuffer(device, renderer.uniformBuffers[i]);
        RHI::DestroyBuffer(device, renderer.shadeParamsBuffers[i]);
    }

    for (u32 i = 0; i < 2; i++)
    {
        RHI::DestroyTexture2D(device, renderer.foamTextures[i]);
    }
}

struct FoamPushConstants
{
    u32 heightMaps[4];
    u32 diffDisplacementMaps[4];
    u32 foamTextureIndex;
    f32 waveChopiness;
    f32 foamDecay;
    f32 foamGain;
    f32 deltaTime;
};

struct OceanPushConstants
{
    u32 uniformBufferIndex;
    u32 shadeParamsBufferIndex;
    u32 vertexBufferIndex;
    u32 heightMapCascades[4];
    u32 diffDisplacementCascades[4];
    u32 skyBoxTextureIndex;
    u32 foamTextureIndex;
    f32 waveChopiness;
};

void RecordOceanRendererCommands(RHI::Device& device,
                                 const OceanRendererInputs& inputs,
                                 OceanRenderer& renderer)
{
    // Render to foam texture
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    {
        u32 inFoamTextureIndex = renderer.foamTextureIndex;
        u32 outFoamTextureIndex = (renderer.foamTextureIndex + 1) % 2;
        RHI::Texture2D& inFoamTexture =
            renderer.foamTextures[inFoamTextureIndex];
        RHI::Texture2D& outFoamTexture =
            renderer.foamTextures[outFoamTextureIndex];

        RecordTransitionImageLayout(cmd, inFoamTexture.image,
                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        RecordTransitionImageLayout(cmd, outFoamTexture.image,
                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkRect2D renderArea = {{0, 0},
                               {outFoamTexture.width, outFoamTexture.height}};
        VkRenderingAttachmentInfo colorAttachment = RHI::ColorAttachmentInfo(
            outFoamTexture.imageView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingInfo renderInfo =
            RHI::RenderingInfo(renderArea, &colorAttachment, 1, nullptr);

        vkCmdBeginRendering(cmd.handle, &renderInfo);

        VkViewport viewport = {};
        viewport.x = 0;
        viewport.y = renderArea.extent.height;
        viewport.width = static_cast<f32>(renderArea.extent.width);
        viewport.height = -static_cast<f32>(renderArea.extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd.handle, 0, 1, &viewport);

        VkRect2D scissor = renderArea;
        vkCmdSetScissor(cmd.handle, 0, 1, &scissor);

        RHI::BindGraphicsPipeline(cmd, renderer.foamPipeline);

        FoamPushConstants pushConstants;
        pushConstants.foamTextureIndex = inFoamTexture.bindlessHandle;
        for (u32 i = 0; i < 4; i++)
        {
            pushConstants.heightMaps[i] = inputs.heightMaps[i];
            pushConstants.diffDisplacementMaps[i] =
                inputs.diffDisplacementMaps[i];
        }
        pushConstants.waveChopiness = renderer.waveChopiness;
        pushConstants.foamGain = renderer.foamGain;
        pushConstants.foamDecay = renderer.foamDecay;
        pushConstants.deltaTime = inputs.deltaTime;
        RHI::SetPushConstants(cmd, &pushConstants, sizeof(pushConstants));

        vkCmdDraw(cmd.handle, 6, 1, 0, 0);

        vkCmdEndRendering(cmd.handle);

        RecordTransitionImageLayout(cmd, outFoamTexture.image,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        renderer.foamTextureIndex = (renderer.foamTextureIndex + 1) % 2;
    }

    // Render to screen
    const RHI::SwapchainTexture& swapchainTexture =
        RenderFrameSwapchainTexture(device);
    VkRect2D renderArea = {{0, 0},
                           {swapchainTexture.width, swapchainTexture.height}};
    VkRenderingAttachmentInfo colorAttachment = RHI::ColorAttachmentInfo(
        swapchainTexture.imageView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = RHI::DepthAttachmentInfo(
        device.depthTexture.imageView,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo =
        RHI::RenderingInfo(renderArea, &colorAttachment, 1, &depthAttachment);

    vkCmdBeginRendering(cmd.handle, &renderInfo);

    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<f32>(renderArea.extent.width);
    viewport.height = static_cast<f32>(renderArea.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd.handle, 0, 1, &viewport);

    VkRect2D scissor = renderArea;
    vkCmdSetScissor(cmd.handle, 0, 1, &scissor);

    // Sky
    {
        RHI::BindGraphicsPipeline(cmd, renderer.skyPipeline);
        u32 pushConstants[] = {
            renderer.uniformBuffers[device.frameIndex].bindlessHandle,
            /*renderer.foamTextures[renderer.foamTextureIndex].bindlessHandle*/
            inputs.skyBox};
        RHI::SetPushConstants(cmd, pushConstants, sizeof(pushConstants));
        vkCmdDraw(cmd.handle, 6, 1, 0, 0);
    }

    // Ocean
    {
        RHI::BindGraphicsPipeline(cmd, *sCurrentPipeline);

        OceanPushConstants pushConstants;
        pushConstants.uniformBufferIndex =
            renderer.uniformBuffers[device.frameIndex].bindlessHandle;
        pushConstants.shadeParamsBufferIndex =
            renderer.shadeParamsBuffers[device.frameIndex].bindlessHandle;
        pushConstants.vertexBufferIndex = renderer.vertexBuffer.bindlessHandle;
        for (u32 i = 0; i < 4; i++)
        {
            pushConstants.heightMapCascades[i] = inputs.heightMaps[i];
            pushConstants.diffDisplacementCascades[i] =
                inputs.diffDisplacementMaps[i];
        }
        pushConstants.skyBoxTextureIndex = inputs.skyBox;
        pushConstants.foamTextureIndex =
            renderer.foamTextures[renderer.foamTextureIndex].bindlessHandle;
        pushConstants.waveChopiness = renderer.waveChopiness;
        RHI::SetPushConstants(cmd, &pushConstants, sizeof(pushConstants));

        vkCmdBindIndexBuffer(cmd.handle, renderer.indexBuffer.handle, 0,
                             VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd.handle, renderer.indexCount, 1, 0, 0, 0);
    }

    vkCmdEndRendering(cmd.handle);
}

void UpdateOceanRendererUniforms(RHI::Device& device,
                                 const SimpleCameraFPS& camera, u32 width,
                                 u32 height, OceanRenderer& renderer)
{
    UniformData data;
    data.projection = camera.GetProjection();
    data.view = camera.GetView();
    data.screenSize =
        Math::Vec4(static_cast<f32>(width), static_cast<f32>(height),
                   1.0f / width, 1.0f / height);
    RHI::CopyDataToBuffer(device, &data, sizeof(UniformData), 0,
                          renderer.uniformBuffers[device.frameIndex]);

    ShadeParams shadeParams;
    shadeParams.lightColorReflectivity =
        Math::Vec4(renderer.lightColor, renderer.reflectivity);
    shadeParams.waterScatterColor =
        Math::Vec4(renderer.waterScatterColor, 0.0f);
    shadeParams.coefficients =
        Math::Vec4(renderer.ss1, renderer.ss2, renderer.a1, renderer.a2);
    shadeParams.bubbleColorDensity =
        Math::Vec4(renderer.bubbleColor, renderer.bubbleDensity);
    RHI::CopyDataToBuffer(device, &shadeParams, sizeof(ShadeParams), 0,
                          renderer.shadeParamsBuffers[device.frameIndex]);
}

void ToggleWireframeOceanRenderer(OceanRenderer& renderer)
{
    if (sCurrentPipeline == &renderer.oceanPipeline)
    {
        sCurrentPipeline = &renderer.wireframePipeline;
    }
    else
    {
        sCurrentPipeline = &renderer.oceanPipeline;
    }
}

} // namespace Fly
