#include <string.h>

#include "core/assert.h"
#include "core/clock.h"
#include "core/log.h"
#include "core/platform.h"
#include "core/thread_context.h"

#include "math/mat.h"

#include "rhi/acceleration_structure.h"
#include "rhi/buffer.h"
#include "rhi/command_buffer.h"
#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"

#include "utils/utils.h"

#include "demos/common/simple_camera_fps.h"

#include <GLFW/glfw3.h>

using namespace Fly;

struct CameraData
{
    Math::Mat4 projection;
    Math::Mat4 view;
};

static RHI::AccelerationStructure sBlas;
static RHI::AccelerationStructure sTlas;
static RHI::Buffer sInstanceBuffer;
static RHI::Buffer sGeometryBuffer;
static RHI::Buffer sCameraBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::GraphicsPipeline sGraphicsPipeline;

static RHI::QueryPool sCompactionQueryPool;

static Fly::SimpleCameraFPS sCamera(90.0f, 1280.0f / 720.0f, 0.01f, 1000.0f,
                                    Math::Vec3(0.0f, 0.0f, 5.0f));

static void OnKeyboardPressed(GLFWwindow* window, int key, int scancode,
                              int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }
}

static void ErrorCallbackGLFW(int error, const char* description)
{
    FLY_ERROR("GLFW - error: %s", description);
}

static bool CreatePipelines(RHI::Device& device)
{
    RHI::ShaderProgram shaderProgram{};
    if (!Fly::LoadShaderFromSpv(
            device, FLY_STRING8_LITERAL("ray_generation.rgen.spv"),
            shaderProgram[RHI::Shader::Type::RayGeneration]))
    {
        return false;
    }

    if (!Fly::LoadShaderFromSpv(
            device, FLY_STRING8_LITERAL("ray_closest_hit.rchit.spv"),
            shaderProgram[RHI::Shader::Type::RayClosestHit]))
    {
        return false;
    }

    if (!Fly::LoadShaderFromSpv(device,
                                FLY_STRING8_LITERAL("ray_miss.rmiss.spv"),
                                shaderProgram[RHI::Shader::Type::RayMiss]))
    {
        return false;
    }

    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::RayGeneration]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::RayClosestHit]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::RayMiss]);
    return true;
}

static void DestroyPipelines(RHI::Device& device) {}

static bool CreateResources(RHI::Device& device)
{
    VkAabbPositionsKHR aabb;
    aabb.minX = 0.0f;
    aabb.minY = 0.0f;
    aabb.minZ = 0.0f;
    aabb.maxX = 2.0f;
    aabb.maxY = 2.0f;
    aabb.maxZ = 2.0f;

    if (!RHI::CreateBuffer(
            device, false,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
            &aabb, sizeof(VkAabbPositionsKHR), sGeometryBuffer))
    {
        return false;
    }

    VkAccelerationStructureGeometryAabbsDataKHR aabbs{};
    aabbs.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
    aabbs.data = {sGeometryBuffer.address};
    aabbs.stride = sizeof(VkAabbPositionsKHR);

    VkAccelerationStructureGeometryKHR blasGeometry{};
    blasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    blasGeometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
    blasGeometry.geometry.aabbs = aabbs;
    blasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    VkAccelerationStructureBuildRangeInfoKHR blasRangeInfo{};
    blasRangeInfo.primitiveCount = 1;
    blasRangeInfo.primitiveOffset = 0;

    if (!RHI::CreateQueryPool(
            device, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, 1,
            0, sCompactionQueryPool))
    {
        return false;
    }

    if (!RHI::CreateAccelerationStructure(
            device, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
            &blasGeometry, &blasRangeInfo, 1, sBlas))
    {
        return false;
    }

    f32 instanceMatrix[3][4] = {{1.0f, 0.0f, 0.0f, 0.0f},
                                {0.0f, 1.0f, 0.0f, 0.0f},
                                {0.0f, 0.0f, 0.0f, 1.0f}};
    VkTransformMatrixKHR instanceTransform;
    memcpy(instanceTransform.matrix, instanceMatrix, sizeof(instanceMatrix));

    VkAccelerationStructureInstanceKHR instance;
    instance.transform = instanceTransform;
    instance.instanceCustomIndex = 0;
    instance.mask = 0xFF;
    instance.instanceShaderBindingTableRecordOffset = 0;
    instance.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    instance.accelerationStructureReference = {sBlas.address};

    if (!RHI::CreateBuffer(
            device, false,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
            &instance, sizeof(instance), sInstanceBuffer))
    {
        return false;
    }

    VkAccelerationStructureGeometryInstancesDataKHR instances{};
    instances.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instances.arrayOfPointers = false;
    instances.data = {sInstanceBuffer.address};

    VkAccelerationStructureGeometryKHR tlasGeometry{};
    tlasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlasGeometry.geometry.instances = instances;
    tlasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    VkAccelerationStructureBuildRangeInfoKHR tlasRangeInfo{};
    tlasRangeInfo.primitiveCount = 1;
    tlasRangeInfo.primitiveOffset = 0;

    if (!RHI::CreateAccelerationStructure(
            device, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            &tlasGeometry, &tlasRangeInfo, 1, sTlas))
    {
        return false;
    }

    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateBuffer(device, true, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                               nullptr, sizeof(CameraData), sCameraBuffers[i]))
        {
            FLY_ERROR("Failed to create camera buffer %u", i);
            return false;
        }
    }

    return true;
}

