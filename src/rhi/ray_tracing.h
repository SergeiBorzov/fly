#ifndef FLY_RHI_RAY_TRACING_H
#define FLY_RHI_RAY_TRACING_H

#include "buffer.h"

namespace Fly
{
namespace RHI
{

struct Device;

struct AccStructure
{
    Buffer buffer;
    VkDeviceAddress address;
    VkAccelerationStructureTypeKHR type;
    VkAccelerationStructureKHR handle;
};

bool CreateAccStructure(
    Device& device, VkAccelerationStructureTypeKHR type,
    VkBuildAccelerationStructureFlagsKHR flags,
    VkAccelerationStructureGeometryKHR* geometries,
    const VkAccelerationStructureBuildRangeInfoKHR* rangeInfos,
    u32 geometryCount, AccStructure& accStructure);

void DestroyAccStructure(Device& device, AccStructure& accStructure);

} // namespace RHI
} // namespace Fly

#endif /* FLY_RHI_RAY_TRACING_H */
