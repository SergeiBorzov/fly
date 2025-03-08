#include <stdio.h>
#include <string.h>

#define VMA_IMPLEMENTATION
#include "context.h" // volk should be included prior to glfw
#include <GLFW/glfw3.h>

#include "core/arena.h"
#include "core/assert.h"
#include "core/log.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(v, min, max) MAX(MIN(v, max), min)

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

bool HlsIsLayerSupported(VkLayerProperties* layerProperties,
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

bool HlsIsExtensionSupported(VkExtensionProperties* extensionProperties,
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

static bool CreateInstance(Arena& arena, const char** instanceLayers,
                           u32 instanceLayerCount,
                           const char** instanceExtensions,
                           u32 instanceExtensionCount, HlsContext& context)
{
    ArenaMarker marker = ArenaGetMarker(arena);

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
        HLS_LOG("Requested instance layer %s", instanceLayers[i]);
        totalLayers[i] = instanceLayers[i];
    }
    HLS_DEBUG_LOG(
        "Adding VK_LAYER_KHRONOS_validation to list of instance layers");
    totalLayers[instanceLayerCount] = "VK_LAYER_KHRONOS_validation";
#endif
    for (u32 i = 0; i < totalLayerCount; i++)
    {
        if (!HlsIsLayerSupported(availableInstanceLayers,
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
        HLS_LOG("Requested instance extension %s", instanceExtensions[i]);
        totalExtensions[i] = instanceExtensions[i];
    }
    HLS_DEBUG_LOG("Adding VK_EXT_debug_utils to list of instance extensions");
    totalExtensions[instanceExtensionCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
#endif
    for (u32 i = 0; i < totalExtensionCount; i++)
    {
        if (!HlsIsExtensionSupported(availableInstanceExtensions,
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
    context.instance = instance;

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// PhysicalDevice
/////////////////////////////////////////////////////////////////////////////
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

static const char* PresentModeToString(VkPresentModeKHR presentMode)
{
    switch (presentMode)
    {
        case VK_PRESENT_MODE_IMMEDIATE_KHR:
        {
            return "VK_PRESENT_MODE_IMMEDIATE_KHR";
        }
        case VK_PRESENT_MODE_MAILBOX_KHR:
        {
            return "VK_PRESENT_MODE_MAILBOX_KHR";
        }
        case VK_PRESENT_MODE_FIFO_KHR:
        {
            return "VK_PRESENT_MODE_FIFO_KHR";
        }
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
        {
            return "VK_PRESENT_MODE_FIFO_RELAXED_KHR";
        }
        case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
        {
            return "VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR";
        }
        case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
        {
            return "VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR";
        }
        default:
            return "Unknown Present Mode";
    }
}

static const char* FormatToString(VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_UNDEFINED:
        {
            return "VK_FORMAT_UNDEFINED";
        }
        case VK_FORMAT_B8G8R8A8_UNORM:
        {
            return "VK_FORMAT_B8G8R8A8_UNORM";
        }
        case VK_FORMAT_R8G8B8A8_UNORM:
        {
            return "VK_FORMAT_R8G8B8A8_UNORM";
        }
        case VK_FORMAT_B8G8R8A8_SRGB:
        {
            return "VK_FORMAT_B8G8R8A8_SRGB";
        }
        case VK_FORMAT_R8G8B8A8_SRGB:
        {
            return "VK_FORMAT_R8G8B8A8_SRGB";
        }
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        {
            return "VK_FORMAT_A2B10G10R10_UNORM_PACK32";
        }
        case VK_FORMAT_R16G16B16A16_SFLOAT:
        {
            return "VK_FORMAT_R16G16B16A16_SFLOAT";
        }
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
        {
            return "VK_FORMAT_B10G11R11_UFLOAT_PACK32";
        }
        case VK_FORMAT_R5G6B5_UNORM_PACK16:
        {
            return "VK_FORMAT_R5G6B5_UNORM_PACK16";
        }
        case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
        {
            return "VK_FORMAT_A1R5G5B5_UNORM_PACK16";
        }
        default:
        {
            return "Unknown Format";
        }
    }
}

static const char* ColorSpaceToString(VkColorSpaceKHR colorSpace)
{
    switch (colorSpace)
    {
        case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
        {
            return "VK_COLOR_SPACE_SRGB_NONLINEAR_KHR";
        }
        case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT:
        {
            return "VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT";
        }
        case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
        {
            return "VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT";
        }
        case VK_COLOR_SPACE_DCI_P3_LINEAR_EXT:
        {
            return "VK_COLOR_SPACE_DCI_P3_LINEAR_EXT";
        }
        case VK_COLOR_SPACE_BT709_LINEAR_EXT:
        {
            return "VK_COLOR_SPACE_BT709_LINEAR_EXT";
        }
        case VK_COLOR_SPACE_BT709_NONLINEAR_EXT:
        {
            return "VK_COLOR_SPACE_BT709_NONLINEAR_EXT";
        }
        case VK_COLOR_SPACE_HDR10_ST2084_EXT:
        {
            return "VK_COLOR_SPACE_HDR10_ST2084_EXT";
        }
        case VK_COLOR_SPACE_DOLBYVISION_EXT:
        {
            return "VK_COLOR_SPACE_DOLBYVISION_EXT";
        }
        case VK_COLOR_SPACE_HDR10_HLG_EXT:
        {
            return "VK_COLOR_SPACE_HDR10_HLG_EXT";
        }
        case VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT:
        {
            return "VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT";
        }
        case VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT:
        {
            return "VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT";
        }
        case VK_COLOR_SPACE_PASS_THROUGH_EXT:
        {
            return "VK_COLOR_SPACE_PASS_THROUGH_EXT";
        }
        case VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT:
        {
            return "VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT";
        }
        default:
        {
            return "Unknown Color Space";
        }
    }
}

static VkSurfaceFormatKHR
DefaultDetermineSurfaceFormat(const VkSurfaceFormatKHR* surfaceFormats,
                              u32 surfaceFormatCount)
{
    HLS_ASSERT(surfaceFormats);
    HLS_ASSERT(surfaceFormatCount > 0);

    VkSurfaceFormatKHR fallbackFormat = surfaceFormats[0];

    for (u32 i = 0; i < surfaceFormatCount; i++)
    {
        if (surfaceFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
            surfaceFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB)
        {
            return surfaceFormats[i];
        }
    }

    return fallbackFormat;
}

static VkPresentModeKHR
DefaultDeterminePresentMode(const VkPresentModeKHR* presentModes,
                            u32 presentModeCount)
{
    HLS_ASSERT(presentModes);
    HLS_ASSERT(presentModeCount > 0);

    VkPresentModeKHR fallbackPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (u32 i = 0; i < presentModeCount; i++)
    {
        if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return presentModes[i];
        }
    }

    return fallbackPresentMode;
}

static const HlsPhysicalDeviceInfo*
QueryPhysicalDevicesInformation(Arena& arena, const HlsContext& context,
                                VkPhysicalDevice* physicalDevices,
                                u32 physicalDeviceCount)
{
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

        // Device Extensions
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr,
                                             &info.extensionCount, nullptr);
        if (info.extensionCount > 0)
        {
            info.extensions =
                HLS_ALLOC(arena, VkExtensionProperties, info.extensionCount);
            vkEnumerateDeviceExtensionProperties(
                physicalDevice, nullptr, &info.extensionCount, info.extensions);
        }

        if (context.windowPtr)
        {
            // Surface capabilities
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                physicalDevice, context.surface, &info.surfaceCapabilities);

            // Surface formats
            vkGetPhysicalDeviceSurfaceFormatsKHR(
                physicalDevice, context.surface, &info.surfaceFormatCount,
                nullptr);
            if (info.surfaceFormatCount > 0)
            {
                info.surfaceFormats = HLS_ALLOC(arena, VkSurfaceFormatKHR,
                                                info.surfaceFormatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(
                    physicalDevice, context.surface, &info.surfaceFormatCount,
                    info.surfaceFormats);
            }

            // Present modes
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                physicalDevice, context.surface, &info.presentModeCount,
                nullptr);
            if (info.presentModeCount > 0)
            {
                info.presentModes =
                    HLS_ALLOC(arena, VkPresentModeKHR, info.presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(
                    physicalDevice, context.surface, &info.presentModeCount,
                    info.presentModes);
            }
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

static bool PhysicalDeviceSupportsAccelerationStructureFeatures(
    const VkPhysicalDeviceAccelerationStructureFeaturesKHR& requested,
    const VkPhysicalDeviceAccelerationStructureFeaturesKHR& supported)
{
    if (requested.accelerationStructure && !supported.accelerationStructure)
    {
        return false;
    }

    if (requested.accelerationStructureCaptureReplay &&
        !supported.accelerationStructureCaptureReplay)
    {
        return false;
    }

    if (requested.accelerationStructureIndirectBuild &&
        !supported.accelerationStructureIndirectBuild)
    {
        return false;
    }

    if (requested.accelerationStructureHostCommands &&
        !supported.accelerationStructureHostCommands)
    {
        return false;
    }

    if (requested.descriptorBindingAccelerationStructureUpdateAfterBind &&
        !supported.descriptorBindingAccelerationStructureUpdateAfterBind)
    {
        return false;
    }

    return true;
}

static bool PhysicalDeviceSupportsRayQueryFeatures(
    const VkPhysicalDeviceRayQueryFeaturesKHR& requested,
    const VkPhysicalDeviceRayQueryFeaturesKHR& supported)
{
    return !requested.rayQuery || supported.rayQuery;
}

static bool PhysicalDeviceSupportsRayTracingPipelineFeatures(
    const VkPhysicalDeviceRayTracingPipelineFeaturesKHR& requested,
    const VkPhysicalDeviceRayTracingPipelineFeaturesKHR& supported)
{
    if (requested.rayTracingPipeline && !supported.rayTracingPipeline)
    {
        return false;
    }

    if (requested.rayTracingPipelineShaderGroupHandleCaptureReplay &&
        !supported.rayTracingPipelineShaderGroupHandleCaptureReplay)
    {
        return false;
    }

    if (requested.rayTracingPipelineShaderGroupHandleCaptureReplayMixed &&
        !supported.rayTracingPipelineShaderGroupHandleCaptureReplayMixed)
    {
        return false;
    }

    if (requested.rayTracingPipelineTraceRaysIndirect &&
        !supported.rayTracingPipelineTraceRaysIndirect)
    {
        return false;
    }

    if (requested.rayTraversalPrimitiveCulling &&
        !supported.rayTraversalPrimitiveCulling)
    {
        return false;
    }

    return true;
}

static bool PhysicalDeviceSupportsRequiredFeatures(
    const VkPhysicalDeviceFeatures2& requested,
    const HlsPhysicalDeviceInfo& supported)
{
    const VkBool32* supportedFeatures =
        reinterpret_cast<const VkBool32*>(&(supported.features));
    const VkBool32* requestedFeatures =
        reinterpret_cast<const VkBool32*>(&(requested.features));

    const u64 featureCount =
        sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);

    for (u64 i = 0; i < featureCount; i++)
    {
        if (requestedFeatures[i] && !supportedFeatures[i])
        {
            return false;
        }
    }

    const VkBaseOutStructure* currRequestedFeaturePtr =
        reinterpret_cast<const VkBaseOutStructure*>(requested.pNext);
    while (currRequestedFeaturePtr != nullptr)
    {
        switch (currRequestedFeaturePtr->sType)
        {
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR:
            {
                const VkPhysicalDeviceAccelerationStructureFeaturesKHR*
                    requestedAccelerationStructureFeatures = reinterpret_cast<
                        const VkPhysicalDeviceAccelerationStructureFeaturesKHR*>(
                        currRequestedFeaturePtr);
                if (!PhysicalDeviceSupportsAccelerationStructureFeatures(
                        *requestedAccelerationStructureFeatures,
                        supported.accelerationStructureFeatures))
                {
                    return false;
                }
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR:
            {
                const VkPhysicalDeviceRayQueryFeaturesKHR*
                    requestedRayQueryFeatures = reinterpret_cast<
                        const VkPhysicalDeviceRayQueryFeaturesKHR*>(
                        currRequestedFeaturePtr);
                if (!PhysicalDeviceSupportsRayQueryFeatures(
                        *requestedRayQueryFeatures, supported.rayQueryFeatures))
                {
                    return false;
                }
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR:
            {
                const VkPhysicalDeviceRayTracingPipelineFeaturesKHR*
                    requestedRayTracingPipelineFeatures = reinterpret_cast<
                        const VkPhysicalDeviceRayTracingPipelineFeaturesKHR*>(
                        currRequestedFeaturePtr);
                if (!PhysicalDeviceSupportsRayTracingPipelineFeatures(
                        *requestedRayTracingPipelineFeatures,
                        supported.rayTracingPipelineFeatures))
                {
                    return false;
                }
                break;
            }
        }
        currRequestedFeaturePtr = currRequestedFeaturePtr->pNext;
    }

    return true;
}

static void
LogDetectedPhysicalDevices(const HlsPhysicalDeviceInfo* physicalDeviceInfos,
                           u32 physicalDeviceCount)
{
    HLS_LOG("List of physical devices detected:");
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

        HLS_LOG("[%u] - %s -- %s -- VRAM %llu MB", i + 1,
                info.properties.deviceName, physicalDeviceType, totalMemory);
    }
}

static bool
PhysicalDeviceSupportsRequiredExtensions(const char** extensions,
                                         u32 extensionCount,
                                         const HlsPhysicalDeviceInfo& info)
{

    for (u32 i = 0; i < extensionCount; i++)
    {
        if (!HlsIsExtensionSupported(info.extensions, info.extensionCount,
                                     extensions[i]))
        {
            return false;
        }
    }
    return true;
}

static bool FindPhysicalDevices(
    Arena& arena, const char** deviceExtensions, u32 deviceExtensionCount,
    const VkPhysicalDeviceFeatures2& deviceFeatures2,
    HlsIsPhysicalDeviceSuitableFn isPhysicalDeviceSuitableCallback,
    HlsDetermineSurfaceFormatFn determineSurfaceFormatCallback,
    HlsDeterminePresentModeFn determinePresentModeCallback, HlsContext& context)
{
    ArenaMarker marker = ArenaGetMarker(arena);

    u32 physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(context.instance, &physicalDeviceCount, nullptr);
    if (physicalDeviceCount == 0)
    {
        HLS_ERROR("No physical devices detected");
        return false;
    }

    VkPhysicalDevice* physicalDevices =
        HLS_ALLOC(arena, VkPhysicalDevice, physicalDeviceCount);
    vkEnumeratePhysicalDevices(context.instance, &physicalDeviceCount,
                               physicalDevices);
    const HlsPhysicalDeviceInfo* physicalDeviceInfos =
        QueryPhysicalDevicesInformation(arena, context, physicalDevices,
                                        physicalDeviceCount);

    LogDetectedPhysicalDevices(physicalDeviceInfos, physicalDeviceCount);

    u32 suitableDeviceCount = 0;
    HlsDevice* suitableDevices =
        HLS_ALLOC(arena, HlsDevice, physicalDeviceCount);
    u32* suitableDeviceIndices = HLS_ALLOC(arena, u32, physicalDeviceCount);
    for (u32 i = 0; i < physicalDeviceCount; i++)
    {
        suitableDevices[i] = {};
    }

    for (u32 i = 0; i < physicalDeviceCount; i++)
    {
        const VkPhysicalDevice physicalDevice = physicalDevices[i];
        const HlsPhysicalDeviceInfo& info = physicalDeviceInfos[i];

        if (!PhysicalDeviceSupportsRequiredExtensions(
                deviceExtensions, deviceExtensionCount, info))
        {
            continue;
        }

        for (u32 i = 0; i < deviceExtensionCount; i++)
        {
            HLS_LOG("Requested device extension %s", deviceExtensions[i]);
        }

        if (!PhysicalDeviceSupportsRequiredFeatures(deviceFeatures2, info))
        {
            continue;
        }

        i32 graphicsComputeQueueFamilyIndex = -1;
        for (u32 j = 0; j < info.queueFamilyCount; j++)
        {
            const VkQueueFamilyProperties& queueFamily = info.queueFamilies[j];
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT &&
                queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                graphicsComputeQueueFamilyIndex = j;
                break;
            }
        }
        if (graphicsComputeQueueFamilyIndex == -1)
        {
            continue;
        }

        i32 presentQueueFamilyIndex = -1;
        VkSurfaceFormatKHR surfaceFormat;
        VkPresentModeKHR presentMode;
        if (context.windowPtr)
        {
            for (u32 j = 0; j < info.queueFamilyCount; j++)
            {
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(
                    physicalDevices[i], j, context.surface, &presentSupport);
                if (presentSupport)
                {
                    presentQueueFamilyIndex = j;
                    break;
                }
            }
            if (presentQueueFamilyIndex == -1)
            {
                continue;
            }

            surfaceFormat = DefaultDetermineSurfaceFormat(
                info.surfaceFormats, info.surfaceFormatCount);
            presentMode = DefaultDeterminePresentMode(info.presentModes,
                                                      info.presentModeCount);

            if (determineSurfaceFormatCallback != nullptr &&
                !determineSurfaceFormatCallback(context, info, surfaceFormat))
            {
                continue;
            }

            if (determinePresentModeCallback != nullptr &&
                !determinePresentModeCallback(context, info, presentMode))
            {
                continue;
            }
        }

        if (isPhysicalDeviceSuitableCallback != nullptr &&
            !isPhysicalDeviceSuitableCallback(context, info))
        {
            continue;
        }

        suitableDeviceIndices[suitableDeviceCount] = i;
        suitableDevices[suitableDeviceCount].physicalDevice =
            physicalDevices[i];
        suitableDevices[suitableDeviceCount].graphicsComputeQueueFamilyIndex =
            graphicsComputeQueueFamilyIndex;
        if (context.windowPtr)
        {
            suitableDevices[suitableDeviceCount].presentQueueFamilyIndex =
                presentQueueFamilyIndex;
            suitableDevices[suitableDeviceCount].surfaceFormat = surfaceFormat;
            suitableDevices[suitableDeviceCount].presentMode = presentMode;
        }
        suitableDeviceCount++;
    }

    context.deviceCount =
        MIN(HLS_PHYSICAL_DEVICE_MAX_COUNT, suitableDeviceCount);
    for (u32 i = 0; i < context.deviceCount; i++)
    {
        HlsDevice& device = context.devices[i];
        const HlsPhysicalDeviceInfo& info =
            physicalDeviceInfos[suitableDeviceIndices[i]];

        context.devices[i] = suitableDevices[i];
        HLS_LOG("Following physical device is suitable and selected - %s",
                info.properties.deviceName);
    }

    ArenaPopToMarker(arena, marker);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// Surface & Swapchain
/////////////////////////////////////////////////////////////////////////////
static bool CreateSurface(HlsContext& context)
{
    HLS_ASSERT(context.windowPtr);
    return glfwCreateWindowSurface(context.instance, context.windowPtr, nullptr,
                                   &context.surface) == VK_SUCCESS;
}

static void DestroySurface(HlsContext& context)
{
    HLS_ASSERT(context.windowPtr);
    vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
}

static bool CreateSwapchainImageViews(HlsContext& context, HlsDevice& device)
{
    HLS_ASSERT(context.windowPtr);
    // Create swapchain image views
    for (u32 i = 0; i < device.swapchainImageCount; i++)
    {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = device.swapchainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = device.surfaceFormat.format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VkResult res =
            vkCreateImageView(device.logicalDevice, &createInfo, nullptr,
                              &device.swapchainImageViews[i]);
        if (res != VK_SUCCESS)
        {
            return false;
        }
    }
    return true;
}

static void DestroySwapchainImageViews(HlsContext& context, HlsDevice& device)
{
    HLS_ASSERT(context.windowPtr);

    for (u32 i = 0; i < device.swapchainImageCount; i++)
    {
        vkDestroyImageView(device.logicalDevice, device.swapchainImageViews[i],
                           nullptr);
    }
}

static bool CreateSwapchain(HlsContext& context, HlsDevice& device,
                            VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE)
{
    HLS_ASSERT(context.windowPtr);

    i32 width = 0;
    i32 height = 0;
    glfwGetFramebufferSize(context.windowPtr, &width, &height);

    HLS_ASSERT(width > 0);
    HLS_ASSERT(height > 0);

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        device.physicalDevice, context.surface, &surfaceCapabilities);

    VkExtent2D extent;
    extent.width =
        CLAMP(static_cast<u32>(width), surfaceCapabilities.minImageExtent.width,
              surfaceCapabilities.maxImageExtent.width);
    extent.height = CLAMP(static_cast<u32>(height),
                          surfaceCapabilities.minImageExtent.height,
                          surfaceCapabilities.maxImageExtent.height);

    HLS_ASSERT(device.graphicsComputeQueueFamilyIndex != -1);
    HLS_ASSERT(device.presentQueueFamilyIndex != -1);

    u32 queueFamilyIndices[2] = {
        static_cast<u32>(device.graphicsComputeQueueFamilyIndex),
        static_cast<u32>(device.presentQueueFamilyIndex)};

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = context.surface;
    createInfo.minImageCount = surfaceCapabilities.minImageCount + 1;
    createInfo.imageFormat = device.surfaceFormat.format;
    createInfo.imageColorSpace = device.surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.preTransform = surfaceCapabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = device.presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchain;

    if (device.graphicsComputeQueueFamilyIndex ==
        device.presentQueueFamilyIndex)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }

    VkResult res = vkCreateSwapchainKHR(device.logicalDevice, &createInfo,
                                        nullptr, &device.swapchain);
    if (res != VK_SUCCESS)
    {
        return false;
    }

    // Get swapchain images
    res = vkGetSwapchainImagesKHR(device.logicalDevice, device.swapchain,
                                  &device.swapchainImageCount, nullptr);
    HLS_ASSERT(device.swapchainImageCount <= HLS_SWAPCHAIN_IMAGE_MAX_COUNT);
    if (res != VK_SUCCESS)
    {
        return false;
    }
    vkGetSwapchainImagesKHR(device.logicalDevice, device.swapchain,
                            &device.swapchainImageCount,
                            device.swapchainImages);

    if (!CreateSwapchainImageViews(context, device))
    {
        return false;
    }

    HLS_LOG("Swapchain format: %s",
            FormatToString(device.surfaceFormat.format));
    HLS_LOG("Color space: %s",
            ColorSpaceToString(device.surfaceFormat.colorSpace));
    HLS_LOG("Present mode: %s", PresentModeToString(device.presentMode));
    HLS_LOG("Swapchain image count: %u", device.swapchainImageCount);

    return true;
}

static void DestroySwapchain(HlsContext& context, HlsDevice& device)
{
    HLS_ASSERT(context.windowPtr);

    DestroySwapchainImageViews(context, device);
    vkDestroySwapchainKHR(device.logicalDevice, device.swapchain, nullptr);
}

static bool RecreateSwapchain(HlsContext& context, HlsDevice& device)
{
    HLS_ASSERT(context.windowPtr);

    vkDeviceWaitIdle(device.logicalDevice);

    VkSwapchainKHR oldSwapchain = device.swapchain;
    VkImageView oldSwapchainImageViews[HLS_SWAPCHAIN_IMAGE_MAX_COUNT];
    u32 oldSwapchainImageCount = device.swapchainImageCount;
    for (u32 i = 0; i < device.swapchainImageCount; i++)
    {
        oldSwapchainImageViews[i] = device.swapchainImageViews[i];
    }
    if (!CreateSwapchain(context, device, oldSwapchain))
    {
        return false;
    }

    if (oldSwapchain != VK_NULL_HANDLE)
    {
        for (u32 i = 0; i < oldSwapchainImageCount; i++)
        {
            vkDestroyImageView(device.logicalDevice, oldSwapchainImageViews[i],
                               nullptr);
        }
        vkDestroySwapchainKHR(device.logicalDevice, oldSwapchain, nullptr);
    }

    HLS_LOG("Swapchain format: %s",
            FormatToString(device.surfaceFormat.format));
    HLS_LOG("Color space: %s",
            ColorSpaceToString(device.surfaceFormat.colorSpace));
    HLS_LOG("Present mode: %s", PresentModeToString(device.presentMode));
    HLS_LOG("Swapchain image count: %u", device.swapchainImageCount);
}

/////////////////////////////////////////////////////////////////////////////
// LogicalDevice
/////////////////////////////////////////////////////////////////////////////
static bool
CreateLogicalDevices(Arena& arena, const char** extensions, u32 extensionCount,
                     const VkPhysicalDeviceFeatures2& deviceFeatures2,
                     HlsContext& context)
{
    ArenaMarker marker = ArenaGetMarker(arena);

    f32 queuePriority = 1.0f;
    for (u32 i = 0; i < context.deviceCount; i++)
    {
        // Queues
        VkDeviceQueueCreateInfo* queueCreateInfos = HLS_ALLOC(
            arena, VkDeviceQueueCreateInfo,
            1 + static_cast<u32>(static_cast<bool>(context.windowPtr)));
        queueCreateInfos[0] = {};
        queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[0].queueFamilyIndex =
            context.devices[i].graphicsComputeQueueFamilyIndex;
        queueCreateInfos[0].queueCount = 1;
        queueCreateInfos[0].pQueuePriorities = &queuePriority;

        if (context.windowPtr)
        {
            queueCreateInfos[1] = {};
            queueCreateInfos[1].sType =
                VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfos[1].queueFamilyIndex =
                context.devices[i].presentQueueFamilyIndex;
            queueCreateInfos[1].queueCount = 1;
            queueCreateInfos[1].pQueuePriorities = &queuePriority;
        }

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.enabledExtensionCount = extensionCount;
        createInfo.ppEnabledExtensionNames = extensions;
        createInfo.pQueueCreateInfos = queueCreateInfos;
        if (!context.windowPtr ||
            context.devices[i].graphicsComputeQueueFamilyIndex ==
                context.devices[i].presentQueueFamilyIndex)
        {
            createInfo.queueCreateInfoCount = 1;
        }
        else
        {
            createInfo.queueCreateInfoCount = 2;
        }
        createInfo.pNext = &deviceFeatures2;

        VkResult res =
            vkCreateDevice(context.devices[i].physicalDevice, &createInfo,
                           nullptr, &(context.devices[i].logicalDevice));
        if (res != VK_SUCCESS)
        {
            ArenaPopToMarker(arena, marker);
            return false;
        }

        vkGetDeviceQueue(context.devices[i].logicalDevice,
                         context.devices[i].graphicsComputeQueueFamilyIndex, 0,
                         &(context.devices[i].graphicsComputeQueue));

        if (context.windowPtr)
        {
            vkGetDeviceQueue(context.devices[i].logicalDevice,
                             context.devices[i].presentQueueFamilyIndex, 0,
                             &(context.devices[i].presentQueue));
        }
    }

    ArenaPopToMarker(arena, marker);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// Vma Allocator
/////////////////////////////////////////////////////////////////////////////

static bool CreateVmaAllocators(HlsContext& context)
{
    for (u32 i = 0; i < context.deviceCount; i++)
    {
        VmaVulkanFunctions vulkanFunctions{};
        vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo createInfo{};
        createInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
        createInfo.vulkanApiVersion = VK_API_VERSION_1_4;
        createInfo.physicalDevice = context.devices[i].physicalDevice;
        createInfo.device = context.devices[i].logicalDevice;
        createInfo.instance = context.instance;
        createInfo.pVulkanFunctions = &vulkanFunctions;

        VkResult res =
            vmaCreateAllocator(&createInfo, &(context.devices[i].allocator));
        if (res != VK_SUCCESS)
        {
            return false;
        }
    }

    return true;
}

static void DestroyVmaAllocators(HlsContext& context)
{
    for (u32 i = 0; i < context.deviceCount; i++)
    {
        vmaDestroyAllocator(context.devices[i].allocator);
    }
}

/////////////////////////////////////////////////////////////////////////////
// Context
/////////////////////////////////////////////////////////////////////////////
bool HlsCreateContext(Arena& arena, const HlsContextSettings& settings,
                      HlsContext& context)
{
    if (!CreateInstance(arena, settings.instanceLayers,
                        settings.instanceLayerCount,
                        settings.instanceExtensions,
                        settings.instanceExtensionCount, context))
    {
        return false;
    }

    context.windowPtr = settings.windowPtr;

    if (context.windowPtr && !CreateSurface(context))
    {
        return false;
    }

    if (!FindPhysicalDevices(
            arena, settings.deviceExtensions, settings.deviceExtensionCount,
            settings.deviceFeatures2, settings.isPhysicalDeviceSuitableCallback,
            settings.determineSurfaceFormatCallback,
            settings.determinePresentModeCallback, context))
    {
        return false;
    }

    if (!CreateLogicalDevices(arena, settings.deviceExtensions,
                              settings.deviceExtensionCount,
                              settings.deviceFeatures2, context))
    {
        return false;
    }

    if (!CreateVmaAllocators(context))
    {
        return false;
    }

    if (context.windowPtr)
    {
        for (u32 i = 0; i < context.deviceCount; i++)
        {
            HLS_LOG("Device[%u] swapchain creation:", i);
            if (!CreateSwapchain(context, context.devices[i]))
            {
                return false;
            }
        }
    }

    return true;
}

void HlsDestroyContext(HlsContext& context)
{
    if (context.windowPtr)
    {
        for (u32 i = 0; i < context.deviceCount; i++)
        {
            DestroySwapchain(context, context.devices[i]);
        }
    }
    DestroyVmaAllocators(context);

    for (u32 i = 0; i < context.deviceCount; i++)
    {
        vkDestroyDevice(context.devices[i].logicalDevice, nullptr);
    }

    if (context.windowPtr)
    {
        DestroySurface(context);
    }
    vkDestroyInstance(context.instance, nullptr);
}
