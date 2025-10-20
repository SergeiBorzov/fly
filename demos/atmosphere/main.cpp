#include "core/assert.h"
#include "core/clock.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "math/mat.h"

#include "rhi/allocation_callbacks.h"
#include "rhi/buffer.h"
#include "rhi/command_buffer.h"
#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"
#include "rhi/texture.h"

#include "utils/utils.h"

#include "demos/common/simple_camera_fps.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>

using namespace Fly;

#define SCALE 1e8f

#define TRANSMITTANCE_LUT_WIDTH 256
#define TRANSMITTANCE_LUT_HEIGHT 64
#define MULTISCATTERING_LUT_WIDTH 32
#define MULTISCATTERING_LUT_HEIGHT 32
#define SKYVIEW_LUT_WIDTH 256
#define SKYVIEW_LUT_HEIGHT 128

#define EARTH_RADIUS_BOTTOM 6360.0f
#define EARTH_RADIUS_TOP 6460.0f

struct SunParams
{
    Math::Vec3 transmittanceSunZenith;
    float illuminanceZenith;
    float angularDiameterDegrees;
    float zenithDegrees;
    float azimuthDegrees;
};

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

    Math::Vec3 sunAlbedo;
    float sunAngularDiameterRadians;

    Math::Vec3 sunLuminanceOuterSpace;
    float sunZenithRadians;

    Math::Vec3 sunIlluminanceOuterSpace;
    float sunAzimuthRadians;

    float rayleighDensityCoeff;
    float mieDensityCoeff;
    float pad[2];
};

struct CameraParams
{
    Math::Mat4 projection;
    Math::Mat4 view;
};

static Fly::SimpleCameraFPS sCamera(90.0f, 1280.0f / 720.0f, 0.01f, 1000.0f,
                                    Math::Vec3(0.0f, 1250.0f, 0.0f));

static VkDescriptorPool sImGuiDescriptorPool;
static AtmosphereParams sAtmosphereParams;
static CameraParams sCameraParams;
static SunParams sSunParams;

static RHI::GraphicsPipeline sScreenQuadPipeline;
static RHI::GraphicsPipeline sTerrainScenePipeline;
static RHI::GraphicsPipeline sDrawCubemapPipeline;
static RHI::GraphicsPipeline sIrradianceMapPipeline;
static RHI::ComputePipeline sTransmittancePipeline;
static RHI::ComputePipeline sMultiscatteringPipeline;
static RHI::ComputePipeline sSkyviewPipeline;
static RHI::ComputePipeline sProjectSkyviewRadiancePipeline;
static RHI::ComputePipeline sAverageHorizonLuminancePipeline;

static RHI::Buffer sAtmosphereParamsBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sCameraBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sSkyviewRadianceProjectionBuffer;
static RHI::Buffer sAverageHorizonLuminanceBuffer;
static RHI::Texture sTransmittanceLUT;
static RHI::Texture sMultiscatteringLUT;
static RHI::Texture sSkyviewLUT;
static RHI::Texture sSkyviewIrradianceMap;

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

static bool CreateImGuiContext(RHI::Context& context, RHI::Device& device,
                               GLFWwindow* window)
{
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE},
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 0;
    for (VkDescriptorPoolSize& poolSize : poolSizes)
        poolInfo.maxSets += poolSize.descriptorCount;
    poolInfo.poolSizeCount = static_cast<u32>(STACK_ARRAY_COUNT(poolSizes));
    poolInfo.pPoolSizes = poolSizes;
    if (vkCreateDescriptorPool(device.logicalDevice, &poolInfo,
                               RHI::GetVulkanAllocationCallbacks(),
                               &sImGuiDescriptorPool) != VK_SUCCESS)
    {
        return false;
    }

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = context.instance;
    initInfo.PhysicalDevice = device.physicalDevice;
    initInfo.Device = device.logicalDevice;
    initInfo.Queue = device.graphicsComputeQueue;
    initInfo.DescriptorPool = sImGuiDescriptorPool;
    initInfo.MinImageCount = device.swapchainTextureCount;
    initInfo.ImageCount = device.swapchainTextureCount;
    initInfo.UseDynamicRendering = true;

    initInfo.PipelineRenderingCreateInfo = {};
    initInfo.PipelineRenderingCreateInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats =
        &device.surfaceFormat.format;

    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.Allocator = RHI::GetVulkanAllocationCallbacks();

    if (!ImGui_ImplVulkan_Init(&initInfo))
    {
        return false;
    }
    if (!ImGui_ImplVulkan_CreateFontsTexture())
    {
        return false;
    }
    return true;
}

