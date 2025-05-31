#include "core/clock.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "rhi/buffer.h"
#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"

#include "demos/common/scene.h"
#include "demos/common/simple_camera_fps.h"

#include <GLFW/glfw3.h>

using namespace Fly;

struct UniformData
{
    Math::Mat4 projection;
    Math::Mat4 view;
    Math::Mat4 cullView;
    f32 hTanX;
    f32 hTanY;
    f32 nearPlane;
    f32 farPlane;
};

struct DrawCommand
{
    u32 indexCount = 0;
    u32 instanceCount = 0;
    u32 firstIndex = 0;
    u32 vertexOffset = 0;
    u32 firstInstance = 0;
};

static RHI::Buffer sUniformBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sIndirectDrawBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sIndirectCountBuffers[FLY_FRAME_IN_FLIGHT_COUNT];

static u32 sDrawCount = 0;

static Fly::SimpleCameraFPS sCamera(85.0f, 1280.0f / 720.0f, 0.01f, 100.0f,
                                    Fly::Math::Vec3(0.0f, 0.0f, -5.0f));

static Fly::SimpleCameraFPS sTopCamera(85.0f, 1280.0f / 720.0f, 0.01f, 100.0f,
                                       Fly::Math::Vec3(0.0f, 20.0f, -5.0f));

static Fly::SimpleCameraFPS* sMainCamera = &sCamera;

static bool IsPhysicalDeviceSuitable(const RHI::Context& context,
                                     const RHI::PhysicalDeviceInfo& info)
{
    return info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

static void OnKeyboardPressed(GLFWwindow* window, int key, int scancode,
                              int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }

    if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
    {
        if (sMainCamera == &sCamera)
        {
            sMainCamera = &sTopCamera;
        }
        else
        {
            sMainCamera = &sCamera;
        }
    }
}

static void ErrorCallbackGLFW(i32 error, const char* description)
{
    FLY_ERROR("GLFW - error: %s", description);
}

