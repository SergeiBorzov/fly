#ifndef HLS_RHI_PIPELINE_H
#define HLS_RHI_PIPELINE_H

#include <volk.h>

#include "core/assert.h"
#include "core/types.h"

#define HLS_GRAPHICS_PIPELINE_COLOR_ATTACHMENT_MAX_COUNT 8

struct Arena;

namespace Hls
{

struct Device;

struct DescriptorSetLayout
{
    VkDescriptorSetLayoutBinding* bindings = nullptr;
    VkDescriptorSetLayout handle = VK_NULL_HANDLE;
    u32 bindingCount = 0;
};

enum class ShaderType
{
    Vertex = 0,
    Fragment = 1,
    Geometry = 2,
    Task = 3,
    Mesh = 4,
    Count
};

struct ShaderModule
{
    DescriptorSetLayout* descriptorSetLayouts = nullptr;
    VkShaderModule handle = VK_NULL_HANDLE;
    u32 descriptorSetLayoutCount = 0;
};

bool CreateShaderModule(Arena& arena, Device& device, const char* spvSource,
                        u64 codeSize, ShaderModule& shaderModule);
void DestroyShaderModule(Device& device, ShaderModule& shaderModule);

struct GraphicsPipelineProgrammableStage
{
    inline ShaderModule& operator[](ShaderType type)
    {
        u32 index = static_cast<u32>(type);
        HLS_ASSERT(index < static_cast<u32>(ShaderType::Count));
        return stages[index];
    }

    inline const ShaderModule& operator[](ShaderType type) const
    {
        u32 index = static_cast<u32>(type);
        HLS_ASSERT(index < static_cast<u32>(ShaderType::Count));
        return stages[index];
    }

private:
    ShaderModule stages[ShaderType::Count] = {};
};

struct GraphicsPipelineFixedStateStage
{
    struct
    {
        VkPipelineColorBlendAttachmentState
            attachments[HLS_GRAPHICS_PIPELINE_COLOR_ATTACHMENT_MAX_COUNT] = {
                VK_FALSE,
                VK_BLEND_FACTOR_ONE,
                VK_BLEND_FACTOR_ZERO,
                VK_BLEND_OP_ADD,
                VK_BLEND_FACTOR_ONE,
                VK_BLEND_FACTOR_ZERO,
                VK_BLEND_OP_ADD,
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
        f32 blendConstants[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        VkPipelineColorBlendStateCreateFlags flags = 0;
        u32 attachmentCount = 0;
    } colorBlendState;
    struct
    {
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        bool primitiveRestartEnable = false;
    } inputAssemblyState;
    struct
    {
        u32 viewportCount = 1;
    } viewportState;
    struct
    {
        VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
        VkCullModeFlags cullMode = VK_CULL_MODE_NONE;
        VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        bool rasterizerDiscardEnable = false;
    } rasterizationState;
    struct
    {
        VkStencilOpState stencilFront = {};
        VkStencilOpState stencilBack = {};
        VkCompareOp depthCompareOp = VK_COMPARE_OP_ALWAYS;
        bool depthTestEnable = false;
        bool depthWriteEnable = true;
        bool stencilTestEnable = false;
    } depthStencilState;
    struct
    {
        VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    } multisampleState;
    struct
    {
        VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED;
        VkFormat stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
        VkFormat
            colorAttachments[HLS_GRAPHICS_PIPELINE_COLOR_ATTACHMENT_MAX_COUNT] =
                {VK_FORMAT_UNDEFINED};
        u32 colorAttachmentCount = 0;
    } pipelineRendering;
};

struct GraphicsPipeline
{
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline handle = VK_NULL_HANDLE;
};

bool CreateGraphicsPipeline(
    Device& device, const GraphicsPipelineFixedStateStage& fixedStateStage,
    const GraphicsPipelineProgrammableStage& programmableStage,
    GraphicsPipeline& graphicsPipeline);

void DestroyGraphicsPipeline(Device& device,
                             GraphicsPipeline& graphicsPipeline);

void DestroyGraphicsPipelineProgrammableStage(
    Device& device, GraphicsPipelineProgrammableStage& programmableStage);

} // namespace Hls

#endif /* HLS_RHI_PIPELINE_H */
