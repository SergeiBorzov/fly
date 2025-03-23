#include "src/core/assert.h"
#include "src/core/filesystem.h"
#include "src/core/log.h"
#include "src/core/thread_context.h"

#include "src/rhi/context.h"
#include "src/rhi/pipeline.h"
#include "src/rhi/utils.h"
#include <GLFW/glfw3.h>

static void SetVulkanLayerPathEnvVariable()
{
    Arena& scratch = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(scratch);
    const char* binaryPath = GetBinaryDirectoryPath(scratch);
    HLS_ASSERT(binaryPath);
    HLS_ENSURE(SetEnv("VK_LAYER_PATH", binaryPath));
    ArenaPopToMarker(scratch, marker);
}

static void ErrorCallbackGLFW(i32 error, const char* description)
{
    HLS_ERROR("GLFW - error: %s", description);
}

static void RecordCommands(Hls::Device& device, Hls::GraphicsPipeline& pipeline)
{
    Hls::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    VkImage image = RenderFrameSwapchainImage(device);
    VkImageView imageView = RenderFrameSwapchainImageView(device);
    VkRect2D renderArea = SwapchainRect2D(device);
    VkRenderingAttachmentInfo colorAttachment = Hls::ColorAttachmentInfo(
        imageView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo =
        Hls::RenderingInfo(renderArea, &colorAttachment, 1);

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

    vkCmdDraw(cmd.handle, 3, 1, 0, 0);

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

    SetVulkanLayerPathEnvVariable();

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
    GLFWwindow* window = glfwCreateWindow(640, 480, "Window", nullptr, nullptr);
    if (!window)
    {
        HLS_ERROR("Failed to create glfw window");
        glfwTerminate();
        return -1;
    }

    // Device extensions
    const char* requiredDeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{};
    dynamicRenderingFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRenderingFeatures.dynamicRendering = VK_TRUE;

    Hls::ContextSettings settings{};
    settings.deviceFeatures2.pNext = &dynamicRenderingFeatures;
    settings.instanceExtensions =
        glfwGetRequiredInstanceExtensions(&settings.instanceExtensionCount);
    settings.deviceExtensions = requiredDeviceExtensions;
    settings.deviceExtensionCount = STACK_ARRAY_COUNT(requiredDeviceExtensions);
    settings.windowPtr = window;

    Hls::Context context;
    if (!Hls::CreateContext(settings, context))
    {
        HLS_ERROR("Failed to create context");
        return -1;
    }

    Hls::Device& device = context.devices[0];

    Hls::GraphicsPipelineProgrammableStage programmableState{};
    Hls::ShaderPathMap shaderPathMap{};
    shaderPathMap[Hls::ShaderType::Vertex] = "triangle.vert.spv";
    shaderPathMap[Hls::ShaderType::Fragment] = "triangle.frag.spv";

    if (!Hls::LoadProgrammableStage(arena, device, shaderPathMap,
                                    programmableState))
    {
        HLS_ERROR("Failed to load and create shader modules");
    }

    Hls::GraphicsPipelineFixedStateStage fixedState{};
    fixedState.pipelineRendering.colorAttachments[0] =
        device.surfaceFormat.format;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.colorBlendState.attachmentCount = 1;

    Hls::GraphicsPipeline graphicsPipeline{};
    if (!Hls::CreateGraphicsPipeline(device, fixedState, programmableState,
                                     graphicsPipeline))
    {
        HLS_ERROR("Failed to create graphics pipeline");
        return -1;
    }
    Hls::DestroyGraphicsPipelineProgrammableStage(device, programmableState);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        BeginRenderFrame(context, device);
        RecordCommands(device, graphicsPipeline);
        EndRenderFrame(context, device);
    }

    Hls::WaitAllDevicesIdle(context);

    Hls::DestroyGraphicsPipeline(device, graphicsPipeline);
    Hls::DestroyContext(context);

    glfwDestroyWindow(window);
    glfwTerminate();
    HLS_LOG("Shutdown successful");
    ShutdownLogger();

    ReleaseThreadContext();
    return 0;
}