static void DestroyResources(RHI::Device& device)
{
    RHI::DestroyQueryPool(device, sCompactionQueryPool);
    RHI::DestroyAccelerationStructure(device, sTlas);
    RHI::DestroyAccelerationStructure(device, sBlas);
    RHI::DestroyBuffer(device, sInstanceBuffer);
    RHI::DestroyBuffer(device, sGeometryBuffer);
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        RHI::DestroyBuffer(device, sCameraBuffers[i]);
    }
}

static void DrawScene(RHI::Device& device)
{
    VkRenderingAttachmentInfo colorAttachment =
        RHI::ColorAttachmentInfo(RenderFrameSwapchainTexture(device).imageView);
    VkRenderingInfo renderingInfo = RHI::RenderingInfo(
        {{0, 0}, {device.swapchainWidth, device.swapchainHeight}},
        &colorAttachment, 1, nullptr, nullptr, 1, 0);
}

static bool IsPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice,
                                     const RHI::PhysicalDeviceInfo& info)
{

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures{};
    rayTracingFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accStructureFeatures{};
    accStructureFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accStructureFeatures.pNext = &rayTracingFeatures;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &accStructureFeatures;

    vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

    if (!rayTracingFeatures.rayTracingPipeline)
    {
        return false;
    }

    if (!accStructureFeatures.accelerationStructure)
    {
        return false;
    }

    return info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

int main(int argc, char* argv[])
{
    InitThreadContext();
    if (!InitLogger())
    {
        return -1;
    }

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
    GLFWwindow* window =
        glfwCreateWindow(1280, 720, "Ray tracing", nullptr, nullptr);

    if (!window)
    {
        FLY_ERROR("Failed to create glfw window");
        glfwTerminate();
        return -1;
    }
    glfwSetKeyCallback(window, OnKeyboardPressed);

    // Device extensions
    const char* requiredDeviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_MAINTENANCE_1_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME};

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures{};
    rayTracingFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rayTracingFeatures.rayTracingPipeline = true;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accStructureFeatures{};
    accStructureFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accStructureFeatures.accelerationStructure = true;
    accStructureFeatures.descriptorBindingAccelerationStructureUpdateAfterBind =
        true;
    accStructureFeatures.pNext = &rayTracingFeatures;

    RHI::ContextSettings settings{};
    settings.isPhysicalDeviceSuitableCallback = IsPhysicalDeviceSuitable;
    settings.instanceExtensions =
        glfwGetRequiredInstanceExtensions(&settings.instanceExtensionCount);
    settings.deviceExtensions = requiredDeviceExtensions;
    settings.deviceExtensionCount = STACK_ARRAY_COUNT(requiredDeviceExtensions);
    settings.windowPtr = window;
    settings.vulkan12Features.bufferDeviceAddress = true;
    settings.vulkan13Features.pNext = &accStructureFeatures;

    RHI::Context context;
    if (!RHI::CreateContext(settings, context))
    {
        FLY_ERROR("Failed to create context");
        return -1;
    }

    RHI::Device& device = context.devices[0];

    if (!CreateResources(device))
    {
        FLY_ERROR("Failed to create resources");
        return -1;
    }

    if (!CreatePipelines(device))
    {
        FLY_ERROR("Failed to create pipeline");
        return -1;
    }

    u64 previousFrameTime = 0;
    u64 loopStartTime = Fly::ClockNow();
    u64 currentFrameTime = loopStartTime;

    while (!glfwWindowShouldClose(window))
    {
        previousFrameTime = currentFrameTime;
        currentFrameTime = Fly::ClockNow();
        f64 deltaTime = Fly::ToSeconds(currentFrameTime - previousFrameTime);

        glfwPollEvents();

        {
            sCamera.Update(window, deltaTime);
            CameraData cameraData = {sCamera.GetProjection(),
                                     sCamera.GetView()};
            RHI::Buffer& cameraBuffer = sCameraBuffers[device.frameIndex];
            RHI::CopyDataToBuffer(device, &cameraData, sizeof(CameraData), 0,
                                  cameraBuffer);
        }

        RHI::BeginRenderFrame(device);
        RHI::EndRenderFrame(device);
    }

    RHI::WaitDeviceIdle(device);
    DestroyPipelines(device);
    DestroyResources(device);
    RHI::DestroyContext(context);

    glfwDestroyWindow(window);
    glfwTerminate();
    FLY_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
