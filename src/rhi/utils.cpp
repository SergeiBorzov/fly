#include <string.h>

#include "core/filesystem.h"
#include "core/platform.h"
#include "core/thread_context.h"

#include "buffer.h"
#include "context.h"
#include "texture.h"
#include "utils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Hls
{

bool LoadImageFromFile(Arena& arena, const char* filename, Image& image)
{
    int x = 0, y = 0, n = 0;
    int desiredChannelCount = 4;
    unsigned char* data = stbi_load(filename, &x, &y, &n, desiredChannelCount);
    if (!data)
    {
        return nullptr;
    }

    image.data = HLS_ALLOC(arena, u8, x * y * desiredChannelCount);
    memcpy(image.data, data, sizeof(u8) * x * y * desiredChannelCount);
    image.width = static_cast<u32>(x);
    image.height = static_cast<u32>(y);
    image.channelCount = static_cast<u32>(desiredChannelCount);
    stbi_image_free(data);

    return data;
}

bool LoadProgrammableStage(Arena& arena, Device& device,
                           const ShaderPathMap& shaderPathMap,
                           GraphicsPipelineProgrammableStage& programmableStage)
{
    Arena& scratch = GetScratchArena(&arena);
    ArenaMarker marker = ArenaGetMarker(scratch);

    for (u32 i = 0; i < static_cast<u32>(ShaderType::Count); i++)
    {
        ArenaMarker loopMarker = ArenaGetMarker(scratch);

        ShaderType shaderType = static_cast<ShaderType>(i);

        if (!shaderPathMap[shaderType])
        {
            continue;
        }

        String8 spvSource =
            ReadFileToString(scratch, shaderPathMap[shaderType], sizeof(u32));
        if (!spvSource)
        {
            ArenaPopToMarker(scratch, marker);
            return false;
        }
        if (!Hls::CreateShaderModule(arena, device, spvSource.Data(), spvSource.Size(),
                                     programmableStage[shaderType]))
        {
            ArenaPopToMarker(scratch, marker);
            return false;
        }

        ArenaPopToMarker(scratch, loopMarker);
    }

    ArenaPopToMarker(scratch, marker);
    return true;
}

bool CreatePoolAndAllocateDescriptorsForProgrammableStage(
    Arena& arena, Device& device,
    const GraphicsPipelineProgrammableStage& programmableStage,
    DescriptorPool& descriptorPool)
{
    Arena& scratch = GetScratchArena(&arena);
    ArenaMarker marker = ArenaGetMarker(arena);

    u32 descriptorTypeCount[11] = {0};

    u32 setCount = 0;
    for (i32 i = 0; i < static_cast<i32>(ShaderType::Count); i++)
    {
        const ShaderModule& shaderModule =
            programmableStage[static_cast<ShaderType>(i)];
        setCount += shaderModule.descriptorSetLayoutCount;
        for (u32 j = 0; j < shaderModule.descriptorSetLayoutCount; j++)
        {
            const DescriptorSetLayout& descriptorSetLayout =
                shaderModule.descriptorSetLayouts[j];
            for (u32 k = 0; k < descriptorSetLayout.bindingCount; k++)
            {
                const VkDescriptorSetLayoutBinding& binding =
                    descriptorSetLayout.bindings[k];
                u32 index = static_cast<u32>(binding.descriptorType);
                HLS_ASSERT(index <= 11);
                descriptorTypeCount[index] += binding.descriptorCount;
            }
        }
    }

    u32 poolSizeCount = 0;
    for (i32 i = 0; i < 11; i++)
    {
        poolSizeCount += descriptorTypeCount[i] > 0;
    }

    if (poolSizeCount == 0)
    {
        return false;
    }

    VkDescriptorPoolSize* poolSizes =
        HLS_ALLOC(scratch, VkDescriptorPoolSize, poolSizeCount);
    u32 poolSizeIndex = 0;
    for (i32 i = 0; i < 11; i++)
    {
        if (descriptorTypeCount[i] == 0)
        {
            continue;
        }
        poolSizes[poolSizeIndex].type = static_cast<VkDescriptorType>(i);
        poolSizes[poolSizeIndex].descriptorCount =
            descriptorTypeCount[i] * HLS_FRAME_IN_FLIGHT_COUNT;
        poolSizeIndex++;
    }

    VkDescriptorPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.poolSizeCount = poolSizeCount;
    createInfo.pPoolSizes = poolSizes;
    createInfo.maxSets = setCount * HLS_FRAME_IN_FLIGHT_COUNT;

    if (vkCreateDescriptorPool(device.logicalDevice, &createInfo, nullptr,
                               &descriptorPool.handle) != VK_SUCCESS)
    {
        ArenaPopToMarker(scratch, marker);
        return false;
    }

    VkDescriptorSetLayout* descriptorSetLayouts = HLS_ALLOC(
        scratch, VkDescriptorSetLayout, setCount * HLS_FRAME_IN_FLIGHT_COUNT);

    u32 layoutIndex = 0;
    for (i32 i = 0; i < static_cast<i32>(ShaderType::Count); i++)
    {
        const ShaderModule& shaderModule =
            programmableStage[static_cast<ShaderType>(i)];
        for (u32 j = 0; j < shaderModule.descriptorSetLayoutCount; j++)
        {
            const DescriptorSetLayout& descriptorSetLayout =
                shaderModule.descriptorSetLayouts[j];

            for (u32 k = 0; k < HLS_FRAME_IN_FLIGHT_COUNT; k++)
            {
                descriptorSetLayouts[layoutIndex++] =
                    shaderModule.descriptorSetLayouts[j].handle;
            }
        }
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool.handle;
    allocInfo.descriptorSetCount = setCount * HLS_FRAME_IN_FLIGHT_COUNT;
    allocInfo.pSetLayouts = descriptorSetLayouts;

    descriptorPool.descriptorSetCount = setCount * HLS_FRAME_IN_FLIGHT_COUNT;
    descriptorPool.descriptorSets =
        HLS_ALLOC(arena, VkDescriptorSet, setCount * HLS_FRAME_IN_FLIGHT_COUNT);

    VkResult res = vkAllocateDescriptorSets(device.logicalDevice, &allocInfo,
                                            descriptorPool.descriptorSets);
    if (res != VK_SUCCESS)
    {
        ArenaPopToMarker(scratch, marker);
        return false;
    }

    ArenaPopToMarker(scratch, marker);
    return true;
}

void BindBufferToDescriptorSet(Device& device, const Buffer& buffer, u64 offset,
                               u64 range, VkDescriptorSet descriptorSet,
                               VkDescriptorType descriptorType,
                               u32 bindingIndex)
{
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer.handle;
    bufferInfo.offset = offset;
    bufferInfo.range = range;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = bindingIndex;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = descriptorType;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(device.logicalDevice, 1, &descriptorWrite, 0,
                           nullptr);
}

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
