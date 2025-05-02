#ifndef HLS_CONTEXT_H
#define HLS_CONTEXT_H

#include "device.h"

#define HLS_PHYSICAL_DEVICE_MAX_COUNT 4

namespace Hls
{
namespace RHI
{

struct Context;

struct PhysicalDeviceInfo
{
    VkPhysicalDeviceFeatures features = {};
    VkPhysicalDeviceVulkan12Features vulkan12Features = {};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures =
        {};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR
        accelerationStructureFeatures = {};
    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = {};
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures = {};
    VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParametersFeatures =
        {};
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

bool IsExtensionSupported(VkExtensionProperties* extensionProperties,
                          u32 extensionPropertiesCount,
                          const char* extensionName);
bool IsLayerSupported(VkLayerProperties* layerProperties,
                      u32 layerPropertiesCount, const char* layerName);

struct ContextSettings
{
    VkPhysicalDeviceFeatures2 deviceFeatures2 = {};
    const char** instanceLayers = nullptr;
    const char** instanceExtensions = nullptr;
    const char** deviceExtensions = nullptr;
    void* windowPtr = nullptr;
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

struct Context
{
    Device devices[HLS_PHYSICAL_DEVICE_MAX_COUNT];
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    void* windowPtr = nullptr;
    u32 deviceCount = 0;
};

bool CreateContext(ContextSettings& settings, Context& outContext);
void DestroyContext(Context& context);
void WaitAllDevicesIdle(Context& context);

} // namespace RHI
} // namespace Hls

#endif /* HLS_CONTEXT_H */
