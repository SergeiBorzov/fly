#include "core/assert.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "math/vec.h"

#include "rhi/buffer.h"
#include "rhi/command_buffer.h"
#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"
#include "rhi/texture.h"

#include "utils/utils.h"

#include <GLFW/glfw3.h>

using namespace Fly;

#define TRANSMITTANCE_LUT_WIDTH 1024
#define TRANSMITTANCE_LUT_HEIGHT 256

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
};
static AtmosphereParams sAtmosphereParams;

static RHI::GraphicsPipeline sScreenQuadPipeline;
static RHI::ComputePipeline sTransmittancePipeline;
static RHI::Buffer sAtmosphereParamsBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Texture sTransmittanceLUT;

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
        &sTransmittancePipeline,
    };

    const char* computeShaderPaths[] = {
        "transmittance_lut.comp.spv",
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
    // fixedState.rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

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
    RHI::DestroyComputePipeline(device, sTransmittancePipeline);
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
    }

    if (!RHI::CreateTexture2D(
            device, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            nullptr, TRANSMITTANCE_LUT_WIDTH, TRANSMITTANCE_LUT_HEIGHT,
            VK_FORMAT_R16G16B16A16_SFLOAT, RHI::Sampler::FilterMode::Bilinear,
            RHI::Sampler::WrapMode::Clamp, 1, sTransmittanceLUT))
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
    }
    RHI::DestroyTexture(device, sTransmittanceLUT);
}

static void RecordComputeTransmittance(
    RHI::CommandBuffer& cmd, const RHI::RecordBufferInput* bufferInput,
    const RHI::RecordTextureInput* textureInput, void* pUserData)
{
    RHI::BindComputePipeline(cmd, sTransmittancePipeline);

    RHI::Buffer& atmosphereParams = *(bufferInput->buffers[0]);
    RHI::Texture& transmittanceLUT = *(textureInput->textures[0]);

    u32 pushConstants[] = {
        atmosphereParams.bindlessHandle,
        transmittanceLUT.bindlessStorageHandle,
    };
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, TRANSMITTANCE_LUT_WIDTH / 16,
                  TRANSMITTANCE_LUT_HEIGHT / 16, 1);
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
    sAtmosphereParams.ozoneAbsorption = Math::Vec3(0.00065f, 0.001881f, 0.000085f);
    sAtmosphereParams.mieAbsorption = 0.0044f;
    sAtmosphereParams.transmittanceMapDims =
        Math::Vec2(TRANSMITTANCE_LUT_WIDTH, TRANSMITTANCE_LUT_HEIGHT);
    sAtmosphereParams.radiusBottom = EARTH_RADIUS_BOTTOM;
    sAtmosphereParams.radiusTop = EARTH_RADIUS_TOP;

    if (!CreateResources(device))
    {
        FLY_ERROR("Failed to create resources");
        return -1;
    }

    RHI::RecordBufferInput bufferInput;
    RHI::RecordTextureInput textureInput;
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

        RHI::BeginTransfer(device);
        RHI::ExecuteCompute(TransferCommandBuffer(device),
                            RecordComputeTransmittance, &bufferInput,
                            &textureInput);
        RHI::EndTransfer(device);
    }

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        RHI::BeginRenderFrame(device);
        {
            RHI::Texture* pTransmittanceLUT = &sTransmittanceLUT;
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
