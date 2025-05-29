#include "core/assert.h"

#include "allocation_callbacks.h"
#include "device.h"
#include "pipeline.h"
#include "shader_program.h"

namespace Hls
{
namespace RHI
{

static VkShaderStageFlagBits ShaderTypeToVkShaderStage(Shader::Type shaderType)
{
    switch (shaderType)
    {
        case Shader::Type::Vertex:
        {
            return VK_SHADER_STAGE_VERTEX_BIT;
        }
        case Shader::Type::Fragment:
        {
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        case Shader::Type::Geometry:
        {
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        }
        case Shader::Type::Task:
        {
            return VK_SHADER_STAGE_TASK_BIT_EXT;
        }
        case Shader::Type::Mesh:
        {
            return VK_SHADER_STAGE_MESH_BIT_EXT;
        }
        case Shader::Type::Compute:
        {
            return VK_SHADER_STAGE_COMPUTE_BIT;
        }
        default:
        {
            HLS_ASSERT(false);
            return VK_SHADER_STAGE_VERTEX_BIT;
        }
    }
}

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

static VkPipelineRenderingCreateInfo PipelineRenderingCreateInfo(
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

static bool CreatePipelineLayout(Device& device,
                                 VkPipelineLayout& pipelineLayout)
{
    VkPushConstantRange pushConstantRange;
    pushConstantRange.stageFlags = VK_SHADER_STAGE_ALL;
    pushConstantRange.offset = 0;
    pushConstantRange.size = 128;

    VkPipelineLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.setLayoutCount = 1;
    createInfo.pSetLayouts = &device.bindlessDescriptorSetLayout;
    createInfo.pPushConstantRanges = &pushConstantRange;
    createInfo.pushConstantRangeCount = 1;

    VkResult res =
        vkCreatePipelineLayout(device.logicalDevice, &createInfo,
                               GetVulkanAllocationCallbacks(), &pipelineLayout);
    return res == VK_SUCCESS;
}

bool CreateGraphicsPipeline(Device& device,
                            const GraphicsPipelineFixedStateStage& fixedState,
                            const ShaderProgram& shaderProgram,
                            GraphicsPipeline& graphicsPipeline)
{
    HLS_ASSERT(fixedState.colorBlendState.attachmentCount ==
               fixedState.pipelineRendering.colorAttachmentCount);

    if (!CreatePipelineLayout(device, graphicsPipeline.layout))
    {
        return false;
    }

    // Programmable state
    u32 stageCount = 0;
    VkPipelineShaderStageCreateInfo
        stages[static_cast<u32>(Shader::Type::Count)];

    for (u32 i = 0; i < static_cast<u32>(Shader::Type::Count); i++)
    {
        Shader::Type shaderType = static_cast<Shader::Type>(i);
        VkShaderModule shaderModule = shaderProgram[shaderType].handle;
        if (shaderModule == VK_NULL_HANDLE)
        {
            continue;
        }

        stages[stageCount].sType =
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[stageCount].pNext = nullptr;
        stages[stageCount].flags = 0;
        stages[stageCount].stage = ShaderTypeToVkShaderStage(shaderType);
        stages[stageCount].module = shaderModule;
        stages[stageCount].pName = "main";
        stages[stageCount].pSpecializationInfo = nullptr;
        stageCount++;
    }
    HLS_ASSERT(stageCount > 0);

    // Fixed state
    VkPipelineViewportStateCreateInfo viewportState =
        ViewportStateCreateInfo(fixedState.viewportState.viewportCount);

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        InputAssemblyStateCreateInfo(
            fixedState.inputAssemblyState.topology,
            fixedState.inputAssemblyState.primitiveRestartEnable);

    VkPipelineVertexInputStateCreateInfo vertexInputState =
        VertexInputStateCreateInfo();

    VkPipelineColorBlendStateCreateInfo colorBlendState =
        ColorBlendStateCreateInfo(fixedState.colorBlendState.attachments,
                                  fixedState.colorBlendState.attachmentCount,
                                  fixedState.colorBlendState.flags,
                                  fixedState.colorBlendState.blendConstants, 4);

    VkPipelineRasterizationStateCreateInfo rasterizationState =
        RasterizationStateCreateInfo(
            fixedState.rasterizationState.polygonMode,
            fixedState.rasterizationState.cullMode,
            fixedState.rasterizationState.frontFace,
            fixedState.rasterizationState.rasterizerDiscardEnable);

    VkPipelineMultisampleStateCreateInfo multisampleState =
        MultisampleStateCreateInfo(fixedState.multisampleState.sampleCount);

    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        DepthStencilStateCreateInfo(
            fixedState.depthStencilState.depthTestEnable,
            fixedState.depthStencilState.depthCompareOp,
            fixedState.depthStencilState.depthWriteEnable,
            fixedState.depthStencilState.stencilTestEnable,
            fixedState.depthStencilState.stencilFront,
            fixedState.depthStencilState.stencilBack);

    VkPipelineDynamicStateCreateInfo dynamicState = DynamicStateCreateInfo();

    // Vulkan 1.3 dynamic rendering is used so attachments are described here
    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo =
        PipelineRenderingCreateInfo(
            fixedState.pipelineRendering.colorAttachments,
            fixedState.pipelineRendering.colorAttachmentCount,
            fixedState.pipelineRendering.depthAttachmentFormat,
            fixedState.pipelineRendering.stencilAttachmentFormat);

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

    createInfo.layout = graphicsPipeline.layout;
    createInfo.renderPass = nullptr;  // using dynamic rendering vulkan 1.3+
    createInfo.subpass = 0;           // using dynamic rendering vulkan 1.3+
    createInfo.basePipelineIndex = 0; // not used
    createInfo.basePipelineHandle = VK_NULL_HANDLE; // not used

    if (vkCreateGraphicsPipelines(device.logicalDevice, VK_NULL_HANDLE, 1,
                                  &createInfo, GetVulkanAllocationCallbacks(),
                                  &graphicsPipeline.handle) != VK_SUCCESS)
    {
        return false;
    }

    return true;
}

void DestroyGraphicsPipeline(Device& device, GraphicsPipeline& graphicsPipeline)
{
    HLS_ASSERT(graphicsPipeline.layout != VK_NULL_HANDLE);
    HLS_ASSERT(graphicsPipeline.handle != VK_NULL_HANDLE);

    vkDestroyPipeline(device.logicalDevice, graphicsPipeline.handle,
                      GetVulkanAllocationCallbacks());
    vkDestroyPipelineLayout(device.logicalDevice, graphicsPipeline.layout,
                            GetVulkanAllocationCallbacks());
    graphicsPipeline.handle = VK_NULL_HANDLE;
    graphicsPipeline.layout = VK_NULL_HANDLE;
}

bool CreateComputePipeline(Device& device, const Shader& computeShader,
                           ComputePipeline& computePipeline)
{
    if (!CreatePipelineLayout(device, computePipeline.layout))
    {
        return false;
    }

    VkPipelineShaderStageCreateInfo stageCreateInfo{};
    stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageCreateInfo.pNext = nullptr;
    stageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageCreateInfo.module = computeShader.handle;
    stageCreateInfo.pName = "main";

    VkComputePipelineCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.layout = computePipeline.layout;
    createInfo.stage = stageCreateInfo;

    if (vkCreateComputePipelines(device.logicalDevice, VK_NULL_HANDLE, 1,
                                 &createInfo, GetVulkanAllocationCallbacks(),
                                 &computePipeline.handle) != VK_SUCCESS)
    {
        return false;
    }

    return true;
}

void DestroyComputePipeline(Device& device, ComputePipeline& computePipeline)
{
    HLS_ASSERT(computePipeline.layout != VK_NULL_HANDLE);
    HLS_ASSERT(computePipeline.handle != VK_NULL_HANDLE);

    vkDestroyPipeline(device.logicalDevice, computePipeline.handle,
                      GetVulkanAllocationCallbacks());
    vkDestroyPipelineLayout(device.logicalDevice, computePipeline.layout,
                            GetVulkanAllocationCallbacks());
    computePipeline.handle = VK_NULL_HANDLE;
    computePipeline.layout = VK_NULL_HANDLE;
}

} // namespace RHI
} // namespace Hls
