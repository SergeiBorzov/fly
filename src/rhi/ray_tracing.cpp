#include "core/log.h"
#include "core/thread_context.h"

#include "allocation_callbacks.h"
#include "device.h"
#include "ray_tracing.h"

namespace Fly
{
namespace RHI
{

bool CreateAccStructure(
    Device& device, VkAccelerationStructureTypeKHR type,
    VkBuildAccelerationStructureFlagsKHR flags,
    VkAccelerationStructureGeometryKHR* geometries,
    const VkAccelerationStructureBuildRangeInfoKHR* rangeInfos,
    u32 geometryCount, AccStructure& accStructure)
{
    accStructure.type = type;

    VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
    buildGeometryInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildGeometryInfo.type = type;
    buildGeometryInfo.flags = flags;
    buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildGeometryInfo.geometryCount = geometryCount;
    buildGeometryInfo.pGeometries = geometries;

    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    {
        u32* maxPrimitiveCounts = FLY_PUSH_ARENA(arena, u32, geometryCount);
        for (u32 i = 0; i < geometryCount; i++)
        {
            maxPrimitiveCounts[i] = rangeInfos[i].primitiveCount;
        }
        sizeInfo.sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(
            device.logicalDevice,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo,
            maxPrimitiveCounts, &sizeInfo);
        ArenaPopToMarker(arena, marker);
    }

    Buffer scratchBuffer;
    if (!CreateBuffer(device, false,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      nullptr, sizeInfo.buildScratchSize, scratchBuffer))
    {
        return false;
    }

    if (!CreateBuffer(device, false,
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      nullptr, sizeInfo.accelerationStructureSize,
                      accStructure.buffer))
    {
        return false;
    }

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = accStructure.buffer.handle;
    createInfo.offset = 0;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = type;

    if (vkCreateAccelerationStructureKHR(device.logicalDevice, &createInfo,
                                         GetVulkanAllocationCallbacks(),
                                         &accStructure.handle) != VK_SUCCESS)
    {
        return false;
    }
    buildGeometryInfo.scratchData = {scratchBuffer.address};
    buildGeometryInfo.dstAccelerationStructure = accStructure.handle;

    if (!(flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR))
    {
        BeginOneTimeSubmit(device);
        vkCmdBuildAccelerationStructuresKHR(
            OneTimeSubmitCommandBuffer(device).handle, 1, &buildGeometryInfo,
            &rangeInfos);
        EndOneTimeSubmit(device);
        DestroyBuffer(device, scratchBuffer);

        VkAccelerationStructureDeviceAddressInfoKHR deviceAddressInfo{};
        deviceAddressInfo.sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        deviceAddressInfo.accelerationStructure = accStructure.handle;
        accStructure.address = vkGetAccelerationStructureDeviceAddressKHR(
            device.logicalDevice, &deviceAddressInfo);

        FLY_DEBUG_LOG("Acc structure [%lu] created with size %f MB",
                      accStructure.handle,
                      sizeInfo.accelerationStructureSize / 1024.0f / 1024.0f);

        return true;
    }

    QueryPool compactionQueryPool;
    if (!RHI::CreateQueryPool(
            device, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, 1,
            0, compactionQueryPool))
    {
        return false;
    }

    CommandBuffer& cmd = OneTimeSubmitCommandBuffer(device);
    BeginOneTimeSubmit(device);
    {
        ResetQueryPool(cmd, compactionQueryPool, 0, 1);
        vkCmdBuildAccelerationStructuresKHR(cmd.handle, 1, &buildGeometryInfo,
                                            &rangeInfos);
        VkMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
            .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
        };
        vkCmdPipelineBarrier(
            cmd.handle, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1,
            &barrier, 0, nullptr, 0, nullptr);
        vkCmdWriteAccelerationStructuresPropertiesKHR(
            cmd.handle, 1, &accStructure.handle,
            VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
            compactionQueryPool.handle, 0);
    }
    EndOneTimeSubmit(device);
    DestroyBuffer(device, scratchBuffer);

    u64 compactAccStructureSize = 0;
    VkAccelerationStructureKHR compactAccStructure;
    Buffer compactAccStructureBuffer;
    {
        GetQueryPoolResults(device, compactionQueryPool, 0, 1,
                            &compactAccStructureSize, sizeof(u64), sizeof(u64),
                            VK_QUERY_RESULT_WAIT_BIT);

        if (!CreateBuffer(
                device, false,
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                nullptr, compactAccStructureSize, compactAccStructureBuffer))
        {
            return false;
        }

        createInfo.sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = compactAccStructureBuffer.handle;
        createInfo.offset = 0;
        createInfo.size = compactAccStructureSize;
        createInfo.type = type;

        if (vkCreateAccelerationStructureKHR(device.logicalDevice, &createInfo,
                                             GetVulkanAllocationCallbacks(),
                                             &compactAccStructure) !=
            VK_SUCCESS)
        {
            return false;
        }
    }

    BeginOneTimeSubmit(device);
    VkCopyAccelerationStructureInfoKHR copyInfo{};
    copyInfo.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR;
    copyInfo.src = accStructure.handle;
    copyInfo.dst = compactAccStructure;
    copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;

    vkCmdCopyAccelerationStructureKHR(cmd.handle, &copyInfo);
    EndOneTimeSubmit(device);

    {
        DestroyBuffer(device, accStructure.buffer);
        vkDestroyAccelerationStructureKHR(device.logicalDevice,
                                          accStructure.handle,
                                          GetVulkanAllocationCallbacks());
    }

    accStructure.handle = compactAccStructure;
    accStructure.buffer = compactAccStructureBuffer;
    DestroyQueryPool(device, compactionQueryPool);

    VkAccelerationStructureDeviceAddressInfoKHR deviceAddressInfo{};
    deviceAddressInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    deviceAddressInfo.accelerationStructure = accStructure.handle;
    accStructure.address = vkGetAccelerationStructureDeviceAddressKHR(
        device.logicalDevice, &deviceAddressInfo);

    FLY_DEBUG_LOG("Acc Structure [%lu] created with size %f MB",
                  accStructure.handle,
                  compactAccStructureSize / 1024.0f / 1024.0f);

    return true;
}

void DestroyAccStructure(Device& device, AccStructure& accStructure)
{
    vkDestroyAccelerationStructureKHR(device.logicalDevice, accStructure.handle,
                                      GetVulkanAllocationCallbacks());
    accStructure.handle = VK_NULL_HANDLE;
    DestroyBuffer(device, accStructure.buffer);
    FLY_DEBUG_LOG("Acc structure [%lu] destroyed", accStructure.handle);
}

} // namespace RHI
} // namespace Fly
