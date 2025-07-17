#include "core/clock.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "math/mat.h"

#include "rhi/allocation_callbacks.h"
#include "rhi/buffer.h"
#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"
#include "rhi/frame_graph.h"

#include "utils/utils.h"

#include <stdlib.h>

#define RADIX_PASS_COUNT 4
#define RADIX_BIT_COUNT 8
#define RADIX_HISTOGRAM_SIZE (1U << RADIX_BIT_COUNT)
#define COUNT_WORKGROUP_SIZE 512
#define COUNT_TILE_SIZE COUNT_WORKGROUP_SIZE
#define SCAN_WORKGROUP_SIZE (COUNT_WORKGROUP_SIZE / 2)

using namespace Fly;

static bool IsPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice,
                                     const RHI::PhysicalDeviceInfo& info)
{
    return info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

static RHI::ComputePipeline sCountPipeline;
static RHI::ComputePipeline sScanPipeline;
static RHI::ComputePipeline sSortPipeline;
static VkQueryPool sTimestampQueryPool;
static f32 sTimestampPeriod;

static bool CreateComputePipelines(RHI::Device& device)
{
    RHI::Shader countShader;
    if (!Fly::LoadShaderFromSpv(device, "radix_count_histogram.comp.spv",
                                countShader))
    {
        return false;
    }
    if (!RHI::CreateComputePipeline(device, countShader, sCountPipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, countShader);

    RHI::Shader scanShader;
    if (!Fly::LoadShaderFromSpv(device, "radix_scan.comp.spv", scanShader))
    {
        return false;
    }
    if (!RHI::CreateComputePipeline(device, scanShader, sScanPipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, scanShader);

    RHI::Shader sortShader;
    if (!Fly::LoadShaderFromSpv(device, "radix_sort.comp.spv", sortShader))
    {
        return false;
    }
    if (!RHI::CreateComputePipeline(device, sortShader, sSortPipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, sortShader);

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

static void DestroyComputePipelines(RHI::Device& device)
{
    vkDestroyQueryPool(device.logicalDevice, sTimestampQueryPool,
                       RHI::GetVulkanAllocationCallbacks());
    RHI::DestroyComputePipeline(device, sScanPipeline);
    RHI::DestroyComputePipeline(device, sCountPipeline);
    RHI::DestroyComputePipeline(device, sSortPipeline);
}

struct BufferState
{
    const u32* cpuKeys;
    RHI::FrameGraph::BufferHandle keys[2];
    RHI::FrameGraph::BufferHandle tileHistograms;
    RHI::FrameGraph::BufferHandle globalHistograms;
    u32 keyCount;
};

struct UserData
{
    BufferState* bufferState;
    u32 passIndex;
};

struct InitPassContext
{
    RHI::FrameGraph::BufferHandle globalHistograms;
};

static void InitPassBuild(
    Arena& arena, RHI::FrameGraph::Builder& builder,
    InitPassContext& context, void* pUserData)
{
    UserData& userData = *(static_cast<UserData*>(pUserData));
    u32 tileCount = static_cast<u32>(
        Math::Ceil(static_cast<f32>(userData.bufferState->keyCount) / COUNT_TILE_SIZE));
    
    context.globalHistograms = builder.CreateBuffer(arena, 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        false, nullptr, sizeof(u32) * RADIX_HISTOGRAM_SIZE * RADIX_PASS_COUNT);
    userData.bufferState->globalHistograms = builder.Write(arena, context.globalHistograms);
    
    userData.bufferState->tileHistograms = builder.CreateBuffer(arena,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        false, nullptr, sizeof(u32) * RADIX_HISTOGRAM_SIZE * tileCount);

    userData.bufferState->keys[1] = builder.CreateBuffer(arena,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        false, nullptr, sizeof(u32) * userData.bufferState->keyCount);
    userData.bufferState->keys[0] = builder.CreateBuffer(arena,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        false, userData.bufferState->cpuKeys, sizeof(u32) * userData.bufferState->keyCount);
}

static void InitPassExecute(
    RHI::CommandBuffer& cmd, RHI::FrameGraph::ResourceMap& resources,
    const InitPassContext& context, void* pUserData)
{
    RHI::FillBuffer(cmd, resources.GetBuffer(context.globalHistograms), 0);
}

struct CountHistogramsPassContext
{
    RHI::FrameGraph::BufferHandle keys;
    RHI::FrameGraph::BufferHandle tileHistograms;
    RHI::FrameGraph::BufferHandle globalHistograms;
};

static void CountHistogramsPassBuild(
    Arena& arena, RHI::FrameGraph::Builder& builder,
    CountHistogramsPassContext& context, void* pUserData)
{
    UserData& userData = *(static_cast<UserData*>(pUserData));
    u32 inKeysIndex = userData.passIndex % 2;

    context.keys = builder.Read(arena, userData.bufferState->keys[inKeysIndex]);
    context.tileHistograms = builder.Write(arena, userData.bufferState->tileHistograms);
    context.globalHistograms = builder.Write(arena, userData.bufferState->globalHistograms);

    userData.bufferState->tileHistograms = context.tileHistograms;
    userData.bufferState->globalHistograms = context.globalHistograms;
}

static void CountHistogramsPassExecute(
    RHI::CommandBuffer& cmd, RHI::FrameGraph::ResourceMap& resources,
    const CountHistogramsPassContext& context, void* pUserData)
{
    UserData& userData = *(static_cast<UserData*>(pUserData));
    u32 workGroupCount = static_cast<u32>(
        Math::Ceil(static_cast<f32>(userData.bufferState->keyCount) / COUNT_TILE_SIZE));

    RHI::BindComputePipeline(cmd, sCountPipeline);

    const RHI::Buffer& keys = resources.GetBuffer(context.keys);
    RHI::Buffer& tileHistograms = resources.GetBuffer(context.tileHistograms);
    RHI::Buffer& globalHistograms = resources.GetBuffer(context.globalHistograms);
    u32 pushConstants[] = {
        userData.passIndex, userData.bufferState->keyCount,
        keys.bindlessHandle, tileHistograms.bindlessHandle,
        globalHistograms.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));

    RHI::Dispatch(cmd, workGroupCount, 1, 1);
}

struct ScanPassContext
{
    RHI::FrameGraph::BufferHandle tileHistograms;
    RHI::FrameGraph::BufferHandle globalHistograms;
};

static void ScanPassBuild(
    Arena& arena, RHI::FrameGraph::Builder& builder,
    ScanPassContext& context, void* pUserData)
{
    UserData& userData = *(static_cast<UserData*>(pUserData));

    context.tileHistograms = builder.Write(arena, userData.bufferState->tileHistograms);
    context.globalHistograms = builder.Read(arena, userData.bufferState->globalHistograms);

    userData.bufferState->tileHistograms = context.tileHistograms;
}

static void ScanPassExecute(
    RHI::CommandBuffer& cmd, RHI::FrameGraph::ResourceMap& resources,
    const ScanPassContext& context, void* pUserData)
{
    UserData& userData = *(static_cast<UserData*>(pUserData));

    RHI::BindComputePipeline(cmd, sScanPipeline);

    RHI::Buffer& tileHistograms = resources.GetBuffer(context.tileHistograms);
    const RHI::Buffer& globalHistograms = resources.GetBuffer(context.globalHistograms);
    u32 pushConstants[] = {
        userData.passIndex, userData.bufferState->keyCount,
        tileHistograms.bindlessHandle,
        globalHistograms.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, RADIX_HISTOGRAM_SIZE, 1, 1);
}

struct SortPassContext
{
    RHI::FrameGraph::BufferHandle prefixSums;
    RHI::FrameGraph::BufferHandle keys;
    RHI::FrameGraph::BufferHandle sortedKeys;
};

static void SortPassBuild(Arena& arena, RHI::FrameGraph::Builder& builder,
    SortPassContext& context, void* pUserData)
{
    UserData& userData = *(static_cast<UserData*>(pUserData));
    u32 inKeysIndex = userData.passIndex % 2;
    u32 outKeysIndex = (inKeysIndex + 1) % 2;
    context.prefixSums = builder.Read(arena, userData.bufferState->tileHistograms);
    context.keys = builder.Read(arena, userData.bufferState->keys[inKeysIndex]);
    context.sortedKeys = builder.Write(arena, userData.bufferState->keys[outKeysIndex]);

    userData.bufferState->keys[outKeysIndex] = context.sortedKeys;
}

static void SortPassExecute(
    RHI::CommandBuffer& cmd, RHI::FrameGraph::ResourceMap& resources,
    const SortPassContext& context, void* pUserData)
{
    UserData& userData = *(static_cast<UserData*>(pUserData));
    u32 workGroupCount = static_cast<u32>(
        Math::Ceil(static_cast<f32>(userData.bufferState->keyCount) / COUNT_TILE_SIZE));

    RHI::BindComputePipeline(cmd, sSortPipeline);

    const RHI::Buffer& prefixSums = resources.GetBuffer(context.prefixSums);
    const RHI::Buffer& keys = resources.GetBuffer(context.keys);
    RHI::Buffer& sortedKeys = resources.GetBuffer(context.sortedKeys);

    u32 pushConstants[] = {
        userData.passIndex, userData.bufferState->keyCount,
        keys.bindlessHandle, prefixSums.bindlessHandle,
        sortedKeys.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, workGroupCount, 1, 1);
}

static void RadixSort(Arena& arena, RHI::Device& device, const u32* keys, u32 keyCount)
{
    RHI::FrameGraph fg(device);

    BufferState bufferState;
    bufferState.cpuKeys = keys;
    bufferState.keyCount = keyCount;

    UserData userData[RADIX_PASS_COUNT];

    const char* countHistogramNames[RADIX_PASS_COUNT] =
    {
        "CountHistograms_0",
        "CountHistograms_1",
        "CountHistograms_2",
        "CountHistograms_3"
    };

    const char* scanNames[RADIX_PASS_COUNT] =
    {
        "Scan_0",
        "Scan_1",
        "Scan_2",
        "Scan_3"
    };

    const char* sortNames[RADIX_PASS_COUNT] =
    {
        "Sort_0",
        "Sort_1",
        "Sort_2",
        "Sort_3"
    };

    for (u32 i = 0; i < RADIX_PASS_COUNT; i++)
    {
        userData[i].bufferState = &bufferState;
        userData[i].passIndex = i;
    }

    fg.AddPass<InitPassContext>(
        arena, "Init", RHI::FrameGraph::PassNode::Type::Transfer,
        InitPassBuild, InitPassExecute, &userData[0]);
    
    for (u32 i = 0; i < RADIX_PASS_COUNT; i++)
    {
        fg.AddPass<CountHistogramsPassContext>(
            arena, countHistogramNames[i], RHI::FrameGraph::PassNode::Type::Compute,
            CountHistogramsPassBuild, CountHistogramsPassExecute, &userData[i]);
        fg.AddPass<ScanPassContext>(
            arena, scanNames[i], RHI::FrameGraph::PassNode::Type::Compute,
            ScanPassBuild, ScanPassExecute, &userData[i]);
        fg.AddPass<SortPassContext>(
            arena, sortNames[i], RHI::FrameGraph::PassNode::Type::Compute,
            SortPassBuild, SortPassExecute, &userData[i]);
    }
    fg.Build(arena);
    fg.Execute();
    fg.Destroy();
}

static int CompareU32(const void* a, const void* b)
{
    u32 ua = *(static_cast<const u32*>(a));
    u32 ub = *(static_cast<const u32*>(b));

    if (ua < ub)
    {
        return -1;
    }
    if (ua > ub)
    {
        return 1;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    InitThreadContext();
    if (!InitLogger())
    {
        return -1;
    }
    Arena& arena = GetScratchArena();

    // Initialize volk, window
    if (volkInitialize() != VK_SUCCESS)
    {
        FLY_ERROR("Failed to load volk");
        return -1;
    }

    // Create graphics context
    RHI::ContextSettings settings{};
    settings.isPhysicalDeviceSuitableCallback = IsPhysicalDeviceSuitable;
    settings.instanceExtensions = nullptr;
    settings.instanceExtensionCount = 0;
    settings.deviceExtensions = nullptr;
    settings.deviceExtensionCount = 0;
    settings.windowPtr = nullptr;

    RHI::Context context;
    if (!RHI::CreateContext(settings, context))
    {
        FLY_ERROR("Failed to create context");
        return -1;
    }
    RHI::Device& device = context.devices[0];

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device.physicalDevice, &props);
    if (props.limits.timestampPeriod == 0)
    {
        FLY_ERROR("Device does not support timestamp queries");
        return -1;
    }
    sTimestampPeriod = props.limits.timestampPeriod;

    if (!CreateComputePipelines(device))
    {
        FLY_ERROR("Failed to create compute pipelines");
        return -1;
    }

    ArenaMarker marker = ArenaGetMarker(arena);
    u32 maxKeyCount = 100000;
    for (u32 i = 0; i < 1; i++)
    {
        u32 keyCount = Math::RandomU32(1, maxKeyCount);
        u32* keys = FLY_PUSH_ARENA(arena, u32, keyCount);
        for (u32 i = 0; i < keyCount; i++)
        {
            keys[i] = Math::RandomU32(0, 255);
        }

        RadixSort(arena, device, keys, keyCount);
        // u64 timestamps[2];
        // vkGetQueryPoolResults(device.logicalDevice, sTimestampQueryPool, 0, 2,
        //                       sizeof(timestamps), timestamps, sizeof(uint64_t),
        //                       VK_QUERY_RESULT_64_BIT |
        //                           VK_QUERY_RESULT_WAIT_BIT);

        // f64 radixSortTime = Fly::ToMilliseconds(static_cast<u64>(
        //     (timestamps[1] - timestamps[0]) * sTimestampPeriod));

        // u64 qsortStart = Fly::ClockNow();
        // qsort(keys, keyCount, sizeof(u32), CompareU32);
        // u64 qsortEnd = Fly::ClockNow();

        // f64 qsortTime = Fly::ToMilliseconds(qsortEnd - qsortStart);

        // const u32* deviceKeys =
        //     static_cast<const u32*>(RHI::BufferMappedPtr(sPingPongKeys[0]));

        // for (u32 i = 0; i < keyCount; i++)
        // {
        //     if (keys[i] != deviceKeys[i])
        //     {
        //         FLY_ERROR("[%u]: radix sort implemented incorrectly");
        //         return -1;
        //     }
        // }

        FLY_LOG("[%u]: Uniform random elements count %u: radix sort: "
                "%f ms | qsort: %f ms",
                i, keyCount, 0.0f, 0.0f);
    }
    ArenaPopToMarker(arena, marker);

    RHI::WaitAllDevicesIdle(context);
    DestroyComputePipelines(device);
    RHI::DestroyContext(context);

    FLY_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
