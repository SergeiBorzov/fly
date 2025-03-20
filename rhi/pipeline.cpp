#include "core/assert.h"
#include "core/thread_context.h"

#include "context.h"
#include "pipeline.h"

#include <spirv_reflect.h>

namespace Hls
{

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

static bool CreateShaderModuleDescriptorSetLayouts(Arena& arena, Device& device,
                                                   const char* spvSource,
                                                   u64 codeSize,
                                                   ShaderModule& shaderModule)
{
    HLS_ASSERT(spvSource);
    HLS_ASSERT((reinterpret_cast<uintptr_t>(spvSource) % 4) == 0);

    Arena& scratch = GetScratchArena(&arena);
    ArenaMarker marker = ArenaGetMarker(scratch);

    SpvReflectShaderModule reflectModule;
    SpvReflectResult res = spvReflectCreateShaderModule(
        codeSize, reinterpret_cast<const u32*>(spvSource), &reflectModule);
    if (res != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    u32 descriptorSetCount = 0;
    res = spvReflectEnumerateDescriptorSets(&reflectModule, &descriptorSetCount,
                                            nullptr);
    if (descriptorSetCount == 0)
    {
        return true;
    }

    SpvReflectDescriptorSet** reflDescriptorSets =
        HLS_ALLOC(scratch, SpvReflectDescriptorSet*, descriptorSetCount);
    res = spvReflectEnumerateDescriptorSets(&reflectModule, &descriptorSetCount,
                                            reflDescriptorSets);
    if (res != SPV_REFLECT_RESULT_SUCCESS)
    {
        ArenaPopToMarker(scratch, marker);
        return false;
    }

    shaderModule.descriptorSetLayouts =
        HLS_ALLOC(arena, DescriptorSetLayout, descriptorSetCount);
    shaderModule.descriptorSetLayoutCount = descriptorSetCount;

    for (u32 i = 0; i < descriptorSetCount; i++)
    {
        SpvReflectDescriptorSet* reflDescriptorSet = reflDescriptorSets[i];
        DescriptorSetLayout& descriptorSetLayout =
            shaderModule.descriptorSetLayouts[i];

        VkDescriptorSetLayoutCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

        descriptorSetLayout.bindings =
            HLS_ALLOC(arena, VkDescriptorSetLayoutBinding,
                      reflDescriptorSet->binding_count);
        descriptorSetLayout.bindingCount = reflDescriptorSet->binding_count;
        for (u32 j = 0; j < reflDescriptorSet->binding_count; j++)
        {
            SpvReflectDescriptorBinding* reflBinding =
                reflDescriptorSet->bindings[j];
            VkDescriptorSetLayoutBinding& layoutBinding =
                descriptorSetLayout.bindings[j];

            layoutBinding.binding = reflBinding->binding;
            layoutBinding.descriptorType =
                static_cast<VkDescriptorType>(reflBinding->descriptor_type);
            layoutBinding.descriptorCount = 1;
            for (u32 k = 0; k < reflBinding->array.dims_count; k++)
            {
                layoutBinding.descriptorCount *= reflBinding->array.dims[k];
            }
            layoutBinding.stageFlags =
                static_cast<VkShaderStageFlagBits>(reflectModule.shader_stage);
            layoutBinding.pImmutableSamplers = nullptr;
        }

        createInfo.bindingCount = reflDescriptorSet->binding_count;
        createInfo.pBindings = descriptorSetLayout.bindings;

        if (vkCreateDescriptorSetLayout(device.logicalDevice, &createInfo,
                                        nullptr,
                                        &descriptorSetLayout.handle) != VK_SUCCESS)
        {
            ArenaPopToMarker(scratch, marker);
            return false;
        }
    }

    ArenaPopToMarker(scratch, marker);

    return true;
}

static bool
CreatePipelineLayout(Device& device,
                     const GraphicsPipelineProgrammableStage& programmableState,
                     VkPipelineLayout& pipelineLayout)
{
    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    VkPipelineLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    u32 totalDescriptorSetCount = 0;
    for (u32 i = 0; i < static_cast<u32>(ShaderType::Count); i++)
    {
        ShaderType shaderType = static_cast<ShaderType>(i);
        totalDescriptorSetCount +=
            programmableState[shaderType].descriptorSetLayoutCount;
    }

    VkDescriptorSetLayout* totalDescriptorSets = nullptr;
    if (totalDescriptorSetCount > 0)
    {

        totalDescriptorSets =
            HLS_ALLOC(arena, VkDescriptorSetLayout, totalDescriptorSetCount);
        u32 k = 0;
        for (u32 i = 0; i < static_cast<u32>(ShaderType::Count); i++)
        {
            ShaderType shaderType = static_cast<ShaderType>(i);
            for (u32 j = 0;
                 j < programmableState[shaderType].descriptorSetLayoutCount;
                 j++)
            {
                totalDescriptorSets[k++] =
                    programmableState[shaderType].descriptorSetLayouts[j].handle;
            }
        }
    }

    createInfo.setLayoutCount = totalDescriptorSetCount;
    createInfo.pSetLayouts = totalDescriptorSets;
    createInfo.pushConstantRangeCount = 0;
    createInfo.pPushConstantRanges = nullptr;

    VkResult res = vkCreatePipelineLayout(device.logicalDevice, &createInfo,
                                          nullptr, &pipelineLayout);
    ArenaPopToMarker(arena, marker);
    return res == VK_SUCCESS;
}

static VkShaderStageFlagBits ShaderTypeToVkShaderStage(ShaderType shaderType)
{
    switch (shaderType)
    {
        case ShaderType::Vertex:
        {
            return VK_SHADER_STAGE_VERTEX_BIT;
        }
        case ShaderType::Fragment:
        {
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        case ShaderType::Geometry:
        {
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        }
        case ShaderType::Task:
        {
            return VK_SHADER_STAGE_TASK_BIT_EXT;
        }
        case ShaderType::Mesh:
        {
            return VK_SHADER_STAGE_MESH_BIT_EXT;
        }
    }
}

bool CreateShaderModule(Arena& arena, Device& device, const char* spvSource,
                        u64 codeSize, ShaderModule& shaderModule)
{
    HLS_ASSERT(spvSource);
    HLS_ASSERT((reinterpret_cast<uintptr_t>(spvSource) % 4) == 0);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = codeSize;
    createInfo.pCode = reinterpret_cast<const u32*>(spvSource);

    if (vkCreateShaderModule(device.logicalDevice, &createInfo, nullptr,
                             &shaderModule.handle) != VK_SUCCESS)
    {
        return false;
    }

    if (!CreateShaderModuleDescriptorSetLayouts(arena, device, spvSource,
                                                codeSize, shaderModule))
    {
        return false;
    }

    return true;
}

void DestroyShaderModule(Device& device, ShaderModule& shaderModule)
{
    for (u32 i = 0; i < shaderModule.descriptorSetLayoutCount; i++)
    {
        vkDestroyDescriptorSetLayout(device.logicalDevice,
                                     shaderModule.descriptorSetLayouts[i].handle,
                                     nullptr);
        shaderModule.descriptorSetLayouts[i].handle = VK_NULL_HANDLE;
    }
    vkDestroyShaderModule(device.logicalDevice, shaderModule.handle, nullptr);
    shaderModule.handle = VK_NULL_HANDLE;
}

void DestroyGraphicsPipelineProgrammableStage(
    Device& device, GraphicsPipelineProgrammableStage& programmableStage)
{
    for (u32 i = 0; i < static_cast<u32>(ShaderType::Count); i++)
    {
        ShaderType shaderType = static_cast<ShaderType>(i);

        ShaderModule& shaderModule = programmableStage[shaderType];

        if (shaderModule.handle != VK_NULL_HANDLE)
        {
            DestroyShaderModule(device, shaderModule);
        }
    }
}

bool CreateGraphicsPipeline(
    Device& device, const GraphicsPipelineFixedStateStage& fixedState,
    const GraphicsPipelineProgrammableStage& programmableState,
    GraphicsPipeline& graphicsPipeline)
{
    HLS_ASSERT(fixedState.colorBlendState.attachmentCount ==
               fixedState.pipelineRendering.colorAttachmentCount);

    // Programmable state
    if (!CreatePipelineLayout(device, programmableState,
                              graphicsPipeline.layout))
    {
        return false;
    }

    u32 stageCount = 0;
    VkPipelineShaderStageCreateInfo stages[ShaderType::Count];

    for (u32 i = 0; i < static_cast<u32>(ShaderType::Count); i++)
    {
        ShaderType shaderType = static_cast<ShaderType>(i);
        VkShaderModule shaderModule = programmableState[shaderType].handle;
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
                                  &createInfo, nullptr,
                                  &graphicsPipeline.handle) != VK_SUCCESS)
    {
        return false;
    }

    return true;
}

void DestroyGraphicsPipeline(Device& device, GraphicsPipeline& graphicsPipeline)
{
    vkDestroyPipeline(device.logicalDevice, graphicsPipeline.handle, nullptr);
    vkDestroyPipelineLayout(device.logicalDevice, graphicsPipeline.layout,
                            nullptr);
    graphicsPipeline.handle = VK_NULL_HANDLE;
    graphicsPipeline.layout = VK_NULL_HANDLE;
}

} // namespace Hls
