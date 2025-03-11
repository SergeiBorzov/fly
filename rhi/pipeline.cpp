#include "core/assert.h"

#include "context.h"
#include "pipeline.h"

static VkPipelineInputAssemblyStateCreateInfo InputAssemblyStateCreateInfo(
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    bool primitiveRestartEnable = false)
{
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState;
    inputAssemblyState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.pNext = nullptr;
    inputAssemblyState.flags = 0;
    inputAssemblyState.topology = topology;
    inputAssemblyState.primitiveRestartEnable = primitiveRestartEnable;
    return inputAssemblyState;
};

static VkPipelineViewportStateCreateInfo
ViewportStateCreateInfo(u32 viewportCount = 1)
{
    VkPipelineViewportStateCreateInfo viewportState;
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;
    viewportState.flags = 0;
    viewportState.viewportCount = viewportCount;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = viewportCount;
    viewportState.pScissors = nullptr;
    return viewportState;
}

static VkPipelineColorBlendStateCreateInfo ColorBlendStateCreateInfo(
    const VkPipelineColorBlendAttachmentState* colorBlendAttachments,
    u32 colorBlendAttachmentCount,
    VkPipelineColorBlendStateCreateFlags flags = 0,
    const f32* blendConstants = nullptr, u32 blendConstantCount = 0)
{
    HLS_ASSERT(colorBlendAttachments);
    HLS_ASSERT(colorBlendAttachmentCount > 0);
    HLS_ASSERT(colorBlendAttachmentCount <=
               HLS_GRAPHICS_PIPELINE_COLOR_ATTACHMENT_MAX_COUNT);
    HLS_ASSERT(blendConstants);
    HLS_ASSERT(blendConstantCount == 4);

    VkPipelineColorBlendStateCreateInfo colorBlendState;

    colorBlendState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.pNext = nullptr;
    colorBlendState.flags = flags;
    colorBlendState.logicOpEnable = false;
    colorBlendState.logicOp = VK_LOGIC_OP_CLEAR;
    colorBlendState.pAttachments = colorBlendAttachments;
    colorBlendState.attachmentCount = colorBlendAttachmentCount;
    colorBlendState.blendConstants[0] = blendConstants[0];
    colorBlendState.blendConstants[1] = blendConstants[1];
    colorBlendState.blendConstants[2] = blendConstants[2];
    colorBlendState.blendConstants[3] = blendConstants[3];

    return colorBlendState;
}

static VkPipelineVertexInputStateCreateInfo VertexInputStateCreateInfo()
{
    VkPipelineVertexInputStateCreateInfo vertexInputState;

    vertexInputState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputState.pNext = nullptr;
    vertexInputState.flags = 0;
    vertexInputState.vertexBindingDescriptionCount = 0;
    vertexInputState.pVertexBindingDescriptions = nullptr;
    vertexInputState.vertexAttributeDescriptionCount = 0;
    vertexInputState.pVertexAttributeDescriptions = nullptr;

    return vertexInputState;
}

static VkPipelineRasterizationStateCreateInfo RasterizationStateCreateInfo(
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL,
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT,
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    bool rasterizerDiscardEnable = false)
{
    VkPipelineRasterizationStateCreateInfo rasterizationState;

    rasterizationState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationState.pNext = nullptr;
    rasterizationState.flags = 0;
    rasterizationState.depthClampEnable = false;
    rasterizationState.depthBiasEnable = false;
    rasterizationState.rasterizerDiscardEnable = rasterizerDiscardEnable;
    rasterizationState.polygonMode = polygonMode;
    rasterizationState.cullMode = cullMode;
    rasterizationState.frontFace = frontFace;
    rasterizationState.depthBiasConstantFactor = 0.0f;
    rasterizationState.depthBiasClamp = 0.0f;
    rasterizationState.depthBiasSlopeFactor = 0.0f;
    rasterizationState.lineWidth = 1.0f;

    return rasterizationState;
}

