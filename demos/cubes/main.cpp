#include "core/assert.h"
#include "core/clock.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "rhi/context.h"
#include "rhi/frame_graph.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"

#include "assets/import_image.h"
#include "utils/utils.h"

#include "demos/common/simple_camera_fps.h"

#include <GLFW/glfw3.h>

using namespace Fly;

struct UniformData
{
    Math::Mat4 projection = {};
    Math::Mat4 view = {};
    f32 time = 0.0f;
    f32 pad[3];
};

struct UserData
{
    u32 viewportWidth;
    u32 viewportHeight;
    Image cubeImage;
    RHI::GraphicsPipeline pipeline;
    RHI::FrameGraph::BufferHandle uniformBuffer;
    RHI::FrameGraph::TextureHandle cubeTexture;
};

struct CubesPassContext
{
    RHI::FrameGraph::TextureHandle depthTexture;
    RHI::FrameGraph::TextureHandle colorAttachment;
    RHI::FrameGraph::TextureHandle depthAttachment;
    RHI::FrameGraph::TextureHandle cubeTexture;
    RHI::FrameGraph::BufferHandle uniformBuffer;
};

static Fly::SimpleCameraFPS sCamera(90.0f, 1280.0f / 720.0f, 0.01f, 1000.0f,
                                    Math::Vec3(0.0f, 0.0f, -5.0f));

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

static void CubesPassBuild(Arena& arena, RHI::FrameGraph::Builder& builder,
                           CubesPassContext& context, void* pUserData)
{
    UserData* userData = static_cast<UserData*>(pUserData);
    Image& image = userData->cubeImage;

    context.depthTexture = RHI::CreateTexture2D(
        arena, builder, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 1.0f, 1.0f,
        VK_FORMAT_D32_SFLOAT_S8_UINT);

    context.colorAttachment = RHI::ColorAttachment(
        arena, builder, 0, RHI::FrameGraph::TextureHandle::sBackBuffer);

    context.depthAttachment =
        RHI::DepthAttachment(arena, builder, context.depthTexture);

    context.cubeTexture = RHI::CreateTexture2D(
        arena, builder,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        image.data, image.width, image.height, VK_FORMAT_R8G8B8A8_SRGB,
        RHI::Sampler::FilterMode::Trilinear, RHI::Sampler::WrapMode::Repeat);

    context.uniformBuffer = RHI::CreateBuffer(
        arena, builder,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        true, nullptr, sizeof(UniformData));

    userData->uniformBuffer = context.uniformBuffer;
    userData->cubeTexture = context.cubeTexture;
}

static void CubesPassExecute(RHI::CommandBuffer& cmd,
                             const RHI::FrameGraph::ResourceMap& resources,
                             const CubesPassContext& context, void* pUserData)
{
    UserData* userData = static_cast<UserData*>(pUserData);
    RHI::SetViewport(cmd, 0, 0, static_cast<f32>(userData->viewportWidth),
                     static_cast<f32>(userData->viewportHeight), 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, userData->viewportWidth,

                    userData->viewportHeight);

    const RHI::Buffer& uniformBuffer =
        resources.GetBuffer(userData->uniformBuffer);
    const RHI::Texture2D& cubeTexture =
        resources.GetTexture2D(userData->cubeTexture);
    RHI::BindGraphicsPipeline(cmd, userData->pipeline);

    u32 indices[2] = {uniformBuffer.bindlessHandle, cubeTexture.bindlessHandle};
    RHI::PushConstants(cmd, indices, sizeof(indices));
    RHI::Draw(cmd, 36, 10000, 0, 0);
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
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Cubes", nullptr, nullptr);
    if (!window)
    {
        FLY_ERROR("Failed to create glfw window");
        glfwTerminate();
        return -1;
    }
    glfwSetKeyCallback(window, OnKeyboardPressed);

    const char* requiredDeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    RHI::ContextSettings settings{};
    settings.deviceFeatures2.pNext = nullptr;
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
    if (!Fly::LoadShaderFromSpv(device, "cubes.vert.spv",
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return -1;
    }
    if (!Fly::LoadShaderFromSpv(device, "cubes.frag.spv",
                                shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return -1;
    }

    RHI::GraphicsPipelineFixedStateStage fixedState{};
    fixedState.pipelineRendering.colorAttachments[0] =
        device.surfaceFormat.format;
    fixedState.pipelineRendering.depthAttachmentFormat =
        VK_FORMAT_D32_SFLOAT_S8_UINT;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.colorBlendState.attachmentCount = 1;
    fixedState.depthStencilState.depthTestEnable = true;
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

    Image image;
    if (!Fly::LoadImageFromFile("CesiumLogoFlat.png", image))
    {
        FLY_ERROR("Failed to load image");
        return false;
    }

    UserData userData;
    userData.pipeline = graphicsPipeline;
    userData.cubeImage = image;

    RHI::FrameGraph fg(device);
    fg.AddPass<CubesPassContext>(arena, "CubesPass",
                                 RHI::FrameGraph::PassNode::Type::Graphics,
                                 CubesPassBuild, CubesPassExecute, &userData);
    fg.Build(arena);

    FreeImage(image);

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

        i32 w, h;
        glfwGetFramebufferSize(context.windowPtr, &w, &h);
        userData.viewportWidth = static_cast<u32>(w);
        userData.viewportHeight = static_cast<u32>(h);

        sCamera.Update(window, deltaTime);
        UniformData uniformData = {sCamera.GetProjection(), sCamera.GetView(),
                                   time};

        RHI::Buffer& uniformBuffer =
            fg.resources_.GetBuffer(userData.uniformBuffer);
        RHI::CopyDataToBuffer(device, &uniformData, sizeof(UniformData), 0,
                              uniformBuffer);
        fg.Execute();
    }

    RHI::WaitDeviceIdle(device);
    fg.Destroy();
    RHI::DestroyGraphicsPipeline(device, graphicsPipeline);
    RHI::DestroyContext(context);

    glfwDestroyWindow(window);
    glfwTerminate();
    FLY_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
