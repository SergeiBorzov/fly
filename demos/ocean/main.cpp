#include "core/clock.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "rhi/allocation_callbacks.h"
#include "rhi/buffer.h"
#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"
#include "rhi/texture.h"

#include "demos/common/simple_camera_fps.h"
#include "utils/utils.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>

using namespace Fly;

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define OCEAN_CASCADE_COUNT 4

struct CameraData
{
    Math::Mat4 projection;
    Math::Mat4 view;
    Math::Vec4 screenSize;
};

struct Vertex
{
    Math::Vec2 position;
    Math::Vec2 uv;
};

struct OceanFrequencyVertex
{
    Math::Vec2 value;
    Math::Vec2 displacement;
    Math::Vec2 dx;
    Math::Vec2 dy;
    Math::Vec2 dxDisplacement;
    Math::Vec2 dyDisplacement;
};

struct OceanPushConstants
{
    u32 uniformBufferIndex;
    u32 shadeParamsBufferIndex;
    u32 vertexBufferIndex;
    u32 heightMaps[OCEAN_CASCADE_COUNT];
    u32 diffDisplacements[OCEAN_CASCADE_COUNT];
    u32 skyboxTextureIndex;
    f32 waveChopiness;
};

struct JonswapData
{
    Math::Vec4 fetchSpeedDirSpread;
    Math::Vec4 timeScale;
    Math::Vec4 domainMinMax;
};

struct ShadeParams
{
    Math::Vec4 lightColorReflectivity;
    Math::Vec4 waterScatterColor;
    Math::Vec4 coefficients;
    Math::Vec4 bubbleColorDensity;
};

struct Cascade
{
    RHI::Buffer frequencyBuffers[2];
    RHI::Buffer uniformBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
    RHI::Texture heightMap;
    RHI::Texture diffDisplacementMap;

    u32 resolution;
    f32 domain;
    f32 kMin;
    f32 kMax;
};

static float sFetch = 334917.0f;
static float sWindSpeed = 6.0f;
static float sWindDirection = FLY_MATH_PI;
static float sSpread = 7.32f;
static float sAmplitudeScale = 1.0f;
static float sTime = 0.0f;
static float sWaveChopiness = 1.2f;

static Math::Vec3 sLightColor = Math::Vec3(0.961f, 0.945f, 0.89f);
static Math::Vec3 sWaterScatterColor = Math::Vec3(0.18f, 0.28f, 0.32f);
static Math::Vec3 sBubbleColor = Math::Vec3(1.0f);
static float sSS1 = 5.92f;
static float sSS2 = 0.17f;
static float sA1 = 0.01f;
static float sA2 = 0.1f;
static float sReflectivity = 1.0f;
static float sBubbleDensity = 0.14f;

static VkDescriptorPool sImGuiDescriptorPool;
static RHI::ComputePipeline sSpectrumPipeline;
static RHI::ComputePipeline sIFFTPipeline;
static RHI::ComputePipeline sTransposePipeline;
static RHI::ComputePipeline sCopyPipeline;
static RHI::GraphicsPipeline sSkyPipeline;
static RHI::GraphicsPipeline sOceanPipeline;
static RHI::Texture sSkyboxTexture;
static RHI::Texture sDepthTexture;

static RHI::Buffer sOceanPlaneVertex;
static RHI::Buffer sOceanPlaneIndex;
static u32 sOceanPlaneIndexCount;
static RHI::Buffer sCameraBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sShadeParams[FLY_FRAME_IN_FLIGHT_COUNT];
static Cascade sCascades[OCEAN_CASCADE_COUNT];
static u32 sCascadeResolution = 256;

static Fly::SimpleCameraFPS
    sCamera(90.0f, static_cast<f32>(WINDOW_WIDTH) / WINDOW_HEIGHT, 0.01f,
            1000.0f, Fly::Math::Vec3(0.0f, 10.0f, -10.0f));

