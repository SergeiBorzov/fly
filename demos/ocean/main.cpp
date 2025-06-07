#include "core/clock.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"

#include "demos/common/scene.h"
#include "demos/common/simple_camera_fps.h"

#include <GLFW/glfw3.h>

using namespace Fly;

#define WINDOW_WIDTH 512
#define WINDOW_HEIGHT 512

struct UniformData
{
    Math::Mat4 projection;       // 0
    Math::Mat4 view;             // 64
    Math::Vec4 viewport;         // 128
    Math::Vec4 cameraParameters; // 144
    Math::Vec4 time;             // 160
};

static Fly::SimpleCameraFPS
    sCamera(80.0f, static_cast<f32>(WINDOW_WIDTH) / WINDOW_HEIGHT, 0.01f,
            100.0f, Fly::Math::Vec3(0.0f, 0.0f, -5.0f));

static bool IsPhysicalDeviceSuitable(const RHI::Context& context,
                                     const RHI::PhysicalDeviceInfo& info)
{
    return info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

static void ErrorCallbackGLFW(i32 error, const char* description)
{
    FLY_ERROR("GLFW - error: %s", description);
}

static RHI::ComputePipeline sGrayscalePipeline;
static RHI::ComputePipeline sFFTPipeline;
static RHI::ComputePipeline sTransposePipeline;
static RHI::ComputePipeline sCopyPipeline;
static RHI::GraphicsPipeline sGraphicsPipeline;
static bool CreatePipelines(RHI::Device& device)
{
    RHI::ComputePipeline* computePipelines[] = {
        &sGrayscalePipeline, &sFFTPipeline, &sTransposePipeline,
        &sCopyPipeline};

    const char* computeShaderPaths[] = {"grayscale.comp.spv", "fft.comp.spv",
                                        "transpose.comp.spv", "copy.comp.spv"};

    for (u32 i = 0; i < STACK_ARRAY_COUNT(computeShaderPaths); i++)
    {
        RHI::Shader shader;
        if (!Fly::LoadShaderFromSpv(device, computeShaderPaths[i], shader))
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
    fixedState.inputAssemblyState.topology =
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    RHI::ShaderProgram shaderProgram{};
    if (!Fly::LoadShaderFromSpv(device, "screen_quad.vert.spv",
                                shaderProgram[RHI::Shader::Type::Vertex]))
    {
        return false;
    }
    if (!Fly::LoadShaderFromSpv(device, "screen_quad.frag.spv",
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
    RHI::DestroyComputePipeline(device, sCopyPipeline);
    RHI::DestroyComputePipeline(device, sTransposePipeline);
    RHI::DestroyComputePipeline(device, sFFTPipeline);
    RHI::DestroyComputePipeline(device, sGrayscalePipeline);
}

static RHI::Buffer sUniformBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sOceanFrequencyBuffers[2 * FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Texture sHeightMaps[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Texture sTexture;

static bool CreateDeviceObjects(RHI::Device& device)
{
    if (!Fly::LoadTextureFromFile(device, "CesiumLogoFlat.png",
                                  VK_FORMAT_R8G8B8A8_SRGB,
                                  RHI::Sampler::FilterMode::Nearest,
                                  RHI::Sampler::WrapMode::Repeat, sTexture))
    {
        return false;
    }

    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!RHI::CreateUniformBuffer(device, nullptr, sizeof(UniformData),
                                      sUniformBuffers[i]))
        {
            return false;
        }

        for (u32 j = 0; j < 2; j++)
        {
            if (!RHI::CreateStorageBuffer(device, false, nullptr,
                                          sizeof(Math::Vec2) * 256 * 256,
                                          sOceanFrequencyBuffers[i * 2 + j]))
            {
                return false;
            }
        }

        if (!RHI::CreateReadWriteTexture(
                device, nullptr, 256 * 256 * sizeof(u8), 256, 256,
                VK_FORMAT_R8_UNORM, RHI::Sampler::FilterMode::Nearest,
                RHI::Sampler::WrapMode::Repeat, sHeightMaps[i]))
        {
            return false;
        }
    }

    return true;
}

static void DestroyDeviceObjects(RHI::Device& device)
{

    RHI::DestroyTexture(device, sTexture);
    for (u32 i = 0; i < FLY_FRAME_IN_FLIGHT_COUNT; i++)
    {
        for (u32 j = 0; j < 2; j++)
        {
            RHI::DestroyBuffer(device, sOceanFrequencyBuffers[2 * i + j]);
        }
        RHI::DestroyBuffer(device, sUniformBuffers[i]);
        RHI::DestroyTexture(device, sHeightMaps[i]);
    }
}

static void GrayscalePass(RHI::Device& device)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    // RHI::FillBuffer(cmd, sOceanFrequencyBuffers[2 * device.frameIndex], 0);
    // VkBufferMemoryBarrier resetToGrayscaleBarrier = RHI::BufferMemoryBarrier(
    //     sOceanFrequencyBuffers[2 * device.frameIndex],
    //     VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT);
    // vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_TRANSFER_BIT,
    //                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
    //                      1, &resetToGrayscaleBarrier, 0, nullptr);

    RHI::BindComputePipeline(device, cmd, sGrayscalePipeline);
    u32 pushConstants[] = {
        sOceanFrequencyBuffers[2 * device.frameIndex].bindlessHandle,
        sTexture.bindlessHandle,
    };
    RHI::SetPushConstants(device, cmd, pushConstants, sizeof(pushConstants));
    vkCmdDispatch(cmd.handle, 256, 1, 1);

    VkBufferMemoryBarrier grayscaleToFFTBarrier = RHI::BufferMemoryBarrier(
        sOceanFrequencyBuffers[2 * device.frameIndex],
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1,
                         &grayscaleToFFTBarrier, 0, nullptr);
}

static void FFTPass(RHI::Device& device)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    RHI::BindComputePipeline(device, cmd, sFFTPipeline);
    u32 pushConstantsFFTX[] = {
        sOceanFrequencyBuffers[2 * device.frameIndex].bindlessHandle, 0, 1};
    RHI::SetPushConstants(device, cmd, pushConstantsFFTX,
                          sizeof(pushConstantsFFTX));
    vkCmdDispatch(cmd.handle, 256, 1, 1);
    VkBufferMemoryBarrier FFTToTransposeBarrier = RHI::BufferMemoryBarrier(
        sOceanFrequencyBuffers[2 * device.frameIndex],
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT);
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1,
                         &FFTToTransposeBarrier, 0, nullptr);

    RHI::BindComputePipeline(device, cmd, sTransposePipeline);
    u32 pushConstantsTranspose[] = {
        sOceanFrequencyBuffers[2 * device.frameIndex].bindlessHandle,
        sOceanFrequencyBuffers[2 * device.frameIndex + 1].bindlessHandle, 256};
    RHI::SetPushConstants(device, cmd, pushConstantsTranspose,
                          sizeof(pushConstantsTranspose));
    vkCmdDispatch(cmd.handle, 16, 16, 1);
    VkBufferMemoryBarrier transposeToFFTBarrier = RHI::BufferMemoryBarrier(
        sOceanFrequencyBuffers[2 * device.frameIndex + 1],
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1,
                         &transposeToFFTBarrier, 0, nullptr);

    RHI::BindComputePipeline(device, cmd, sFFTPipeline);
    u32 pushConstantsFFTY[] = {
        sOceanFrequencyBuffers[2 * device.frameIndex + 1].bindlessHandle, 0, 1};
    RHI::SetPushConstants(device, cmd, pushConstantsFFTY,
                          sizeof(pushConstantsFFTY));
    vkCmdDispatch(cmd.handle, 256, 1, 1);
    // VkBufferMemoryBarrier fftToGraphicsBarrier = RHI::BufferMemoryBarrier(
    //     sOceanFrequencyBuffers[2 * device.frameIndex + 1],
    //     VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    // vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
    //                      1, &fftToGraphicsBarrier, 0, nullptr);
    VkBufferMemoryBarrier fftToInverseBarrier = RHI::BufferMemoryBarrier(
        sOceanFrequencyBuffers[2 * device.frameIndex + 1],
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1,
                         &fftToInverseBarrier, 0, nullptr);
}

static void IFFTPass(RHI::Device& device)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    RHI::BindComputePipeline(device, cmd, sFFTPipeline);
    u32 pushConstantsFFTX[] = {
        sOceanFrequencyBuffers[2 * device.frameIndex + 1].bindlessHandle, 1,
        256};
    RHI::SetPushConstants(device, cmd, pushConstantsFFTX,
                          sizeof(pushConstantsFFTX));
    vkCmdDispatch(cmd.handle, 256, 1, 1);
    VkBufferMemoryBarrier IFFTToTransposeBarrier = RHI::BufferMemoryBarrier(
        sOceanFrequencyBuffers[2 * device.frameIndex + 1],
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT);
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1,
                         &IFFTToTransposeBarrier, 0, nullptr);

    RHI::BindComputePipeline(device, cmd, sTransposePipeline);
    u32 pushConstantsTranspose[] = {
        sOceanFrequencyBuffers[2 * device.frameIndex + 1].bindlessHandle,
        sOceanFrequencyBuffers[2 * device.frameIndex].bindlessHandle, 256};
    RHI::SetPushConstants(device, cmd, pushConstantsTranspose,
                          sizeof(pushConstantsTranspose));
    vkCmdDispatch(cmd.handle, 16, 16, 1);
    VkBufferMemoryBarrier transposeToFFTBarrier = RHI::BufferMemoryBarrier(
        sOceanFrequencyBuffers[2 * device.frameIndex],
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1,
                         &transposeToFFTBarrier, 0, nullptr);

    RHI::BindComputePipeline(device, cmd, sFFTPipeline);
    u32 pushConstantsFFTY[] = {
        sOceanFrequencyBuffers[2 * device.frameIndex].bindlessHandle, 1, 256};
    RHI::SetPushConstants(device, cmd, pushConstantsFFTY,
                          sizeof(pushConstantsFFTY));
    vkCmdDispatch(cmd.handle, 256, 1, 1);
    VkBufferMemoryBarrier fftToGraphicsBarrier = RHI::BufferMemoryBarrier(
        sOceanFrequencyBuffers[2 * device.frameIndex],
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1,
                         &fftToGraphicsBarrier, 0, nullptr);
}

