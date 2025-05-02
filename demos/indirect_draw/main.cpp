#include "core/clock.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "rhi/buffer.h"
#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"

#include "platform/window.h"

#include "demos/common/scene.h"
#include "demos/common/simple_camera_fps.h"

using namespace Hls;

struct UniformData
{
    Math::Mat4 projection = {};
    Math::Mat4 view = {};
};

struct DrawCommand
{
    u32 indexCount = 0;
    u32 instanceCount = 0;
    u32 firstIndex = 0;
    u32 vertexOffset = 0;
    u32 firstInstance = 0;
    u32 vertexBufferIndex = 0;
    u32 materialIndex = 0;
};

struct DrawData
{
    u32 indexCount = 0;
    u32 firstIndex = 0;
    u32 vertexBufferIndex = 0;
    u32 materialIndex = 0;
};

static RHI::Buffer sUniformBuffers[HLS_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sIndirectDrawBuffers[HLS_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sIndirectCountBuffers[HLS_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sDrawDataBuffer;
static u32 sDrawCount = 0;

static Hls::SimpleCameraFPS
    sCamera(Hls::Math::Perspective(45.0f, 1280.0f / 720.0f, 0.01f, 100.0f),
            Hls::Math::Vec3(0.0f, 0.0f, -5.0f));

static void OnKeyboardPressed(GLFWwindow* window, int key, int scancode,
                              int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }
}

static void ErrorCallbackGLFW(i32 error, const char* description)
{
    HLS_ERROR("GLFW - error: %s", description);
}

static void RecordCommands(RHI::Device& device, RHI::GraphicsPipeline& pipeline,
                           RHI::ComputePipeline& cullPipeline,
                           const Hls::Scene& scene)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    // Culling pass
    vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                      cullPipeline.handle);
    vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                            cullPipeline.layout, 0, 1,
                            &device.bindlessDescriptorSet, 0, nullptr);
    u32 cullIndices[4] = {
        sDrawDataBuffer.bindlessHandle,
        sIndirectDrawBuffers[device.frameIndex].bindlessHandle,
        sIndirectCountBuffers[device.frameIndex].bindlessHandle, sDrawCount};
    vkCmdPushConstants(cmd.handle, cullPipeline.layout, VK_SHADER_STAGE_ALL, 0,
                       sizeof(u32) * 4, cullIndices);

    vkCmdDispatch(cmd.handle, static_cast<u32>(Math::Ceil(sDrawCount / 64.0f)),
                  1, 1);

    VkBufferMemoryBarrier barriers[2];
    barriers[0] = {};
    barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barriers[0].dstAccessMask =
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    barriers[0].srcQueueFamilyIndex = device.graphicsComputeQueueFamilyIndex;
    barriers[0].dstQueueFamilyIndex = device.graphicsComputeQueueFamilyIndex;
    barriers[0].buffer = sIndirectDrawBuffers[device.frameIndex].handle;
    barriers[0].offset = 0;
    barriers[0].size = VK_WHOLE_SIZE;

    barriers[1] = {};
    barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barriers[1].dstAccessMask =
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    barriers[1].srcQueueFamilyIndex = device.graphicsComputeQueueFamilyIndex;
    barriers[1].dstQueueFamilyIndex = device.graphicsComputeQueueFamilyIndex;
    barriers[1].buffer = sIndirectCountBuffers[device.frameIndex].handle;
    barriers[1].offset = 0;
    barriers[1].size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT |
                             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 2, barriers, 0, nullptr);

    // Graphics pass
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
    vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline.handle);

    vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline.layout, 0, 1,
                            &device.bindlessDescriptorSet, 0, nullptr);
    vkCmdBindIndexBuffer(cmd.handle, scene.indexBuffer.handle, 0,
                         VK_INDEX_TYPE_UINT32);

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

    u32 indices[3] = {sUniformBuffers[device.frameIndex].bindlessHandle,
                      scene.materialBuffer.bindlessHandle,
                      sIndirectDrawBuffers[device.frameIndex].bindlessHandle};
    vkCmdPushConstants(cmd.handle, pipeline.layout, VK_SHADER_STAGE_ALL, 0,
                       sizeof(u32) * 3, indices);

    vkCmdDrawIndexedIndirectCount(
        cmd.handle, sIndirectDrawBuffers[device.frameIndex].handle, 0,
        sIndirectCountBuffers[device.frameIndex].handle, 0, sDrawCount,
        sizeof(DrawCommand));
    vkCmdEndRendering(cmd.handle);
}

