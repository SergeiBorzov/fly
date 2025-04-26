#ifndef HLS_RHI_INDEX_BUFFER_H
#define HLS_RHI_INDEX_BUFFER_H

#include <volk.h>

#include "core/types.h"
#include "vma.h"

namespace Hls
{
namespace RHI
{

struct Device;

struct IndexBuffer
{
    VmaAllocationInfo allocationInfo;
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation;
};

bool CreateIndexBuffer(Device& device, const u32* indices, u32 indexCount,
                       IndexBuffer& indexBuffer);
void DestroyIndexBuffer(Device& device, IndexBuffer& indexBuffer);
bool CopyDataToIndexBuffer(Device& device, const u32* indices, u32 indexCount,
                           u32 offsetCount, IndexBuffer& storageBuffer);

} // namespace RHI
} // namespace Hls

#endif /* HLS_RHI_INDEX_BUFFER_H */
