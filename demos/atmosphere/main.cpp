#include "core/assert.h"
#include "core/clock.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "math/mat.h"

#include "rhi/buffer.h"
#include "rhi/command_buffer.h"
#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"
#include "rhi/texture.h"

#include "utils/utils.h"

#include "demos/common/simple_camera_fps.h"

#include <GLFW/glfw3.h>

using namespace Fly;

#define TRANSMITTANCE_LUT_WIDTH 4096
#define TRANSMITTANCE_LUT_HEIGHT 256
#define MULTISCATTERING_LUT_WIDTH 256
#define MULTISCATTERING_LUT_HEIGHT 256
#define SKYVIEW_LUT_WIDTH 2048
#define SKYVIEW_LUT_HEIGHT 512

#define EARTH_RADIUS_BOTTOM 6360.0f
#define EARTH_RADIUS_TOP 6460.0f

struct AtmosphereParams
{
    Math::Vec3 rayleighScattering;
    float mieScattering;
    Math::Vec3 ozoneAbsorption;
    float mieAbsorption;
    Math::Vec2 transmittanceMapDims;
    float radiusBottom;
    float radiusTop;
    Math::Vec2 multiscatteringMapDims;
    Math::Vec2 skyviewMapDims;
    float zenith;
    float azimuth;
};

struct CameraParams
{
    Math::Mat4 projection;
    Math::Mat4 view;
};

static Fly::SimpleCameraFPS sCamera(90.0f, 1280.0f / 720.0f, 0.01f, 1000.0f,
                                    Math::Vec3(0.0f, 200.0f, -5.0f));

static AtmosphereParams sAtmosphereParams;
static CameraParams sCameraParams;

static RHI::GraphicsPipeline sScreenQuadPipeline;
static RHI::ComputePipeline sTransmittancePipeline;
static RHI::ComputePipeline sMultiscatteringPipeline;
static RHI::ComputePipeline sSkyviewPipeline;

static RHI::Buffer sAtmosphereParamsBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sCameraBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Texture sTransmittanceLUT;
static RHI::Texture sMultiscatteringLUT;
static RHI::Texture sSkyviewLUT;

static void OnKeyboardPressed(GLFWwindow* window, int key, int scancode,
                              int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }
}

static void ErrorCallbackGLFW(int error, const char* description)
{
    FLY_ERROR("GLFW - error: %s", description);
}

static bool CreatePipelines(RHI::Device& device)
{
    RHI::ComputePipeline* computePipelines[] = {
        &sTransmittancePipeline, &sMultiscatteringPipeline, &sSkyviewPipeline};

    const char* computeShaderPaths[] = {
        "transmittance_lut.comp.spv",
        "multiscattering_lut.comp.spv",
        "skyview_lut.comp.spv",
    };

    for (u32 i = 0; i < STACK_ARRAY_COUNT(computeShaderPaths); i++)
    {
        RHI::Shader shader;
        if (!Fly::LoadShaderFromSpv(device, computeShaderPaths[i], shader))
        {
            return false;
        }
        if (!RHI::CreateComputePipeline(device, shader, *computePipelines[i]))
        {
            return false;
        }
        RHI::DestroyShader(device, shader);
    }

    RHI::GraphicsPipelineFixedStateStage fixedState{};
    fixedState.pipelineRendering.colorAttachments[0] =
        device.surfaceFormat.format;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.colorBlendState.attachmentCount = 1;
    fixedState.rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

    RHI::ShaderProgram shaderProgram;
    if (!Fly::LoadShaderFromSpv(device, "screen_quad.vert.spv",
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }

    if (!Fly::LoadShaderFromSpv(device, "screen_quad.frag.spv",
                                shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
    }

    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     sScreenQuadPipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    return true;
}

static void DestroyPipelines(RHI::Device& device)
{
    RHI::DestroyGraphicsPipeline(device, sScreenQuadPipeline);
    RHI::DestroyComputePipeline(device, sMultiscatteringPipeline);
    RHI::DestroyComputePipeline(device, sTransmittancePipeline);
    RHI::DestroyComputePipeline(device, sSkyviewPipeline);
}

static bool CreateResources(RHI::Device& device)
{
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateBuffer(device, true, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                               &sAtmosphereParams, sizeof(AtmosphereParams),
                               sAtmosphereParamsBuffers[i]))
        {
            return false;
        }

        if (!RHI::CreateBuffer(device, true, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                               nullptr, sizeof(CameraParams),
                               sCameraBuffers[i]))
        {
            return false;
        }
    }

    if (!RHI::CreateTexture2D(
            device, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            nullptr, TRANSMITTANCE_LUT_WIDTH, TRANSMITTANCE_LUT_HEIGHT,
            VK_FORMAT_R16G16B16A16_SFLOAT, RHI::Sampler::FilterMode::Nearest,
            RHI::Sampler::WrapMode::Clamp, 1, sTransmittanceLUT))
    {
        return false;
    }

    if (!RHI::CreateTexture2D(
            device, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            nullptr, MULTISCATTERING_LUT_WIDTH, MULTISCATTERING_LUT_HEIGHT,
            VK_FORMAT_R16G16B16A16_SFLOAT, RHI::Sampler::FilterMode::Nearest,
            RHI::Sampler::WrapMode::Clamp, 1, sMultiscatteringLUT))
    {
        return false;
    }

    if (!RHI::CreateTexture2D(
            device, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            nullptr, SKYVIEW_LUT_WIDTH, SKYVIEW_LUT_HEIGHT,
            VK_FORMAT_R16G16B16A16_SFLOAT, RHI::Sampler::FilterMode::Bilinear,
            RHI::Sampler::WrapMode::Clamp, 1, sSkyviewLUT))
    {
        return false;
    }

    return true;
}

