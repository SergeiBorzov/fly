#include "core/assert.h"
#include "core/clock.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "rhi/buffer.h"
#include "rhi/context.h"
#include "rhi/descriptor.h"
#include "rhi/pipeline.h"
#include "rhi/texture.h"
#include "rhi/utils.h"

#include "platform/window.h"

#include "assets/import_image.h"
#include "assets/import_obj.h"

#include "demos/common/simple_camera_fps.h"

struct UniformData
{
    Hls::Math::Mat4 projection = {};
    Hls::Math::Mat4 view = {};
};

struct Vertex
{
    Hls::Math::Vec3 position;
    f32 uvX;
    Hls::Math::Vec3 normal;
    f32 uvY;
};

struct Mesh
{
    Hls::Buffer vertexBuffer;
    i32 diffuseTexture = -1;
    u64 vertexCount = 0;
    VkDescriptorSet descriptorSet;
};

static VkDescriptorSet* sUniformDescriptorSets = nullptr;
static VkDescriptorSet* sMaterialDescriptorSets = nullptr;
static Hls::Texture* sTextures = nullptr;
static u32 sTextureCount = 0;

static Hls::DescriptorPool sDescriptorPool = {};

static Hls::SimpleCameraFPS
    sCamera(Hls::Math::Perspective(45.0f, 1280.0f / 720.0f, 0.01f, 100.0f),
            Hls::Math::Vec3(0.0f, 0.0f, -5.0f));

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

static Mesh* LoadMeshes(Arena& arena, Hls::Device& device, u32& meshCount)
{
    Arena& scratch = GetScratchArena(&arena);
    ArenaMarker marker = ArenaGetMarker(scratch);

    Hls::ObjData objData;
    Hls::ObjImportSettings settings;
    settings.scale = 0.01f;
    settings.uvOriginBottom = false;
    settings.flipFaceOrientation = true;
    if (!Hls::ImportWavefrontObj(HLS_STRING8_LITERAL("sponza.obj"), settings,
                                 objData))
    {
        HLS_ERROR("Failed to parse sponza.obj");
        return nullptr;
    }
    HLS_ASSERT(objData.shapeCount);

    Vertex* vertices = HLS_ALLOC(scratch, Vertex, objData.faceCount * 3);
    for (u64 i = 0; i < objData.faceCount; i++)
    {
        const Hls::ObjData::Face& face = objData.faces[i];
        for (u32 j = 0; j < 3; j++)
        {
            Vertex& v = vertices[3 * i + j];
            v.position = objData.vertices[face.indices[j].vertexIndex];
            v.normal = objData.normals[face.indices[j].normalIndex];
            const Hls::Math::Vec2 uv =
                objData.texCoords[face.indices[j].texCoordIndex];
            v.uvX = uv.x;
            v.uvY = uv.y;
        }
    }

    // TODO: Process shapes with multiple materials
    meshCount = 0;

    i32* textureIndices = HLS_ALLOC(scratch, i32, objData.materialCount);
    u32 textureCount = 0;
    for (u64 i = 0; i < objData.shapeCount; i++)
    {
        const Hls::ObjData::Shape& s = objData.shapes[i];
        if (objData.faces[s.firstFaceIndex].materialIndex !=
            objData.faces[s.firstFaceIndex + s.faceCount - 1].materialIndex)
        {
            continue;
        }
        const Hls::ObjData::Material& material =
            objData.materials[objData.faces[s.firstFaceIndex].materialIndex];
        if (material.diffuseTextureIndex == -1)
        {
            continue;
        }
        meshCount++;

        bool found = false;
        for (u32 j = 0; j < textureCount; j++)
        {
            if (textureIndices[j] == material.diffuseTextureIndex)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            textureIndices[textureCount++] = material.diffuseTextureIndex;
        }
    }

    Mesh* meshes = HLS_ALLOC(arena, Mesh, meshCount);
    sTextureCount = textureCount;
    sTextures = HLS_ALLOC(arena, Hls::Texture, textureCount);

    u32 j = 0;
    for (u64 i = 0; i < objData.shapeCount; i++)
    {
        const Hls::ObjData::Shape& s = objData.shapes[i];
        if (objData.faces[s.firstFaceIndex].materialIndex !=
            objData.faces[s.firstFaceIndex + s.faceCount - 1].materialIndex)
        {
            continue;
        }
        const Hls::ObjData::Material& material =
            objData.materials[objData.faces[s.firstFaceIndex].materialIndex];
        if (material.diffuseTextureIndex == -1)
        {
            continue;
        }

        Mesh& mesh = meshes[j++];
        mesh.vertexCount = s.faceCount * 3;
        const Vertex* start = &vertices[s.firstFaceIndex * 3];

        if (!Hls::CreateBuffer(device,
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                               VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT,
                               sizeof(Vertex) * mesh.vertexCount,
                               mesh.vertexBuffer))
        {
            return nullptr;
        }

        bool res = TransferDataToBuffer(device, start,
                                        sizeof(Vertex) * mesh.vertexCount,
                                        mesh.vertexBuffer);

        i32 index = 0;
        for (u32 k = 0; k < textureCount; k++)
        {
            if (textureIndices[k] == material.diffuseTextureIndex)
            {
                index = k;
                break;
            }
        }

        mesh.diffuseTexture = index;

        HLS_ASSERT(res);
    }

    for (u32 i = 0; i < textureCount; i++)
    {
        Hls::Image image;
        bool res =
            Hls::LoadImageFromFile(objData.textures[textureIndices[i]], image);
        HLS_ASSERT(res);
        res = Hls::CreateTexture(device, image.width, image.height,
                                 VK_FORMAT_R8G8B8A8_SRGB, sTextures[i]);
        HLS_ASSERT(res);
        res = TransferImageDataToTexture(device, image, sTextures[i]);
        HLS_ASSERT(res);
        Hls::FreeImage(image);
    }

    Hls::FreeWavefrontObj(objData);
    ArenaPopToMarker(scratch, marker);
    return meshes;
}

