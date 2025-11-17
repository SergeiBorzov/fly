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

struct SphereData
{
    Math::Vec3 center;
    f32 radius;
    Math::Vec3 albedo;
    f32 reflectionCoeff;
};

static u32 sInstanceCount = 500;
static u32 sCurrentSample = 0;
static u32 sSampleCount = 16384;
static RHI::AccelerationStructure sBlas;
static RHI::AccelerationStructure sTlas;
static RHI::Buffer sCameraBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sSphereBuffer;
static RHI::Buffer sInstanceBuffer;
static RHI::Buffer sGeometryBuffer;
static RHI::Texture sOutputTexture;
static RHI::Texture sSkyboxTexture;
static RHI::RayTracingPipeline sRayTracingPipeline;
static RHI::GraphicsPipeline sGraphicsPipeline;

static Fly::SimpleCameraFPS sCamera(90.0f, 1280.0f / 720.0f, 0.01f, 1000.0f,
                                    Math::Vec3(0.0f, 10.0f, 25.0f));

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
    {
        RHI::Shader shaders[4];
        String8 shaderPaths[4] = {
            FLY_STRING8_LITERAL("ray_generation.rgen.spv"),
            FLY_STRING8_LITERAL("ray_miss.rmiss.spv"),
            FLY_STRING8_LITERAL("ray_intersection.rint.spv"),
            FLY_STRING8_LITERAL("ray_closest_hit.rchit.spv")};

        for (u32 i = 0; i < 4; i++)
        {
            if (!Fly::LoadShaderFromSpv(device, shaderPaths[i], shaders[i]))
            {
                return false;
            }
        }

        RHI::RayTracingGroup groups[3] = {
            RHI::GeneralRayTracingGroup(0), RHI::GeneralRayTracingGroup(1),
            RHI::ProceduralHitRayTracingGroup(2, 3)};

        if (!RHI::CreateRayTracingPipeline(device, 3, shaders, 4, groups, 3,
                                           sRayTracingPipeline))
        {
            FLY_ERROR("Failed to create ray tracing pipeline");
            return false;
        }

        for (u32 i = 0; i < 4; i++)
        {
            RHI::DestroyShader(device, shaders[i]);
        }
    }

    {
        RHI::ShaderProgram shaderProgram;
        if (!Fly::LoadShaderFromSpv(device,
                                    FLY_STRING8_LITERAL("screen_quad.vert.spv"),
                                    shaderProgram[RHI::Shader::Type::Vertex]))
        {
            return false;
        }
        if (!Fly::LoadShaderFromSpv(device,
                                    FLY_STRING8_LITERAL("screen_quad.frag.spv"),
                                    shaderProgram[RHI::Shader::Type::Fragment]))
        {
            return false;
        }

        RHI::GraphicsPipelineFixedStateStage fixedState{};
        fixedState.pipelineRendering.colorAttachments[0] =
            device.surfaceFormat.format;
        fixedState.pipelineRendering.colorAttachmentCount = 1;
        fixedState.colorBlendState.attachmentCount = 1;
        fixedState.rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

        if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                         sGraphicsPipeline))
        {
            return false;
        }

        RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
        RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);
    }
    return true;
}

static void DestroyPipelines(RHI::Device& device)
{
    RHI::DestroyRayTracingPipeline(device, sRayTracingPipeline);
    RHI::DestroyGraphicsPipeline(device, sGraphicsPipeline);
}

