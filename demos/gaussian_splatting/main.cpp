#include "core/clock.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "math/quat.h"

#include "rhi/buffer.h"
#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"

#include "demos/common/scene.h"
#include "demos/common/simple_camera_fps.h"

#include <GLFW/glfw3.h>

#include <stdlib.h>

using namespace Hls;

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define RADIX_PASS_COUNT 4
#define RADIX_BIT_COUNT 8
#define RADIX_HISTOGRAM_SIZE (1U << RADIX_BIT_COUNT)
#define COUNT_WORKGROUP_SIZE 512
#define COUNT_TILE_SIZE COUNT_WORKGROUP_SIZE
#define SCAN_WORKGROUP_SIZE (COUNT_WORKGROUP_SIZE / 2)

#pragma pack(push, 1)
struct Splat
{
    Math::Vec3 position;
    Math::Vec3 scale;
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    u8 w;
    u8 x;
    u8 y;
    u8 z;
};
#pragma pack(pop)

struct Vertex
{
    Math::Quat rotation;
    Math::Vec3 position;
    f32 r;
    Math::Vec3 scale;
    f32 g;
    f32 b;
    f32 a;
    f32 pad[2];
};

struct UniformData
{
    Math::Mat4 projection;       // 0
    Math::Mat4 view;             // 64
    Math::Vec4 viewport;         // 128
    Math::Vec4 cameraParameters; // 144
    Math::Vec4 time;             // 160
};

static Hls::SimpleCameraFPS
    sCamera(80.0f, static_cast<f32>(WINDOW_WIDTH) / WINDOW_HEIGHT, 0.01f,
            100.0f, Hls::Math::Vec3(0.0f, 0.0f, -5.0f));

