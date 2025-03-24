#ifndef HLS_RHI_DEVICE_H
#define HLS_RHI_DEVICE_H

#include "command_buffer.h"
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_VULKAN_VERSION 1004000 // Vulkan 1.4
#include <vk_mem_alloc.h>

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

struct DepthImage
{
    VmaAllocationInfo allocationInfo;
    VkImage handle = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_D24_UNORM_S8_UINT;
    VmaAllocation allocation;
};

struct Device
{
    char name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
    VmaAllocator allocator = {};
    VkImage swapchainImages[HLS_SWAPCHAIN_IMAGE_MAX_COUNT] = {VK_NULL_HANDLE};
    VkImageView swapchainImageViews[HLS_SWAPCHAIN_IMAGE_MAX_COUNT] = {
        VK_NULL_HANDLE};
    FrameData frameData[HLS_FRAME_IN_FLIGHT_COUNT];
    TransferData transferData = {};
    DepthImage depthImage = {};
    VkSurfaceFormatKHR surfaceFormat = {};
    VkExtent2D swapchainExtent = {0, 0};
    Context* context = nullptr;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice logicalDevice = VK_NULL_HANDLE;
    VkQueue graphicsComputeQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkPresentModeKHR presentMode;
    u32 swapchainImageCount = 0;
    u32 swapchainImageIndex = 0;
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

VkRect2D SwapchainRect2D(const Device& device);
VkImage RenderFrameSwapchainImage(Device& device);
VkImageView RenderFrameSwapchainImageView(Device& device);
CommandBuffer& RenderFrameCommandBuffer(Device& device);
bool BeginRenderFrame(Device& device);
bool EndRenderFrame(Device& device);

void DeviceWaitIdle(Device& device);

} // namespace Hls

#endif /* HLS_RHI_DEVICE_H */