static bool CreateResources(RHI::Device& device)
{
    VkAabbPositionsKHR aabb;
    aabb.minX = -1.0f;
    aabb.minY = -1.0f;
    aabb.minZ = -1.0f;
    aabb.maxX = 1.0f;
    aabb.maxY = 1.0f;
    aabb.maxZ = 1.0f;

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

    if (!RHI::CreateAccelerationStructure(
            device, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
            &blasGeometry, &blasRangeInfo, 1, sBlas))
    {
        return false;
    }

    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);
    SphereData* sphereData = FLY_PUSH_ARENA(arena, SphereData, sInstanceCount);
    VkAccelerationStructureInstanceKHR* instances = FLY_PUSH_ARENA(
        arena, VkAccelerationStructureInstanceKHR, sInstanceCount);

    sphereData[0].center = Math::Vec3(0.0f, -348500.0f, 0.0f);
    sphereData[0].radius = 348500.0f;
    sphereData[0].albedo = Math::Vec3(0.8f);
    sphereData[0].reflectionCoeff = 0.0f;

    for (u32 i = 1; i < sInstanceCount; i++)
    {
        sphereData[i].center = Math::Vec3(Math::RandomF32(-100.0f, 100.0f),
                                          Math::RandomF32(0.0f, 50.0f),
                                          Math::RandomF32(-100.0f, 100.0f));
        sphereData[i].radius = Math::RandomF32(0.25f, 8.0f);
        sphereData[i].albedo =
            Math::Vec3(Math::RandomF32(0.0f, 1.0f), Math::RandomF32(0.0f, 1.0f),
                       Math::RandomF32(0.0f, 1.0f));
        sphereData[i].reflectionCoeff = Math::RandomF32(0.0f, 1.0f);
    }

    for (u32 i = 0; i < sInstanceCount; i++)
    {

        f32 instanceMatrix[3][4] = {
            {sphereData[i].radius, 0.0f, 0.0f, sphereData[i].center.x},
            {0.0f, sphereData[i].radius, 0.0f, sphereData[i].center.y},
            {0.0f, 0.0f, sphereData[i].radius, sphereData[i].center.z}};
        memcpy(instances[i].transform.matrix, instanceMatrix,
               sizeof(instanceMatrix));
        instances[i].instanceCustomIndex = i;
        instances[i].mask = 0xFF;
        instances[i].instanceShaderBindingTableRecordOffset = 0;
        instances[i].flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        instances[i].accelerationStructureReference = {sBlas.address};
    }

    if (!RHI::CreateBuffer(device, false,
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           sphereData, sizeof(SphereData) * sInstanceCount,
                           sSphereBuffer))
    {
        return false;
    }

    if (!RHI::CreateBuffer(
            device, false,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            instances,
            sizeof(VkAccelerationStructureInstanceKHR) * sInstanceCount,
            sInstanceBuffer))
    {
        return false;
    }

    ArenaPopToMarker(arena, marker);

    VkAccelerationStructureGeometryInstancesDataKHR instancesGeometry{};
    instancesGeometry.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instancesGeometry.arrayOfPointers = false;
    instancesGeometry.data = {sInstanceBuffer.address};

    VkAccelerationStructureGeometryKHR tlasGeometry{};
    tlasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlasGeometry.geometry.instances = instancesGeometry;
    tlasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    VkAccelerationStructureBuildRangeInfoKHR tlasRangeInfo{};
    tlasRangeInfo.primitiveCount = sInstanceCount;
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

    if (!RHI::CreateTexture2D(
            device,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            nullptr, device.swapchainWidth, device.swapchainHeight,
            VK_FORMAT_R16G16B16A16_SFLOAT, RHI::Sampler::FilterMode::Nearest,
            RHI::Sampler::WrapMode::Clamp, 1, sOutputTexture))
    {
        return false;
    }

    if (!LoadCubemap(device,
                     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     FLY_STRING8_LITERAL("day.exr"),
                     VK_FORMAT_R16G16B16A16_SFLOAT,
                     RHI::Sampler::FilterMode::Bilinear, 1, sSkyboxTexture))
    {
        FLY_ERROR("Failed to load cubemap");
        return false;
    }

    return true;
}

static void DestroyResources(RHI::Device& device)
{
    RHI::DestroyAccelerationStructure(device, sTlas);
    RHI::DestroyAccelerationStructure(device, sBlas);
    RHI::DestroyBuffer(device, sSphereBuffer);
    RHI::DestroyBuffer(device, sInstanceBuffer);
    RHI::DestroyBuffer(device, sGeometryBuffer);
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        RHI::DestroyBuffer(device, sCameraBuffers[i]);
    }
    RHI::DestroyTexture(device, sSkyboxTexture);
    RHI::DestroyTexture(device, sOutputTexture);
}

