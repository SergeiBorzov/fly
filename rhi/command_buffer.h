#ifndef HLS_COMMAND_BUFFER_H
#define HLS_COMMAND_BUFFER_H

#include "core/types.h"

#include <volk.h>

struct Arena;

namespace Hls
{

struct Device;

struct CommandBuffer
{
    enum class State
    {
        NotAllocated,
        Idle,
        Recording,
        Recorded,
        Submitted,
        Invalid
    };
    VkCommandBuffer handle = VK_NULL_HANDLE;
    State state = State::NotAllocated;
};

bool CreateCommandBuffers(Arena& arena, const Device& device,
                          VkCommandPool commandPool,
                          CommandBuffer* commandBuffers, u32 commandBufferCount,
                          bool arePrimary = true);
void DestroyCommandBuffers(Arena& arena, const Device& device,
                           CommandBuffer* commandBuffers,
                           VkCommandPool commandPool, u32 commandBufferCount);
bool BeginCommandBuffer(CommandBuffer& commandBuffer, bool singleUse,
                        bool renderPassContinue = false, bool simultaneousUse = false);
bool EndCommandBuffer(CommandBuffer& commandBuffer);
bool SubmitCommandBuffer(CommandBuffer& commandBuffer, VkQueue queue,
                         const VkSemaphore* waitSemaphores,
                         u32 waitSemaphoreCount,
                         const VkPipelineStageFlags& waitStageMask,
                         const VkSemaphore* signalSemaphores,
                         u32 signalSemaphoreCount, VkFence fence);
bool ResetCommandBuffer(CommandBuffer& commandBuffer, bool releaseResources);

} // namespace Hls

#endif /* End of HLS_COMMAND_BUFFER_H */