static void RecordCommands(Hls::Device& device, Hls::GraphicsPipeline& pipeline,
                           const Mesh* meshes, u32 meshCount)
{
    Hls::CommandBuffer& cmd = RenderFrameCommandBuffer(device);

    const Hls::SwapchainTexture& swapchainTexture =
        RenderFrameSwapchainTexture(device);
    VkRect2D renderArea = {{0, 0},
                           {swapchainTexture.width, swapchainTexture.height}};
    VkRenderingAttachmentInfo colorAttachment = Hls::ColorAttachmentInfo(
        swapchainTexture.imageView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = Hls::DepthAttachmentInfo(
        device.depthTexture.imageView,
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
        &sUniformDescriptorSets[device.frameIndex], 0, nullptr);
    for (u32 i = 0; i < meshCount; i++)
    {
        const Mesh& mesh = meshes[i];

        vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline.layout, 1, 1, &meshes[i].descriptorSet,
                                0, nullptr);
        vkCmdBindDescriptorSets(
            cmd.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 2, 1,
            &sMaterialDescriptorSets[meshes[i].diffuseTexture], 0, nullptr);
        vkCmdDraw(cmd.handle, static_cast<u32>(mesh.vertexCount), 1, 0, 0);
    }

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
    GLFWwindow* window =
        glfwCreateWindow(1280, 720, "Import obj", nullptr, nullptr);
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
    settings.deviceFeatures2.features.samplerAnisotropy = VK_TRUE;
    settings.deviceFeatures2.pNext = &dynamicRenderingFeatures;
    settings.instanceExtensions =
        glfwGetRequiredInstanceExtensions(&settings.instanceExtensionCount);
    settings.deviceExtensions = requiredDeviceExtensions;
    settings.deviceExtensionCount = STACK_ARRAY_COUNT(requiredDeviceExtensions);
    settings.windowPtr = Hls::GetNativeWindowPtr(window);

    Hls::Context context;
    if (!Hls::CreateContext(settings, context))
    {
        HLS_ERROR("Failed to create context");
        return -1;
    }

    Hls::Device& device = context.devices[0];

    Hls::GraphicsPipelineProgrammableStage programmableStage{};
    Hls::ShaderPathMap shaderPathMap{};
    shaderPathMap[Hls::ShaderType::Vertex] =
        HLS_STRING8_LITERAL("unlit.vert.spv");
    shaderPathMap[Hls::ShaderType::Fragment] =
        HLS_STRING8_LITERAL("unlit.frag.spv");
    if (!Hls::LoadProgrammableStage(arena, device, shaderPathMap,
                                    programmableStage))
    {
        HLS_ERROR("Failed to load and create shader modules");
    }

    u32 meshCount = 0;
    Mesh* meshes = LoadMeshes(arena, device, meshCount);
    if (!meshes)
    {
        return -1;
    }
    HLS_LOG("Loaded %u meshes", meshCount);

    VkDescriptorPoolSize poolSizes[3];
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = HLS_FRAME_IN_FLIGHT_COUNT;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = meshCount;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSizes[2].descriptorCount = sTextureCount;

    if (!Hls::CreateDescriptorPool(device, poolSizes, 3,
                                   meshCount + sTextureCount +
                                       HLS_FRAME_IN_FLIGHT_COUNT,
                                   sDescriptorPool))
    {
        return -1;
    }

    VkDescriptorSetLayout uniformDescriptorSetLayouts[2] = {
        programmableStage[Hls::ShaderType::Vertex]
            .descriptorSetLayouts[0]
            .handle,
        programmableStage[Hls::ShaderType::Vertex]
            .descriptorSetLayouts[0]
            .handle};
    sUniformDescriptorSets =
        HLS_ALLOC(arena, VkDescriptorSet, HLS_FRAME_IN_FLIGHT_COUNT);
    if (!AllocateDescriptorSets(
            device, sDescriptorPool, uniformDescriptorSetLayouts,
            HLS_FRAME_IN_FLIGHT_COUNT, sUniformDescriptorSets))
    {
        HLS_ERROR("Failed to allocate uniform descriptors");
        return -1;
    }

    for (u32 i = 0; i < meshCount; i++)
    {
        if (!AllocateDescriptorSets(device, sDescriptorPool,
                                    &programmableStage[Hls::ShaderType::Vertex]
                                         .descriptorSetLayouts[1]
                                         .handle,
                                    1, &meshes[i].descriptorSet))
        {
            HLS_ERROR("Failed to allocate mesh descriptors");
            return -1;
        }
        Hls::BindBufferToDescriptorSet(device, meshes[i].vertexBuffer, 0,
                                       sizeof(Vertex) * meshes[i].vertexCount,
                                       meshes[i].descriptorSet,
                                       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
    }

    sMaterialDescriptorSets = HLS_ALLOC(arena, VkDescriptorSet, sTextureCount);
    for (u32 i = 0; i < sTextureCount; i++)
    {
        if (!AllocateDescriptorSets(
                device, sDescriptorPool,
                &programmableStage[Hls::ShaderType::Fragment]
                     .descriptorSetLayouts[0]
                     .handle,
                1, &sMaterialDescriptorSets[i]))
        {
            HLS_ERROR("Failed to allocate material descriptor set");
            return -1;
        }
        Hls::BindTextureToDescriptorSet(device, sTextures[i],
                                        sMaterialDescriptorSets[i], 0);
    }

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
            sUniformDescriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0);
    }

    Hls::GraphicsPipelineFixedStateStage fixedState{};
    fixedState.pipelineRendering.colorAttachments[0] =
        device.surfaceFormat.format;
    fixedState.pipelineRendering.depthAttachmentFormat =
        device.depthTexture.format;
    fixedState.pipelineRendering.colorAttachmentCount = 1;
    fixedState.colorBlendState.attachmentCount = 1;
    fixedState.depthStencilState.depthTestEnable = true;
    fixedState.rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
        UniformData uniformData = {sCamera.GetProjection(), sCamera.GetView()};
        Hls::CopyDataToBuffer(device, uniformBuffer,
                              device.frameIndex * sizeof(UniformData),
                              &uniformData, sizeof(UniformData));

        Hls::BeginRenderFrame(device);
        RecordCommands(device, graphicsPipeline, meshes, meshCount);
        Hls::EndRenderFrame(device);
    }

    Hls::WaitAllDevicesIdle(context);

    for (u32 i = 0; i < sTextureCount; i++)
    {
        Hls::DestroyTexture(device, sTextures[i]);
    }

    for (u32 i = 0; i < meshCount; i++)
    {
        Hls::DestroyBuffer(device, meshes[i].vertexBuffer);
    }

    Hls::UnmapBuffer(device, uniformBuffer);
    Hls::DestroyBuffer(device, uniformBuffer);

    Hls::DestroyDescriptorPool(device, sDescriptorPool);
    Hls::DestroyGraphicsPipeline(device, graphicsPipeline);
    Hls::DestroyContext(context);

    glfwDestroyWindow(window);
    glfwTerminate();
    HLS_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
