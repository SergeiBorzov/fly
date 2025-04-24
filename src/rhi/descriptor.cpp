#include "allocation_callbacks.h"

#include "descriptor.h"
#include "device.h"

namespace Hls
{

bool CreateDescriptorPool(Device& device, const VkDescriptorPoolSize* poolSizes,
                          u32 poolSizeCount, u32 maxSetCount,
                          DescriptorPool& descriptorPool)
{
    VkDescriptorPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.poolSizeCount = poolSizeCount;
    createInfo.pPoolSizes = poolSizes;
    createInfo.maxSets = maxSetCount;

    if (vkCreateDescriptorPool(device.logicalDevice, &createInfo,
                               GetVulkanAllocationCallbacks(),
                               &descriptorPool.handle) != VK_SUCCESS)
    {
        return false;
    }
    return true;
}

void DestroyDescriptorPool(Device& device, DescriptorPool& descriptorPool)
{
    vkDestroyDescriptorPool(device.logicalDevice, descriptorPool.handle,
                            GetVulkanAllocationCallbacks());
}

bool AllocateDescriptorSets(Device& device, DescriptorPool& descriptorPool,
                            const VkDescriptorSetLayout* descriptorSetLayouts,
                            u32 descriptorSetLayoutCount,
                            VkDescriptorSet* descriptorSets)
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool.handle;
    allocInfo.descriptorSetCount = descriptorSetLayoutCount;
    allocInfo.pSetLayouts = descriptorSetLayouts;

    if (vkAllocateDescriptorSets(device.logicalDevice, &allocInfo,
                                 descriptorSets) != VK_SUCCESS)
    {
        return false;
    }
    return true;
}

// void BindBufferToDescriptorSet(Device& device, const Buffer& buffer, u64
// offset,
//                                u64 range, VkDescriptorSet descriptorSet,
//                                VkDescriptorType descriptorType,
//                                u32 bindingIndex)
// {
//     VkDescriptorBufferInfo bufferInfo{};
//     bufferInfo.buffer = buffer.handle;
//     bufferInfo.offset = offset;
//     bufferInfo.range = range;

//     VkWriteDescriptorSet descriptorWrite{};
//     descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//     descriptorWrite.dstSet = descriptorSet;
//     descriptorWrite.dstBinding = bindingIndex;
//     descriptorWrite.dstArrayElement = 0;
//     descriptorWrite.descriptorType = descriptorType;
//     descriptorWrite.descriptorCount = 1;
//     descriptorWrite.pBufferInfo = &bufferInfo;

//     vkUpdateDescriptorSets(device.logicalDevice, 1, &descriptorWrite, 0,
//                            nullptr);
// }

void BindTextureToDescriptorSet(Device& device, const Texture& texture,
                                VkDescriptorSet descriptorSet, u32 bindingIndex)
{
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = texture.imageView;
    imageInfo.sampler = texture.sampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = bindingIndex;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device.logicalDevice, 1, &descriptorWrite, 0,
                           nullptr);
}

} // namespace Hls
