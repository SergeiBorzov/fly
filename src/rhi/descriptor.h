#ifndef HLS_RHI_DESCRIPTOR_H
#define HLS_RHI_DESCRIPTOR_H

#include "core/types.h"

#include <volk.h>

namespace Hls
{

struct Device;
struct Buffer;
struct Texture;

struct DescriptorPool
{
    u32 allocatedSetCount = 0;
    u32 maxSetCount = 0;
    VkDescriptorPool handle = VK_NULL_HANDLE;
};

bool CreateDescriptorPool(Device& device, const VkDescriptorPoolSize* poolSizes,
                          u32 poolSizeCount, u32 maxSetCount,
                          DescriptorPool& descriptorPool);
void DestroyDescriptorPool(Device& device, DescriptorPool& descriptorPool);

bool AllocateDescriptorSets(Device& device, DescriptorPool& descriptorPool,
                            const VkDescriptorSetLayout* descriptorSetLayouts,
                            u32 descriptorSetLayoutCount,
                            VkDescriptorSet* descriptorSets);

void BindBufferToDescriptorSet(Device& device, const Buffer& buffer, u64 offset,
                               u64 range, VkDescriptorSet descriptorSet,
                               VkDescriptorType descriptorType, u32 setBinding);
void BindTextureToDescriptorSet(Device& device, const Texture& texture,
                                VkDescriptorSet descriptorSet,
                                u32 bindingIndex);

} // namespace Hls

#endif /* HLS_RHI_DESCRIPTOR_H */
