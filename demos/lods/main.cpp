#include "core/assert.h"
#include "core/clock.h"
#include "core/log.h"
#include "core/platform.h"
#include "core/thread_context.h"

#include "rhi/allocation_callbacks.h"
#include "rhi/command_buffer.h"
#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"

#include "assets/geometry/mesh.h"

#include "utils/utils.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include "demos/common/simple_camera_fps.h"

#include <GLFW/glfw3.h>

using namespace Fly;

#define SCALE 1e10f
#define BRDF_INTEGRATION_LUT_SIZE 256

struct CameraData
{
    Math::Mat4 projection;
    Math::Mat4 view;
    Math::Vec4 screenSize;
    float near;
    float far;
    float halfTanFovHorizontal;
    float haflTanFovVertical;
};

struct MeshInstance
{
    Math::Vec3 position;
    f32 roughness;
    f32 metallic;
    f32 pad[3];
};

struct MeshData
{
    Math::Vec3 center;
    f32 radius;
    u32 lodCount;
};

struct LodInstanceOffset
{
    i32 lodIndex;
    u32 localInstanceOffset;
};

struct RadianceProjectionCoeff
{
    i64 r;
    i64 g;
    i64 b;
    i64 pad;
};

static VkQueryPool sTimestampQueryPool;
static VkDescriptorPool sImGuiDescriptorPool;
static RHI::GraphicsPipeline sGraphicsPipeline;
static RHI::GraphicsPipeline sSkyboxPipeline;
static RHI::GraphicsPipeline sPrefilterPipeline;
static RHI::ComputePipeline sCullPipeline;
static RHI::ComputePipeline sFirstInstancePrefixSumPipeline;
static RHI::ComputePipeline sRemapPipeline;
static RHI::ComputePipeline sRadianceProjectionPipeline;
static RHI::ComputePipeline sBrdfIntegrationPipeline;
static RHI::Buffer sCameraBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sMeshInstances;
static RHI::Buffer sMeshDataBuffer;
static RHI::Buffer sDrawCommands;
static RHI::Buffer sDrawCountBuffer;
static RHI::Buffer sLodInstanceOffsetBuffer;
static RHI::Buffer sRemapBuffer;
static RHI::Buffer sRadianceProjectionBuffer;
static RHI::Texture sDepthTexture;
static RHI::Texture sSkyboxTexture;
static RHI::Texture sBrdfIntegrationLUT;
static RHI::Texture sPrefilteredSkyboxTexture;
static VkImageView* sPrefilteredSkyboxImageViews;
static Mesh sMesh;

static i32 sInstanceRowCount = 15;
static bool sIsCullingFixed = false;
static f32 sTimestampPeriod;

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
                         nullptr, width, height, VK_FORMAT_D32_SFLOAT_S8_UINT,
                         RHI::Sampler::FilterMode::Nearest,
                         RHI::Sampler::WrapMode::Repeat, 1, sDepthTexture);
}

static void ErrorCallbackGLFW(int error, const char* description)
{
    FLY_ERROR("GLFW - error: %s", description);
}

static Fly::SimpleCameraFPS sCamera(90.0f, 1280.0f / 720.0f, 0.01f, 1000.0f,
                                    Math::Vec3(0.0f, 0.0f, -10.0f));

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

