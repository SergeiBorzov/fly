#define VMA_IMPLEMENTATION

#include "core/assert.h"
#include "core/log.h"
#include "core/platform.h"
#include "core/thread_context.h"

#include "allocation_callbacks.h"
#include "context.h"
#include "surface.h"
#include "texture.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(v, min, max) MAX(MIN(v, max), min)

#define FLY_DESCRIPTOR_MAX_COUNT 100000

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
        case VK_FORMAT_D16_UNORM_S8_UINT:
        {
            return "VK_FORMAT_D16_UNORM_S8_UINT";
        }
        case VK_FORMAT_D24_UNORM_S8_UINT:
        {
            return "VK_FORMAT_D24_UNORM_S8_UINT";
        }
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
        {
            return "VK_FORMAT_D32_UNORM_S8_UINT";
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

static bool CreatePipelineLayout(Fly::RHI::Device& device,
                                 VkPipelineLayout& pipelineLayout)
{
    VkPushConstantRange pushConstantRange;
    pushConstantRange.stageFlags = VK_SHADER_STAGE_ALL;
    pushConstantRange.offset = 0;
    pushConstantRange.size = 128;

    VkPipelineLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.setLayoutCount = 1;
    createInfo.pSetLayouts = &device.bindlessDescriptorSetLayout;
    createInfo.pPushConstantRanges = &pushConstantRange;
    createInfo.pushConstantRangeCount = 1;

    return vkCreatePipelineLayout(device.logicalDevice, &createInfo,
                                  Fly::RHI::GetVulkanAllocationCallbacks(),
                                  &pipelineLayout) == VK_SUCCESS;
}

namespace Fly
{
namespace RHI
{

/////////////////////////////////////////////////////////////////////////////
// Swapchain
/////////////////////////////////////////////////////////////////////////////
static bool CreateSwapchainImageViews(Device& device)
{
    FLY_ASSERT(device.context->windowPtr);
    // Create swapchain image views
    for (u32 i = 0; i < device.swapchainTextureCount; i++)
    {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = device.swapchainTextures[i].handle;
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

        VkResult res = vkCreateImageView(
            device.logicalDevice, &createInfo, GetVulkanAllocationCallbacks(),
            &device.swapchainTextures[i].imageView);
        if (res != VK_SUCCESS)
        {
            return false;
        }
    }
    return true;
}

static void DestroySwapchainImageViews(Device& device)
{
    FLY_ASSERT(device.context->windowPtr);

    for (u32 i = 0; i < device.swapchainTextureCount; i++)
    {
        vkDestroyImageView(device.logicalDevice,
                           device.swapchainTextures[i].imageView,
                           GetVulkanAllocationCallbacks());
    }
}

static bool CreateSwapchain(Device& device,
                            VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE)
{
    FLY_ASSERT(device.context);
    FLY_ASSERT(device.context->windowPtr);

    i32 width = 0;
    i32 height = 0;
    GetFramebufferSize(*device.context, width, height);
    while (width == 0 || height == 0)
    {
        PollWindowEvents(*device.context);
        GetFramebufferSize(*device.context, width, height);
    }

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        device.physicalDevice, device.context->surface, &surfaceCapabilities);

    VkExtent2D extent;
    extent.width =
        CLAMP(static_cast<u32>(width), surfaceCapabilities.minImageExtent.width,
              surfaceCapabilities.maxImageExtent.width);
    extent.height = CLAMP(static_cast<u32>(height),
                          surfaceCapabilities.minImageExtent.height,
                          surfaceCapabilities.maxImageExtent.height);

    FLY_ASSERT(device.graphicsComputeQueueFamilyIndex != -1);
    FLY_ASSERT(device.presentQueueFamilyIndex != -1);

    u32 queueFamilyIndices[2] = {
        static_cast<u32>(device.graphicsComputeQueueFamilyIndex),
        static_cast<u32>(device.presentQueueFamilyIndex)};

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = device.context->surface;
    createInfo.minImageCount = surfaceCapabilities.minImageCount + 1;
    createInfo.imageFormat = device.surfaceFormat.format;
    createInfo.imageColorSpace = device.surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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

    VkResult res =
        vkCreateSwapchainKHR(device.logicalDevice, &createInfo,
                             GetVulkanAllocationCallbacks(), &device.swapchain);
    if (res != VK_SUCCESS)
    {
        return false;
    }

    // Get swapchain images
    res = vkGetSwapchainImagesKHR(device.logicalDevice, device.swapchain,
                                  &device.swapchainTextureCount, nullptr);
    if (res != VK_SUCCESS)
    {
        return false;
    }
    FLY_ASSERT(device.swapchainTextureCount <= FLY_SWAPCHAIN_IMAGE_MAX_COUNT);

    VkImage images[FLY_SWAPCHAIN_IMAGE_MAX_COUNT] = {VK_NULL_HANDLE};
    vkGetSwapchainImagesKHR(device.logicalDevice, device.swapchain,
                            &device.swapchainTextureCount, images);
    device.swapchainWidth = extent.width;
    device.swapchainHeight = extent.height;
    for (u32 i = 0; i < device.swapchainTextureCount; i++)
    {
        device.swapchainTextures[i].handle = images[i];
    }

    if (!CreateSwapchainImageViews(device))
    {
        return false;
    }

    FLY_LOG("Device %s created new swapchain %u %u", device.name,
            device.swapchainWidth, device.swapchainHeight);
    FLY_LOG("Swapchain format: %s",
            FormatToString(device.surfaceFormat.format));
    FLY_LOG("Color space: %s",
            ColorSpaceToString(device.surfaceFormat.colorSpace));
    FLY_LOG("Present mode: %s", PresentModeToString(device.presentMode));
    FLY_LOG("Swapchain image count: %u", device.swapchainTextureCount);

    return true;
}

static void DestroySwapchain(Device& device)
{
    FLY_ASSERT(device.context->windowPtr);

    DestroySwapchainImageViews(device);
    vkDestroySwapchainKHR(device.logicalDevice, device.swapchain,
                          GetVulkanAllocationCallbacks());
}

static bool RecreateSwapchain(Device& device)
{
    FLY_ASSERT(device.context);
    FLY_ASSERT(device.context->windowPtr);

    vkDeviceWaitIdle(device.logicalDevice);

    VkSwapchainKHR oldSwapchain = device.swapchain;
    VkImageView oldSwapchainImageViews[FLY_SWAPCHAIN_IMAGE_MAX_COUNT];
    u32 oldSwapchainImageCount = device.swapchainTextureCount;
    for (u32 i = 0; i < device.swapchainTextureCount; i++)
    {
        oldSwapchainImageViews[i] = device.swapchainTextures[i].imageView;
    }
    if (!CreateSwapchain(device, oldSwapchain))
    {
        return false;
    }

    if (oldSwapchain != VK_NULL_HANDLE)
    {
        for (u32 i = 0; i < oldSwapchainImageCount; i++)
        {
            vkDestroyImageView(device.logicalDevice, oldSwapchainImageViews[i],
                               GetVulkanAllocationCallbacks());
        }
        vkDestroySwapchainKHR(device.logicalDevice, oldSwapchain,
                              GetVulkanAllocationCallbacks());
    }

    device.isFramebufferResized = false;

    for (const SwapchainRecreatedCallback& callback :
         device.swapchainRecreatedCallbacks)
    {
        callback.func(device.swapchainWidth, device.swapchainHeight,
                      callback.data);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// Descriptor pool
/////////////////////////////////////////////////////////////////////////////

static bool CreateDescriptorPool(Device& device)
{
    VkDescriptorPoolSize poolSizes[4] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, FLY_DESCRIPTOR_MAX_COUNT},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, FLY_DESCRIPTOR_MAX_COUNT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, FLY_DESCRIPTOR_MAX_COUNT},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, FLY_DESCRIPTOR_MAX_COUNT}};

    VkDescriptorPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.pPoolSizes = poolSizes;
    createInfo.poolSizeCount = STACK_ARRAY_COUNT(poolSizes);
    createInfo.maxSets = 1;
    createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

    if (vkCreateDescriptorPool(device.logicalDevice, &createInfo,
                               GetVulkanAllocationCallbacks(),
                               &device.descriptorPool) != VK_SUCCESS)
    {
        return false;
    }

    VkDescriptorBindingFlags bindingFlags[] = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo{};
    bindingFlagsCreateInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsCreateInfo.pBindingFlags = bindingFlags;
    bindingFlagsCreateInfo.bindingCount = STACK_ARRAY_COUNT(bindingFlags);
    bindingFlagsCreateInfo.pNext = nullptr;

    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, FLY_DESCRIPTOR_MAX_COUNT,
         VK_SHADER_STAGE_ALL, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, FLY_DESCRIPTOR_MAX_COUNT,
         VK_SHADER_STAGE_ALL, nullptr},
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, FLY_DESCRIPTOR_MAX_COUNT,
         VK_SHADER_STAGE_ALL, nullptr},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, FLY_DESCRIPTOR_MAX_COUNT,
         VK_SHADER_STAGE_ALL, nullptr}};

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = STACK_ARRAY_COUNT(bindings);
    descriptorSetLayoutCreateInfo.pBindings = bindings;
    descriptorSetLayoutCreateInfo.flags =
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    descriptorSetLayoutCreateInfo.pNext = &bindingFlagsCreateInfo;

    if (vkCreateDescriptorSetLayout(
            device.logicalDevice, &descriptorSetLayoutCreateInfo,
            GetVulkanAllocationCallbacks(),
            &device.bindlessDescriptorSetLayout) != VK_SUCCESS)
    {
        return false;
    }

    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = device.descriptorPool;
    allocateInfo.pSetLayouts = &device.bindlessDescriptorSetLayout;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pNext = nullptr;

    if (vkAllocateDescriptorSets(device.logicalDevice, &allocateInfo,
                                 &device.bindlessDescriptorSet) != VK_SUCCESS)
    {
        return false;
    }

    return true;
}

