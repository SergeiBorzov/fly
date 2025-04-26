#ifndef HLS_RHI_DEVICE_H
#define HLS_RHI_DEVICE_H

#include "command_buffer.h"
#include "texture.h"

#define HLS_SWAPCHAIN_IMAGE_MAX_COUNT 8
#define HLS_FRAME_IN_FLIGHT_COUNT 2
#define HLS_UNIFORM_BUFFER_BINDING_INDEX 0
#define HLS_STORAGE_BUFFER_BINDING_INDEX 1
#define HLS_TEXTURE_BINDING_INDEX 2

namespace Hls
{
namespace RHI
{

struct Context;
struct CommandBuffer;

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
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout bindlessDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet bindlessDescriptorSet = VK_NULL_HANDLE;
    VkPresentModeKHR presentMode;
    u32 bindlessUniformBufferHandleCount = 0;
    u32 bindlessStorageBufferHandleCount = 0;
    u32 bindlessTextureHandleCount = 0;
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

CommandBuffer& TransferCommandBuffer(Device& device);
void BeginTransfer(Device& device);
void EndTransfer(Device& device);

const SwapchainTexture& RenderFrameSwapchainTexture(const Device& device);
CommandBuffer& RenderFrameCommandBuffer(Device& device);
bool BeginRenderFrame(Device& device);
bool EndRenderFrame(Device& device);

void DeviceWaitIdle(Device& device);

} // namespace RHI
} // namespace Hls

#endif /* HLS_RHI_DEVICE_H */
