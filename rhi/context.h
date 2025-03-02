#ifndef HLS_CONTEXT_H
#define HLS_CONTEXT_H

#include "core/types.h"
#include <volk.h>

#define HLS_PHYSICAL_DEVICE_MAX_COUNT 8

struct Arena;

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
    VkExtensionProperties* extensions = nullptr;
    VkQueueFamilyProperties* queueFamilies = nullptr;
    u32 queueFamilyCount = 0;
    u32 extensionCount = 0;
};
typedef bool (*HlsIsPhysicalDeviceSuitableFn)(const HlsPhysicalDeviceInfo&);

struct HlsDevice
{
    VkPhysicalDevice physicalDevice;
    VkDevice logicalDevice;
    VkQueue graphicsComputeQueue;
    VkQueue presentQueue;
    i32 graphicsComputeQueueFamilyIndex = -1;
    i32 presentQueueFamilyIndex = -1;
};

struct HlsContext
{
    VkInstance instance = VK_NULL_HANDLE;
    HlsDevice devices[HLS_PHYSICAL_DEVICE_MAX_COUNT];
    u32 deviceCount = 0;
};

struct HlsContextSettings
{
    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    const char** instanceLayers = nullptr;
    const char** instanceExtensions = nullptr;
    const char** deviceExtensions = nullptr;
    HlsIsPhysicalDeviceSuitableFn isPhysicalDeviceSuitableFn = nullptr;
    u32 instanceLayerCount = 0;
    u32 instanceExtensionCount = 0;
    u32 deviceExtensionCount = 0;
    bool renderOffscreen = false;
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
