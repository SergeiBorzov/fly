#include "core/clock.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "rhi/allocation_callbacks.h"
#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/shader_program.h"

#include "demos/common/scene.h"
#include "demos/common/simple_camera_fps.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>

using namespace Fly;

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 1024

struct UniformData
{
    Math::Vec4 fetchSpeedDirSpread;
    Math::Vec4 normalizationDomainTime;
};
static UniformData sUniformData;

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

static RHI::ComputePipeline sFFTPipeline;
static RHI::ComputePipeline sTransposePipeline;
static RHI::ComputePipeline sJonswapPipeline;
static RHI::ComputePipeline sCopyPipeline;
static RHI::GraphicsPipeline sGraphicsPipeline;
static bool CreatePipelines(RHI::Device& device)
{
    RHI::ComputePipeline* computePipelines[] = {
        &sFFTPipeline, &sTransposePipeline, &sJonswapPipeline, &sCopyPipeline};

    const char* computeShaderPaths[] = {"fft.comp.spv", "transpose.comp.spv",
                                        "jonswap.comp.spv", "copy.comp.spv"};

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
    RHI::DestroyComputePipeline(device, sJonswapPipeline);
    RHI::DestroyComputePipeline(device, sTransposePipeline);
    RHI::DestroyComputePipeline(device, sFFTPipeline);
}

static f32 sTime;

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

static RHI::Buffer sUniformBuffers[FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Buffer sOceanFrequencyBuffers[2 * FLY_FRAME_IN_FLIGHT_COUNT];
static RHI::Texture sHeightMaps[FLY_FRAME_IN_FLIGHT_COUNT];
static bool CreateDeviceObjects(RHI::Device& device)
{
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

static void JonswapPass(RHI::Device& device)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    RHI::BindComputePipeline(device, cmd, sJonswapPipeline);

    u32 pushConstants[] = {
        sOceanFrequencyBuffers[2 * device.frameIndex].bindlessHandle,
        sUniformBuffers[device.frameIndex].bindlessHandle, 256};
    RHI::SetPushConstants(device, cmd, &pushConstants, sizeof(pushConstants));

    vkCmdDispatch(cmd.handle, 256, 1, 1);
    VkBufferMemoryBarrier jonswapToIFFTBarrier = RHI::BufferMemoryBarrier(
        sOceanFrequencyBuffers[2 * device.frameIndex],
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1,
                         &jonswapToIFFTBarrier, 0, nullptr);
}

static void IFFTPass(RHI::Device& device)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    RHI::BindComputePipeline(device, cmd, sFFTPipeline);
    u32 pushConstantsFFTX[] = {
        sOceanFrequencyBuffers[2 * device.frameIndex].bindlessHandle, 1, 256};
    RHI::SetPushConstants(device, cmd, pushConstantsFFTX,
                          sizeof(pushConstantsFFTX));
    vkCmdDispatch(cmd.handle, 256, 1, 1);
    VkBufferMemoryBarrier IFFTToTransposeBarrier = RHI::BufferMemoryBarrier(
        sOceanFrequencyBuffers[2 * device.frameIndex],
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT);
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1,
                         &IFFTToTransposeBarrier, 0, nullptr);

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
        sOceanFrequencyBuffers[2 * device.frameIndex + 1].bindlessHandle, 1,
        256};
    RHI::SetPushConstants(device, cmd, pushConstantsFFTY,
                          sizeof(pushConstantsFFTY));
    vkCmdDispatch(cmd.handle, 256, 1, 1);
    VkBufferMemoryBarrier fftToCopyBarrier = RHI::BufferMemoryBarrier(
        sOceanFrequencyBuffers[2 * device.frameIndex + 1],
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT);
    vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1,
                         &fftToCopyBarrier, 0, nullptr);
}

static void ProcessImGuiFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
        ImGui::Begin("Ocean parameters");

        ImGui::SliderFloat("Fetch", &sUniformData.fetchSpeedDirSpread.x, 0.0f,
                           1000000.0f, "%.1f");
        ImGui::SliderFloat("Wind Speed", &sUniformData.fetchSpeedDirSpread.y,
                           0.0f, 50.0f, "%.2f");
        ImGui::SliderFloat("Theta 0", &sUniformData.fetchSpeedDirSpread.z,
                           -FLY_MATH_PI, FLY_MATH_PI, "%.2f rad");
        ImGui::SliderFloat("Spread", &sUniformData.fetchSpeedDirSpread.w, 0.01f,
                           100.0f, "%.2f");
        ImGui::SliderFloat("Normalization scalar",
                           &sUniformData.normalizationDomainTime.x, 0.0f, 10.0f,
                           "%.2f");
        ImGui::SliderFloat("Domain size",
                           &sUniformData.normalizationDomainTime.y, 128.0f,
                           8192.0f, "%.1f");

        ImGui::End();
    }

    ImGui::Render();
}

static void CopyPass(RHI::Device& device)
{
    RHI::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    RHI::BindComputePipeline(device, cmd, sCopyPipeline);
    u32 pushConstants[] = {
        sOceanFrequencyBuffers[2 * device.frameIndex + 1].bindlessHandle,
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

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd.handle);
    vkCmdEndRendering(cmd.handle);
}

static void ExecuteCommands(RHI::Device& device)
{
    JonswapPass(device);
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

    if (!CreateImGuiContext(context, device, window))
    {
        FLY_ERROR("Failed to create imgui context");
        return -1;
    }

    u64 previousFrameTime = 0;
    u64 loopStartTime = Fly::ClockNow();
    u64 currentFrameTime = loopStartTime;

    sUniformData.fetchSpeedDirSpread =
        Math::Vec4(500000.0f, 30.0f, 0.0f, 10.0f);
    sUniformData.normalizationDomainTime = Math::Vec4(1, 512.0f, 0.0f, 0.0f);
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        previousFrameTime = currentFrameTime;
        currentFrameTime = Fly::ClockNow();
        sTime =
            static_cast<f32>(Fly::ToSeconds(currentFrameTime - loopStartTime));
        f64 deltaTime = Fly::ToSeconds(currentFrameTime - previousFrameTime);

        sUniformData.normalizationDomainTime.z = sTime;
        sCamera.Update(window, deltaTime);

        ProcessImGuiFrame();
        RHI::CopyDataToBuffer(device, &sUniformData, sizeof(UniformData), 0,
                              sUniformBuffers[device.frameIndex]);

        RHI::BeginRenderFrame(device);
        ExecuteCommands(device);
        RHI::EndRenderFrame(device);
    }

    RHI::WaitAllDevicesIdle(context);

    DestroyPipelines(device);
    DestroyDeviceObjects(device);
    DestroyImGuiContext(device);
    RHI::DestroyContext(context);
    glfwDestroyWindow(window);
    glfwTerminate();

    FLY_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
