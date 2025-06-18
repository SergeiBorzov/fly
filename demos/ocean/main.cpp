#include "core/clock.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "rhi/allocation_callbacks.h"
#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"

#include "demos/common/simple_camera_fps.h"

#include "cascades.h"
#include "ocean_renderer.h"
#include "skybox_renderer.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>

using namespace Fly;

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

static Fly::SimpleCameraFPS
    sCamera(90.0f, static_cast<f32>(WINDOW_WIDTH) / WINDOW_HEIGHT, 0.01f,
            2000.0f, Fly::Math::Vec3(0.0f, 10.0f, -10.0f));
static JonswapCascadesRenderer sCascadesRenderer;
static SkyBoxRenderer sSkyBoxRenderer;
static OceanRenderer sOceanRenderer;

static bool IsPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice,
                                     const RHI::PhysicalDeviceInfo& info)
{

    VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT
        unusedAttachmentsFeature{};
    unusedAttachmentsFeature.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT;
    unusedAttachmentsFeature.pNext = nullptr;

    VkPhysicalDeviceMultiviewFeatures multiviewFeatures{};
    multiviewFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
    multiviewFeatures.pNext = &unusedAttachmentsFeature;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &multiviewFeatures;

    vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

    if (!multiviewFeatures.multiview ||
        !unusedAttachmentsFeature.dynamicRenderingUnusedAttachments)
    {
        return false;
    }

    return info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

static void ErrorCallbackGLFW(i32 error, const char* description)
{
    FLY_ERROR("GLFW - error: %s", description);
}

static VkDescriptorPool sImGuiDescriptorPool;
static bool CreateImGuiContext(RHI::Context& context, RHI::Device& device,
                               GLFWwindow* window)
{
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE},
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 0;
    for (VkDescriptorPoolSize& poolSize : poolSizes)
        poolInfo.maxSets += poolSize.descriptorCount;
    poolInfo.poolSizeCount = static_cast<u32>(STACK_ARRAY_COUNT(poolSizes));
    poolInfo.pPoolSizes = poolSizes;
    if (vkCreateDescriptorPool(device.logicalDevice, &poolInfo,
                               RHI::GetVulkanAllocationCallbacks(),
                               &sImGuiDescriptorPool) != VK_SUCCESS)
    {
        return false;
    }

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = context.instance;
    initInfo.PhysicalDevice = device.physicalDevice;
    initInfo.Device = device.logicalDevice;
    initInfo.Queue = device.graphicsComputeQueue;
    initInfo.DescriptorPool = sImGuiDescriptorPool;
    initInfo.MinImageCount = device.swapchainTextureCount;
    initInfo.ImageCount = device.swapchainTextureCount;
    initInfo.UseDynamicRendering = true;

    initInfo.PipelineRenderingCreateInfo = {};
    initInfo.PipelineRenderingCreateInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineRenderingCreateInfo.depthAttachmentFormat =
        VK_FORMAT_D32_SFLOAT_S8_UINT;
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats =
        &device.surfaceFormat.format;

    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.Allocator = RHI::GetVulkanAllocationCallbacks();

    if (!ImGui_ImplVulkan_Init(&initInfo))
    {
        return false;
    }
    if (!ImGui_ImplVulkan_CreateFontsTexture())
    {
        return false;
    }
    return true;
}

static void DestroyImGuiContext(RHI::Device& device)
{
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(device.logicalDevice, sImGuiDescriptorPool,
                            RHI::GetVulkanAllocationCallbacks());
}

// static void ProcessImGuiFrame()
// {
//     ImGui_ImplVulkan_NewFrame();
//     ImGui_ImplGlfw_NewFrame();
//     ImGui::NewFrame();

//     {
//         ImGui::Begin("Ocean parameters");

//         ImGui::SliderFloat("Fetch",
//         &sUniformData.fetchSpeedDirSpread.x, 1.0f,
//                            1000000.0f, "%.1f");
//         ImGui::SliderFloat("Wind Speed", &sUniformData.fetchSpeedDirSpread.y,
//                            0.001f, 30.0f, "%.6f");
//         ImGui::SliderFloat("Theta 0", &sUniformData.fetchSpeedDirSpread.z,
//                            -FLY_MATH_PI, FLY_MATH_PI, "%.2f rad");
//         ImGui::SliderFloat("Spread",
//         &sUniformData.fetchSpeedDirSpread.w, 1.0f,
//                            30.0f, "%.2f");
//         ImGui::SliderFloat("Scale",
//         &sUniformData.domainTimeLambdaScale.w, 1.0f,
//                            50.0f, "%.2f");
//         ImGui::SliderFloat("Displacement multiplier",
//                            &sUniformData.domainTimeLambdaScale.z,
//                            -2.0f, 2.0f,
//                            "%.01f");

//         ImGui::End();
//     }

//     ImGui::Render();
// }

static void ExecuteCommands(RHI::Device& device)
{
    RecordJonswapCascadesRendererCommands(device, sCascadesRenderer);
    RecordSkyBoxRendererCommands(device, sSkyBoxRenderer);

    OceanRendererInputs inputs;
    inputs.skyBox = sSkyBoxRenderer.skyBoxes[device.frameIndex].bindlessHandle;
    for (u32 i = 0; i < DEMO_OCEAN_CASCADE_COUNT; i++)
    {
        inputs.heightMaps[i] = sCascadesRenderer.cascades[i]
                                   .heightMaps[device.frameIndex]
                                   .bindlessHandle;
        inputs.diffDisplacementMaps[i] =
            sCascadesRenderer.cascades[i]
                .diffDisplacementMaps[device.frameIndex]
                .bindlessHandle;
    }
    RecordOceanRendererCommands(device, inputs, sOceanRenderer);
}

