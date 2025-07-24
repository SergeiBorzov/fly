#include "core/clock.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "rhi/buffer.h"
#include "rhi/context.h"
#include "rhi/frame_graph.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"

#include "utils/utils.h"

#include "demos/common/scene.h"
#include "demos/common/simple_camera_fps.h"

#include <GLFW/glfw3.h>

using namespace Fly;

struct UniformData
{
    Math::Mat4 projection = {};
    Math::Mat4 view = {};
};

static Fly::SimpleCameraFPS sCamera(80.0f, 1280.0f / 720.0f, 0.01f, 100.0f,
                                    Fly::Math::Vec3(0.0f, 0.0f, -5.0f));

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

struct GraphicsPassContext
{
    RHI::FrameGraph::TextureHandle depthTexture;
    RHI::FrameGraph::TextureHandle colorAttachment;
    RHI::FrameGraph::TextureHandle depthAttachment;
    RHI::FrameGraph::BufferHandle uniformBuffer;
    RHI::FrameGraph::BufferHandle materialBuffer;
};

struct UserData
{
    RHI::Device* device;
    Fly::Scene* scene;
    RHI::GraphicsPipeline pipeline;
    RHI::FrameGraph::BufferHandle uniformBuffer;
};

static void GraphicsPassBuild(Arena& arena, RHI::FrameGraph::Builder& builder,
                              GraphicsPassContext& context, void* pUserData)
{
    UserData* userData = static_cast<UserData*>(pUserData);

    context.depthTexture = builder.CreateTexture2D(
        arena, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 1.0f, 1.0f,
        VK_FORMAT_D32_SFLOAT_S8_UINT);

    context.colorAttachment = builder.ColorAttachment(
        arena, 0, RHI::FrameGraph::TextureHandle::sBackBuffer);

    context.depthAttachment =
        builder.DepthAttachment(arena, context.depthTexture);

    context.uniformBuffer = builder.CreateBuffer(
        arena,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        true, nullptr, sizeof(UniformData));
    builder.Read(arena, context.uniformBuffer);

    context.materialBuffer =
        builder.RegisterExternalBuffer(arena, userData->scene->materialBuffer);
    builder.Read(arena, context.materialBuffer);

    for (u32 i = 0; i < userData->scene->vertexBufferCount; i++)
    {
        RHI::FrameGraph::BufferHandle bh = builder.RegisterExternalBuffer(
            arena, userData->scene->vertexBuffers[i]);
        builder.Read(arena, bh);
    }

    for (u32 i = 0; i < userData->scene->textureCount; i++)
    {
        RHI::FrameGraph::TextureHandle th = builder.RegisterExternalTexture2D(
            arena, userData->scene->textures[i]);
        builder.Read(arena, th);
    }

    userData->uniformBuffer = context.uniformBuffer;
}

static void GraphicsPassExecute(RHI::CommandBuffer& cmd,
                                RHI::FrameGraph::ResourceMap& resources,
                                const GraphicsPassContext& context,
                                void* pUserData)
{
    UserData* userData = static_cast<UserData*>(pUserData);

    RHI::SetViewport(
        cmd, 0, 0, static_cast<f32>(userData->device->swapchainWidth),
        static_cast<f32>(userData->device->swapchainHeight), 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, userData->device->swapchainWidth,
                    userData->device->swapchainHeight);

    const RHI::Buffer& uniformBuffer =
        resources.GetBuffer(context.uniformBuffer);
    const RHI::Buffer& materialBuffer =
        resources.GetBuffer(context.materialBuffer);

    RHI::BindGraphicsPipeline(cmd, userData->pipeline);
    RHI::BindIndexBuffer(cmd, userData->scene->indexBuffer,
                         VK_INDEX_TYPE_UINT32);

    u32 globalIndices[2] = {uniformBuffer.bindlessHandle,
                            materialBuffer.bindlessHandle};
    RHI::PushConstants(cmd, globalIndices, sizeof(globalIndices),
                       sizeof(Math::Mat4));

    for (u32 i = 0; i < userData->scene->meshNodeCount; i++)
    {
        const Fly::MeshNode& meshNode = userData->scene->meshNodes[i];
        RHI::PushConstants(cmd, meshNode.model.data,
                           sizeof(meshNode.model.data));
        for (u32 j = 0; j < meshNode.mesh->submeshCount; j++)
        {
            const Fly::Submesh& submesh = meshNode.mesh->submeshes[j];
            u32 localIndices[2] = {submesh.vertexBufferIndex,
                                   submesh.materialIndex};
            RHI::PushConstants(cmd, localIndices, sizeof(localIndices),
                               sizeof(Math::Mat4) + sizeof(globalIndices));
            RHI::DrawIndexed(cmd, submesh.indexCount, 1, submesh.indexOffset, 0,
                             0);
        }
    }
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
        glfwCreateWindow(1280, 720, "Sponza gltf", nullptr, nullptr);
    if (!window)
    {
        FLY_ERROR("Failed to create glfw window");
        glfwTerminate();
        return -1;
    }
    glfwSetKeyCallback(window, OnKeyboardPressed);

    // Create graphics context
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
        VK_FORMAT_D32_SFLOAT_S8_UINT;
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

    // Scene
    Fly::Scene scene;
    if (!Fly::LoadSceneFromGLTF(arena, device, "Sponza.gltf", scene))
    {
        FLY_ERROR("Failed to load gltf");
        return -1;
    }

    UserData userData;
    userData.pipeline = graphicsPipeline;
    userData.scene = &scene;
    userData.device = &device;

    RHI::FrameGraph fg(device);
    fg.AddPass<GraphicsPassContext>(
        arena, "GraphicsPass", RHI::FrameGraph::PassNode::Type::Graphics,
        GraphicsPassBuild, GraphicsPassExecute, &userData);
    fg.Build(arena);

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

        RHI::Buffer& uniformBuffer = fg.GetBuffer(userData.uniformBuffer);
        RHI::CopyDataToBuffer(device, &uniformData, sizeof(UniformData), 0,
                              uniformBuffer);

        fg.Execute();
    }

    RHI::WaitAllDevicesIdle(context);
    fg.Destroy();
    Fly::UnloadScene(device, scene);
    RHI::DestroyGraphicsPipeline(device, graphicsPipeline);
    RHI::DestroyContext(context);

    glfwDestroyWindow(window);
    glfwTerminate();
    FLY_LOG("Shutdown successful");

    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
