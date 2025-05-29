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

NativeWindowHandles GetNativeWindowPtr(GLFWwindow* glfwWindow)
{
    NativeWindowHandles handles{};
#ifdef HLS_PLATFORM_OS_WINDOWS
    handles.windowPtr = static_cast<void*>(glfwGetWin32Window(glfwWindow));
    handles.displayPtr = nullptr;
#elif defined HLS_PLATFORM_OS_LINUX
    handles.windowHandle = glfwGetX11Window(glfwWindow);
    handles.displayPtr = glfwGetX11Display();
#endif
    return handles;
}

} // namespace Hls
