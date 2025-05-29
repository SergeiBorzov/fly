#include "core/assert.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/platform.h"
#include "core/thread_context.h"

#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"

#include "demos/common/scene.h"

#include <GLFW/glfw3.h>

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

using namespace Hls;
static void RecordCommands(RHI::Device& device, RHI::GraphicsPipeline& pipeline)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    const RHI::SwapchainTexture& swapchainTexture =
        RenderFrameSwapchainTexture(device);

    VkRect2D renderArea = {{0, 0},
                           {swapchainTexture.width, swapchainTexture.height}};
    VkRenderingAttachmentInfo colorAttachment = RHI::ColorAttachmentInfo(
        swapchainTexture.imageView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo =
        RHI::RenderingInfo(renderArea, &colorAttachment, 1);

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
    glfwSetKeyCallback(window, OnKeyboardPressed);

    // Device extensions
    const char* requiredDeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    RHI::ContextSettings settings{};
    settings.instanceExtensions =
        glfwGetRequiredInstanceExtensions(&settings.instanceExtensionCount);
    settings.deviceExtensions = requiredDeviceExtensions;
    settings.deviceExtensionCount = STACK_ARRAY_COUNT(requiredDeviceExtensions);
    settings.windowPtr = window;

    RHI::Context context;
    if (!RHI::CreateContext(settings, context))
    {
        HLS_ERROR("Failed to create context");
        return -1;
    }

    RHI::Device& device = context.devices[0];

    RHI::ShaderProgram shaderProgram{};
    if (!Hls::LoadShaderFromSpv(device, "triangle.vert.spv",
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return -1;
    }
    if (!Hls::LoadShaderFromSpv(device, "triangle.frag.spv",
                                shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return -1;
    }

    RHI::GraphicsPipelineFixedStateStage fixedState{};
    fixedState.pipelineRendering.colorAttachments[0] =
        device.surfaceFormat.format;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.colorBlendState.attachmentCount = 1;

    RHI::GraphicsPipeline graphicsPipeline{};
    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     graphicsPipeline))
    {
        HLS_ERROR("Failed to create graphics pipeline");
        return -1;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        RHI::BeginRenderFrame(device);
        RecordCommands(device, graphicsPipeline);
        RHI::EndRenderFrame(device);
    }

    RHI::WaitAllDevicesIdle(context);

    RHI::DestroyGraphicsPipeline(device, graphicsPipeline);
    RHI::DestroyContext(context);

    glfwDestroyWindow(window);
    glfwTerminate();
    HLS_LOG("Shutdown successful");
    ShutdownLogger();

    ReleaseThreadContext();
    return 0;
}
