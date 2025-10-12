#include "core/assert.h"
#include "core/clock.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "rhi/buffer.h"
#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"
#include "rhi/texture.h"

#include "math/vec.h"
#include "utils/utils.h"

#include "demos/common/simple_camera_fps.h"

#include <GLFW/glfw3.h>

using namespace Fly;

#define IRRADIANCE_MAP_SIZE 256
#define RADIANCE_MAP_COUNT 4

static RHI::GraphicsPipeline sDrawCubemapPipeline;
static RHI::GraphicsPipeline sConvoluteIrradianceSlowPipeline;
static RHI::Buffer sCameraBuffers[FLY_FRAME_IN_FLIGHT_COUNT];

static RHI::Texture sRadianceMaps[RADIANCE_MAP_COUNT];
static RHI::Texture sIrradianceMaps[RADIANCE_MAP_COUNT];

static u32 sCurrentCubemapIndex = 0;
static RHI::Texture* sCurrentCubemap = &sRadianceMaps[sCurrentCubemapIndex];

struct CameraData
{
    Math::Mat4 projection = {};
    Math::Mat4 view = {};
    Math::Vec4 screenSize;
};

static Fly::SimpleCameraFPS sCamera(90.0f, 1024.0f / 1024.0f, 0.01f, 1000.0f,
                                    Math::Vec3(0.0f, 0.0f, 0.0f));

static void OnKeyboardPressed(GLFWwindow* window, int key, int scancode,
                              int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }

    if (key == GLFW_KEY_1 && action == GLFW_PRESS)
    {
        sCurrentCubemap = &sRadianceMaps[sCurrentCubemapIndex];
    }
    else if (key == GLFW_KEY_2 && action == GLFW_PRESS)
    {
        sCurrentCubemap = &sIrradianceMaps[sCurrentCubemapIndex];
    }

    if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
    {
        sCurrentCubemapIndex = (sCurrentCubemapIndex + 1) % RADIANCE_MAP_COUNT;
        sCurrentCubemap = &sRadianceMaps[sCurrentCubemapIndex];
    }
}

static void ErrorCallbackGLFW(i32 error, const char* description)
{
    FLY_ERROR("GLFW - error: %s", description);
}

static bool CreatePipeline(RHI::Device& device)
{
    RHI::ShaderProgram shaderProgram{};
    if (!Fly::LoadShaderFromSpv(device, "sample_cubemap.vert.spv",
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }
    if (!Fly::LoadShaderFromSpv(device, "sample_cubemap.frag.spv",
                                shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
    }

    RHI::GraphicsPipelineFixedStateStage fixedState{};
    fixedState.pipelineRendering.colorAttachments[0] =
        device.surfaceFormat.format;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.colorBlendState.attachmentCount = 1;
    fixedState.rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     sDrawCubemapPipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    if (!Fly::LoadShaderFromSpv(device, "irradiance_slow.frag.spv",
                                shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
    }
    fixedState.pipelineRendering.colorAttachments[0] =
        VK_FORMAT_R16G16B16A16_SFLOAT;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.pipelineRendering.viewMask = 0x3F;

    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     sConvoluteIrradianceSlowPipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);

    return true;
}

static void DestroyPipeline(RHI::Device& device)
{
    RHI::DestroyGraphicsPipeline(device, sConvoluteIrradianceSlowPipeline);
    RHI::DestroyGraphicsPipeline(device, sDrawCubemapPipeline);
}

static bool CreateResources(RHI::Device& device)
{
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateBuffer(device, true, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                               nullptr, sizeof(CameraData), sCameraBuffers[i]))
        {
            FLY_ERROR("Failed to create uniform buffer %u", i);
            return false;
        }
    }

    const char* radianceMapPaths[RADIANCE_MAP_COUNT] = {
        "Night.exr", "Tunnel.exr", "Forest.exr", "Night2.exr"};

    for (u32 i = 0; i < RADIANCE_MAP_COUNT; i++)
    {
        if (!LoadCubemap(
                device, radianceMapPaths[i], VK_FORMAT_R16G16B16A16_SFLOAT,
                RHI::Sampler::FilterMode::Bilinear, 1, sRadianceMaps[i]))
        {
            return false;
        }

        if (!RHI::CreateCubemap(
                device,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT,
                nullptr, IRRADIANCE_MAP_SIZE, VK_FORMAT_R16G16B16A16_SFLOAT,
                RHI::Sampler::FilterMode::Bilinear, 1, sIrradianceMaps[i]))
        {
            return false;
        }
    }

    return true;
}

