#include "core/assert.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "allocation_callbacks.h"
#include "device.h"
#include "pipeline.h"
#include "shader_program.h"

namespace Fly
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
        case Shader::Type::RayGeneration:
        {
            return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        }
        case Shader::Type::RayIntersection:
        {
            return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        }
        case Shader::Type::RayAnyHit:
        {
            return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        }
        case Shader::Type::RayClosestHit:
        {
            return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        }
        case Shader::Type::RayMiss:
        {
            return VK_SHADER_STAGE_MISS_BIT_KHR;
        }
        case Shader::Type::RayCall:
        {
            return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        }
        default:
        {
            FLY_ASSERT(false);
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
    FLY_ASSERT(colorBlendAttachmentCount <=
               FLY_GRAPHICS_PIPELINE_COLOR_ATTACHMENT_MAX_COUNT);
    FLY_ASSERT(blendConstants);
    FLY_ASSERT(blendConstantCount == 4);

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

static VkPipelineRenderingCreateInfo
PipelineRenderingCreateInfo(const VkFormat* colorAttachments,
                            u32 colorAttachmentCount,
                            VkFormat depthAttachmentFormat,
                            VkFormat stencilAttachmentFormat, u32 viewMask)
{
    FLY_ASSERT(colorAttachmentCount <=
               FLY_GRAPHICS_PIPELINE_COLOR_ATTACHMENT_MAX_COUNT);

    VkPipelineRenderingCreateInfo pipelineRendering;

    pipelineRendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRendering.pNext = nullptr;
    pipelineRendering.viewMask = viewMask;
    pipelineRendering.pColorAttachmentFormats = colorAttachments;
    pipelineRendering.colorAttachmentCount = colorAttachmentCount;
    pipelineRendering.depthAttachmentFormat = depthAttachmentFormat;
    pipelineRendering.stencilAttachmentFormat = stencilAttachmentFormat;

    return pipelineRendering;
}

bool CreateGraphicsPipeline(Device& device,
                            const GraphicsPipelineFixedStateStage& fixedState,
                            const ShaderProgram& shaderProgram,
                            GraphicsPipeline& graphicsPipeline)
{
    FLY_ASSERT(device.logicalDevice != VK_NULL_HANDLE);
    FLY_ASSERT(fixedState.colorBlendState.attachmentCount ==
               fixedState.pipelineRendering.colorAttachmentCount);

    graphicsPipeline.layout = device.pipelineLayout;

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
    FLY_ASSERT(stageCount > 0);

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
            fixedState.pipelineRendering.stencilAttachmentFormat,
            fixedState.pipelineRendering.viewMask);

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
    FLY_ASSERT(graphicsPipeline.layout != VK_NULL_HANDLE);
    FLY_ASSERT(graphicsPipeline.handle != VK_NULL_HANDLE);

    vkDestroyPipeline(device.logicalDevice, graphicsPipeline.handle,
                      GetVulkanAllocationCallbacks());
    graphicsPipeline.handle = VK_NULL_HANDLE;
    graphicsPipeline.layout = VK_NULL_HANDLE;
}

bool CreateComputePipeline(Device& device, const Shader& computeShader,
                           ComputePipeline& computePipeline)
{
    FLY_ASSERT(device.logicalDevice != VK_NULL_HANDLE);
    computePipeline.layout = device.pipelineLayout;

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
    FLY_ASSERT(device.logicalDevice != VK_NULL_HANDLE);
    FLY_ASSERT(computePipeline.layout != VK_NULL_HANDLE);
    FLY_ASSERT(computePipeline.handle != VK_NULL_HANDLE);

    vkDestroyPipeline(device.logicalDevice, computePipeline.handle,
                      GetVulkanAllocationCallbacks());
    computePipeline.handle = VK_NULL_HANDLE;
    computePipeline.layout = VK_NULL_HANDLE;
}

RayTracingGroup GeneralRayTracingGroup(u32 generalShaderIndex)
{
    RayTracingGroup group{};
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.generalShader = generalShaderIndex;
    group.closestHitShader = VK_SHADER_UNUSED_KHR;
    group.anyHitShader = VK_SHADER_UNUSED_KHR;
    group.intersectionShader = VK_SHADER_UNUSED_KHR;
    return group;
}

