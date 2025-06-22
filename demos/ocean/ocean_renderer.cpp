#include "ocean_renderer.h"

#include "core/log.h"
#include "core/thread_context.h"

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

bool CreateOceanRenderer(RHI::Device& device, OceanRenderer& renderer)
{
    RHI::GraphicsPipelineFixedStateStage fixedState{};
    fixedState.pipelineRendering.colorAttachments[0] =
        device.surfaceFormat.format;
    fixedState.pipelineRendering.depthAttachmentFormat =
        device.depthTexture.format;
    fixedState.depthStencilState.depthTestEnable = true;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.colorBlendState.attachmentCount = 1;
    fixedState.rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    fixedState.rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    fixedState.rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    fixedState.inputAssemblyState.topology =
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    RHI::ShaderProgram shaderProgram{};
    if (!Fly::LoadShaderFromSpv(device, "ocean.vert.spv",
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }
    if (!Fly::LoadShaderFromSpv(device, "ocean.frag.spv",
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
    if (!Fly::LoadShaderFromSpv(device, "screen_quad.vert.spv",
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }
    if (!Fly::LoadShaderFromSpv(device, "sky.frag.spv",
                                shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
    }
    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     renderer.skyPipeline))
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

    if (!CreateGrid(device, renderer))
    {
        return false;
    }

    renderer.waterScatterColor = Math::Vec3(0.2f, 1.0f, 1.0f);
    renderer.lightColor = Math::Vec3(1.0f, 0.843f, 0.702f);
    renderer.bubbleColor = Math::Vec3(1.0f);
    renderer.ss1 = 24.0f;
    renderer.ss2 = 0.1f;
    renderer.a1 = 0.01f;
    renderer.a2 = 0.1f;
    renderer.reflectivity = 1.0f;
    renderer.bubbleDensity = 0.5f;

    return true;
}

void DestroyOceanRenderer(RHI::Device& device, OceanRenderer& renderer)
{
    RHI::DestroyGraphicsPipeline(device, renderer.wireframePipeline);
    RHI::DestroyGraphicsPipeline(device, renderer.oceanPipeline);
    RHI::DestroyGraphicsPipeline(device, renderer.skyPipeline);

    RHI::DestroyBuffer(device, renderer.vertexBuffer);
    RHI::DestroyBuffer(device, renderer.indexBuffer);

    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        RHI::DestroyBuffer(device, renderer.uniformBuffers[i]);
        RHI::DestroyBuffer(device, renderer.shadeParamsBuffers[i]);
    }
}

void RecordOceanRendererCommands(RHI::Device& device,
                                 const OceanRendererInputs& inputs,
                                 OceanRenderer& renderer)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

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
        RHI::BindGraphicsPipeline(device, cmd, renderer.skyPipeline);
        u32 pushConstants[] = {
            renderer.uniformBuffers[device.frameIndex].bindlessHandle,
            inputs.skyBox};
        RHI::SetPushConstants(device, cmd, pushConstants,
                              sizeof(pushConstants));
        vkCmdDraw(cmd.handle, 6, 1, 0, 0);
    }

    // Ocean
    {
        RHI::BindGraphicsPipeline(device, cmd, renderer.oceanPipeline);
        u32 pushConstants[] = {
            renderer.uniformBuffers[device.frameIndex].bindlessHandle,
            renderer.shadeParamsBuffers[device.frameIndex].bindlessHandle,
            renderer.vertexBuffer.bindlessHandle};
        RHI::SetPushConstants(device, cmd, pushConstants,
                              sizeof(pushConstants));
        RHI::SetPushConstants(device, cmd, &inputs, sizeof(inputs),
                              sizeof(pushConstants));
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

} // namespace Fly