static bool CreatePipelines(RHI::Device& device)
{
    RHI::ComputePipeline* computePipelines[] = {
        &sCullPipeline, &sFirstInstancePrefixSumPipeline, &sRemapPipeline,
        &sRadianceProjectionPipeline, &sBrdfIntegrationPipeline};

    String8 computeShaderPaths[] = {
        FLY_STRING8_LITERAL("cull.comp.spv"),
        FLY_STRING8_LITERAL("prefix_sum.comp.spv"),
        FLY_STRING8_LITERAL("remap.comp.spv"),
        FLY_STRING8_LITERAL("radiance_projection.comp.spv"),
        FLY_STRING8_LITERAL("brdf_integration_lut.comp.spv")};

    for (u32 i = 0; i < STACK_ARRAY_COUNT(computeShaderPaths); i++)
    {
        RHI::Shader shader;
        if (!LoadShaderFromSpv(device, computeShaderPaths[i], shader))
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
    fixedState.depthStencilState.depthTestEnable = false;

    RHI::ShaderProgram shaderProgram{};
    if (!Fly::LoadShaderFromSpv(device, FLY_STRING8_LITERAL("skybox.vert.spv"),
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }

    if (!Fly::LoadShaderFromSpv(device, FLY_STRING8_LITERAL("skybox.frag.spv"),
                                shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
    }

    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     sSkyboxPipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    fixedState.pipelineRendering.depthAttachmentFormat =
        VK_FORMAT_D32_SFLOAT_S8_UINT;
    fixedState.depthStencilState.depthTestEnable = true;

    if (!Fly::LoadShaderFromSpv(device, FLY_STRING8_LITERAL("lit.vert.spv"),
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }

    if (!Fly::LoadShaderFromSpv(device, FLY_STRING8_LITERAL("lit.frag.spv"),
                                shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
    }

    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     sGraphicsPipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    fixedState.pipelineRendering.colorAttachments[0] =
        VK_FORMAT_R16G16B16A16_SFLOAT;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.pipelineRendering.viewMask = 0x3F;
    if (!Fly::LoadShaderFromSpv(device,
                                FLY_STRING8_LITERAL("prefilter.vert.spv"),
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }

    if (!Fly::LoadShaderFromSpv(device,
                                FLY_STRING8_LITERAL("prefilter.frag.spv"),
                                shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
    }
    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     sPrefilterPipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    return true;
}

static void DestroyPipelines(RHI::Device& device)
{
    RHI::DestroyGraphicsPipeline(device, sGraphicsPipeline);
    RHI::DestroyGraphicsPipeline(device, sSkyboxPipeline);
    RHI::DestroyGraphicsPipeline(device, sPrefilterPipeline);
    RHI::DestroyComputePipeline(device, sCullPipeline);
    RHI::DestroyComputePipeline(device, sFirstInstancePrefixSumPipeline);
    RHI::DestroyComputePipeline(device, sRemapPipeline);
    RHI::DestroyComputePipeline(device, sRadianceProjectionPipeline);
    RHI::DestroyComputePipeline(device, sBrdfIntegrationPipeline);
}

static bool CreateResources(RHI::Device& device)
{
    if (!ImportMesh(FLY_STRING8_LITERAL("dragon.fmesh"), device, sMesh))
    {
        FLY_ERROR("Failed to import mesh");
        return false;
    }
    FLY_LOG("Mesh vertex size is %u, lod count is %u", sMesh.vertexSize,
            sMesh.lodCount);
    FLY_LOG("Bounding sphere center is %f %f %f and radius %f",
            sMesh.sphereCenter.x, sMesh.sphereCenter.y, sMesh.sphereCenter.z,
            sMesh.sphereRadius);
    for (u32 i = 0; i < sMesh.lodCount; i++)
    {
        FLY_LOG("LOD %u: triangle count %u", i, sMesh.lods[i].indexCount / 3);
    }

    MeshData meshData;
    meshData.center = sMesh.sphereCenter;
    meshData.radius = sMesh.sphereRadius;
    meshData.lodCount = sMesh.lodCount;

    if (!RHI::CreateBuffer(device, false,
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           &meshData, sizeof(MeshData), sMeshDataBuffer))
    {
        FLY_LOG("Failed to create mesh data buffer");
        return false;
    }

    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    VkDrawIndexedIndirectCommand* drawCommands =
        FLY_PUSH_ARENA(arena, VkDrawIndexedIndirectCommand, FLY_MAX_LOD_COUNT);
    for (u32 i = 0; i < FLY_MAX_LOD_COUNT; i++)
    {
        drawCommands[i] = {};
        if (i < sMesh.lodCount)
        {
            drawCommands[i].indexCount = sMesh.lods[i].indexCount;
            drawCommands[i].instanceCount = 0;
            drawCommands[i].firstIndex = sMesh.lods[i].firstIndex;
            drawCommands[i].vertexOffset = 0;
            drawCommands[i].firstInstance = 0;
        }
    }

    if (!RHI::CreateBuffer(device, false,
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           drawCommands,
                           sizeof(VkDrawIndexedIndirectCommand) *
                               FLY_MAX_LOD_COUNT,
                           sDrawCommands))
    {
        FLY_ERROR("Failed to create draw commands buffer");
        return false;
    }

    if (!RHI::CreateBuffer(device, false,
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           nullptr, sizeof(u32), sDrawCountBuffer))
    {
        FLY_ERROR("Failed to create draw count buffer");
        return false;
    }

    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateBuffer(device, true, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                               nullptr, sizeof(CameraData), sCameraBuffers[i]))
        {
            FLY_ERROR("Failed to create uniform buffer %u", i);
            return false;
        }
    }

    MeshInstance* meshInstances = FLY_PUSH_ARENA(
        arena, MeshInstance, sInstanceRowCount * sInstanceRowCount);
    for (i32 i = 0; i < sInstanceRowCount; i++)
    {
        for (i32 j = 0; j < sInstanceRowCount; j++)
        {
            meshInstances[i * sInstanceRowCount + j].position =
                Math::Vec3(i * 5.0f, 0.0f, -j * 5.0f);
            meshInstances[i * sInstanceRowCount + j].roughness =
                float(j) / Math::Max((sInstanceRowCount - 1), 1);
            meshInstances[i * sInstanceRowCount + j].metallic =
                float(i) / Math::Max((sInstanceRowCount - 1), 1);
        }
    }

    if (!RHI::CreateBuffer(device, false,
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           meshInstances,
                           sizeof(MeshInstance) * sInstanceRowCount *
                               sInstanceRowCount,
                           sMeshInstances))
    {
        FLY_ERROR("Failed to create instance buffer");
        return false;
    }

    ArenaPopToMarker(arena, marker);

    if (!RHI::CreateBuffer(device, false,
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           nullptr,
                           sizeof(LodInstanceOffset) * sInstanceRowCount *
                               sInstanceRowCount,
                           sLodInstanceOffsetBuffer))
    {
        FLY_ERROR("Failed to create lod, instance offset buffer");
        return false;
    }

    if (!RHI::CreateBuffer(device, false,
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           nullptr,
                           sizeof(u32) * sInstanceRowCount * sInstanceRowCount,
                           sRemapBuffer))
    {
        FLY_ERROR("Failed to create remap buffer");
        return false;
    }

    if (!RHI::CreateTexture2D(
            device, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, nullptr,
            device.swapchainWidth, device.swapchainHeight,
            VK_FORMAT_D32_SFLOAT_S8_UINT, RHI::Sampler::FilterMode::Nearest,
            RHI::Sampler::WrapMode::Repeat, 1, sDepthTexture))
    {
        FLY_ERROR("Failed to create depth texture");
        return false;
    }

    if (!RHI::CreateTexture2D(
            device, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            nullptr, BRDF_INTEGRATION_LUT_SIZE, BRDF_INTEGRATION_LUT_SIZE,
            VK_FORMAT_R16G16_SFLOAT, RHI::Sampler::FilterMode::Bilinear,
            RHI::Sampler::WrapMode::Clamp, 1, sBrdfIntegrationLUT))
    {
        FLY_ERROR("Failed to create brdf integration lut");
        return false;
    }

    if (!LoadCubemap(device,
                     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     FLY_STRING8_LITERAL("day.exr"),
                     VK_FORMAT_R16G16B16A16_SFLOAT,
                     RHI::Sampler::FilterMode::Bilinear, 1, sSkyboxTexture))
    {
        FLY_ERROR("Failed to load cubemap");
        return false;
    }

    if (!CreateCubemap(
            device,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            nullptr, sSkyboxTexture.width, sSkyboxTexture.format,
            RHI::Sampler::FilterMode::Trilinear, 0, sPrefilteredSkyboxTexture))
    {
        FLY_ERROR("Failed to create prefiltered skybox");
        return false;
    }

    RadianceProjectionCoeff data[9];
    memset(data, 0, sizeof(data));
    if (!RHI::CreateBuffer(device, true,
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           data, sizeof(RadianceProjectionCoeff) * 9,
                           sRadianceProjectionBuffer))
    {
        FLY_ERROR("Failed to create radiance projection buffer");
        return false;
    }

    VkQueryPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    createInfo.queryCount = 2;

    if (vkCreateQueryPool(device.logicalDevice, &createInfo,
                          RHI::GetVulkanAllocationCallbacks(),
                          &sTimestampQueryPool) != VK_SUCCESS)
    {
        return false;
    }

    return true;
}

static void ProcessImGuiFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Culling");
    ImGui::Checkbox("Fix current cull", &sIsCullingFixed);
    ImGui::End();

    ImGui::Render();
}

static void DestroyResources(RHI::Device& device)
{
    vkDestroyQueryPool(device.logicalDevice, sTimestampQueryPool,
                       RHI::GetVulkanAllocationCallbacks());

    DestroyMesh(device, sMesh);
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        RHI::DestroyBuffer(device, sCameraBuffers[i]);
    }
    RHI::DestroyBuffer(device, sMeshInstances);
    RHI::DestroyBuffer(device, sLodInstanceOffsetBuffer);
    RHI::DestroyBuffer(device, sRemapBuffer);
    RHI::DestroyBuffer(device, sDrawCommands);
    RHI::DestroyBuffer(device, sDrawCountBuffer);
    RHI::DestroyBuffer(device, sMeshDataBuffer);
    RHI::DestroyBuffer(device, sRadianceProjectionBuffer);
    RHI::DestroyTexture(device, sDepthTexture);
    RHI::DestroyTexture(device, sSkyboxTexture);
    RHI::DestroyTexture(device, sBrdfIntegrationLUT);
    RHI::DestroyTexture(device, sPrefilteredSkyboxTexture);
}

static void RecordComputeBrdfIntegrationLUT(
    RHI::CommandBuffer& cmd, const RHI::RecordBufferInput* bufferInput,
    u32 bufferInputCount, const RHI::RecordTextureInput* textureInput,
    u32 textureInputCount, void* pUserData)
{
    RHI::BindComputePipeline(cmd, sBrdfIntegrationPipeline);

    RHI::Texture& brdfIntegrationMap = *(textureInput[0].pTexture);
    u32 width = brdfIntegrationMap.width;
    u32 height = brdfIntegrationMap.height;

    u32 pushConstants[] = {brdfIntegrationMap.bindlessStorageHandle, width,
                           height};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, width / 16, height / 16, 1);
}

static void ComputeBrdfIntegrationLUT(RHI::Device& device,
                                      RHI::CommandBuffer& cmd)
{

    RHI::RecordTextureInput textureInput = {&sBrdfIntegrationLUT,
                                            VK_ACCESS_2_SHADER_WRITE_BIT,
                                            VK_IMAGE_LAYOUT_GENERAL};
    RHI::ExecuteCompute(cmd, RecordComputeBrdfIntegrationLUT, nullptr, 0,
                        &textureInput, 1);
}

static void RecordPrefilterEnvironmentMap(
    RHI::CommandBuffer& cmd, const RHI::RecordBufferInput* bufferInput,
    u32 bufferInputCount, const RHI::RecordTextureInput* textureInput,
    u32 textureInputCount, void* pUserData)
{
    RHI::Texture& skybox = *(textureInput[0].pTexture);
    RHI::Texture& prefilterMap = *(textureInput[1].pTexture);

    u32 size = prefilterMap.width >> textureInput[1].baseMipLevel;

    RHI::SetViewport(cmd, 0, 0, static_cast<f32>(size), static_cast<f32>(size),
                     0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, size, size);

    RHI::BindGraphicsPipeline(cmd, sPrefilterPipeline);

    u32 pushConstants[] = {skybox.bindlessHandle, textureInput[1].baseMipLevel,
                           prefilterMap.mipCount};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Draw(cmd, 6, 1, 0, 0);
}

static void PrefilterEnvironmentMap(RHI::Device& device,
                                    RHI::CommandBuffer& cmd)
{

    RHI::RecordTextureInput textureInput[2] = {
        {&sSkyboxTexture, VK_ACCESS_2_SHADER_READ_BIT,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {&sPrefilteredSkyboxTexture, VK_ACCESS_2_SHADER_WRITE_BIT,
         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

    for (u32 i = 0; i < sPrefilteredSkyboxTexture.mipCount; i++)
    {
        textureInput[1].baseMipLevel = i;
        textureInput[1].levelCount = 1;

        u32 size =
            sPrefilteredSkyboxTexture.width >> textureInput[1].baseMipLevel;

        VkRenderingAttachmentInfo colorAttachment =
            RHI::ColorAttachmentInfo(sPrefilteredSkyboxImageViews[i]);
        VkRenderingInfo renderingInfo =
            RHI::RenderingInfo({{0, 0}, {size, size}}, &colorAttachment, 1,
                               nullptr, nullptr, 6, 0x3F);

        RHI::ExecuteGraphics(cmd, renderingInfo, RecordPrefilterEnvironmentMap,
                             nullptr, 0, textureInput, 2);
    }

    RHI::RecordTransitionImageLayout(cmd, sPrefilteredSkyboxTexture.image,
                                     VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    sPrefilteredSkyboxTexture.imageLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

static void RecordDrawSky(RHI::CommandBuffer& cmd,
                          const RHI::RecordBufferInput* bufferInput,
                          u32 bufferInputCount,
                          const RHI::RecordTextureInput* textureInput,
                          u32 textureInputCount, void* pUserData)
{
    RHI::SetViewport(cmd, 0, 0, static_cast<f32>(cmd.device->swapchainWidth),
                     static_cast<f32>(cmd.device->swapchainHeight), 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, cmd.device->swapchainWidth,
                    cmd.device->swapchainHeight);

    RHI::Buffer& cameraBuffer = *(bufferInput[0].pBuffer);
    RHI::Texture& skybox = *(textureInput[0].pTexture);

    RHI::BindGraphicsPipeline(cmd, sSkyboxPipeline);
    u32 pushConstants[] = {cameraBuffer.bindlessHandle, skybox.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Draw(cmd, 6, 1, 0, 0);
}

static void DrawSky(RHI::Device& device)
{
    RHI::RecordBufferInput bufferInput = {&sCameraBuffers[device.frameIndex],
                                          VK_ACCESS_2_SHADER_READ_BIT};
    RHI::RecordTextureInput textureInput = {
        &sSkyboxTexture, VK_ACCESS_2_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    VkRenderingAttachmentInfo colorAttachment =
        RHI::ColorAttachmentInfo(RenderFrameSwapchainTexture(device).imageView);
    VkRenderingInfo renderingInfo = RHI::RenderingInfo(
        {{0, 0}, {device.swapchainWidth, device.swapchainHeight}},
        &colorAttachment, 1);
    RHI::ExecuteGraphics(RenderFrameCommandBuffer(device), renderingInfo,
                         RecordDrawSky, &bufferInput, 1, &textureInput, 1);
}

static void RecordProjectRadiance(RHI::CommandBuffer& cmd,
                                  const RHI::RecordBufferInput* bufferInput,
                                  u32 bufferInputCount,
                                  const RHI::RecordTextureInput* textureInput,
                                  u32 textureInputCount, void* pUserData)
{
    RHI::BindComputePipeline(cmd, sRadianceProjectionPipeline);

    RHI::Texture& radianceMap = *(textureInput[0].pTexture);
    RHI::Buffer& radianceProjectionBuffer = *(bufferInput[0].pBuffer);

    u32 radianceMapSize = radianceMap.width;
    u32 pushConstants[] = {radianceMap.bindlessStorageHandle,
                           radianceProjectionBuffer.bindlessHandle,
                           radianceMapSize};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, radianceMapSize / 16, radianceMapSize / 16, 6);
}

static void ProjectRadiance(RHI::Device& device, RHI::CommandBuffer& cmd)
{
    RHI::RecordBufferInput bufferInput = {&sRadianceProjectionBuffer,
                                          VK_ACCESS_2_SHADER_WRITE_BIT};
    RHI::RecordTextureInput textureInput = {
        &sSkyboxTexture, VK_ACCESS_2_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    RHI::ExecuteCompute(cmd, RecordProjectRadiance, &bufferInput, 1,
                        &textureInput, 1);
}

static void RecordCull(RHI::CommandBuffer& cmd,
                       const RHI::RecordBufferInput* bufferInput,
                       u32 bufferInputCount,
                       const RHI::RecordTextureInput* textureInput,
                       u32 textureInputCount, void* pUserData)
{
    RHI::BindComputePipeline(cmd, sCullPipeline);

    RHI::Buffer& cameraBuffer = *(bufferInput[0].pBuffer);
    RHI::Buffer& instanceBuffer = *(bufferInput[1].pBuffer);
    RHI::Buffer& meshDataBuffer = *(bufferInput[2].pBuffer);
    RHI::Buffer& lodInstanceOffsetBuffer = *(bufferInput[3].pBuffer);
    RHI::Buffer& drawCommandBuffer = *(bufferInput[4].pBuffer);
    RHI::Buffer& drawCountBuffer = *(bufferInput[5].pBuffer);

    u32 pushConstants[] = {
        cameraBuffer.bindlessHandle,
        instanceBuffer.bindlessHandle,
        meshDataBuffer.bindlessHandle,
        lodInstanceOffsetBuffer.bindlessHandle,
        drawCommandBuffer.bindlessHandle,
        drawCountBuffer.bindlessHandle,
        static_cast<u32>(sInstanceRowCount * sInstanceRowCount)};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd,
                  static_cast<u32>(Math::Ceil(
                      (sInstanceRowCount * sInstanceRowCount) / 256.0f)),
                  1, 1);
}

static void Cull(RHI::Device& device)
{
    RHI::RecordBufferInput bufferInput[6] = {
        {&sCameraBuffers[device.frameIndex], VK_ACCESS_2_SHADER_READ_BIT},
        {&sMeshInstances, VK_ACCESS_2_SHADER_READ_BIT},
        {&sMeshDataBuffer, VK_ACCESS_2_SHADER_READ_BIT},
        {&sLodInstanceOffsetBuffer, VK_ACCESS_2_SHADER_WRITE_BIT},
        {&sDrawCommands,
         VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT},
        {&sDrawCountBuffer,
         VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT}};

    RHI::ExecuteCompute(RenderFrameCommandBuffer(device), RecordCull,
                        bufferInput, 6);
}

static void RecordFirstInstancePrefixSum(
    RHI::CommandBuffer& cmd, const RHI::RecordBufferInput* bufferInput,
    u32 bufferInputCount, const RHI::RecordTextureInput* textureInput,
    u32 textureInputCount, void* pUserData)
{
    RHI::BindComputePipeline(cmd, sFirstInstancePrefixSumPipeline);

    RHI::Buffer& drawCommandBuffer = *(bufferInput[0].pBuffer);

    u32 pushConstants[] = {drawCommandBuffer.bindlessHandle, FLY_MAX_LOD_COUNT};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, 1, 1, 1);
}

static void FirstInstancePrefixSum(RHI::Device& device)
{
    RHI::RecordBufferInput bufferInput = {&sDrawCommands,
                                          VK_ACCESS_2_SHADER_WRITE_BIT};
    RHI::ExecuteCompute(RenderFrameCommandBuffer(device),
                        RecordFirstInstancePrefixSum, &bufferInput, 1);
}

static void RecordRemap(RHI::CommandBuffer& cmd,
                        const RHI::RecordBufferInput* bufferInput,
                        u32 bufferInputCount,
                        const RHI::RecordTextureInput* textureInput,
                        u32 textureInputCount, void* pUserData)
{
    RHI::BindComputePipeline(cmd, sRemapPipeline);

    RHI::Buffer& lodInstanceOffsetBuffer = *(bufferInput[0].pBuffer);
    RHI::Buffer& drawCommandBuffer = *(bufferInput[1].pBuffer);
    RHI::Buffer& remapBuffer = *(bufferInput[2].pBuffer);

    u32 pushConstants[] = {
        lodInstanceOffsetBuffer.bindlessHandle,
        drawCommandBuffer.bindlessHandle, remapBuffer.bindlessHandle,
        static_cast<u32>(sInstanceRowCount * sInstanceRowCount)};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd,
                  static_cast<u32>(Math::Ceil(
                      (sInstanceRowCount * sInstanceRowCount) / 256.0f)),
                  1, 1);
}

static void Remap(RHI::Device& device)
{
    RHI::RecordBufferInput bufferInput[3] = {
        {&sLodInstanceOffsetBuffer, VK_ACCESS_2_SHADER_READ_BIT},
        {&sDrawCommands, VK_ACCESS_2_SHADER_READ_BIT},
        {&sRemapBuffer, VK_ACCESS_2_SHADER_WRITE_BIT}};

    RHI::ExecuteCompute(RenderFrameCommandBuffer(device), RecordRemap,
                        bufferInput, 3);
}

static void RecordDrawMesh(RHI::CommandBuffer& cmd,
                           const RHI::RecordBufferInput* bufferInput,
                           u32 bufferInputCount,
                           const RHI::RecordTextureInput* textureInput,
                           u32 textureInputCount, void* pUserData)
{
    RHI::WriteTimestamp(cmd, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                        sTimestampQueryPool, 0);

    RHI::SetViewport(cmd, 0, 0, static_cast<f32>(cmd.device->swapchainWidth),
                     static_cast<f32>(cmd.device->swapchainHeight), 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, cmd.device->swapchainWidth,
                    cmd.device->swapchainHeight);

    RHI::BindGraphicsPipeline(cmd, sGraphicsPipeline);

    RHI::Buffer& cameraBuffer = *(bufferInput[0].pBuffer);
    RHI::Buffer& meshInstanceBuffer = *(bufferInput[1].pBuffer);
    RHI::Buffer& remapBuffer = *(bufferInput[2].pBuffer);
    RHI::Buffer& radianceProjectionBuffer = *(bufferInput[3].pBuffer);

    RHI::Texture& brdfIntegrationLUT = *(textureInput[0].pTexture);
    RHI::Texture& prefilteredMap = *(textureInput[1].pTexture);

    RHI::BindIndexBuffer(cmd, sMesh.indexBuffer, VK_INDEX_TYPE_UINT32);

    u32 pushConstants[] = {cameraBuffer.bindlessHandle,
                           sMesh.vertexBuffer.bindlessHandle,
                           remapBuffer.bindlessHandle,
                           meshInstanceBuffer.bindlessHandle,
                           radianceProjectionBuffer.bindlessHandle,
                           brdfIntegrationLUT.bindlessHandle,
                           prefilteredMap.bindlessHandle,
                           prefilteredMap.mipCount};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));

    RHI::DrawIndexedIndirectCount(cmd, sDrawCommands, 0, sDrawCountBuffer, 0,
                                  FLY_MAX_LOD_COUNT,
                                  sizeof(VkDrawIndexedIndirectCommand));

    RHI::WriteTimestamp(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        sTimestampQueryPool, 1);
}

static void DrawMesh(RHI::Device& device)
{
    RHI::RecordBufferInput bufferInput[6] = {
        {&sCameraBuffers[device.frameIndex], VK_ACCESS_2_SHADER_READ_BIT},
        {&sMeshInstances, VK_ACCESS_2_SHADER_READ_BIT},
        {&sRemapBuffer, VK_ACCESS_2_SHADER_READ_BIT},
        {&sRadianceProjectionBuffer, VK_ACCESS_2_SHADER_READ_BIT},
        {&sDrawCommands, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT},
        {&sDrawCountBuffer, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT}};

    RHI::RecordTextureInput textureInput[3] = {
        {&sBrdfIntegrationLUT, VK_ACCESS_2_SHADER_READ_BIT,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {&sPrefilteredSkyboxTexture, VK_ACCESS_2_SHADER_READ_BIT,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {&sDepthTexture, VK_ACCESS_2_SHADER_WRITE_BIT,
         VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}};

    VkRenderingAttachmentInfo colorAttachment =
        RHI::ColorAttachmentInfo(RenderFrameSwapchainTexture(device).imageView,
                                 VK_ATTACHMENT_LOAD_OP_LOAD);
    VkRenderingAttachmentInfo depthAttachment =
        RHI::DepthAttachmentInfo(sDepthTexture.imageView);
    VkRenderingInfo renderingInfo = RHI::RenderingInfo(
        {{0, 0}, {device.swapchainWidth, device.swapchainHeight}},
        &colorAttachment, 1, &depthAttachment);

    RHI::ExecuteGraphics(RenderFrameCommandBuffer(device), renderingInfo,
                         RecordDrawMesh, bufferInput, 6, textureInput, 3);
}

static void RecordDrawGUI(RHI::CommandBuffer& cmd,
                          const RHI::RecordBufferInput* bufferInput,
                          u32 bufferInputCount,
                          const RHI::RecordTextureInput* textureInput,
                          u32 textureInputCount, void* pUserData)
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

int main(int argc, char* argv[])
{
    InitThreadContext();
    if (!InitLogger())
    {
        return -1;
    }

    Arena& arena = GetScratchArena();

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
    GLFWwindow* window = glfwCreateWindow(1280, 720, "LODs", nullptr, nullptr);

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
    settings.vulkan12Features.shaderFloat16 = true;
    settings.vulkan12Features.drawIndirectCount = true;
    settings.vulkan12Features.shaderBufferInt64Atomics = true;
    settings.vulkan12Features.shaderSharedInt64Atomics = true;
    settings.vulkan12Features.shaderSubgroupExtendedTypes = true;
    settings.vulkan11Features.storageBuffer16BitAccess = true;
    settings.vulkan11Features.multiview = true;
    settings.features2.features.shaderInt64 = true;

    RHI::Context context;
    if (!RHI::CreateContext(settings, context))
    {
        FLY_ERROR("Failed to create context");
        return -1;
    }

    RHI::Device& device = context.devices[0];
    device.swapchainRecreatedCallback.func = OnFramebufferResize;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device.physicalDevice, &props);
    if (props.limits.timestampPeriod == 0)
    {
        FLY_ERROR("Device does not support timestamp queries");
        return -1;
    }
    sTimestampPeriod = props.limits.timestampPeriod;

    if (!CreateImGuiContext(context, device, window))
    {
        FLY_ERROR("Failed to create imgui context");
        return -1;
    }

    if (!CreateResources(device))
    {
        FLY_ERROR("Failed to create resources");
        return -1;
    }

    sPrefilteredSkyboxImageViews =
        FLY_PUSH_ARENA(arena, VkImageView, sPrefilteredSkyboxTexture.mipCount);
    for (u32 i = 0; i < sPrefilteredSkyboxTexture.mipCount; i++)
    {
        sPrefilteredSkyboxImageViews[i] = RHI::CreateImageView(
            device, sPrefilteredSkyboxTexture, VK_IMAGE_VIEW_TYPE_CUBE, i, 1);
    }

    if (!CreatePipelines(device))
    {
        FLY_ERROR("Failed to create pipeline");
        return -1;
    }

    u64 previousFrameTime = 0;
    u64 loopStartTime = Fly::ClockNow();
    u64 currentFrameTime = loopStartTime;

    float halfTanFovHorizontal =
        Math::Tan(Math::Radians(sCamera.GetHorizontalFov()) * 0.5f);
    float halfTanFovVertical =
        Math::Tan(Math::Radians(sCamera.GetVerticalFov()) * 0.5f);

    RHI::BeginOneTimeSubmit(device);
    ProjectRadiance(device, RHI::OneTimeSubmitCommandBuffer(device));
    ComputeBrdfIntegrationLUT(device, RHI::OneTimeSubmitCommandBuffer(device));
    PrefilterEnvironmentMap(device, RHI::OneTimeSubmitCommandBuffer(device));
    RHI::EndOneTimeSubmit(device);

    while (!glfwWindowShouldClose(window))
    {
        previousFrameTime = currentFrameTime;
        currentFrameTime = Fly::ClockNow();

        f64 deltaTime = Fly::ToSeconds(currentFrameTime - previousFrameTime);

        glfwPollEvents();

        {
            sCamera.Update(window, deltaTime);
            CameraData cameraData = {
                sCamera.GetProjection(),
                sCamera.GetView(),
                Math::Vec4(static_cast<f32>(device.swapchainWidth),
                           static_cast<f32>(device.swapchainHeight),
                           1.0f / device.swapchainWidth,
                           1.0f / device.swapchainHeight),
                sCamera.GetNear(),
                sCamera.GetFar(),
                halfTanFovHorizontal,
                halfTanFovVertical};
            RHI::Buffer& cameraBuffer = sCameraBuffers[device.frameIndex];
            RHI::CopyDataToBuffer(device, &cameraData, sizeof(CameraData), 0,
                                  cameraBuffer);
        }

        ProcessImGuiFrame();

        RHI::BeginRenderFrame(device);
        RHI::ResetQueryPool(RenderFrameCommandBuffer(device),
                            sTimestampQueryPool, 0, 2);

        if (!sIsCullingFixed)
        {
            Cull(device);
            FirstInstancePrefixSum(device);
            Remap(device);
        }

        DrawSky(device);
        DrawMesh(device);
        DrawGUI(device);
        RHI::EndRenderFrame(device);

        // RadianceProjectionCoeff* coeffs =
        // static_cast<RadianceProjectionCoeff*>(
        //     RHI::BufferMappedPtr(sRadianceProjectionBuffer));
        // for (u32 i = 0; i < 9; i++)
        // {
        //     FLY_LOG("%u %f %f %f", i, coeffs[i].r / SCALE, coeffs[i].g /
        //     SCALE,
        //             coeffs[i].b / SCALE);
        // }

        // u64 timestamps[2];
        // vkGetQueryPoolResults(device.logicalDevice, sTimestampQueryPool, 0,
        // 2,
        //                       sizeof(timestamps), timestamps,
        //                       sizeof(uint64_t), VK_QUERY_RESULT_64_BIT |
        //                           VK_QUERY_RESULT_WAIT_BIT);
        // f64 drawTime = Fly::ToMilliseconds(static_cast<u64>(
        //     (timestamps[1] - timestamps[0]) * sTimestampPeriod));

        // FLY_LOG("Dragon draw: %f ms", drawTime);
    }

    RHI::WaitDeviceIdle(device);
    DestroyPipelines(device);
    DestroyResources(device);
    for (u32 i = 0; i < sPrefilteredSkyboxTexture.mipCount; i++)
    {
        vkDestroyImageView(device.logicalDevice,
                           sPrefilteredSkyboxImageViews[i],
                           RHI::GetVulkanAllocationCallbacks());
    }
    DestroyImGuiContext(device);
    RHI::DestroyContext(context);

    glfwDestroyWindow(window);
    glfwTerminate();
    FLY_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
