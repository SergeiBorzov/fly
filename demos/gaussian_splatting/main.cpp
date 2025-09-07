#include "core/clock.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "math/quat.h"

#include "rhi/buffer.h"
#include "rhi/context.h"
#include "rhi/frame_graph.h"
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

static RHI::Buffer sSplatBuffer;
static RHI::Buffer sSortedSplatBuffer;

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

struct BufferState
{
    RHI::Device* device;
    RHI::FrameGraph::BufferHandle pingPongKeys[2];
    RHI::FrameGraph::BufferHandle uniformBuffer;
    RHI::FrameGraph::BufferHandle indirectCount;
    RHI::FrameGraph::BufferHandle indirectDraws;
    RHI::FrameGraph::BufferHandle indirectDispatch;
    RHI::FrameGraph::BufferHandle tileHistograms;
    RHI::FrameGraph::BufferHandle globalHistograms;
    RHI::FrameGraph::BufferHandle splatBuffer;
    RHI::FrameGraph::BufferHandle sortedSplatBuffer;
};

struct UserData
{
    u32 passIndex;
    BufferState* bufferState;
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
    const char* computeShaderPaths[] = {
        "cull.comp.spv",       "write_indirect_dispatch.comp.spv",
        "copy.comp.spv",       "radix_count_histogram.comp.spv",
        "radix_scan.comp.spv", "radix_sort.comp.spv",
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
    if (!Fly::LoadShaderFromSpv(device, "splat.vert.spv",
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }
    if (!Fly::LoadShaderFromSpv(device, "splat.frag.spv",
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
    RHI::DestroyBuffer(device, sSplatBuffer);
    RHI::DestroyBuffer(device, sSortedSplatBuffer);
}

struct CullPassContext
{
    RHI::FrameGraph::BufferHandle pingPongKeys[2];
    RHI::FrameGraph::BufferHandle splatBuffer;
    RHI::FrameGraph::BufferHandle indirectCount;
    RHI::FrameGraph::BufferHandle uniformBuffer;
};

static void CullPassBuild(Arena& arena, RHI::FrameGraph::Builder& builder,
                          CullPassContext& context, void* pBufferState)
{
    BufferState* bufferState = static_cast<BufferState*>(pBufferState);

    u32 tileCount = static_cast<u32>(
        Math::Ceil(static_cast<f32>(sSplatCount) / COUNT_TILE_SIZE));

    context.splatBuffer = builder.RegisterExternalBuffer(arena, sSplatBuffer);

    for (u32 i = 0; i < 2; i++)
    {
        context.pingPongKeys[i] =
            builder.CreateBuffer(arena,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 false, nullptr, 2 * sizeof(u32) * sSplatCount);
    }
    context.uniformBuffer = builder.CreateBuffer(
        arena,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        true, nullptr, sizeof(UniformData));

    context.indirectCount =
        builder.CreateBuffer(arena,
                             VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                             false, nullptr, sizeof(u32));

    builder.Read(arena, context.splatBuffer);
    builder.Read(arena, context.uniformBuffer);
    bufferState->pingPongKeys[0] =
        builder.Write(arena, context.pingPongKeys[0]);
    bufferState->pingPongKeys[1] = context.pingPongKeys[1];
    bufferState->indirectCount =
        builder.ReadWrite(arena, context.indirectCount);
    bufferState->uniformBuffer = context.uniformBuffer;
    bufferState->tileHistograms = builder.CreateBuffer(
        arena,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        false, nullptr, sizeof(u32) * RADIX_HISTOGRAM_SIZE * tileCount);
    bufferState->globalHistograms = builder.CreateBuffer(
        arena,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        false, nullptr, sizeof(u32) * RADIX_HISTOGRAM_SIZE * RADIX_PASS_COUNT);
    bufferState->splatBuffer = context.splatBuffer;
}
static void CullPassExecute(RHI::CommandBuffer& cmd,
                            RHI::FrameGraph::ResourceMap& resources,
                            const CullPassContext& context, void* pBufferState)
{
    u32 workGroupCount = static_cast<u32>(
        Math::Ceil(static_cast<f32>(sSplatCount) / COUNT_TILE_SIZE));
    RHI::BindComputePipeline(cmd, sCullPipeline);
    u32 pushConstants[] = {
        resources.GetBuffer(context.uniformBuffer).bindlessHandle,
        resources.GetBuffer(context.splatBuffer).bindlessHandle,
        resources.GetBuffer(context.pingPongKeys[RADIX_PASS_COUNT % 2])
            .bindlessHandle,
        resources.GetBuffer(context.indirectCount).bindlessHandle, sSplatCount};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, workGroupCount, 1, 1);
}

struct WriteIndirectPassContext
{
    RHI::FrameGraph::BufferHandle indirectCount;
    RHI::FrameGraph::BufferHandle indirectDispatch;
    RHI::FrameGraph::BufferHandle indirectDraws;
};
static void WriteIndirectPassBuild(Arena& arena,
                                   RHI::FrameGraph::Builder& builder,
                                   WriteIndirectPassContext& context,
                                   void* pBufferState)
{
    BufferState* bufferState = static_cast<BufferState*>(pBufferState);

    context.indirectDispatch = builder.CreateBuffer(
        arena,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        false, nullptr, sizeof(VkDispatchIndirectCommand) * 2);
    context.indirectDraws =
        builder.CreateBuffer(arena,
                             VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                             false, nullptr, sizeof(VkDrawIndirectCommand));

    context.indirectCount = builder.Read(arena, bufferState->indirectCount);
    bufferState->indirectDispatch =
        builder.Write(arena, context.indirectDispatch);
    bufferState->indirectDraws = builder.Write(arena, context.indirectDraws);
}
static void WriteIndirectPassExecute(RHI::CommandBuffer& cmd,
                                     RHI::FrameGraph::ResourceMap& resources,
                                     const WriteIndirectPassContext& context,
                                     void* pBufferState)
{
    RHI::BindComputePipeline(cmd, sWriteIndirectDispatchPipeline);
    u32 pushConstants[] = {
        resources.GetBuffer(context.indirectCount).bindlessHandle,
        resources.GetBuffer(context.indirectDraws).bindlessHandle,
        resources.GetBuffer(context.indirectDispatch).bindlessHandle,
    };
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, 1, 1, 1);
}

struct CountHistogramsPassContext
{
    RHI::FrameGraph::BufferHandle keys;
    RHI::FrameGraph::BufferHandle tileHistograms;
    RHI::FrameGraph::BufferHandle globalHistograms;
    RHI::FrameGraph::BufferHandle indirectCount;
    RHI::FrameGraph::BufferHandle indirectDispatch;
};
static void CountHistogramsPassBuild(Arena& arena,
                                     RHI::FrameGraph::Builder& builder,
                                     CountHistogramsPassContext& context,
                                     void* pUserData)
{
    UserData* userData = static_cast<UserData*>(pUserData);
    BufferState* bufferState = userData->bufferState;
    u32 inKeysIndex = userData->passIndex % 2;

    context.keys = builder.Read(arena, bufferState->pingPongKeys[inKeysIndex]);
    context.indirectCount = builder.Read(arena, bufferState->indirectCount);
    context.indirectDispatch =
        builder.Read(arena, bufferState->indirectDispatch);
    context.tileHistograms = builder.Write(arena, bufferState->tileHistograms);
    context.globalHistograms =
        builder.ReadWrite(arena, bufferState->globalHistograms);

    bufferState->tileHistograms = context.tileHistograms;
    bufferState->globalHistograms = context.globalHistograms;
}
static void CountHistogramsPassExecute(
    RHI::CommandBuffer& cmd, RHI::FrameGraph::ResourceMap& resources,
    const CountHistogramsPassContext& context, void* pUserData)
{
    UserData* userData = static_cast<UserData*>(pUserData);

    RHI::BindComputePipeline(cmd, sCountPipeline);
    u32 pushConstants[] = {
        userData->passIndex,
        resources.GetBuffer(context.indirectCount).bindlessHandle,
        resources.GetBuffer(context.keys).bindlessHandle,
        resources.GetBuffer(context.tileHistograms).bindlessHandle,
        resources.GetBuffer(context.globalHistograms).bindlessHandle};

    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::DispatchIndirect(cmd, resources.GetBuffer(context.indirectDispatch),
                          0);
}

struct ScanPassContext
{
    RHI::FrameGraph::BufferHandle indirectCount;
    RHI::FrameGraph::BufferHandle indirectDispatch;
    RHI::FrameGraph::BufferHandle globalHistograms;
    RHI::FrameGraph::BufferHandle tileHistograms;
};
static void ScanPassBuild(Arena& arena, RHI::FrameGraph::Builder& builder,
                          ScanPassContext& context, void* pUserData)
{
    BufferState* bufferState = static_cast<UserData*>(pUserData)->bufferState;
    context.indirectCount = builder.Read(arena, bufferState->indirectCount);
    context.indirectDispatch =
        builder.Read(arena, bufferState->indirectDispatch);
    context.globalHistograms =
        builder.Read(arena, bufferState->globalHistograms);
    context.tileHistograms =
        builder.ReadWrite(arena, bufferState->tileHistograms);
    bufferState->tileHistograms = context.tileHistograms;
}
static void ScanPassExecute(RHI::CommandBuffer& cmd,
                            RHI::FrameGraph::ResourceMap& resources,
                            const ScanPassContext& context, void* pUserData)
{
    UserData* userData = static_cast<UserData*>(pUserData);
    // Exclusive prefix sums - global offsets
    RHI::BindComputePipeline(cmd, sScanPipeline);
    u32 pushConstants[] = {
        userData->passIndex,
        resources.GetBuffer(context.indirectCount).bindlessHandle,
        resources.GetBuffer(context.globalHistograms).bindlessHandle,
        resources.GetBuffer(context.tileHistograms).bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::DispatchIndirect(cmd, resources.GetBuffer(context.indirectDispatch),
                          sizeof(VkDispatchIndirectCommand));
}

struct SortPassContext
{
    RHI::FrameGraph::BufferHandle prefixSums;
    RHI::FrameGraph::BufferHandle keys;
    RHI::FrameGraph::BufferHandle sortedKeys;
    RHI::FrameGraph::BufferHandle indirectDispatch;
    RHI::FrameGraph::BufferHandle indirectCount;
};
static void SortPassBuild(Arena& arena, RHI::FrameGraph::Builder& builder,
                          SortPassContext& context, void* pUserData)
{
    UserData* userData = static_cast<UserData*>(pUserData);
    BufferState* bufferState = userData->bufferState;

    u32 inKeysIndex = userData->passIndex % 2;
    u32 outKeysIndex = (inKeysIndex + 1) % 2;

    context.indirectCount = builder.Read(arena, bufferState->indirectCount);
    context.indirectDispatch =
        builder.Read(arena, bufferState->indirectDispatch);
    context.prefixSums = builder.Read(arena, bufferState->tileHistograms);
    context.keys = builder.Read(arena, bufferState->pingPongKeys[inKeysIndex]);
    context.sortedKeys =
        builder.Write(arena, bufferState->pingPongKeys[outKeysIndex]);

    bufferState->pingPongKeys[outKeysIndex] = context.sortedKeys;
}
static void SortPassExecute(RHI::CommandBuffer& cmd,
                            RHI::FrameGraph::ResourceMap& resources,
                            const SortPassContext& context, void* pUserData)
{
    UserData* userData = static_cast<UserData*>(pUserData);
    RHI::BindComputePipeline(cmd, sSortPipeline);
    u32 pushConstants[] = {
        userData->passIndex,
        resources.GetBuffer(context.indirectCount).bindlessHandle,
        resources.GetBuffer(context.keys).bindlessHandle,
        resources.GetBuffer(context.prefixSums).bindlessHandle,
        resources.GetBuffer(context.sortedKeys).bindlessHandle,
    };
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::DispatchIndirect(cmd, resources.GetBuffer(context.indirectDispatch),
                          0);
}

struct CopyPassContext
{
    RHI::FrameGraph::BufferHandle splatBuffer;
    RHI::FrameGraph::BufferHandle sortedSplatBuffer;
    RHI::FrameGraph::BufferHandle sortedKeys;
    RHI::FrameGraph::BufferHandle indirectCount;
    RHI::FrameGraph::BufferHandle indirectDispatch;
};
static void CopyPassBuild(Arena& arena, RHI::FrameGraph::Builder& builder,
                          CopyPassContext& context, void* pBufferState)
{
    BufferState* bufferState = static_cast<BufferState*>(pBufferState);
    u32 sortedKeysIndex = RADIX_PASS_COUNT % 2;

    context.sortedSplatBuffer =
        builder.RegisterExternalBuffer(arena, sSortedSplatBuffer);
    bufferState->sortedSplatBuffer =
        builder.Write(arena, context.sortedSplatBuffer);
    context.splatBuffer = builder.Read(arena, bufferState->splatBuffer);
    context.sortedKeys =
        builder.Read(arena, bufferState->pingPongKeys[sortedKeysIndex]);
    context.indirectCount = builder.Read(arena, bufferState->indirectCount);
    context.indirectDispatch =
        builder.Read(arena, bufferState->indirectDispatch);
}
static void CopyPassExecute(RHI::CommandBuffer& cmd,
                            RHI::FrameGraph::ResourceMap& resources,
                            const CopyPassContext& context, void* pUserData)
{
    RHI::BindComputePipeline(cmd, sCopyPipeline);
    u32 pushConstants[] = {
        resources.GetBuffer(context.splatBuffer).bindlessHandle,
        resources.GetBuffer(context.sortedKeys).bindlessHandle,
        resources.GetBuffer(context.sortedSplatBuffer).bindlessHandle,
        resources.GetBuffer(context.indirectCount).bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::DispatchIndirect(cmd, resources.GetBuffer(context.indirectDispatch));
}

struct DrawPassContext
{
    RHI::FrameGraph::TextureHandle colorAttachment;
    RHI::FrameGraph::BufferHandle uniformBuffer;
    RHI::FrameGraph::BufferHandle sortedSplatBuffer;
    RHI::FrameGraph::BufferHandle indirectDraws;
    RHI::FrameGraph::BufferHandle indirectCount;
};
static void DrawPassBuild(Arena& arena, RHI::FrameGraph::Builder& builder,
                          DrawPassContext& context, void* pBufferState)
{
    BufferState* bufferState = static_cast<BufferState*>(pBufferState);
    context.colorAttachment = builder.ColorAttachment(
        arena, 0, RHI::FrameGraph::TextureHandle::sBackBuffer);
    context.uniformBuffer = builder.Read(arena, bufferState->uniformBuffer);
    context.sortedSplatBuffer =
        builder.Read(arena, bufferState->sortedSplatBuffer);
    context.indirectDraws = builder.Read(arena, bufferState->indirectDraws);
    context.indirectCount = builder.Read(arena, bufferState->indirectCount);
}
static void DrawPassExecute(RHI::CommandBuffer& cmd,
                            RHI::FrameGraph::ResourceMap& resources,
                            const DrawPassContext& context, void* pBufferState)
{
    BufferState* bufferState = static_cast<BufferState*>(pBufferState);
    RHI::BindGraphicsPipeline(cmd, sGraphicsPipeline);

    RHI::SetViewport(
        cmd, 0, 0, static_cast<f32>(bufferState->device->swapchainWidth),
        static_cast<f32>(bufferState->device->swapchainHeight), 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, bufferState->device->swapchainWidth,
                    bufferState->device->swapchainHeight);

    u32 pushConstants[] = {
        resources.GetBuffer(context.uniformBuffer).bindlessHandle,
        resources.GetBuffer(context.sortedSplatBuffer).bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::DrawIndirectCount(cmd, resources.GetBuffer(context.indirectDraws), 0,
                           resources.GetBuffer(context.indirectCount), 0, 1,
                           sizeof(VkDrawIndirectCommand));
}

static const char* splatScenes[] = {
    "garden.splat", "stump.splat", "bicycle.splat",
    "truck.splat",  "nike.splat",  "plush.splat",
    "room.splat",   "train.splat", "treehill.splat"};

static bool LoadNextScene(RHI::Device& device)
{
    static bool isFirstLoad = true;
    static u32 sceneIndex = 0;
    if (!isFirstLoad)
    {
        for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; ++i)
        {
            vkWaitForFences(device.logicalDevice, 1,
                            &device.frameData[i].renderFence, VK_TRUE,
                            UINT64_MAX);
        }
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

    if (!RHI::CreateStorageBuffer(device, false, vertices,
                                  sizeof(Vertex) * sSplatCount, sSplatBuffer))
    {
        ArenaPopToMarker(arena, marker);
        return false;
    }
    if (!RHI::CreateStorageBuffer(device, false, vertices,
                                  sizeof(Vertex) * sSplatCount,
                                  sSortedSplatBuffer))
    {
        ArenaPopToMarker(arena, marker);
        return false;
    }
    ArenaPopToMarker(arena, marker);
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

    if (!LoadNextScene(device))
    {
        FLY_ERROR("Failed to load scene");
        return -1;
    }

    BufferState bufferState;
    bufferState.device = &device;

    UserData userData[RADIX_PASS_COUNT];
    for (u32 i = 0; i < RADIX_PASS_COUNT; i++)
    {
        userData[i].passIndex = i;
        userData[i].bufferState = &bufferState;
    }

    Arena& arena = GetScratchArena();
    RHI::FrameGraph fg(device);
    fg.AddPass<CullPassContext>(arena, "ViewFrustumCull",
                                RHI::FrameGraph::PassType::Compute,
                                CullPassBuild, CullPassExecute, &bufferState);
    fg.AddPass<WriteIndirectPassContext>(
        arena, "WriteIndirect", RHI::FrameGraph::PassType::Compute,
        WriteIndirectPassBuild, WriteIndirectPassExecute, &bufferState);

    for (u32 i = 0; i < 4; i++)
    {
        fg.AddPass<CountHistogramsPassContext>(
            arena, "CountHistogram", RHI::FrameGraph::PassType::Compute,
            CountHistogramsPassBuild, CountHistogramsPassExecute, &userData[i]);
        fg.AddPass<ScanPassContext>(
            arena, "Scan", RHI::FrameGraph::PassType::Compute, ScanPassBuild,
            ScanPassExecute, &userData[i]);
        fg.AddPass<SortPassContext>(
            arena, "Sort", RHI::FrameGraph::PassType::Compute, SortPassBuild,
            SortPassExecute, &userData[i]);
    }
    fg.AddPass<CopyPassContext>(arena, "Copy",
                                RHI::FrameGraph::PassType::Compute,
                                CopyPassBuild, CopyPassExecute, &bufferState);
    fg.AddPass<DrawPassContext>(arena, "Draw",
                                RHI::FrameGraph::PassType::Graphics,
                                DrawPassBuild, DrawPassExecute, &bufferState);
    fg.Build(arena);

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

        RHI::Buffer& uniformBuffer = fg.GetBuffer(bufferState.uniformBuffer);
        RHI::CopyDataToBuffer(device, &uniformData, sizeof(UniformData), 0,
                              uniformBuffer);
        fg.Execute();
    }

    RHI::WaitAllDevicesIdle(context);

    fg.Destroy();
    DestroyPipelines(device);
    DestroyScene(device);
    RHI::DestroyContext(context);
    glfwDestroyWindow(window);
    glfwTerminate();

    FLY_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
