#include <stdio.h>
#include <string.h>

#include "core/assert.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "allocation_callbacks.h"
#include "context.h"
#include "surface.h"

#include <GLFW/glfw3.h>

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
        FLY_DEBUG_LOG("%s", callbackData->pMessage);
    }
    return VK_FALSE;
}
#endif

static Fly::RHI::Context* sContext = nullptr;

static void OnFramebufferResize(GLFWwindow* window, int width, int height)
{
    for (u32 i = 0; i < sContext->deviceCount; i++)
    {
        if (width == static_cast<int>(sContext->devices[i].swapchainWidth) &&
            height == static_cast<int>(sContext->devices[i].swapchainHeight))
        {
            continue;
        }
        sContext->devices[i].isFramebufferResized = true;
    }

    if (sContext->framebufferResizeCallback)
    {
        sContext->framebufferResizeCallback(window, width, height);
    }
}

namespace Fly
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
        availableInstanceLayers = FLY_PUSH_ARENA(arena, VkLayerProperties,
                                                 availableInstanceLayerCount);
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
        availableInstanceExtensions = FLY_PUSH_ARENA(
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
    totalLayers = FLY_PUSH_ARENA(arena, const char*, totalLayerCount);
    for (u32 i = 0; i < instanceLayerCount; i++)
    {
        FLY_LOG("Requested instance layer %s", instanceLayers[i]);
        totalLayers[i] = instanceLayers[i];
    }
    FLY_DEBUG_LOG(
        "Adding VK_LAYER_KHRONOS_validation to list of instance layers");
    totalLayers[instanceLayerCount] = "VK_LAYER_KHRONOS_validation";
#endif
    for (u32 i = 0; i < totalLayerCount; i++)
    {
        if (!IsLayerSupported(availableInstanceLayers,
                              availableInstanceLayerCount, totalLayers[i]))
        {
            FLY_ERROR("Following required instance layer %s is not present",
                      totalLayers[i]);
            return false;
        }
    }

    // Check Extensions
    u32 totalExtensionCount = instanceExtensionCount;
    const char** totalExtensions = instanceExtensions;
#ifndef NDEBUG

    totalExtensionCount += 1;
    totalExtensions = FLY_PUSH_ARENA(arena, const char*, totalExtensionCount);
    for (u32 i = 0; i < instanceExtensionCount; i++)
    {
        FLY_LOG("Requested instance extension %s", instanceExtensions[i]);
        totalExtensions[i] = instanceExtensions[i];
    }
    FLY_DEBUG_LOG("Adding VK_EXT_debug_utils to list of instance extensions");
    totalExtensions[instanceExtensionCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
#endif
    for (u32 i = 0; i < totalExtensionCount; i++)
    {
        if (!IsExtensionSupported(availableInstanceExtensions,
                                  availableInstanceExtensionCount,
                                  totalExtensions[i]))
        {
            FLY_ERROR("Following required extension %s is not present",
                      totalExtensions[i]);
            return false;
        }
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Fly";
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
        FLY_LOG("Failed code %d", static_cast<i32>(result));
        return false;
    }
    FLY_DEBUG_LOG("Created vulkan instance");

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
    FLY_ASSERT(surfaceFormats);
    FLY_ASSERT(surfaceFormatCount > 0);

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
    FLY_ASSERT(presentModes);
    FLY_ASSERT(presentModeCount > 0);

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
    FLY_ASSERT(physicalDevices);

    if (physicalDeviceCount == 0)
    {
        return nullptr;
    }

    PhysicalDeviceInfo* physicalDeviceInfos =
        FLY_PUSH_ARENA(arena, PhysicalDeviceInfo, physicalDeviceCount);

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
            info.extensions = FLY_PUSH_ARENA(arena, VkExtensionProperties,
                                             info.extensionCount);
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
                info.surfaceFormats = FLY_PUSH_ARENA(arena, VkSurfaceFormatKHR,
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
                info.presentModes = FLY_PUSH_ARENA(arena, VkPresentModeKHR,
                                                   info.presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(
                    physicalDevice, context.surface, &info.presentModeCount,
                    info.presentModes);
            }
        }

        // Device features
        info.vulkan13Features = {};
        info.vulkan13Features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        info.vulkan12Features = {};
        info.vulkan12Features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        info.vulkan12Features.pNext = &info.vulkan13Features;

        info.vulkan11Features = {};
        info.vulkan11Features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        info.vulkan11Features.pNext = &info.vulkan12Features;

        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &info.vulkan11Features;

        vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);
        info.features = deviceFeatures2.features;

        // Queue Family properties
        vkGetPhysicalDeviceQueueFamilyProperties(
            physicalDevice, &info.queueFamilyCount, nullptr);
        if (info.queueFamilyCount > 0)
        {
            info.queueFamilies = FLY_PUSH_ARENA(arena, VkQueueFamilyProperties,
                                                info.queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(
                physicalDevice, &info.queueFamilyCount, info.queueFamilies);
        }
    }

    return physicalDeviceInfos;
}