static void DestroyResources(RHI::Device& device)
{
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        RHI::DestroyBuffer(device, sAtmosphereParamsBuffers[i]);
        RHI::DestroyBuffer(device, sCameraBuffers[i]);
    }
    RHI::DestroyTexture(device, sTransmittanceLUT);
    RHI::DestroyTexture(device, sMultiscatteringLUT);
    RHI::DestroyTexture(device, sSkyviewLUT);
}

static void RecordComputeTransmittance(
    RHI::CommandBuffer& cmd, const RHI::RecordBufferInput* bufferInput,
    const RHI::RecordTextureInput* textureInput, void* pUserData)
{
    RHI::BindComputePipeline(cmd, sTransmittancePipeline);

    RHI::Buffer& atmosphereParams = *(bufferInput->buffers[0]);
    RHI::Texture& transmittanceLUT = *(textureInput->textures[0]);

    u32 pushConstants[] = {atmosphereParams.bindlessHandle,
                           transmittanceLUT.bindlessStorageHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, TRANSMITTANCE_LUT_WIDTH / 16,
                  TRANSMITTANCE_LUT_HEIGHT / 16, 1);
}

static void RecordComputeMultiscattering(
    RHI::CommandBuffer& cmd, const RHI::RecordBufferInput* bufferInput,
    const RHI::RecordTextureInput* textureInput, void* pUserData)
{
    RHI::BindComputePipeline(cmd, sMultiscatteringPipeline);

    RHI::Buffer& atmosphereParams = *(bufferInput->buffers[0]);
    RHI::Texture& transmittanceLUT = *(textureInput->textures[0]);
    RHI::Texture& multiscatteringLUT = *(textureInput->textures[1]);

    u32 pushConstants[] = {atmosphereParams.bindlessHandle,
                           transmittanceLUT.bindlessHandle,
                           multiscatteringLUT.bindlessStorageHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, MULTISCATTERING_LUT_WIDTH / 16,
                  MULTISCATTERING_LUT_HEIGHT / 16, 1);
}

static void RecordComputeSkyview(RHI::CommandBuffer& cmd,
                                 const RHI::RecordBufferInput* bufferInput,
                                 const RHI::RecordTextureInput* textureInput,
                                 void* pUserData)
{
    RHI::BindComputePipeline(cmd, sSkyviewPipeline);

    RHI::Buffer& atmosphereParams = *(bufferInput->buffers[0]);
    RHI::Buffer& cameraParams = *(bufferInput->buffers[1]);
    RHI::Texture& transmittanceLUT = *(textureInput->textures[0]);
    RHI::Texture& multiscatteringLUT = *(textureInput->textures[1]);
    RHI::Texture& skyviewLUT = *(textureInput->textures[2]);

    u32 pushConstants[] = {
        atmosphereParams.bindlessHandle, cameraParams.bindlessHandle,
        transmittanceLUT.bindlessHandle, multiscatteringLUT.bindlessHandle,
        skyviewLUT.bindlessStorageHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, SKYVIEW_LUT_WIDTH / 16, SKYVIEW_LUT_HEIGHT / 16, 1);
}

