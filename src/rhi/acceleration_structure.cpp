#include "core/log.h"
#include "core/thread_context.h"

#include "acceleration_structure.h"
#include "allocation_callbacks.h"
#include "device.h"

namespace Fly
{
namespace RHI
{

static bool CreateAccelerationStructureWithoutCompaction(
    RHI::Device& device,
    const VkAccelerationStructureBuildGeometryInfoKHR& buildGeometryInfo,
    const VkAccelerationStructureBuildRangeInfoKHR** rangeInfos,
    AccelerationStructure& accelerationStructure)
{
    BeginOneTimeSubmit(device);
    vkCmdBuildAccelerationStructuresKHR(
        OneTimeSubmitCommandBuffer(device).handle, 1, &buildGeometryInfo,
        rangeInfos);
    EndOneTimeSubmit(device);
    return true;
}

static bool CreateAccelerationStructureWithCompaction(
    RHI::Device& device,
    const VkAccelerationStructureBuildGeometryInfoKHR& buildGeometryInfo,
    const VkAccelerationStructureBuildRangeInfoKHR** rangeInfos,
    AccelerationStructure& accelerationStructure)
{
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
                                            rangeInfos);
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
            cmd.handle, 1, &accelerationStructure.handle,
            VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
            compactionQueryPool.handle, 0);
    }
    EndOneTimeSubmit(device);

    u64 compactAccelerationStructureSize = 0;
    VkAccelerationStructureKHR compactAccelerationStructure;
    Buffer compactAccelerationStructureBuffer;
    {
        GetQueryPoolResults(device, compactionQueryPool, 0, 1,
                            &compactAccelerationStructureSize, sizeof(u64),
                            sizeof(u64), VK_QUERY_RESULT_WAIT_BIT);

        if (!CreateBuffer(
                device, false,
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                nullptr, compactAccelerationStructureSize,
                compactAccelerationStructureBuffer))
        {
            return false;
        }

        VkAccelerationStructureCreateInfoKHR createInfo{};
        createInfo.sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = compactAccelerationStructureBuffer.handle;
        createInfo.offset = 0;
        createInfo.size = compactAccelerationStructureSize;
        createInfo.type = accelerationStructure.type;

        if (vkCreateAccelerationStructureKHR(device.logicalDevice, &createInfo,
                                             GetVulkanAllocationCallbacks(),
                                             &compactAccelerationStructure) !=
            VK_SUCCESS)
        {
            return false;
        }
    }

    BeginOneTimeSubmit(device);
    VkCopyAccelerationStructureInfoKHR copyInfo{};
    copyInfo.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR;
    copyInfo.src = accelerationStructure.handle;
    copyInfo.dst = compactAccelerationStructure;
    copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;

    vkCmdCopyAccelerationStructureKHR(cmd.handle, &copyInfo);
    EndOneTimeSubmit(device);

    {
        DestroyBuffer(device, accelerationStructure.buffer);
        vkDestroyAccelerationStructureKHR(device.logicalDevice,
                                          accelerationStructure.handle,
                                          GetVulkanAllocationCallbacks());
    }

    accelerationStructure.handle = compactAccelerationStructure;
    accelerationStructure.buffer = compactAccelerationStructureBuffer;
    DestroyQueryPool(device, compactionQueryPool);

    return true;
}

bool CreateAccelerationStructure(
    Device& device, VkAccelerationStructureTypeKHR type,
    VkBuildAccelerationStructureFlagsKHR flags,
    VkAccelerationStructureGeometryKHR* geometries,
    const VkAccelerationStructureBuildRangeInfoKHR* rangeInfos,
    u32 geometryCount, AccelerationStructure& accelerationStructure)
{
    accelerationStructure.type = type;

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
    u32* maxPrimitiveCounts = FLY_PUSH_ARENA(arena, u32, geometryCount);
    for (u32 i = 0; i < geometryCount; i++)
    {
        maxPrimitiveCounts[i] = rangeInfos[i].primitiveCount;
    }

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(
        device.logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildGeometryInfo, maxPrimitiveCounts, &sizeInfo);

    ArenaPopToMarker(arena, marker);

    RHI::Buffer scratchBuffer;
    if (!CreateBuffer(
            device, false,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
            nullptr, sizeInfo.buildScratchSize, scratchBuffer))
    {
        return false;
    }

    if (!CreateBuffer(device, false,
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      nullptr, sizeInfo.accelerationStructureSize,
                      accelerationStructure.buffer))
    {
        return false;
    }

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = accelerationStructure.buffer.handle;
    createInfo.offset = 0;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = type;

    if (vkCreateAccelerationStructureKHR(
            device.logicalDevice, &createInfo, GetVulkanAllocationCallbacks(),
            &accelerationStructure.handle) != VK_SUCCESS)
    {
        return false;
    }
    buildGeometryInfo.scratchData = {scratchBuffer.address};
    buildGeometryInfo.dstAccelerationStructure = accelerationStructure.handle;

    QueryPool compactionQueryPool;
    if (flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR)
    {
        if (!CreateAccelerationStructureWithCompaction(
                device, buildGeometryInfo, &rangeInfos, accelerationStructure))
        {
            return false;
        }
    }
    else
    {
        if (!CreateAccelerationStructureWithoutCompaction(
                device, buildGeometryInfo, &rangeInfos, accelerationStructure))
        {
            return false;
        }
    }
    DestroyBuffer(device, scratchBuffer);

    VkAccelerationStructureDeviceAddressInfoKHR deviceAddressInfo{};
    deviceAddressInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    deviceAddressInfo.accelerationStructure = accelerationStructure.handle;
    accelerationStructure.address = vkGetAccelerationStructureDeviceAddressKHR(
        device.logicalDevice, &deviceAddressInfo);

    accelerationStructure.bindlessHandle = FLY_MAX_U32;
    if (accelerationStructure.type ==
        VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR)
    {
        VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
        asInfo.sType =
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        asInfo.accelerationStructureCount = 1;
        asInfo.pAccelerationStructures = &accelerationStructure.handle;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = device.bindlessDescriptorSet;
        descriptorWrite.dstBinding = FLY_ACCELERATION_STRUCTURE_BINDING_INDEX;
        descriptorWrite.dstArrayElement =
            device.bindlessAccelerationStructureHandleCount;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.descriptorType =
            VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        descriptorWrite.pNext = &asInfo;
        vkUpdateDescriptorSets(device.logicalDevice, 1, &descriptorWrite, 0,
                               nullptr);
        accelerationStructure.bindlessHandle =
            (device.bindlessAccelerationStructureHandleCount)++;
    }

    // FLY_DEBUG_LOG("Acc Structure [%lu] created with bindless handle %u",
    //               accelerationStructure.handle,
    //               accelerationStructure.bindlessHandle);

    return true;
}

void DestroyAccelerationStructure(Device& device,
                                  AccelerationStructure& accelerationStructure)
{
    vkDestroyAccelerationStructureKHR(device.logicalDevice,
                                      accelerationStructure.handle,
                                      GetVulkanAllocationCallbacks());
    accelerationStructure.handle = VK_NULL_HANDLE;
    DestroyBuffer(device, accelerationStructure.buffer);
    // FLY_DEBUG_LOG("Acceleration structure [%lu] destroyed",
    //               accelerationStructure.handle);
}

} // namespace RHI
} // namespace Fly
