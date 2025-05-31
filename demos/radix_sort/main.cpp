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

#include "demos/common/scene.h"

#include <stdlib.h>

#define RADIX_PASS_COUNT 4
#define RADIX_BIT_COUNT 8
#define RADIX_HISTOGRAM_SIZE (1U << RADIX_BIT_COUNT)
#define COUNT_WORKGROUP_SIZE 512
#define COUNT_TILE_SIZE COUNT_WORKGROUP_SIZE
#define SCAN_WORKGROUP_SIZE (COUNT_WORKGROUP_SIZE / 2)

using namespace Fly;

static bool IsPhysicalDeviceSuitable(const RHI::Context& context,
                                     const RHI::PhysicalDeviceInfo& info)
{
    return info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

static RHI::Buffer sTileHistograms;
static RHI::Buffer sGlobalHistograms;
static RHI::Buffer sPingPongKeys[2];
static bool CreateDeviceBuffers(RHI::Device& device, u32 maxKeyCount)
{
    u32 tileCount = static_cast<u32>(
        Math::Ceil(static_cast<f32>(maxKeyCount) / COUNT_TILE_SIZE));

    if (!RHI::CreateStorageBuffer(
            device, true, nullptr,
            RADIX_HISTOGRAM_SIZE * tileCount * sizeof(u32), sTileHistograms))
    {
        return false;
    }

    if (!RHI::CreateStorageBuffer(device, true, nullptr,
                                  sizeof(u32) * RADIX_HISTOGRAM_SIZE *
                                      RADIX_PASS_COUNT,
                                  sGlobalHistograms))
    {
        return false;
    }

    for (u32 i = 0; i < 2; i++)
    {
        if (!RHI::CreateStorageBuffer(device, true, nullptr,
                                      sizeof(u32) * maxKeyCount,
                                      sPingPongKeys[i]))
        {
            return false;
        }
    }
    return true;
}

static void DestroyDeviceBuffers(RHI::Device& device)
{
    for (u32 i = 0; i < 2; i++)
    {
        RHI::DestroyBuffer(device, sPingPongKeys[i]);
    }
    RHI::DestroyBuffer(device, sGlobalHistograms);
    RHI::DestroyBuffer(device, sTileHistograms);
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

static void ExecuteRadixKernels(RHI::Device& device, u32 keyCount)
{
    RHI::BeginTransfer(device);
    RHI::CommandBuffer& cmd = TransferCommandBuffer(device);
    vkCmdResetQueryPool(cmd.handle, sTimestampQueryPool, 0, 2);

    u32 workGroupCount = static_cast<u32>(
        Math::Ceil(static_cast<f32>(keyCount) / COUNT_TILE_SIZE));

    vkCmdWriteTimestamp(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        sTimestampQueryPool, 0);
    // Reset global histogram buffer
    vkCmdFillBuffer(cmd.handle, sGlobalHistograms.handle, 0,
                    sizeof(u32) * RADIX_HISTOGRAM_SIZE * RADIX_PASS_COUNT, 0);
    VkBufferMemoryBarrier resetToCountBarrier;
    resetToCountBarrier = RHI::BufferMemoryBarrier(
        sGlobalHistograms, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1,
                         &resetToCountBarrier, 0, nullptr);

    for (u32 i = 0; i < RADIX_PASS_COUNT; i++)
    {
        u32 in = i % 2;
        u32 out = (i + 1) % 2;

        // Calculate count histograms
        vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                          sCountPipeline.handle);
        vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                                sCountPipeline.layout, 0, 1,
                                &device.bindlessDescriptorSet, 0, nullptr);

        u32 pushConstants[] = {i, keyCount, sPingPongKeys[in].bindlessHandle,
                               sTileHistograms.bindlessHandle,
                               sGlobalHistograms.bindlessHandle};
        vkCmdPushConstants(cmd.handle, sCountPipeline.layout,
                           VK_SHADER_STAGE_ALL, 0, sizeof(pushConstants),
                           pushConstants);
        vkCmdDispatch(cmd.handle, workGroupCount, 1, 1);

        VkBufferMemoryBarrier countToScanBarriers[2];
        countToScanBarriers[0] = RHI::BufferMemoryBarrier(
            sTileHistograms, VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        countToScanBarriers[1] = RHI::BufferMemoryBarrier(
            sGlobalHistograms, VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT);
        vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                             nullptr, STACK_ARRAY_COUNT(countToScanBarriers),
                             countToScanBarriers, 0, nullptr);

        // Exclusive prefix sums - global offsets
        vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                          sScanPipeline.handle);
        vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                                sScanPipeline.layout, 0, 1,
                                &device.bindlessDescriptorSet, 0, nullptr);
        vkCmdPushConstants(cmd.handle, sScanPipeline.layout,
                           VK_SHADER_STAGE_ALL, 0, sizeof(pushConstants),
                           pushConstants);
        vkCmdDispatch(cmd.handle, RADIX_HISTOGRAM_SIZE, 1, 1);

        VkBufferMemoryBarrier scanToSortBarrier = RHI::BufferMemoryBarrier(
            sTileHistograms,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT);
        vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                             nullptr, 1, &scanToSortBarrier, 0, nullptr);

        // Sort
        vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                          sSortPipeline.handle);
        vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                                sSortPipeline.layout, 0, 1,
                                &device.bindlessDescriptorSet, 0, nullptr);
        pushConstants[4] = sPingPongKeys[out].bindlessHandle;
        vkCmdPushConstants(cmd.handle, sSortPipeline.layout,
                           VK_SHADER_STAGE_ALL, 0, sizeof(pushConstants),
                           pushConstants);
        vkCmdDispatch(cmd.handle, workGroupCount, 1, 1);

        if (i == RADIX_PASS_COUNT - 1)
        {
            continue;
        }

        VkBufferMemoryBarrier nextPassBarriers[3];
        nextPassBarriers[0] = RHI::BufferMemoryBarrier(
            sPingPongKeys[out], VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT);
        nextPassBarriers[1] = RHI::BufferMemoryBarrier(
            sPingPongKeys[in], VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_SHADER_WRITE_BIT);
        nextPassBarriers[2] = RHI::BufferMemoryBarrier(
            sGlobalHistograms, VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_SHADER_WRITE_BIT);
        vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                             nullptr, STACK_ARRAY_COUNT(nextPassBarriers),
                             nextPassBarriers, 0, nullptr);
    }
    vkCmdWriteTimestamp(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        sTimestampQueryPool, 1);

    RHI::EndTransfer(device);
}

