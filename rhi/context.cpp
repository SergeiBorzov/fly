#include <stdio.h>
#include <string.h>

#include "context.h" // volk should be included prior to glfw
#include <GLFW/glfw3.h>

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
        HLS_DEBUG_LOG("Requested instance layer %s", instanceLayers[i]);
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
        HLS_DEBUG_LOG("Requested instance extension %s", instanceExtensions[i]);
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

static const HlsPhysicalDeviceInfo*
QueryPhysicalDevicesInformation(Arena& arena, VkPhysicalDevice* physicalDevices,
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
    bool renderOffscreen, HlsContext& context)
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
        QueryPhysicalDevicesInformation(arena, physicalDevices,
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
        if (!renderOffscreen)
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
        }

        if (isPhysicalDeviceSuitableCallback != nullptr &&
            !isPhysicalDeviceSuitableCallback(info))
        {
            continue;
        }

        suitableDeviceIndices[suitableDeviceCount] = i;
        suitableDevices[suitableDeviceCount].physicalDevice =
            physicalDevices[i];
        suitableDevices[suitableDeviceCount].graphicsComputeQueueFamilyIndex =
            graphicsComputeQueueFamilyIndex;
        if (!renderOffscreen)
        {
            suitableDevices[suitableDeviceCount].presentQueueFamilyIndex =
                presentQueueFamilyIndex;
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
        HLS_LOG("Following physical device is suitable and selected [%u] - %s",
                i, info.properties.deviceName);
    }

    ArenaPopToMarker(arena, marker);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// Surface
/////////////////////////////////////////////////////////////////////////////
static bool CreateSurface(GLFWwindow* window, HlsContext& context)
{
    return glfwCreateWindowSurface(context.instance, window, nullptr,
                                   &context.surface) == VK_SUCCESS;
}

static void DestroySurface(HlsContext& context)
{
    vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
}

/////////////////////////////////////////////////////////////////////////////
// LogicalDevice
/////////////////////////////////////////////////////////////////////////////
static bool
CreateLogicalDevices(Arena& arena, const char** extensions, u32 extensionCount,
                     const VkPhysicalDeviceFeatures2& deviceFeatures2,
                     bool renderOffscreen, HlsContext& context)
{
    ArenaMarker marker = ArenaGetMarker(arena);

    f32 queuePriority = 1.0f;
    for (u32 i = 0; i < context.deviceCount; i++)
    {
        // Queues
        VkDeviceQueueCreateInfo* queueCreateInfos =
            HLS_ALLOC(arena, VkDeviceQueueCreateInfo,
                      1 + static_cast<u32>(!renderOffscreen));
        queueCreateInfos[0] = {};
        queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[0].queueFamilyIndex =
            context.devices[i].graphicsComputeQueueFamilyIndex;
        queueCreateInfos[0].queueCount = 1;
        queueCreateInfos[0].pQueuePriorities = &queuePriority;

        if (!renderOffscreen)
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
        if (renderOffscreen ||
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

        if (!renderOffscreen)
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

    bool renderOffscreen = settings.windowPtr == nullptr;

    if (!renderOffscreen && !CreateSurface(settings.windowPtr, context))
    {
        return false;
    }

    if (!FindPhysicalDevices(
            arena, settings.deviceExtensions, settings.deviceExtensionCount,
            settings.deviceFeatures2, settings.isPhysicalDeviceSuitableCallback,
            renderOffscreen, context))
    {
        return false;
    }

    if (!CreateLogicalDevices(
            arena, settings.deviceExtensions, settings.deviceExtensionCount,
            settings.deviceFeatures2, renderOffscreen, context))
    {
        return false;
    }

    return true;
}

void HlsDestroyContext(HlsContext& context)
{
    for (u32 i = 0; i < context.deviceCount; i++)
    {
        vkDestroyDevice(context.devices[i].logicalDevice, nullptr);
    }

    if (context.surface != VK_NULL_HANDLE)
    {
        DestroySurface(context);
    }
    vkDestroyInstance(context.instance, nullptr);
}
