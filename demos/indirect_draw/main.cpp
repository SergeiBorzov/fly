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

struct UniformData
{
    Math::Mat4 projection;
    Math::Mat4 view;
    Math::Mat4 cullView;
    f32 hTanX;
    f32 hTanY;
    f32 nearPlane;
    f32 farPlane;
};

static RHI::GraphicsPipeline sDrawPipeline;
static RHI::ComputePipeline sCullPipeline;
static RHI::Buffer sUniformBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sDrawCommands;
static RHI::Buffer sDrawCountBuffer;
static RHI::Texture sDepthTexture;

static const u32 sDrawCount = 30000;

static Fly::SimpleCameraFPS sCamera(85.0f, 1280.0f / 720.0f, 0.01f, 100.0f,
                                    Fly::Math::Vec3(0.0f, 0.0f, -5.0f));

static Fly::SimpleCameraFPS sTopCamera(85.0f, 1280.0f / 720.0f, 0.01f, 100.0f,
                                       Fly::Math::Vec3(0.0f, 20.0f, -5.0f));

static Fly::SimpleCameraFPS* sMainCamera = &sCamera;

static bool IsPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice,
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

    if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
    {
        if (sMainCamera == &sCamera)
        {
            sMainCamera = &sTopCamera;
        }
        else
        {
            sMainCamera = &sCamera;
        }
    }
}

static void OnFramebufferResize(RHI::Device& device, u32 width, u32 height,
                                void*)
{
    RHI::DestroyTexture(device, sDepthTexture);
    RHI::CreateTexture2D(device, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                         nullptr, width, height, VK_FORMAT_D32_SFLOAT_S8_UINT,
                         RHI::Sampler::FilterMode::Nearest,
                         RHI::Sampler::WrapMode::Repeat, 1, sDepthTexture);
}

static void ErrorCallbackGLFW(i32 error, const char* description)
{
    FLY_ERROR("GLFW - error: %s", description);
}

static bool CreatePipelines(RHI::Device& device)
{
    RHI::Shader cullShader;
    if (!Fly::LoadShaderFromSpv(device, "cull.comp.spv", cullShader))
    {
        return false;
    }
    if (!RHI::CreateComputePipeline(device, cullShader, sCullPipeline))
    {
        FLY_ERROR("Failed to create cull compute pipeline");
        return false;
    }
    RHI::DestroyShader(device, cullShader);

    RHI::ShaderProgram shaderProgram{};
    if (!Fly::LoadShaderFromSpv(device, "unlit.vert.spv",
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }
    if (!Fly::LoadShaderFromSpv(device, "unlit.frag.spv",
                                shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
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

    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     sDrawPipeline))
    {
        FLY_ERROR("Failed to create graphics pipeline");
        return false;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    return true;
}

static void DestroyPipelines(RHI::Device& device)
{
    RHI::DestroyGraphicsPipeline(device, sDrawPipeline);
    RHI::DestroyComputePipeline(device, sCullPipeline);
}

static bool CreateResources(RHI::Device& device)
{
    if (!RHI::CreateTexture2D(
            device, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, nullptr,
            device.swapchainWidth, device.swapchainHeight,
            VK_FORMAT_D32_SFLOAT_S8_UINT, RHI::Sampler::FilterMode::Nearest,
            RHI::Sampler::WrapMode::Repeat, 1, sDepthTexture))
    {
        FLY_ERROR("Failed to create depth texture");
        return false;
    }

    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateBuffer(device, true,
                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               nullptr, sizeof(UniformData),
                               sUniformBuffers[i]))
        {
            FLY_ERROR("Failed to create uniform buffer %u", i);
            return false;
        }
    }

    if (!RHI::CreateBuffer(device, false,
                           VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           nullptr,
                           sizeof(VkDrawIndexedIndirectCommand) * sDrawCount,
                           sDrawCommands))
    {
        return false;
    }

    if (!RHI::CreateBuffer(device, false,
                           VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           nullptr, sizeof(u32), sDrawCountBuffer))
    {
        return false;
    }
    return true;
}

static void DestroyResources(RHI::Device& device)
{
    RHI::DestroyBuffer(device, sDrawCountBuffer);
    RHI::DestroyBuffer(device, sDrawCommands);
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        RHI::DestroyBuffer(device, sUniformBuffers[i]);
    }
    RHI::DestroyTexture(device, sDepthTexture);
}

