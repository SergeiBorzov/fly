#ifndef FLY_RHI_PIPELINE_H
#define FLY_RHI_PIPELINE_H

#include <volk.h>

#include "buffer.h"
#include "core/types.h"

#define FLY_GRAPHICS_PIPELINE_COLOR_ATTACHMENT_MAX_COUNT 8

namespace Fly
{
namespace RHI
{

struct Device;
struct Shader;
struct ShaderProgram;

struct GraphicsPipelineFixedStateStage
{
    struct
    {
        VkPipelineColorBlendAttachmentState
            attachments[FLY_GRAPHICS_PIPELINE_COLOR_ATTACHMENT_MAX_COUNT] = {
                {VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO,
                 VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO,
                 VK_BLEND_OP_ADD,
                 VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT}};
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
        VkCompareOp depthCompareOp = VK_COMPARE_OP_GREATER;
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
            colorAttachments[FLY_GRAPHICS_PIPELINE_COLOR_ATTACHMENT_MAX_COUNT] =
                {VK_FORMAT_UNDEFINED};
        u32 colorAttachmentCount = 0;
        u32 viewMask = 0;
    } pipelineRendering;
};

struct GraphicsPipeline
{
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline handle = VK_NULL_HANDLE;
};

bool CreateGraphicsPipeline(
    Device& device, const GraphicsPipelineFixedStateStage& fixedStateStage,
    const ShaderProgram& shaderProgram, GraphicsPipeline& graphicsPipeline);

void DestroyGraphicsPipeline(Device& device,
                             GraphicsPipeline& graphicsPipeline);

struct ComputePipeline
{
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline handle = VK_NULL_HANDLE;
};
bool CreateComputePipeline(Device& device, const Shader& computeShader,
                           ComputePipeline& computePipeline);
void DestroyComputePipeline(Device& device, ComputePipeline& computePipeline);

struct RayTracingGroup
{
    VkRayTracingShaderGroupTypeKHR type =
        VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    u32 generalShader = VK_SHADER_UNUSED_KHR;
    u32 closestHitShader = VK_SHADER_UNUSED_KHR;
    u32 anyHitShader = VK_SHADER_UNUSED_KHR;
    u32 intersectionShader = VK_SHADER_UNUSED_KHR;
};

RayTracingGroup GeneralRayTracingGroup(u32 generalShaderIndex);
RayTracingGroup
ProceduralHitRayTracingGroup(u32 intersectionShaderIndex,
                             u32 closestHitShaderIndex = VK_SHADER_UNUSED_KHR,
                             u32 anyHitShaderIndex = VK_SHADER_UNUSED_KHR);
RayTracingGroup TriangleHitGroup(u32 closestHitShaderIndex,
                                 u32 anyHitShaderIndex = VK_SHADER_UNUSED_KHR);

struct RayTracingPipeline
{
    Buffer sbtBuffer;
    VkStridedDeviceAddressRegionKHR rayGenRegion{};
    VkStridedDeviceAddressRegionKHR missRegion{};
    VkStridedDeviceAddressRegionKHR hitRegion{};
    VkStridedDeviceAddressRegionKHR callRegion{};
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline handle = VK_NULL_HANDLE;
    u32 sbtStride = 0;
};
bool CreateRayTracingPipeline(Device& device, u32 maxRecursionDepth,
                              const Shader* shaders, u32 shaderCount,
                              const RayTracingGroup* groups, u32 groupCount,
                              RayTracingPipeline& rayTracingPipeline);
void DestroyRayTracingPipeline(Device& device,
                               RayTracingPipeline& rayTracingPipeline);

} // namespace RHI
} // namespace Fly

#endif /* FLY_RHI_PIPELINE_H */
