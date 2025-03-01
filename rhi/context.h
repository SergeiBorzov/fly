#ifndef HLS_CONTEXT_H
#define HLS_CONTEXT_H

#include "core/types.h"
#include <volk.h>

#define HLS_PHYSICAL_DEVICE_MAX_COUNT 4

struct Arena;

struct HlsContext
{
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevices[HLS_PHYSICAL_DEVICE_MAX_COUNT];
};

bool HlsCreateContext(Arena* arena, const char** instanceLayers,
                      u32 instanceLayerCount, const char** instanceExtensions,
                      u32 instanceExtensionCount, const char** deviceExtensions,
                      u32 deviceExtensionCount, HlsContext* outContext);
void HlsDestroyContext(HlsContext* context);

#endif /* HLS_CONTEXT_H */
