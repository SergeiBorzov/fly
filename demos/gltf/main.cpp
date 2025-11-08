#include "core/clock.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "rhi/buffer.h"
#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"

#include "utils/utils.h"

#include "demos/common/scene.h"
#include "demos/common/simple_camera_fps.h"

#include <GLFW/glfw3.h>

using namespace Fly;

static RHI::GraphicsPipeline sGraphicsPipeline;
static RHI::Buffer sUniformBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Texture sDepthTexture;

struct UniformData
{
    Math::Mat4 projection = {};
    Math::Mat4 view = {};
};

static Fly::SimpleCameraFPS sCamera(80.0f, 1280.0f / 720.0f, 0.01f, 100.0f,
                                    Fly::Math::Vec3(0.0f, 2.5f, 0.0f));

static void OnKeyboardPressed(GLFWwindow* window, int key, int scancode,
                              int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }
}

static void OnFramebufferResize(RHI::Device& device, u32 width, u32 height,
                                void*)
{
    RHI::DestroyTexture(device, sDepthTexture);
    RHI::CreateTexture2D(device, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                         nullptr, width, height, VK_FORMAT_D32_SFLOAT,
                         RHI::Sampler::FilterMode::Nearest,
                         RHI::Sampler::WrapMode::Repeat, 1, sDepthTexture);
}

static void ErrorCallbackGLFW(i32 error, const char* description)
{
    FLY_ERROR("GLFW - error: %s", description);
}

static bool CreatePipeline(RHI::Device& device)
{
    RHI::ShaderProgram shaderProgram{};
    if (!Fly::LoadShaderFromSpv(device, FLY_STRING8_LITERAL("unlit.vert.spv"),
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        FLY_ERROR("Failed to load vertex shader");
        return false;
    }
    if (!Fly::LoadShaderFromSpv(device, FLY_STRING8_LITERAL("unlit.frag.spv"),
                                shaderProgram[RHI::Shader::Type::Fragment]))
    {
        FLY_ERROR("Failed to load fragment shader");
        return false;
    }

    RHI::GraphicsPipelineFixedStateStage fixedState{};
    fixedState.pipelineRendering.colorAttachments[0] =
        device.surfaceFormat.format;
    fixedState.pipelineRendering.depthAttachmentFormat =
        VK_FORMAT_D32_SFLOAT;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.colorBlendState.attachmentCount = 1;
    fixedState.depthStencilState.depthTestEnable = true;
    fixedState.rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    fixedState.rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     sGraphicsPipeline))
    {
        FLY_ERROR("Failed to create graphics pipeline");
        return false;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    return true;
}

static void DestroyPipeline(RHI::Device& device)
{
    RHI::DestroyGraphicsPipeline(device, sGraphicsPipeline);
}

static bool CreateResources(RHI::Device& device)
{
    if (!RHI::CreateTexture2D(
            device, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, nullptr,
            device.swapchainWidth, device.swapchainHeight,
            VK_FORMAT_D32_SFLOAT, RHI::Sampler::FilterMode::Nearest,
            RHI::Sampler::WrapMode::Repeat, 1, sDepthTexture))
    {
        FLY_ERROR("Failed to create depth texture");
        return false;
    }

    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateBuffer(device, true, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                               nullptr, sizeof(UniformData),
                               sUniformBuffers[i]))
        {
            FLY_ERROR("Failed to create uniform buffer %u", i);
            return false;
        }
    }

    return true;
}

static void DestroyResources(RHI::Device& device)
{
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        RHI::DestroyBuffer(device, sUniformBuffers[i]);
    }
    RHI::DestroyTexture(device, sDepthTexture);
}

static void RecordDrawScene(RHI::CommandBuffer& cmd,
                            const RHI::RecordBufferInput* bufferInput,
                            u32 bufferInputCount,
                            const RHI::RecordTextureInput* textureInput,
                            u32 textureInputCount, void* pUserData)
{
    Scene* scene = static_cast<Scene*>(pUserData);

    RHI::SetViewport(cmd, 0, 0, static_cast<f32>(cmd.device->swapchainWidth),
                     static_cast<f32>(cmd.device->swapchainHeight), 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, cmd.device->swapchainWidth,
                    cmd.device->swapchainHeight);

    const RHI::Buffer& uniformBuffer = *(bufferInput[0].pBuffer);
    const RHI::Buffer& materialBuffer = scene->materialBuffer;

    RHI::BindGraphicsPipeline(cmd, sGraphicsPipeline);
    RHI::BindIndexBuffer(cmd, scene->indexBuffer, VK_INDEX_TYPE_UINT32);

    u32 globalIndices[2] = {uniformBuffer.bindlessHandle,
                            materialBuffer.bindlessHandle};
    RHI::PushConstants(cmd, globalIndices, sizeof(globalIndices),
                       sizeof(Math::Mat4));

    for (u32 i = 0; i < scene->meshNodeCount; i++)
    {
        const Fly::MeshNode& meshNode = scene->meshNodes[i];
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

static void DrawScene(RHI::Device& device, Scene* scene)
{
    RHI::RecordBufferInput bufferInput = {&sUniformBuffers[device.frameIndex],
                                          VK_ACCESS_2_SHADER_READ_BIT};

    VkRenderingAttachmentInfo colorAttachment =
        RHI::ColorAttachmentInfo(RenderFrameSwapchainTexture(device).imageView);
    VkRenderingAttachmentInfo depthAttachment =
        RHI::DepthAttachmentInfo(sDepthTexture.imageView);
    VkRenderingInfo renderingInfo = RHI::RenderingInfo(
        {{0, 0}, {device.swapchainWidth, device.swapchainHeight}},
        &colorAttachment, 1, &depthAttachment);
    RHI::ExecuteGraphics(RenderFrameCommandBuffer(device), renderingInfo,
                         RecordDrawScene, &bufferInput, 1, nullptr, 0, scene);
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
    device.swapchainRecreatedCallback.func = OnFramebufferResize;

    if (!CreatePipeline(device))
    {
        return -1;
    }

    if (!CreateResources(device))
    {
        return -1;
    }

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
        f64 deltaTime = Fly::ToSeconds(currentFrameTime - previousFrameTime);

        glfwPollEvents();

        sCamera.Update(window, deltaTime);
        UniformData uniformData = {sCamera.GetProjection(), sCamera.GetView()};

        RHI::Buffer& uniformBuffer = sUniformBuffers[device.frameIndex];
        RHI::CopyDataToBuffer(device, &uniformData, sizeof(UniformData), 0,
                              uniformBuffer);

        RHI::BeginRenderFrame(device);
        DrawScene(device, &scene);
        RHI::EndRenderFrame(device);
    }

    RHI::WaitAllDevicesIdle(context);
    DestroyResources(device);
    Fly::UnloadScene(device, scene);
    DestroyPipeline(device);
    RHI::DestroyContext(context);

    glfwDestroyWindow(window);
    glfwTerminate();
    FLY_LOG("Shutdown successful");

    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