static bool IsPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice,
                                     const RHI::PhysicalDeviceInfo& info)
{

    VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT
        unusedAttachmentsFeature{};
    unusedAttachmentsFeature.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT;
    unusedAttachmentsFeature.pNext = nullptr;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &unusedAttachmentsFeature;

    vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

    if (!unusedAttachmentsFeature.dynamicRenderingUnusedAttachments)
    {
        return false;
    }

    return info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

static void ErrorCallbackGLFW(i32 error, const char* description)
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
    initInfo.PipelineRenderingCreateInfo.depthAttachmentFormat =
        VK_FORMAT_D32_SFLOAT_S8_UINT;
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

        if (ImGui::TreeNode("Ocean spectrum"))
        {

            ImGui::SliderFloat("Fetch", &sFetch, 1.0f, 1000000.0f, "%.1f");
            ImGui::SliderFloat("Wind Speed", &sWindSpeed, 0.1f, 30.0f, "%.6f");
            ImGui::SliderFloat("Wind Direction", &sWindDirection, -FLY_MATH_PI,
                               FLY_MATH_PI, "%.2f rad");
            ImGui::SliderFloat("Spread", &sSpread, 1.0f, 30.0f, "%.2f");
            ImGui::SliderFloat("Wave Chopiness", &sWaveChopiness, -10.0f, 10.0f,
                               "%.2f");
            ImGui::SliderFloat("Amplitude scale", &sAmplitudeScale, 0.0f, 20.0f,
                               "%.2f");
            // ImGui::SliderFloat("Foam Decay", &sOceanRenderer.foamDecay, 0.0f,
            //                    10.0f, "%.2f");
            // ImGui::SliderFloat("Foam Gain", &sOceanRenderer.foamGain, 0.0f,
            //                    10.0f, "%.2f");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Ocean shade"))
        {
            ImGui::ColorEdit3("Water scatter", sWaterScatterColor.data);
            ImGui::ColorEdit3("Light color", sLightColor.data);
            ImGui::SliderFloat("ss1", &sSS1, 0.0f, 100.0f, "%.2f");
            ImGui::SliderFloat("ss2", &sSS2, 0.0f, 100.0f, "%.2f");
            ImGui::SliderFloat("a1", &sA1, 0.0f, 0.05f, "%.2f");
            ImGui::SliderFloat("a2", &sA2, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("reflectivity", &sReflectivity, 0.0f, 1.0f,
                               "%.2f");
            ImGui::SliderFloat("bubble density", &sBubbleDensity, 0.0f, 1.0f,
                               "%.2f");
            ImGui::TreePop();
        }

        ImGui::End();
    }

    ImGui::Render();
}

static bool CreatePipelines(RHI::Device& device)
{
    RHI::ComputePipeline* computePipelines[] = {
        &sSpectrumPipeline,
        &sIFFTPipeline,
        &sTransposePipeline,
        &sCopyPipeline,
    };

    String8 computeShaderPaths[] = {FLY_STRING8_LITERAL("jonswap.comp.spv"),
                                    FLY_STRING8_LITERAL("ifft.comp.spv"),
                                    FLY_STRING8_LITERAL("transpose.comp.spv"),
                                    FLY_STRING8_LITERAL("copy.comp.spv")};

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
    fixedState.depthStencilState.depthTestEnable = false;
    fixedState.pipelineRendering.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    fixedState.rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    fixedState.rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    fixedState.inputAssemblyState.topology =
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    RHI::ShaderProgram shaderProgram{};
    if (!LoadShaderFromSpv(device, FLY_STRING8_LITERAL("sky.vert.spv"),
                           shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }
    if (!LoadShaderFromSpv(device, FLY_STRING8_LITERAL("sky.frag.spv"),
                           shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
    }

    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     sSkyPipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    if (!LoadShaderFromSpv(device, FLY_STRING8_LITERAL("ocean.vert.spv"),
                           shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }
    if (!LoadShaderFromSpv(device, FLY_STRING8_LITERAL("ocean.frag.spv"),
                           shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
    }
    fixedState.pipelineRendering.depthAttachmentFormat =
        VK_FORMAT_D32_SFLOAT_S8_UINT;
    fixedState.depthStencilState.depthTestEnable = true;
    // fixedState.rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     sOceanPipeline))
    {
        return false;
    }

    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    return true;
}

static void DestroyPipelines(RHI::Device& device)
{
    RHI::DestroyComputePipeline(device, sCopyPipeline);
    RHI::DestroyComputePipeline(device, sTransposePipeline);
    RHI::DestroyComputePipeline(device, sIFFTPipeline);
    RHI::DestroyComputePipeline(device, sSpectrumPipeline);
    RHI::DestroyGraphicsPipeline(device, sOceanPipeline);
    RHI::DestroyGraphicsPipeline(device, sSkyPipeline);
}

static bool CreateResources(RHI::Device& device)
{
    if (!RHI::CreateTexture2D(
            device, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, nullptr,
            device.swapchainWidth, device.swapchainHeight,
            VK_FORMAT_D32_SFLOAT_S8_UINT, RHI::Sampler::FilterMode::Nearest,
            RHI::Sampler::WrapMode::Repeat, 1, sDepthTexture))
    {
        return false;
    }

    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateBuffer(device, true,
                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               nullptr, sizeof(CameraData), sCameraBuffers[i]))
        {
            return false;
        }
        if (!RHI::CreateBuffer(device, true,
                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               nullptr, sizeof(ShadeParams), sShadeParams[i]))
        {
            return false;
        }
    }

    if (!LoadCompressedCubemap(
            device,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            FLY_STRING8_LITERAL("DaySkyHDRI051B_4K-TONEMAPPED.fbc1"), VK_FORMAT_BC1_RGB_SRGB_BLOCK,
            RHI::Sampler::FilterMode::Trilinear, sSkyboxTexture))
    {
        return false;
    }

    // Generate vertices
    {
        Arena& arena = GetScratchArena();
        ArenaMarker marker = ArenaGetMarker(arena);

        f32 tileSize = 1000.0f;
        f32 quadSize = 1.0f;
        i32 quadPerSide = static_cast<i32>(tileSize / quadSize);
        i32 offset = quadPerSide / 2;
        u32 vertexPerSide = quadPerSide + 1;

        FLY_LOG("Vertices size is %u", vertexPerSide * vertexPerSide);
        Vertex* vertices =
            FLY_PUSH_ARENA(arena, Vertex, vertexPerSide * vertexPerSide);
        u32 count = 0;
        for (i32 z = -offset; z <= offset; z++)
        {
            for (i32 x = -offset; x <= offset; x++)
            {
                Vertex& vertex = vertices[count++];
                vertex.position = Math::Vec2(static_cast<f32>(x) * quadSize,
                                             static_cast<f32>(z) * quadSize);
                // FLY_LOG("%f %f", vertex.position.x, vertex.position.y);
                vertex.uv =
                    Math::Vec2((x + offset) / static_cast<f32>(quadPerSide),
                               (z + offset) / static_cast<f32>(quadPerSide));
            }
        }

        if (!CreateBuffer(device, false,
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          vertices,
                          vertexPerSide * vertexPerSide * sizeof(Vertex),
                          sOceanPlaneVertex))
        {
            ArenaPopToMarker(arena, marker);
            return false;
        }
        ArenaPopToMarker(arena, marker);

        sOceanPlaneIndexCount = static_cast<u32>(6 * quadPerSide * quadPerSide);
        u32* indices = FLY_PUSH_ARENA(arena, u32, sOceanPlaneIndexCount);
        for (i32 z = 0; z < quadPerSide; z++)
        {
            for (i32 x = 0; x < quadPerSide; x++)
            {
                u32 indexBase = 6 * (quadPerSide * z + x);
                indices[indexBase + 0] = vertexPerSide * z + x;
                indices[indexBase + 1] = vertexPerSide * z + x + 1;
                indices[indexBase + 2] = vertexPerSide * (z + 1) + x;
                indices[indexBase + 3] = vertexPerSide * (z + 1) + x;
                indices[indexBase + 4] = vertexPerSide * z + x + 1;
                indices[indexBase + 5] = vertexPerSide * (z + 1) + x + 1;
            }
        }
        if (!CreateBuffer(device, false,
                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          indices, sizeof(u32) * sOceanPlaneIndexCount,
                          sOceanPlaneIndex))
        {
            ArenaPopToMarker(arena, marker);
            return false;
        }
        ArenaPopToMarker(arena, marker);
    }

    return true;
}

static bool CreateCascade(RHI::Device& device, Cascade& cascade, float domain,
                          float kMin, float kMax)
{
    cascade.domain = domain;
    cascade.kMin = kMin;
    cascade.kMax = kMax;

    for (u32 i = 0; i < 2; i++)
    {
        if (!RHI::CreateBuffer(device, false,
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               nullptr,
                               sizeof(OceanFrequencyVertex) *
                                   sCascadeResolution * sCascadeResolution,
                               cascade.frequencyBuffers[i]))
        {
            return false;
        }
    }

    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateBuffer(device, true,
                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               nullptr, sizeof(JonswapData),
                               cascade.uniformBuffers[i]))
        {
            return false;
        }
    }

    if (!RHI::CreateTexture2D(
            device,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                VK_IMAGE_USAGE_STORAGE_BIT,
            nullptr, sCascadeResolution, sCascadeResolution,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            RHI::Sampler::FilterMode::Anisotropy4x,
            RHI::Sampler::WrapMode::Repeat, 0, cascade.diffDisplacementMap))
    {
        return false;
    }

    if (!RHI::CreateTexture2D(
            device,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                VK_IMAGE_USAGE_STORAGE_BIT,
            nullptr, sCascadeResolution, sCascadeResolution,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            RHI::Sampler::FilterMode::Anisotropy4x,
            RHI::Sampler::WrapMode::Repeat, 0, cascade.heightMap))
    {
        return false;
    }

    return true;
}

void UpdateCascades(RHI::Device& device)
{
    JonswapData jonswapData[OCEAN_CASCADE_COUNT];
    for (u32 i = 0; i < OCEAN_CASCADE_COUNT; i++)
    {
        jonswapData[i].fetchSpeedDirSpread =
            Math::Vec4(sFetch, sWindSpeed, sWindDirection, sSpread);
        jonswapData[i].timeScale =
            Math::Vec4(sTime, sAmplitudeScale, 0.0f, 0.0f);
        jonswapData[i].domainMinMax = Math::Vec4(
            sCascades[i].domain, sCascades[i].kMin, sCascades[i].kMax, 0.0f);

        RHI::CopyDataToBuffer(device, &(jonswapData[i]), sizeof(JonswapData), 0,
                              sCascades[i].uniformBuffers[device.frameIndex]);
    }
}

void UpdateShadeParams(RHI::Device& device)
{
    ShadeParams shadeParams;
    shadeParams.lightColorReflectivity = Math::Vec4(sLightColor, sReflectivity);
    shadeParams.waterScatterColor = Math::Vec4(sWaterScatterColor, 0.0f);
    shadeParams.coefficients = Math::Vec4(sSS1, sSS2, sA1, sA2);
    shadeParams.bubbleColorDensity = Math::Vec4(sBubbleColor, sBubbleDensity);
    RHI::CopyDataToBuffer(device, &shadeParams, sizeof(ShadeParams), 0,
                          sShadeParams[device.frameIndex]);
}

static void DestroyCascade(RHI::Device& device, Cascade& cascade)
{
    RHI::DestroyTexture(device, cascade.heightMap);
    RHI::DestroyTexture(device, cascade.diffDisplacementMap);
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        RHI::DestroyBuffer(device, cascade.uniformBuffers[i]);
    }
    for (u32 i = 0; i < 2; i++)
    {
        RHI::DestroyBuffer(device, cascade.frequencyBuffers[i]);
    }
}

