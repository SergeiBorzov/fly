#ifndef FLY_COMMAND_BUFFER_H
#define FLY_COMMAND_BUFFER_H

#include "core/types.h"

#include <volk.h>

namespace Fly
{
namespace RHI
{

struct Device;
struct Buffer;
struct GraphicsPipeline;
struct ComputePipeline;

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

void BindGraphicsPipeline(Device& device, CommandBuffer& commandBuffer,
                          const GraphicsPipeline& graphicsPipeline);
void BindComputePipeline(Device& device, CommandBuffer& commandBuffer,
                         const ComputePipeline& computePipeline);
void SetPushConstants(Device& device, CommandBuffer& commandBuffer,
                      const void* pushConstants, u32 pushConstantsSize,
                      u32 offset = 0);
void FillBuffer(CommandBuffer& commandBuffer, Buffer& buffer, u32 value = 0,
                u64 offset = 0, u64 size = 0);

VkRenderingAttachmentInfo ColorAttachmentInfo(VkImageView imageView,
                                              VkImageLayout imageLayout);
VkRenderingAttachmentInfo DepthAttachmentInfo(VkImageView imageView,
                                              VkImageLayout imageLayout);

VkRenderingInfo
RenderingInfo(const VkRect2D& renderArea,
              const VkRenderingAttachmentInfo* colorAttachments,
              u32 colorAttachmentCount,
              const VkRenderingAttachmentInfo* depthAttachment = nullptr,
              const VkRenderingAttachmentInfo* stencilAttachment = nullptr,
              u32 layerCount = 1, u32 viewMask = 0);

VkBufferMemoryBarrier BufferMemoryBarrier(const RHI::Buffer& buffer,
                                          VkAccessFlags srcAccessMask,
                                          VkAccessFlags dstAccessMask,
                                          u64 offset = 0,
                                          u64 size = VK_WHOLE_SIZE);

} // namespace RHI
} // namespace Fly

#endif /* End of FLY_COMMAND_BUFFER_H */
