#include "core/clock.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "rhi/allocation_callbacks.h"
#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"

#include "demos/common/scene.h"
#include "demos/common/simple_camera_fps.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>

using namespace Fly;

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

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
};

struct UniformData
{
    Math::Mat4 projection;
    Math::Mat4 view;
    Math::Vec4 fetchSpeedDirSpread;
    Math::Vec4 domainTimeLambdaScale;
    Math::Vec4 screenSize;
};
static UniformData sUniformData;
static i32 sTileSize = 16;

static Fly::SimpleCameraFPS
    sCamera(90.0f, static_cast<f32>(WINDOW_WIDTH) / WINDOW_HEIGHT, 0.01f,
            500.0f, Fly::Math::Vec3(0.0f, 10.0f, -5.0f));

static bool IsPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice,
                                     const RHI::PhysicalDeviceInfo& info)
{

    VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT
        unusedAttachmentsFeature{};
    unusedAttachmentsFeature.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT;
    unusedAttachmentsFeature.pNext = nullptr;

    VkPhysicalDeviceMultiviewFeatures multiviewFeatures{};
    multiviewFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
    multiviewFeatures.pNext = &unusedAttachmentsFeature;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &multiviewFeatures;

    vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

    if (!multiviewFeatures.multiview ||
        !unusedAttachmentsFeature.dynamicRenderingUnusedAttachments)
    {
        return false;
    }

    return info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

static void ErrorCallbackGLFW(i32 error, const char* description)
{
    FLY_ERROR("GLFW - error: %s", description);
}

static RHI::ComputePipeline sFFTPipeline;
static RHI::ComputePipeline sTransposePipeline;
static RHI::ComputePipeline sJonswapPipeline;
static RHI::ComputePipeline sCopyPipeline;
static RHI::GraphicsPipeline sGraphicsPipeline;
static RHI::GraphicsPipeline sWireframeGraphicsPipeline;
static RHI::GraphicsPipeline sSkyPipeline;
static RHI::GraphicsPipeline sSkyBoxPipeline;
static RHI::GraphicsPipeline* sCurrentGraphicsPipeline;
static bool CreatePipelines(RHI::Device& device)
{
    RHI::ComputePipeline* computePipelines[] = {
        &sFFTPipeline, &sTransposePipeline, &sJonswapPipeline, &sCopyPipeline};

    const char* computeShaderPaths[] = {"ifft.comp.spv", "transpose.comp.spv",
                                        "jonswap.comp.spv", "copy.comp.spv"};

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
    fixedState.pipelineRendering.depthAttachmentFormat =
        device.depthTexture.format;
    fixedState.depthStencilState.depthTestEnable = true;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.colorBlendState.attachmentCount = 1;
    fixedState.rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    fixedState.rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    fixedState.rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    fixedState.inputAssemblyState.topology =
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    RHI::ShaderProgram shaderProgram{};
    if (!Fly::LoadShaderFromSpv(device, "ocean.vert.spv",
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }
    if (!Fly::LoadShaderFromSpv(device, "ocean.frag.spv",
                                shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
    }

    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     sGraphicsPipeline))
    {
        return false;
    }
    fixedState.rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     sWireframeGraphicsPipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    fixedState.rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    fixedState.depthStencilState.depthTestEnable = false;
    fixedState.pipelineRendering.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    if (!Fly::LoadShaderFromSpv(device, "screen_quad.vert.spv",
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }
    if (!Fly::LoadShaderFromSpv(device, "sky.frag.spv",
                                shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
    }
    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     sSkyPipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    fixedState.pipelineRendering.colorAttachments[0] = VK_FORMAT_R8G8B8A8_SRGB;
    fixedState.pipelineRendering.viewMask = 0x0000007F;
    if (!Fly::LoadShaderFromSpv(device, "skybox.frag.spv",
                                shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
    }
    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     sSkyBoxPipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    return true;
}

static void DestroyPipelines(RHI::Device& device)
{
    RHI::DestroyGraphicsPipeline(device, sSkyBoxPipeline);
    RHI::DestroyGraphicsPipeline(device, sSkyPipeline);
    RHI::DestroyGraphicsPipeline(device, sWireframeGraphicsPipeline);
    RHI::DestroyGraphicsPipeline(device, sGraphicsPipeline);
    RHI::DestroyComputePipeline(device, sCopyPipeline);
    RHI::DestroyComputePipeline(device, sJonswapPipeline);
    RHI::DestroyComputePipeline(device, sTransposePipeline);
    RHI::DestroyComputePipeline(device, sFFTPipeline);
}

static VkDescriptorPool sImGuiDescriptorPool;
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

static RHI::Buffer sUniformBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sOceanFrequencyBuffers[2 * FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sVertexBuffer;
static RHI::Buffer sIndexBuffer;
static u32 sIndexCount;
static RHI::Texture sHeightMaps[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Texture sDiffDisplacementMaps[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Cubemap sSkyBoxes[FLY_FRAME_IN_FLIGHT_COUNT];
static bool CreateDeviceObjects(RHI::Device& device)
{
    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    f32 quadSize = 1.0f / 16.0f;
    i32 quadPerSide = static_cast<i32>(sTileSize / quadSize);
    i32 offset = quadPerSide / 2;
    u32 vertexPerSide = quadPerSide + 1;

    FLY_LOG("vertex per side %u", vertexPerSide);

    // Generate vertices
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
            vertex.uv =
                Math::Vec2((x + offset) / static_cast<f32>(quadPerSide),
                           (z + offset) / static_cast<f32>(quadPerSide));
        }
    }

    if (!CreateStorageBuffer(device, false, vertices, count * sizeof(Vertex),
                             sVertexBuffer))
    {
        ArenaPopToMarker(arena, marker);
        return false;
    }
    ArenaPopToMarker(arena, marker);

    // Generate indices
    sIndexCount =
        static_cast<u32>(6 * (sTileSize * sTileSize) / (quadSize * quadSize));
    u32* indices = FLY_PUSH_ARENA(arena, u32, sIndexCount);
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
    if (!CreateIndexBuffer(device, indices, sizeof(u32) * sIndexCount,
                           sIndexBuffer))
    {
        return false;
    }
    ArenaPopToMarker(arena, marker);

    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateUniformBuffer(device, nullptr, sizeof(UniformData),
                                      sUniformBuffers[i]))
        {
            return false;
        }

        for (u32 j = 0; j < 2; j++)
        {
            if (!RHI::CreateStorageBuffer(device, false, nullptr,
                                          sizeof(OceanFrequencyVertex) * 256 *
                                              256,
                                          sOceanFrequencyBuffers[i * 2 + j]))
            {
                return false;
            }
        }

        if (!RHI::CreateReadWriteTexture(
                device, nullptr, 256 * 256 * sizeof(u16), 256, 256,
                VK_FORMAT_R16_SFLOAT, RHI::Sampler::FilterMode::Bilinear,
                RHI::Sampler::WrapMode::Repeat, sHeightMaps[i]))
        {
            return false;
        }

        if (!RHI::CreateReadWriteTexture(
                device, nullptr, 256 * 256 * 4 * sizeof(u16), 256, 256,
                VK_FORMAT_R16G16B16A16_SFLOAT,
                RHI::Sampler::FilterMode::Bilinear,
                RHI::Sampler::WrapMode::Repeat, sDiffDisplacementMaps[i]))
        {
            return false;
        }

        if (!RHI::CreateCubemap(device, nullptr, 256 * 256 * 6 * 4 * sizeof(u8),
                                256, VK_FORMAT_R8G8B8A8_SRGB,
                                RHI::Sampler::FilterMode::Bilinear,
                                sSkyBoxes[i]))
        {
            return false;
        }
    }

    return true;
}

static void DestroyDeviceObjects(RHI::Device& device)
{
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        for (u32 j = 0; j < 2; j++)
        {
            RHI::DestroyBuffer(device, sOceanFrequencyBuffers[2 * i + j]);
        }
        RHI::DestroyBuffer(device, sUniformBuffers[i]);
        RHI::DestroyTexture(device, sHeightMaps[i]);
        RHI::DestroyTexture(device, sDiffDisplacementMaps[i]);
        RHI::DestroyCubemap(device, sSkyBoxes[i]);
    }
    RHI::DestroyBuffer(device, sIndexBuffer);
    RHI::DestroyBuffer(device, sVertexBuffer);
}

static void JonswapPass(RHI::Device& device)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    RHI::BindComputePipeline(device, cmd, sJonswapPipeline);

    u32 pushConstants[] = {
        sOceanFrequencyBuffers[2 * device.frameIndex].bindlessHandle,
        sUniformBuffers[device.frameIndex].bindlessHandle, 256};
    RHI::SetPushConstants(device, cmd, &pushConstants, sizeof(pushConstants));

    vkCmdDispatch(cmd.handle, 256, 1, 1);
    VkBufferMemoryBarrier jonswapToIFFTBarrier = RHI::BufferMemoryBarrier(
        sOceanFrequencyBuffers[2 * device.frameIndex],
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1,
                         &jonswapToIFFTBarrier, 0, nullptr);
}

static void IFFTPass(RHI::Device& device)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    RHI::BindComputePipeline(device, cmd, sFFTPipeline);
    u32 pushConstantsFFTX[] = {
        sOceanFrequencyBuffers[2 * device.frameIndex].bindlessHandle};
    RHI::SetPushConstants(device, cmd, pushConstantsFFTX,
                          sizeof(pushConstantsFFTX));
    vkCmdDispatch(cmd.handle, 256, 1, 1);
    VkBufferMemoryBarrier IFFTToTransposeBarrier = RHI::BufferMemoryBarrier(
        sOceanFrequencyBuffers[2 * device.frameIndex],
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT);
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1,
                         &IFFTToTransposeBarrier, 0, nullptr);

    RHI::BindComputePipeline(device, cmd, sTransposePipeline);
    u32 pushConstantsTranspose[] = {
        sOceanFrequencyBuffers[2 * device.frameIndex].bindlessHandle,
        sOceanFrequencyBuffers[2 * device.frameIndex + 1].bindlessHandle, 256};
    RHI::SetPushConstants(device, cmd, pushConstantsTranspose,
                          sizeof(pushConstantsTranspose));
    vkCmdDispatch(cmd.handle, 16, 16, 1);
    VkBufferMemoryBarrier transposeToFFTBarrier = RHI::BufferMemoryBarrier(
        sOceanFrequencyBuffers[2 * device.frameIndex + 1],
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1,
                         &transposeToFFTBarrier, 0, nullptr);

    RHI::BindComputePipeline(device, cmd, sFFTPipeline);
    u32 pushConstantsFFTY[] = {
        sOceanFrequencyBuffers[2 * device.frameIndex + 1].bindlessHandle, 1, 1};
    RHI::SetPushConstants(device, cmd, pushConstantsFFTY,
                          sizeof(pushConstantsFFTY));
    vkCmdDispatch(cmd.handle, 256, 1, 1);
    VkBufferMemoryBarrier fftToCopyBarrier = RHI::BufferMemoryBarrier(
        sOceanFrequencyBuffers[2 * device.frameIndex + 1],
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT);
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1,
                         &fftToCopyBarrier, 0, nullptr);
}