static VkPipelineMultisampleStateCreateInfo MultisampleStateCreateInfo(
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT)
{
    VkPipelineMultisampleStateCreateInfo multisampleState;

    multisampleState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.pNext = nullptr;
    multisampleState.flags = 0;
    multisampleState.sampleShadingEnable = false;
    multisampleState.minSampleShading = 1.0f;
    multisampleState.rasterizationSamples = sampleCount;
    multisampleState.alphaToCoverageEnable = false;
    multisampleState.alphaToOneEnable = false;
    multisampleState.pSampleMask = nullptr;

    return multisampleState;
}

static VkPipelineDepthStencilStateCreateInfo DepthStencilStateCreateInfo(
    bool depthTestEnable = false,
    VkCompareOp depthCompareOp = VK_COMPARE_OP_ALWAYS,
    bool depthWriteEnable = true, bool stencilTestEnable = false,
    VkStencilOpState stencilFront = {}, VkStencilOpState stencilBack = {})
{
    VkPipelineDepthStencilStateCreateInfo depthStencilState;

    depthStencilState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.pNext = nullptr;
    depthStencilState.flags = 0;

    depthStencilState.depthTestEnable = depthTestEnable;
    depthStencilState.depthWriteEnable = depthWriteEnable;
    depthStencilState.depthCompareOp = depthCompareOp;

    depthStencilState.stencilTestEnable = stencilTestEnable;
    depthStencilState.front = stencilFront;
    depthStencilState.back = stencilBack;

    depthStencilState.depthBoundsTestEnable = false;
    depthStencilState.minDepthBounds = 0.0f;
    depthStencilState.maxDepthBounds = 1.0f;

    return depthStencilState;
}

static const VkDynamicState sDynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                                VK_DYNAMIC_STATE_SCISSOR};
static VkPipelineDynamicStateCreateInfo DynamicStateCreateInfo()
{
    VkPipelineDynamicStateCreateInfo dynamicState;

    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pNext = nullptr;
    dynamicState.flags = 0;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = sDynamicStates;

    return dynamicState;
}

VkPipelineRenderingCreateInfo PipelineRenderingCreateInfo(
    const VkFormat* colorAttachments, u32 colorAttachmentCount,
    VkFormat depthAttachmentFormat, VkFormat stencilAttachmentFormat)
{
    HLS_ASSERT(colorAttachments);
    HLS_ASSERT(colorAttachmentCount > 0);
    HLS_ASSERT(colorAttachmentCount <=
               HLS_GRAPHICS_PIPELINE_COLOR_ATTACHMENT_MAX_COUNT);

    VkPipelineRenderingCreateInfo pipelineRendering;

    pipelineRendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRendering.pNext = nullptr;
    pipelineRendering.viewMask = 0;
    pipelineRendering.pColorAttachmentFormats = colorAttachments;
    pipelineRendering.colorAttachmentCount = colorAttachmentCount;
    pipelineRendering.depthAttachmentFormat = depthAttachmentFormat;
    pipelineRendering.stencilAttachmentFormat = stencilAttachmentFormat;

    return pipelineRendering;
}

namespace Hls
{

bool CreatePipelineLayout(Device& device, VkPipelineLayout& pipelineLayout)
{
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 0;
    pipelineLayoutCreateInfo.pSetLayouts = nullptr;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(device.logicalDevice, &pipelineLayoutCreateInfo,
                               nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        return false;
    }
    return true;
}

void DestroyPipelineLayout(Device& device, VkPipelineLayout pipelineLayout)
{
    vkDestroyPipelineLayout(device.logicalDevice, pipelineLayout, nullptr);
}

bool CreateShaderModule(VkDevice device, const char* spvSource, u64 codeSize,
                        VkShaderModule& shaderModule)
{
    HLS_ASSERT(spvSource);
    HLS_ASSERT((reinterpret_cast<uintptr_t>(spvSource) % 4) == 0);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = codeSize;
    createInfo.pCode = reinterpret_cast<const u32*>(spvSource);

    return vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) ==
           VK_SUCCESS;
}

void DestroyShaderModule(VkDevice device, VkShaderModule shaderModule)
{
    vkDestroyShaderModule(device, shaderModule, nullptr);
}

