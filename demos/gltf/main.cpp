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
    Math::Mat4 projection = {};
    Math::Mat4 view = {};
};

static RHI::Buffer sUniformBuffers[FLY_FRAME_IN_FLIGHT_COUNT];

static Fly::SimpleCameraFPS sCamera(80.0f, 1280.0f / 720.0f, 0.01f, 100.0f,
                                    Fly::Math::Vec3(0.0f, 0.0f, -5.0f));

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
    FLY_ERROR("GLFW - error: %s", description);
}

static void RecordCommands(RHI::Device& device, RHI::GraphicsPipeline& pipeline,
                           const Fly::Scene& scene)
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
    RHI::BindGraphicsPipeline(device, cmd, pipeline);

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

    vkCmdBindIndexBuffer(cmd.handle, scene.indexBuffer.handle, 0,
                         VK_INDEX_TYPE_UINT32);

    u32 globalIndices[2] = {sUniformBuffers[device.frameIndex].bindlessHandle,
                            scene.materialBuffer.bindlessHandle};
    RHI::SetPushConstants(device, cmd, globalIndices, sizeof(globalIndices),
                          sizeof(Math::Mat4));
    for (u32 i = 0; i < scene.meshNodeCount; i++)
    {
        const Fly::MeshNode& meshNode = scene.meshNodes[i];
        RHI::SetPushConstants(device, cmd, meshNode.model.data,
                              sizeof(meshNode.model.data));
        for (u32 j = 0; j < meshNode.mesh->submeshCount; j++)
        {
            const Fly::Submesh& submesh = meshNode.mesh->submeshes[j];
            u32 localIndices[2] = {submesh.vertexBufferIndex,
                                   submesh.materialIndex};
            RHI::SetPushConstants(device, cmd, localIndices,
                                  sizeof(localIndices),
                                  sizeof(Math::Mat4) + sizeof(u32) * 2);
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

    // Camera data
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateUniformBuffer(device, nullptr, sizeof(UniformData),
                                      sUniformBuffers[i]))
        {
            FLY_ERROR("Failed to create uniform buffer!");
            return -1;
        }
    }

    // Scene
    Fly::Scene scene;
    if (!Fly::LoadSceneFromGLTF(arena, device, "Sponza.gltf", scene))
    {
        FLY_ERROR("Failed to load gltf");
        return -1;
    }

    u64 previousFrameTime = 0;
    u64 loopStartTime = Fly::ClockNow();
    u64 currentFrameTime = loopStartTime;
    while (!glfwWindowShouldClose(window))
    {
        previousFrameTime = currentFrameTime;
        currentFrameTime = Fly::ClockNow();

        f32 time =
            static_cast<f32>(Fly::ToSeconds(currentFrameTime - loopStartTime));
        f64 deltaTime = Fly::ToSeconds(currentFrameTime - previousFrameTime);

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

    Fly::UnloadScene(device, scene);
    RHI::DestroyGraphicsPipeline(device, graphicsPipeline);
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
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
