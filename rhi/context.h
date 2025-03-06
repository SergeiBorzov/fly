#ifndef HLS_CONTEXT_H
#define HLS_CONTEXT_H

#include "core/types.h"

#include <volk.h>
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_VULKAN_VERSION 1004000 // Vulkan 1.4
#include <vk_mem_alloc.h>

#define HLS_PHYSICAL_DEVICE_MAX_COUNT 8

struct Arena;
struct GLFWwindow;
struct HlsContext;

struct HlsPhysicalDeviceInfo
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
typedef bool (*HlsIsPhysicalDeviceSuitableFn)(const HlsContext&,
                                              const HlsPhysicalDeviceInfo&);
typedef bool (*HlsDetermineSurfaceFormatFn)(const HlsContext&,
                                            const HlsPhysicalDeviceInfo&,
                                            VkSurfaceFormatKHR&);
typedef bool (*HlsDeterminePresentModeFn)(const HlsContext&,
                                          const HlsPhysicalDeviceInfo&,
                                          VkPresentModeKHR&);

struct HlsDevice
{
    VmaAllocator allocator = {};
    VkSurfaceFormatKHR surfaceFormat = {};
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice logicalDevice = VK_NULL_HANDLE;
    VkQueue graphicsComputeQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkPresentModeKHR presentMode;
    i32 graphicsComputeQueueFamilyIndex = -1;
    i32 presentQueueFamilyIndex = -1;
};

struct HlsContext
{
    HlsDevice devices[HLS_PHYSICAL_DEVICE_MAX_COUNT];
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    GLFWwindow* windowPtr = nullptr;
    u32 deviceCount = 0;
};

struct HlsContextSettings
{
    VkPhysicalDeviceFeatures2 deviceFeatures2 = {};
    const char** instanceLayers = nullptr;
    const char** instanceExtensions = nullptr;
    const char** deviceExtensions = nullptr;
    GLFWwindow* windowPtr = nullptr;
    HlsIsPhysicalDeviceSuitableFn isPhysicalDeviceSuitableCallback = nullptr;
    HlsDetermineSurfaceFormatFn determineSurfaceFormatCallback = nullptr;
    HlsDeterminePresentModeFn determinePresentModeCallback = nullptr;
    u32 instanceLayerCount = 0;
    u32 instanceExtensionCount = 0;
    u32 deviceExtensionCount = 0;
};

bool HlsIsExtensionSupported(VkExtensionProperties* extensionProperties,
                             u32 extensionPropertiesCount,
                             const char* extensionName);
bool HlsIsLayerSupported(VkLayerProperties* layerProperties,
                         u32 layerPropertiesCount, const char* layerName);

bool HlsCreateContext(Arena& arena, const HlsContextSettings& settings,
                      HlsContext& outContext);
void HlsDestroyContext(HlsContext& context);

#endif /* HLS_CONTEXT_H */
