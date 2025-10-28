#include "core/assert.h"
#include "core/clock.h"
#include "core/log.h"
#include "core/platform.h"
#include "core/thread_context.h"

#include "rhi/allocation_callbacks.h"
#include "rhi/command_buffer.h"
#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"

#include "assets/geometry/mesh.h"

#include "utils/utils.h"

#include "demos/common/simple_camera_fps.h"

#include <GLFW/glfw3.h>

using namespace Fly;

struct CameraData
{
    Math::Mat4 projection = {};
    Math::Mat4 view = {};
};

struct MeshInstance
{
    Math::Vec3 position;
    f32 pad;
};

struct MeshData
{
    Math::Vec3 center;
    f32 radius;
    u32 lodCount;
};

static RHI::GraphicsPipeline sGraphicsPipeline;
static RHI::ComputePipeline sCullPipeline;
static RHI::ComputePipeline sFirstInstancePrefixSumPipeline;
static RHI::Buffer sCameraBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sMeshInstances;
static RHI::Buffer sMeshDataBuffer;
static RHI::Buffer sDrawCommands;
static RHI::Buffer sDrawCountBuffer;
static RHI::Texture sDepthTexture;
static i32 sInstanceRowCount = 10;
static Mesh sMesh;

static VkQueryPool sTimestampQueryPool;
static f32 sTimestampPeriod;

static void OnKeyboardPressed(GLFWwindow* window, int key, int scancode,
                              int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }
}

static void OnFramebufferResize(RHI::Device& device, u32 width, u32 height,
                                void*)
{
    RHI::DestroyTexture(device, sDepthTexture);
    RHI::CreateTexture2D(device, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                         nullptr, width, height, VK_FORMAT_D32_SFLOAT_S8_UINT,
                         RHI::Sampler::FilterMode::Nearest,
                         RHI::Sampler::WrapMode::Repeat, 1, sDepthTexture);
}

static void ErrorCallbackGLFW(int error, const char* description)
{
    FLY_ERROR("GLFW - error: %s", description);
}

static Fly::SimpleCameraFPS sCamera(90.0f, 1280.0f / 720.0f, 0.01f, 1000.0f,
                                    Math::Vec3(0.0f, 0.0f, -10.0f));

static bool CreatePipelines(RHI::Device& device)
{
    RHI::ComputePipeline* computePipelines[] = {
        &sCullPipeline,
        &sFirstInstancePrefixSumPipeline,
    };

    String8 computeShaderPaths[] = {
        FLY_STRING8_LITERAL("cull.comp.spv"),
        FLY_STRING8_LITERAL("prefix_sum.comp.spv"),
    };

    for (u32 i = 0; i < STACK_ARRAY_COUNT(computeShaderPaths); i++)
    {
        RHI::Shader shader;
        if (!LoadShaderFromSpv(device, computeShaderPaths[i], shader))
        {
            return false;
        }
        if (!RHI::CreateComputePipeline(device, shader, *computePipelines[i]))
        {
            return false;
        }
        RHI::DestroyShader(device, shader);
    }

    RHI::ShaderProgram shaderProgram{};
    if (!Fly::LoadShaderFromSpv(device, FLY_STRING8_LITERAL("lit.vert.spv"),
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }

    if (!Fly::LoadShaderFromSpv(device, FLY_STRING8_LITERAL("lit.frag.spv"),
                                shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
    }

    RHI::GraphicsPipelineFixedStateStage fixedState{};
    fixedState.pipelineRendering.colorAttachments[0] =
        device.surfaceFormat.format;
    fixedState.pipelineRendering.depthAttachmentFormat =
        VK_FORMAT_D32_SFLOAT_S8_UINT;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.colorBlendState.attachmentCount = 1;
    fixedState.depthStencilState.depthTestEnable = true;
    fixedState.rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

    if (!RHI::CreateGraphicsPipeline(device, fixedState, shaderProgram,
                                     sGraphicsPipeline))
    {
        return false;
    }
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Vertex]);
    RHI::DestroyShader(device, shaderProgram[RHI::Shader::Type::Fragment]);

    return true;
}

static void DestroyPipelines(RHI::Device& device)
{
    RHI::DestroyGraphicsPipeline(device, sGraphicsPipeline);
    RHI::DestroyComputePipeline(device, sCullPipeline);
    RHI::DestroyComputePipeline(device, sFirstInstancePrefixSumPipeline);
}

