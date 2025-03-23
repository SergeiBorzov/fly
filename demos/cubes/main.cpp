#include "core/assert.h"
#include "core/clock.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "rhi/buffer.h"
#include "rhi/context.h"
#include "rhi/image.h"
#include "rhi/pipeline.h"
#include "rhi/utils.h"

#include <GLFW/glfw3.h>

#include "demos/common/simple_camera_fps.h"

static Hls::DescriptorPool sDescriptorPool = {};

static Hls::SimpleCameraFPS
    sCamera(Hls::Math::Perspective(45.0f, 1280.0f / 720.0f, 0.01f, 100.0f),
            Hls::Math::Vec3(0.0f, 0.0f, -5.0f));

struct UniformData
{
    Hls::Math::Mat4 projection = {};
    Hls::Math::Mat4 view = {};
    f32 time = 0.0f;
    f32 padding[15] = {0.0f};
};

static void SetVulkanLayerPathEnvVariable()
{
    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);
    const char* binaryPath = GetBinaryDirectoryPath(arena);
    HLS_ASSERT(binaryPath);
    HLS_ENSURE(SetEnv("VK_LAYER_PATH", binaryPath));
    ArenaPopToMarker(arena, marker);
}

static void OnKeyboardPressed(GLFWwindow* window, int key, int scancode,
                              int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }
}

static void ErrorCallbackGLFW(i32 error, const char* description)
{
    HLS_ERROR("GLFW - error: %s", description);
}

static void RecordCommands(Hls::Device& device, Hls::GraphicsPipeline& pipeline)
{
    Hls::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    VkImage image = RenderFrameSwapchainImage(device);
    VkImageView imageView = RenderFrameSwapchainImageView(device);
    VkRect2D renderArea = SwapchainRect2D(device);
    VkRenderingAttachmentInfo colorAttachment = Hls::ColorAttachmentInfo(
        imageView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = Hls::DepthAttachmentInfo(
        device.depthImage.imageView,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo =
        Hls::RenderingInfo(renderArea, &colorAttachment, 1, &depthAttachment);

    vkCmdBeginRendering(cmd.handle, &renderInfo);
    vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline.handle);

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

    vkCmdBindDescriptorSets(
        cmd.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0, 1,
        &sDescriptorPool.descriptorSets[device.frameIndex], 0, nullptr);
    vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline.layout, 1, 1,
                            &sDescriptorPool.descriptorSets[3], 0, nullptr);
    vkCmdDraw(cmd.handle, 36, 10000, 0, 0);

    vkCmdEndRendering(cmd.handle);
}

