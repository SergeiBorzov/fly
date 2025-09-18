#ifndef FLY_RHI_DEVICE_H
#define FLY_RHI_DEVICE_H

#include "core/list.h"

#include "command_buffer.h"
#include "vma.h"

#define FLY_SWAPCHAIN_IMAGE_MAX_COUNT 8
#define FLY_FRAME_IN_FLIGHT_COUNT 2
#define FLY_UNIFORM_BUFFER_BINDING_INDEX 0
#define FLY_STORAGE_BUFFER_BINDING_INDEX 1
#define FLY_TEXTURE_BINDING_INDEX 2
#define FLY_STORAGE_TEXTURE_BINDING_INDEX 3

namespace Fly
{
namespace RHI
{

struct Context;
struct Device;

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

struct SwapchainTexture
{
    VkImage handle = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
};

typedef void (*SwapchainRecreatedFn)(Device& device, u32 w, u32 h, void*);

struct SwapchainRecreatedCallback
{
    SwapchainRecreatedFn func;
    void* data;

    inline bool operator==(const SwapchainRecreatedCallback& other)
    {
        return func == other.func && data == other.data;
    }

    inline bool operator!=(const SwapchainRecreatedCallback& other)
    {
        return func != other.func || data != other.data;
    }
};

struct Device
{
    char name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE] = {0};
    VmaAllocator allocator = {};
    SwapchainTexture swapchainTextures[FLY_SWAPCHAIN_IMAGE_MAX_COUNT] = {};
    FrameData frameData[FLY_FRAME_IN_FLIGHT_COUNT];
    TransferData transferData = {};
    VkSurfaceFormatKHR surfaceFormat = {};
    SwapchainRecreatedCallback swapchainRecreatedCallback;
    Context* context = nullptr;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice logicalDevice = VK_NULL_HANDLE;
    VkQueue graphicsComputeQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout bindlessDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet bindlessDescriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPresentModeKHR presentMode;
    u32 bindlessUniformBufferHandleCount = 0;
    u32 bindlessStorageBufferHandleCount = 0;
    u32 bindlessTextureHandleCount = 0;
    u32 bindlessWriteTextureHandleCount = 0;
    u32 swapchainTextureCount = 0;
    u32 swapchainTextureIndex = 0;
    u32 swapchainWidth = 0;
    u32 swapchainHeight = 0;
    u32 frameIndex = 0;
    i32 graphicsComputeQueueFamilyIndex = -1;
    i32 presentQueueFamilyIndex = -1;
    bool isFramebufferResized = false;
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
bool EndRenderFrame(Device& device, VkSemaphore* extraWaitSemaphores,
                    VkPipelineStageFlags* extraWaitStageMasks,
                    u32 extraWaitSemaphoreCount,
                    VkSemaphore* extraSignalSemaphores,
                    u32 extraSignalSemaphoreCount, void* pNext);

void WaitDeviceIdle(Device& device);

} // namespace RHI
} // namespace Fly

#endif /* FLY_RHI_DEVICE_H */
