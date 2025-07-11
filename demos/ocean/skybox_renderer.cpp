#include "skybox_renderer.h"

#include "demos/common/scene.h"

namespace Fly
{

bool CreateSkyBoxRenderer(RHI::Device& device, u32 resolution,
                          SkyBoxRenderer& renderer)
{
    renderer.resolution = 256;

    RHI::GraphicsPipelineFixedStateStage fixedState{};
    fixedState.depthStencilState.depthTestEnable = false;
    fixedState.pipelineRendering.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    fixedState.pipelineRendering.colorAttachments[0] = VK_FORMAT_R8G8B8A8_SRGB;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.pipelineRendering.viewMask = 0x3F;
    fixedState.colorBlendState.attachmentCount = 1;
    fixedState.rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    fixedState.rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    fixedState.rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    fixedState.inputAssemblyState.topology =
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    RHI::ShaderProgram shaderProgram{};
    if (!Fly::LoadShaderFromSpv(device, "screen_quad.vert.spv",
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }

    if (!Fly::LoadShaderFromSpv(device, "skybox.frag.spv",
                                shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
    }

    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     renderer.skyBoxPipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateCubemap(device, nullptr, 256 * 256 * 6 * 4 * sizeof(u8),
                                256, VK_FORMAT_R8G8B8A8_SRGB,
                                RHI::Sampler::FilterMode::Bilinear,
                                renderer.skyBoxes[i]))
        {
            return false;
        }
    }

    return true;
}

void DestroySkyBoxRenderer(RHI::Device& device, SkyBoxRenderer& renderer)
{
    RHI::DestroyGraphicsPipeline(device, renderer.skyBoxPipeline);
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        RHI::DestroyCubemap(device, renderer.skyBoxes[i]);
    }
}

void RecordSkyBoxRendererCommands(RHI::Device& device, SkyBoxRenderer& renderer)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    RecordTransitionImageLayout(cmd, renderer.skyBoxes[device.frameIndex].image,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRect2D renderArea = {{0, 0}, {256, 256}};
    VkRenderingAttachmentInfo colorAttachment = RHI::ColorAttachmentInfo(
        renderer.skyBoxes[device.frameIndex].arrayImageView,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = RHI::RenderingInfo(
        renderArea, &colorAttachment, 1, nullptr, nullptr, 6, 0x3F);

    vkCmdBeginRendering(cmd.handle, &renderInfo);
    RHI::BindGraphicsPipeline(cmd, renderer.skyBoxPipeline);

    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<f32>(256);
    viewport.height = static_cast<f32>(256);
    vkCmdSetViewport(cmd.handle, 0, 1, &viewport);

    VkRect2D scissor = renderArea;
    vkCmdSetScissor(cmd.handle, 0, 1, &scissor);

    vkCmdDraw(cmd.handle, 6, 1, 0, 0);
    vkCmdEndRendering(cmd.handle);

    RecordTransitionImageLayout(cmd, renderer.skyBoxes[device.frameIndex].image,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // VkImageMemoryBarrier imageBarrier{};
    // imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // imageBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    // imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    // imageBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    // imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    // imageBarrier.image = renderer.skyBoxes[device.frameIndex].image;
    // imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    // imageBarrier.subresourceRange.baseMipLevel = 0;
    // imageBarrier.subresourceRange.levelCount = 1; // or all mips if needed
    // imageBarrier.subresourceRange.baseArrayLayer = 0;
    // imageBarrier.subresourceRange.layerCount = 6; // all cubemap faces

    // vkCmdPipelineBarrier(
    //     cmd.handle,
    //     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStage
    //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,         // dstStage
    //     0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
}

} // namespace Fly