static bool IsPhysicalDeviceSuitable(const RHI::Context& context,
                                     const RHI::PhysicalDeviceInfo& info)
{
    return info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

static void ErrorCallbackGLFW(i32 error, const char* description)
{
    HLS_ERROR("GLFW - error: %s", description);
}

static RHI::ComputePipeline sCullPipeline;
static RHI::ComputePipeline sCopyPipeline;
static RHI::ComputePipeline sCountPipeline;
static RHI::ComputePipeline sScanPipeline;
static RHI::ComputePipeline sSortPipeline;
static RHI::GraphicsPipeline sGraphicsPipeline;
static bool CreatePipelines(RHI::Device& device)
{
    RHI::ComputePipeline* computePipelines[] = {
        &sCullPipeline, &sCopyPipeline, &sCountPipeline,
        &sScanPipeline, &sSortPipeline,
    };
    const char* computeShaderPaths[] = {
        "cull.comp.spv",
        "copy.comp.spv",
        "radix_count_histogram.comp.spv",
        "radix_scan.comp.spv",
        "radix_sort.comp.spv",
    };

    for (u32 i = 0; i < STACK_ARRAY_COUNT(computeShaderPaths); i++)
    {
        RHI::Shader shader;
        if (!Hls::LoadShaderFromSpv(device, computeShaderPaths[i], shader))
        {
            return false;
        }
        if (!RHI::CreateComputePipeline(device, shader, *computePipelines[i]))
        {
            return false;
        }
        RHI::DestroyShader(device, shader);
    }

    RHI::GraphicsPipelineFixedStateStage fixedState{};
    fixedState.pipelineRendering.colorAttachments[0] =
        device.surfaceFormat.format;
    fixedState.pipelineRendering.depthAttachmentFormat =
        device.depthTexture.format;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.colorBlendState.attachmentCount = 1;
    fixedState.colorBlendState.attachments[0].blendEnable = true;
    fixedState.colorBlendState.attachments[0].srcColorBlendFactor =
        VK_BLEND_FACTOR_SRC_ALPHA;
    fixedState.colorBlendState.attachments[0].dstColorBlendFactor =
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    fixedState.colorBlendState.attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    fixedState.colorBlendState.attachments[0].srcAlphaBlendFactor =
        VK_BLEND_FACTOR_ZERO;
    fixedState.colorBlendState.attachments[0].dstAlphaBlendFactor =
        VK_BLEND_FACTOR_ONE;
    fixedState.colorBlendState.attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    fixedState.depthStencilState.depthTestEnable = false;
    fixedState.inputAssemblyState.topology =
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    RHI::ShaderProgram shaderProgram{};
    if (!Hls::LoadShaderFromSpv(device, "splat.vert.spv",
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }
    if (!Hls::LoadShaderFromSpv(device, "splat.frag.spv",
                                shaderProgram[RHI::Shader::Type::Fragment]))
    {
        return false;
    }

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
    RHI::DestroyComputePipeline(device, sScanPipeline);
    RHI::DestroyComputePipeline(device, sCountPipeline);
    RHI::DestroyComputePipeline(device, sSortPipeline);
    RHI::DestroyComputePipeline(device, sCopyPipeline);
    RHI::DestroyComputePipeline(device, sCullPipeline);
}

static u32 sSplatCount;
static RHI::Buffer sSplatBuffer;
static RHI::Buffer sSortedSplatBuffers[HLS_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sTileHistograms[HLS_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sGlobalHistograms[HLS_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sPingPongKeys[2 * HLS_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sUniformBuffers[HLS_FRAME_IN_FLIGHT_COUNT];

static bool CreateDeviceBuffers(RHI::Device& device, const Splat* splats, u32 splatCount)
{
    Arena& scratch = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(scratch);

    Vertex* vertices = HLS_ALLOC(scratch, Vertex, splatCount);
    for (u32 i = 0; i < splatCount; i++)
    {
        f32 x = (splats[i].x - 128.0f) / 128.0f;
        f32 y = (splats[i].y - 128.0f) / 128.0f;
        f32 z = (splats[i].z - 128.0f) / 128.0f;
        f32 w = (splats[i].w - 128.0f) / 128.0f;

        Math::Quat q = Math::Conjugate(Math::Normalize(Math::Quat(x, y, z, w)));
        Math::Quat r1 = Math::AngleAxis(Math::Vec3(0.0f, 0.0f, 1.0f), 180.0f);
        Math::Quat r2 = Math::AngleAxis(Math::Vec3(1.0f, 0.0f, 0.0f), 25.0f);
        vertices[i].rotation = q * r2 * r1;
        vertices[i].position =
            r2 * r1 *
            Math::Vec3(splats[i].position.x, splats[i].position.y,
                       splats[i].position.z);
        // vertices[i].position = Math::Vec3(
        //     splats[i].position.x, splats[i].position.y,
        //     splats[i].position.z);
        vertices[i].scale = splats[i].scale;
        vertices[i].r = splats[i].r / 255.0f;
        vertices[i].g = splats[i].g / 255.0f;
        vertices[i].b = splats[i].b / 255.0f;
        vertices[i].a = splats[i].a / 255.0f;
    }

    if (!RHI::CreateStorageBuffer(device, false, vertices,
                                  sizeof(Vertex) * splatCount, sSplatBuffer))
    {
        ArenaPopToMarker(scratch, marker);
        return false;
    }

    for (u32 i = 0; i < HLS_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateUniformBuffer(device, nullptr, sizeof(UniformData),
                                      sUniformBuffers[i]))
        {
            ArenaPopToMarker(scratch, marker);
            return false;
        }
    }

    u32 tileCount = static_cast<u32>(
        Math::Ceil(static_cast<f32>(splatCount) / COUNT_TILE_SIZE));
    for (u32 i = 0; i < HLS_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateStorageBuffer(device, false, nullptr,
                                      RADIX_HISTOGRAM_SIZE * tileCount *
                                          sizeof(u32),
                                      sTileHistograms[i]))
        {
            ArenaPopToMarker(scratch, marker);
            return false;
        }

        if (!RHI::CreateStorageBuffer(device, false, nullptr,
                                      sizeof(u32) * RADIX_HISTOGRAM_SIZE *
                                          RADIX_PASS_COUNT,
                                      sGlobalHistograms[i]))
        {
            ArenaPopToMarker(scratch, marker);
            return false;
        }

        if (!RHI::CreateStorageBuffer(device, false, vertices,
                                      sizeof(Vertex) * splatCount,
                                      sSortedSplatBuffers[i]))
        {
            ArenaPopToMarker(scratch, marker);
            return false;
        }

        for (u32 j = 0; j < 2; j++)
        {
            if (!RHI::CreateStorageBuffer(device, true, nullptr,
                                          2 * sizeof(u32) * splatCount,
                                          sPingPongKeys[2 * i + j]))
            {
                ArenaPopToMarker(scratch, marker);
                return false;
            }
        }
    }
    ArenaPopToMarker(scratch, marker);

    return true;
}

static void DestroyDeviceBuffers(RHI::Device& device)
{
    for (u32 i = 0; i < HLS_FRAME_IN_FLIGHT_COUNT; i++)
    {
        for (u32 j = 0; j < 2; j++)
        {
            RHI::DestroyBuffer(device, sPingPongKeys[2 * i + j]);
        }
        RHI::DestroyBuffer(device, sTileHistograms[i]);
        RHI::DestroyBuffer(device, sGlobalHistograms[i]);
        RHI::DestroyBuffer(device, sUniformBuffers[i]);
        RHI::DestroyBuffer(device, sSortedSplatBuffers[i]);
    }
    RHI::DestroyBuffer(device, sSplatBuffer);
}

static void CullPass(RHI::Device& device)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    u32 workGroupCount = static_cast<u32>(
        Math::Ceil(static_cast<f32>(sSplatCount) / COUNT_TILE_SIZE));

    vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                      sCullPipeline.handle);
    vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                            sCullPipeline.layout, 0, 1,
                            &device.bindlessDescriptorSet, 0, nullptr);
    u32 pushConstants[] = {sUniformBuffers[device.frameIndex].bindlessHandle,
                           sSplatBuffer.bindlessHandle,
                           sPingPongKeys[2 * device.frameIndex].bindlessHandle,
                           sSplatCount};
    vkCmdPushConstants(cmd.handle, sCullPipeline.layout, VK_SHADER_STAGE_ALL, 0,
                       sizeof(pushConstants), pushConstants);
    vkCmdDispatch(cmd.handle, workGroupCount, 1, 1);

    VkBufferMemoryBarrier cullToSortBarrier = RHI::BufferMemoryBarrier(
        sPingPongKeys[2 * device.frameIndex], VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT);
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1,
                         &cullToSortBarrier, 0, nullptr);
}

