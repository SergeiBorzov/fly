#ifndef HLS_CONTEXT_H
#define HLS_CONTEXT_H

#include "core/types.h"
#include <volk.h>

#define HLS_PHYSICAL_DEVICE_MAX_COUNT 8

struct Arena;

struct HlsDevice
{
    VkPhysicalDevice physicalDevice;
    i32 graphicsComputeFamilyIndex = -1;
};

struct HlsContext
{
    VkInstance instance = VK_NULL_HANDLE;
    HlsDevice devices[HLS_PHYSICAL_DEVICE_MAX_COUNT];
    u32 deviceCount = 0;
};

bool HlsCreateContext(Arena* arena, const char** instanceLayers,
                      u32 instanceLayerCount, const char** instanceExtensions,
                      u32 instanceExtensionCount, const char** deviceExtensions,
                      u32 deviceExtensionCount, HlsContext* outContext);
void HlsDestroyContext(HlsContext* context);

#endif /* HLS_CONTEXT_H */
