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

    FLY_LOG("Acc structure size in bytes %lu, %lu, %lu\n",
            sizeInfo.accelerationStructureSize, sizeInfo.updateScratchSize,
            sizeInfo.buildScratchSize);

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

    BeginOneTimeSubmit(device);
    vkCmdBuildAccelerationStructuresKHR(
        OneTimeSubmitCommandBuffer(device).handle, 1, &buildGeometryInfo,
        &rangeInfos);
    EndOneTimeSubmit(device);

    DestroyBuffer(device, scratchBuffer);

    accStructure.type = type;
    return true;
}

void DestroyAccStructure(Device& device, AccStructure& accStructure)
{
    vkDestroyAccelerationStructureKHR(device.logicalDevice, accStructure.handle,
                                      GetVulkanAllocationCallbacks());
    DestroyBuffer(device, accStructure.buffer);
}

} // namespace RHI
} // namespace Fly
