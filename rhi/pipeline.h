#ifndef HLS_RHI_PIPELINE_H
#define HLS_RHI_PIPELINE_H

#include <volk.h>

#include "core/types.h"

#define HLS_GRAPHICS_PIPELINE_COLOR_ATTACHMENT_MAX_COUNT 8

namespace Hls
{

struct Device;

struct GraphicsPipelineFixedStateSettings
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

bool CreateGraphicsPipeline(
    Device& device,
    const GraphicsPipelineFixedStateSettings& fixedStateSettings,
    const VkPipelineShaderStageCreateInfo* stages, u32 stageCount,
    VkPipelineLayout layout, VkPipeline& graphicsPipeline);

void DestroyGraphicsPipeline(Device& device, VkPipeline graphicsPipeline);

bool CreateShaderModule(VkDevice device, const char* spvSource, u64 codeSize,
                        VkShaderModule& shaderModule);
void DestroyShaderModule(VkDevice device, VkShaderModule shaderModule);

bool CreatePipelineLayout(Device& device, VkPipelineLayout& pipelineLayout);
void DestroyPipelineLayout(Device& device, VkPipelineLayout pipelineLayout);

} // namespace Hls

#endif /* HLS_RHI_PIPELINE_H */