static void ProcessImGuiFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
        ImGui::Begin("Ocean parameters");

        ImGui::SliderFloat("Fetch", &sUniformData.fetchSpeedDirSpread.x, 1.0f,
                           1000000.0f, "%.1f");
        ImGui::SliderFloat("Wind Speed", &sUniformData.fetchSpeedDirSpread.y,
                           0.001f, 30.0f, "%.6f");
        ImGui::SliderFloat("Theta 0", &sUniformData.fetchSpeedDirSpread.z,
                           -FLY_MATH_PI, FLY_MATH_PI, "%.2f rad");
        ImGui::SliderFloat("Spread", &sUniformData.fetchSpeedDirSpread.w, 1.0f,
                           30.0f, "%.2f");
        ImGui::SliderFloat("Scale", &sUniformData.domainTimeLambdaScale.w, 1.0f,
                           50.0f, "%.2f");
        ImGui::SliderFloat("Displacement multiplier",
                           &sUniformData.domainTimeLambdaScale.z, -2.0f, 2.0f,
                           "%.01f");

        ImGui::End();
    }

    ImGui::Render();
}

static void CopyPass(RHI::Device& device)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    RHI::BindComputePipeline(device, cmd, sCopyPipeline);
    u32 pushConstants[] = {
        sOceanFrequencyBuffers[2 * device.frameIndex + 1].bindlessHandle,
        sDiffDisplacementMaps[device.frameIndex].bindlessStorageHandle,
        sHeightMaps[device.frameIndex].bindlessStorageHandle};
    RHI::SetPushConstants(device, cmd, pushConstants, sizeof(pushConstants));
    vkCmdDispatch(cmd.handle, 256, 1, 1);

    VkImageMemoryBarrier barriers[2];
    barriers[0] = {};
    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = sHeightMaps[device.frameIndex].image;
    barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[0].subresourceRange.baseMipLevel = 0;
    barriers[0].subresourceRange.levelCount = 1;
    barriers[0].subresourceRange.baseArrayLayer = 0;
    barriers[0].subresourceRange.layerCount = 1;
    barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    barriers[1] = {};
    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = sDiffDisplacementMaps[device.frameIndex].image;
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[1].subresourceRange.baseMipLevel = 0;
    barriers[1].subresourceRange.levelCount = 1;
    barriers[1].subresourceRange.baseArrayLayer = 0;
    barriers[1].subresourceRange.layerCount = 1;
    barriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, STACK_ARRAY_COUNT(barriers), barriers);
}

