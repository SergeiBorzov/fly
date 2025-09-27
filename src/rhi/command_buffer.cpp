#include "core/assert.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "buffer.h"
#include "command_buffer.h"
#include "device.h"
#include "texture.h"

namespace Fly
{
namespace RHI
{

bool CreateCommandBuffers(Device& device, VkCommandPool commandPool,
                          CommandBuffer* commandBuffers, u32 commandBufferCount,
                          bool arePrimary)
{
    FLY_ASSERT(commandBuffers);
    FLY_ASSERT(commandBufferCount > 0);

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
        FLY_PUSH_ARENA(arena, VkCommandBuffer, commandBufferCount);
    VkResult res =
        vkAllocateCommandBuffers(device.logicalDevice, &allocInfo, handles);

    for (u32 i = 0; i < commandBufferCount; i++)
    {
        if (res == VK_SUCCESS)
        {
            commandBuffers[i].handle = handles[i];
            commandBuffers[i].state = CommandBuffer::State::Idle;
            commandBuffers[i].device = &device;
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

void DestroyCommandBuffers(Device& device, CommandBuffer* commandBuffers,
                           VkCommandPool commandPool, u32 commandBufferCount)
{
    FLY_ASSERT(commandBuffers);
    FLY_ASSERT(commandBufferCount > 0);

    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    VkCommandBuffer* handles =
        FLY_PUSH_ARENA(arena, VkCommandBuffer, commandBufferCount);
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
    FLY_ASSERT(commandBuffer.state == CommandBuffer::State::Idle);

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
    FLY_ASSERT(commandBuffer.state == CommandBuffer::State::Recording);

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
                         VkPipelineStageFlags* pipelineStageMasks,
                         const VkSemaphore* signalSemaphores,
                         u32 signalSemaphoreCount, VkFence fence, void* pNext)
{
    FLY_ASSERT(commandBuffer.state == CommandBuffer::State::Recorded);

    VkSubmitInfo submitInfo{};
    submitInfo.pNext = pNext;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = waitSemaphoreCount;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = pipelineStageMasks;
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
    FLY_ASSERT(commandBuffer.state == CommandBuffer::State::Recording);

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

VkRenderingAttachmentInfo ColorAttachmentInfo(VkImageView imageView,
                                              VkAttachmentLoadOp loadOp,
                                              VkAttachmentStoreOp storeOp,
                                              VkClearColorValue clearColor)
{
    VkRenderingAttachmentInfo attachmentInfo{};
    attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachmentInfo.pNext = nullptr;
    attachmentInfo.imageView = imageView;
    attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
    attachmentInfo.loadOp = loadOp;
    attachmentInfo.storeOp = storeOp;
    attachmentInfo.clearValue.color = clearColor;

    return attachmentInfo;
}

VkRenderingAttachmentInfo
DepthAttachmentInfo(VkImageView imageView, VkAttachmentLoadOp loadOp,
                    VkAttachmentStoreOp storeOp,
                    VkClearDepthStencilValue clearDepthStencil)
{
    VkRenderingAttachmentInfo attachmentInfo{};
    attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachmentInfo.pNext = nullptr;
    attachmentInfo.imageView = imageView;
    attachmentInfo.imageLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
    attachmentInfo.loadOp = loadOp;
    attachmentInfo.storeOp = storeOp;
    attachmentInfo.clearValue.depthStencil = clearDepthStencil;

    return attachmentInfo;
}

VkRenderingInfo
RenderingInfo(const VkRect2D& renderArea,
              const VkRenderingAttachmentInfo* colorAttachments,
              u32 colorAttachmentCount,
              const VkRenderingAttachmentInfo* depthAttachment,
              const VkRenderingAttachmentInfo* stencilAttachment,
              u32 layerCount, u32 viewMask)
{
    FLY_ASSERT(colorAttachments);
    FLY_ASSERT(colorAttachmentCount > 0);

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;
    renderInfo.flags = 0;
    renderInfo.viewMask = viewMask;
    renderInfo.layerCount = layerCount;
    renderInfo.renderArea = renderArea;
    renderInfo.colorAttachmentCount = colorAttachmentCount;
    renderInfo.pColorAttachments = colorAttachments;
    renderInfo.pDepthAttachment = depthAttachment;
    renderInfo.pStencilAttachment = stencilAttachment;

    return renderInfo;
}

static void InsertBarriers(RHI::CommandBuffer& cmd,
                           VkPipelineStageFlagBits2 pipelineStageMask,
                           const RecordBufferInput* bufferInput,
                           const RecordTextureInput* textureInput)
{
    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    VkBufferMemoryBarrier2* bufferBarriers = nullptr;
    u32 bufferBarrierCount = 0;

    if (bufferInput && bufferInput->bufferCount > 0)
    {
        bufferBarriers = FLY_PUSH_ARENA(arena, VkBufferMemoryBarrier2,
                                        bufferInput->bufferCount);
        for (u32 i = 0; i < bufferInput->bufferCount; i++)
        {
            RHI::Buffer& buffer = *(bufferInput->buffers[i]);

            // TODO
            if (buffer.pipelineStageMask != pipelineStageMask ||
                buffer.accessMask != bufferInput->bufferAccesses[i])
            {
                VkBufferMemoryBarrier2& barrier =
                    bufferBarriers[bufferBarrierCount];
                barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.offset = 0;
                barrier.size = VK_WHOLE_SIZE;

                barrier.srcAccessMask = buffer.accessMask;
                barrier.dstAccessMask = bufferInput->bufferAccesses[i];
                barrier.srcStageMask = buffer.pipelineStageMask;
                barrier.dstStageMask = pipelineStageMask;
                barrier.buffer = buffer.handle;

                buffer.accessMask = bufferInput->bufferAccesses[i];
                buffer.pipelineStageMask = pipelineStageMask;
                ++bufferBarrierCount;
            }
        }
    }

    VkImageMemoryBarrier2* imageBarriers = nullptr;
    u32 imageBarrierCount = 0;
    if (textureInput && textureInput->textureCount > 0)
    {
        imageBarriers = FLY_PUSH_ARENA(arena, VkImageMemoryBarrier2,
                                       textureInput->textureCount);

        for (u32 i = 0; i < textureInput->textureCount; i++)
        {
            RHI::Texture& texture = *(textureInput->textures[i]);

            // TODO
            if (texture.pipelineStageMask != pipelineStageMask ||
                texture.imageLayout !=
                    textureInput->imageLayoutsAccesses[i].imageLayout ||
                texture.accessMask !=
                    textureInput->imageLayoutsAccesses[i].accessMask)
            {
                VkImageMemoryBarrier2& barrier =
                    imageBarriers[imageBarrierCount];
                barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = texture.image;
                barrier.srcAccessMask = texture.accessMask;
                barrier.dstAccessMask =
                    textureInput->imageLayoutsAccesses[i].accessMask;
                barrier.srcStageMask = texture.pipelineStageMask;
                barrier.dstStageMask = pipelineStageMask;
                barrier.oldLayout = texture.imageLayout;
                barrier.newLayout =
                    textureInput->imageLayoutsAccesses[i].imageLayout;

                barrier.subresourceRange.aspectMask =
                    GetImageAspectMask(texture.format);
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

                texture.accessMask =
                    textureInput->imageLayoutsAccesses[i].accessMask;
                texture.pipelineStageMask = pipelineStageMask;
                texture.imageLayout =
                    textureInput->imageLayoutsAccesses[i].imageLayout;
                ++imageBarrierCount;
            }
        }
    }

    RHI::PipelineBarrier(cmd, bufferBarriers, bufferBarrierCount, imageBarriers,
                         imageBarrierCount);

    ArenaPopToMarker(arena, marker);
}

void ExecuteGraphics(RHI::CommandBuffer& cmd,
                     const VkRenderingInfo& renderingInfo,
                     RecordCallback recordCallback,
                     const RecordBufferInput* bufferInput,
                     const RecordTextureInput* textureInput, void* userData)
{
    InsertBarriers(cmd, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, bufferInput,
                   textureInput);

    vkCmdBeginRendering(cmd.handle, &renderingInfo);
    recordCallback(cmd, bufferInput, textureInput, userData);
    vkCmdEndRendering(cmd.handle);
}

void ExecuteCompute(RHI::CommandBuffer& cmd, RecordCallback recordCallback,
                    const RecordBufferInput* bufferInput,
                    const RecordTextureInput* textureInput, void* userData)
{
    InsertBarriers(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, bufferInput,
                   textureInput);
    recordCallback(cmd, bufferInput, textureInput, userData);
}

void ExecuteComputeIndirect(RHI::CommandBuffer& cmd,
                            RecordCallback recordCallback,
                            const RecordBufferInput* bufferInput,
                            const RecordTextureInput* textureInput,
                            void* userData)
{
    InsertBarriers(cmd,
                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                       VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                   bufferInput, textureInput);
    recordCallback(cmd, bufferInput, textureInput, userData);
}

void ExecuteTransfer(RHI::CommandBuffer& cmd, RecordCallback recordCallback,
                     const RecordBufferInput* bufferInput,
                     const RecordTextureInput* textureInput, void* userData)
{
    InsertBarriers(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, bufferInput,
                   textureInput);
    recordCallback(cmd, bufferInput, textureInput, userData);
}

void ChangeTextureAccessLayout(CommandBuffer& cmd, RHI::Texture& texture,
                               VkImageLayout newLayout,
                               VkAccessFlagBits2 accessMask)
{
    if (texture.imageLayout == newLayout && texture.accessMask == accessMask)
    {
        return;
    }

    RHI::ImageLayoutAccess layoutAccess;
    layoutAccess.imageLayout = newLayout;
    layoutAccess.accessMask = accessMask;

    RHI::RecordTextureInput textureInput;
    RHI::Texture* pTexture = &texture;
    textureInput.textureCount = 1;
    textureInput.textures = &pTexture;
    textureInput.imageLayoutsAccesses = &layoutAccess;

    RHI::InsertBarriers(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, nullptr,
                        &textureInput);
}

} // namespace RHI
} // namespace Fly