static void RecordFrustumCull(RHI::CommandBuffer& cmd,
                              const RHI::RecordBufferInput* bufferInput,
                              const RHI::RecordTextureInput* textureInput,
                              void* pUserData)
{
    Scene* scene = static_cast<Scene*>(pUserData);
    RHI::BindComputePipeline(cmd, sCullPipeline);

    RHI::Buffer& uniformBuffer = *(bufferInput->buffers[0]);
    RHI::Buffer& drawCommands = *(bufferInput->buffers[1]);
    RHI::Buffer& drawCount = *(bufferInput->buffers[2]);

    u32 pushConstants[] = {
        scene->indirectDrawData.instanceDataBuffer.bindlessHandle,
        scene->indirectDrawData.meshDataBuffer.bindlessHandle,
        uniformBuffer.bindlessHandle,
        scene->indirectDrawData.boundingSphereDrawBuffer.bindlessHandle,
        drawCommands.bindlessHandle,
        drawCount.bindlessHandle,
        sDrawCount};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, static_cast<u32>(Math::Ceil(sDrawCount / 64.0f)), 1, 1);
}

static void RecordDrawScene(RHI::CommandBuffer& cmd,
                            const RHI::RecordBufferInput* bufferInput,
                            const RHI::RecordTextureInput* textureInput,
                            void* pUserData)
{
    Scene* scene = static_cast<Scene*>(pUserData);

    RHI::BindGraphicsPipeline(cmd, sDrawPipeline);
    RHI::BindIndexBuffer(cmd, scene->indexBuffer, VK_INDEX_TYPE_UINT32);

    RHI::SetViewport(cmd, 0, 0, static_cast<f32>(cmd.device->swapchainWidth),
                     static_cast<f32>(cmd.device->swapchainHeight), 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, cmd.device->swapchainWidth,
                    cmd.device->swapchainHeight);

    RHI::Buffer& uniformBuffer = *(bufferInput->buffers[0]);
    RHI::Buffer& drawCommands = *(bufferInput->buffers[1]);
    RHI::Buffer& drawCountBuffer = *(bufferInput->buffers[2]);

    u32 pushConstants[] = {
        scene->indirectDrawData.instanceDataBuffer.bindlessHandle,
        scene->indirectDrawData.meshDataBuffer.bindlessHandle,
        uniformBuffer.bindlessHandle,
        scene->materialBuffer.bindlessHandle,
    };
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));

    RHI::DrawIndexedIndirectCount(cmd, drawCommands, 0, drawCountBuffer, 0,
                                  sDrawCount,
                                  sizeof(VkDrawIndexedIndirectCommand));
}

