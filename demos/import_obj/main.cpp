#include "core/assert.h"
#include "core/clock.h"
#include "core/filesystem.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "rhi/context.h"
#include "rhi/pipeline.h"
#include "rhi/storage_buffer.h"
#include "rhi/texture.h"
#include "rhi/uniform_buffer.h"
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
    Hls::StorageBuffer vertexBuffer;
    i32 diffuseTexture = -1;
    u64 vertexCount = 0;
};

static Hls::Texture* sTextures = nullptr;
static u32 sTextureCount = 0;

static Hls::UniformBuffer sUniformBuffers[HLS_FRAME_IN_FLIGHT_COUNT];

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
    if (!Hls::ImportWavefrontObj("sponza.obj", settings, objData))
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

        if (!Hls::CreateStorageBuffer(device, start,
                                      sizeof(Vertex) * mesh.vertexCount,
                                      mesh.vertexBuffer))
        {
            return nullptr;
        }

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
    }

    for (u32 i = 0; i < textureCount; i++)
    {
        Hls::Image image;
        bool res = Hls::ImportImageFromFile(objData.textures[textureIndices[i]],
                                            image);
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

    vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline.layout, 0, 1,
                            &device.bindlessDescriptorSet, 0, nullptr);

    u32 indices[3] = {sUniformBuffers[device.frameIndex].bindlessHandle, 0, 0};
    for (u32 i = 0; i < meshCount; i++)
    {
        const Mesh& mesh = meshes[i];
        indices[1] = mesh.vertexBuffer.bindlessHandle;
        indices[2] = sTextures[mesh.diffuseTexture].bindlessHandle;
        vkCmdPushConstants(cmd.handle, pipeline.layout, VK_SHADER_STAGE_ALL, 0,
                           sizeof(u32) * 3, indices);
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

    Hls::ContextSettings settings{};
    settings.deviceFeatures2.features.samplerAnisotropy = VK_TRUE;
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
    Hls::Path::Create(arena, HLS_STRING8_LITERAL("unlit.vert.spv"),
                      shaderPathMap[Hls::ShaderType::Vertex]);
    Hls::Path::Create(arena, HLS_STRING8_LITERAL("unlit.frag.spv"),
                      shaderPathMap[Hls::ShaderType::Fragment]);
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

    for (u32 i = 0; i < HLS_FRAME_IN_FLIGHT_COUNT; i++)
    {
        if (!Hls::CreateUniformBuffer(device, nullptr, sizeof(UniformData),
                                      sUniformBuffers[i]))
        {
            HLS_ERROR("Failed to create uniform buffer!");
        }
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
        Hls::CopyDataToUniformBuffer(device, &uniformData, sizeof(UniformData),
                                     0, sUniformBuffers[device.frameIndex]);

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
        Hls::DestroyStorageBuffer(device, meshes[i].vertexBuffer);
    }

    for (u32 i = 0; i < HLS_FRAME_IN_FLIGHT_COUNT; i++)
    {
        Hls::DestroyUniformBuffer(device, sUniformBuffers[i]);
    }

    Hls::DestroyGraphicsPipeline(device, graphicsPipeline);
    Hls::DestroyContext(context);

    glfwDestroyWindow(window);
    glfwTerminate();
    HLS_LOG("Shutdown successful");
    ShutdownLogger();
    ReleaseThreadContext();
    return 0;
}