static void DestroyResources(RHI::Device& device)
{
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        RHI::DestroyBuffer(device, sCameraBuffers[i]);
    }

    for (u32 i = 0; i < RADIANCE_MAP_COUNT; i++)
    {
        RHI::DestroyTexture(device, sRadianceMaps[i]);
        RHI::DestroyTexture(device, sIrradianceMaps[i]);
    }
}

static void RecordConvoluteIrradianceSlow(
    RHI::CommandBuffer& cmd, const RHI::RecordBufferInput* bufferInput,
    const RHI::RecordTextureInput* textureInput, void* pUserData)
{
    RHI::SetViewport(cmd, 0, 0, IRRADIANCE_MAP_SIZE, IRRADIANCE_MAP_SIZE, 0.0f,
                     1.0f);
    RHI::SetScissor(cmd, 0, 0, IRRADIANCE_MAP_SIZE, IRRADIANCE_MAP_SIZE);
    RHI::BindGraphicsPipeline(cmd, sConvoluteIrradianceSlowPipeline);

    RHI::Buffer& cameraBuffer = *(bufferInput->buffers[0]);
    RHI::Texture& radianceMap = *(textureInput->textures[0]);

    u32 pushConstants[] = {cameraBuffer.bindlessHandle,
                           radianceMap.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Draw(cmd, 6, 1, 0, 0);
}

static void ConvoluteIrradianceSlow(RHI::Device& device)
{
    for (u32 i = 0; i < RADIANCE_MAP_COUNT; i++)
    {
        RHI::RecordBufferInput bufferInput;
        RHI::Buffer* pCameraBuffer = &sCameraBuffers[device.frameIndex];
        VkAccessFlagBits2 bufferAccess = VK_ACCESS_2_SHADER_READ_BIT;
        bufferInput.buffers = &pCameraBuffer;
        bufferInput.bufferAccesses = &bufferAccess;
        bufferInput.bufferCount = 1;

        RHI::RecordTextureInput textureInput;
        RHI::Texture* textures[2];
        RHI::ImageLayoutAccess imageLayoutsAccesses[2];
        textureInput.textures = textures;
        textureInput.imageLayoutsAccesses = imageLayoutsAccesses;
        textureInput.textureCount = 2;

        textures[0] = &sRadianceMaps[i];
        imageLayoutsAccesses[0].imageLayout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageLayoutsAccesses[0].accessMask = VK_ACCESS_2_SHADER_READ_BIT;

        textures[1] = &sIrradianceMaps[i];
        imageLayoutsAccesses[1].imageLayout =
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        imageLayoutsAccesses[1].accessMask =
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderingAttachmentInfo colorAttachment =
            RHI::ColorAttachmentInfo(sIrradianceMaps[i].arrayImageView);
        VkRenderingInfo renderingInfo = RHI::RenderingInfo(
            {{0, 0}, {IRRADIANCE_MAP_SIZE, IRRADIANCE_MAP_SIZE}},
            &colorAttachment, 1, nullptr, nullptr, 6, 0x3F);

        RHI::ExecuteGraphics(OneTimeSubmitCommandBuffer(device), renderingInfo,
                             RecordConvoluteIrradianceSlow, &bufferInput,
                             &textureInput);
    }
}

static void RecordDrawCubemap(RHI::CommandBuffer& cmd,
                              const RHI::RecordBufferInput* bufferInput,
                              const RHI::RecordTextureInput* textureInput,
                              void* pUserData)
{
    RHI::SetViewport(cmd, 0, 0, static_cast<f32>(cmd.device->swapchainWidth),
                     static_cast<f32>(cmd.device->swapchainHeight), 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, cmd.device->swapchainWidth,
                    cmd.device->swapchainHeight);

    RHI::BindGraphicsPipeline(cmd, sDrawCubemapPipeline);

    RHI::Buffer& cameraBuffer = *(bufferInput->buffers[0]);
    RHI::Texture& cubemap = *(textureInput->textures[0]);

    u32 pushConstants[] = {cameraBuffer.bindlessHandle, cubemap.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Draw(cmd, 6, 1, 0, 0);
}

static void DrawCubemap(RHI::Device& device)
{
    RHI::RecordBufferInput bufferInput;
    RHI::Buffer* pCameraBuffer = &sCameraBuffers[device.frameIndex];
    VkAccessFlagBits2 bufferAccess = VK_ACCESS_2_SHADER_READ_BIT;
    bufferInput.buffers = &pCameraBuffer;
    bufferInput.bufferAccesses = &bufferAccess;
    bufferInput.bufferCount = 1;

    RHI::RecordTextureInput textureInput;
    RHI::ImageLayoutAccess imageLayoutAccess;
    imageLayoutAccess.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageLayoutAccess.accessMask = VK_ACCESS_2_SHADER_READ_BIT;
    textureInput.textures = &sCurrentCubemap;
    textureInput.imageLayoutsAccesses = &imageLayoutAccess;
    textureInput.textureCount = 1;

    VkRenderingAttachmentInfo colorAttachment =
        RHI::ColorAttachmentInfo(RenderFrameSwapchainTexture(device).imageView);
    VkRenderingInfo renderingInfo = RHI::RenderingInfo(
        {{0, 0}, {device.swapchainWidth, device.swapchainHeight}},
        &colorAttachment, 1);
    RHI::ExecuteGraphics(RenderFrameCommandBuffer(device), renderingInfo,
                         RecordDrawCubemap, &bufferInput, &textureInput);
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
    GLFWwindow* window =
        glfwCreateWindow(1024, 1024, "Irradiance map", nullptr, nullptr);
    if (!window)
    {
        FLY_ERROR("Failed to create glfw window");
        glfwTerminate();
        return -1;
    }
    glfwSetKeyCallback(window, OnKeyboardPressed);

    const char* requiredDeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    RHI::ContextSettings settings{};
    settings.instanceExtensions =
        glfwGetRequiredInstanceExtensions(&settings.instanceExtensionCount);
    settings.deviceExtensions = requiredDeviceExtensions;
    settings.deviceExtensionCount = STACK_ARRAY_COUNT(requiredDeviceExtensions);
    settings.windowPtr = window;
    settings.vulkan11Features.multiview = true;

    RHI::Context context;
    if (!RHI::CreateContext(settings, context))
    {
        FLY_ERROR("Failed to create context");
        return -1;
    }

    RHI::Device& device = context.devices[0];

    if (!CreatePipeline(device))
    {
        FLY_ERROR("Failed to create pipeline");
        return -1;
    }

    if (!CreateResources(device))
    {
        return -1;
    }

    Math::Vec4 screenSize =
        Math::Vec4(IRRADIANCE_MAP_SIZE, IRRADIANCE_MAP_SIZE,
                   1.0f / IRRADIANCE_MAP_SIZE, 1.0f / IRRADIANCE_MAP_SIZE);
    CameraData cameraData = {sCamera.GetProjection(), sCamera.GetView(),
                             screenSize};
    RHI::Buffer& cameraBuffer = sCameraBuffers[device.frameIndex];
    RHI::CopyDataToBuffer(device, &cameraData, sizeof(CameraData), 0,
                          cameraBuffer);
    RHI::BeginOneTimeSubmit(device);
    ConvoluteIrradianceSlow(device);
    RHI::EndOneTimeSubmit(device);

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
        Math::Vec4 screenSize = Math::Vec4(
            static_cast<f32>(device.swapchainWidth),
            static_cast<f32>(device.swapchainHeight),
            1.0f / device.swapchainWidth, 1.0f / device.swapchainHeight);

        CameraData cameraData = {sCamera.GetProjection(), sCamera.GetView(),
                                 screenSize};

        RHI::Buffer& cameraBuffer = sCameraBuffers[device.frameIndex];
        RHI::CopyDataToBuffer(device, &cameraData, sizeof(CameraData), 0,
                              cameraBuffer);

        RHI::BeginRenderFrame(device);
        DrawCubemap(device);
        RHI::EndRenderFrame(device);
    }

    RHI::WaitDeviceIdle(device);

    DestroyPipeline(device);
    DestroyResources(device);
    RHI::DestroyContext(context);

    glfwDestroyWindow(window);
    glfwTerminate();
    FLY_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
