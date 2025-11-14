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

#include "utils/utils.h"

#include <stdlib.h>

#define RADIX_PASS_COUNT 4
#define RADIX_BIT_COUNT 8
#define RADIX_HISTOGRAM_SIZE (1U << RADIX_BIT_COUNT)
#define COUNT_WORKGROUP_SIZE 512
#define COUNT_TILE_SIZE COUNT_WORKGROUP_SIZE
#define SCAN_WORKGROUP_SIZE (COUNT_WORKGROUP_SIZE / 2)

using namespace Fly;

static RHI::ComputePipeline sCountPipeline;
static RHI::ComputePipeline sScanPipeline;
static RHI::ComputePipeline sSortPipeline;
static RHI::QueryPool sTimestampQueryPool;
static f32 sTimestampPeriod;
static u32 sMaxKeyCount;

static RHI::Buffer sKeys[2];
static RHI::Buffer sTileHistograms;
static RHI::Buffer sGlobalHistograms;

struct RadixSortData
{
    u32 passIndex;
    u32 keyCount;
};

static bool CreateComputePipelines(RHI::Device& device)
{
    RHI::Shader countShader;
    if (!Fly::LoadShaderFromSpv(
            device, FLY_STRING8_LITERAL("radix_count_histogram.comp.spv"),
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
    if (!Fly::LoadShaderFromSpv(
            device, FLY_STRING8_LITERAL("radix_scan.comp.spv"), scanShader))
    {
        return false;
    }
    if (!RHI::CreateComputePipeline(device, scanShader, sScanPipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, scanShader);

    RHI::Shader sortShader;
    if (!Fly::LoadShaderFromSpv(
            device, FLY_STRING8_LITERAL("radix_sort.comp.spv"), sortShader))
    {
        return false;
    }
    if (!RHI::CreateComputePipeline(device, sortShader, sSortPipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, sortShader);

    if (!RHI::CreateQueryPool(device, VK_QUERY_TYPE_TIMESTAMP, 2, 0,
                              sTimestampQueryPool))
    {
        return false;
    }

    return true;
}

static void DestroyComputePipelines(RHI::Device& device)
{
    RHI::DestroyQueryPool(device, sTimestampQueryPool);
    RHI::DestroyComputePipeline(device, sScanPipeline);
    RHI::DestroyComputePipeline(device, sCountPipeline);
    RHI::DestroyComputePipeline(device, sSortPipeline);
}

static bool CreateResources(RHI::Device& device)
{
    for (u32 i = 0; i < 2; i++)
    {
        if (!RHI::CreateBuffer(device, true,
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               nullptr, sizeof(u32) * sMaxKeyCount, sKeys[i]))
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
        Math::Ceil(static_cast<f32>(sMaxKeyCount) / COUNT_TILE_SIZE));
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

static void DestroyResources(RHI::Device& device)
{
    for (u32 i = 0; i < 2; i++)
    {
        RHI::DestroyBuffer(device, sKeys[i]);
    }
    RHI::DestroyBuffer(device, sTileHistograms);
    RHI::DestroyBuffer(device, sGlobalHistograms);
}

static void RecordCountHistograms(RHI::CommandBuffer& cmd,
                                  const RHI::RecordBufferInput* bufferInput,
                                  u32 bufferInputCount,
                                  const RHI::RecordTextureInput* textureInput,
                                  u32 textureInputCount, void* pUserData)
{
    RadixSortData& sortData = *(static_cast<RadixSortData*>(pUserData));
    u32 workGroupCount = static_cast<u32>(
        Math::Ceil(static_cast<f32>(sortData.keyCount) / COUNT_TILE_SIZE));

    if (sortData.passIndex == 0)
    {
        RHI::ResetQueryPool(cmd, sTimestampQueryPool, 0, 2);
        RHI::WriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            sTimestampQueryPool, 0);
    }

    RHI::BindComputePipeline(cmd, sCountPipeline);

    RHI::Buffer& keys = *(bufferInput[0].pBuffer);
    RHI::Buffer& tileHistograms = *(bufferInput[1].pBuffer);
    RHI::Buffer& globalHistograms = *(bufferInput[2].pBuffer);

    u32 pushConstants[] = {sortData.passIndex, sortData.keyCount,
                           keys.bindlessHandle, tileHistograms.bindlessHandle,
                           globalHistograms.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));

    RHI::Dispatch(cmd, workGroupCount, 1, 1);
}

static void RecordScan(RHI::CommandBuffer& cmd,
                       const RHI::RecordBufferInput* bufferInput,
                       u32 bufferInputCount,
                       const RHI::RecordTextureInput* textureInput,
                       u32 textureInputCount, void* pUserData)
{
    RadixSortData& sortData = *(static_cast<RadixSortData*>(pUserData));

    RHI::BindComputePipeline(cmd, sScanPipeline);

    RHI::Buffer& tileHistograms = *(bufferInput[0].pBuffer);
    RHI::Buffer& globalHistograms = *(bufferInput[1].pBuffer);

    u32 pushConstants[] = {sortData.passIndex, sortData.keyCount,
                           tileHistograms.bindlessHandle,
                           globalHistograms.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, RADIX_HISTOGRAM_SIZE, 1, 1);
}

static void RecordSort(RHI::CommandBuffer& cmd,
                       const RHI::RecordBufferInput* bufferInput,
                       u32 bufferInputCount,
                       const RHI::RecordTextureInput* textureInput,
                       u32 textureInputCount, void* pUserData)
{
    RadixSortData& sortData = *(static_cast<RadixSortData*>(pUserData));
    u32 workGroupCount = static_cast<u32>(
        Math::Ceil(static_cast<f32>(sortData.keyCount) / COUNT_TILE_SIZE));

    RHI::BindComputePipeline(cmd, sSortPipeline);

    RHI::Buffer& keys = *(bufferInput[0].pBuffer);
    RHI::Buffer& prefixSums = *(bufferInput[1].pBuffer);
    RHI::Buffer& sortedKeys = *(bufferInput[2].pBuffer);

    u32 pushConstants[] = {sortData.passIndex, sortData.keyCount,
                           keys.bindlessHandle, prefixSums.bindlessHandle,
                           sortedKeys.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, workGroupCount, 1, 1);

    if (sortData.passIndex == RADIX_PASS_COUNT - 1)
    {
        RHI::WriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            sTimestampQueryPool, 1);
    }
}

static void RadixSort(RHI::Device& device, const u32* keys, u32 keyCount)
{
    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    RHI::CopyDataToBuffer(device, keys, sizeof(u32) * keyCount, 0, sKeys[0]);

    RHI::BeginOneTimeSubmit(device);

    RHI::RecordBufferInput bufferInput[3];

    RadixSortData sortData;
    sortData.keyCount = keyCount;
    for (u32 i = 0; i < RADIX_PASS_COUNT; i++)
    {
        sortData.passIndex = i;
        {
            bufferInput[0] = {&sKeys[i % 2], VK_ACCESS_2_SHADER_READ_BIT};
            bufferInput[1] = {&sTileHistograms, VK_ACCESS_2_SHADER_WRITE_BIT};
            bufferInput[2] = {&sGlobalHistograms,
                              VK_ACCESS_2_SHADER_READ_BIT |
                                  VK_ACCESS_2_SHADER_WRITE_BIT};

            RHI::ExecuteCompute(OneTimeSubmitCommandBuffer(device),
                                RecordCountHistograms, bufferInput, 3, nullptr,
                                0, &sortData);
        }

        {
            bufferInput[0] = {&sTileHistograms, VK_ACCESS_2_SHADER_WRITE_BIT};
            bufferInput[1] = {&sGlobalHistograms, VK_ACCESS_2_SHADER_READ_BIT};

            RHI::ExecuteCompute(OneTimeSubmitCommandBuffer(device), RecordScan,
                                bufferInput, 2, nullptr, 0, &sortData);
        }

        {
            bufferInput[0] = {&sKeys[i % 2], VK_ACCESS_2_SHADER_READ_BIT};
            bufferInput[1] = {&sTileHistograms, VK_ACCESS_2_SHADER_READ_BIT};
            bufferInput[2] = {&sKeys[(i + 1) % 2], VK_ACCESS_SHADER_WRITE_BIT};
            RHI::ExecuteCompute(OneTimeSubmitCommandBuffer(device), RecordSort,
                                bufferInput, 3, nullptr, 0, &sortData);
        }
    }
    RHI::EndOneTimeSubmit(device);

    ArenaPopToMarker(arena, marker);
    RHI::WaitDeviceIdle(device);
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

    // Initialize volk, window
    if (volkInitialize() != VK_SUCCESS)
    {
        FLY_ERROR("Failed to load volk");
        return -1;
    }

    // Create graphics context
    RHI::ContextSettings settings{};
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

    sMaxKeyCount = 100000;
    if (!CreateResources(device))
    {
        return false;
    }

    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    for (u32 i = 0; i < 25; i++)
    {
        u32 keyCount = Math::RandomU32(1, sMaxKeyCount);
        u32* keys = FLY_PUSH_ARENA(arena, u32, keyCount);
        for (u32 i = 0; i < keyCount; i++)
        {
            keys[i] = Math::RandomU32(0, 12345678);
        }

        RadixSort(device, keys, keyCount);

        u64 timestamps[2];
        RHI::GetQueryPoolResults(device, sTimestampQueryPool, 0, 2, timestamps,
                                 sizeof(timestamps), sizeof(uint64_t),
                                 VK_QUERY_RESULT_64_BIT |
                                     VK_QUERY_RESULT_WAIT_BIT);
        f64 radixSortTime = Fly::ToMilliseconds(static_cast<u64>(
            (timestamps[1] - timestamps[0]) * sTimestampPeriod));

        u64 qsortStart = Fly::ClockNow();
        qsort(keys, keyCount, sizeof(u32), CompareU32);
        u64 qsortEnd = Fly::ClockNow();
        f64 qsortTime = Fly::ToMilliseconds(qsortEnd - qsortStart);

        const u32* deviceKeys =
            static_cast<const u32*>(RHI::BufferMappedPtr(sKeys[0]));

        for (u32 i = 0; i < keyCount; i++)
        {
            if (keys[i] != deviceKeys[i])
            {
                FLY_ERROR("[%u]: radix sort implemented incorrectly");
                return -1;
            }
        }

        FLY_LOG("[%u]: Uniform random elements count %u: radix sort: "
                "%f ms | qsort: %f ms",
                i, keyCount, radixSortTime, qsortTime);
        ArenaPopToMarker(arena, marker);
    }

    RHI::WaitAllDevicesIdle(context);
    DestroyResources(device);
    DestroyComputePipelines(device);

    RHI::DestroyContext(context);

    FLY_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
