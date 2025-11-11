#include "core/assert.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "buffer.h"
#include "command_buffer.h"
#include "device.h"
#include "texture.h"

#define WRITE_MASK                                                             \
    (VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |   \
     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |                          \
     VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_HOST_WRITE_BIT |             \
     VK_ACCESS_2_MEMORY_WRITE_BIT |                                            \
     VK_ACCESS_2_TRANSFORM_FEEDBACK_WRITE_BIT_EXT |                            \
     VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR)

#define DEVICE_READ_MASK                                                       \
    (VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_UNIFORM_READ_BIT |              \
     VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_2_TRANSFER_READ_BIT |   \
     VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT |     \
     VK_ACCESS_2_INDEX_READ_BIT | VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT |      \
     VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR |                         \
     VK_ACCESS_2_CONDITIONAL_RENDERING_READ_BIT_EXT)

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

void RecordTransitionImageLayout(CommandBuffer& commandBuffer, Texture& texture,
                                 VkImageLayout currentLayout,
                                 VkImageLayout newLayout)
{
    FLY_ASSERT(commandBuffer.state == CommandBuffer::State::Recording);

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

    imageBarrier.subresourceRange.baseMipLevel = 0;
    imageBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    imageBarrier.subresourceRange.baseArrayLayer = 0;
    imageBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    imageBarrier.subresourceRange.aspectMask =
        GetImageAspectMask(texture.format);

    imageBarrier.image = texture.image;

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.pNext = nullptr;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(commandBuffer.handle, &dependencyInfo);

    texture.imageLayout = newLayout;
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

static bool IsReadAfterRead(VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess)
{
    if ((srcAccess & WRITE_MASK) || (dstAccess & WRITE_MASK))
    {
        return false;
    }

    bool srcIsDeviceRead =
        (srcAccess & DEVICE_READ_MASK) && !(srcAccess & ~DEVICE_READ_MASK);

    bool dstIsHostRead = (dstAccess & VK_ACCESS_2_HOST_READ_BIT) &&
                         !(dstAccess & ~VK_ACCESS_2_HOST_READ_BIT);

    if (srcIsDeviceRead && dstIsHostRead)
    {
        return false;
    }

    return true;
}

static void InsertBarriers(RHI::CommandBuffer& cmd,
                           VkPipelineStageFlagBits2 pipelineStageMask,
                           const RecordBufferInput* bufferInput,
                           u32 bufferInputCount,
                           const RecordTextureInput* textureInput,
                           u32 textureInputCount)
{
    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    VkBufferMemoryBarrier2* bufferBarriers = nullptr;
    u32 bufferBarrierCount = 0;

    if (bufferInput && bufferInputCount > 0)
    {
        bufferBarriers =
            FLY_PUSH_ARENA(arena, VkBufferMemoryBarrier2, bufferInputCount);
        for (u32 i = 0; i < bufferInputCount; i++)
        {
            RHI::Buffer& buffer = *(bufferInput[i].pBuffer);

            if (buffer.pipelineStageMask != pipelineStageMask ||
                !IsReadAfterRead(buffer.accessMask, bufferInput[i].accessMask))
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
                barrier.dstAccessMask = bufferInput[i].accessMask;
                barrier.srcStageMask = buffer.pipelineStageMask;
                barrier.dstStageMask = pipelineStageMask;
                barrier.buffer = buffer.handle;

                buffer.accessMask = bufferInput[i].accessMask;
                buffer.pipelineStageMask = pipelineStageMask;
                ++bufferBarrierCount;
            }
        }
    }

    VkImageMemoryBarrier2* imageBarriers = nullptr;
    u32 imageBarrierCount = 0;
    if (textureInput && textureInputCount > 0)
    {
        imageBarriers =
            FLY_PUSH_ARENA(arena, VkImageMemoryBarrier2, textureInputCount);

        for (u32 i = 0; i < textureInputCount; i++)
        {
            RHI::Texture& texture = *(textureInput[i].pTexture);

            // TODO
            if (texture.pipelineStageMask != pipelineStageMask ||
                texture.imageLayout != textureInput[i].imageLayout ||
                !IsReadAfterRead(texture.accessMask,
                                 textureInput[i].accessMask))
            {
                VkImageMemoryBarrier2& barrier =
                    imageBarriers[imageBarrierCount];
                barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = texture.image;

                if (textureInput[i].baseMipLevel != 0 ||
                    textureInput[i].levelCount != VK_REMAINING_MIP_LEVELS)
                {
                    barrier.srcAccessMask = textureInput[i].srcAccessMask;
                    barrier.oldLayout = textureInput[i].srcImageLayout;
                }
                else
                {
                    barrier.srcAccessMask = texture.accessMask;
                    barrier.oldLayout = texture.imageLayout;
                }
                barrier.srcStageMask = texture.pipelineStageMask;

                barrier.dstAccessMask = textureInput[i].accessMask;
                barrier.dstStageMask = pipelineStageMask;
                barrier.newLayout = textureInput[i].imageLayout;

                barrier.subresourceRange.aspectMask =
                    GetImageAspectMask(texture.format);
                barrier.subresourceRange.baseMipLevel =
                    textureInput[i].baseMipLevel;
                barrier.subresourceRange.levelCount =
                    textureInput[i].levelCount;
                barrier.subresourceRange.baseArrayLayer =
                    textureInput[i].baseArrayLayer;
                barrier.subresourceRange.layerCount =
                    textureInput[i].layerCount;

                if (textureInput[i].baseMipLevel == 0)
                {
                    texture.accessMask = textureInput[i].accessMask;
                    texture.pipelineStageMask = pipelineStageMask;
                    texture.imageLayout = textureInput[i].imageLayout;
                }
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
                     const RecordBufferInput* bufferInput, u32 bufferInputCount,
                     const RecordTextureInput* textureInput,
                     u32 textureInputCount, void* userData)
{
    InsertBarriers(cmd, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, bufferInput,
                   bufferInputCount, textureInput, textureInputCount);

    vkCmdBeginRendering(cmd.handle, &renderingInfo);
    recordCallback(cmd, bufferInput, bufferInputCount, textureInput,
                   textureInputCount, userData);
    vkCmdEndRendering(cmd.handle);
}

void ExecuteCompute(RHI::CommandBuffer& cmd, RecordCallback recordCallback,
                    const RecordBufferInput* bufferInput, u32 bufferInputCount,
                    const RecordTextureInput* textureInput,
                    u32 textureInputCount, void* userData)
{
    InsertBarriers(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, bufferInput,
                   bufferInputCount, textureInput, textureInputCount);
    recordCallback(cmd, bufferInput, bufferInputCount, textureInput,
                   textureInputCount, userData);
}

void ExecuteComputeIndirect(RHI::CommandBuffer& cmd,
                            RecordCallback recordCallback,
                            const RecordBufferInput* bufferInput,
                            u32 bufferInputCount,
                            const RecordTextureInput* textureInput,
                            u32 textureInputCount, void* userData)
{
    InsertBarriers(cmd,
                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                       VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                   bufferInput, bufferInputCount, textureInput,
                   textureInputCount);
    recordCallback(cmd, bufferInput, bufferInputCount, textureInput,
                   textureInputCount, userData);
}

void ExecuteTransfer(RHI::CommandBuffer& cmd, RecordCallback recordCallback,
                     const RecordBufferInput* bufferInput, u32 bufferInputCount,
                     const RecordTextureInput* textureInput,
                     u32 textureInputCount, void* userData)
{
    InsertBarriers(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, bufferInput,
                   bufferInputCount, textureInput, textureInputCount);
    recordCallback(cmd, bufferInput, bufferInputCount, textureInput,
                   textureInputCount, userData);
}

void ChangeTextureAccessLayout(CommandBuffer& cmd, RHI::Texture& texture,
                               VkImageLayout newLayout,
                               VkAccessFlagBits2 accessMask)
{
    if (texture.imageLayout == newLayout && texture.accessMask == accessMask)
    {
        return;
    }

    RHI::RecordTextureInput textureInput;
    textureInput.pTexture = &texture;
    textureInput.accessMask = accessMask;
    textureInput.imageLayout = newLayout;

    RHI::InsertBarriers(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, nullptr, 0,
                        &textureInput, 1);
}

} // namespace RHI
} // namespace Fly