static void RecordCommands(RHI::Device& device, RHI::GraphicsPipeline& pipeline,
                           RHI::ComputePipeline& cullPipeline,
                           const Fly::Scene& scene)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    // Culling pass
    vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                      cullPipeline.handle);
    vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                            cullPipeline.layout, 0, 1,
                            &device.bindlessDescriptorSet, 0, nullptr);
    u32 cullIndices[7] = {
        scene.indirectDrawData.instanceDataBuffer.bindlessHandle,
        scene.indirectDrawData.meshDataBuffer.bindlessHandle,
        sUniformBuffers[device.frameIndex].bindlessHandle,
        scene.indirectDrawData.boundingSphereDrawBuffer.bindlessHandle,
        sIndirectDrawBuffers[device.frameIndex].bindlessHandle,
        sIndirectCountBuffers[device.frameIndex].bindlessHandle,
        sDrawCount};

    vkCmdPushConstants(cmd.handle, cullPipeline.layout, VK_SHADER_STAGE_ALL, 0,
                       sizeof(u32) * 7, cullIndices);
    vkCmdDispatch(cmd.handle, static_cast<u32>(Math::Ceil(sDrawCount / 64.0f)),
                  1, 1);

    VkBufferMemoryBarrier barriers[2];
    barriers[0] = RHI::BufferMemoryBarrier(
        sIndirectDrawBuffers[device.frameIndex], VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
    barriers[1] = RHI::BufferMemoryBarrier(
        sIndirectCountBuffers[device.frameIndex], VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT);

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

    u32 indices[4] = {scene.indirectDrawData.instanceDataBuffer.bindlessHandle,
                      scene.indirectDrawData.meshDataBuffer.bindlessHandle,
                      sUniformBuffers[device.frameIndex].bindlessHandle,
                      scene.materialBuffer.bindlessHandle};
    vkCmdPushConstants(cmd.handle, pipeline.layout, VK_SHADER_STAGE_ALL, 0,
                       sizeof(u32) * 4, indices);

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
        FLY_ERROR("Failed to load volk");
        return -1;
    }
    glfwInitVulkanLoader(vkGetInstanceProcAddr);
    if (!glfwInit())
    {
        FLY_ERROR("Failed to init glfw");
        return -1;
    }
    glfwSetErrorCallback(ErrorCallbackGLFW);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window =
        glfwCreateWindow(1280, 720, "glTF Viewer", nullptr, nullptr);
    if (!window)
    {
        FLY_ERROR("Failed to create glfw window");
        glfwTerminate();
        return -1;
    }
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetKeyCallback(window, OnKeyboardPressed);

    // Create graphics context
    const char* requiredDeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    RHI::ContextSettings settings{};
    settings.isPhysicalDeviceSuitableCallback = IsPhysicalDeviceSuitable;
    settings.deviceFeatures2.features.samplerAnisotropy = VK_TRUE;
    settings.instanceExtensions =
        glfwGetRequiredInstanceExtensions(&settings.instanceExtensionCount);
    settings.deviceExtensions = requiredDeviceExtensions;
    settings.deviceExtensionCount = STACK_ARRAY_COUNT(requiredDeviceExtensions);
    settings.windowPtr = window;

    RHI::Context context;
    if (!RHI::CreateContext(settings, context))
    {
        FLY_ERROR("Failed to create context");
        return -1;
    }
    RHI::Device& device = context.devices[0];

    // Scene
    Fly::Scene scene;
    if (!Fly::LoadSceneFromGLTF(arena, device, "BoxTextured.gltf", scene))
    {
        FLY_ERROR("Failed to load gltf");
        return -1;
    }

    // Hack
    RHI::DestroyBuffer(device, scene.indirectDrawData.instanceDataBuffer);

    sDrawCount = 30000;
    ArenaMarker marker = ArenaGetMarker(arena);
    InstanceData* instanceData = FLY_PUSH_ARENA(arena, InstanceData, sDrawCount);

    for (u32 i = 0; i < sDrawCount; i++)
    {
        f32 randomX = Math::RandomF32(-100.0f, 100.0f);
        f32 randomY = Math::RandomF32(-2.0f, 2.0f);
        f32 randomZ = Math::RandomF32(0.0f, 100.0f);

        instanceData[i].model =
            Math::TranslationMatrix(randomX, randomY, randomZ);
        instanceData[i].meshDataIndex = 0;
    }
    if (!RHI::CreateStorageBuffer(device, false, instanceData,
                                  sizeof(InstanceData) * sDrawCount,
                                  scene.indirectDrawData.instanceDataBuffer))
    {
        return -1;
    }
    ArenaPopToMarker(arena, marker);

    // Create buffer - that holds draw commands, and another that holds
    // drawCount
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateIndirectBuffer(device, false, nullptr,
                                       sizeof(DrawCommand) * sDrawCount,
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
    if (!Fly::LoadShaderFromSpv(device, "unlit.vert.spv",
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return -1;
    }
    if (!Fly::LoadShaderFromSpv(device, "unlit.frag.spv",
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
    fixedState.rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    fixedState.rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

    RHI::GraphicsPipeline graphicsPipeline{};
    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     graphicsPipeline))
    {
        FLY_ERROR("Failed to create graphics pipeline");
        return -1;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    // Compute pipeline
    RHI::Shader cullShader;
    if (!Fly::LoadShaderFromSpv(device, "cull.comp.spv", cullShader))
    {
        return -1;
    }
    RHI::ComputePipeline cullPipeline{};
    if (!RHI::CreateComputePipeline(device, cullShader, cullPipeline))
    {
        FLY_ERROR("Failed to create cull compute pipeline");
        return -1;
    }
    RHI::DestroyShader(device, cullShader);

    // Camera data
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateUniformBuffer(device, nullptr, sizeof(UniformData),
                                      sUniformBuffers[i]))
        {
            FLY_ERROR("Failed to create uniform buffer!");
        }
    }

    // Main Loop
    u64 previousFrameTime = 0;
    u64 loopStartTime = Fly::ClockNow();
    u64 currentFrameTime = loopStartTime;
    sTopCamera.SetPitch(-89.0f);
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        previousFrameTime = currentFrameTime;
        currentFrameTime = Fly::ClockNow();

        f32 time =
            static_cast<f32>(Fly::ToSeconds(currentFrameTime - loopStartTime));
        f64 deltaTime = Fly::ToSeconds(currentFrameTime - previousFrameTime);

        sCamera.Update(window, deltaTime);
        sTopCamera.SetPosition(sCamera.GetPosition() +
                               Math::Vec3(0.0f, 20.0f, 0.0f));

        UniformData uniformData;
        uniformData.projection = sMainCamera->GetProjection();
        uniformData.view = sMainCamera->GetView();
        uniformData.cullView = sCamera.GetView();
        uniformData.hTanX =
            Math::Tan(Math::Radians(sMainCamera->GetHorizontalFov()) * 0.5f);
        uniformData.hTanY =
            Math::Tan(Math::Radians(sMainCamera->GetVerticalFov()) * 0.5f);
        uniformData.nearPlane = sMainCamera->GetNear();
        uniformData.farPlane = sMainCamera->GetFar();

        RHI::CopyDataToBuffer(device, &uniformData, sizeof(UniformData), 0,
                              sUniformBuffers[device.frameIndex]);

        u32 count = 0;
        RHI::CopyDataToBuffer(device, &count, sizeof(u32), 0,
                              sIndirectCountBuffers[device.frameIndex]);

        RHI::BeginRenderFrame(device);
        RecordCommands(device, graphicsPipeline, cullPipeline, scene);
        RHI::EndRenderFrame(device);
    }

    RHI::WaitAllDevicesIdle(context);

    Fly::UnloadScene(device, scene);
    RHI::DestroyComputePipeline(device, cullPipeline);
    RHI::DestroyGraphicsPipeline(device, graphicsPipeline);
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        RHI::DestroyBuffer(device, sIndirectDrawBuffers[i]);
        RHI::DestroyBuffer(device, sIndirectCountBuffers[i]);
        RHI::DestroyBuffer(device, sUniformBuffers[i]);
    }
    RHI::DestroyContext(context);

    glfwDestroyWindow(window);
    glfwTerminate();
    FLY_LOG("Shutdown successful");

    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