bool CreateGraphicsPipeline(
    Device& device,
    const GraphicsPipelineFixedStateSettings& fixedStateSettings,
    const VkPipelineShaderStageCreateInfo* stages, u32 stageCount,
    VkPipelineLayout layout, VkPipeline& graphicsPipeline)
{
    HLS_ASSERT(stages);
    HLS_ASSERT(stageCount > 0);
    HLS_ASSERT(fixedStateSettings.colorBlendState.attachmentCount ==
               fixedStateSettings.pipelineRendering.colorAttachmentCount);

    VkPipelineViewportStateCreateInfo viewportState =
        ViewportStateCreateInfo(fixedStateSettings.viewportState.viewportCount);

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        InputAssemblyStateCreateInfo(
            fixedStateSettings.inputAssemblyState.topology,
            fixedStateSettings.inputAssemblyState.primitiveRestartEnable);

    VkPipelineVertexInputStateCreateInfo vertexInputState =
        VertexInputStateCreateInfo();

    VkPipelineColorBlendStateCreateInfo colorBlendState =
        ColorBlendStateCreateInfo(
            fixedStateSettings.colorBlendState.attachments,
            fixedStateSettings.colorBlendState.attachmentCount,
            fixedStateSettings.colorBlendState.flags,
            fixedStateSettings.colorBlendState.blendConstants, 4);

    VkPipelineRasterizationStateCreateInfo rasterizationState =
        RasterizationStateCreateInfo(
            fixedStateSettings.rasterizationState.polygonMode,
            fixedStateSettings.rasterizationState.cullMode,
            fixedStateSettings.rasterizationState.frontFace,
            fixedStateSettings.rasterizationState.rasterizerDiscardEnable);

    VkPipelineMultisampleStateCreateInfo multisampleState =
        MultisampleStateCreateInfo(
            fixedStateSettings.multisampleState.sampleCount);

    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        DepthStencilStateCreateInfo(
            fixedStateSettings.depthStencilState.depthTestEnable,
            fixedStateSettings.depthStencilState.depthCompareOp,
            fixedStateSettings.depthStencilState.depthWriteEnable,
            fixedStateSettings.depthStencilState.stencilTestEnable,
            fixedStateSettings.depthStencilState.stencilFront,
            fixedStateSettings.depthStencilState.stencilBack);

    VkPipelineDynamicStateCreateInfo dynamicState = DynamicStateCreateInfo();

    // Vulkan 1.3 dynamic rendering is used so attachments are described here
    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo =
        PipelineRenderingCreateInfo(
            fixedStateSettings.pipelineRendering.colorAttachments,
            fixedStateSettings.pipelineRendering.colorAttachmentCount,
            fixedStateSettings.pipelineRendering.depthAttachmentFormat,
            fixedStateSettings.pipelineRendering.stencilAttachmentFormat);

    VkGraphicsPipelineCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.pNext = &pipelineRenderingCreateInfo;
    createInfo.flags = 0;

    createInfo.stageCount = stageCount;
    createInfo.pStages = stages;

    createInfo.pVertexInputState = &vertexInputState;
    createInfo.pInputAssemblyState = &inputAssemblyState;
    createInfo.pTessellationState = nullptr;    // disabled
    createInfo.pViewportState = &viewportState; // dynamic state
    createInfo.pRasterizationState = &rasterizationState;
    createInfo.pMultisampleState = &multisampleState;
    createInfo.pDepthStencilState = &depthStencilState;
    createInfo.pColorBlendState = &colorBlendState;

    createInfo.pDynamicState = &dynamicState;

    createInfo.layout = layout;
    createInfo.renderPass = nullptr;  // using dynamic rendering vulkan 1.3+
    createInfo.subpass = 0;           // using dynamic rendering vulkan 1.3+
    createInfo.basePipelineIndex = 0; // not used
    createInfo.basePipelineHandle = VK_NULL_HANDLE; // not used

    if (vkCreateGraphicsPipelines(device.logicalDevice, VK_NULL_HANDLE, 1,
                                  &createInfo, nullptr,
                                  &graphicsPipeline) != VK_SUCCESS)
    {
        return false;
    }

    return true;
}

void DestroyGraphicsPipeline(Device& device, VkPipeline graphicsPipeline)
{
    vkDestroyPipeline(device.logicalDevice, graphicsPipeline, nullptr);
}

} // namespace Hls
