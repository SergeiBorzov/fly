#include "surface.h"
#include "allocation_callbacks.h"
#include "context.h"
#include <GLFW/glfw3.h>

namespace Hls
{
namespace RHI
{

bool CreateSurface(Context& context)
{
    return glfwCreateWindowSurface(context.instance, context.windowPtr,
                                   GetVulkanAllocationCallbacks(),
                                   &context.surface) == VK_SUCCESS;
}

void DestroySurface(Context& context)
{
    vkDestroySurfaceKHR(context.instance, context.surface,
                        GetVulkanAllocationCallbacks());
}

void GetWindowSize(Context& context, i32& width, i32& height)
{
    glfwGetFramebufferSize(context.windowPtr, &width, &height);
}

void PollWindowEvents(Context& context) { glfwWaitEvents(); }

} // namespace RHI
} // namespace Hls