static void SortPass(RHI::Device& device)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    u32 workGroupCount = static_cast<u32>(
        Math::Ceil(static_cast<f32>(sSplatCount) / COUNT_TILE_SIZE));

    // Reset global histogram buffer
    vkCmdFillBuffer(cmd.handle, sGlobalHistograms[device.frameIndex].handle, 0,
                    sizeof(u32) * RADIX_HISTOGRAM_SIZE * RADIX_PASS_COUNT, 0);
    VkBufferMemoryBarrier resetToCountBarrier;
    resetToCountBarrier = RHI::BufferMemoryBarrier(
        sGlobalHistograms[device.frameIndex], VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1,
                         &resetToCountBarrier, 0, nullptr);

    for (u32 i = 0; i < RADIX_PASS_COUNT; i++)
    {
        u32 in = (i % 2) + 2 * device.frameIndex;
        u32 out = ((i + 1) % 2) + 2 * device.frameIndex;

        // Calculate count histograms
        vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                          sCountPipeline.handle);
        vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                                sCountPipeline.layout, 0, 1,
                                &device.bindlessDescriptorSet, 0, nullptr);

        u32 pushConstants[] = {
            i, sSplatCount, sPingPongKeys[in].bindlessHandle,
            sTileHistograms[device.frameIndex].bindlessHandle,
            sGlobalHistograms[device.frameIndex].bindlessHandle};
        vkCmdPushConstants(cmd.handle, sCountPipeline.layout,
                           VK_SHADER_STAGE_ALL, 0, sizeof(pushConstants),
                           pushConstants);
        vkCmdDispatch(cmd.handle, workGroupCount, 1, 1);

        VkBufferMemoryBarrier countToScanBarriers[2];
        countToScanBarriers[0] = RHI::BufferMemoryBarrier(
            sTileHistograms[device.frameIndex], VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        countToScanBarriers[1] = RHI::BufferMemoryBarrier(
            sGlobalHistograms[device.frameIndex], VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT);
        vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                             nullptr, STACK_ARRAY_COUNT(countToScanBarriers),
                             countToScanBarriers, 0, nullptr);

        // Exclusive prefix sums - global offsets
        vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                          sScanPipeline.handle);
        vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                                sScanPipeline.layout, 0, 1,
                                &device.bindlessDescriptorSet, 0, nullptr);
        vkCmdPushConstants(cmd.handle, sScanPipeline.layout,
                           VK_SHADER_STAGE_ALL, 0, sizeof(pushConstants),
                           pushConstants);
        vkCmdDispatch(cmd.handle, RADIX_HISTOGRAM_SIZE, 1, 1);

        VkBufferMemoryBarrier scanToSortBarrier = RHI::BufferMemoryBarrier(
            sTileHistograms[device.frameIndex],
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT);
        vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                             nullptr, 1, &scanToSortBarrier, 0, nullptr);

        // Sort
        vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                          sSortPipeline.handle);
        vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                                sSortPipeline.layout, 0, 1,
                                &device.bindlessDescriptorSet, 0, nullptr);
        pushConstants[4] = sPingPongKeys[out].bindlessHandle;
        vkCmdPushConstants(cmd.handle, sSortPipeline.layout,
                           VK_SHADER_STAGE_ALL, 0, sizeof(pushConstants),
                           pushConstants);
        vkCmdDispatch(cmd.handle, workGroupCount, 1, 1);

        if (i == RADIX_PASS_COUNT - 1)
        {
            break;
        }

        VkBufferMemoryBarrier nextPassBarriers[3];
        nextPassBarriers[0] = RHI::BufferMemoryBarrier(
            sPingPongKeys[out], VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT);
        nextPassBarriers[1] = RHI::BufferMemoryBarrier(
            sPingPongKeys[in], VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_SHADER_WRITE_BIT);
        nextPassBarriers[2] = RHI::BufferMemoryBarrier(
            sGlobalHistograms[device.frameIndex], VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_SHADER_WRITE_BIT);
        vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                             nullptr, STACK_ARRAY_COUNT(nextPassBarriers),
                             nextPassBarriers, 0, nullptr);
    }

    VkBufferMemoryBarrier sortToCopyBarrier = RHI::BufferMemoryBarrier(
        sPingPongKeys[2 * device.frameIndex], VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT);
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1,
                         &sortToCopyBarrier, 0, nullptr);
}

