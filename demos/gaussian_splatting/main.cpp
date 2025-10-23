#include "core/clock.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "math/quat.h"

#include "rhi/buffer.h"
#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"

#include "utils/utils.h"

#include "demos/common/scene.h"
#include "demos/common/simple_camera_fps.h"

#include <GLFW/glfw3.h>

#include <stdlib.h>

using namespace Fly;

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define RADIX_PASS_COUNT 4
#define RADIX_BIT_COUNT 8
#define RADIX_HISTOGRAM_SIZE (1U << RADIX_BIT_COUNT)
#define COUNT_WORKGROUP_SIZE 512
#define COUNT_TILE_SIZE COUNT_WORKGROUP_SIZE
#define SCAN_WORKGROUP_SIZE (COUNT_WORKGROUP_SIZE / 2)

static SimpleCameraFPS sCamera(80.0f,
                               static_cast<f32>(WINDOW_WIDTH) / WINDOW_HEIGHT,
                               0.01f, 100.0f, Math::Vec3(0.0f, 0.0f, -5.0f));

static RHI::ComputePipeline sCullPipeline;
static RHI::ComputePipeline sWriteIndirectDispatchPipeline;
static RHI::ComputePipeline sCopyPipeline;
static RHI::ComputePipeline sCountPipeline;
static RHI::ComputePipeline sScanPipeline;
static RHI::ComputePipeline sSortPipeline;
static RHI::GraphicsPipeline sGraphicsPipeline;

static u32 sSplatCount;

static RHI::Buffer sUniformBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sKeys[2];
static RHI::Buffer sSplatBuffer;
static RHI::Buffer sSortedSplatBuffer;
static RHI::Buffer sTileHistograms;
static RHI::Buffer sGlobalHistograms;
static RHI::Buffer sIndirectCount;
static RHI::Buffer sIndirectDispatch;
static RHI::Buffer sIndirectDraw;

#pragma pack(push, 1)
struct Splat
{
    Math::Vec3 position;
    Math::Vec3 scale;
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    u8 w;
    u8 x;
    u8 y;
    u8 z;
};
#pragma pack(pop)

struct Vertex
{
    Math::Quat rotation;
    Math::Vec3 position;
    f32 r;
    Math::Vec3 scale;
    f32 g;
    f32 b;
    f32 a;
    f32 pad[2];
};

struct UniformData
{
    Math::Mat4 projection;       // 0
    Math::Mat4 view;             // 64
    Math::Vec4 viewport;         // 128
    Math::Vec4 cameraParameters; // 144
    Math::Vec4 time;             // 160
};

struct RadixSortData
{
    u32 passIndex;
};

