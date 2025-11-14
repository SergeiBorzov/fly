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
    RHI::Buffer buffer;
    VkAccelerationStructureTypeKHR type;
    VkAccelerationStructureKHR handle;
};

bool CreateAccStructure(
    RHI::Device& device, VkAccelerationStructureTypeKHR type,
    VkBuildAccelerationStructureFlagsKHR flags,
    VkAccelerationStructureGeometryKHR* geometries,
    const VkAccelerationStructureBuildRangeInfoKHR* rangeInfos,
    u32 geometryCount, RHI::AccStructure& accStructure);

void DestroyAccStructure(RHI::Device& device, RHI::AccStructure& accStructure);

} // namespace RHI
} // namespace Fly

#endif /* FLY_RHI_RAY_TRACING_H */