static void DrawScene(RHI::Device& device, Scene* scene)
{
    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    RHI::RecordBufferInput bufferInput;
    bufferInput.bufferCount = 3;

    RHI::Buffer** buffers =
        FLY_PUSH_ARENA(arena, RHI::Buffer*, bufferInput.bufferCount);
    buffers[0] = &sUniformBuffers[device.frameIndex];
    buffers[1] = &sDrawCommands;
    buffers[2] = &sDrawCountBuffer;
    bufferInput.buffers = buffers;

    {
        VkAccessFlagBits2* bufferAccesses =
            FLY_PUSH_ARENA(arena, VkAccessFlagBits2, 3);
        bufferAccesses[0] = VK_ACCESS_2_SHADER_READ_BIT;
        bufferAccesses[1] = VK_ACCESS_2_SHADER_WRITE_BIT;
        bufferAccesses[2] = VK_ACCESS_2_SHADER_WRITE_BIT;

        bufferInput.bufferAccesses = bufferAccesses;

        RHI::ExecuteCompute(RenderFrameCommandBuffer(device), RecordFrustumCull,
                            &bufferInput, nullptr, scene);
    }

    {
        VkAccessFlagBits2* bufferAccesses =
            FLY_PUSH_ARENA(arena, VkAccessFlagBits2, bufferInput.bufferCount);
        bufferAccesses[0] = VK_ACCESS_2_SHADER_READ_BIT;
        bufferAccesses[1] = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
        bufferAccesses[2] = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;

        bufferInput.bufferAccesses = bufferAccesses;

        VkRenderingAttachmentInfo colorAttachment = RHI::ColorAttachmentInfo(
            RenderFrameSwapchainTexture(device).imageView);
        VkRenderingAttachmentInfo depthAttachment =
            RHI::DepthAttachmentInfo(sDepthTexture.imageView);
        VkRenderingInfo renderingInfo = RHI::RenderingInfo(
            {{0, 0}, {device.swapchainWidth, device.swapchainHeight}},
            &colorAttachment, 1, &depthAttachment);
        RHI::ExecuteGraphics(RenderFrameCommandBuffer(device), renderingInfo,
                             RecordDrawScene, &bufferInput, nullptr, scene);
    }
    ArenaPopToMarker(arena, marker);
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
        glfwCreateWindow(1280, 720, "Indirect draw", nullptr, nullptr);
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
    settings.vulkan12Features.drawIndirectCount = true;
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
    device.swapchainRecreatedCallback.func = OnFramebufferResize;

    // Scene
    Fly::Scene scene;
    if (!Fly::LoadSceneFromGLTF(arena, device, "BoxTextured.gltf", scene))
    {
        FLY_ERROR("Failed to load gltf");
        return -1;
    }

    // Hack
    RHI::DestroyBuffer(device, scene.indirectDrawData.instanceDataBuffer);

    ArenaMarker marker = ArenaGetMarker(arena);
    InstanceData* instanceData =
        FLY_PUSH_ARENA(arena, InstanceData, sDrawCount);

    for (u32 i = 0; i < sDrawCount; i++)
    {
        f32 randomX = Math::RandomF32(-100.0f, 100.0f);
        f32 randomY = Math::RandomF32(-2.0f, 2.0f);
        f32 randomZ = Math::RandomF32(0.0f, 100.0f);

        instanceData[i].model =
            Math::TranslationMatrix(randomX, randomY, randomZ);
        instanceData[i].meshDataIndex = 0;
    }
    if (!RHI::CreateStorageBuffer(device, false, instanceData,
                                  sizeof(InstanceData) * sDrawCount,
                                  scene.indirectDrawData.instanceDataBuffer))
    {
        return -1;
    }
    ArenaPopToMarker(arena, marker);

    if (!CreatePipelines(device))
    {
        return false;
    }
    if (!CreateResources(device))
    {
        return false;
    }

    // Main Loop
    u64 previousFrameTime = 0;
    u64 loopStartTime = Fly::ClockNow();
    u64 currentFrameTime = loopStartTime;
    sTopCamera.SetPitch(-89.0f);
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        previousFrameTime = currentFrameTime;
        currentFrameTime = Fly::ClockNow();

        f64 deltaTime = Fly::ToSeconds(currentFrameTime - previousFrameTime);

        sCamera.Update(window, deltaTime);
        sTopCamera.SetPosition(sCamera.GetPosition() +
                               Math::Vec3(0.0f, 20.0f, 0.0f));

        UniformData uniformData;
        uniformData.projection = sMainCamera->GetProjection();
        uniformData.view = sMainCamera->GetView();
        uniformData.cullView = sCamera.GetView();
        uniformData.hTanX =
            Math::Tan(Math::Radians(sMainCamera->GetHorizontalFov()) * 0.5f);
        uniformData.hTanY =
            Math::Tan(Math::Radians(sMainCamera->GetVerticalFov()) * 0.5f);
        uniformData.nearPlane = sMainCamera->GetNear();
        uniformData.farPlane = sMainCamera->GetFar();

        RHI::Buffer& cameraBuffer = sUniformBuffers[device.frameIndex];
        RHI::CopyDataToBuffer(device, &uniformData, sizeof(UniformData), 0,
                              cameraBuffer);

        RHI::BeginRenderFrame(device);
        DrawScene(device, &scene);
        RHI::EndRenderFrame(device);
    }

    RHI::WaitAllDevicesIdle(context);
    DestroyResources(device);
    Fly::UnloadScene(device, scene);
    DestroyPipelines(device);
    RHI::DestroyContext(context);

    glfwDestroyWindow(window);
    glfwTerminate();
    FLY_LOG("Shutdown successful");

    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