static bool IsPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice,
                                     const RHI::PhysicalDeviceInfo& info)
{
    return info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

static bool DeterminePresentMode(const RHI::Context& context,
                                 const RHI::PhysicalDeviceInfo& physicalDevice,
                                 VkPresentModeKHR& presentMode)
{
    presentMode = VK_PRESENT_MODE_FIFO_KHR;
    return true;
}

static void ErrorCallbackGLFW(i32 error, const char* description)
{
    FLY_ERROR("GLFW - error: %s", description);
}

static bool CreatePipelines(RHI::Device& device)
{
    RHI::ComputePipeline* computePipelines[] = {
        &sCullPipeline, &sWriteIndirectDispatchPipeline,
        &sCopyPipeline, &sCountPipeline,
        &sScanPipeline, &sSortPipeline,
    };
    String8 computeShaderPaths[] = {
        FLY_STRING8_LITERAL("cull.comp.spv"),
        FLY_STRING8_LITERAL("write_indirect_dispatch.comp.spv"),
        FLY_STRING8_LITERAL("copy.comp.spv"),
        FLY_STRING8_LITERAL("radix_count_histogram.comp.spv"),
        FLY_STRING8_LITERAL("radix_scan.comp.spv"),
        FLY_STRING8_LITERAL("radix_sort.comp.spv"),
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
    fixedState.colorBlendState.attachments[0].blendEnable = true;
    fixedState.colorBlendState.attachments[0].srcColorBlendFactor =
        VK_BLEND_FACTOR_SRC_ALPHA;
    fixedState.colorBlendState.attachments[0].dstColorBlendFactor =
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    fixedState.colorBlendState.attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    fixedState.colorBlendState.attachments[0].srcAlphaBlendFactor =
        VK_BLEND_FACTOR_ZERO;
    fixedState.colorBlendState.attachments[0].dstAlphaBlendFactor =
        VK_BLEND_FACTOR_ONE;
    fixedState.colorBlendState.attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    fixedState.depthStencilState.depthTestEnable = false;
    fixedState.inputAssemblyState.topology =
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    RHI::ShaderProgram shaderProgram{};
    if (!Fly::LoadShaderFromSpv(device, FLY_STRING8_LITERAL("splat.vert.spv"),
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }
    if (!Fly::LoadShaderFromSpv(device, FLY_STRING8_LITERAL("splat.frag.spv"),
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

    return true;
}

static void DestroyPipelines(RHI::Device& device)
{
    RHI::DestroyGraphicsPipeline(device, sGraphicsPipeline);
    RHI::DestroyComputePipeline(device, sScanPipeline);
    RHI::DestroyComputePipeline(device, sCountPipeline);
    RHI::DestroyComputePipeline(device, sSortPipeline);
    RHI::DestroyComputePipeline(device, sCopyPipeline);
    RHI::DestroyComputePipeline(device, sWriteIndirectDispatchPipeline);
    RHI::DestroyComputePipeline(device, sCullPipeline);
}

static void DestroyScene(RHI::Device& device)
{
    RHI::DestroyBuffer(device, sTileHistograms);
    RHI::DestroyBuffer(device, sGlobalHistograms);
    for (u32 i = 0; i < 2; i++)
    {
        RHI::DestroyBuffer(device, sKeys[i]);
    }
    RHI::DestroyBuffer(device, sSortedSplatBuffer);
    RHI::DestroyBuffer(device, sSplatBuffer);
}

static bool CreateResources(RHI::Device& device)
{
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
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                           nullptr, sizeof(u32), sIndirectCount))
    {
        return false;
    }

    if (!RHI::CreateBuffer(device, false,
                           VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                           nullptr, sizeof(VkDispatchIndirectCommand) * 2,
                           sIndirectDispatch))
    {
        return false;
    }

    if (!RHI::CreateBuffer(device, false,
                           VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                           nullptr, sizeof(VkDrawIndirectCommand),
                           sIndirectDraw))
    {
        return false;
    }

    return true;
}

static void DestroyResources(RHI::Device& device)
{
    RHI::DestroyBuffer(device, sIndirectDraw);
    RHI::DestroyBuffer(device, sIndirectDispatch);
    RHI::DestroyBuffer(device, sIndirectCount);
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        RHI::DestroyBuffer(device, sUniformBuffers[i]);
    }
}

static void RecordFrustumCull(RHI::CommandBuffer& cmd,
                              const RHI::RecordBufferInput* bufferInput,
                              const RHI::RecordTextureInput* textureInput,
                              void* pUserData)
{
    u32 workGroupCount = static_cast<u32>(
        Math::Ceil(static_cast<f32>(sSplatCount) / COUNT_TILE_SIZE));
    RHI::BindComputePipeline(cmd, sCullPipeline);

    RHI::Buffer& uniformBuffer = *(bufferInput->buffers[0]);
    RHI::Buffer& keys = *(bufferInput->buffers[1]);
    RHI::Buffer& indirectCount = *(bufferInput->buffers[2]);
    u32 pushConstants[] = {uniformBuffer.bindlessHandle,
                           sSplatBuffer.bindlessHandle, keys.bindlessHandle,
                           indirectCount.bindlessHandle, sSplatCount};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, workGroupCount, 1, 1);
}

static void RecordWriteIndirect(RHI::CommandBuffer& cmd,
                                const RHI::RecordBufferInput* bufferInput,
                                const RHI::RecordTextureInput* textureInput,
                                void* pBufferState)
{
    RHI::Buffer& indirectCount = *(bufferInput->buffers[0]);
    RHI::Buffer& indirectDraw = *(bufferInput->buffers[1]);
    RHI::Buffer& indirectDispatch = *(bufferInput->buffers[2]);

    RHI::BindComputePipeline(cmd, sWriteIndirectDispatchPipeline);
    u32 pushConstants[] = {indirectCount.bindlessHandle,
                           indirectDraw.bindlessHandle,
                           indirectDispatch.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, 1, 1, 1);
}

static void RecordCountHistograms(RHI::CommandBuffer& cmd,
                                  const RHI::RecordBufferInput* bufferInput,
                                  const RHI::RecordTextureInput* textureInput,
                                  void* pUserData)
{
    RadixSortData& sortData = *(static_cast<RadixSortData*>(pUserData));

    RHI::Buffer& indirectCount = *(bufferInput->buffers[0]);
    RHI::Buffer& keys = *(bufferInput->buffers[1]);
    RHI::Buffer& tileHistograms = *(bufferInput->buffers[2]);
    RHI::Buffer& globalHistograms = *(bufferInput->buffers[3]);
    RHI::Buffer& indirectDispatch = *(bufferInput->buffers[4]);

    RHI::BindComputePipeline(cmd, sCountPipeline);
    u32 pushConstants[] = {sortData.passIndex, indirectCount.bindlessHandle,
                           keys.bindlessHandle, tileHistograms.bindlessHandle,
                           globalHistograms.bindlessHandle};

    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::DispatchIndirect(cmd, indirectDispatch, 0);
}

static void RecordScan(RHI::CommandBuffer& cmd,
                       const RHI::RecordBufferInput* bufferInput,
                       const RHI::RecordTextureInput* textureInput,
                       void* pUserData)
{
    RadixSortData& sortData = *(static_cast<RadixSortData*>(pUserData));

    RHI::Buffer& indirectCount = *(bufferInput->buffers[0]);
    RHI::Buffer& globalHistograms = *(bufferInput->buffers[1]);
    RHI::Buffer& tileHistograms = *(bufferInput->buffers[2]);
    RHI::Buffer& indirectDispatch = *(bufferInput->buffers[3]);

    // Exclusive prefix sums - global offsets
    RHI::BindComputePipeline(cmd, sScanPipeline);
    u32 pushConstants[] = {sortData.passIndex, indirectCount.bindlessHandle,
                           globalHistograms.bindlessHandle,
                           tileHistograms.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::DispatchIndirect(cmd, indirectDispatch,
                          sizeof(VkDispatchIndirectCommand));
}

static void RecordSort(RHI::CommandBuffer& cmd,
                       const RHI::RecordBufferInput* bufferInput,
                       const RHI::RecordTextureInput* textureInput,
                       void* pUserData)
{

    RadixSortData& sortData = *(static_cast<RadixSortData*>(pUserData));

    RHI::Buffer& indirectCount = *(bufferInput->buffers[0]);
    RHI::Buffer& keys = *(bufferInput->buffers[1]);
    RHI::Buffer& prefixSums = *(bufferInput->buffers[2]);
    RHI::Buffer& sortedKeys = *(bufferInput->buffers[3]);
    RHI::Buffer& indirectDispatch = *(bufferInput->buffers[4]);

    RHI::BindComputePipeline(cmd, sSortPipeline);
    u32 pushConstants[] = {sortData.passIndex, indirectCount.bindlessHandle,
                           keys.bindlessHandle, prefixSums.bindlessHandle,
                           sortedKeys.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::DispatchIndirect(cmd, indirectDispatch, 0);
}

static void RecordCopy(RHI::CommandBuffer& cmd,
                       const RHI::RecordBufferInput* bufferInput,
                       const RHI::RecordTextureInput* textureInput,
                       void* pUserData)
{
    RHI::Buffer& indirectCount = *(bufferInput->buffers[0]);
    RHI::Buffer& sortedKeys = *(bufferInput->buffers[1]);
    RHI::Buffer& sortedSplats = *(bufferInput->buffers[2]);
    RHI::Buffer& indirectDispatch = *(bufferInput->buffers[3]);

    RHI::BindComputePipeline(cmd, sCopyPipeline);
    u32 pushConstants[] = {
        sSplatBuffer.bindlessHandle, sortedKeys.bindlessHandle,
        sortedSplats.bindlessHandle, indirectCount.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::DispatchIndirect(cmd, indirectDispatch);
}

static void RecordDraw(RHI::CommandBuffer& cmd,
                       const RHI::RecordBufferInput* bufferInput,
                       const RHI::RecordTextureInput* textureInput,
                       void* pBufferState)
{
    RHI::Buffer& uniformBuffer = *(bufferInput->buffers[0]);
    RHI::Buffer& sortedSplats = *(bufferInput->buffers[1]);
    RHI::Buffer& indirectDraws = *(bufferInput->buffers[2]);
    RHI::Buffer& indirectCount = *(bufferInput->buffers[3]);

    RHI::BindGraphicsPipeline(cmd, sGraphicsPipeline);
    RHI::SetViewport(cmd, 0, 0, static_cast<f32>(cmd.device->swapchainWidth),
                     static_cast<f32>(cmd.device->swapchainHeight), 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, cmd.device->swapchainWidth,
                    cmd.device->swapchainHeight);

    u32 pushConstants[] = {uniformBuffer.bindlessHandle,
                           sortedSplats.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::DrawIndirectCount(cmd, indirectDraws, 0, indirectCount, 0, 1,
                           sizeof(VkDrawIndirectCommand));
}

static void DrawSplats(RHI::Device& device)
{
    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    RadixSortData sortData;
    RHI::RecordBufferInput bufferInput;
    RHI::Buffer** buffers = FLY_PUSH_ARENA(arena, RHI::Buffer*, 5);
    VkAccessFlagBits2* bufferAccesses =
        FLY_PUSH_ARENA(arena, VkAccessFlagBits2, 5);
    bufferInput.buffers = buffers;
    bufferInput.bufferAccesses = bufferAccesses;

    {
        bufferInput.bufferCount = 3;
        buffers[0] = &sUniformBuffers[device.frameIndex];
        bufferAccesses[0] = VK_ACCESS_2_SHADER_READ_BIT;
        buffers[1] = &sKeys[0];
        bufferAccesses[1] = VK_ACCESS_2_SHADER_WRITE_BIT;
        buffers[2] = &sIndirectCount;
        bufferAccesses[2] = VK_ACCESS_2_SHADER_WRITE_BIT;
        RHI::ExecuteCompute(RenderFrameCommandBuffer(device), RecordFrustumCull,
                            &bufferInput);
    }

    {
        bufferInput.bufferCount = 3;
        buffers[0] = &sIndirectCount;
        bufferAccesses[0] = VK_ACCESS_2_SHADER_READ_BIT;
        buffers[1] = &sIndirectDraw;
        bufferAccesses[1] = VK_ACCESS_2_SHADER_WRITE_BIT;
        buffers[2] = &sIndirectDispatch;
        bufferAccesses[2] = VK_ACCESS_2_SHADER_WRITE_BIT;
        RHI::ExecuteCompute(RenderFrameCommandBuffer(device),
                            RecordWriteIndirect, &bufferInput);
    }

    for (u32 i = 0; i < RADIX_PASS_COUNT; i++)
    {
        sortData.passIndex = i;
        {
            bufferInput.bufferCount = 5;
            buffers[0] = &sIndirectCount;
            bufferAccesses[0] = VK_ACCESS_2_SHADER_READ_BIT;
            buffers[1] = &sKeys[i % 2];
            bufferAccesses[1] = VK_ACCESS_2_SHADER_READ_BIT;
            buffers[2] = &sTileHistograms;
            bufferAccesses[2] = VK_ACCESS_2_SHADER_WRITE_BIT;
            buffers[3] = &sGlobalHistograms;
            bufferAccesses[3] =
                VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
            buffers[4] = &sIndirectDispatch;
            bufferAccesses[4] = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
            RHI::ExecuteComputeIndirect(RenderFrameCommandBuffer(device),
                                        RecordCountHistograms, &bufferInput,
                                        nullptr, &sortData);
        }
        {
            bufferInput.bufferCount = 4;
            buffers[0] = &sIndirectCount;
            bufferAccesses[0] = VK_ACCESS_2_SHADER_READ_BIT;
            buffers[1] = &sGlobalHistograms;
            bufferAccesses[1] = VK_ACCESS_2_SHADER_READ_BIT;
            buffers[2] = &sTileHistograms;
            bufferAccesses[2] =
                VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
            buffers[3] = &sIndirectDispatch;
            bufferAccesses[3] = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
            RHI::ExecuteComputeIndirect(RenderFrameCommandBuffer(device),
                                        RecordScan, &bufferInput, nullptr,
                                        &sortData);
        }
        {
            bufferInput.bufferCount = 5;
            buffers[0] = &sIndirectCount;
            bufferAccesses[0] = VK_ACCESS_2_SHADER_READ_BIT;
            buffers[1] = &sKeys[i % 2];
            bufferAccesses[1] = VK_ACCESS_2_SHADER_READ_BIT;
            buffers[2] = &sTileHistograms;
            bufferAccesses[2] = VK_ACCESS_2_SHADER_READ_BIT;
            buffers[3] = &sKeys[(i + 1) % 2];
            bufferAccesses[3] = VK_ACCESS_2_SHADER_WRITE_BIT;
            buffers[4] = &sIndirectDispatch;
            bufferAccesses[4] = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;

            RHI::ExecuteComputeIndirect(RenderFrameCommandBuffer(device),
                                        RecordSort, &bufferInput, nullptr,
                                        &sortData);
        }
    }

    {
        bufferInput.bufferCount = 4;
        buffers[0] = &sIndirectCount;
        bufferAccesses[0] = VK_ACCESS_2_SHADER_READ_BIT;
        buffers[1] = &sKeys[RADIX_PASS_COUNT % 2];
        bufferAccesses[1] = VK_ACCESS_2_SHADER_READ_BIT;
        buffers[2] = &sSortedSplatBuffer;
        bufferAccesses[2] = VK_ACCESS_2_SHADER_WRITE_BIT;
        buffers[3] = &sIndirectDispatch;
        bufferAccesses[3] = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
        RHI::ExecuteComputeIndirect(RenderFrameCommandBuffer(device),
                                    RecordCopy, &bufferInput);
    }
    {
        bufferInput.bufferCount = 4;
        buffers[0] = &sUniformBuffers[device.frameIndex];
        bufferAccesses[0] = VK_ACCESS_2_SHADER_READ_BIT;
        buffers[1] = &sSortedSplatBuffer;
        bufferAccesses[1] = VK_ACCESS_2_SHADER_READ_BIT;
        buffers[2] = &sIndirectDraw;
        bufferAccesses[2] = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
        buffers[3] = &sIndirectCount;
        bufferAccesses[3] = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;

        VkRenderingAttachmentInfo colorAttachment = RHI::ColorAttachmentInfo(
            RenderFrameSwapchainTexture(device).imageView);
        VkRenderingInfo renderingInfo = RHI::RenderingInfo(
            {{0, 0}, {device.swapchainWidth, device.swapchainHeight}},
            &colorAttachment, 1);
        RHI::ExecuteGraphics(RenderFrameCommandBuffer(device), renderingInfo,
                             RecordDraw, &bufferInput);
    }

    ArenaPopToMarker(arena, marker);
}

static String8 splatScenes[] = {
    FLY_STRING8_LITERAL("garden.splat"),  FLY_STRING8_LITERAL("stump.splat"),
    FLY_STRING8_LITERAL("bicycle.splat"), FLY_STRING8_LITERAL("truck.splat"),
    FLY_STRING8_LITERAL("nike.splat"),    FLY_STRING8_LITERAL("plush.splat"),
    FLY_STRING8_LITERAL("room.splat"),    FLY_STRING8_LITERAL("train.splat"),
    FLY_STRING8_LITERAL("treehill.splat")};

static bool LoadNextScene(RHI::Device& device)
{
    static bool isFirstLoad = true;
    static u32 sceneIndex = 0;
    if (!isFirstLoad)
    {
        vkQueueWaitIdle(device.graphicsComputeQueue);
        sceneIndex = (sceneIndex + 1) % STACK_ARRAY_COUNT(splatScenes);
        DestroyScene(device);
    }
    else
    {
        isFirstLoad = false;
    }

    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);
    String8 data = ReadFileToString(arena, splatScenes[sceneIndex]);
    if (!data)
    {
        FLY_ERROR("Failed to read splat file");
        return false;
    }
    sSplatCount = static_cast<u32>(data.Size() / sizeof(Splat));
    const Splat* splats = reinterpret_cast<const Splat*>(data.Data());
    FLY_ASSERT(sSplatCount);
    FLY_LOG("Splat count is %u", sSplatCount);

    Vertex* vertices = FLY_PUSH_ARENA(arena, Vertex, sSplatCount);
    for (u32 i = 0; i < sSplatCount; i++)
    {
        f32 x = (splats[i].x - 128.0f) / 128.0f;
        f32 y = (splats[i].y - 128.0f) / 128.0f;
        f32 z = (splats[i].z - 128.0f) / 128.0f;
        f32 w = (splats[i].w - 128.0f) / 128.0f;

        Math::Quat q = Math::Conjugate(Math::Normalize(Math::Quat(x, y, z, w)));
        Math::Quat r1 = Math::AngleAxis(Math::Vec3(0.0f, 0.0f, 1.0f), 180.0f);
        Math::Quat r2 = Math::AngleAxis(Math::Vec3(1.0f, 0.0f, 0.0f), 25.0f);
        vertices[i].rotation = q * r2 * r1;
        vertices[i].position =
            r2 * r1 *
            Math::Vec3(splats[i].position.x, splats[i].position.y,
                       splats[i].position.z);
        vertices[i].scale = splats[i].scale;
        vertices[i].r = splats[i].r / 255.0f;
        vertices[i].g = splats[i].g / 255.0f;
        vertices[i].b = splats[i].b / 255.0f;
        vertices[i].a = splats[i].a / 255.0f;
    }

    if (!RHI::CreateBuffer(device, false,
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           vertices, sizeof(Vertex) * sSplatCount,
                           sSplatBuffer))
    {
        ArenaPopToMarker(arena, marker);
        return false;
    }
    if (!RHI::CreateBuffer(device, false,
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           vertices, sizeof(Vertex) * sSplatCount,
                           sSortedSplatBuffer))
    {
        ArenaPopToMarker(arena, marker);
        return false;
    }
    ArenaPopToMarker(arena, marker);

    for (u32 i = 0; i < 2; i++)
    {
        if (!RHI::CreateBuffer(device, false,
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               nullptr, 2 * sizeof(u32) * sSplatCount,
                               sKeys[i]))
        {
            return false;
        }
    }

    if (!RHI::CreateBuffer(
            device, false,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            nullptr, sizeof(u32) * RADIX_HISTOGRAM_SIZE * RADIX_PASS_COUNT,
            sGlobalHistograms))
    {
        return false;
    }

    u32 tileCount = static_cast<u32>(
        Math::Ceil(static_cast<f32>(sSplatCount) / COUNT_TILE_SIZE));

    if (!RHI::CreateBuffer(device, false,
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           nullptr,
                           sizeof(u32) * RADIX_HISTOGRAM_SIZE * tileCount,
                           sTileHistograms))
    {
        return false;
    }

    return true;
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
        if (!LoadNextScene(*pDevice))
        {
            exit(-1);
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
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
                                          "Splat viewer", nullptr, nullptr);
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
    settings.determinePresentModeCallback = DeterminePresentMode;
    settings.isPhysicalDeviceSuitableCallback = IsPhysicalDeviceSuitable;
    settings.instanceExtensions =
        glfwGetRequiredInstanceExtensions(&settings.instanceExtensionCount);
    settings.deviceExtensions = requiredDeviceExtensions;
    settings.deviceExtensionCount = STACK_ARRAY_COUNT(requiredDeviceExtensions);
    settings.windowPtr = window;
    settings.vulkan12Features.drawIndirectCount = true;

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

    if (!CreateResources(device))
    {
        return -1;
    }

    if (!LoadNextScene(device))
    {
        FLY_ERROR("Failed to load scene");
        return -1;
    }

    u64 previousFrameTime = 0;
    u64 loopStartTime = Fly::ClockNow();
    u64 currentFrameTime = loopStartTime;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        previousFrameTime = currentFrameTime;
        currentFrameTime = Fly::ClockNow();
        f32 time =
            static_cast<f32>(Fly::ToSeconds(currentFrameTime - loopStartTime));
        f64 deltaTime = Fly::ToSeconds(currentFrameTime - previousFrameTime);

        sCamera.Update(window, deltaTime);

        UniformData uniformData;
        uniformData.projection = sCamera.GetProjection();
        uniformData.view = sCamera.GetView();
        uniformData.cameraParameters.x =
            Math::Tan(Math::Radians(sCamera.GetHorizontalFov()) * 0.5f);
        uniformData.cameraParameters.y =
            Math::Tan(Math::Radians(sCamera.GetVerticalFov()) * 0.5f);
        uniformData.cameraParameters.z = sCamera.GetNear();
        uniformData.cameraParameters.w = sCamera.GetFar();
        uniformData.viewport = Math::Vec4(
            static_cast<f32>(WINDOW_WIDTH), static_cast<f32>(WINDOW_HEIGHT),
            1.0f / WINDOW_WIDTH, 1.0f / WINDOW_HEIGHT);
        uniformData.time.x = time;

        RHI::Buffer& uniformBuffer = sUniformBuffers[device.frameIndex];
        RHI::CopyDataToBuffer(device, &uniformData, sizeof(UniformData), 0,
                              uniformBuffer);

        RHI::BeginRenderFrame(device);
        DrawSplats(device);
        RHI::EndRenderFrame(device);
    }

    RHI::WaitAllDevicesIdle(context);

    DestroyScene(device);
    DestroyResources(device);
    DestroyPipelines(device);
    RHI::DestroyContext(context);
    glfwDestroyWindow(window);
    glfwTerminate();

    FLY_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
