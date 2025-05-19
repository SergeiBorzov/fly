#include "core/assert.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "buffer.h"
#include "command_buffer.h"
#include "device.h"

namespace Hls
{
namespace RHI
{

bool CreateCommandBuffers(const Device& device, VkCommandPool commandPool,
                          CommandBuffer* commandBuffers, u32 commandBufferCount,
                          bool arePrimary)
{
    HLS_ASSERT(commandBuffers);
    HLS_ASSERT(commandBufferCount > 0);

    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    if (arePrimary)
    {
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    }
    else
    {
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    }
    allocInfo.commandBufferCount = commandBufferCount;

    VkCommandBuffer* handles =
        HLS_ALLOC(arena, VkCommandBuffer, commandBufferCount);
    VkResult res =
        vkAllocateCommandBuffers(device.logicalDevice, &allocInfo, handles);

    for (u32 i = 0; i < commandBufferCount; i++)
    {
        if (res == VK_SUCCESS)
        {
            commandBuffers[i].handle = handles[i];
            commandBuffers[i].state = CommandBuffer::State::Idle;
        }
        else
        {
            commandBuffers[i].handle = VK_NULL_HANDLE;
            commandBuffers[i].state = CommandBuffer::State::Invalid;
        }
    }

    ArenaPopToMarker(arena, marker);

    return res == VK_SUCCESS;
}

void DestroyCommandBuffers(const Device& device, CommandBuffer* commandBuffers,
                           VkCommandPool commandPool, u32 commandBufferCount)
{
    HLS_ASSERT(commandBuffers);
    HLS_ASSERT(commandBufferCount > 0);

    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    VkCommandBuffer* handles =
        HLS_ALLOC(arena, VkCommandBuffer, commandBufferCount);
    for (u32 i = 0; i < commandBufferCount; i++)
    {
        handles[i] = commandBuffers[i].handle;
    }

    vkFreeCommandBuffers(device.logicalDevice, commandPool, commandBufferCount,
                         handles);

    for (u32 i = 0; i < commandBufferCount; i++)
    {
        commandBuffers[i].handle = VK_NULL_HANDLE;
        commandBuffers[i].state = CommandBuffer::State::NotAllocated;
    }

    ArenaPopToMarker(arena, marker);
}

bool BeginCommandBuffer(CommandBuffer& commandBuffer, bool singleUse,
                        bool renderPassContinue, bool simultaneousUse)
{
    HLS_ASSERT(commandBuffer.state == CommandBuffer::State::Idle);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    if (singleUse)
    {
        beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    }
    if (renderPassContinue)
    {
        beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    }
    if (simultaneousUse)
    {
        beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    }
    beginInfo.pInheritanceInfo = nullptr;

    VkResult res = vkBeginCommandBuffer(commandBuffer.handle, &beginInfo);

    if (res != VK_SUCCESS)
    {
        commandBuffer.state = CommandBuffer::State::Invalid;
        return false;
    }

    commandBuffer.state = CommandBuffer::State::Recording;
    return true;
}

bool EndCommandBuffer(CommandBuffer& commandBuffer)
{
    HLS_ASSERT(commandBuffer.state == CommandBuffer::State::Recording);

    VkResult res = vkEndCommandBuffer(commandBuffer.handle);
    if (res != VK_SUCCESS)
    {
        commandBuffer.state = CommandBuffer::State::Invalid;
        return false;
    }

    commandBuffer.state = CommandBuffer::State::Recorded;
    return true;
}

bool SubmitCommandBuffer(CommandBuffer& commandBuffer, VkQueue queue,
                         const VkSemaphore* waitSemaphores,
                         u32 waitSemaphoreCount,
                         const VkPipelineStageFlags& waitStageMask,
                         const VkSemaphore* signalSemaphores,
                         u32 signalSemaphoreCount, VkFence fence)
{
    HLS_ASSERT(commandBuffer.state == CommandBuffer::State::Recorded);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = waitSemaphoreCount;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = &waitStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer.handle;
    submitInfo.signalSemaphoreCount = signalSemaphoreCount;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VkResult res = vkQueueSubmit(queue, 1, &submitInfo, fence);
    if (res != VK_SUCCESS)
    {
        commandBuffer.state = CommandBuffer::State::Invalid;
        return false;
    }

    commandBuffer.state = CommandBuffer::State::Submitted;
    return true;
}

bool ResetCommandBuffer(CommandBuffer& commandBuffer, bool releaseResources)
{
    VkCommandBufferResetFlagBits flags =
        static_cast<VkCommandBufferResetFlagBits>(0);

    if (releaseResources)
    {
        flags = VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT;
    }

    VkResult res = vkResetCommandBuffer(commandBuffer.handle, flags);

    if (res != VK_SUCCESS)
    {
        commandBuffer.state = CommandBuffer::State::Invalid;
        return false;
    }
    commandBuffer.state = CommandBuffer::State::Idle;
    return true;
}

static VkImageSubresourceRange
ImageSubresourceRange(VkImageAspectFlags aspectMask)
{
    VkImageSubresourceRange subImage{};
    subImage.aspectMask = aspectMask;
    subImage.baseMipLevel = 0;
    subImage.levelCount = VK_REMAINING_MIP_LEVELS;
    subImage.baseArrayLayer = 0;
    subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;

    return subImage;
}

void RecordTransitionImageLayout(CommandBuffer& commandBuffer, VkImage image,
                                 VkImageLayout currentLayout,
                                 VkImageLayout newLayout)
{
    HLS_ASSERT(commandBuffer.state == CommandBuffer::State::Recording);

    VkImageAspectFlags aspectMask =
        (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
            : VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageMemoryBarrier2 imageBarrier{};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    imageBarrier.pNext = nullptr;
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.dstAccessMask =
        VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
    imageBarrier.oldLayout = currentLayout;
    imageBarrier.newLayout = newLayout;
    imageBarrier.subresourceRange = ImageSubresourceRange(aspectMask);
    imageBarrier.image = image;

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.pNext = nullptr;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(commandBuffer.handle, &dependencyInfo);
}

void RecordClearColor(CommandBuffer& commandBuffer, VkImage image, f32 r, f32 g,
                      f32 b, f32 a)
{
    HLS_ASSERT(commandBuffer.state == CommandBuffer::State::Recording);

    VkClearColorValue clearValue;
    clearValue = {{r, g, b, a}};
    VkImageSubresourceRange clearRange =
        ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
    vkCmdClearColorImage(commandBuffer.handle, image, VK_IMAGE_LAYOUT_GENERAL,
                         &clearValue, 1, &clearRange);
}

VkRenderingAttachmentInfo ColorAttachmentInfo(VkImageView imageView,
                                              VkImageLayout imageLayout)
{
    VkRenderingAttachmentInfo attachmentInfo{};
    attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachmentInfo.pNext = nullptr;
    attachmentInfo.imageView = imageView;
    attachmentInfo.imageLayout = imageLayout;
    attachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
    attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentInfo.clearValue = {{0.0f, 0.0f, 0.0f, 1.0f}};

    return attachmentInfo;
}

VkRenderingAttachmentInfo DepthAttachmentInfo(VkImageView imageView,
                                              VkImageLayout imageLayout)
{
    VkRenderingAttachmentInfo attachmentInfo{};
    attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachmentInfo.pNext = nullptr;
    attachmentInfo.imageView = imageView;
    attachmentInfo.imageLayout = imageLayout;
    attachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
    attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentInfo.clearValue.depthStencil.depth = 1.0f;
    attachmentInfo.clearValue.depthStencil.stencil = 0;

    return attachmentInfo;
}

VkRenderingInfo
RenderingInfo(const VkRect2D& renderArea,
              const VkRenderingAttachmentInfo* colorAttachments,
              u32 colorAttachmentCount,
              const VkRenderingAttachmentInfo* depthAttachment,
              const VkRenderingAttachmentInfo* stencilAttachment)
{
    HLS_ASSERT(colorAttachments);
    HLS_ASSERT(colorAttachmentCount > 0);

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;
    renderInfo.flags = 0;
    renderInfo.viewMask = 0;
    renderInfo.layerCount =
        1; // will be different for texture arrays and cube maps
    renderInfo.renderArea = renderArea;
    renderInfo.colorAttachmentCount = colorAttachmentCount;
    renderInfo.pColorAttachments = colorAttachments;
    renderInfo.pDepthAttachment = depthAttachment;
    renderInfo.pStencilAttachment = stencilAttachment;

    return renderInfo;
}

VkBufferMemoryBarrier BufferMemoryBarrier(const RHI::Buffer& buffer,
                                          VkAccessFlags srcAccessMask,
                                          VkAccessFlags dstAccessMask,
                                          u64 offset, u64 size)
{
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer.handle;
    barrier.offset = offset;
    barrier.size = size;

    return barrier;
}

} // namespace RHI
} // namespace Hls