static void RadixSort(RHI::Device& device, const u32* keys, u32 keyCount)
{
    RHI::BeginTransfer(device);
    RHI::CopyDataToBuffer(device, keys, sizeof(u32) * keyCount, 0,
                          sPingPongKeys[0]);
    RHI::EndTransfer(device);

    ExecuteRadixKernels(device, keyCount);
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

    u32 maxKeyCount = 7000000;
    if (!CreateDeviceBuffers(device, maxKeyCount))
    {
        FLY_ERROR("Failed to create device buffers");
        return -1;
    }

    if (!CreateComputePipelines(device))
    {
        FLY_ERROR("Failed to create compute pipelines");
        return -1;
    }

    ArenaMarker marker = ArenaGetMarker(arena);
    for (u32 i = 0; i < 100; i++)
    {
        u32 keyCount = Math::RandomU32(1, maxKeyCount);
        u32* keys = FLY_ALLOC(arena, u32, keyCount);
        for (u32 i = 0; i < keyCount; i++)
        {
            keys[i] = Math::RandomU32(0, 255);
        }

        RadixSort(device, keys, keyCount);
        u64 timestamps[2];
        vkGetQueryPoolResults(device.logicalDevice, sTimestampQueryPool, 0, 2,
                              sizeof(timestamps), timestamps, sizeof(uint64_t),
                              VK_QUERY_RESULT_64_BIT |
                                  VK_QUERY_RESULT_WAIT_BIT);

        f64 radixSortTime = Fly::ToMilliseconds(static_cast<u64>(
            (timestamps[1] - timestamps[0]) * sTimestampPeriod));

        u64 qsortStart = Fly::ClockNow();
        qsort(keys, keyCount, sizeof(u32), CompareU32);
        u64 qsortEnd = Fly::ClockNow();

        f64 qsortTime = Fly::ToMilliseconds(qsortEnd - qsortStart);

        const u32* deviceKeys =
            static_cast<const u32*>(RHI::BufferMappedPtr(sPingPongKeys[0]));

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
    DestroyDeviceBuffers(device);
    DestroyComputePipelines(device);
    RHI::DestroyContext(context);

    FLY_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
