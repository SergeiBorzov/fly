#include "core/assert.h"
#include "core/clock.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/texture.h"
#include "rhi/uniform_buffer.h"
#include "rhi/utils.h"

#include "assets/import_image.h"

#include "platform/window.h"

#include "demos/common/simple_camera_fps.h"

using namespace Hls;

static RHI::UniformBuffer uniformBuffers[HLS_FRAME_IN_FLIGHT_COUNT];
static RHI::Texture texture;

static Hls::SimpleCameraFPS
    sCamera(Math::Perspective(45.0f, 1280.0f / 720.0f, 0.01f, 100.0f),
            Math::Vec3(0.0f, 0.0f, -5.0f));

struct UniformData
{
    Math::Mat4 projection = {};
    Math::Mat4 view = {};
    f32 time = 0.0f;
    f32 padding[15] = {0.0f};
};

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

static void RecordCommands(RHI::Device& device, RHI::GraphicsPipeline& pipeline)
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

    u32 indices[2] = {uniformBuffers[device.frameIndex].bindlessHandle,
                      texture.bindlessHandle};
    vkCmdPushConstants(cmd.handle, pipeline.layout, VK_SHADER_STAGE_ALL, 0,
                       sizeof(u32) * 2, indices);
    vkCmdDraw(cmd.handle, 36, 10000, 0, 0);

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
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Cubes", nullptr, nullptr);
    if (!window)
    {
        HLS_ERROR("Failed to create glfw window");
        glfwTerminate();
        return -1;
    }
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetKeyCallback(window, OnKeyboardPressed);

    const char* requiredDeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    RHI::ContextSettings settings{};
    settings.deviceFeatures2.pNext = nullptr;
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

    RHI::Device& device = context.devices[1];

    RHI::GraphicsPipelineProgrammableStage programmableStage{};
    RHI::ShaderPathMap shaderPathMap{};
    Hls::Path::Create(arena, HLS_STRING8_LITERAL("cubes.vert.spv"),
                      shaderPathMap[RHI::ShaderType::Vertex]);
    Hls::Path::Create(arena, HLS_STRING8_LITERAL("cubes.frag.spv"),
                      shaderPathMap[RHI::ShaderType::Fragment]);

    if (!RHI::LoadProgrammableStage(arena, device, shaderPathMap,
                                    programmableStage))
    {
        HLS_ERROR("Failed to load and create shader modules");
    }

    Hls::Image image;
    if (!Hls::ImportImageFromFile("default.png", image))
    {
        HLS_ERROR("Failed to load image");
        return -1;
    }
    if (!RHI::CreateTexture(device, image.width, image.height,
                            VK_FORMAT_R8G8B8A8_SRGB, texture, false, 0))
    {
        HLS_ERROR("Failed to create texture");
        return -1;
    }
    if (!Hls::TransferImageDataToTexture(device, image, texture))
    {
        HLS_ERROR("Failed to transfer image data to texture");
        return -1;
    }
    Hls::FreeImage(image);

    for (u32 i = 0; i < HLS_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateUniformBuffer(device, nullptr, sizeof(UniformData),
                                      uniformBuffers[i]))
        {
            HLS_ERROR("Failed to create uniform buffer!");
        }
    }

    RHI::GraphicsPipelineFixedStateStage fixedState{};
    fixedState.pipelineRendering.colorAttachments[0] =
        device.surfaceFormat.format;
    fixedState.pipelineRendering.depthAttachmentFormat =
        device.depthTexture.format;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.colorBlendState.attachmentCount = 1;
    fixedState.depthStencilState.depthTestEnable = true;
    fixedState.rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

    RHI::GraphicsPipeline graphicsPipeline{};
    if (!RHI::CreateGraphicsPipeline(device, fixedState, programmableStage,
                                     graphicsPipeline))
    {
        HLS_ERROR("Failed to create graphics pipeline");
        return -1;
    }
    RHI::DestroyGraphicsPipelineProgrammableStage(device, programmableStage);

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
        UniformData uniformData = {sCamera.GetProjection(), sCamera.GetView(),
                                   time};

        RHI::CopyDataToUniformBuffer(device, &uniformData, sizeof(UniformData),
                                     0, uniformBuffers[device.frameIndex]);

        RHI::BeginRenderFrame(device);
        RecordCommands(device, graphicsPipeline);
        RHI::EndRenderFrame(device);
    }

    RHI::WaitAllDevicesIdle(context);
    for (u32 i = 0; i < HLS_FRAME_IN_FLIGHT_COUNT; i++)
    {
        RHI::DestroyUniformBuffer(device, uniformBuffers[i]);
    }
    RHI::DestroyTexture(device, texture);

    RHI::DestroyGraphicsPipeline(device, graphicsPipeline);
    RHI::DestroyContext(context);

    glfwDestroyWindow(window);
    glfwTerminate();
    HLS_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
