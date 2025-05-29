#include <GLFW/glfw3.h>

#include "core/types.h"

namespace Hls
{

struct NativeWindowHandles
{
    union
    {
        void* windowPtr;
        u64 windowHandle;
    };
    void* displayPtr;
};

NativeWindowHandles GetNativeWindowHandles(GLFWwindow* glfwWindow);

} // namespace Hls