RayTracingGroup ProceduralHitRayTracingGroup(u32 intersectionShaderIndex,
                                             u32 closestHitShaderIndex,
                                             u32 anyHitShaderIndex)
{
    RayTracingGroup group{};
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
    group.generalShader = VK_SHADER_UNUSED_KHR;
    group.closestHitShader = closestHitShaderIndex;
    group.anyHitShader = anyHitShaderIndex;
    group.intersectionShader = intersectionShaderIndex;
    return group;
}

RayTracingGroup TriangleHitRayTracingGroup(u32 closestHitShaderIndex,
                                           u32 anyHitShaderIndex)
{
    RayTracingGroup group{};
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    group.generalShader = VK_SHADER_UNUSED_KHR;
    group.closestHitShader = closestHitShaderIndex;
    group.anyHitShader = anyHitShaderIndex;
    group.intersectionShader = VK_SHADER_UNUSED_KHR;
    return group;
}

bool CreateRayTracingPipeline(Device& device, u32 maxRecursionDepth,
                              const Shader* shaders, u32 shaderCount,
                              const RayTracingGroup* groups, u32 groupCount,
                              RayTracingPipeline& rayTracingPipeline)

{
    FLY_ASSERT(maxRecursionDepth >= 1);
    FLY_ASSERT(device.logicalDevice != VK_NULL_HANDLE);
    FLY_ASSERT(groups);
    FLY_ASSERT(groupCount >= 0);

    rayTracingPipeline.layout = device.pipelineLayout;

    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    VkPipelineShaderStageCreateInfo* stages =
        FLY_PUSH_ARENA(arena, VkPipelineShaderStageCreateInfo, shaderCount);
    for (u32 i = 0; i < shaderCount; i++)
    {
        stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[i].pNext = nullptr;
        stages[i].flags = 0;
        stages[i].stage = ShaderTypeToVkShaderStage(shaders[i].type);
        stages[i].module = shaders[i].handle;
        stages[i].pName = "main";
        stages[i].pSpecializationInfo = nullptr;
    }

    VkRayTracingShaderGroupCreateInfoKHR* groupCreateInfos =
        FLY_PUSH_ARENA(arena, VkRayTracingShaderGroupCreateInfoKHR, groupCount);
    for (u32 i = 0; i < groupCount; i++)
    {
        groupCreateInfos[i].sType =
            VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        groupCreateInfos[i].pNext = nullptr;
        groupCreateInfos[i].type = groups[i].type;
        groupCreateInfos[i].generalShader = groups[i].generalShader;
        groupCreateInfos[i].closestHitShader = groups[i].closestHitShader;
        groupCreateInfos[i].anyHitShader = groups[i].anyHitShader;
        groupCreateInfos[i].intersectionShader = groups[i].intersectionShader;
        groupCreateInfos[i].pShaderGroupCaptureReplayHandle = nullptr;
    }

    VkRayTracingPipelineCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    createInfo.stageCount = shaderCount;
    createInfo.pStages = stages;
    createInfo.groupCount = groupCount;
    createInfo.pGroups = groupCreateInfos;
    createInfo.maxPipelineRayRecursionDepth = maxRecursionDepth;
    createInfo.layout = device.pipelineLayout;

    if (vkCreateRayTracingPipelinesKHR(
            device.logicalDevice, VK_NULL_HANDLE, VK_NULL_HANDLE, 1,
            &createInfo, GetVulkanAllocationCallbacks(),
            &rayTracingPipeline.handle) != VK_SUCCESS)
    {
        return false;
    }

    ArenaPopToMarker(arena, marker);
    return true;
}

void DestroyRayTracingPipeline(Device& device,
                               RayTracingPipeline& rayTracingPipeline)
{
    FLY_ASSERT(device.logicalDevice != VK_NULL_HANDLE);
    FLY_ASSERT(rayTracingPipeline.layout != VK_NULL_HANDLE);
    FLY_ASSERT(rayTracingPipeline.handle != VK_NULL_HANDLE);

    vkDestroyPipeline(device.logicalDevice, rayTracingPipeline.handle,
                      GetVulkanAllocationCallbacks());
    rayTracingPipeline.handle = VK_NULL_HANDLE;
    rayTracingPipeline.layout = VK_NULL_HANDLE;
}

} // namespace RHI
} // namespace Fly