static void DestroyImGuiContext(RHI::Device& device)
{
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(device.logicalDevice, sImGuiDescriptorPool,
                            RHI::GetVulkanAllocationCallbacks());
}

static void ProcessImGuiFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
        ImGui::Begin("Parameters");

        if (ImGui::TreeNode("Atmosphere"))
        {
            ImGui::SliderFloat3("Rayleigh scattering",
                                sAtmosphereParams.rayleighScattering.data, 0.0f,
                                1.0f, ".%6f");
            ImGui::SliderFloat("Rayleight density coefficient",
                               &sAtmosphereParams.rayleighDensityCoeff, 1.0f,
                               30.0f);

            ImGui::SliderFloat("Mie scattering",
                               &sAtmosphereParams.mieScattering, 0.0f, 1.0f);
            ImGui::SliderFloat("Mie absorption",
                               &sAtmosphereParams.mieAbsorption, 0.0f, 1.0f);
            ImGui::SliderFloat("Mie density coefficient",
                               &sAtmosphereParams.mieDensityCoeff, 1.0f, 30.0f);

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Sun"))
        {
            ImGui::ColorEdit3("Sun albedo", sAtmosphereParams.sunAlbedo.data);
            ImGui::SliderFloat("Illuminance zenith",
                               &sSunParams.illuminanceZenith, 5000.0f,
                               120000.0f);
            ImGui::SliderFloat("Zenith", &sSunParams.zenithDegrees, -180.0f,
                               180.0f, "%.3f");
            ImGui::SliderFloat("Azimuth", &sSunParams.azimuthDegrees, -180.0f,
                               180.0f, "%.3f");
            ImGui::SliderFloat("Angular diameter",
                               &sSunParams.angularDiameterDegrees, 0.5f, 20.0f,
                               "%.5f");
            ImGui::TreePop();
        }
        ImGui::End();
    }

    ImGui::Render();
}

