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
struct Texture;
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

void RecordTransitionImageLayout(CommandBuffer& commandBuffer, VkImage image,
                                 VkImageLayout currentLayout,
                                 VkImageLayout newLayout);

void FillBuffer(CommandBuffer& commandBuffer, Buffer& buffer, u32 value,
                u64 size = 0, u64 offset = 0);

VkBufferMemoryBarrier BufferMemoryBarrier(const Buffer& buffer,
                                          VkAccessFlags srcAccessMask,
                                          VkAccessFlags dstAccessMask,
                                          u64 offset = 0,
                                          u64 size = VK_WHOLE_SIZE);

/*--Commands--*/
void CopyTextureToBuffer(CommandBuffer& cmd, Buffer& dstBuffer,
                         const Texture& srcTexture, u32 mipLevel);
void CopyBufferToTexture(CommandBuffer& cmd, Texture& dstTexture,
                         Buffer& srcBuffer, u32 mipLevel);
void CopyBufferToMip(CommandBuffer& cmd, Texture& dstTexture, u32 layer,
                     u32 mipLevel, u32 width, u32 height, u32 depth,
                     Buffer& srcBuffer);
void GenerateMipmaps(CommandBuffer& cmd, Texture& texture);
void BindGraphicsPipeline(CommandBuffer& cmd,
                          const GraphicsPipeline& graphicsPipeline);
void BindComputePipeline(CommandBuffer& cmd,
                         const ComputePipeline& computePipeline);
void BindIndexBuffer(CommandBuffer& cmd, Buffer& buffer, VkIndexType indexType,
                     u64 offset = 0);
void SetViewport(CommandBuffer& cmd, f32 x, f32 y, f32 w, f32 h, f32 minDepth,
                 f32 maxDepth);
void SetScissor(CommandBuffer& cmd, i32 x, i32 y, u32 w, u32 h);
void ClearColor(CommandBuffer& cmd, Texture& texture, f32 r, f32 g, f32 b,
                f32 a);
void PushConstants(CommandBuffer& commandBuffer, const void* pushConstants,
                   u32 pushConstantsSize, u32 offset = 0);
void Dispatch(CommandBuffer& cmd, u32 groupCountX, u32 groupCountY,
              u32 groupCountZ);
void DispatchIndirect(CommandBuffer& cmd, const Buffer& buffer, u64 offset = 0);
void Draw(CommandBuffer& cmd, u32 vertexCount, u32 instanceCount,
          u32 firstVertex, u32 firstInstance);
void DrawIndexed(CommandBuffer& cmd, u32 indexCount, u32 instanceCount,
                 u32 firstIndex, u32 vertexOffset, u32 firstInstance);
void DrawIndirectCount(CommandBuffer& cmd, const Buffer& indirectDrawBuffer,
                       u64 offset, const Buffer& indirectCountBuffer,
                       u64 countOffset, u32 maxCount, u32 stride);
void DrawIndexedIndirectCount(CommandBuffer& cmd,
                              const Buffer& indirectDrawBuffer, u64 offset,
                              const Buffer& indirectCountBuffer,
                              u64 countOffset, u32 maxCount, u32 stride);
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
VkRenderingAttachmentInfo
ColorAttachmentInfo(VkImageView imageView,
                    VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    VkClearColorValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f});
VkRenderingAttachmentInfo
DepthAttachmentInfo(VkImageView imageView,
                    VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    VkClearDepthStencilValue clearDepthStencil = {1.0f, 0});

VkRenderingInfo
RenderingInfo(const VkRect2D& renderArea,
              const VkRenderingAttachmentInfo* colorAttachments,
              u32 colorAttachmentCount,
              const VkRenderingAttachmentInfo* depthAttachment = nullptr,
              const VkRenderingAttachmentInfo* stencilAttachment = nullptr,
              u32 layerCount = 1, u32 viewMask = 0);

struct ImageLayoutAccess
{
    VkAccessFlagBits2 accessMask = VK_ACCESS_2_NONE;
    VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct RecordBufferInput
{
    Buffer** buffers;
    const VkAccessFlagBits2* bufferAccesses;
    u32 bufferCount;
};

struct RecordTextureInput
{
    Texture** textures;
    const ImageLayoutAccess* imageLayoutsAccesses;
    u32 textureCount;
};

typedef void (*RecordCallback)(CommandBuffer& cmd,
                               const RecordBufferInput* bufferInput,
                               const RecordTextureInput* textureInput,
                               void* userData);

void ExecuteGraphics(CommandBuffer& cmd, const VkRenderingInfo& renderingInfo,
                     RecordCallback recordCallback,
                     const RecordBufferInput* bufferInput = nullptr,
                     const RecordTextureInput* textureInput = nullptr,
                     void* userData = nullptr);
void ExecuteCompute(CommandBuffer& cmd, RecordCallback recordCallback,
                    const RecordBufferInput* bufferInput = nullptr,
                    const RecordTextureInput* textureInput = nullptr,
                    void* userData = nullptr);
void ExecuteComputeIndirect(CommandBuffer& cmd, RecordCallback recordCallback,
                            const RecordBufferInput* bufferInput = nullptr,
                            const RecordTextureInput* textureInput = nullptr,
                            void* userData = nullptr);
void ExecuteTransfer(CommandBuffer& cmd, RecordCallback recordCallback,
                     const RecordBufferInput* bufferInput = nullptr,
                     const RecordTextureInput* textureInput = nullptr,
                     void* userData = nullptr);

void ChangeTextureAccessLayout(CommandBuffer& commandBuffer, Texture& texture,
                               VkImageLayout newLayout,
                               VkAccessFlagBits2 accessMask);

} // namespace RHI
} // namespace Fly

#endif /* End of FLY_COMMAND_BUFFER_H */