static void DestroyDescriptorPool(Device& device)
{
    vkDestroyDescriptorSetLayout(device.logicalDevice,
                                 device.bindlessDescriptorSetLayout,
                                 GetVulkanAllocationCallbacks());
    vkDestroyDescriptorPool(device.logicalDevice, device.descriptorPool,
                            GetVulkanAllocationCallbacks());
}

/////////////////////////////////////////////////////////////////////////////
// Command pools and command buffers
/////////////////////////////////////////////////////////////////////////////

static bool CreateFrameData(Device& device)
{
    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = device.graphicsComputeQueueFamilyIndex;

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = nullptr;
    semaphoreCreateInfo.flags = 0;

    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (vkCreateCommandPool(device.logicalDevice, &createInfo,
                                GetVulkanAllocationCallbacks(),
                                &device.frameData[i].commandPool) != VK_SUCCESS)
        {
            return false;
        };

        if (!CreateCommandBuffers(device, device.frameData[i].commandPool,
                                  &device.frameData[i].commandBuffer, 1))
        {
            return false;
        }

        if (vkCreateFence(device.logicalDevice, &fenceCreateInfo,
                          GetVulkanAllocationCallbacks(),
                          &device.frameData[i].renderFence) != VK_SUCCESS)
        {
            return false;
        }

        if (vkCreateSemaphore(device.logicalDevice, &semaphoreCreateInfo,
                              GetVulkanAllocationCallbacks(),
                              &device.frameData[i].swapchainSemaphore) !=
            VK_SUCCESS)
        {
            return false;
        }

        if (vkCreateSemaphore(device.logicalDevice, &semaphoreCreateInfo,
                              GetVulkanAllocationCallbacks(),
                              &device.frameData[i].renderSemaphore) !=
            VK_SUCCESS)
        {
            return false;
        }
    }

    return true;
}

