#ifndef FLY_RHI_ACCELERATION_STRUCTURE_H
#define FLY_RHI_ACCELERATION_STRUCTURE_H

#include "buffer.h"

namespace Fly
{
namespace RHI
{

struct Device;

struct AccelerationStructure
{
    Buffer buffer;
    VkDeviceAddress address;
    VkAccelerationStructureTypeKHR type;
    VkAccelerationStructureKHR handle;
};

bool CreateAccelerationStructure(
    Device& device, VkAccelerationStructureTypeKHR type,
    VkBuildAccelerationStructureFlagsKHR flags,
    VkAccelerationStructureGeometryKHR* geometries,
    const VkAccelerationStructureBuildRangeInfoKHR* rangeInfos,
    u32 geometryCount, AccelerationStructure& accelerationStructure);

void DestroyAccelerationStructure(Device& device,
                                  AccelerationStructure& accelerationStructure);

} // namespace RHI
} // namespace Fly

#endif /* FLY_RHI_ACCELERATION_STRUCTURE_H */
