#ifndef HLS_RHI_STORAGE_BUFFER_H
#define HLS_RHI_STORAGE_BUFFER_H

#include <volk.h>

#include "core/types.h"
#include "vma.h"

namespace Hls
{

struct Device;

struct StorageBuffer
{
    VmaAllocationInfo allocationInfo;
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation;
};

bool CreateStorageBuffer(Device& device, const void* data, u64 size,
                         StorageBuffer& storageBuffer);
void DestroyStorageBuffer(Device& device, StorageBuffer& storageBuffer);
bool CopyDataToStorageBuffer(Device& device, const void* data, u64 size,
                             u64 offset, StorageBuffer& storageBuffer);

} // namespace Hls

#endif /* HLS_RHI_STORAGE_BUFFER_H */