static void DestroyFrameData(Device& device)
{
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        vkDestroySemaphore(device.logicalDevice,
                           device.frameData[i].renderSemaphore,
                           GetVulkanAllocationCallbacks());
        vkDestroySemaphore(device.logicalDevice,
                           device.frameData[i].swapchainSemaphore,
                           GetVulkanAllocationCallbacks());
        vkDestroyFence(device.logicalDevice, device.frameData[i].renderFence,
                       GetVulkanAllocationCallbacks());

        vkDestroyCommandPool(device.logicalDevice,
                             device.frameData[i].commandPool,
                             GetVulkanAllocationCallbacks());
    }
}

static bool CreateTransferData(Device& device)
{
    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = device.graphicsComputeQueueFamilyIndex;

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = 0;

    if (vkCreateCommandPool(device.logicalDevice, &createInfo,
                            GetVulkanAllocationCallbacks(),
                            &device.transferData.commandPool) != VK_SUCCESS)
    {
        return false;
    }

    if (!CreateCommandBuffers(device, device.transferData.commandPool,
                              &device.transferData.commandBuffer, 1))
    {
        return false;
    }

    if (vkCreateFence(device.logicalDevice, &fenceCreateInfo,
                      GetVulkanAllocationCallbacks(),
                      &device.transferData.transferFence) != VK_SUCCESS)
    {
        return false;
    }

    return true;
}

