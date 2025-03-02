#include <stdio.h>
#include <string.h>

#include "context.h"
#include "core/arena.h"
#include "core/assert.h"
#include "core/log.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/////////////////////////////////////////////////////////////////////////////
// Instance - start
/////////////////////////////////////////////////////////////////////////////
#ifndef NDEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL ValidationLayerCallbackFunc(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData)
{
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        HLS_DEBUG_LOG("%s", callbackData->pMessage);
    }
    return VK_FALSE;
}
#endif

static bool IsLayerSupported(VkLayerProperties* layerProperties,
                             u32 layerPropertiesCount, const char* layerName)
{
    bool isLayerPresent = false;
    for (u32 i = 0; i < layerPropertiesCount; i++)
    {
        if (strcmp(layerName, layerProperties[i].layerName) == 0)
        {
            isLayerPresent = true;
            break;
        }
    }

    return isLayerPresent;
}

static bool IsExtensionSupported(VkExtensionProperties* extensionProperties,
                                 u32 extensionPropertiesCount,
                                 const char* extensionName)
{
    bool isExtensionPresent = false;
    for (u32 i = 0; i < extensionPropertiesCount; i++)
    {
        if (strcmp(extensionName, extensionProperties[i].extensionName) == 0)
        {
            isExtensionPresent = true;
            break;
        }
    }

    return isExtensionPresent;
}

