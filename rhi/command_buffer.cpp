#include "core/arena.h"
#include "core/assert.h"
#include "core/log.h"

#include "rhi/command_buffer.h"
#include "rhi/context.h"

namespace Hls
{
bool CreateCommandBuffers(Arena& arena, const Hls::Device& device,
                          VkCommandPool commandPool,
                          CommandBuffer* commandBuffers, u32 commandBufferCount,
                          bool arePrimary)
{
    HLS_ASSERT(commandBuffers);
    HLS_ASSERT(commandBufferCount > 0);

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

void DestroyCommandBuffers(Arena& arena, const Hls::Device& device,
                           CommandBuffer* commandBuffers,
                           VkCommandPool commandPool, u32 commandBufferCount)
{
    HLS_ASSERT(commandBuffers);
    HLS_ASSERT(commandBufferCount > 0);

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
    submitInfo.signalSemaphoreCount = 1;
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
} // namespace Hls
