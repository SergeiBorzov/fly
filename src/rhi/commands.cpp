#include "core/assert.h"

#include "buffer.h"
#include "command_buffer.h"
#include "device.h"
#include "pipeline.h"
#include "texture.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

namespace Fly
{
namespace RHI
{

void GenerateMipmaps(CommandBuffer& cmd, Texture& texture)
{
    i32 mipWidth = static_cast<i32>(texture.width);
    i32 mipHeight = static_cast<i32>(texture.height);

    RHI::ChangeTextureAccessLayout(cmd, texture,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   VK_ACCESS_2_TRANSFER_WRITE_BIT);

    VkImageMemoryBarrier2 imageBarriers[2];
    for (u32 i = 0; i < 2; i++)
    {
        imageBarriers[i] = {};
        imageBarriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        imageBarriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageBarriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageBarriers[i].subresourceRange.aspectMask =
            GetImageAspectMask(texture.format);
        imageBarriers[i].subresourceRange.baseArrayLayer = 0;
        imageBarriers[i].subresourceRange.layerCount = texture.layerCount;
        imageBarriers[i].subresourceRange.levelCount = 1;
        imageBarriers[i].image = texture.image;
    }

    VkAccessFlags2 srcAccessMask = texture.accessMask;
    VkPipelineStageFlags2 srcPipelineStageMask = texture.pipelineStageMask;

    VkAccessFlags2 dstAccessMask = texture.accessMask;
    VkPipelineStageFlags2 dstPipelineStageMask = texture.pipelineStageMask;

    for (u32 i = 1; i < texture.mipCount; i++)
    {
        imageBarriers[0].srcAccessMask = srcAccessMask;
        imageBarriers[0].dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        imageBarriers[0].srcStageMask = srcPipelineStageMask;
        imageBarriers[0].dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
        imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imageBarriers[0].subresourceRange.baseMipLevel = i - 1;

        imageBarriers[1].srcAccessMask = dstAccessMask;
        imageBarriers[1].dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        imageBarriers[1].srcStageMask = dstPipelineStageMask;
        imageBarriers[1].dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
        imageBarriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarriers[1].subresourceRange.baseMipLevel = i;

        PipelineBarrier(cmd, nullptr, 0, imageBarriers, 1);
        srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        srcPipelineStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = GetImageAspectMask(texture.format);
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = texture.layerCount;

        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1,
                              mipHeight > 1 ? mipHeight / 2 : 1, 1};
        blit.dstSubresource.aspectMask = GetImageAspectMask(texture.format);
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = texture.layerCount;

        vkCmdBlitImage(cmd.handle, texture.image,
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture.image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                       VK_FILTER_LINEAR);

        if (mipWidth > 1)
        {
            mipWidth /= 2;
        }
        if (mipHeight > 1)
        {
            mipHeight /= 2;
        }
    }

    texture.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    texture.accessMask =
        VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT;
    RHI::ChangeTextureAccessLayout(cmd, texture,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                   VK_ACCESS_2_SHADER_READ_BIT);
}

void CopyTextureToBuffer(CommandBuffer& cmd, Buffer& dstBuffer,
                         const Texture& srcTexture, u32 mipLevel)
{
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);
    FLY_ASSERT(mipLevel < srcTexture.mipCount);

    u32 width = MAX(srcTexture.width >> mipLevel, 1);
    u32 height = MAX(srcTexture.height >> mipLevel, 1);
    u32 depth = MAX(srcTexture.depth >> mipLevel, 1);

    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource.aspectMask =
        GetImageAspectMask(srcTexture.format);
    copyRegion.imageSubresource.mipLevel = mipLevel;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = srcTexture.layerCount;
    copyRegion.imageExtent.width = width;
    copyRegion.imageExtent.height = height;
    copyRegion.imageExtent.depth = depth;

    vkCmdCopyImageToBuffer(cmd.handle, srcTexture.image, srcTexture.imageLayout,
                           dstBuffer.handle, 1, &copyRegion);
}

void CopyBufferToTexture(CommandBuffer& cmd, Texture& dstTexture,
                         Buffer& srcBuffer, u32 mipLevel)
{
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);
    FLY_ASSERT(mipLevel < dstTexture.mipCount);

    u32 width = MAX(dstTexture.width >> mipLevel, 1);
    u32 height = MAX(dstTexture.height >> mipLevel, 1);
    u32 depth = MAX(dstTexture.depth >> mipLevel, 1);

    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource.aspectMask =
        GetImageAspectMask(dstTexture.format);
    copyRegion.imageSubresource.mipLevel = mipLevel;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = dstTexture.layerCount;
    copyRegion.imageExtent.width = width;
    copyRegion.imageExtent.height = height;
    copyRegion.imageExtent.depth = depth;

    vkCmdCopyBufferToImage(cmd.handle, srcBuffer.handle, dstTexture.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copyRegion);
}

void BindGraphicsPipeline(CommandBuffer& cmd,
                          const GraphicsPipeline& graphicsPipeline)
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
                         const ComputePipeline& computePipeline)
{
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);
    FLY_ASSERT(computePipeline.handle != VK_NULL_HANDLE);

    vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                      computePipeline.handle);
    vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                            cmd.device->pipelineLayout, 0, 1,
                            &(cmd.device->bindlessDescriptorSet), 0, nullptr);
}

void BindIndexBuffer(CommandBuffer& cmd, Buffer& buffer, VkIndexType indexType,
                     u64 offset)
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

void ClearColor(CommandBuffer& cmd, Texture& texture, f32 r, f32 g, f32 b,
                f32 a)
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

    vkCmdClearColorImage(cmd.handle, texture.image,
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

void DispatchIndirect(CommandBuffer& cmd, const Buffer& buffer, u64 offset)
{
    FLY_ASSERT(cmd.device);
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);

    vkCmdDispatchIndirect(cmd.handle, buffer.handle, offset);
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

void DrawIndirectCount(CommandBuffer& cmd, const Buffer& indirectDrawBuffer,
                       u64 offset, const Buffer& indirectCountBuffer,
                       u64 countOffset, u32 maxCount, u32 stride)
{
    FLY_ASSERT(cmd.device);
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);
    vkCmdDrawIndirectCount(cmd.handle, indirectDrawBuffer.handle, offset,
                           indirectCountBuffer.handle, countOffset, maxCount,
                           stride);
}

void DrawIndexedIndirectCount(CommandBuffer& cmd,
                              const Buffer& indirectDrawBuffer, u64 offset,
                              const Buffer& indirectCountBuffer,
                              u64 countOffset, u32 maxCount, u32 stride)
{
    FLY_ASSERT(cmd.device);
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);
    vkCmdDrawIndexedIndirectCount(cmd.handle, indirectDrawBuffer.handle, offset,
                                  indirectCountBuffer.handle, countOffset,
                                  maxCount, stride);
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

void ResetQueryPool(CommandBuffer& cmd, VkQueryPool queryPool, u32 firstQuery,
                    u32 queryCount)
{
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);
    vkCmdResetQueryPool(cmd.handle, queryPool, firstQuery, queryCount);
}

void WriteTimestamp(CommandBuffer& cmd, VkPipelineStageFlagBits pipelineStage,
                    VkQueryPool queryPool, u32 queryIndex)
{
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);
    vkCmdWriteTimestamp(cmd.handle, pipelineStage, queryPool, queryIndex);
}

} // namespace RHI
} // namespace Fly
