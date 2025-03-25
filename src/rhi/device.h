#ifndef HLS_RHI_DEVICE_H
#define HLS_RHI_DEVICE_H

#include "command_buffer.h"
#include "texture.h"

#define HLS_SWAPCHAIN_IMAGE_MAX_COUNT 8
#define HLS_FRAME_IN_FLIGHT_COUNT 2

namespace Hls
{
struct Context;
struct CommandBuffer;
} // namespace Hls

namespace Hls
{

struct FrameData
{
    CommandBuffer commandBuffer = {};
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkSemaphore swapchainSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderSemaphore = VK_NULL_HANDLE;
    VkFence renderFence = VK_NULL_HANDLE;
};

struct TransferData
{
    CommandBuffer commandBuffer = {};
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkFence transferFence = VK_NULL_HANDLE;
};

struct Device
{
    char name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE] = {0};
    VmaAllocator allocator = {};
    SwapchainTexture swapchainTextures[HLS_SWAPCHAIN_IMAGE_MAX_COUNT] = {};
    DepthTexture depthTexture = {};
    FrameData frameData[HLS_FRAME_IN_FLIGHT_COUNT];
    TransferData transferData = {};
    VkSurfaceFormatKHR surfaceFormat = {};
    Context* context = nullptr;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice logicalDevice = VK_NULL_HANDLE;
    VkQueue graphicsComputeQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkPresentModeKHR presentMode;
    u32 swapchainTextureCount = 0;
    u32 swapchainTextureIndex = 0;
    u32 frameIndex = 0;
    i32 graphicsComputeQueueFamilyIndex = -1;
    i32 presentQueueFamilyIndex = -1;
};

bool CreateLogicalDevice(const char** extensions, u32 extensionCount,
                         VkPhysicalDeviceFeatures2& deviceFeatures2,
                         Context& context, Device& device);
void DestroyLogicalDevice(Device& device);

bool CreateDescriptorPool(Device& device, const VkDescriptorPoolSize* poolSizes,
                          u32 poolSizeCount, u32 maxSets,
                          VkDescriptorPool& descriptorPool);
VkResult AllocateDescriptorSets(Device& device, VkDescriptorPool descriptorPool,
                                const VkDescriptorSetLayout* layouts,
                                VkDescriptorSet* descriptorSets,
                                u32 descriptorSetCount);
void DestroyDescriptorPool(Device& device, VkDescriptorPool descriptorPool);

CommandBuffer& TransferCommandBuffer(Device& device);
void BeginTransfer(Device& device);
void EndTransfer(Device& device);

const SwapchainTexture& RenderFrameSwapchainTexture(const Device& device);
CommandBuffer& RenderFrameCommandBuffer(Device& device);
bool BeginRenderFrame(Device& device);
bool EndRenderFrame(Device& device);

void DeviceWaitIdle(Device& device);

} // namespace Hls

#endif /* HLS_RHI_DEVICE_H */