int main(int argc, char* argv[])
{
    InitThreadContext();
    Arena& arena = GetScratchArena();
    if (!InitLogger())
    {
        return -1;
    }

    // Initialize volk, window
    if (volkInitialize() != VK_SUCCESS)
    {
        HLS_ERROR("Failed to load volk");
        return -1;
    }
    glfwInitVulkanLoader(vkGetInstanceProcAddr);
    if (!glfwInit())
    {
        HLS_ERROR("Failed to init glfw");
        return -1;
    }
    glfwSetErrorCallback(ErrorCallbackGLFW);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window =
        glfwCreateWindow(1280, 720, "glTF Viewer", nullptr, nullptr);
    if (!window)
    {
        HLS_ERROR("Failed to create glfw window");
        glfwTerminate();
        return -1;
    }
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetKeyCallback(window, OnKeyboardPressed);

    // Create graphics context
    const char* requiredDeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    RHI::ContextSettings settings{};
    settings.deviceFeatures2.features.samplerAnisotropy = VK_TRUE;
    settings.instanceExtensions =
        glfwGetRequiredInstanceExtensions(&settings.instanceExtensionCount);
    settings.deviceExtensions = requiredDeviceExtensions;
    settings.deviceExtensionCount = STACK_ARRAY_COUNT(requiredDeviceExtensions);
    settings.windowPtr = Hls::GetNativeWindowPtr(window);

    RHI::Context context;
    if (!RHI::CreateContext(settings, context))
    {
        HLS_ERROR("Failed to create context");
        return -1;
    }
    RHI::Device& device = context.devices[0];

    // Create buffer - that holds draw commands, and another that holds
    // drawCount
    for (u32 i = 0; i < HLS_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateIndirectBuffer(device, false, nullptr,
                                       sizeof(DrawCommand) * 10000,
                                       sIndirectDrawBuffers[i]))
        {
            return -1;
        }

        u32 count = 0;
        if (!RHI::CreateIndirectBuffer(device, false, &count, sizeof(u32),
                                       sIndirectCountBuffers[i]))
        {
            return -1;
        }
    }

    // Pipeline
    RHI::ShaderProgram shaderProgram{};
    if (!Hls::LoadShaderFromSpv(device, "unlit.vert.spv",
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return -1;
    }
    if (!Hls::LoadShaderFromSpv(device, "unlit.frag.spv",
                                shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return -1;
    }

    RHI::GraphicsPipelineFixedStateStage fixedState{};
    fixedState.pipelineRendering.colorAttachments[0] =
        device.surfaceFormat.format;
    fixedState.pipelineRendering.depthAttachmentFormat =
        device.depthTexture.format;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.colorBlendState.attachmentCount = 1;
    fixedState.depthStencilState.depthTestEnable = true;
    fixedState.rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
    fixedState.rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

    RHI::GraphicsPipeline graphicsPipeline{};
    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     graphicsPipeline))
    {
        HLS_ERROR("Failed to create graphics pipeline");
        return -1;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    // Compute pipeline
    RHI::Shader cullShader;
    if (!Hls::LoadShaderFromSpv(device, "cull.comp.spv", cullShader))
    {
        return -1;
    }
    RHI::ComputePipeline cullPipeline{};
    if (!RHI::CreateComputePipeline(device, cullShader, cullPipeline))
    {
        HLS_ERROR("Failed to create cull compute pipeline");
        return -1;
    }
    RHI::DestroyShader(device, cullShader);

    // Scene
    Hls::Scene scene;
    if (!Hls::LoadSceneFromGLTF(arena, device, "Sponza.gltf", scene))
    {
        HLS_ERROR("Failed to load gltf");
        return -1;
    }

    // Camera data
    for (u32 i = 0; i < HLS_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateUniformBuffer(device, nullptr, sizeof(UniformData),
                                      sUniformBuffers[i]))
        {
            HLS_ERROR("Failed to create uniform buffer!");
        }
    }

    // Fill indirect draw buffer
    ArenaMarker marker = ArenaGetMarker(arena);
    for (u64 i = 0; i < scene.meshCount; i++)
    {
        sDrawCount += scene.meshes[i].submeshCount;
    }
    DrawData* drawDataBuffer = HLS_ALLOC(arena, DrawData, sDrawCount);
    u64 index = 0;
    for (u64 i = 0; i < scene.meshCount; i++)
    {
        for (u64 j = 0; j < scene.meshes[i].submeshCount; j++)
        {
            Hls::Submesh& submesh = scene.meshes[i].submeshes[j];
            DrawData& drawData = drawDataBuffer[index++];
            drawData.indexCount = submesh.indexCount;
            drawData.firstIndex = submesh.indexOffset;
            drawData.vertexBufferIndex = submesh.vertexBuffer.bindlessHandle;
            drawData.materialIndex = submesh.materialIndex;
        }
    }
    if (!RHI::CreateStorageBuffer(device, false, drawDataBuffer,
                                  sizeof(DrawData) * sDrawCount,
                                  sDrawDataBuffer))
    {
        return -1;
    }
    ArenaPopToMarker(arena, marker);

    // Main Loop
    u64 previousFrameTime = 0;
    u64 loopStartTime = Hls::ClockNow();
    u64 currentFrameTime = loopStartTime;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        previousFrameTime = currentFrameTime;
        currentFrameTime = Hls::ClockNow();

        f32 time =
            static_cast<f32>(Hls::ToSeconds(currentFrameTime - loopStartTime));
        f64 deltaTime = Hls::ToSeconds(currentFrameTime - previousFrameTime);

        sCamera.Update(window, deltaTime);
        UniformData uniformData = {sCamera.GetProjection(), sCamera.GetView()};
        RHI::CopyDataToBuffer(device, &uniformData, sizeof(UniformData), 0,
                              sUniformBuffers[device.frameIndex]);

        u32 count = 0;
        RHI::CopyDataToBuffer(device, &count, sizeof(u32), 0,
                              sIndirectCountBuffers[device.frameIndex]);

        RHI::BeginRenderFrame(device);
        RecordCommands(device, graphicsPipeline, cullPipeline, scene);
        RHI::EndRenderFrame(device);

        // RHI::DeviceWaitIdle(device);
        //  for (u32 i = 0; i < sDrawCount; i++)
        //  {
        //      DrawCommand* dc = (static_cast<DrawCommand*>(
        //                             sIndirectDrawBuffers[device.frameIndex]
        //                                 .allocationInfo.pMappedData) +
        //                         i);
        //      HLS_LOG("%u %u %u %u %u %u %u", dc->indexCount,
        //      dc->instanceCount,
        //              dc->firstIndex, dc->vertexOffset, dc->firstInstance,
        //              dc->vertexBufferIndex, dc->materialIndex);
        //  }
        //  HLS_LOG("Count %u",
        //          *(static_cast<u32*>(sIndirectCountBuffers[device.frameIndex]
        //                                  .allocationInfo.pMappedData)));
    }

    RHI::WaitAllDevicesIdle(context);

    Hls::UnloadScene(device, scene);
    RHI::DestroyBuffer(device, sDrawDataBuffer);
    RHI::DestroyComputePipeline(device, cullPipeline);
    RHI::DestroyGraphicsPipeline(device, graphicsPipeline);
    for (u32 i = 0; i < HLS_FRAME_IN_FLIGHT_COUNT; i++)
    {
        RHI::DestroyBuffer(device, sIndirectDrawBuffers[i]);
        RHI::DestroyBuffer(device, sIndirectCountBuffers[i]);
        RHI::DestroyBuffer(device, sUniformBuffers[i]);
    }
    RHI::DestroyContext(context);

    glfwDestroyWindow(window);
    glfwTerminate();
    HLS_LOG("Shutdown successful");

    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