static bool PhysicalDeviceSupportsVulkan11Features(
    const VkPhysicalDeviceVulkan11Features& requested,
    const VkPhysicalDeviceVulkan11Features& supported)
{
    if (requested.storageBuffer16BitAccess && !supported.storageBuffer16BitAccess)
    {
        return false;
    }

    if (requested.uniformAndStorageBuffer16BitAccess && !supported.uniformAndStorageBuffer16BitAccess)
    {
        return false;
    }

    if (requested.storagePushConstant16 && !supported.storagePushConstant16)
    {
        return false;
    }

    if (requested.storageInputOutput16 && !supported.storageInputOutput16)
    {
        return false;
    }

    if (requested.multiview && !supported.multiview)
    {
        return false;
    }

    if (requested.multiviewGeometryShader && !supported.multiviewGeometryShader)
    {
        return false;
    }

    if (requested.multiviewTessellationShader && !supported.multiviewTessellationShader)
    {
        return false;
    }

    if (requested.variablePointersStorageBuffer && !supported.variablePointersStorageBuffer)
    {
        return false;
    }

    if (requested.variablePointers && !supported.variablePointers)
    {
        return false;
    }

    if (requested.protectedMemory && !supported.protectedMemory)
    {
        return false;
    }

    if (requested.samplerYcbcrConversion && !supported.samplerYcbcrConversion)
    {
        return false;
    }

    if (requested.shaderDrawParameters && !supported.shaderDrawParameters)
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

static bool PhysicalDeviceSupportsVulkan13Features(
    const VkPhysicalDeviceVulkan13Features& requested,
    const VkPhysicalDeviceVulkan13Features& supported)
{
    if (requested.robustImageAccess && !supported.robustImageAccess)
    {
        return false;
    }

    if (requested.inlineUniformBlock && !supported.inlineUniformBlock)
    {
        return false;
    }

    if (requested.descriptorBindingInlineUniformBlockUpdateAfterBind &&
        !supported.descriptorBindingInlineUniformBlockUpdateAfterBind)
    {
        return false;
    }

    if (requested.pipelineCreationCacheControl &&
        !supported.pipelineCreationCacheControl)
    {
        return false;
    }

    if (requested.privateData && !supported.privateData)
    {
        return false;
    }

    if (requested.shaderDemoteToHelperInvocation &&
        !supported.shaderDemoteToHelperInvocation)
    {
        return false;
    }

    if (requested.shaderTerminateInvocation &&
        !supported.shaderTerminateInvocation)
    {
        return false;
    }

    if (requested.subgroupSizeControl &&
        !supported.subgroupSizeControl)
    {
        return false;
    }

    if (requested.computeFullSubgroups && !supported.computeFullSubgroups)
    {
        return false;
    }

    if (requested.synchronization2 && !supported.synchronization2)
    {
        return false;
    }

    if (requested.textureCompressionASTC_HDR &&
        !supported.textureCompressionASTC_HDR)
    {
        return false;
    }

    if (requested.shaderZeroInitializeWorkgroupMemory &&
        !supported.shaderZeroInitializeWorkgroupMemory)
    {
        return false;
    }

    if (requested.dynamicRendering && !supported.dynamicRendering)
    {
        return false;
    }

    if (requested.shaderIntegerDotProduct && !supported.shaderIntegerDotProduct)
    {
        return false;
    }

    if (requested.maintenance4 && !supported.maintenance4)
    {
        return false;
    }

    return true;
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
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES:
            {
                const VkPhysicalDeviceVulkan11Features*
                    requestedVulkan11Features = reinterpret_cast<
                        const VkPhysicalDeviceVulkan11Features*>(
                        currRequestedFeaturePtr);
                if (!PhysicalDeviceSupportsVulkan11Features(
                        *requestedVulkan11Features, supported.vulkan11Features))
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
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES:
            {
                const VkPhysicalDeviceVulkan13Features*
                    requestedVulkan13Features = reinterpret_cast<
                        const VkPhysicalDeviceVulkan13Features*>(
                        currRequestedFeaturePtr);
                if (!PhysicalDeviceSupportsVulkan13Features(
                        *requestedVulkan13Features, supported.vulkan13Features))
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
    FLY_LOG("List of physical devices detected:");
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

        FLY_LOG("[%u] - %s -- %s -- VRAM %llu MB", i + 1,
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
        FLY_ERROR("No physical devices detected");
        return false;
    }

    VkPhysicalDevice* physicalDevices =
        FLY_PUSH_ARENA(arena, VkPhysicalDevice, physicalDeviceCount);
    vkEnumeratePhysicalDevices(context.instance, &physicalDeviceCount,
                               physicalDevices);
    const PhysicalDeviceInfo* physicalDeviceInfos =
        QueryPhysicalDevicesInformation(arena, context, physicalDevices,
                                        physicalDeviceCount);

    LogDetectedPhysicalDevices(physicalDeviceInfos, physicalDeviceCount);

    u32 suitableDeviceCount = 0;
    Device* suitableDevices =
        FLY_PUSH_ARENA(arena, Device, physicalDeviceCount);
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
            !isPhysicalDeviceSuitableCallback(physicalDevices[i], info))
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
        MIN(FLY_PHYSICAL_DEVICE_MAX_COUNT, suitableDeviceCount);
    for (u32 i = 0; i < context.deviceCount; i++)
    {
        context.devices[i] = suitableDevices[i];
        FLY_LOG("Following physical device is suitable and selected - %s",
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
    FLY_LOG("Trying to create instance");
    if (!CreateInstance(settings.instanceLayers, settings.instanceLayerCount,
                        settings.instanceExtensions,
                        settings.instanceExtensionCount, context))
    {
        return false;
    }

    context.windowPtr = settings.windowPtr;

    if (context.windowPtr && !CreateSurface(context))
    {
        FLY_LOG("Failed to create surface");
        return false;
    }

    // Fundamental always required features
    settings.vulkan13Features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    settings.vulkan13Features.synchronization2 = VK_TRUE;
    settings.vulkan13Features.dynamicRendering = VK_TRUE;

    settings.vulkan12Features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    settings.vulkan12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    settings.vulkan12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    settings.vulkan12Features.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE;
    settings.vulkan12Features.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
    settings.vulkan12Features.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
    settings.vulkan12Features.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    settings.vulkan12Features.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
    settings.vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE;
    settings.vulkan12Features.runtimeDescriptorArray = VK_TRUE;
    settings.vulkan12Features.drawIndirectCount = VK_TRUE;
    settings.vulkan12Features.timelineSemaphore = VK_TRUE;
    settings.vulkan12Features.pNext = &settings.vulkan13Features;

    settings.vulkan11Features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    settings.vulkan11Features.pNext = &settings.vulkan12Features;

    settings.features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    settings.features2.features.samplerAnisotropy = VK_TRUE;
    settings.features2.pNext = &settings.vulkan11Features;

    if (!FindPhysicalDevices(
            settings.deviceExtensions, settings.deviceExtensionCount,
            settings.features2, settings.isPhysicalDeviceSuitableCallback,
            settings.determineSurfaceFormatCallback,
            settings.determinePresentModeCallback, context))
    {
        return false;
    }

    for (u32 i = 0; i < context.deviceCount; i++)
    {
        if (!CreateLogicalDevice(
                settings.deviceExtensions, settings.deviceExtensionCount,
                settings.features2, context, context.devices[i]))
        {
            return false;
        }
    }

    context.framebufferResizeCallback = settings.framebufferResizeCallback;
    sContext = &context;

    if (context.windowPtr)
    {
        glfwSetFramebufferSizeCallback(context.windowPtr, OnFramebufferResize);
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
    FLY_DEBUG_LOG("Vulkan instance is destroyed");
}

void WaitAllDevicesIdle(Context& context)
{
    for (u32 i = 0; i < context.deviceCount; i++)
    {
        vkDeviceWaitIdle(context.devices[i].logicalDevice);
    }
}

} // namespace RHI
} // namespace Fly
