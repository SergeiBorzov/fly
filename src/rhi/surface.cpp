#include "src/core/platform.h"

#ifdef HLS_PLATFORM_OS_WINDOWS
#include <windows.h>
#else
#error "Surface API is not implemented for this platform"
#endif

#include <volk.h>

#include "allocation_callbacks.h"
#include "context.h"
#include "surface.h"

namespace Hls
{
namespace RHI
{

bool CreateSurface(Context& context)
{
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
#ifdef HLS_PLATFORM_OS_WINDOWS
    createInfo.hinstance = GetModuleHandle(nullptr);
    createInfo.hwnd = reinterpret_cast<HWND>(context.windowPtr);
#endif
    return vkCreateWin32SurfaceKHR(context.instance, &createInfo,
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
#ifdef HLS_PLATFORM_OS_WINDOWS
    RECT rect;
    if (GetClientRect(reinterpret_cast<HWND>(context.windowPtr), &rect))
    {
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;
    }
    else
    {
        width = height = 0;
    }
    return;
#endif
}

void PollWindowEvents(Context& context)
{
#ifdef HLS_PLATFORM_OS_WINDOWS
    MSG msg;
    while (PeekMessage(&msg, reinterpret_cast<HWND>(context.windowPtr), 0, 0,
                       PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return;
#endif
}

} // namespace RHI
} // namespace Hls