static void DestroyResources(RHI::Device& device)
{
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        RHI::DestroyBuffer(device, sCameraBuffers[i]);
        RHI::DestroyBuffer(device, sShadeParams[i]);
    }
    RHI::DestroyBuffer(device, sOceanPlaneVertex);
    RHI::DestroyBuffer(device, sOceanPlaneIndex);
    RHI::DestroyTexture(device, sSkyboxTexture);
    RHI::DestroyTexture(device, sDepthTexture);
}

static void RecordCascadeSpectrum(RHI::CommandBuffer& cmd,
                                  const RHI::RecordBufferInput* bufferInput,
                                  const RHI::RecordTextureInput* textureInput,
                                  void* pUserData)
{
    RHI::BindComputePipeline(cmd, sSpectrumPipeline);

    RHI::Buffer& uniformBuffer = *(bufferInput->buffers[0]);
    RHI::Buffer& frequencyBuffer = *(bufferInput->buffers[1]);

    u32 pushConstants[] = {uniformBuffer.bindlessHandle,
                           frequencyBuffer.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, sCascadeResolution, 1, 1);
}

static void RecordIFFT(RHI::CommandBuffer& cmd,
                       const RHI::RecordBufferInput* bufferInput,
                       const RHI::RecordTextureInput* textureInput,
                       void* pUserData)
{
    RHI::BindComputePipeline(cmd, sIFFTPipeline);

    RHI::Buffer& frequencyBuffer = *(bufferInput->buffers[0]);

    u32 pushConstants[] = {frequencyBuffer.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, sCascadeResolution, 1, 1);
}