static void DestroyTransferData(Device& device)
{
    vkDestroyFence(device.logicalDevice, device.transferData.transferFence,
                   GetVulkanAllocationCallbacks());

    vkDestroyCommandPool(device.logicalDevice, device.transferData.commandPool,
                         GetVulkanAllocationCallbacks());
}

static bool CreateCommandPool(Device& device)
{
    if (!CreateFrameData(device))
    {
        return false;
    }
    if (!CreateTransferData(device))
    {
        return false;
    }
    return true;
}

static void DestroyCommandPool(Device& device)
{
    DestroyFrameData(device);
    DestroyTransferData(device);
}

/////////////////////////////////////////////////////////////////////////////
// Vma Allocator
/////////////////////////////////////////////////////////////////////////////

static bool CreateVmaAllocator(Context& context, Device& device)
{
    VmaVulkanFunctions vulkanFunctions{};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo createInfo{};
    createInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    createInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    createInfo.physicalDevice = device.physicalDevice;
    createInfo.device = device.logicalDevice;
    createInfo.instance = context.instance;
    createInfo.pVulkanFunctions = &vulkanFunctions;
    createInfo.pAllocationCallbacks = GetVulkanAllocationCallbacks();

    VkResult res = vmaCreateAllocator(&createInfo, &(device.allocator));
    if (res != VK_SUCCESS)
    {
        return false;
    }
    FLY_DEBUG_LOG("Vma allocator created for device %s", device.name);

    return true;
}

static void DestroyVmaAllocator(Device& device)
{
    vmaDestroyAllocator(device.allocator);
    FLY_DEBUG_LOG("Vma allocator destroyed for device %s", device.name);
}

/////////////////////////////////////////////////////////////////////////////
// Device
/////////////////////////////////////////////////////////////////////////////

