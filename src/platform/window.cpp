#include "core/platform.h"

#include "window.h"

#ifdef HLS_PLATFORM_OS_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined HLS_PLATFORM_OS_LINUX
// Assuming x11 is used, to be improved
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>

namespace Hls
{

void* GetNativeWindowPtr(GLFWwindow* glfwWindow)
{
#ifdef HLS_PLATFORM_OS_WINDOWS
    return static_cast<void*>(glfwGetWin32Window(glfwWindow));
#elif defined HLS_PLATFORM_OS_LINUX
    return static_cast<void*>(glfwGetX11Window(glfwWindow));
#else
    return nullptr;
#endif
}

} // namespace Hls