static void SkyBoxPass(RHI::Device& device)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    RecordTransitionImageLayout(cmd, sSkyBoxes[device.frameIndex].image,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRect2D renderArea = {{0, 0}, {256, 256}};
    VkRenderingAttachmentInfo colorAttachment =
        RHI::ColorAttachmentInfo(sSkyBoxes[device.frameIndex].arrayImageView,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = RHI::RenderingInfo(
        renderArea, &colorAttachment, 1, nullptr, nullptr, 6, 0x0000007F);
    vkCmdBeginRendering(cmd.handle, &renderInfo);
    RHI::BindGraphicsPipeline(device, cmd, sSkyBoxPipeline);

    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<f32>(256);
    viewport.height = static_cast<f32>(256);
    vkCmdSetViewport(cmd.handle, 0, 1, &viewport);

    VkRect2D scissor = renderArea;
    vkCmdSetScissor(cmd.handle, 0, 1, &scissor);

    vkCmdDraw(cmd.handle, 6, 1, 0, 0);
    vkCmdEndRendering(cmd.handle);

    RecordTransitionImageLayout(cmd, sSkyBoxes[device.frameIndex].image,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    // VkImageMemoryBarrier imageBarrier{};
    // imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // imageBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    // imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    // imageBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    // imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    // imageBarrier.image = cubemap.image;
    // imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    // imageBarrier.subresourceRange.baseMipLevel = 0;
    // imageBarrier.subresourceRange.levelCount = 1; // or all mips if needed
    // imageBarrier.subresourceRange.baseArrayLayer = 0;
    // imageBarrier.subresourceRange.layerCount = 6; // all cubemap faces

    // vkCmdPipelineBarrier(
    //     commandBuffer,
    //     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStage
    //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,         // dstStage
    //     0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
}

static void GraphicsPass(RHI::Device& device)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    const RHI::SwapchainTexture& swapchainTexture =
        RenderFrameSwapchainTexture(device);
    VkRect2D renderArea = {{0, 0},
                           {swapchainTexture.width, swapchainTexture.height}};
    VkRenderingAttachmentInfo colorAttachment = RHI::ColorAttachmentInfo(
        swapchainTexture.imageView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = RHI::DepthAttachmentInfo(
        device.depthTexture.imageView,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo =
        RHI::RenderingInfo(renderArea, &colorAttachment, 1, &depthAttachment);

    vkCmdBeginRendering(cmd.handle, &renderInfo);

    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<f32>(renderArea.extent.width);
    viewport.height = static_cast<f32>(renderArea.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd.handle, 0, 1, &viewport);

    VkRect2D scissor = renderArea;
    vkCmdSetScissor(cmd.handle, 0, 1, &scissor);

    // Sky
    {
        RHI::BindGraphicsPipeline(device, cmd, sSkyPipeline);
        u32 pushConstants[] = {
            sUniformBuffers[device.frameIndex].bindlessHandle,
            sSkyBoxes[device.frameIndex].bindlessHandle};
        RHI::SetPushConstants(device, cmd, pushConstants,
                              sizeof(pushConstants));
        vkCmdDraw(cmd.handle, 6, 1, 0, 0);
    }

    // Ocean
    {
        RHI::BindGraphicsPipeline(device, cmd, *sCurrentGraphicsPipeline);
        u32 pushConstants[] = {
            sUniformBuffers[device.frameIndex].bindlessHandle,
            sDiffDisplacementMaps[device.frameIndex].bindlessHandle,
            sHeightMaps[device.frameIndex].bindlessHandle,
            sVertexBuffer.bindlessHandle,
            sSkyBoxes[device.frameIndex].bindlessHandle};
        RHI::SetPushConstants(device, cmd, pushConstants,
                              sizeof(pushConstants));

        vkCmdBindIndexBuffer(cmd.handle, sIndexBuffer.handle, 0,
                             VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd.handle, sIndexCount, 1, 0, 0, 0);
    }

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd.handle);

    vkCmdEndRendering(cmd.handle);
}

static void ExecuteCommands(RHI::Device& device)
{
    SkyBoxPass(device);
    JonswapPass(device);
    IFFTPass(device);
    CopyPass(device);
    GraphicsPass(device);
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
        if (sCurrentGraphicsPipeline == &sGraphicsPipeline)
        {
            sCurrentGraphicsPipeline = &sWireframeGraphicsPipeline;
        }
        else
        {
            sCurrentGraphicsPipeline = &sGraphicsPipeline;
        }
    }
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
    const char* requiredDeviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_EXTENSION_NAME};

    VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT
        unusedAttachmentsFeature{};
    unusedAttachmentsFeature.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT;
    unusedAttachmentsFeature.dynamicRenderingUnusedAttachments = VK_TRUE;

    VkPhysicalDeviceMultiviewFeatures multiviewFeatures = {};
    multiviewFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
    multiviewFeatures.multiview = VK_TRUE;
    multiviewFeatures.pNext = &unusedAttachmentsFeature;

    RHI::ContextSettings settings{};
    settings.isPhysicalDeviceSuitableCallback = IsPhysicalDeviceSuitable;
    settings.instanceExtensions =
        glfwGetRequiredInstanceExtensions(&settings.instanceExtensionCount);
    settings.deviceExtensions = requiredDeviceExtensions;
    settings.deviceExtensionCount = STACK_ARRAY_COUNT(requiredDeviceExtensions);
    settings.windowPtr = window;
    settings.deviceFeatures2.features.fillModeNonSolid = true;
    settings.deviceFeatures2.pNext = &multiviewFeatures;

    RHI::Context context;
    if (!RHI::CreateContext(settings, context))
    {
        FLY_ERROR("Failed to create context");
        return -1;
    }
    RHI::Device& device = context.devices[0];
    glfwSetWindowUserPointer(window, &device);

    if (!CreatePipelines(device))
    {
        FLY_ERROR("Failed to create pipelines");
        return -1;
    }
    sCurrentGraphicsPipeline = &sGraphicsPipeline;

    if (!CreateDeviceObjects(device))
    {
        FLY_ERROR("Failed to create device buffers");
        return -1;
    }

    if (!CreateImGuiContext(context, device, window))
    {
        FLY_ERROR("Failed to create imgui context");
        return -1;
    }

    u64 previousFrameTime = 0;
    u64 loopStartTime = Fly::ClockNow();
    u64 currentFrameTime = loopStartTime;

    sUniformData.fetchSpeedDirSpread =
        Math::Vec4(500 * 1000.0f, 2.0f, 0.5f, 10.0f);
    sUniformData.domainTimeLambdaScale =
        Math::Vec4(static_cast<f32>(sTileSize), 0.0f, 0.4f, 4.0f);
    sUniformData.projection = sCamera.GetProjection();
    sUniformData.screenSize = Math::Vec4(
        WINDOW_WIDTH, WINDOW_HEIGHT, 1.0f / WINDOW_WIDTH, 1.0f / WINDOW_HEIGHT);

    sCamera.speed = 25.0f;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        previousFrameTime = currentFrameTime;
        currentFrameTime = Fly::ClockNow();
        f64 deltaTime = Fly::ToSeconds(currentFrameTime - previousFrameTime);

        ImGuiIO& io = ImGui::GetIO();
        bool wantMouse = io.WantCaptureMouse;
        bool wantKeyboard = io.WantCaptureKeyboard;
        if (!wantMouse && !wantKeyboard)
        {
            sCamera.Update(window, deltaTime);
        }
        sUniformData.view = sCamera.GetView();
        sUniformData.domainTimeLambdaScale.y =
            static_cast<f32>(Fly::ToSeconds(currentFrameTime - loopStartTime));

        ProcessImGuiFrame();
        RHI::CopyDataToBuffer(device, &sUniformData, sizeof(UniformData), 0,
                              sUniformBuffers[device.frameIndex]);

        RHI::BeginRenderFrame(device);
        ExecuteCommands(device);
        RHI::EndRenderFrame(device);
    }

    RHI::WaitAllDevicesIdle(context);

    DestroyPipelines(device);
    DestroyDeviceObjects(device);
    DestroyImGuiContext(device);
    RHI::DestroyContext(context);
    glfwDestroyWindow(window);
    glfwTerminate();

    FLY_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
