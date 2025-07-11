#include "core/assert.h"

#include "command_buffer.h"
#include "device.h"
#include "texture.h"

namespace Fly
{
namespace RHI
{

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

void Draw(CommandBuffer& cmd, u32 vertexCount, u32 instanceCount,
          u32 firstVertex, u32 firstInstance)
{
    FLY_ASSERT(cmd.device);
    FLY_ASSERT(cmd.state == CommandBuffer::State::Recording);

    vkCmdDraw(cmd.handle, vertexCount, instanceCount, firstVertex,
              firstInstance);
}

} // namespace RHI
} // namespace Fly
