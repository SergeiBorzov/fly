#include "core/clock.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"

#include "demos/common/scene.h"
#include "demos/common/simple_camera_fps.h"

#include <GLFW/glfw3.h>

using namespace Fly;

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

struct UniformData
{
    Math::Mat4 projection;       // 0
    Math::Mat4 view;             // 64
    Math::Vec4 viewport;         // 128
    Math::Vec4 cameraParameters; // 144
    Math::Vec4 time;             // 160
};

static Fly::SimpleCameraFPS
    sCamera(80.0f, static_cast<f32>(WINDOW_WIDTH) / WINDOW_HEIGHT, 0.01f,
            100.0f, Fly::Math::Vec3(0.0f, 0.0f, -5.0f));

static bool IsPhysicalDeviceSuitable(const RHI::Context& context,
                                     const RHI::PhysicalDeviceInfo& info)
{
    return info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

static void ErrorCallbackGLFW(i32 error, const char* description)
{
    FLY_ERROR("GLFW - error: %s", description);
}

static RHI::ComputePipeline sFFTPipeline;
static RHI::GraphicsPipeline sGraphicsPipeline;
static bool CreatePipelines(RHI::Device& device)
{
    RHI::ComputePipeline* computePipelines[] = {
        &sFFTPipeline,
    };

    const char* computeShaderPaths[] = {
        "fft.comp.spv",
    };

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

    // RHI::GraphicsPipelineFixedStateStage fixedState{};
    // fixedState.pipelineRendering.colorAttachments[0] =
    //     device.surfaceFormat.format;
    // fixedState.pipelineRendering.depthAttachmentFormat =
    //     device.depthTexture.format;
    // fixedState.pipelineRendering.colorAttachmentCount = 1;
    // fixedState.colorBlendState.attachmentCount = 1;
    // fixedState.colorBlendState.attachments[0].blendEnable = true;
    // fixedState.colorBlendState.attachments[0].srcColorBlendFactor =
    //     VK_BLEND_FACTOR_SRC_ALPHA;
    // fixedState.colorBlendState.attachments[0].dstColorBlendFactor =
    //     VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    // fixedState.colorBlendState.attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    // fixedState.colorBlendState.attachments[0].srcAlphaBlendFactor =
    //     VK_BLEND_FACTOR_ZERO;
    // fixedState.colorBlendState.attachments[0].dstAlphaBlendFactor =
    //     VK_BLEND_FACTOR_ONE;
    // fixedState.colorBlendState.attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    // fixedState.depthStencilState.depthTestEnable = false;
    // fixedState.inputAssemblyState.topology =
    //     VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // RHI::ShaderProgram shaderProgram{};
    // if (!Fly::LoadShaderFromSpv(device, "splat.vert.spv",
    //                             shaderProgram[RHI::Shader::Type::Vertex]))
    // {
    //     return false;
    // }
    // if (!Fly::LoadShaderFromSpv(device, "splat.frag.spv",
    //                             shaderProgram[RHI::Shader::Type::Fragment]))
    // {
    //     return false;
    // }

    // if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
    //                                  sGraphicsPipeline))
    // {
    //     return false;
    // }
    // RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    // RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    return true;
}

static void DestroyPipelines(RHI::Device& device)
{
    // RHI::DestroyGraphicsPipeline(device, sGraphicsPipeline);
    RHI::DestroyComputePipeline(device, sFFTPipeline);
}

static RHI::Buffer sUniformBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
static bool CreateDeviceBuffers(RHI::Device& device)
{
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateUniformBuffer(device, nullptr, sizeof(UniformData),
                                      sUniformBuffers[i]))
        {
            return false;
        }
    }
    return true;
}

static void DestroyDeviceBuffers(RHI::Device& device)
{
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        RHI::DestroyBuffer(device, sUniformBuffers[i]);
    }
}

static void GraphicsPass(RHI::Device& device)
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
                      sGraphicsPipeline.handle);

    vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            sGraphicsPipeline.layout, 0, 1,
                            &device.bindlessDescriptorSet, 0, nullptr);

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
    vkCmdEndRendering(cmd.handle);
}

static void ExecuteCommands(RHI::Device& device) { // GraphicsPass(device);
}

static void OnKeyboardPressed(GLFWwindow* window, int key, int scancode,
                              int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }
}

int main(int argc, char* argv[])
{
    InitThreadContext();
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
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Ocean",
                                          nullptr, nullptr);
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
    glfwSetWindowUserPointer(window, &device);

    if (!CreatePipelines(device))
    {
        FLY_ERROR("Failed to create pipelines");
        return -1;
    }

    if (!CreateDeviceBuffers(device))
    {
        FLY_ERROR("Failed to create device buffers");
        return -1;
    }

    u64 previousFrameTime = 0;
    u64 loopStartTime = Fly::ClockNow();
    u64 currentFrameTime = loopStartTime;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        previousFrameTime = currentFrameTime;
        currentFrameTime = Fly::ClockNow();
        f32 time =
            static_cast<f32>(Fly::ToSeconds(currentFrameTime - loopStartTime));
        f64 deltaTime = Fly::ToSeconds(currentFrameTime - previousFrameTime);

        sCamera.Update(window, deltaTime);

        UniformData uniformData;
        uniformData.projection = sCamera.GetProjection();
        uniformData.view = sCamera.GetView();
        uniformData.cameraParameters.x =
            Math::Tan(Math::Radians(sCamera.GetHorizontalFov()) * 0.5f);
        uniformData.cameraParameters.y =
            Math::Tan(Math::Radians(sCamera.GetVerticalFov()) * 0.5f);
        uniformData.cameraParameters.z = sCamera.GetNear();
        uniformData.cameraParameters.w = sCamera.GetFar();
        uniformData.viewport = Math::Vec4(
            static_cast<f32>(WINDOW_WIDTH), static_cast<f32>(WINDOW_HEIGHT),
            1.0f / WINDOW_WIDTH, 1.0f / WINDOW_HEIGHT);
        uniformData.time.x = time;
        RHI::CopyDataToBuffer(device, &uniformData, sizeof(UniformData), 0,
                              sUniformBuffers[device.frameIndex]);

        RHI::BeginRenderFrame(device);
        ExecuteCommands(device);
        RHI::EndRenderFrame(device);
    }

    RHI::WaitAllDevicesIdle(context);

    DestroyPipelines(device);
    DestroyDeviceBuffers(device);
    RHI::DestroyContext(context);
    glfwDestroyWindow(window);
    glfwTerminate();

    FLY_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
