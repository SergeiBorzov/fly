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

static RHI::Buffer sUniformBuffers[HLS_FRAME_IN_FLIGHT_COUNT];

static Hls::SimpleCameraFPS
    sCamera(Hls::Math::Perspective(45.0f, 1280.0f / 720.0f, 0.01f, 100.0f),
            Hls::Math::Vec3(0.0f, 0.0f, -5.0f));

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
}

static void ErrorCallbackGLFW(i32 error, const char* description)
{
    HLS_ERROR("GLFW - error: %s", description);
}

static void RecordCommands(RHI::Device& device, RHI::GraphicsPipeline& pipeline,
                           const Hls::Scene& scene)
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
    vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline.handle);

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

    vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline.layout, 0, 1,
                            &device.bindlessDescriptorSet, 0, nullptr);
    vkCmdBindIndexBuffer(cmd.handle, scene.indexBuffer.handle, 0,
                         VK_INDEX_TYPE_UINT32);

    u32 globalIndices[2] = {sUniformBuffers[device.frameIndex].bindlessHandle,
                            scene.materialBuffer.bindlessHandle};
    vkCmdPushConstants(cmd.handle, pipeline.layout, VK_SHADER_STAGE_ALL,
                       sizeof(Math::Mat4), sizeof(u32) * 2, globalIndices);
    for (u32 i = 0; i < scene.meshNodeCount; i++)
    {
        const Hls::MeshNode& meshNode = scene.meshNodes[i];
        vkCmdPushConstants(cmd.handle, pipeline.layout, VK_SHADER_STAGE_ALL, 0,
                           sizeof(Math::Mat4), meshNode.model.data);
        for (u32 j = 0; j < meshNode.mesh->submeshCount; j++)
        {
            const Hls::Submesh& submesh = meshNode.mesh->submeshes[j];
            u32 localIndices[2] = {submesh.vertexBufferIndex,
                                   submesh.materialIndex};
            vkCmdPushConstants(cmd.handle, pipeline.layout, VK_SHADER_STAGE_ALL,
                               sizeof(Math::Mat4) + sizeof(u32) * 2,
                               sizeof(u32) * 2, localIndices);
            vkCmdDrawIndexed(cmd.handle, submesh.indexCount, 1,
                             submesh.indexOffset, 0, 0);
        }
    }

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
    settings.isPhysicalDeviceSuitableCallback = IsPhysicalDeviceSuitable;
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
    fixedState.rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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

    // Camera data
    for (u32 i = 0; i < HLS_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateUniformBuffer(device, nullptr, sizeof(UniformData),
                                      sUniformBuffers[i]))
        {
            HLS_ERROR("Failed to create uniform buffer!");
            return -1;
        }
    }

    // Scene
    Hls::Scene scene;
    if (!Hls::LoadSceneFromGLTF(arena, device, "Sponza.gltf", scene))
    {
        HLS_ERROR("Failed to load gltf");
        return -1;
    }

    u64 previousFrameTime = 0;
    u64 loopStartTime = Hls::ClockNow();
    u64 currentFrameTime = loopStartTime;
    while (!glfwWindowShouldClose(window))
    {
        previousFrameTime = currentFrameTime;
        currentFrameTime = Hls::ClockNow();

        f32 time =
            static_cast<f32>(Hls::ToSeconds(currentFrameTime - loopStartTime));
        f64 deltaTime = Hls::ToSeconds(currentFrameTime - previousFrameTime);

        glfwPollEvents();

        sCamera.Update(window, deltaTime);
        UniformData uniformData = {sCamera.GetProjection(), sCamera.GetView()};
        RHI::CopyDataToBuffer(device, &uniformData, sizeof(UniformData), 0,
                              sUniformBuffers[device.frameIndex]);

        RHI::BeginRenderFrame(device);
        RecordCommands(device, graphicsPipeline, scene);
        RHI::EndRenderFrame(device);
    }

    RHI::WaitAllDevicesIdle(context);

    Hls::UnloadScene(device, scene);
    RHI::DestroyGraphicsPipeline(device, graphicsPipeline);
    for (u32 i = 0; i < HLS_FRAME_IN_FLIGHT_COUNT; i++)
    {
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