static void RecordTranspose(RHI::CommandBuffer& cmd,
                            const RHI::RecordBufferInput* bufferInput,
                            const RHI::RecordTextureInput* textureInput,
                            void* pUserData)
{
    RHI::BindComputePipeline(cmd, sTransposePipeline);

    RHI::Buffer& inFrequencyBuffer = *(bufferInput->buffers[0]);
    RHI::Buffer& outFrequencyBuffer = *(bufferInput->buffers[1]);
    u32 pushConstants[] = {inFrequencyBuffer.bindlessHandle,
                           outFrequencyBuffer.bindlessHandle,
                           sCascadeResolution};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));

    RHI::Dispatch(cmd, sCascadeResolution / 16, sCascadeResolution / 16, 1);
}

static void RecordCopy(RHI::CommandBuffer& cmd,
                       const RHI::RecordBufferInput* bufferInput,
                       const RHI::RecordTextureInput* textureInput,
                       void* pUserData)
{
    RHI::BindComputePipeline(cmd, sCopyPipeline);

    RHI::Buffer& frequencyBuffer = *(bufferInput->buffers[0]);
    RHI::Texture& diffDisplacementMap = *(textureInput->textures[0]);
    RHI::Texture& heightMap = *(textureInput->textures[1]);

    u32 pushConstants[] = {frequencyBuffer.bindlessHandle,
                           diffDisplacementMap.bindlessStorageHandle,
                           heightMap.bindlessStorageHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, sCascadeResolution, 1, 1);
}