static void RecordDrawScreenQuad(RHI::CommandBuffer& cmd,
                                 const RHI::RecordBufferInput* bufferInput,
                                 const RHI::RecordTextureInput* textureInput,
                                 void* pUserData)
{
    RHI::SetViewport(cmd, 0.0f, 0.0f,
                     static_cast<f32>(cmd.device->swapchainWidth),
                     static_cast<f32>(cmd.device->swapchainHeight), 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, cmd.device->swapchainWidth,
                    cmd.device->swapchainHeight);

    RHI::BindGraphicsPipeline(cmd, sScreenQuadPipeline);
    RHI::Texture& transmittanceLUT = *(textureInput->textures[0]);
    u32 pushConstants[] = {transmittanceLUT.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Draw(cmd, 6, 1, 0, 0);
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

    if (!CreatePipelines(device))
    {
        FLY_ERROR("Failed to create pipelines");
        return -1;
    }

    sAtmosphereParams.rayleighScattering =
        Math::Vec3(0.005802f, 0.013558f, 0.0331f);
    sAtmosphereParams.mieScattering = 0.003996f;
    sAtmosphereParams.ozoneAbsorption =
        Math::Vec3(0.00065f, 0.001881f, 0.000085f);
    sAtmosphereParams.mieAbsorption = 0.0044f;
    sAtmosphereParams.transmittanceMapDims =
        Math::Vec2(TRANSMITTANCE_LUT_WIDTH, TRANSMITTANCE_LUT_HEIGHT);
    sAtmosphereParams.radiusBottom = EARTH_RADIUS_BOTTOM;
    sAtmosphereParams.radiusTop = EARTH_RADIUS_TOP;
    sAtmosphereParams.multiscatteringMapDims =
        Math::Vec2(MULTISCATTERING_LUT_WIDTH, MULTISCATTERING_LUT_HEIGHT);
    sAtmosphereParams.skyviewMapDims =
        Math::Vec2(SKYVIEW_LUT_WIDTH, SKYVIEW_LUT_HEIGHT);
    sAtmosphereParams.zenith = 45.0f;
    sAtmosphereParams.azimuth = 0.0f;

    if (!CreateResources(device))
    {
        FLY_ERROR("Failed to create resources");
        return -1;
    }

    RHI::RecordBufferInput bufferInput;
    RHI::RecordTextureInput textureInput;
    RHI::BeginTransfer(device);
    {
        RHI::Buffer* pAtmosphereParamsBuffer =
            &sAtmosphereParamsBuffers[device.frameIndex];
        VkAccessFlagBits2 bufferAccess = VK_ACCESS_2_SHADER_READ_BIT;
        bufferInput.buffers = &pAtmosphereParamsBuffer;
        bufferInput.bufferAccesses = &bufferAccess;
        bufferInput.bufferCount = 1;

        RHI::Texture* pTransmittanceLUT = &sTransmittanceLUT;
        RHI::ImageLayoutAccess imageLayoutAccess;
        imageLayoutAccess.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageLayoutAccess.accessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
        textureInput.textures = &pTransmittanceLUT;
        textureInput.imageLayoutsAccesses = &imageLayoutAccess;
        textureInput.textureCount = 1;

        RHI::ExecuteCompute(TransferCommandBuffer(device),
                            RecordComputeTransmittance, &bufferInput,
                            &textureInput);
    }
    {
        RHI::Buffer* pAtmosphereParamsBuffer =
            &sAtmosphereParamsBuffers[device.frameIndex];
        VkAccessFlagBits2 bufferAccess = VK_ACCESS_2_SHADER_READ_BIT;
        bufferInput.buffers = &pAtmosphereParamsBuffer;
        bufferInput.bufferAccesses = &bufferAccess;
        bufferInput.bufferCount = 1;

        RHI::Texture* textures[2];
        RHI::ImageLayoutAccess imageLayoutsAccesses[2];

        textureInput.textureCount = 2;
        textureInput.textures = textures;
        textureInput.imageLayoutsAccesses = imageLayoutsAccesses;

        textures[0] = &sTransmittanceLUT;
        imageLayoutsAccesses[0].imageLayout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageLayoutsAccesses[0].accessMask = VK_ACCESS_2_SHADER_READ_BIT;

        textures[1] = &sMultiscatteringLUT;
        imageLayoutsAccesses[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageLayoutsAccesses[0].accessMask = VK_ACCESS_2_SHADER_WRITE_BIT;

        RHI::ExecuteCompute(TransferCommandBuffer(device),
                            RecordComputeMultiscattering, &bufferInput,
                            &textureInput);
    }
    {
        RHI::Buffer* buffers[2];
        VkAccessFlagBits2 bufferAccesses[2];
        bufferInput.bufferCount = 2;
        bufferInput.buffers = buffers;
        bufferInput.bufferAccesses = bufferAccesses;

        buffers[0] = &sAtmosphereParamsBuffers[device.frameIndex];
        bufferAccesses[0] = VK_ACCESS_2_SHADER_READ_BIT;
        buffers[1] = &sCameraBuffers[device.frameIndex];
        bufferAccesses[1] = VK_ACCESS_2_SHADER_READ_BIT;

        RHI::Texture* textures[3];
        RHI::ImageLayoutAccess imageLayoutsAccesses[2];
        textureInput.textureCount = 3;
        textureInput.textures = textures;
        textureInput.imageLayoutsAccesses = imageLayoutsAccesses;

        textures[0] = &sTransmittanceLUT;
        imageLayoutsAccesses[0].imageLayout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageLayoutsAccesses[0].accessMask = VK_ACCESS_2_SHADER_READ_BIT;

        textures[1] = &sMultiscatteringLUT;
        imageLayoutsAccesses[1].imageLayout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageLayoutsAccesses[1].accessMask = VK_ACCESS_2_SHADER_READ_BIT;

        textures[2] = &sSkyviewLUT;
        imageLayoutsAccesses[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageLayoutsAccesses[0].accessMask = VK_ACCESS_2_SHADER_WRITE_BIT;

        RHI::ExecuteCompute(TransferCommandBuffer(device), RecordComputeSkyview,
                            &bufferInput, &textureInput);
    }
    RHI::EndTransfer(device);

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

        {
            sCamera.Update(window, deltaTime);
            CameraParams cameraParams = {sCamera.GetProjection(),
                                         sCamera.GetView()};
            RHI::Buffer& cameraBuffer = sCameraBuffers[device.frameIndex];
            RHI::CopyDataToBuffer(device, &cameraParams, sizeof(CameraParams),
                                  0, cameraBuffer);
        }

        RHI::BeginRenderFrame(device);
        {
            // RHI::Texture* pTransmittanceLUT = &sTransmittanceLUT;
            RHI::Texture* pTransmittanceLUT = &sSkyviewLUT;
            // RHI::Texture* pTransmittanceLUT = &sSkyviewLUT;
            RHI::ImageLayoutAccess imageLayoutAccess;
            imageLayoutAccess.imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageLayoutAccess.accessMask = VK_ACCESS_2_SHADER_READ_BIT;
            textureInput.textures = &pTransmittanceLUT;
            textureInput.imageLayoutsAccesses = &imageLayoutAccess;
            textureInput.textureCount = 1;

            VkRenderingAttachmentInfo colorAttachment =
                RHI::ColorAttachmentInfo(
                    RenderFrameSwapchainTexture(device).imageView);
            VkRenderingInfo renderingInfo = RHI::RenderingInfo(
                {{0, 0}, {device.swapchainWidth, device.swapchainHeight}},
                &colorAttachment, 1, nullptr, nullptr, 1, 0);
            RHI::ExecuteGraphics(RenderFrameCommandBuffer(device),
                                 renderingInfo, RecordDrawScreenQuad, nullptr,
                                 &textureInput);
        }
        RHI::EndRenderFrame(device);
    }

    RHI::WaitDeviceIdle(device);

    DestroyResources(device);
    DestroyPipelines(device);
    RHI::DestroyContext(context);

    FLY_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
