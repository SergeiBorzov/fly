#ifndef HLS_RHI_INDIRECT_DRAW_BUFFER_H
#define HLS_RHI_INDIRECT_DRAW_BUFFER_H

#include <volk.h>

#include "core/types.h"
#include "vma.h"

namespace Hls
{
namespace RHI
{

struct Device;

struct IndirectDrawBuffer
{
    VmaAllocationInfo allocationInfo;
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation;
    void* mappedPtr = nullptr;
    u32 bindlessHandle = HLS_MAX_U32;
};

bool CreateIndirectDrawBuffer(Device& device, const void* data, u64 size,
                              IndirectDrawBuffer& indirectDrawBuffer);
void DestroyIndirectDrawBuffer(Device& device,
                               IndirectDrawBuffer& indirectDrawBuffer);
bool CopyDataToIndirectDrawBuffer(Device& device, const void* data, u64 size,
                                  u64 offset,
                                  IndirectDrawBuffer& indirectDrawBuffer);

} // namespace RHI
} // namespace Hls

#endif /* HLS_RHI_INDIRECT_DRAW_BUFFER */
