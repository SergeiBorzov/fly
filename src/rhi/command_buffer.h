#ifndef HLS_COMMAND_BUFFER_H
#define HLS_COMMAND_BUFFER_H

#include "core/types.h"

#include <volk.h>

namespace Hls
{
namespace RHI
{

struct Device;
struct Buffer;

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

bool CreateCommandBuffers(const Device& device, VkCommandPool commandPool,
                          CommandBuffer* commandBuffers, u32 commandBufferCount,
                          bool arePrimary = true);
void DestroyCommandBuffers(const Device& device, CommandBuffer* commandBuffers,
                           VkCommandPool commandPool, u32 commandBufferCount);
bool BeginCommandBuffer(CommandBuffer& commandBuffer, bool singleUse,
                        bool renderPassContinue = false,
                        bool simultaneousUse = false);
bool EndCommandBuffer(CommandBuffer& commandBuffer);
bool SubmitCommandBuffer(CommandBuffer& commandBuffer, VkQueue queue,
                         const VkSemaphore* waitSemaphores,
                         u32 waitSemaphoreCount,
                         const VkPipelineStageFlags& waitStageMask,
                         const VkSemaphore* signalSemaphores,
                         u32 signalSemaphoreCount, VkFence fence);
bool ResetCommandBuffer(CommandBuffer& commandBuffer, bool releaseResources);

void RecordTransitionImageLayout(CommandBuffer& commandBuffer, VkImage image,
                                 VkImageLayout currentLayout,
                                 VkImageLayout newLayout);
void RecordClearColor(CommandBuffer& commandBuffer, VkImage image, f32 r, f32 g,
                      f32 b, f32 a);

VkRenderingAttachmentInfo ColorAttachmentInfo(VkImageView imageView,
                                              VkImageLayout imageLayout);
VkRenderingAttachmentInfo DepthAttachmentInfo(VkImageView imageView,
                                              VkImageLayout imageLayout);

VkRenderingInfo
RenderingInfo(const VkRect2D& renderArea,
              const VkRenderingAttachmentInfo* colorAttachments,
              u32 colorAttachmentCount,
              const VkRenderingAttachmentInfo* depthAttachment = nullptr,
              const VkRenderingAttachmentInfo* stencilAttachment = nullptr);

VkBufferMemoryBarrier
BufferMemoryBarrier(const RHI::Buffer& buffer,
                    VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                    u64 offset = 0, u64 size = VK_WHOLE_SIZE);

} // namespace RHI
} // namespace Hls

#endif /* End of HLS_COMMAND_BUFFER_H */
