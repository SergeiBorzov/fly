#ifndef HLS_RHI_UTILS_H
#define HLS_RHI_UTILS_H

#include "pipeline.h"

struct Arena;
struct Image;

namespace Hls
{

struct Device;
struct Buffer;

struct ShaderPathMap
{
    inline const char*& operator[](ShaderType type)
    {
        u32 index = static_cast<size_t>(type);
        HLS_ASSERT(index < static_cast<size_t>(ShaderType::Count));
        return paths[index];
    }

    inline const char* const& operator[](ShaderType type) const
    {
        u32 index = static_cast<u32>(type);
        HLS_ASSERT(index < static_cast<u32>(ShaderType::Count));
        return paths[static_cast<size_t>(index)];
    }

private:
    const char* paths[ShaderType::Count];
};

struct DescriptorPool
{
    VkDescriptorPool handle = VK_NULL_HANDLE;
    VkDescriptorSet* descriptorSets = nullptr;
    u32 descriptorSetCount = 0;
};

bool LoadImageFromFile(Arena& arena, const char* filename, Image& image);

bool LoadProgrammableStage(
    Arena& arena, Device& device, const ShaderPathMap& shaderPathMap,
    GraphicsPipelineProgrammableStage& programmableStage);

bool CreatePoolAndAllocateDescriptorsForProgrammableStage(
    Arena& arena, Device& device,
    const GraphicsPipelineProgrammableStage& programmableStage,
    DescriptorPool& descriptorPool);

void BindBufferToDescriptorSet(Device& device, const Buffer& buffer, u64 offset,
                               u64 range, VkDescriptorSet descriptorSet,
                               VkDescriptorType descriptorType, u32 setBinding);

} // namespace Hls

#endif /* End of HLS_RHI_UTILS_H */
