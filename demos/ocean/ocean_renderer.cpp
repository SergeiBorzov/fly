#include "ocean_renderer.h"

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
    }

    return true;
}

void DestroyOceanRenderer(RHI::Device& device, OceanRenderer& renderer)
{
    RHI::DestroyGraphicsPipeline(device, renderer.wireframePipeline);
    RHI::DestroyGraphicsPipeline(device, renderer.oceanPipeline);
    RHI::DestroyGraphicsPipeline(device, renderer.skyPipeline);

    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        RHI::DestroyBuffer(device, renderer.uniformBuffers[i]);
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

    // // Ocean
    // {
    //     RHI::BindGraphicsPipeline(device, cmd, *sCurrentGraphicsPipeline);
    //     u32 pushConstants[] = {
    //         sUniformBuffers[device.frameIndex].bindlessHandle,
    //         sDiffDisplacementMaps[device.frameIndex].bindlessHandle,
    //         sHeightMaps[device.frameIndex].bindlessHandle,
    //         sVertexBuffer.bindlessHandle,
    //         sSkyBoxes[device.frameIndex].bindlessHandle};
    //     RHI::SetPushConstants(device, cmd, pushConstants,
    //                           sizeof(pushConstants));

    //     vkCmdBindIndexBuffer(cmd.handle, sIndexBuffer.handle, 0,
    //                          VK_INDEX_TYPE_UINT32);
    //     vkCmdDrawIndexed(cmd.handle, sIndexCount, 1, 0, 0, 0);
    // }

    // ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd.handle);

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
}

} // namespace Fly