bool CreateLogicalDevice(const char** extensions, u32 extensionCount,
                         VkPhysicalDeviceFeatures2& deviceFeatures2,
                         Context& context, Device& device)
{
    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    f32 queuePriority = 1.0f;
    VkDeviceQueueCreateInfo* queueCreateInfos = FLY_PUSH_ARENA(
        arena, VkDeviceQueueCreateInfo,
        1 + static_cast<u32>(static_cast<bool>(context.windowPtr)));
    queueCreateInfos[0] = {};
    queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfos[0].queueFamilyIndex =
        device.graphicsComputeQueueFamilyIndex;
    queueCreateInfos[0].queueCount = 1;
    queueCreateInfos[0].pQueuePriorities = &queuePriority;

    if (context.windowPtr)
    {
        queueCreateInfos[1] = {};
        queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[1].queueFamilyIndex = device.presentQueueFamilyIndex;
        queueCreateInfos[1].queueCount = 1;
        queueCreateInfos[1].pQueuePriorities = &queuePriority;
    }

    u32 totalExtensionCount = extensionCount;
    const char** totalExtensions = extensions;
#if defined(FLY_PLATFORM_OS_MAC_OSX)
    totalExtensionCount += 1;
    totalExtensions = FLY_PUSH_ARENA(arena, const char*, totalExtensionCount);
    for (u32 i = 0; i < extensionCount; i++)
    {
        FLY_LOG("Requested device extensions %s", extensions[i]);
        totalExtensions[i] = extensions[i];
    }
    FLY_LOG("Adding VK_KHR_portability_subset to list of device extensions");
    totalExtensions[extensionCount] = "VK_KHR_portability_subset";
#endif

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.enabledExtensionCount = totalExtensionCount;
    createInfo.ppEnabledExtensionNames = totalExtensions;
    createInfo.pQueueCreateInfos = queueCreateInfos;
    if (!context.windowPtr || device.graphicsComputeQueueFamilyIndex ==
                                  device.presentQueueFamilyIndex)
    {
        createInfo.queueCreateInfoCount = 1;
    }
    else
    {
        createInfo.queueCreateInfoCount = 2;
    }
    createInfo.pNext = &deviceFeatures2;

    VkResult res =
        vkCreateDevice(device.physicalDevice, &createInfo,
                       GetVulkanAllocationCallbacks(), &(device.logicalDevice));
    if (res != VK_SUCCESS)
    {
        ArenaPopToMarker(arena, marker);
        return false;
    }
    FLY_DEBUG_LOG("Vulkan logical device created for %s", device.name);

    vkGetDeviceQueue(device.logicalDevice,
                     device.graphicsComputeQueueFamilyIndex, 0,
                     &(device.graphicsComputeQueue));

    if (context.windowPtr)
    {
        vkGetDeviceQueue(device.logicalDevice, device.presentQueueFamilyIndex,
                         0, &(device.presentQueue));
    }
    device.context = &context;

    if (!CreateVmaAllocator(context, device))
    {
        FLY_ERROR("Failed to create vma allocator %s", device.name);
        return false;
    }

    if (!CreateCommandPool(device))
    {
        FLY_ERROR("Failed to create command pool %s", device.name);
        return false;
    }

    if (!CreateDescriptorPool(device))
    {
        FLY_ERROR("Failed to create descriptor pool %s", device.name);
        return false;
    }

    if (context.windowPtr)
    {
        if (!CreateSwapchain(device))
        {
            FLY_ERROR("Failed to create swapchain %s", device.name);
            return false;
        }
    }

    if (!CreatePipelineLayout(device, device.pipelineLayout))
    {
        return false;
    }

    ArenaPopToMarker(arena, marker);
    return true;
}

void DestroyLogicalDevice(Device& device)
{
    if (device.swapchain != VK_NULL_HANDLE)
    {
        DestroySwapchain(device);
    }
    vkDestroyPipelineLayout(device.logicalDevice, device.pipelineLayout,
                            GetVulkanAllocationCallbacks());
    DestroyDescriptorPool(device);
    DestroyCommandPool(device);
    DestroyVmaAllocator(device);

    vkDestroyDevice(device.logicalDevice, GetVulkanAllocationCallbacks());
    FLY_DEBUG_LOG("Vulkan logical device %s destroyed", device.name);
}

CommandBuffer& RenderFrameCommandBuffer(Device& device)
{
    return device.frameData[device.frameIndex].commandBuffer;
}

const SwapchainTexture& RenderFrameSwapchainTexture(const Device& device)
{
    return device.swapchainTextures[device.swapchainTextureIndex];
}