int main(int argc, char* argv[])
{
    InitThreadContext();
    Arena& arena = GetScratchArena();
    if (!InitLogger())
    {
        return -1;
    }

    SetVulkanLayerPathEnvVariable();

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
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Cubes", nullptr, nullptr);
    if (!window)
    {
        HLS_ERROR("Failed to create glfw window");
        glfwTerminate();
        return -1;
    }
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetKeyCallback(window, OnKeyboardPressed);

    const char* requiredDeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{};
    dynamicRenderingFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRenderingFeatures.dynamicRendering = VK_TRUE;

    Hls::ContextSettings settings{};
    settings.deviceFeatures2.pNext = &dynamicRenderingFeatures;
    settings.instanceExtensions =
        glfwGetRequiredInstanceExtensions(&settings.instanceExtensionCount);
    settings.deviceExtensions = requiredDeviceExtensions;
    settings.deviceExtensionCount = STACK_ARRAY_COUNT(requiredDeviceExtensions);
    settings.windowPtr = window;

    Hls::Context context;
    if (!Hls::CreateContext(settings, context))
    {
        HLS_ERROR("Failed to create context");
        return -1;
    }

    Hls::Device& device = context.devices[0];

    Hls::GraphicsPipelineProgrammableStage programmableStage{};
    Hls::ShaderPathMap shaderPathMap{};
    shaderPathMap[Hls::ShaderType::Vertex] = "cubes.vert.spv";
    shaderPathMap[Hls::ShaderType::Fragment] = "cubes.frag.spv";
    if (!Hls::LoadProgrammableStage(arena, device, shaderPathMap,
                                    programmableStage))
    {
        HLS_ERROR("Failed to load and create shader modules");
    }

    if (!CreatePoolAndAllocateDescriptorsForProgrammableStage(
            arena, device, programmableStage, sDescriptorPool))
    {
        HLS_ERROR("Failed to create descriptor pool!");
        return -1;
    }

    HLS_LOG("Number of descriptor sets %u", sDescriptorPool.descriptorSetCount);

    Hls::Image image;
    if (!Hls::LoadImageFromFile(arena, "default.png", image))
    {
        HLS_ERROR("Failed to load image");
        return -1;
    }
    Hls::Texture texture;
    if (!Hls::CreateTexture(device, image.width, image.height,
                            VK_FORMAT_R8G8B8A8_SRGB, texture))
    {
        HLS_ERROR("Failed to create texture");
        return -1;
    }
    if (!Hls::TransferImageDataToTexture(device, image, texture))
    {
        HLS_ERROR("Failed to transfer image data to texture");
        return -1;
    }
    Hls::BindTextureToDescriptorSet(device, texture,
                                    sDescriptorPool.descriptorSets[3], 0);

    Hls::Buffer uniformBuffer;
    if (!Hls::CreateBuffer(
            device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            sizeof(UniformData) * HLS_FRAME_IN_FLIGHT_COUNT, uniformBuffer))
    {
        HLS_ERROR("Failed to create uniform buffer!");
        return -1;
    }
    if (!Hls::MapBuffer(device, uniformBuffer))
    {
        HLS_ERROR("Failed to map the uniform buffer!");
        return -1;
    }

    for (u32 i = 0; i < HLS_FRAME_IN_FLIGHT_COUNT; i++)
    {
        Hls::BindBufferToDescriptorSet(
            device, uniformBuffer, i * sizeof(UniformData), sizeof(UniformData),
            sDescriptorPool.descriptorSets[i],
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0);
    }

    Hls::GraphicsPipelineFixedStateStage fixedState{};
    fixedState.pipelineRendering.colorAttachments[0] =
        device.surfaceFormat.format;
    fixedState.pipelineRendering.depthAttachmentFormat =
        device.depthImage.format;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.colorBlendState.attachmentCount = 1;
    fixedState.depthStencilState.depthTestEnable = true;
    fixedState.rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

    Hls::GraphicsPipeline graphicsPipeline{};
    if (!Hls::CreateGraphicsPipeline(device, fixedState, programmableStage,
                                     graphicsPipeline))
    {
        HLS_ERROR("Failed to create graphics pipeline");
        return -1;
    }
    Hls::DestroyGraphicsPipelineProgrammableStage(device, programmableStage);

    u64 previousFrameTime = 0;
    u64 loopStartTime = Hls::ClockNow();
    u64 currentFrameTime = loopStartTime;
    while (!glfwWindowShouldClose(window))
    {
        previousFrameTime = currentFrameTime;
        currentFrameTime = Hls::ClockNow();

        f32 time =
            static_cast<f32>(Hls::ToSeconds(currentFrameTime - loopStartTime));
        f64 deltaTime = Hls::ToSeconds(currentFrameTime - previousFrameTime);

        glfwPollEvents();

        sCamera.Update(window, deltaTime);
        UniformData uniformData = {sCamera.GetProjection(), sCamera.GetView(),
                                   time};
        Hls::CopyDataToBuffer(device, uniformBuffer,
                              device.frameIndex * sizeof(UniformData),
                              &uniformData, sizeof(UniformData));

        BeginRenderFrame(context, device);
        RecordCommands(device, graphicsPipeline);
        EndRenderFrame(context, device);
    }

    Hls::WaitAllDevicesIdle(context);
    Hls::UnmapBuffer(device, uniformBuffer);
    Hls::DestroyBuffer(device, uniformBuffer);
    Hls::DestroyTexture(device, texture);

    Hls::DestroyDescriptorPool(device, sDescriptorPool.handle);
    Hls::DestroyGraphicsPipeline(device, graphicsPipeline);
    Hls::DestroyContext(context);

    glfwDestroyWindow(window);
    glfwTerminate();
    HLS_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
