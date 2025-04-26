#ifndef HLS_RHI_STORAGE_BUFFER_H
#define HLS_RHI_STORAGE_BUFFER_H

#include <volk.h>

#include "core/types.h"
#include "vma.h"

namespace Hls
{
namespace RHI
{

struct Device;

struct StorageBuffer
{
    VmaAllocationInfo allocationInfo;
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation;
    u32 bindlessHandle = HLS_MAX_U32;
};

bool CreateStorageBuffer(Device& device, const void* data, u64 size,
                         StorageBuffer& storageBuffer);
void DestroyStorageBuffer(Device& device, StorageBuffer& storageBuffer);
bool CopyDataToStorageBuffer(Device& device, const void* data, u64 size,
                             u64 offset, StorageBuffer& storageBuffer);

} // namespace RHI
} // namespace Hls

#endif /* HLS_RHI_STORAGE_BUFFER_H */
