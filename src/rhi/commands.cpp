#include "core/assert.h"

#include "buffer.h"
#include "command_buffer.h"
#include "device.h"
#include "pipeline.h"
#include "texture.h"

namespace Fly
{
namespace RHI
{

void BindGraphicsPipeline(CommandBuffer& cmd,
                          const RHI::GraphicsPipeline& graphicsPipeline)
{
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);
    FLY_ASSERT(graphicsPipeline.handle != VK_NULL_HANDLE);

    vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      graphicsPipeline.handle);
    vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            cmd.device->pipelineLayout, 0, 1,
                            &(cmd.device->bindlessDescriptorSet), 0, nullptr);
}

void BindComputePipeline(CommandBuffer& cmd,
                         const RHI::ComputePipeline& computePipeline)
{
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);
    FLY_ASSERT(computePipeline.handle != VK_NULL_HANDLE);

    vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                      computePipeline.handle);
    vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                            cmd.device->pipelineLayout, 0, 1,
                            &(cmd.device->bindlessDescriptorSet), 0, nullptr);
}

void BindIndexBuffer(CommandBuffer& cmd, RHI::Buffer& buffer,
                     VkIndexType indexType, u64 offset)
{
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);
    FLY_ASSERT(buffer.handle != VK_NULL_HANDLE);
    FLY_ASSERT(buffer.usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    vkCmdBindIndexBuffer(cmd.handle, buffer.handle, offset, indexType);
}

void SetViewport(CommandBuffer& cmd, f32 x, f32 y, f32 w, f32 h, f32 minDepth,
                 f32 maxDepth)
{
    FLY_ASSERT(cmd.device);
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);

    VkViewport viewport;
    viewport.x = x;
    viewport.y = y;
    viewport.width = w;
    viewport.height = h;
    viewport.minDepth = minDepth;
    viewport.maxDepth = maxDepth;
    vkCmdSetViewport(cmd.handle, 0, 1, &viewport);
}

void SetScissor(CommandBuffer& cmd, i32 x, i32 y, u32 w, u32 h)
{
    FLY_ASSERT(cmd.device);
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);

    VkRect2D scissorRect = {{x, y}, {w, h}};
    vkCmdSetScissor(cmd.handle, 0, 1, &scissorRect);
}

void ClearColor(CommandBuffer& cmd, RHI::Texture2D& texture2D, f32 r, f32 g,
                f32 b, f32 a)
{
    FLY_ASSERT(cmd.device);
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);

    VkClearColorValue clearValue;
    clearValue = {{r, g, b, a}};

    VkImageSubresourceRange clearRange{};
    clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clearRange.baseMipLevel = 0;
    clearRange.levelCount = VK_REMAINING_MIP_LEVELS;
    clearRange.baseArrayLayer = 0;
    clearRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    vkCmdClearColorImage(cmd.handle, texture2D.image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1,
                         &clearRange);
}

void PushConstants(CommandBuffer& cmd, const void* pushConstants,
                   u32 pushConstantsSize, u32 offset)
{
    FLY_ASSERT(cmd.device);
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);

    vkCmdPushConstants(cmd.handle, cmd.device->pipelineLayout,
                       VK_SHADER_STAGE_ALL, offset, pushConstantsSize,
                       pushConstants);
}

void Dispatch(CommandBuffer& cmd, u32 groupCountX, u32 groupCountY,
              u32 groupCountZ)
{
    FLY_ASSERT(cmd.device);
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);

    vkCmdDispatch(cmd.handle, groupCountX, groupCountY, groupCountZ);
}

void Draw(CommandBuffer& cmd, u32 vertexCount, u32 instanceCount,
          u32 firstVertex, u32 firstInstance)
{
    FLY_ASSERT(cmd.device);
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);

    vkCmdDraw(cmd.handle, vertexCount, instanceCount, firstVertex,
              firstInstance);
}

void DrawIndexed(CommandBuffer& cmd, u32 indexCount, u32 instanceCount,
                 u32 firstIndex, u32 vertexOffset, u32 firstInstance)
{
    FLY_ASSERT(cmd.device);
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);

    vkCmdDrawIndexed(cmd.handle, indexCount, instanceCount, firstIndex,
                     vertexOffset, firstInstance);
}

void FillBuffer(CommandBuffer& cmd, Buffer& buffer, u32 value, u64 size,
                u64 offset)
{
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);
    FLY_ASSERT(buffer.handle != VK_NULL_HANDLE);
    FLY_ASSERT(buffer.usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    if (size == 0)
    {
        size = VK_WHOLE_SIZE;
    }
    vkCmdFillBuffer(cmd.handle, buffer.handle, offset, size, value);
}

void PipelineBarrier(CommandBuffer& cmd,
                     const VkBufferMemoryBarrier2* bufferBarriers,
                     u32 bufferBarrierCount,
                     const VkImageMemoryBarrier2* imageBarriers,
                     u32 imageBarrierCount)
{
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);

    if (bufferBarriers == nullptr && imageBarriers == nullptr)
    {
        return;
    }

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.pBufferMemoryBarriers = bufferBarriers;
    dependencyInfo.bufferMemoryBarrierCount = bufferBarrierCount;
    dependencyInfo.pImageMemoryBarriers = imageBarriers;
    dependencyInfo.imageMemoryBarrierCount = imageBarrierCount;

    vkCmdPipelineBarrier2(cmd.handle, &dependencyInfo);
}

} // namespace RHI
} // namespace Fly