static void CopyPass(RHI::Device& device)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    u32 workGroupCount = static_cast<u32>(
        Math::Ceil(static_cast<f32>(sSplatCount) / COUNT_TILE_SIZE));

    vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                      sCopyPipeline.handle);
    vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                            sCopyPipeline.layout, 0, 1,
                            &device.bindlessDescriptorSet, 0, nullptr);
    u32 pushConstants[] = {
        sSplatBuffer.bindlessHandle,
        sPingPongKeys[2 * device.frameIndex].bindlessHandle,
        sSortedSplatBuffers[device.frameIndex].bindlessHandle, sSplatCount};
    vkCmdPushConstants(cmd.handle, sCopyPipeline.layout, VK_SHADER_STAGE_ALL, 0,
                       sizeof(pushConstants), pushConstants);
    vkCmdDispatch(cmd.handle, workGroupCount, 1, 1);

    VkBufferMemoryBarrier copyToGraphics = RHI::BufferMemoryBarrier(
        sSortedSplatBuffers[device.frameIndex], VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT);
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 0, nullptr, 1,
                         &copyToGraphics, 0, nullptr);
    VkBufferMemoryBarrier cullToCopyBarrier = RHI::BufferMemoryBarrier(
        sPingPongKeys[2 * device.frameIndex], VK_ACCESS_SHADER_READ_BIT,
        VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1,
                         &cullToCopyBarrier, 0, nullptr);
}

static void GraphicsPass(RHI::Device& device)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    const RHI::SwapchainTexture& swapchainTexture =
        RenderFrameSwapchainTexture(device);
    VkRect2D renderArea = {{0, 0},
                           {swapchainTexture.width, swapchainTexture.height}};
    VkRenderingAttachmentInfo colorAttachment = RHI::ColorAttachmentInfo(
        swapchainTexture.imageView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = RHI::DepthAttachmentInfo(
        device.depthTexture.imageView,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo =
        RHI::RenderingInfo(renderArea, &colorAttachment, 1, &depthAttachment);

    vkCmdBeginRendering(cmd.handle, &renderInfo);
    vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      sGraphicsPipeline.handle);

    vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            sGraphicsPipeline.layout, 0, 1,
                            &device.bindlessDescriptorSet, 0, nullptr);

    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<f32>(renderArea.extent.width);
    viewport.height = static_cast<f32>(renderArea.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd.handle, 0, 1, &viewport);

    VkRect2D scissor = renderArea;
    vkCmdSetScissor(cmd.handle, 0, 1, &scissor);

    u32 indices[2] = {sUniformBuffers[device.frameIndex].bindlessHandle,
                      sSortedSplatBuffers[device.frameIndex].bindlessHandle};
    vkCmdPushConstants(cmd.handle, sGraphicsPipeline.layout,
                       VK_SHADER_STAGE_ALL, 0, sizeof(u32) * 2, indices);

    vkCmdDraw(cmd.handle, 6, sSplatCount, 0, 0);
    vkCmdEndRendering(cmd.handle);
}

static void ExecuteCommands(RHI::Device& device)
{
    CullPass(device);
    SortPass(device);
    CopyPass(device);
    GraphicsPass(device);
}

static const char* splatScenes[] = {
    "garden.splat", "stump.splat", "bicycle.splat",
    "truck.splat",  "nike.splat",  "plush.splat",
    "room.splat",   "train.splat", "treehill.splat"};