static void RecordDrawSky(RHI::CommandBuffer& cmd,
                          const RHI::RecordBufferInput* bufferInput,
                          const RHI::RecordTextureInput* textureInput,
                          void* pUserData)
{
    RHI::SetViewport(cmd, 0, 0, static_cast<f32>(cmd.device->swapchainWidth),
                     static_cast<f32>(cmd.device->swapchainHeight), 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, cmd.device->swapchainWidth,
                    cmd.device->swapchainHeight);

    RHI::Buffer& uniformBuffer = *(bufferInput->buffers[0]);
    RHI::Texture& skybox = *(textureInput->textures[0]);

    RHI::BindGraphicsPipeline(cmd, sSkyPipeline);
    u32 pushConstants[] = {uniformBuffer.bindlessHandle, skybox.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Draw(cmd, 6, 1, 0, 0);
}

static void RecordDrawOcean(RHI::CommandBuffer& cmd,
                            const RHI::RecordBufferInput* bufferInput,
                            const RHI::RecordTextureInput* textureInput,
                            void* pUserData)
{
    RHI::SetViewport(cmd, 0, 0, static_cast<f32>(cmd.device->swapchainWidth),
                     static_cast<f32>(cmd.device->swapchainHeight), 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, cmd.device->swapchainWidth,
                    cmd.device->swapchainHeight);

    RHI::Buffer& uniformBuffer = *(bufferInput->buffers[0]);
    RHI::Buffer& shadeParams = *(bufferInput->buffers[1]);

    RHI::BindGraphicsPipeline(cmd, sOceanPipeline);

    OceanPushConstants pushConstants;
    pushConstants.uniformBufferIndex = uniformBuffer.bindlessHandle;
    pushConstants.shadeParamsBufferIndex = shadeParams.bindlessHandle;
    pushConstants.vertexBufferIndex = sOceanPlaneVertex.bindlessHandle;
    for (u32 i = 0; i < OCEAN_CASCADE_COUNT; i++)
    {
        pushConstants.diffDisplacements[i] =
            textureInput->textures[i]->bindlessHandle;
        pushConstants.heightMaps[i] =
            textureInput->textures[OCEAN_CASCADE_COUNT + i]->bindlessHandle;
    }
    pushConstants.skyboxTextureIndex = sSkyboxTexture.bindlessHandle;
    pushConstants.waveChopiness = sWaveChopiness;

    RHI::PushConstants(cmd, &pushConstants, sizeof(pushConstants));

    RHI::BindIndexBuffer(cmd, sOceanPlaneIndex, VK_INDEX_TYPE_UINT32);
    RHI::DrawIndexed(cmd, sOceanPlaneIndexCount, 1, 0, 0, 0);
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

static void Draw(RHI::Device& device)
{
    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    RHI::RecordBufferInput bufferInput;
    RHI::RecordTextureInput textureInput;
    RHI::Buffer** buffers = FLY_PUSH_ARENA(arena, RHI::Buffer*, 2);
    VkAccessFlagBits2* bufferAccesses =
        FLY_PUSH_ARENA(arena, VkAccessFlagBits2, 2);
    RHI::Texture** textures =
        FLY_PUSH_ARENA(arena, RHI::Texture*, OCEAN_CASCADE_COUNT * 2);
    RHI::ImageLayoutAccess* imageLayoutsAccesses =
        FLY_PUSH_ARENA(arena, RHI::ImageLayoutAccess, OCEAN_CASCADE_COUNT * 2);
    bufferInput.buffers = buffers;
    bufferInput.bufferAccesses = bufferAccesses;
    textureInput.textures = textures;
    textureInput.imageLayoutsAccesses = imageLayoutsAccesses;

    {
        for (u32 i = 0; i < OCEAN_CASCADE_COUNT; i++)
        {
            bufferInput.bufferCount = 2;
            buffers[0] = &sCascades[i].uniformBuffers[device.frameIndex];
            bufferAccesses[0] = VK_ACCESS_2_SHADER_READ_BIT;
            buffers[1] = &sCascades[i].frequencyBuffers[0];
            bufferAccesses[1] = VK_ACCESS_2_SHADER_WRITE_BIT;

            RHI::ExecuteCompute(RenderFrameCommandBuffer(device),
                                RecordCascadeSpectrum, &bufferInput);
        }
    }

    {
        bufferInput.bufferCount = 1;
        for (u32 i = 0; i < OCEAN_CASCADE_COUNT; i++)
        {
            buffers[0] = &sCascades[i].frequencyBuffers[0];
            bufferAccesses[0] =
                VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;

            RHI::ExecuteCompute(RenderFrameCommandBuffer(device), RecordIFFT,
                                &bufferInput);
        }
    }

    {
        bufferInput.bufferCount = 2;
        for (u32 i = 0; i < OCEAN_CASCADE_COUNT; i++)
        {
            buffers[0] = &sCascades[i].frequencyBuffers[0];
            bufferAccesses[0] = VK_ACCESS_2_SHADER_READ_BIT;
            buffers[1] = &sCascades[i].frequencyBuffers[1];
            bufferAccesses[1] = VK_ACCESS_2_SHADER_WRITE_BIT;

            RHI::ExecuteCompute(RenderFrameCommandBuffer(device),
                                RecordTranspose, &bufferInput);
        }
    }

    {
        bufferInput.bufferCount = 1;
        for (u32 i = 0; i < OCEAN_CASCADE_COUNT; i++)
        {
            buffers[0] = &sCascades[i].frequencyBuffers[1];
            bufferAccesses[0] =
                VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;

            RHI::ExecuteCompute(RenderFrameCommandBuffer(device), RecordIFFT,
                                &bufferInput);
        }
    }

    {
        bufferInput.bufferCount = 1;
        textureInput.textureCount = 2;
        for (u32 i = 0; i < OCEAN_CASCADE_COUNT; i++)
        {
            buffers[0] = &sCascades[i].frequencyBuffers[1];
            bufferAccesses[0] = VK_ACCESS_2_SHADER_READ_BIT;

            textures[0] = &sCascades[i].diffDisplacementMap;
            imageLayoutsAccesses[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageLayoutsAccesses[0].accessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
            textures[1] = &sCascades[i].heightMap;
            imageLayoutsAccesses[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageLayoutsAccesses[1].accessMask = VK_ACCESS_2_SHADER_WRITE_BIT;

            RHI::ExecuteCompute(RenderFrameCommandBuffer(device), RecordCopy,
                                &bufferInput, &textureInput);
        }
    }

    {
        bufferInput.bufferCount = 1;
        buffers[0] = &sCameraBuffers[device.frameIndex];
        bufferAccesses[0] = VK_ACCESS_2_SHADER_READ_BIT;

        textureInput.textureCount = 1;
        textures[0] = &sSkyboxTexture;
        imageLayoutsAccesses[0].imageLayout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageLayoutsAccesses[0].accessMask = VK_ACCESS_2_SHADER_READ_BIT;

        VkRenderingAttachmentInfo colorAttachment = RHI::ColorAttachmentInfo(
            RenderFrameSwapchainTexture(device).imageView);
        VkRenderingInfo renderingInfo = RHI::RenderingInfo(
            {{0, 0}, {device.swapchainWidth, device.swapchainHeight}},
            &colorAttachment, 1);
        RHI::ExecuteGraphics(RenderFrameCommandBuffer(device), renderingInfo,
                             RecordDrawSky, &bufferInput, &textureInput);
    }

    {
        bufferInput.bufferCount = 2;
        buffers[0] = &sCameraBuffers[device.frameIndex];
        bufferAccesses[0] = VK_ACCESS_2_SHADER_READ_BIT;
        buffers[1] = &sShadeParams[device.frameIndex];
        bufferAccesses[1] = VK_ACCESS_2_SHADER_READ_BIT;

        textureInput.textureCount = OCEAN_CASCADE_COUNT * 2;
        for (u32 i = 0; i < OCEAN_CASCADE_COUNT; i++)
        {
            textures[i] = &sCascades[i].diffDisplacementMap;
            imageLayoutsAccesses[i].imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageLayoutsAccesses[i].accessMask = VK_ACCESS_2_SHADER_READ_BIT;
        }
        for (u32 i = 0; i < OCEAN_CASCADE_COUNT; i++)
        {
            textures[OCEAN_CASCADE_COUNT + i] = &sCascades[i].heightMap;
            imageLayoutsAccesses[OCEAN_CASCADE_COUNT + i].imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageLayoutsAccesses[OCEAN_CASCADE_COUNT + i].accessMask =
                VK_ACCESS_2_SHADER_READ_BIT;
        }

        VkRenderingAttachmentInfo colorAttachment = RHI::ColorAttachmentInfo(
            RenderFrameSwapchainTexture(device).imageView,
            VK_ATTACHMENT_LOAD_OP_LOAD);
        VkRenderingAttachmentInfo depthAttachment =
            RHI::DepthAttachmentInfo(sDepthTexture.imageView);
        VkRenderingInfo renderingInfo = RHI::RenderingInfo(
            {{0, 0}, {device.swapchainWidth, device.swapchainHeight}},
            &colorAttachment, 1, &depthAttachment);
        RHI::ExecuteGraphics(RenderFrameCommandBuffer(device), renderingInfo,
                             RecordDrawOcean, &bufferInput, &textureInput);
    }

    {
        VkRenderingAttachmentInfo colorAttachment = RHI::ColorAttachmentInfo(
            RenderFrameSwapchainTexture(device).imageView,
            VK_ATTACHMENT_LOAD_OP_LOAD);
        VkRenderingInfo renderingInfo = RHI::RenderingInfo(
            {{0, 0}, {device.swapchainWidth, device.swapchainHeight}},
            &colorAttachment, 1);
        RHI::ExecuteGraphics(RenderFrameCommandBuffer(device), renderingInfo,
                             RecordDrawGUI);
    }

    ArenaPopToMarker(arena, marker);
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
        // ToggleWireframeOceanRenderer(sOceanRenderer);
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

int main(int argc, char* argv[])
{
    InitThreadContext();
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
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Ocean",
                                          nullptr, nullptr);
    if (!window)
    {
        FLY_ERROR("Failed to create glfw window");
        glfwTerminate();
        return -1;
    }
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetKeyCallback(window, OnKeyboardPressed);

    // Create graphics context
    const char* requiredDeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    RHI::ContextSettings settings{};
    settings.vulkan11Features.multiview = true;
    settings.features2.features.samplerAnisotropy = true;
    settings.features2.features.fillModeNonSolid = true;
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
    glfwSetWindowUserPointer(window, &device);

    if (!CreateImGuiContext(context, device, window))
    {
        FLY_ERROR("Failed to create imgui context");
        return -1;
    }

    if (!CreatePipelines(device))
    {
        return -1;
    }

    if (!CreateResources(device))
    {
        return -1;
    }

    f32 domain = 1000.0f;
    f32 ratio = 1 + Math::Sqrt(11);
    f32 kMins[OCEAN_CASCADE_COUNT] = {0.0f, 0.5f, 3.0f, 20.0f};
    f32 kMaxs[OCEAN_CASCADE_COUNT] = {0.5f, 3.0f, 20.0f, 230.0f};

    for (u32 i = 0; i < OCEAN_CASCADE_COUNT; i++)
    {
        if (!CreateCascade(device, sCascades[i], domain, kMins[i], kMaxs[i]))
        {
            return -1;
        }
        domain /= ratio;
    }

    u64 previousFrameTime = 0;
    u64 loopStartTime = Fly::ClockNow();
    u64 currentFrameTime = loopStartTime;

    sCamera.speed = 60.0f;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        previousFrameTime = currentFrameTime;
        currentFrameTime = Fly::ClockNow();
        f64 deltaTime = Fly::ToSeconds(currentFrameTime - previousFrameTime);
        sTime = Fly::ToSeconds(currentFrameTime - loopStartTime);

        // FLY_LOG("FPS %f", 1.0f / deltaTime);

        ImGuiIO& io = ImGui::GetIO();
        bool wantMouse = io.WantCaptureMouse;
        bool wantKeyboard = io.WantCaptureKeyboard;
        if (!wantMouse && !wantKeyboard)
        {
            sCamera.Update(window, deltaTime);
        }

        ProcessImGuiFrame();

        CameraData data;
        data.projection = sCamera.GetProjection();
        data.view = sCamera.GetView();
        data.screenSize = Math::Vec4(static_cast<f32>(device.swapchainWidth),
                                     static_cast<f32>(device.swapchainHeight),
                                     1.0f / device.swapchainWidth,
                                     1.0f / device.swapchainHeight);
        RHI::CopyDataToBuffer(device, &data, sizeof(CameraData), 0,
                              sCameraBuffers[device.frameIndex]);

        UpdateCascades(device);
        UpdateShadeParams(device);

        RHI::BeginRenderFrame(device);
        Draw(device);
        RHI::EndRenderFrame(device);
    }

    RHI::WaitAllDevicesIdle(context);

    for (u32 i = 0; i < OCEAN_CASCADE_COUNT; i++)
    {
        DestroyCascade(device, sCascades[i]);
    }

    DestroyResources(device);
    DestroyPipelines(device);
    DestroyImGuiContext(device);
    RHI::DestroyContext(context);
    glfwDestroyWindow(window);
    glfwTerminate();

    FLY_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