static void CopyPass(RHI::Device& device)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    RHI::BindComputePipeline(device, cmd, sCopyPipeline);
    u32 pushConstants[] = {
        sOceanFrequencyBuffers[2 * device.frameIndex].bindlessHandle,
        sHeightMaps[device.frameIndex].bindlessStorageHandle};
    RHI::SetPushConstants(device, cmd, pushConstants, sizeof(pushConstants));
    vkCmdDispatch(cmd.handle, 256, 1, 1);

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = sHeightMaps[device.frameIndex].image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);
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
    RHI::BindGraphicsPipeline(device, cmd, sGraphicsPipeline);
    u32 pushConstants[] = {sHeightMaps[device.frameIndex].bindlessHandle};
    RHI::SetPushConstants(device, cmd, pushConstants, sizeof(pushConstants));

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

    vkCmdDraw(cmd.handle, 6, 1, 0, 0);

    vkCmdEndRendering(cmd.handle);
}

static void ExecuteCommands(RHI::Device& device)
{
    GrayscalePass(device);
    FFTPass(device);
    IFFTPass(device);
    CopyPass(device);
    GraphicsPass(device);
}

static void OnKeyboardPressed(GLFWwindow* window, int key, int scancode,
                              int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
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
        FLY_ERROR("Failed to create context");
        return -1;
    }
    RHI::Device& device = context.devices[0];
    glfwSetWindowUserPointer(window, &device);

    if (!CreatePipelines(device))
    {
        FLY_ERROR("Failed to create pipelines");
        return -1;
    }

    if (!CreateDeviceObjects(device))
    {
        FLY_ERROR("Failed to create device buffers");
        return -1;
    }

    u64 previousFrameTime = 0;
    u64 loopStartTime = Fly::ClockNow();
    u64 currentFrameTime = loopStartTime;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        previousFrameTime = currentFrameTime;
        currentFrameTime = Fly::ClockNow();
        f32 time =
            static_cast<f32>(Fly::ToSeconds(currentFrameTime - loopStartTime));
        f64 deltaTime = Fly::ToSeconds(currentFrameTime - previousFrameTime);

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
    DestroyDeviceObjects(device);
    RHI::DestroyContext(context);
    glfwDestroyWindow(window);
    glfwTerminate();

    FLY_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