static bool LoadNextScene(RHI::Device& device)
{
    static bool isFirstLoad = true;
    static u32 sceneIndex = 0;
    if (!isFirstLoad)
    {
        for (u32 i = 0; i < HLS_FRAME_IN_FLIGHT_COUNT; ++i)
        {
            vkWaitForFences(device.logicalDevice, 1,
                            &device.frameData[i].renderFence, VK_TRUE,
                            UINT64_MAX);
        }
        sceneIndex = (sceneIndex + 1) % STACK_ARRAY_COUNT(splatScenes);
        DestroyDeviceBuffers(device);
    }
    else
    {
        isFirstLoad = false;
    }

    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);
    String8 data = ReadFileToString(arena, splatScenes[sceneIndex]);
    if (!data)
    {
        HLS_ERROR("Failed to read splat file");
        return false;
    }
    sSplatCount = static_cast<u32>(data.Size() / sizeof(Splat));
    const Splat* splats = reinterpret_cast<const Splat*>(data.Data());
    HLS_ASSERT(sSplatCount);
    HLS_LOG("Splat count is %u", sSplatCount);

    if (!CreateDeviceBuffers(device, splats, sSplatCount))
    {
        return false;
    }
    ArenaPopToMarker(arena, marker);
    return true;
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
        RHI::Device* pDevice =
            static_cast<RHI::Device*>(glfwGetWindowUserPointer(window));
        if (!LoadNextScene(*pDevice))
        {
            exit(-1);
        }
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
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
                                          "Splat viewer", nullptr, nullptr);
    if (!window)
    {
        HLS_ERROR("Failed to create glfw window");
        glfwTerminate();
        return -1;
    }
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetKeyCallback(window, OnKeyboardPressed);

    // Create graphics context
    const char* requiredDeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    RHI::ContextSettings settings{};
    settings.isPhysicalDeviceSuitableCallback = IsPhysicalDeviceSuitable;
    settings.instanceExtensions =
        glfwGetRequiredInstanceExtensions(&settings.instanceExtensionCount);
    settings.deviceExtensions = requiredDeviceExtensions;
    settings.deviceExtensionCount = STACK_ARRAY_COUNT(requiredDeviceExtensions);
    settings.windowPtr = window;

    RHI::Context context;
    if (!RHI::CreateContext(settings, context))
    {
        HLS_ERROR("Failed to create context");
        return -1;
    }
    RHI::Device& device = context.devices[0];
    glfwSetWindowUserPointer(window, &device);

    if (!CreatePipelines(device))
    {
        HLS_ERROR("Failed to create pipelines");
        return -1;
    }

    if (!LoadNextScene(device))
    {
        return -1;
    }

    u64 previousFrameTime = 0;
    u64 loopStartTime = Hls::ClockNow();
    u64 currentFrameTime = loopStartTime;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        previousFrameTime = currentFrameTime;
        currentFrameTime = Hls::ClockNow();
        f32 time =
            static_cast<f32>(Hls::ToSeconds(currentFrameTime - loopStartTime));
        f64 deltaTime = Hls::ToSeconds(currentFrameTime - previousFrameTime);

        sCamera.Update(window, deltaTime);

        UniformData uniformData;
        uniformData.projection = sCamera.GetProjection();
        uniformData.view = sCamera.GetView();
        uniformData.cameraParameters.x =
            Math::Tan(Math::Radians(sCamera.GetHorizontalFov()) * 0.5f);
        uniformData.cameraParameters.y =
            Math::Tan(Math::Radians(sCamera.GetVerticalFov()) * 0.5f);
        uniformData.cameraParameters.z = sCamera.GetNear();
        uniformData.cameraParameters.w = sCamera.GetFar();
        uniformData.viewport = Math::Vec4(
            static_cast<f32>(WINDOW_WIDTH), static_cast<f32>(WINDOW_HEIGHT),
            1.0f / WINDOW_WIDTH, 1.0f / WINDOW_HEIGHT);
        uniformData.time.x = time;
        RHI::CopyDataToBuffer(device, &uniformData, sizeof(UniformData), 0,
                              sUniformBuffers[device.frameIndex]);

        RHI::BeginRenderFrame(device);
        ExecuteCommands(device);
        RHI::EndRenderFrame(device);
    }

    RHI::WaitAllDevicesIdle(context);

    DestroyPipelines(device);
    DestroyDeviceBuffers(device);
    RHI::DestroyContext(context);
    glfwDestroyWindow(window);
    glfwTerminate();

    HLS_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
