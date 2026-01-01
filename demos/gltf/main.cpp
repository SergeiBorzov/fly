#include <stdlib.h>

#include "core/clock.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "rhi/buffer.h"
#include "rhi/context.h"
#include "rhi/pipeline.h"

#include "assets/scene/scene.h"

#include "utils/utils.h"

#include "demos/common/simple_camera_fps.h"

#include <GLFW/glfw3.h>

using namespace Fly;

static RHI::GraphicsPipeline sGraphicsPipeline;
static RHI::Buffer sUniformBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Texture sDepthTexture;

static i32 sCurrentLodLevel = 0;
static Scene sScene;

struct UniformData
{
    Math::Mat4 projection = {};
    Math::Mat4 view = {};
};

static Fly::SimpleCameraFPS sCamera(80.0f, 1280.0f / 720.0f, 0.01f, 100.0f,
                                    Fly::Math::Vec3(0.0f, 0.0f, 5.0f));

static bool ImportNextScene(RHI::Device& device)
{
    static u32 currentSceneIndex = 0;
    vkQueueWaitIdle(device.graphicsComputeQueue);

    static String8 scenes[] = {FLY_STRING8_LITERAL("damaged_helmet.fscene"),
                               FLY_STRING8_LITERAL("sponza.fscene"),
                               FLY_STRING8_LITERAL("a_beautiful_game.fscene")};

    currentSceneIndex = (currentSceneIndex + 1) % STACK_ARRAY_COUNT(scenes);

    return ImportScene(scenes[currentSceneIndex], device, sScene);
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
        RHI::Device* pDevice =
            static_cast<RHI::Device*>(glfwGetWindowUserPointer(window));
        if (!ImportNextScene(*pDevice))
        {
            exit(-1);
        }
    }
    if (key == GLFW_KEY_K && action == GLFW_PRESS)
    {
        sCurrentLodLevel = Math::Max(sCurrentLodLevel - 1, 0);
    }
    if (key == GLFW_KEY_L && action == GLFW_PRESS)
    {
        sCurrentLodLevel = Math::Min(sCurrentLodLevel + 1,
                                     static_cast<i32>(FLY_MAX_LOD_COUNT) - 1);
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
    RHI::Shader shaders[2];
    String8 shaderPaths[2] = {FLY_STRING8_LITERAL("unlit.vert.spv"),
                              FLY_STRING8_LITERAL("unlit.frag.spv")};

    for (u32 i = 0; i < STACK_ARRAY_COUNT(shaders); i++)
    {
        if (!Fly::LoadShaderFromSpv(device, shaderPaths[i], shaders[i]))
        {
            return false;
        }
    }

    RHI::GraphicsPipelineFixedStateStage fixedState{};
    fixedState.pipelineRendering.colorAttachments[0] =
        device.surfaceFormat.format;
    fixedState.pipelineRendering.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.colorBlendState.attachmentCount = 1;
    fixedState.depthStencilState.depthTestEnable = true;
    fixedState.rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    fixedState.rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaders,
                                     STACK_ARRAY_COUNT(shaders),
                                     sGraphicsPipeline))
    {
        FLY_ERROR("Failed to create graphics pipeline");
        return false;
    }

    for (u32 i = 0; i < STACK_ARRAY_COUNT(shaders); i++)
    {
        RHI::DestroyShader(device, shaders[i]);
    }

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
            device.swapchainWidth, device.swapchainHeight, VK_FORMAT_D32_SFLOAT,
            RHI::Sampler::FilterMode::Nearest, RHI::Sampler::WrapMode::Repeat,
            1, sDepthTexture))
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
    RHI::SetViewport(cmd, 0, 0, static_cast<f32>(cmd.device->swapchainWidth),
                     static_cast<f32>(cmd.device->swapchainHeight), 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, cmd.device->swapchainWidth,
                    cmd.device->swapchainHeight);

    const RHI::Buffer& cameraBuffer = *(bufferInput[0].pBuffer);
    // const RHI::Buffer& materialBuffer = scene->materialBuffer;

    RHI::BindGraphicsPipeline(cmd, sGraphicsPipeline);
    RHI::BindIndexBuffer(cmd, sScene.indexBuffer, VK_INDEX_TYPE_UINT32);

    u32 pushConstants[] = {cameraBuffer.bindlessHandle,
                           sScene.vertexBuffer.bindlessHandle,
                           sScene.pbrMaterialBuffer.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants),
                       sizeof(Math::Mat4));

    for (u32 i = 0; i < sScene.nodeCount; i++)
    {
        SceneNode& node = sScene.nodes[i];
        if (node.mesh)
        {
            const Math::Mat4& model = node.transform.GetWorldMatrix();
            RHI::PushConstants(cmd, &model, sizeof(Math::Mat4));
            for (u32 j = 0; j < node.mesh->submeshCount; j++)
            {
                i32 materialIndex = node.mesh->submeshes[j].materialIndex;
                RHI::PushConstants(cmd, &materialIndex, sizeof(materialIndex),
                                   sizeof(Math::Mat4) + sizeof(u32) * 3);
                i32 lodLevel =
                    Math::Min(sCurrentLodLevel,
                              static_cast<i32>(node.mesh->lodCount) - 1);
                RHI::DrawIndexed(
                    cmd, node.mesh->submeshes[j].lods[lodLevel].indexCount, 1,
                    node.mesh->submeshes[j].lods[lodLevel].firstIndex,
                    node.mesh->vertexOffset, 0);
            }
        }
    }
}

static void DrawScene(RHI::Device& device)
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
                         RecordDrawScene, &bufferInput, 1);
}

int main(int argc, char* argv[])
{
    InitThreadContext();
    Arena& arena = GetScratchArena();
    if (!InitLogger())
    {
        return -1;
    }

    sCamera.speed = 3.0f;

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
        glfwCreateWindow(1280, 720, "Gltf demo", nullptr, nullptr);
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
    settings.vulkan12Features.shaderFloat16 = true;
    settings.vulkan11Features.storageBuffer16BitAccess = true;

    RHI::Context context;
    if (!RHI::CreateContext(settings, context))
    {
        FLY_ERROR("Failed to create context");
        return -1;
    }
    RHI::Device& device = context.devices[0];
    device.swapchainRecreatedCallback.func = OnFramebufferResize;
    glfwSetWindowUserPointer(window, &device);

    if (!CreatePipeline(device))
    {
        return -1;
    }

    if (!CreateResources(device))
    {
        return -1;
    }

    if (!ImportNextScene(device))
    {
        return false;
    }

    FLY_LOG("Scene has %u textures", sScene.textureCount);
    FLY_LOG("Scene has %u meshes", sScene.meshCount);
    FLY_LOG("Scene has %u nodes", sScene.nodeCount);
    FLY_LOG("Scene has %u materials", sScene.materialCount);
    for (u32 i = 0; i < sScene.meshCount; i++)
    {
        FLY_LOG("Mesh %u has %u submeshes", i, sScene.meshes[i].submeshCount);
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
        DrawScene(device);
        RHI::EndRenderFrame(device);
    }

    RHI::WaitAllDevicesIdle(context);
    DestroyResources(device);
    DestroyScene(device, sScene);
    DestroyPipeline(device);
    RHI::DestroyContext(context);

    glfwDestroyWindow(window);
    glfwTerminate();
    FLY_LOG("Shutdown successful");

    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