bool BeginRenderFrame(Device& device)
{
    vkWaitForFences(device.logicalDevice, 1,
                    &device.frameData[device.frameIndex].renderFence, VK_TRUE,
                    UINT64_MAX);

    if (device.swapchain != VK_NULL_HANDLE)
    {
        VkResult res = VK_ERROR_UNKNOWN;
        while (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
        {
            res = vkAcquireNextImageKHR(
                device.logicalDevice, device.swapchain, UINT64_MAX,
                device.frameData[device.frameIndex].swapchainSemaphore,
                VK_NULL_HANDLE, &device.swapchainTextureIndex);
        }
    }
    else
    {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 0;
        submitInfo.pCommandBuffers = nullptr;
        submitInfo.waitSemaphoreCount = 0;
        submitInfo.pWaitSemaphores = nullptr;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores =
            &device.frameData[device.frameIndex].swapchainSemaphore;
        vkQueueSubmit(device.graphicsComputeQueue, 1, &submitInfo,
                      VK_NULL_HANDLE);
    }

    vkResetFences(device.logicalDevice, 1,
                  &device.frameData[device.frameIndex].renderFence);

    CommandBuffer& cmd = RenderFrameCommandBuffer(device);
    ResetCommandBuffer(cmd, false);
    BeginCommandBuffer(cmd, true);

    if (device.swapchain != VK_NULL_HANDLE)
    {
        // Swapchain image transition to writeable
        RecordTransitionImageLayout(
            cmd, device.swapchainTextures[device.swapchainTextureIndex].handle,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

    return true;
}

bool EndRenderFrame(Device& device)
{
    CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    if (device.swapchain != VK_NULL_HANDLE)
    {
        // Image transition to presentable
        RecordTransitionImageLayout(
            cmd, device.swapchainTextures[device.swapchainTextureIndex].handle,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }
    EndCommandBuffer(cmd);

    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    SubmitCommandBuffer(
        cmd, device.graphicsComputeQueue,
        &device.frameData[device.frameIndex].swapchainSemaphore, 1,
        &waitStageMask, &device.frameData[device.frameIndex].renderSemaphore, 1,
        device.frameData[device.frameIndex].renderFence, nullptr);

    if (device.swapchain != VK_NULL_HANDLE)
    {
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores =
            &device.frameData[device.frameIndex].renderSemaphore;
        presentInfo.pSwapchains = &device.swapchain;
        presentInfo.swapchainCount = 1;
        presentInfo.pImageIndices = &device.swapchainTextureIndex;
        presentInfo.pResults = nullptr;

        VkResult res = vkQueuePresentKHR(device.presentQueue, &presentInfo);

        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR ||
            device.isFramebufferResized)
        {
            if (!RecreateSwapchain(device))
            {
                return false;
            }
        }
        else if (res != VK_SUCCESS)
        {
            return false;
        }
    }
    else
    {
        VkPipelineStageFlags waitMask = VK_PIPELINE_STAGE_NONE;
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 0;
        submitInfo.pCommandBuffers = nullptr;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores =
            &device.frameData[device.frameIndex].renderSemaphore;
        submitInfo.signalSemaphoreCount = 0;
        submitInfo.pSignalSemaphores = nullptr;
        submitInfo.pWaitDstStageMask = &waitMask;
        vkQueueSubmit(device.graphicsComputeQueue, 1, &submitInfo,
                      VK_NULL_HANDLE);
    }

    device.frameIndex = (device.frameIndex + 1) % FLY_FRAME_IN_FLIGHT_COUNT;
    return true;
}

// bool EndRenderFrame(Device& device, VkSemaphore* extraWaitSemaphores,
//                     VkPipelineStageFlags* extraWaitStageMasks,
//                     u32 extraWaitSemaphoreCount,
//                     VkSemaphore* extraSignalSemaphores,
//                     u32 extraSignalSemaphoreCount, void* pNext)
// {
//     CommandBuffer& cmd = RenderFrameCommandBuffer(device);

//     if (device.swapchain != VK_NULL_HANDLE)
//     {
//         // Image transition to presentable
//         RecordTransitionImageLayout(
//             cmd,
//             device.swapchainTextures[device.swapchainTextureIndex].handle,
//             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
//             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
//     }
//     EndCommandBuffer(cmd);

//     Arena& arena = GetScratchArena();
//     ArenaMarker marker = ArenaGetMarker(arena);

//     u32 waitSemaphoreCount = 1 + extraWaitSemaphoreCount;
//     u32 waitStageMaskCount = 1 + extraWaitSemaphoreCount;
//     u32 signalSemaphoreCount = 1 + extraSignalSemaphoreCount;
//     VkSemaphore* waitSemaphores =
//         FLY_PUSH_ARENA(arena, VkSemaphore, waitSemaphoreCount);
//     VkPipelineStageFlags* waitStageMasks =
//         FLY_PUSH_ARENA(arena, VkPipelineStageFlags, waitStageMaskCount);
//     VkSemaphore* signalSemaphores =
//         FLY_PUSH_ARENA(arena, VkSemaphore, signalSemaphoreCount);
//     waitSemaphores[0] =
//     device.frameData[device.frameIndex].swapchainSemaphore; waitStageMasks[0]
//     = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; signalSemaphores[0] =
//     device.frameData[device.frameIndex].renderSemaphore;

//     if (waitSemaphoreCount > 1)
//     {
//         memcpy(waitSemaphores + 1, extraWaitSemaphores,
//                sizeof(VkSemaphore) * extraWaitSemaphoreCount);
//         memcpy(waitStageMasks + 1, extraWaitStageMasks,
//                sizeof(VkPipelineStageFlagBits) * extraWaitSemaphoreCount);
//     }
//     if (signalSemaphoreCount > 1)
//     {
//         memcpy(signalSemaphores + 1, extraSignalSemaphores,
//                sizeof(VkSemaphore) * extraSignalSemaphoreCount);
//     }

//     SubmitCommandBuffer(cmd, device.graphicsComputeQueue, waitSemaphores,
//                         waitSemaphoreCount, waitStageMasks, signalSemaphores,
//                         signalSemaphoreCount,
//                         device.frameData[device.frameIndex].renderFence,
//                         pNext);
//     ArenaPopToMarker(arena, marker);

//     if (device.swapchain != VK_NULL_HANDLE)
//     {
//         VkPresentInfoKHR presentInfo{};
//         presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
//         presentInfo.waitSemaphoreCount = 1;
//         presentInfo.pWaitSemaphores =
//             &device.frameData[device.frameIndex].renderSemaphore;
//         presentInfo.pSwapchains = &device.swapchain;
//         presentInfo.swapchainCount = 1;
//         presentInfo.pImageIndices = &device.swapchainTextureIndex;
//         presentInfo.pResults = nullptr;

//         VkResult res = vkQueuePresentKHR(device.presentQueue, &presentInfo);

//         if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
//         {
//             if (!RecreateSwapchain(device))
//             {
//                 return false;
//             }
//         }
//         else if (res != VK_SUCCESS)
//         {
//             return false;
//         }
//     }
//     device.frameIndex = (device.frameIndex + 1) % FLY_FRAME_IN_FLIGHT_COUNT;
//     return true;
// }

CommandBuffer& TransferCommandBuffer(Device& device)
{
    return device.transferData.commandBuffer;
}

void BeginTransfer(Device& device)
{
    CommandBuffer& cmd = device.transferData.commandBuffer;
    vkResetFences(device.logicalDevice, 1, &device.transferData.transferFence);

    ResetCommandBuffer(cmd, false);
    BeginCommandBuffer(cmd, true);
}

void EndTransfer(Device& device)
{
    CommandBuffer& cmd = device.transferData.commandBuffer;
    EndCommandBuffer(cmd);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    SubmitCommandBuffer(cmd, device.graphicsComputeQueue, nullptr, 0,
                        &waitStage, nullptr, 0,
                        device.transferData.transferFence);
    vkWaitForFences(device.logicalDevice, 1, &device.transferData.transferFence,
                    VK_TRUE, UINT64_MAX);
}

void WaitDeviceIdle(Device& device) { vkDeviceWaitIdle(device.logicalDevice); }

} // namespace RHI
} // namespace Fly
