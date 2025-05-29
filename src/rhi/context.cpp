#include <stdio.h>
#include <string.h>

#include "core/assert.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "allocation_callbacks.h"
#include "context.h"
#include "surface.h"

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

namespace Hls
{
namespace RHI
{

bool IsLayerSupported(VkLayerProperties* layerProperties,
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

bool IsExtensionSupported(VkExtensionProperties* extensionProperties,
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

static bool CreateInstance(const char** instanceLayers, u32 instanceLayerCount,
                           const char** instanceExtensions,
                           u32 instanceExtensionCount, Context& context)
{
    Arena& arena = GetScratchArena();
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
        if (!IsLayerSupported(availableInstanceLayers,
                              availableInstanceLayerCount, totalLayers[i]))
        {
            HLS_LOG("A");
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
        if (!IsExtensionSupported(availableInstanceExtensions,
                                  availableInstanceExtensionCount,
                                  totalExtensions[i]))
        {
            HLS_LOG("B");
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
    appInfo.apiVersion = VK_API_VERSION_1_3;

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
    VkResult result = vkCreateInstance(
        &createInfo, GetVulkanAllocationCallbacks(), &instance);
    ArenaPopToMarker(arena, marker);

    if (result != VK_SUCCESS)
    {
        HLS_LOG("Failed code %d", static_cast<i32>(result));
        return false;
    }
    HLS_DEBUG_LOG("Created vulkan instance");

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

static const PhysicalDeviceInfo*
QueryPhysicalDevicesInformation(Arena& arena, const Context& context,
                                VkPhysicalDevice* physicalDevices,
                                u32 physicalDeviceCount)
{
    HLS_ASSERT(physicalDevices);

    if (physicalDeviceCount == 0)
    {
        return nullptr;
    }

    PhysicalDeviceInfo* physicalDeviceInfos =
        HLS_ALLOC(arena, PhysicalDeviceInfo, physicalDeviceCount);

    for (u32 i = 0; i < physicalDeviceCount; i++)
    {
        VkPhysicalDevice physicalDevice = physicalDevices[i];
        PhysicalDeviceInfo& info = physicalDeviceInfos[i];

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

        info.shaderDrawParametersFeatures = {};
        info.shaderDrawParametersFeatures.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
        info.shaderDrawParametersFeatures.pNext =
            &info.accelerationStructureFeatures;

        info.vulkan12Features = {};
        info.vulkan12Features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        info.vulkan12Features.pNext = &info.shaderDrawParametersFeatures;

        info.dynamicRenderingFeatures = {};
        info.dynamicRenderingFeatures.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
        info.dynamicRenderingFeatures.pNext = &info.vulkan12Features;

        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &info.dynamicRenderingFeatures;

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

static bool PhysicalDeviceSupportsVulkan12Features(
    const VkPhysicalDeviceVulkan12Features& requested,
    const VkPhysicalDeviceVulkan12Features& supported)
{
    if (requested.samplerMirrorClampToEdge &&
        !supported.samplerMirrorClampToEdge)
    {
        return false;
    }

    if (requested.drawIndirectCount && !supported.drawIndirectCount)
    {
        return false;
    }

    if (requested.storageBuffer8BitAccess && !supported.storageBuffer8BitAccess)
    {
        return false;
    }

    if (requested.uniformAndStorageBuffer8BitAccess &&
        !supported.uniformAndStorageBuffer8BitAccess)
    {
        return false;
    }

    if (requested.storagePushConstant8 && !supported.storagePushConstant8)
    {
        return false;
    }

    if (requested.shaderBufferInt64Atomics &&
        !supported.shaderBufferInt64Atomics)
    {
        return false;
    }

    if (requested.shaderSharedInt64Atomics &&
        !supported.shaderSharedInt64Atomics)
    {
        return false;
    }

    if (requested.shaderFloat16 && !supported.shaderFloat16)
    {
        return false;
    }

    if (requested.shaderInt8 && !supported.shaderInt8)
    {
        return false;
    }

    if (requested.descriptorIndexing && !supported.descriptorIndexing)
    {
        return false;
    }

    if (requested.shaderInputAttachmentArrayDynamicIndexing &&
        !supported.shaderInputAttachmentArrayDynamicIndexing)
    {
        return false;
    }

    if (requested.shaderUniformTexelBufferArrayDynamicIndexing &&
        !supported.shaderUniformTexelBufferArrayDynamicIndexing)
    {
        return false;
    }

    if (requested.shaderStorageTexelBufferArrayDynamicIndexing &&
        !supported.shaderStorageTexelBufferArrayDynamicIndexing)
    {
        return false;
    }

    if (requested.shaderUniformBufferArrayNonUniformIndexing &&
        !supported.shaderUniformBufferArrayNonUniformIndexing)
    {
        return false;
    }

    if (requested.shaderSampledImageArrayNonUniformIndexing &&
        !supported.shaderSampledImageArrayNonUniformIndexing)
    {
        return false;
    }

    if (requested.shaderStorageBufferArrayNonUniformIndexing &&
        !supported.shaderStorageBufferArrayNonUniformIndexing)
    {
        return false;
    }

    if (requested.shaderStorageImageArrayNonUniformIndexing &&
        !supported.shaderStorageImageArrayNonUniformIndexing)
    {
        return false;
    }

    if (requested.shaderInputAttachmentArrayNonUniformIndexing &&
        !supported.shaderInputAttachmentArrayNonUniformIndexing)
    {
        return false;
    }

    if (requested.shaderUniformTexelBufferArrayNonUniformIndexing &&
        !supported.shaderUniformTexelBufferArrayNonUniformIndexing)
    {
        return false;
    }

    if (requested.shaderStorageTexelBufferArrayNonUniformIndexing &&
        !supported.shaderStorageTexelBufferArrayNonUniformIndexing)
    {
        return false;
    }

    if (requested.descriptorBindingUniformBufferUpdateAfterBind &&
        !supported.descriptorBindingUniformBufferUpdateAfterBind)
    {
        return false;
    }

    if (requested.descriptorBindingSampledImageUpdateAfterBind &&
        !supported.descriptorBindingSampledImageUpdateAfterBind)
    {
        return false;
    }

    if (requested.descriptorBindingStorageImageUpdateAfterBind &&
        !supported.descriptorBindingStorageImageUpdateAfterBind)
    {
        return false;
    }

    if (requested.descriptorBindingStorageBufferUpdateAfterBind &&
        !supported.descriptorBindingStorageBufferUpdateAfterBind)
    {
        return false;
    }

    if (requested.descriptorBindingUniformTexelBufferUpdateAfterBind &&
        !supported.descriptorBindingUniformTexelBufferUpdateAfterBind)
    {
        return false;
    }

    if (requested.descriptorBindingStorageTexelBufferUpdateAfterBind &&
        !supported.descriptorBindingStorageTexelBufferUpdateAfterBind)
    {
        return false;
    }

    if (requested.descriptorBindingUpdateUnusedWhilePending &&
        !supported.descriptorBindingUpdateUnusedWhilePending)
    {
        return false;
    }

    if (requested.descriptorBindingPartiallyBound &&
        !supported.descriptorBindingPartiallyBound)
    {
        return false;
    }

    if (requested.descriptorBindingPartiallyBound &&
        !supported.descriptorBindingPartiallyBound)
    {
        return false;
    }

    if (requested.descriptorBindingVariableDescriptorCount &&
        !supported.descriptorBindingVariableDescriptorCount)
    {
        return false;
    }

    if (requested.runtimeDescriptorArray && !supported.runtimeDescriptorArray)
    {
        return false;
    }

    if (requested.samplerFilterMinmax && !supported.samplerFilterMinmax)
    {
        return false;
    }

    if (requested.scalarBlockLayout && !supported.scalarBlockLayout)
    {
        return false;
    }

    if (requested.imagelessFramebuffer && !supported.imagelessFramebuffer)
    {
        return false;
    }

    if (requested.uniformBufferStandardLayout &&
        !supported.uniformBufferStandardLayout)
    {
        return false;
    }

    if (requested.shaderSubgroupExtendedTypes &&
        !supported.shaderSubgroupExtendedTypes)
    {
        return false;
    }

    if (requested.separateDepthStencilLayouts &&
        !supported.separateDepthStencilLayouts)
    {
        return false;
    }

    if (requested.hostQueryReset && !supported.hostQueryReset)
    {
        return false;
    }

    if (requested.timelineSemaphore && !supported.timelineSemaphore)
    {
        return false;
    }

    if (requested.bufferDeviceAddress && !supported.bufferDeviceAddress)
    {
        return false;
    }

    if (requested.bufferDeviceAddressCaptureReplay &&
        !supported.bufferDeviceAddressCaptureReplay)
    {
        return false;
    }

    if (requested.bufferDeviceAddressMultiDevice &&
        !supported.bufferDeviceAddressMultiDevice)
    {
        return false;
    }

    if (requested.vulkanMemoryModel && !supported.vulkanMemoryModel)
    {
        return false;
    }

    if (requested.vulkanMemoryModelDeviceScope &&
        !supported.vulkanMemoryModelDeviceScope)
    {
        return false;
    }

    if (requested.vulkanMemoryModelAvailabilityVisibilityChains &&
        !supported.vulkanMemoryModelAvailabilityVisibilityChains)
    {
        return false;
    }

    if (requested.shaderOutputViewportIndex &&
        !supported.shaderOutputViewportIndex)
    {
        return false;
    }

    if (requested.shaderOutputLayer && !supported.shaderOutputLayer)
    {
        return false;
    }

    if (requested.subgroupBroadcastDynamicId &&
        !supported.subgroupBroadcastDynamicId)
    {
        return false;
    }

    return true;
}

static bool PhysicalDeviceSupportsDynamicRenderingFeatures(
    const VkPhysicalDeviceDynamicRenderingFeatures& requested,
    const VkPhysicalDeviceDynamicRenderingFeatures& supported)
{
    return !requested.dynamicRendering || supported.dynamicRendering;
}

static bool PhysicalDeviceSupportsShaderDrawParametersFeatures(
    const VkPhysicalDeviceShaderDrawParametersFeatures& requested,
    const VkPhysicalDeviceShaderDrawParametersFeatures& supported)
{
    return !requested.shaderDrawParameters || supported.shaderDrawParameters;
}

static bool PhysicalDeviceSupportsRequiredFeatures(
    const VkPhysicalDeviceFeatures2& requested,
    const PhysicalDeviceInfo& supported)
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
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
            {
                const VkPhysicalDeviceVulkan12Features*
                    requestedVulkan12Features = reinterpret_cast<
                        const VkPhysicalDeviceVulkan12Features*>(
                        currRequestedFeaturePtr);
                if (!PhysicalDeviceSupportsVulkan12Features(
                        *requestedVulkan12Features, supported.vulkan12Features))
                {
                    return false;
                }
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES:
            {
                const VkPhysicalDeviceDynamicRenderingFeatures*
                    requestedDynamicRenderingFeatures = reinterpret_cast<
                        const VkPhysicalDeviceDynamicRenderingFeatures*>(
                        currRequestedFeaturePtr);
                if (!PhysicalDeviceSupportsDynamicRenderingFeatures(
                        *requestedDynamicRenderingFeatures,
                        supported.dynamicRenderingFeatures))
                {
                    return false;
                }
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES:
            {
                const VkPhysicalDeviceShaderDrawParametersFeatures*
                    requestedShaderDrawParametersFeatures = reinterpret_cast<
                        const VkPhysicalDeviceShaderDrawParametersFeatures*>(
                        currRequestedFeaturePtr);
                if (!PhysicalDeviceSupportsShaderDrawParametersFeatures(
                        *requestedShaderDrawParametersFeatures,
                        supported.shaderDrawParametersFeatures))
                {
                    return false;
                }
                break;
            }
            default:
            {
                break;
            }
        }
        currRequestedFeaturePtr = currRequestedFeaturePtr->pNext;
    }

    return true;
}

static void
LogDetectedPhysicalDevices(const PhysicalDeviceInfo* physicalDeviceInfos,
                           u32 physicalDeviceCount)
{
    HLS_LOG("List of physical devices detected:");
    for (u32 i = 0; i < physicalDeviceCount; i++)
    {
        const PhysicalDeviceInfo& info = physicalDeviceInfos[i];

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

static bool PhysicalDeviceSupportsRequiredExtensions(
    const char** extensions, u32 extensionCount, const PhysicalDeviceInfo& info)
{

    for (u32 i = 0; i < extensionCount; i++)
    {
        if (!IsExtensionSupported(info.extensions, info.extensionCount,
                                  extensions[i]))
        {
            return false;
        }
    }
    return true;
}

static bool
FindPhysicalDevices(const char** deviceExtensions, u32 deviceExtensionCount,
                    const VkPhysicalDeviceFeatures2& deviceFeatures2,
                    IsPhysicalDeviceSuitableFn isPhysicalDeviceSuitableCallback,
                    DetermineSurfaceFormatFn determineSurfaceFormatCallback,
                    DeterminePresentModeFn determinePresentModeCallback,
                    Context& context)
{
    Arena& arena = GetScratchArena();
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
    const PhysicalDeviceInfo* physicalDeviceInfos =
        QueryPhysicalDevicesInformation(arena, context, physicalDevices,
                                        physicalDeviceCount);

    LogDetectedPhysicalDevices(physicalDeviceInfos, physicalDeviceCount);

    u32 suitableDeviceCount = 0;
    Device* suitableDevices = HLS_ALLOC(arena, Device, physicalDeviceCount);
    for (u32 i = 0; i < physicalDeviceCount; i++)
    {
        suitableDevices[i] = {};
    }

    for (u32 i = 0; i < physicalDeviceCount; i++)
    {
        const PhysicalDeviceInfo& info = physicalDeviceInfos[i];

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

        suitableDevices[suitableDeviceCount].physicalDevice =
            physicalDevices[i];
        suitableDevices[suitableDeviceCount].graphicsComputeQueueFamilyIndex =
            graphicsComputeQueueFamilyIndex;
        strncpy(suitableDevices[suitableDeviceCount].name,
                info.properties.deviceName,
                VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1);
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
        context.devices[i] = suitableDevices[i];
        HLS_LOG("Following physical device is suitable and selected - %s",
                context.devices[i].name);
    }

    ArenaPopToMarker(arena, marker);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
// Context
/////////////////////////////////////////////////////////////////////////////
bool CreateContext(ContextSettings& settings, Context& context)
{
    HLS_LOG("Trying to create instance");
    if (!CreateInstance(settings.instanceLayers, settings.instanceLayerCount,
                        settings.instanceExtensions,
                        settings.instanceExtensionCount, context))
    {
        return false;
    }

    context.windowPtr = settings.windowPtr;

    if (context.windowPtr && !CreateSurface(context))
    {
        HLS_LOG("Failed to create surface");
        return false;
    }

    // Fundamental always requested features
    settings.deviceFeatures2.features.multiDrawIndirect = VK_TRUE;

    VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParametersFeatures{};
    shaderDrawParametersFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
    shaderDrawParametersFeatures.shaderDrawParameters = VK_TRUE;
    shaderDrawParametersFeatures.pNext = nullptr;

    VkPhysicalDeviceVulkan12Features vulkan12Features{};
    vulkan12Features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12Features.shaderSampledImageArrayNonUniformIndexing = true;
    vulkan12Features.descriptorBindingSampledImageUpdateAfterBind = true;
    vulkan12Features.shaderUniformBufferArrayNonUniformIndexing = true;
    vulkan12Features.descriptorBindingUniformBufferUpdateAfterBind = true;
    vulkan12Features.shaderStorageBufferArrayNonUniformIndexing = true;
    vulkan12Features.descriptorBindingStorageBufferUpdateAfterBind = true;
    vulkan12Features.descriptorBindingPartiallyBound = true;
    vulkan12Features.runtimeDescriptorArray = true;
    vulkan12Features.drawIndirectCount = true;
    vulkan12Features.pNext = &shaderDrawParametersFeatures;

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{};
    dynamicRenderingFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRenderingFeatures.dynamicRendering = VK_TRUE;
    dynamicRenderingFeatures.pNext = &vulkan12Features;

    VkPhysicalDeviceSynchronization2Features synchronization2Features{};
    synchronization2Features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    synchronization2Features.synchronization2 = VK_TRUE;
    synchronization2Features.pNext = &dynamicRenderingFeatures;

    VkBaseOutStructure* currRequestedFeaturePtr =
        reinterpret_cast<VkBaseOutStructure*>(&settings.deviceFeatures2);
    while (currRequestedFeaturePtr->pNext != nullptr)
    {
        currRequestedFeaturePtr = currRequestedFeaturePtr->pNext;
    }
    currRequestedFeaturePtr->pNext =
        reinterpret_cast<VkBaseOutStructure*>(&synchronization2Features);

    if (!FindPhysicalDevices(
            settings.deviceExtensions, settings.deviceExtensionCount,
            settings.deviceFeatures2, settings.isPhysicalDeviceSuitableCallback,
            settings.determineSurfaceFormatCallback,
            settings.determinePresentModeCallback, context))
    {
        return false;
    }

    for (u32 i = 0; i < context.deviceCount; i++)
    {
        if (!CreateLogicalDevice(
                settings.deviceExtensions, settings.deviceExtensionCount,
                settings.deviceFeatures2, context, context.devices[i]))
        {
            return false;
        }
    }

    return true;
}

void DestroyContext(Context& context)
{
    for (u32 i = 0; i < context.deviceCount; i++)
    {
        DestroyLogicalDevice(context.devices[i]);
    }

    if (context.windowPtr)
    {
        DestroySurface(context);
    }

    vkDestroyInstance(context.instance, GetVulkanAllocationCallbacks());
    HLS_DEBUG_LOG("Vulkan instance is destroyed");
}

void WaitAllDevicesIdle(Context& context)
{
    for (u32 i = 0; i < context.deviceCount; i++)
    {
        vkDeviceWaitIdle(context.devices[i].logicalDevice);
    }
}

} // namespace RHI
} // namespace Hls