static bool CreatePipelines(RHI::Device& device)
{
    RHI::ComputePipeline* computePipelines[] = {
        &sTransmittancePipeline, &sMultiscatteringPipeline, &sSkyviewPipeline,
        &sProjectSkyviewRadiancePipeline, &sAverageHorizonLuminancePipeline};

    const char* computeShaderPaths[] = {
        "transmittance_lut.comp.spv",
        "multiscattering_lut.comp.spv",
        "skyview_lut.comp.spv",
        "project_skyview_radiance.comp.spv",
        "average_horizon_luminance.comp.spv",
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
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    if (!Fly::LoadShaderFromSpv(device, "terrain_scene.frag.spv",
                                shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
    }
    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     sTerrainScenePipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

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
    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     sDrawCubemapPipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    fixedState.pipelineRendering.colorAttachments[0] =
        VK_FORMAT_R16G16B16A16_SFLOAT;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.pipelineRendering.viewMask = 0x3F;

    if (!Fly::LoadShaderFromSpv(device, "screen_quad.vert.spv",
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }

    if (!Fly::LoadShaderFromSpv(device, "irradiance_map.frag.spv",
                                shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
    }
    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     sIrradianceMapPipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);
    return true;
}

static void DestroyPipelines(RHI::Device& device)
{
    RHI::DestroyGraphicsPipeline(device, sTerrainScenePipeline);
    RHI::DestroyGraphicsPipeline(device, sScreenQuadPipeline);
    RHI::DestroyGraphicsPipeline(device, sDrawCubemapPipeline);
    RHI::DestroyGraphicsPipeline(device, sIrradianceMapPipeline);
    RHI::DestroyComputePipeline(device, sMultiscatteringPipeline);
    RHI::DestroyComputePipeline(device, sTransmittancePipeline);
    RHI::DestroyComputePipeline(device, sSkyviewPipeline);
    RHI::DestroyComputePipeline(device, sProjectSkyviewRadiancePipeline);
    RHI::DestroyComputePipeline(device, sAverageHorizonLuminancePipeline);
}

static bool CreateResources(RHI::Device& device)
{
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateBuffer(device, true,
                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               &sAtmosphereParams, sizeof(AtmosphereParams),
                               sAtmosphereParamsBuffers[i]))
        {
            return false;
        }

        if (!RHI::CreateBuffer(device, true,
                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               nullptr, sizeof(CameraParams),
                               sCameraBuffers[i]))
        {
            return false;
        }
    }

    if (!RHI::CreateBuffer(device, false, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                           nullptr, sizeof(i32) * 4 * 9,
                           sSkyviewRadianceProjectionBuffer))
    {
        return false;
    }

    if (!RHI::CreateBuffer(device, true, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                           nullptr, sizeof(i32) * 4,
                           sAverageHorizonLuminanceBuffer))
    {
        return false;
    }

    if (!RHI::CreateTexture2D(
            device, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            nullptr, TRANSMITTANCE_LUT_WIDTH, TRANSMITTANCE_LUT_HEIGHT,
            VK_FORMAT_R16G16B16A16_SFLOAT, RHI::Sampler::FilterMode::Bilinear,
            RHI::Sampler::WrapMode::Clamp, 1, sTransmittanceLUT))
    {
        return false;
    }

    if (!RHI::CreateTexture2D(
            device, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            nullptr, MULTISCATTERING_LUT_WIDTH, MULTISCATTERING_LUT_HEIGHT,
            VK_FORMAT_R16G16B16A16_SFLOAT, RHI::Sampler::FilterMode::Bilinear,
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

    if (!RHI::CreateCubemap(
            device,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            nullptr, 256, VK_FORMAT_R16G16B16A16_SFLOAT,
            RHI::Sampler::FilterMode::Bilinear, 1, sSkyviewIrradianceMap))
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
    RHI::DestroyBuffer(device, sSkyviewRadianceProjectionBuffer);
    RHI::DestroyBuffer(device, sAverageHorizonLuminanceBuffer);
    RHI::DestroyTexture(device, sTransmittanceLUT);
    RHI::DestroyTexture(device, sMultiscatteringLUT);
    RHI::DestroyTexture(device, sSkyviewLUT);
    RHI::DestroyTexture(device, sSkyviewIrradianceMap);
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

static void DrawTransmittanceLUT(RHI::Device& device)
{
    RHI::RecordBufferInput bufferInput;
    RHI::RecordTextureInput textureInput;

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

    RHI::ExecuteCompute(RenderFrameCommandBuffer(device),
                        RecordComputeTransmittance, &bufferInput,
                        &textureInput);
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

static void DrawMultiscatteringLUT(RHI::Device& device)
{
    RHI::RecordBufferInput bufferInput;
    RHI::RecordTextureInput textureInput;

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
    imageLayoutsAccesses[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageLayoutsAccesses[1].accessMask = VK_ACCESS_2_SHADER_WRITE_BIT;

    RHI::ExecuteCompute(RenderFrameCommandBuffer(device),
                        RecordComputeMultiscattering, &bufferInput,
                        &textureInput);
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

static void DrawSkyviewLUT(RHI::Device& device)
{
    RHI::RecordBufferInput bufferInput;
    RHI::RecordTextureInput textureInput;

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
    RHI::ImageLayoutAccess imageLayoutsAccesses[3];
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
    imageLayoutsAccesses[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageLayoutsAccesses[2].accessMask = VK_ACCESS_2_SHADER_WRITE_BIT;

    RHI::ExecuteCompute(RenderFrameCommandBuffer(device), RecordComputeSkyview,
                        &bufferInput, &textureInput);
}

static void RecordProjectSkyviewRadiance(
    RHI::CommandBuffer& cmd, const RHI::RecordBufferInput* bufferInput,
    const RHI::RecordTextureInput* textureInput, void* pUserData)
{
    RHI::BindComputePipeline(cmd, sProjectSkyviewRadiancePipeline);
    RHI::Texture& skyviewLUT = *(textureInput->textures[0]);
    RHI::Buffer& projectionBuffer = *(bufferInput->buffers[0]);

    u32 pushConstants[] = {skyviewLUT.bindlessStorageHandle,
                           projectionBuffer.bindlessHandle, SKYVIEW_LUT_WIDTH,
                           SKYVIEW_LUT_HEIGHT};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, SKYVIEW_LUT_WIDTH / 16, (SKYVIEW_LUT_HEIGHT / 2) / 16,
                  1);
}

static void ProjectSkyviewRadiance(RHI::Device& device)
{
    RHI::RecordBufferInput bufferInput;
    RHI::Buffer* pSkyviewRadianceProjectionBuffer =
        &sSkyviewRadianceProjectionBuffer;
    VkAccessFlagBits2 bufferAccess =
        VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    bufferInput.buffers = &pSkyviewRadianceProjectionBuffer;
    bufferInput.bufferAccesses = &bufferAccess;
    bufferInput.bufferCount = 1;

    RHI::RecordTextureInput textureInput;
    RHI::Texture* pSkyviewLUT = &sSkyviewLUT;
    RHI::ImageLayoutAccess imageLayoutAccess;
    imageLayoutAccess.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageLayoutAccess.accessMask = VK_ACCESS_2_SHADER_READ_BIT;
    textureInput.textures = &pSkyviewLUT;
    textureInput.imageLayoutsAccesses = &imageLayoutAccess;
    textureInput.textureCount = 1;

    RHI::ExecuteCompute(RenderFrameCommandBuffer(device),
                        RecordProjectSkyviewRadiance, &bufferInput,
                        &textureInput);
}

static void RecordAverageHorizonLuminance(
    RHI::CommandBuffer& cmd, const RHI::RecordBufferInput* bufferInput,
    const RHI::RecordTextureInput* textureInput, void* pUserData)
{
    RHI::BindComputePipeline(cmd, sAverageHorizonLuminancePipeline);

    RHI::Texture& skyviewLUT = *(textureInput->textures[0]);
    RHI::Buffer& averageLuminanceBuffer = *(bufferInput->buffers[0]);
    u32 pushConstants[] = {skyviewLUT.bindlessStorageHandle,
                           averageLuminanceBuffer.bindlessHandle,
                           skyviewLUT.width, skyviewLUT.height};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, SKYVIEW_LUT_WIDTH / 256, 1, 1);
}

static void AverageHorizonLuminance(RHI::Device& device)
{
    RHI::RecordBufferInput bufferInput;
    RHI::Buffer* pAverageHorizonLuminanceBuffer =
        &sAverageHorizonLuminanceBuffer;
    VkAccessFlagBits2 bufferAccess =
        VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    bufferInput.buffers = &pAverageHorizonLuminanceBuffer;
    bufferInput.bufferAccesses = &bufferAccess;
    bufferInput.bufferCount = 1;

    RHI::RecordTextureInput textureInput;
    RHI::Texture* pSkyviewLUT = &sSkyviewLUT;
    RHI::ImageLayoutAccess imageLayoutAccess;
    imageLayoutAccess.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageLayoutAccess.accessMask = VK_ACCESS_2_SHADER_READ_BIT;
    textureInput.textures = &pSkyviewLUT;
    textureInput.imageLayoutsAccesses = &imageLayoutAccess;
    textureInput.textureCount = 1;

    RHI::ExecuteCompute(RenderFrameCommandBuffer(device),
                        RecordAverageHorizonLuminance, &bufferInput,
                        &textureInput);
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
    RHI::Texture& texture = *(textureInput->textures[0]);
    u32 pushConstants[] = {texture.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Draw(cmd, 6, 1, 0, 0);
}

static void DrawScreenQuad(RHI::Device& device, RHI::Texture* pMapToShow)
{
    RHI::RecordTextureInput textureInput;

    RHI::ImageLayoutAccess imageLayoutAccess;
    imageLayoutAccess.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageLayoutAccess.accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    textureInput.textures = &pMapToShow;
    textureInput.imageLayoutsAccesses = &imageLayoutAccess;
    textureInput.textureCount = 1;

    VkRenderingAttachmentInfo colorAttachment =
        RHI::ColorAttachmentInfo(RenderFrameSwapchainTexture(device).imageView);
    VkRenderingInfo renderingInfo = RHI::RenderingInfo(
        {{0, 0}, {device.swapchainWidth, device.swapchainHeight}},
        &colorAttachment, 1);
    RHI::ExecuteGraphics(RenderFrameCommandBuffer(device), renderingInfo,
                         RecordDrawScreenQuad, nullptr, &textureInput);
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

static void DrawCubemap(RHI::Device& device, RHI::Texture* cubemap)
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
    textureInput.textures = &cubemap;
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

static void RecordConvoluteIrradiance(
    RHI::CommandBuffer& cmd, const RHI::RecordBufferInput* bufferInput,
    const RHI::RecordTextureInput* textureInput, void* pUserData)
{
    RHI::SetViewport(cmd, 0, 0, 256, 256, 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, 256, 256);
    RHI::BindGraphicsPipeline(cmd, sIrradianceMapPipeline);

    RHI::Buffer& cameraBuffer = *(bufferInput->buffers[0]);
    RHI::Buffer& radianceProjectionBuffer = *(bufferInput->buffers[1]);

    u32 pushConstants[] = {cameraBuffer.bindlessHandle,
                           radianceProjectionBuffer.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Draw(cmd, 6, 1, 0, 0);
}

static void ConvoluteIrradiance(RHI::Device& device)
{
    RHI::Buffer* buffers[2];
    VkAccessFlagBits2 bufferAccesses[2];
    buffers[0] = &sCameraBuffers[device.frameIndex];
    bufferAccesses[0] = VK_ACCESS_2_SHADER_READ_BIT;
    buffers[1] = &sSkyviewRadianceProjectionBuffer;
    bufferAccesses[1] = VK_ACCESS_2_SHADER_READ_BIT;

    RHI::RecordBufferInput bufferInput;
    bufferInput.buffers = buffers;
    bufferInput.bufferAccesses = bufferAccesses;
    bufferInput.bufferCount = 2;

    RHI::Texture* textures[1];
    RHI::ImageLayoutAccess imageLayoutsAccesses[1];

    textures[0] = &sSkyviewIrradianceMap;
    imageLayoutsAccesses[0].imageLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imageLayoutsAccesses[0].accessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

    RHI::RecordTextureInput textureInput;
    textureInput.textures = textures;
    textureInput.imageLayoutsAccesses = imageLayoutsAccesses;
    textureInput.textureCount = 1;

    VkRenderingAttachmentInfo colorAttachment =
        RHI::ColorAttachmentInfo(sSkyviewIrradianceMap.arrayImageView);
    VkRenderingInfo renderingInfo = RHI::RenderingInfo(
        {{0, 0}, {256, 256}}, &colorAttachment, 1, nullptr, nullptr, 6, 0x3F);

    RHI::ExecuteGraphics(RenderFrameCommandBuffer(device), renderingInfo,
                         RecordConvoluteIrradiance, &bufferInput,
                         &textureInput);
}

static void RecordDrawTerrainScene(RHI::CommandBuffer& cmd,
                                   const RHI::RecordBufferInput* bufferInput,
                                   const RHI::RecordTextureInput* textureInput,
                                   void* pUserData)
{
    RHI::SetViewport(cmd, 0.0f, 0.0f,
                     static_cast<f32>(cmd.device->swapchainWidth),
                     static_cast<f32>(cmd.device->swapchainHeight), 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, cmd.device->swapchainWidth,
                    cmd.device->swapchainHeight);

    RHI::BindGraphicsPipeline(cmd, sTerrainScenePipeline);

    RHI::Buffer& atmosphereParams = *(bufferInput->buffers[0]);
    RHI::Buffer& cameraParams = *(bufferInput->buffers[1]);
    RHI::Buffer& skyRadianceProjectionBuffer = *(bufferInput->buffers[2]);
    RHI::Buffer& averageHorizonLuminance = *(bufferInput->buffers[3]);
    RHI::Texture& transmittanceLUT = *(textureInput->textures[0]);
    RHI::Texture& skyviewLUT = *(textureInput->textures[1]);

    u32 pushConstants[] = {
        atmosphereParams.bindlessHandle,
        cameraParams.bindlessHandle,
        skyRadianceProjectionBuffer.bindlessHandle,
        averageHorizonLuminance.bindlessHandle,
        transmittanceLUT.bindlessHandle,
        skyviewLUT.bindlessHandle,
    };
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Draw(cmd, 6, 1, 0, 0);
}

static void DrawTerrainScene(RHI::Device& device)
{
    RHI::RecordBufferInput bufferInput;
    RHI::RecordTextureInput textureInput;

    RHI::Buffer* buffers[4];
    VkAccessFlagBits2 bufferAccesses[4];
    bufferInput.bufferCount = 4;
    bufferInput.buffers = buffers;
    bufferInput.bufferAccesses = bufferAccesses;

    buffers[0] = &sAtmosphereParamsBuffers[device.frameIndex];
    bufferAccesses[0] = VK_ACCESS_2_SHADER_READ_BIT;
    buffers[1] = &sCameraBuffers[device.frameIndex];
    bufferAccesses[1] = VK_ACCESS_2_SHADER_READ_BIT;
    buffers[2] = &sSkyviewRadianceProjectionBuffer;
    bufferAccesses[2] = VK_ACCESS_2_SHADER_READ_BIT;
    buffers[3] = &sAverageHorizonLuminanceBuffer;
    bufferAccesses[3] = VK_ACCESS_2_SHADER_READ_BIT;

    RHI::Texture* textures[2];
    RHI::ImageLayoutAccess imageLayoutsAccesses[2];
    textureInput.textureCount = 2;
    textureInput.textures = textures;
    textureInput.imageLayoutsAccesses = imageLayoutsAccesses;

    textures[0] = &sTransmittanceLUT;
    imageLayoutsAccesses[0].imageLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageLayoutsAccesses[0].accessMask = VK_ACCESS_2_SHADER_READ_BIT;

    textures[1] = &sSkyviewLUT;
    imageLayoutsAccesses[1].imageLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageLayoutsAccesses[1].accessMask = VK_ACCESS_2_SHADER_READ_BIT;

    VkRenderingAttachmentInfo colorAttachment =
        RHI::ColorAttachmentInfo(RenderFrameSwapchainTexture(device).imageView);
    VkRenderingInfo renderingInfo = RHI::RenderingInfo(
        {{0, 0}, {device.swapchainWidth, device.swapchainHeight}},
        &colorAttachment, 1);

    RHI::ExecuteGraphics(RenderFrameCommandBuffer(device), renderingInfo,
                         RecordDrawTerrainScene, &bufferInput, &textureInput);
}

static void RecordDrawGUI(RHI::CommandBuffer& cmd,
                          const RHI::RecordBufferInput* bufferInput,
                          const RHI::RecordTextureInput* textureInput,
                          void* pUserData)
{
    RHI::SetViewport(cmd, 0, 0, static_cast<f32>(cmd.device->swapchainWidth),
                     static_cast<f32>(cmd.device->swapchainHeight), 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, cmd.device->swapchainWidth,
                    cmd.device->swapchainHeight);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd.handle);
}

static void DrawGUI(RHI::Device& device)
{
    VkRenderingAttachmentInfo colorAttachment =
        RHI::ColorAttachmentInfo(RenderFrameSwapchainTexture(device).imageView,
                                 VK_ATTACHMENT_LOAD_OP_LOAD);
    VkRenderingInfo renderingInfo = RHI::RenderingInfo(
        {{0, 0}, {device.swapchainWidth, device.swapchainHeight}},
        &colorAttachment, 1);
    RHI::ExecuteGraphics(RenderFrameCommandBuffer(device), renderingInfo,
                         RecordDrawGUI);
}

static Math::Vec3 TransmittanceSunZenith(u32 sampleCount)
{
    Math::Vec3 transmittance = Math::Vec3(1.0f);

    float integrationLength =
        sAtmosphereParams.radiusTop - sAtmosphereParams.radiusBottom;
    float delta = integrationLength / sampleCount;
    for (u32 i = 0; i < sampleCount; i++)
    {
        float height = (i + 0.5f) * delta;
        Math::Vec3 extinctionRayleigh =
            sAtmosphereParams.rayleighScattering *
            Math::Exp(-height / sAtmosphereParams.rayleighDensityCoeff);
        Math::Vec3 extinctionMie =
            Math::Vec3((sAtmosphereParams.mieScattering +
                        sAtmosphereParams.mieAbsorption) *
                       Math::Exp(-height / sAtmosphereParams.mieDensityCoeff));
        Math::Vec3 extinctionOzone =
            sAtmosphereParams.ozoneAbsorption *
            Math::Max(0.0f, 1.0f - Math::Abs((height - 25.0f) / 15.0f));

        transmittance *= Math::Exp(
            -(extinctionRayleigh + extinctionMie + extinctionOzone) * delta);
    }

    return transmittance;
}

static void CopyAtmosphereParamsToDevice(RHI::Device& device)
{
    RHI::Buffer& atmosphereParams = sAtmosphereParamsBuffers[device.frameIndex];

    sAtmosphereParams.sunZenithRadians =
        Math::Radians(sSunParams.zenithDegrees);
    sAtmosphereParams.sunAzimuthRadians =
        Math::Radians(sSunParams.azimuthDegrees);
    sAtmosphereParams.sunAngularDiameterRadians =
        Math::Radians(sSunParams.angularDiameterDegrees);

    float solidAngle =
        FLY_MATH_TWO_PI *
        (1.0f -
         Math::Cos(Math::Radians(sSunParams.angularDiameterDegrees) * 0.5f));

    sAtmosphereParams.sunIlluminanceOuterSpace =
        Math::Vec3(sSunParams.illuminanceZenith) /
        sSunParams.transmittanceSunZenith;
    sAtmosphereParams.sunLuminanceOuterSpace =
        sAtmosphereParams.sunIlluminanceOuterSpace / solidAngle;

    RHI::CopyDataToBuffer(device, &sAtmosphereParams, sizeof(AtmosphereParams),
                          0, atmosphereParams);
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
        glfwCreateWindow(1920, 1080, "Window", nullptr, nullptr);

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
    settings.vulkan11Features.multiview = true;

    RHI::Context context;
    if (!RHI::CreateContext(settings, context))
    {
        FLY_ERROR("Failed to create context");
        return -1;
    }

    RHI::Device& device = context.devices[0];

    if (!CreateImGuiContext(context, device, window))
    {
        FLY_ERROR("Failed to create imgui context");
        return -1;
    }

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
    sAtmosphereParams.rayleighDensityCoeff = 8.0f;
    sAtmosphereParams.mieDensityCoeff = 1.2f;
    sAtmosphereParams.sunAlbedo = Math::Vec3(1.0f, 1.0f, 1.0f);

    sSunParams.zenithDegrees = 85.0f;
    sSunParams.azimuthDegrees = 0.0f;
    sSunParams.illuminanceZenith = 100000.0f;
    sSunParams.angularDiameterDegrees = 2.545f;
    sSunParams.transmittanceSunZenith = TransmittanceSunZenith(50);

    if (!CreateResources(device))
    {
        FLY_ERROR("Failed to create resources");
        return -1;
    }

    sCamera.speed = 1000.0f;

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

        ImGuiIO& io = ImGui::GetIO();
        bool wantMouse = io.WantCaptureMouse;
        bool wantKeyboard = io.WantCaptureKeyboard;
        if (!wantMouse && !wantKeyboard)
        {
            sCamera.Update(window, deltaTime);
        }
        // sCamera.SetPosition(sCamera.GetPosition() +
        //                     Math::Vec3(0.0f, 0.0f, 2000.0f) * deltaTime);

        ProcessImGuiFrame();

        {
            CameraParams cameraParams = {sCamera.GetProjection(),
                                         sCamera.GetView()};
            RHI::Buffer& cameraBuffer = sCameraBuffers[device.frameIndex];
            RHI::CopyDataToBuffer(device, &cameraParams, sizeof(CameraParams),
                                  0, cameraBuffer);
            CopyAtmosphereParamsToDevice(device);
        }

        RHI::BeginRenderFrame(device);
        DrawTransmittanceLUT(device);
        DrawMultiscatteringLUT(device);
        DrawSkyviewLUT(device);
        ProjectSkyviewRadiance(device);
        ConvoluteIrradiance(device);
        AverageHorizonLuminance(device);
        // DrawCubemap(device, &sSkyviewIrradianceMap);
        // DrawScreenQuad(device, sMultiscatteringLUT);
        DrawTerrainScene(device);
        DrawGUI(device);

        RHI::EndRenderFrame(device);

        i32* values = static_cast<i32*>(
            RHI::BufferMappedPtr(sAverageHorizonLuminanceBuffer));
        FLY_LOG("%i %i %i", values[0], values[1], values[2]);
    }

    RHI::WaitDeviceIdle(device);

    DestroyResources(device);
    DestroyPipelines(device);
    DestroyImGuiContext(device);
    RHI::DestroyContext(context);

    FLY_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
