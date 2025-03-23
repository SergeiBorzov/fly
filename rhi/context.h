#ifndef HLS_CONTEXT_H
#define HLS_CONTEXT_H

#include "core/types.h"

#include "command_buffer.h"
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_VULKAN_VERSION 1004000 // Vulkan 1.4
#include <vk_mem_alloc.h>

#define HLS_PHYSICAL_DEVICE_MAX_COUNT 8
#define HLS_SWAPCHAIN_IMAGE_MAX_COUNT 8

#define HLS_FRAME_IN_FLIGHT_COUNT 2

struct GLFWwindow;

namespace Hls
{
struct Context;
}

namespace Hls
{
struct PhysicalDeviceInfo
{
    VkPhysicalDeviceFeatures features = {};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures =
        {};
    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = {};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR
        accelerationStructureFeatures = {};
    VkPhysicalDeviceProperties properties = {};
    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
    VkSurfaceFormatKHR* surfaceFormats = nullptr;
    VkPresentModeKHR* presentModes = nullptr;
    VkExtensionProperties* extensions = nullptr;
    VkQueueFamilyProperties* queueFamilies = nullptr;
    u32 queueFamilyCount = 0;
    u32 extensionCount = 0;
    u32 surfaceFormatCount = 0;
    u32 presentModeCount = 0;
};
typedef bool (*IsPhysicalDeviceSuitableFn)(const Context&,
                                           const PhysicalDeviceInfo&);
typedef bool (*DetermineSurfaceFormatFn)(const Context&,
                                         const PhysicalDeviceInfo&,
                                         VkSurfaceFormatKHR&);
typedef bool (*DeterminePresentModeFn)(const Context&,
                                       const PhysicalDeviceInfo&,
                                       VkPresentModeKHR&);

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

struct Context
{
    Device devices[HLS_PHYSICAL_DEVICE_MAX_COUNT];
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    GLFWwindow* windowPtr = nullptr;
    u32 deviceCount = 0;
};

struct ContextSettings
{
    VkPhysicalDeviceFeatures2 deviceFeatures2 = {};
    const char** instanceLayers = nullptr;
    const char** instanceExtensions = nullptr;
    const char** deviceExtensions = nullptr;
    GLFWwindow* windowPtr = nullptr;
    IsPhysicalDeviceSuitableFn isPhysicalDeviceSuitableCallback = nullptr;
    DetermineSurfaceFormatFn determineSurfaceFormatCallback = nullptr;
    DeterminePresentModeFn determinePresentModeCallback = nullptr;
    u32 instanceLayerCount = 0;
    u32 instanceExtensionCount = 0;
    u32 deviceExtensionCount = 0;

    ContextSettings()
    {
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    }
};

bool IsExtensionSupported(VkExtensionProperties* extensionProperties,
                          u32 extensionPropertiesCount,
                          const char* extensionName);
bool IsLayerSupported(VkLayerProperties* layerProperties,
                      u32 layerPropertiesCount, const char* layerName);

bool CreateContext(ContextSettings& settings, Context& outContext);
void DestroyContext(Context& context);

bool BeginRenderFrame(Context& context, Device& device);
bool EndRenderFrame(Context& context, Device& device);
void WaitAllDevicesIdle(Context& context);

void BeginTransfer(Device& device);
void EndTransfer(Device& device);
CommandBuffer& TransferCommandBuffer(Device& device);

CommandBuffer& RenderFrameCommandBuffer(Device& device);
VkImage RenderFrameSwapchainImage(Device& device);
VkImageView RenderFrameSwapchainImageView(Device& device);
VkRect2D SwapchainRect2D(const Device& device);

bool CreateDescriptorPool(Device& device, const VkDescriptorPoolSize* poolSizes,
                          u32 poolSizeCount, u32 maxSets,
                          VkDescriptorPool& descriptorPool);
VkResult AllocateDescriptorSets(Device& device, VkDescriptorPool descriptorPool,
                                const VkDescriptorSetLayout* layouts,
                                VkDescriptorSet* descriptorSets,
                                u32 descriptorSetCount);
void DestroyDescriptorPool(Device& device, VkDescriptorPool descriptorPool);

} // namespace Hls

#endif /* HLS_CONTEXT_H */