static bool CreateInstance(Arena* arena, const char** instanceLayers,
                           u32 instanceLayerCount,
                           const char** instanceExtensions,
                           u32 instanceExtensionCount, HlsContext* context)
{
    HLS_ASSERT(arena);

    ArenaMarker marker = ArenaGetMarker(arena);

    VkResult res = volkInitialize();
    if (res != VK_SUCCESS)
    {
        return false;
    }

    // Fill available instance layers
    u32 availableInstanceLayerCount = 0;
    VkLayerProperties* availableInstanceLayers = nullptr;
    vkEnumerateInstanceLayerProperties(&availableInstanceLayerCount, nullptr);
    if (availableInstanceLayerCount > 0)
    {
        availableInstanceLayers =
            HLS_ALLOC(arena, VkLayerProperties, availableInstanceLayerCount);
        vkEnumerateInstanceLayerProperties(&availableInstanceLayerCount,
                                           availableInstanceLayers);
    }

    // Fill available instance extensions
    u32 availableInstanceExtensionCount = 0;
    VkExtensionProperties* availableInstanceExtensions = nullptr;
    vkEnumerateInstanceExtensionProperties(
        nullptr, &availableInstanceExtensionCount, nullptr);
    if (availableInstanceExtensionCount > 0)
    {
        availableInstanceExtensions = HLS_ALLOC(
            arena, VkExtensionProperties, availableInstanceExtensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr,
                                               &availableInstanceExtensionCount,
                                               availableInstanceExtensions);
    }

    // Check Layers
    u32 totalLayerCount = instanceLayerCount;
    const char** totalLayers = instanceLayers;
#ifndef NDEBUG
    totalLayerCount += 1;
    totalLayers = HLS_ALLOC(arena, const char*, totalLayerCount);
    for (u32 i = 0; i < instanceLayerCount; i++)
    {
        HLS_DEBUG_LOG("Requested instance layer %s", instanceLayers[i]);
        totalLayers[i] = instanceLayers[i];
    }
    HLS_DEBUG_LOG(
        "Adding VK_LAYER_KHRONOS_validation to list of instance layers");
    totalLayers[instanceLayerCount] = "VK_LAYER_KHRONOS_validation";
#endif
    for (u32 i = 0; i < totalLayerCount; i++)
    {
        if (!IsLayerSupported(availableInstanceLayers,
                              availableInstanceLayerCount, totalLayers[i]))
        {
            HLS_ERROR("Following required instance layer %s is not present",
                      totalLayers[i]);
            return false;
        }
    }

    // Check Extensions
    u32 totalExtensionCount = instanceExtensionCount;
    const char** totalExtensions = instanceExtensions;
#ifndef NDEBUG

    totalExtensionCount += 1;
    totalExtensions = HLS_ALLOC(arena, const char*, totalExtensionCount);
    for (u32 i = 0; i < instanceExtensionCount; i++)
    {
        HLS_DEBUG_LOG("Requested instance extension %s", instanceExtensions[i]);
        totalExtensions[i] = instanceExtensions[i];
    }
    HLS_DEBUG_LOG("Adding VK_EXT_debug_utils to list of instance extensions");
    totalExtensions[instanceExtensionCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
#endif
    for (u32 i = 0; i < totalExtensionCount; i++)
    {
        if (!IsExtensionSupported(availableInstanceExtensions,
                                  availableInstanceExtensionCount,
                                  totalExtensions[i]))
        {
            HLS_ERROR("Following required extension %s is not present",
                      totalExtensions[i]);
            return false;
        }
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Helios";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = totalExtensionCount;
    createInfo.ppEnabledExtensionNames = totalExtensions;
    createInfo.enabledLayerCount = totalLayerCount;
    createInfo.ppEnabledLayerNames = totalLayers;

#ifndef NDEBUG
    VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};
    debugMessengerCreateInfo.sType =
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugMessengerCreateInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugMessengerCreateInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugMessengerCreateInfo.pfnUserCallback = ValidationLayerCallbackFunc;
    debugMessengerCreateInfo.pUserData = nullptr;

    createInfo.pNext =
        (VkDebugUtilsMessengerCreateInfoEXT*)&debugMessengerCreateInfo;
#endif

    VkInstance instance = VK_NULL_HANDLE;

    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    ArenaPopToMarker(arena, marker);

    if (result != VK_SUCCESS)
    {
        return false;
    }

    volkLoadInstance(instance);
    context->instance = instance;

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// PhysicalDevice
/////////////////////////////////////////////////////////////////////////////

struct HlsPhysicalDeviceInfo
{
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures;
    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR
        accelerationStructureFeatures;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    VkExtensionProperties* extensions = nullptr;
    VkQueueFamilyProperties* queueFamilies = nullptr;
    u32 queueFamilyCount = 0;
    u32 extensionCount = 0;
};

static const char* PhysicalDeviceTypeToString(VkPhysicalDeviceType deviceType)
{
    switch (deviceType)
    {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        {
            return "Integrated GPU";
        }
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        {
            return "Discrete GPU";
        }
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        {
            return "Virtual GPU";
        }
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
        {
            return "CPU";
        }
        default:
        {
            return "Unknown";
        }
    }
}

static bool
HasPhysicalDeviceHardwareRayTracingSupport(const HlsPhysicalDeviceInfo& info)
{
    bool isAccelStructureSupported =
        IsExtensionSupported(info.extensions, info.extensionCount,
                             VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);

    bool isRaytracingPipelineSupported =
        IsExtensionSupported(info.extensions, info.extensionCount,
                             VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    bool isRayQuerySupported = IsExtensionSupported(
        info.extensions, info.extensionCount, VK_KHR_RAY_QUERY_EXTENSION_NAME);

    bool driverSupportsRayTracing = isAccelStructureSupported &&
                                    isRaytracingPipelineSupported &&
                                    isRayQuerySupported;

    if (!driverSupportsRayTracing)
    {
        return false;
    }

    bool deviceSupportsRayTracing =
        info.rayTracingPipelineFeatures.rayTracingPipeline &&
        info.rayQueryFeatures.rayQuery &&
        info.accelerationStructureFeatures.accelerationStructure;

    return deviceSupportsRayTracing;
}

static HlsPhysicalDeviceInfo*
QueryPhysicalDevicesInformation(Arena* arena, VkPhysicalDevice* physicalDevices,
                                u32 physicalDeviceCount)
{
    HLS_ASSERT(arena);
    HLS_ASSERT(physicalDevices);

    if (physicalDeviceCount == 0)
    {
        return nullptr;
    }

    HlsPhysicalDeviceInfo* physicalDeviceInfos =
        HLS_ALLOC(arena, HlsPhysicalDeviceInfo, physicalDeviceCount);

    for (u32 i = 0; i < physicalDeviceCount; i++)
    {
        VkPhysicalDevice physicalDevice = physicalDevices[i];
        HlsPhysicalDeviceInfo& info = physicalDeviceInfos[i];

        // Properties
        vkGetPhysicalDeviceProperties(physicalDevice, &info.properties);

        // Memory Properties
        vkGetPhysicalDeviceMemoryProperties(physicalDevice,
                                            &info.memoryProperties);

        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr,
                                             &info.extensionCount, nullptr);
        if (info.extensionCount > 0)
        {
            info.extensions =
                HLS_ALLOC(arena, VkExtensionProperties, info.extensionCount);
            vkEnumerateDeviceExtensionProperties(
                physicalDevice, nullptr, &info.extensionCount, info.extensions);
        }

        // Device features + ray tracing features
        info.rayTracingPipelineFeatures = {};
        info.rayTracingPipelineFeatures.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

        info.rayQueryFeatures = {};
        info.rayQueryFeatures.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
        info.rayQueryFeatures.pNext = &info.rayTracingPipelineFeatures;

        info.accelerationStructureFeatures = {};
        info.accelerationStructureFeatures.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        info.accelerationStructureFeatures.pNext = &info.rayQueryFeatures;

        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &info.accelerationStructureFeatures;

        vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);
        info.features = deviceFeatures2.features;

        // Queue Family properties
        vkGetPhysicalDeviceQueueFamilyProperties(
            physicalDevice, &info.queueFamilyCount, nullptr);
        if (info.queueFamilyCount > 0)
        {
            info.queueFamilies = HLS_ALLOC(arena, VkQueueFamilyProperties,
                                           info.queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(
                physicalDevice, &info.queueFamilyCount, info.queueFamilies);
        }
    }

    return physicalDeviceInfos;
}

static void
LogDetectedPhysicalDevices(const HlsPhysicalDeviceInfo* physicalDeviceInfos,
                           u32 physicalDeviceCount)
{
    HLS_LOG("Following physical devices detected:");
    for (u32 i = 0; i < physicalDeviceCount; i++)
    {
        const HlsPhysicalDeviceInfo& info = physicalDeviceInfos[i];

        u64 totalMemory = 0;
        for (u32 i = 0; i < info.memoryProperties.memoryHeapCount; i++)
        {
            if (info.memoryProperties.memoryHeaps[i].flags &
                VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            {
                totalMemory += info.memoryProperties.memoryHeaps[i].size;
            }
        }
        totalMemory = totalMemory / (1024 * 1024);

        const char* physicalDeviceType =
            PhysicalDeviceTypeToString(info.properties.deviceType);

        bool rt = HasPhysicalDeviceHardwareRayTracingSupport(info);
        HLS_LOG("[%u] - %s -- %s -- RT %s -- VRAM %llu MB", i + 1,
                info.properties.deviceName, physicalDeviceType,
                rt ? "ON" : "OFF", totalMemory);
    }
}

static bool FindPhysicalDevices(Arena* arena, const char** deviceExtensions,
                                u32 deviceExtensionCount, HlsContext* context)
{
    HLS_ASSERT(arena);

    ArenaMarker marker = ArenaGetMarker(arena);

    u32 physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(context->instance, &physicalDeviceCount,
                               nullptr);
    if (physicalDeviceCount == 0)
    {
        HLS_ERROR("No physical devices detected");
        return false;
    }

    VkPhysicalDevice* physicalDevices =
        HLS_ALLOC(arena, VkPhysicalDevice, physicalDeviceCount);
    vkEnumeratePhysicalDevices(context->instance, &physicalDeviceCount,
                               physicalDevices);
    HlsPhysicalDeviceInfo* physicalDeviceInfos =
        QueryPhysicalDevicesInformation(arena, physicalDevices,
                                        physicalDeviceCount);

    LogDetectedPhysicalDevices(physicalDeviceInfos, physicalDeviceCount);

    u32 suitableDeviceCount = 0;
    HlsDevice* suitableDevices =
        HLS_ALLOC(arena, HlsDevice, physicalDeviceCount);
    u32* suitableDeviceIndices = HLS_ALLOC(arena, u32, physicalDeviceCount);

    for (u32 i = 0; i < physicalDeviceCount; i++)
    {
        const HlsPhysicalDeviceInfo& info = physicalDeviceInfos[i];

        if (!HasPhysicalDeviceHardwareRayTracingSupport(info))
        {
            continue;
        }

        i32 graphicsComputeFamilyIndex = -1;
        for (u32 j = 0; j < info.queueFamilyCount; j++)
        {
            const VkQueueFamilyProperties& queueFamily = info.queueFamilies[j];
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT &&
                queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                graphicsComputeFamilyIndex = j;
            }
        }
        if (graphicsComputeFamilyIndex == -1)
        {
            continue;
        }

        // TODO: uncomment
        // if (info.properties.deviceType !=
        // VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        // {
        //     continue;
        // }

        suitableDeviceIndices[suitableDeviceCount] = i;
        suitableDevices[suitableDeviceCount].physicalDevice =
            physicalDevices[i];
        suitableDevices[suitableDeviceCount].graphicsComputeFamilyIndex =
            graphicsComputeFamilyIndex;
        suitableDeviceCount++;
    }

    context->deviceCount =
        MIN(HLS_PHYSICAL_DEVICE_MAX_COUNT, suitableDeviceCount);
    for (u32 i = 0; i < context->deviceCount; i++)
    {
        HlsDevice& device = context->devices[i];
        const HlsPhysicalDeviceInfo& info =
            physicalDeviceInfos[suitableDeviceIndices[i]];

        context->devices[i] = suitableDevices[i];
        HLS_LOG("Following physical device is suitable and selected [%u] - %s",
                i, info.properties.deviceName);
    }

    ArenaPopToMarker(arena, marker);

    return true;
}

bool HlsCreateContext(Arena* arena, const char** instanceLayers,
                      u32 instanceLayerCount, const char** instanceExtensions,
                      u32 instanceExtensionCount, const char** deviceExtensions,
                      u32 deviceExtensionCount, HlsContext* context)
{
    HLS_ASSERT(arena);
    HLS_ASSERT(context);

    if (!CreateInstance(arena, instanceLayers, instanceLayerCount,
                        instanceExtensions, instanceExtensionCount, context))
    {
        return false;
    }

    if (!FindPhysicalDevices(arena, deviceExtensions, deviceExtensionCount,
                             context))
    {
        return false;
    }

    return true;
}

void HlsDestroyContext(HlsContext* context)
{
    HLS_ASSERT(context);
    vkDestroyInstance(context->instance, nullptr);
}