static void OnKeyboardPressed(GLFWwindow* window, int key, int scancode,
                              int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }

    if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
    {
        // if (sCurrentGraphicsPipeline == &sGraphicsPipeline)
        // {
        //     sCurrentGraphicsPipeline = &sWireframeGraphicsPipeline;
        // }
        // else
        // {
        //     sCurrentGraphicsPipeline = &sGraphicsPipeline;
        // }
    }
}

int main(int argc, char* argv[])
{
    InitThreadContext();
    if (!InitLogger())
    {
        return -1;
    }

    // Initialize volk, window
    if (volkInitialize() != VK_SUCCESS)
    {
        FLY_ERROR("Failed to load volk");
        return -1;
    }
    glfwInitVulkanLoader(vkGetInstanceProcAddr);
    if (!glfwInit())
    {
        FLY_ERROR("Failed to init glfw");
        return -1;
    }
    glfwSetErrorCallback(ErrorCallbackGLFW);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Ocean",
                                          nullptr, nullptr);
    if (!window)
    {
        FLY_ERROR("Failed to create glfw window");
        glfwTerminate();
        return -1;
    }
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetKeyCallback(window, OnKeyboardPressed);

    // Create graphics context
    const char* requiredDeviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_EXTENSION_NAME};

    VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT
        unusedAttachmentsFeature{};
    unusedAttachmentsFeature.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT;
    unusedAttachmentsFeature.dynamicRenderingUnusedAttachments = VK_TRUE;

    VkPhysicalDeviceMultiviewFeatures multiviewFeatures = {};
    multiviewFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
    multiviewFeatures.multiview = VK_TRUE;
    multiviewFeatures.pNext = &unusedAttachmentsFeature;

    RHI::ContextSettings settings{};
    settings.isPhysicalDeviceSuitableCallback = IsPhysicalDeviceSuitable;
    settings.instanceExtensions =
        glfwGetRequiredInstanceExtensions(&settings.instanceExtensionCount);
    settings.deviceExtensions = requiredDeviceExtensions;
    settings.deviceExtensionCount = STACK_ARRAY_COUNT(requiredDeviceExtensions);
    settings.windowPtr = window;
    settings.deviceFeatures2.features.fillModeNonSolid = true;
    settings.deviceFeatures2.pNext = &multiviewFeatures;

    RHI::Context context;
    if (!RHI::CreateContext(settings, context))
    {
        FLY_ERROR("Failed to create context");
        return -1;
    }
    RHI::Device& device = context.devices[0];
    glfwSetWindowUserPointer(window, &device);

    // if (!CreateImGuiContext(context, device, window))
    // {
    //     FLY_ERROR("Failed to create imgui context");
    //     return -1;
    // }

    u64 previousFrameTime = 0;
    u64 loopStartTime = Fly::ClockNow();
    u64 currentFrameTime = loopStartTime;

    if (!CreateJonswapCascadesRenderer(device, 256, sCascadesRenderer))
    {
        return false;
    }
    sCascadesRenderer.cascades[0].domain = 256.0f;
    sCascadesRenderer.cascades[1].domain = 64.0f;
    sCascadesRenderer.cascades[2].domain = 16.0f;
    sCascadesRenderer.cascades[3].domain = 4.0f;

    if (!CreateSkyBoxRenderer(device, 256, sSkyBoxRenderer))
    {
        return false;
    }

    if (!CreateOceanRenderer(device, sOceanRenderer))
    {
        return false;
    }

    sCamera.speed = 25.0f;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        previousFrameTime = currentFrameTime;
        currentFrameTime = Fly::ClockNow();
        f64 deltaTime = Fly::ToSeconds(currentFrameTime - previousFrameTime);

        // FLY_LOG("FPS: %f", 1.0f / deltaTime);

        // ImGuiIO& io = ImGui::GetIO();
        // bool wantMouse = io.WantCaptureMouse;
        // bool wantKeyboard = io.WantCaptureKeyboard;
        // if (!wantMouse && !wantKeyboard)
        // {
        sCamera.Update(window, deltaTime);
        //}

        // ProcessImGuiFrame();

        sCascadesRenderer.time = currentFrameTime;
        UpdateJonswapCascadesRendererUniforms(device, sCascadesRenderer);
        UpdateOceanRendererUniforms(device, sCamera, WINDOW_WIDTH,
                                    WINDOW_HEIGHT, sOceanRenderer);

        RHI::BeginRenderFrame(device);
        ExecuteCommands(device);
        RHI::EndRenderFrame(device);
    }

    RHI::WaitAllDevicesIdle(context);

    DestroyJonswapCascadesRenderer(device, sCascadesRenderer);
    DestroySkyBoxRenderer(device, sSkyBoxRenderer);
    DestroyOceanRenderer(device, sOceanRenderer);

    // DestroyImGuiContext(device);
    RHI::DestroyContext(context);
    glfwDestroyWindow(window);
    glfwTerminate();

    FLY_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