static void RecordDrawOutputTexture(RHI::CommandBuffer& cmd,
                                    const RHI::RecordBufferInput* bufferInput,
                                    u32 bufferInputCount,
                                    const RHI::RecordTextureInput* textureInput,
                                    u32 textureInputCount, void* pUserData)
{
    RHI::SetViewport(cmd, 0, 0, static_cast<f32>(cmd.device->swapchainWidth),
                     static_cast<f32>(cmd.device->swapchainHeight), 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, cmd.device->swapchainWidth,
                    cmd.device->swapchainHeight);
    RHI::BindGraphicsPipeline(cmd, sGraphicsPipeline);

    RHI::Texture& outputTexture = *(textureInput[0].pTexture);

    u32 pushConstants[] = {outputTexture.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Draw(cmd, 6, 1, 0, 0);
}

static void RecordClearOutput(RHI::CommandBuffer& cmd,
                              const RHI::RecordBufferInput* bufferInput,
                              u32 bufferInputCount,
                              const RHI::RecordTextureInput* textureInput,
                              u32 textureInputCount, void* pUserData)
{
    RHI::Texture& outputTexture = *(textureInput[0].pTexture);
    RHI::ClearColor(cmd, outputTexture, 0.0f, 0.0f, 0.0f, 1.0f);
}

static void RecordRayTraceScene(RHI::CommandBuffer& cmd,
                                const RHI::RecordBufferInput* bufferInput,
                                u32 bufferInputCount,
                                const RHI::RecordTextureInput* textureInput,
                                u32 textureInputCount, void* pUserData)
{
    RHI::BindRayTracingPipeline(cmd, sRayTracingPipeline);

    RHI::AccelerationStructure& tlas =
        *(static_cast<RHI::AccelerationStructure*>(pUserData));

    RHI::Buffer& cameraBuffer = *(bufferInput[0].pBuffer);
    RHI::Buffer& sphereBuffer = *(bufferInput[1].pBuffer);
    RHI::Texture& skyboxTexture = *(textureInput[0].pTexture);
    RHI::Texture& outputTexture = *(textureInput[1].pTexture);

    u32 pushConstants[] = {cameraBuffer.bindlessHandle,
                           sphereBuffer.bindlessHandle,
                           tlas.bindlessHandle,
                           skyboxTexture.bindlessHandle,
                           outputTexture.bindlessStorageHandle,
                           sRayTracingPipeline.sbtStride,
                           sCurrentSample,
                           sSampleCount};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));

    RHI::TraceRays(
        cmd, &sRayTracingPipeline.rayGenRegion, &sRayTracingPipeline.missRegion,
        &sRayTracingPipeline.hitRegion, &sRayTracingPipeline.callRegion,
        cmd.device->swapchainWidth, cmd.device->swapchainHeight, 1);
}

static void DrawScene(RHI::Device& device, bool cameraMoved)
{
    if (cameraMoved)
    {
        sCurrentSample = 0;
        RHI::RecordTextureInput textureInput[] = {
            {&sOutputTexture, VK_ACCESS_2_TRANSFER_WRITE_BIT,
             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL}};
        RHI::ExecuteTransfer(RenderFrameCommandBuffer(device),
                             RecordClearOutput, nullptr, 0, textureInput, 1);
    }

    if (sCurrentSample < sSampleCount)
    {
        RHI::RecordBufferInput bufferInput[] = {
            {&sCameraBuffers[device.frameIndex], VK_ACCESS_2_SHADER_READ_BIT},
            {&sSphereBuffer, VK_ACCESS_2_SHADER_READ_BIT}};
        RHI::RecordTextureInput textureInput[] = {
            {&sSkyboxTexture, VK_ACCESS_2_SHADER_READ_BIT,
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {&sOutputTexture,
             VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
             VK_IMAGE_LAYOUT_GENERAL}};

        RHI::ExecuteRayTracing(
            RenderFrameCommandBuffer(device), RecordRayTraceScene, bufferInput,
            STACK_ARRAY_COUNT(bufferInput), textureInput, 1, &sTlas);
        sCurrentSample++;
    }

    {
        VkRenderingAttachmentInfo colorAttachment = RHI::ColorAttachmentInfo(
            RenderFrameSwapchainTexture(device).imageView);
        VkRenderingInfo renderingInfo = RHI::RenderingInfo(
            {{0, 0}, {device.swapchainWidth, device.swapchainHeight}},
            &colorAttachment, 1, nullptr, nullptr, 1, 0);

        RHI::RecordTextureInput textureInput = {
            &sOutputTexture, VK_ACCESS_2_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        RHI::ExecuteGraphics(RenderFrameCommandBuffer(device), renderingInfo,
                             RecordDrawOutputTexture, nullptr, 0, &textureInput,
                             1);
    }
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

        bool cameraMoved = sCamera.Update(window, deltaTime);
        CameraData cameraData = {sCamera.GetProjection(), sCamera.GetView()};
        RHI::Buffer& cameraBuffer = sCameraBuffers[device.frameIndex];
        RHI::CopyDataToBuffer(device, &cameraData, sizeof(CameraData), 0,
                              cameraBuffer);

        RHI::BeginRenderFrame(device);
        DrawScene(device, cameraMoved);
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
