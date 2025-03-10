#include "core/arena.h"
#include "core/assert.h"
#include "core/filesystem.h"
#include "core/log.h"

#include "rhi/context.h"
#include <GLFW/glfw3.h>

static void SetVulkanLayerPathEnvVariable(Arena& arena)
{
    ArenaMarker marker = ArenaGetMarker(arena);
    const char* binaryPath = GetBinaryDirectoryPath(arena);
    HLS_ASSERT(binaryPath);
    HLS_ENSURE(SetEnv("VK_LAYER_PATH", binaryPath));
    ArenaPopToMarker(arena, marker);
}

static void ErrorCallbackGLFW(i32 error, const char* description)
{
    HLS_ERROR("GLFW - error: %s", description);
}

static void RecordCommands(Hls::Context& context, Hls::Device& device)
{
    Hls::CommandBuffer& cmd = RenderFrameCommandBuffer(context, device);
    VkImage image = RenderFrameSwapchainImage(context, device);
    RecordClearColor(cmd, image, 1.0f, 0.0f, 0.0f, 1.0f);
}

int main(int argc, char* argv[])
{
    if (!InitLogger())
    {
        return -1;
    }

    Arena arena = ArenaCreate(HLS_SIZE_GB(1), HLS_SIZE_MB(16));
    SetVulkanLayerPathEnvVariable(arena);

    if (volkInitialize() != VK_SUCCESS)
    {
        HLS_ERROR("Failed to load volk");
        return -1;
    }
    glfwInitVulkanLoader(vkGetInstanceProcAddr);

    if (!glfwInit())
    {
        HLS_ERROR("Failed to init glfw");
        return -1;
    }
    glfwSetErrorCallback(ErrorCallbackGLFW);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(640, 480, "Window", nullptr, nullptr);
    if (!window)
    {
        HLS_ERROR("Failed to create glfw window");
        glfwTerminate();
        return -1;
    }

    // Device extensions
    const char* requiredDeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    Hls::ContextSettings settings{};
    settings.instanceExtensions =
        glfwGetRequiredInstanceExtensions(&settings.instanceExtensionCount);
    settings.deviceExtensions = requiredDeviceExtensions;
    settings.deviceExtensionCount = STACK_ARRAY_SIZE(requiredDeviceExtensions);
    settings.windowPtr = window;

    Hls::Context context;
    if (!Hls::CreateContext(arena, settings, context))
    {
        HLS_ERROR("Failed to create context");
        return -1;
    }
    HLS_ASSERT(ArenaGetMarker(arena).value == 0);

    Hls::Device& device = context.devices[0];
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        BeginRenderFrame(context, device);
        RecordCommands(context, device);
        EndRenderFrame(context, device);
    }

    Hls::DestroyContext(context);
    glfwDestroyWindow(window);
    glfwTerminate();
    ArenaDestroy(arena);
    HLS_LOG("Shutdown successful");
    ShutdownLogger();
    return 0;
}
