#ifndef FLY_CONTEXT_H
#define FLY_CONTEXT_H

#include "device.h"

#define FLY_PHYSICAL_DEVICE_MAX_COUNT 4

struct GLFWwindow;

namespace Fly
{
namespace RHI
{

struct Context;

struct PhysicalDeviceInfo
{
    VkPhysicalDeviceFeatures features = {};
    VkPhysicalDeviceVulkan11Features vulkan11Features = {};
    VkPhysicalDeviceVulkan12Features vulkan12Features = {};
    VkPhysicalDeviceVulkan13Features vulkan13Features = {};
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

typedef bool (*IsPhysicalDeviceSuitableFn)(VkPhysicalDevice physicalDevice,
                                           const PhysicalDeviceInfo&);
typedef bool (*DetermineSurfaceFormatFn)(const Context&,
                                         const PhysicalDeviceInfo&,
                                         VkSurfaceFormatKHR&);
typedef bool (*DeterminePresentModeFn)(const Context&,
                                       const PhysicalDeviceInfo&,
                                       VkPresentModeKHR&);
typedef void (*FramebufferResizeFn)(GLFWwindow*, i32 w, i32 h);

bool IsExtensionSupported(VkExtensionProperties* extensionProperties,
                          u32 extensionPropertiesCount,
                          const char* extensionName);
bool IsLayerSupported(VkLayerProperties* layerProperties,
                      u32 layerPropertiesCount, const char* layerName);

struct ContextSettings
{
    VkPhysicalDeviceFeatures2 features2 = {};
    VkPhysicalDeviceVulkan11Features vulkan11Features = {};
    VkPhysicalDeviceVulkan12Features vulkan12Features = {};
    VkPhysicalDeviceVulkan13Features vulkan13Features = {};
    const char** instanceLayers = nullptr;
    const char** instanceExtensions = nullptr;
    const char** deviceExtensions = nullptr;
    GLFWwindow* windowPtr = nullptr;
    IsPhysicalDeviceSuitableFn isPhysicalDeviceSuitableCallback = nullptr;
    DetermineSurfaceFormatFn determineSurfaceFormatCallback = nullptr;
    DeterminePresentModeFn determinePresentModeCallback = nullptr;
    FramebufferResizeFn framebufferResizeCallback = nullptr;
    u32 instanceLayerCount = 0;
    u32 instanceExtensionCount = 0;
    u32 deviceExtensionCount = 0;
};

struct Context
{
    Device devices[FLY_PHYSICAL_DEVICE_MAX_COUNT];
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    FramebufferResizeFn framebufferResizeCallback = nullptr;
    GLFWwindow* windowPtr = nullptr;
    u32 deviceCount = 0;
    bool accelerationStructureExtensionPresent = false;
};

bool CreateContext(ContextSettings& settings, Context& outContext);
void DestroyContext(Context& context);
void WaitAllDevicesIdle(Context& context);

} // namespace RHI
} // namespace Fly

#endif /* FLY_CONTEXT_H */