static bool CreateResources(RHI::Device& device)
{
    if (!ImportMesh(FLY_STRING8_LITERAL("dragon.fmesh"), device, sMesh))
    {
        FLY_ERROR("Failed to import mesh");
        return false;
    }
    FLY_LOG("Mesh vertex size is %u, lod count is %u", sMesh.vertexSize,
            sMesh.lodCount);
    FLY_LOG("Bounding sphere center is %f %f %f and radius %f",
            sMesh.sphereCenter.x, sMesh.sphereCenter.y, sMesh.sphereCenter.z,
            sMesh.sphereRadius);
    for (u32 i = 0; i < sMesh.lodCount; i++)
    {
        FLY_LOG("LOD %u: triangle count %u", i, sMesh.lods[i].indexCount / 3);
    }

    if (!RHI::CreateBuffer(device, false,
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           nullptr, sizeof(MeshData), sMeshDataBuffer))
    {
        FLY_LOG("Failed to create mesh data buffer");
        return false;
    }

    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    VkDrawIndexedIndirectCommand* drawCommands =
        FLY_PUSH_ARENA(arena, VkDrawIndexedIndirectCommand, FLY_MAX_LOD_COUNT);
    for (u32 i = 0; i < FLY_MAX_LOD_COUNT; i++)
    {
        drawCommands[i] = {};
        if (i < sMesh.lodCount)
        {
            drawCommands[i].indexCount = sMesh.lods[i].indexCount;
            drawCommands[i].instanceCount = 0;
            drawCommands[i].firstIndex = sMesh.lods[i].firstIndex;
            drawCommands[i].vertexOffset = 0;
            drawCommands[i].firstInstance = 0;
        }
    }

    if (!RHI::CreateBuffer(device, true,
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           drawCommands,
                           sizeof(VkDrawIndexedIndirectCommand) *
                               FLY_MAX_LOD_COUNT,
                           sDrawCommands))
    {
        FLY_ERROR("Failed to create draw commands buffer");
        return false;
    }

    if (!RHI::CreateBuffer(device, false,
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           nullptr, sizeof(u32), sDrawCountBuffer))
    {
        FLY_ERROR("Failed to create draw count buffer");
        return false;
    }

    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateBuffer(device, true, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                               nullptr, sizeof(CameraData), sCameraBuffers[i]))
        {
            FLY_ERROR("Failed to create uniform buffer %u", i);
            return false;
        }
    }

    MeshInstance* meshInstances = FLY_PUSH_ARENA(
        arena, MeshInstance, sInstanceRowCount * sInstanceRowCount);
    for (i32 i = 0; i < sInstanceRowCount; i++)
    {
        for (i32 j = 0; j < sInstanceRowCount; j++)
        {
            meshInstances[i * sInstanceRowCount + j].position =
                Math::Vec3(i * 5.0f, 0.0f, -j * 5.0f);
        }
    }

    if (!RHI::CreateBuffer(device, false,
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           meshInstances,
                           sizeof(MeshInstance) * sInstanceRowCount *
                               sInstanceRowCount,
                           sMeshInstances))
    {
        FLY_ERROR("Failed to create instance buffer");
        return false;
    }

    ArenaPopToMarker(arena, marker);

    if (!RHI::CreateTexture2D(
            device, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, nullptr,
            device.swapchainWidth, device.swapchainHeight,
            VK_FORMAT_D32_SFLOAT_S8_UINT, RHI::Sampler::FilterMode::Nearest,
            RHI::Sampler::WrapMode::Repeat, 1, sDepthTexture))
    {
        FLY_ERROR("Failed to create depth texture");
        return false;
    }

    VkQueryPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    createInfo.queryCount = 2;

    if (vkCreateQueryPool(device.logicalDevice, &createInfo,
                          RHI::GetVulkanAllocationCallbacks(),
                          &sTimestampQueryPool) != VK_SUCCESS)
    {
        return false;
    }

    return true;
}

static void DestroyResources(RHI::Device& device)
{
    vkDestroyQueryPool(device.logicalDevice, sTimestampQueryPool,
                       RHI::GetVulkanAllocationCallbacks());

    DestroyMesh(device, sMesh);
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        RHI::DestroyBuffer(device, sCameraBuffers[i]);
    }
    RHI::DestroyBuffer(device, sMeshInstances);
    RHI::DestroyBuffer(device, sDrawCommands);
    RHI::DestroyBuffer(device, sDrawCountBuffer);
    RHI::DestroyBuffer(device, sMeshDataBuffer);
    RHI::DestroyTexture(device, sDepthTexture);
}

