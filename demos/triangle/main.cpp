#include "core/assert.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/platform.h"
#include "core/thread_context.h"

#include "rhi/context.h"
#include "rhi/frame_graph.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"

#include "utils/utils.h"

#include <GLFW/glfw3.h>

using namespace Fly;

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

struct UserData
{
    u32 viewportWidth;
    u32 viewportHeight;
    RHI::GraphicsPipeline pipeline;
};

struct TrianglePassContext
{
    RHI::FrameGraph::TextureHandle colorAttachment;
};

static void TrianglePassBuild(Arena& arena, RHI::FrameGraph::Builder& builder,
                              TrianglePassContext& context, void* pUserData)
{
    context.colorAttachment = builder.ColorAttachment(
        arena, 0, RHI::FrameGraph::TextureHandle::sBackBuffer);
}

static void TrianglePassExecute(RHI::CommandBuffer& cmd,
                                const RHI::FrameGraph::ResourceMap& resources,
                                const TrianglePassContext& context,
                                void* pUserData)
{
    UserData* userData = static_cast<UserData*>(pUserData);
    RHI::BindGraphicsPipeline(cmd, userData->pipeline);
    RHI::SetViewport(cmd, 0, 0, static_cast<f32>(userData->viewportWidth),
                     static_cast<f32>(userData->viewportHeight), 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, userData->viewportWidth,
                    userData->viewportHeight);
    RHI::Draw(cmd, 3, 1, 0, 0);
}

int main(int argc, char* argv[])
{
    InitThreadContext();
    if (!InitLogger())
    {
        return -1;
    }

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
    GLFWwindow* window = glfwCreateWindow(640, 480, "Window", nullptr, nullptr);
    if (!window)
    {
        FLY_ERROR("Failed to create glfw window");
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
        FLY_ERROR("Failed to create context");
        return -1;
    }

    RHI::Device& device = context.devices[0];

    RHI::ShaderProgram shaderProgram{};
    if (!Fly::LoadShaderFromSpv(device, "triangle.vert.spv",
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return -1;
    }
    if (!Fly::LoadShaderFromSpv(device, "triangle.frag.spv",
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
        FLY_ERROR("Failed to create graphics pipeline");
        return -1;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    UserData userData;
    userData.pipeline = graphicsPipeline;

    Arena& arena = GetScratchArena();
    RHI::FrameGraph fg(device);
    fg.AddPass<TrianglePassContext>(
        arena, "TrianglePass", RHI::FrameGraph::PassNode::Type::Graphics,
        TrianglePassBuild, TrianglePassExecute, &userData);
    fg.Build(arena);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        fg.GetSwapchainSize(userData.viewportWidth, userData.viewportHeight);
        fg.Execute();
    }

    RHI::WaitDeviceIdle(device);

    RHI::DestroyGraphicsPipeline(device, graphicsPipeline);
    RHI::DestroyContext(context);

    glfwDestroyWindow(window);
    glfwTerminate();
    FLY_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
