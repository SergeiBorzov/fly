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
struct Texture2D;
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
    Device* device = nullptr;
    VkCommandBuffer handle = VK_NULL_HANDLE;
    State state = State::NotAllocated;
};

bool CreateCommandBuffers(Device& device, VkCommandPool commandPool,
                          CommandBuffer* commandBuffers, u32 commandBufferCount,
                          bool arePrimary = true);
void DestroyCommandBuffers(Device& device, CommandBuffer* commandBuffers,
                           VkCommandPool commandPool, u32 commandBufferCount);
bool BeginCommandBuffer(CommandBuffer& commandBuffer, bool singleUse,
                        bool renderPassContinue = false,
                        bool simultaneousUse = false);
bool EndCommandBuffer(CommandBuffer& commandBuffer);
bool SubmitCommandBuffer(CommandBuffer& commandBuffer, VkQueue queue,
                         const VkSemaphore* waitSemaphores,
                         u32 waitSemaphoreCount,
                         VkPipelineStageFlags* pipelineStageMasks,
                         const VkSemaphore* signalSemaphores,
                         u32 signalSemaphoreCount, VkFence fence,
                         void* pNext = nullptr);
bool ResetCommandBuffer(CommandBuffer& commandBuffer, bool releaseResources);

void ChangeTexture2DLayout(CommandBuffer& commandBuffer,
                           RHI::Texture2D& texture, VkImageLayout newLayout);
void RecordTransitionImageLayout(CommandBuffer& commandBuffer, VkImage image,
                                 VkImageLayout currentLayout,
                                 VkImageLayout newLayout);

void FillBuffer(CommandBuffer& commandBuffer, Buffer& buffer, u32 value,
                u64 size = 0, u64 offset = 0);

VkBufferMemoryBarrier BufferMemoryBarrier(const RHI::Buffer& buffer,
                                          VkAccessFlags srcAccessMask,
                                          VkAccessFlags dstAccessMask,
                                          u64 offset = 0,
                                          u64 size = VK_WHOLE_SIZE);

/*--Commands--*/
void BindGraphicsPipeline(CommandBuffer& commandBuffer,
                          const GraphicsPipeline& graphicsPipeline);
void BindComputePipeline(CommandBuffer& commandBuffer,
                         const ComputePipeline& computePipeline);
void BindIndexBuffer(CommandBuffer& commandBuffer, RHI::Buffer& buffer,
                     VkIndexType indexType, u64 offset = 0);
void SetViewport(CommandBuffer& cmd, f32 x, f32 y, f32 w, f32 h, f32 minDepth,
                 f32 maxDepth);
void SetScissor(CommandBuffer& cmd, i32 x, i32 y, u32 w, u32 h);
void ClearColor(CommandBuffer& cmd, Texture2D& texture2D, f32 r, f32 g, f32 b,
                f32 a);
void PushConstants(CommandBuffer& commandBuffer, const void* pushConstants,
                   u32 pushConstantsSize, u32 offset = 0);
void Dispatch(CommandBuffer& cmd, u32 groupCountX, u32 groupCountY,
              u32 groupCountZ);
void Draw(CommandBuffer& cmd, u32 vertexCount, u32 instanceCount,
          u32 firstVertex, u32 firstInstance);
void DrawIndexed(CommandBuffer& cmd, u32 indexCount, u32 instanceCount,
                 u32 firstIndex, u32 vertexOffset, u32 firstInstance);
void PipelineBarrier(CommandBuffer& cmd,
                     const VkBufferMemoryBarrier2* bufferBarriers,
                     u32 bufferBarrierCount,
                     const VkImageMemoryBarrier2* imageBarriers,
                     u32 imageBarrierCount);
void ResetQueryPool(CommandBuffer& cmd, VkQueryPool, u32 firstQuery,
                    u32 queryCount);
void WriteTimestamp(CommandBuffer& cmd, VkPipelineStageFlagBits pipelineStage,
                    VkQueryPool queryPool, u32 query);
/*----------*/

} // namespace RHI
} // namespace Fly

#endif /* End of FLY_COMMAND_BUFFER_H */