static void RecordCull(RHI::CommandBuffer& cmd,
                       const RHI::RecordBufferInput* bufferInput,
                       const RHI::RecordTextureInput* textureInput,
                       void* pUserData)
{
    RHI::BindComputePipeline(cmd, sCullPipeline);

    RHI::Buffer& cameraBuffer = *(bufferInput->buffers[0]);
    RHI::Buffer& instanceBuffer = *(bufferInput->buffers[1]);
    RHI::Buffer& meshDataBuffer = *(bufferInput->buffers[2]);
    RHI::Buffer& drawCommandBuffer = *(bufferInput->buffers[3]);
    RHI::Buffer& drawCountBuffer = *(bufferInput->buffers[4]);

    u32 pushConstants[] = {
        cameraBuffer.bindlessHandle,
        instanceBuffer.bindlessHandle,
        meshDataBuffer.bindlessHandle,
        drawCommandBuffer.bindlessHandle,
        drawCountBuffer.bindlessHandle,
        static_cast<u32>(sInstanceRowCount * sInstanceRowCount)};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd,
                  static_cast<u32>(Math::Ceil(
                      (sInstanceRowCount * sInstanceRowCount) / 256.0f)),
                  1, 1);
}

static void Cull(RHI::Device& device)
{
    RHI::RecordBufferInput bufferInput;
    RHI::Buffer* buffers[5];
    VkAccessFlagBits2 bufferAccesses[5];

    buffers[0] = &sCameraBuffers[device.frameIndex];
    bufferAccesses[0] = VK_ACCESS_2_SHADER_READ_BIT;
    buffers[1] = &sMeshInstances;
    bufferAccesses[1] = VK_ACCESS_2_SHADER_READ_BIT;
    buffers[2] = &sMeshDataBuffer;
    bufferAccesses[2] = VK_ACCESS_2_SHADER_READ_BIT;
    buffers[3] = &sDrawCommands;
    bufferAccesses[3] =
        VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    buffers[4] = &sDrawCountBuffer;
    bufferAccesses[4] =
        VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;

    bufferInput.buffers = buffers;
    bufferInput.bufferAccesses = bufferAccesses;
    bufferInput.bufferCount = 5;

    RHI::ExecuteCompute(RenderFrameCommandBuffer(device), RecordCull,
                        &bufferInput);
}

static void RecordFirstInstancePrefixSum(
    RHI::CommandBuffer& cmd, const RHI::RecordBufferInput* bufferInput,
    const RHI::RecordTextureInput* textureInput, void* pUserData)
{
    RHI::BindComputePipeline(cmd, sFirstInstancePrefixSumPipeline);

    RHI::Buffer& drawCommandBuffer = *(bufferInput->buffers[0]);

    u32 pushConstants[] = {drawCommandBuffer.bindlessHandle, FLY_MAX_LOD_COUNT};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));
    RHI::Dispatch(cmd, static_cast<u32>(Math::Ceil(1 / 64.0f)), 1, 1);
}

static void FirstInstancePrefixSum(RHI::Device& device)
{
    RHI::RecordBufferInput bufferInput;
    RHI::Buffer* buffers[1];
    VkAccessFlagBits2 bufferAccesses[1];

    buffers[0] = &sDrawCommands;
    bufferAccesses[0] = VK_ACCESS_2_SHADER_READ_BIT;

    bufferInput.buffers = buffers;
    bufferInput.bufferAccesses = bufferAccesses;
    bufferInput.bufferCount = 1;

    RHI::ExecuteCompute(RenderFrameCommandBuffer(device),
                        RecordFirstInstancePrefixSum, &bufferInput);
}

static void RecordDrawMesh(RHI::CommandBuffer& cmd,
                           const RHI::RecordBufferInput* bufferInput,
                           const RHI::RecordTextureInput* textureInput,
                           void* pUserData)
{
    RHI::WriteTimestamp(cmd, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                        sTimestampQueryPool, 0);

    RHI::SetViewport(cmd, 0, 0, static_cast<f32>(cmd.device->swapchainWidth),
                     static_cast<f32>(cmd.device->swapchainHeight), 0.0f, 1.0f);
    RHI::SetScissor(cmd, 0, 0, cmd.device->swapchainWidth,
                    cmd.device->swapchainHeight);

    RHI::BindGraphicsPipeline(cmd, sGraphicsPipeline);

    RHI::Buffer& cameraBuffer = *(bufferInput->buffers[0]);
    RHI::Buffer& meshInstanceBuffer = *(bufferInput->buffers[1]);

    RHI::BindIndexBuffer(cmd, sMesh.indexBuffer, VK_INDEX_TYPE_UINT32);

    u32 pushConstants[] = {cameraBuffer.bindlessHandle,
                           sMesh.vertexBuffer.bindlessHandle,
                           meshInstanceBuffer.bindlessHandle};
    RHI::PushConstants(cmd, pushConstants, sizeof(pushConstants));

    RHI::DrawIndexedIndirectCount(cmd, sDrawCommands, 0, sDrawCountBuffer, 0,
                                  FLY_MAX_LOD_COUNT,
                                  sizeof(VkDrawIndexedIndirectCommand));

    RHI::WriteTimestamp(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        sTimestampQueryPool, 1);
}

static void DrawMesh(RHI::Device& device)
{
    RHI::RecordBufferInput bufferInput;
    RHI::Buffer* buffers[4];
    VkAccessFlagBits2 bufferAccesses[4];

    buffers[0] = &sCameraBuffers[device.frameIndex];
    bufferAccesses[0] = VK_ACCESS_2_SHADER_READ_BIT;
    buffers[1] = &sMeshInstances;
    bufferAccesses[1] = VK_ACCESS_2_SHADER_READ_BIT;
    buffers[2] = &sDrawCommands;
    bufferAccesses[2] = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
    buffers[3] = &sDrawCountBuffer;
    bufferAccesses[3] = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;

    bufferInput.buffers = buffers;
    bufferInput.bufferAccesses = bufferAccesses;
    bufferInput.bufferCount = 4;

    RHI::RecordTextureInput textureInput;
    RHI::Texture* pDepthTexture = &sDepthTexture;
    RHI::ImageLayoutAccess imageLayoutAccess;
    imageLayoutAccess.imageLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    imageLayoutAccess.accessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    textureInput.textures = &pDepthTexture;
    textureInput.imageLayoutsAccesses = &imageLayoutAccess;
    textureInput.textureCount = 1;

    VkRenderingAttachmentInfo colorAttachment =
        RHI::ColorAttachmentInfo(RenderFrameSwapchainTexture(device).imageView);
    VkRenderingAttachmentInfo depthAttachment =
        RHI::DepthAttachmentInfo(sDepthTexture.imageView);
    VkRenderingInfo renderingInfo = RHI::RenderingInfo(
        {{0, 0}, {device.swapchainWidth, device.swapchainHeight}},
        &colorAttachment, 1, &depthAttachment);

    RHI::ExecuteGraphics(RenderFrameCommandBuffer(device), renderingInfo,
                         RecordDrawMesh, &bufferInput, &textureInput);
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
    GLFWwindow* window = glfwCreateWindow(1280, 720, "LODs", nullptr, nullptr);

    if (!window)
    {
        FLY_ERROR("Failed to create glfw window");
        glfwTerminate();
        return -1;
    }
    glfwSetKeyCallback(window, OnKeyboardPressed);

    // Device extensions
    const char* requiredDeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    RHI::ContextSettings settings{};
    settings.instanceExtensions =
        glfwGetRequiredInstanceExtensions(&settings.instanceExtensionCount);
    settings.deviceExtensions = requiredDeviceExtensions;
    settings.deviceExtensionCount = STACK_ARRAY_COUNT(requiredDeviceExtensions);
    settings.windowPtr = window;
    settings.vulkan12Features.shaderFloat16 = true;
    settings.vulkan12Features.drawIndirectCount = true;
    settings.vulkan11Features.storageBuffer16BitAccess = true;

    RHI::Context context;
    if (!RHI::CreateContext(settings, context))
    {
        FLY_ERROR("Failed to create context");
        return -1;
    }

    RHI::Device& device = context.devices[0];
    device.swapchainRecreatedCallback.func = OnFramebufferResize;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device.physicalDevice, &props);
    if (props.limits.timestampPeriod == 0)
    {
        FLY_ERROR("Device does not support timestamp queries");
        return -1;
    }
    sTimestampPeriod = props.limits.timestampPeriod;

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

        sCamera.Update(window, deltaTime);

        CameraData cameraData = {sCamera.GetProjection(), sCamera.GetView()};

        RHI::Buffer& cameraBuffer = sCameraBuffers[device.frameIndex];
        RHI::CopyDataToBuffer(device, &cameraData, sizeof(CameraData), 0,
                              cameraBuffer);

        RHI::BeginRenderFrame(device);
        RHI::ResetQueryPool(RenderFrameCommandBuffer(device),
                            sTimestampQueryPool, 0, 2);
        Cull(device);
        FirstInstancePrefixSum(device);
        DrawMesh(device);
        RHI::EndRenderFrame(device);

        // VkDrawIndexedIndirectCommand* commands =
        //     static_cast<VkDrawIndexedIndirectCommand*>(
        //         RHI::BufferMappedPtr(sDrawCommands));

        // for (u32 i = 0; i < FLY_MAX_LOD_COUNT; i++)
        // {
        //     FLY_LOG("%u: i - %u fi - %u", i, commands[i].instanceCount,
        //             commands[i].firstInstance);
        // }

        u64 timestamps[2];
        vkGetQueryPoolResults(device.logicalDevice, sTimestampQueryPool, 0, 2,
                              sizeof(timestamps), timestamps, sizeof(uint64_t),
                              VK_QUERY_RESULT_64_BIT |
                                  VK_QUERY_RESULT_WAIT_BIT);
        f64 drawTime = Fly::ToMilliseconds(static_cast<u64>(
            (timestamps[1] - timestamps[0]) * sTimestampPeriod));

        FLY_LOG("Dragon draw: %f ms", drawTime);
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
