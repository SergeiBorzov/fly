#include "core/assert.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "assets/import_gltf.h"
#include "assets/import_image.h"
#include "assets/import_spv.h"

#include "rhi/shader_program.h"

#include "scene.h"

namespace Hls
{

struct Vertex
{
    Math::Vec3 position;
    f32 uvX;
    Math::Vec3 normal;
    f32 uvY;
};

static bool ProcessPrimitiveVertices(RHI::Device& device, cgltf_data* data,
                                     cgltf_primitive* primitive,
                                     Submesh& submesh)
{
    HLS_ASSERT(data);
    HLS_ASSERT(primitive);

    bool isPosPresent = false;
    bool isNormalPresent = false;
    bool isTexCoordPresent = false;

    u64 vertexCount = 0;
    for (u64 i = 0; i < primitive->attributes_count; i++)
    {
        cgltf_attribute& attribute = primitive->attributes[i];
        cgltf_accessor* accessor = attribute.data;

        vertexCount = accessor->count;

        if (attribute.type == cgltf_attribute_type_position)
        {
            isPosPresent = true;
            if (accessor->component_type != cgltf_component_type_r_32f &&
                accessor->type != cgltf_type_vec3)
            {
                return false;
            }
        }
        else if (attribute.type == cgltf_attribute_type_normal)
        {
            isNormalPresent = true;
            if (accessor->component_type != cgltf_component_type_r_32f &&
                accessor->type != cgltf_type_vec3)
            {
                return false;
            }
        }
        else if (attribute.type == cgltf_attribute_type_texcoord)
        {
            isTexCoordPresent = true;
            if (accessor->component_type != cgltf_component_type_r_32f &&
                accessor->type != cgltf_type_vec2)
            {
                return false;
            }
        }
    }

    if (!isPosPresent || !isNormalPresent || !isTexCoordPresent)
    {
        return false;
    }

    Arena& scratch = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(scratch);

    Vertex* vertices = HLS_ALLOC(scratch, Vertex, vertexCount);
    for (u64 i = 0; i < primitive->attributes_count; i++)
    {
        cgltf_attribute& attribute = primitive->attributes[i];
        cgltf_accessor* accessor = attribute.data;
        cgltf_buffer_view* bv = accessor->buffer_view;
        cgltf_buffer* buffer = bv->buffer;

        ArenaMarker loopMarker = ArenaGetMarker(scratch);
        if (attribute.type == cgltf_attribute_type_position)
        {
            f32* unpacked = HLS_ALLOC(scratch, f32, vertexCount * 3);
            cgltf_accessor_unpack_floats(accessor, unpacked, vertexCount * 3);
            for (u64 j = 0; j < vertexCount; j++)
            {
                memcpy(vertices[j].position.data, unpacked + 3 * j,
                       sizeof(f32) * 3);
                vertices[j].position *= 0.01f;
            }
        }
        else if (attribute.type == cgltf_attribute_type_normal)
        {
            f32* unpacked = HLS_ALLOC(scratch, f32, vertexCount * 3);
            cgltf_accessor_unpack_floats(accessor, unpacked, vertexCount * 3);
            for (u64 j = 0; j < vertexCount; j++)
            {
                memcpy(vertices[j].normal.data, unpacked + 3 * j,
                       sizeof(f32) * 3);
            }
        }
        else if (attribute.type == cgltf_attribute_type_texcoord)
        {
            f32* unpacked = HLS_ALLOC(scratch, f32, vertexCount * 2);
            cgltf_accessor_unpack_floats(accessor, unpacked, vertexCount * 3);
            for (u64 j = 0; j < vertexCount; j++)
            {
                vertices[j].uvX = *(unpacked + 2 * j);
                vertices[j].uvY = *(unpacked + 2 * j + 1);
            }
        }

        ArenaPopToMarker(scratch, loopMarker);
    }

    if (!RHI::CreateStorageBuffer(device, false, vertices,
                                  sizeof(Vertex) * vertexCount,
                                  submesh.vertexBuffer))
    {
        return false;
    }

    ArenaPopToMarker(scratch, marker);
    return true;
}

static bool ProcessPrimitiveMaterial(RHI::Device& device, cgltf_data* data,
                                     cgltf_material* material, Submesh& submesh)
{
    HLS_ASSERT(data);
    HLS_ASSERT(material);

    if (!material->has_pbr_metallic_roughness)
    {
        return false;
    }

    submesh.materialIndex =
        static_cast<u32>(cgltf_material_index(data, material));
    return true;
}

static bool ProcessPrimitive(RHI::Device& device, cgltf_data* data,
                             cgltf_primitive* primitive, Submesh& submesh)
{
    HLS_ASSERT(data);
    HLS_ASSERT(primitive);

    if (!ProcessPrimitiveVertices(device, data, primitive, submesh))
    {
        return false;
    }

    ProcessPrimitiveMaterial(device, data, primitive->material, submesh);
    return true;
}

static bool ProcessMaterials(RHI::Device& device, cgltf_data* data,
                             Scene& scene)
{
    HLS_ASSERT(data);
    if (data->materials_count == 0)
    {
        return true;
    }

    Arena& scratch = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(scratch);
    MaterialData* materialDataBuffer =
        HLS_ALLOC(scratch, MaterialData, data->materials_count);

    u64 materialDataIndex = 0;
    for (u64 i = 0; i < data->materials_count; i++)
    {
        cgltf_material& material = data->materials[i];
        MaterialData& materialData = materialDataBuffer[materialDataIndex];

        if (!material.has_pbr_metallic_roughness)
        {
            continue;
        }

        cgltf_texture_view albedoTextureView =
            material.pbr_metallic_roughness.base_color_texture;
        if (albedoTextureView.has_transform)
        {
            materialData.albedoTexture.offset.x =
                albedoTextureView.transform.offset[0];
            materialData.albedoTexture.offset.y =
                albedoTextureView.transform.offset[1];
            materialData.albedoTexture.scale.x =
                albedoTextureView.transform.scale[0];
            materialData.albedoTexture.scale.y =
                albedoTextureView.transform.scale[1];
        }

        u32 textureIndex = static_cast<u32>(
            cgltf_texture_index(data, albedoTextureView.texture));
        materialData.albedoTexture.textureHandle = textureIndex;

        materialDataIndex++;
    }

    if (!RHI::CreateStorageBuffer(device, false, materialDataBuffer,
                                  sizeof(MaterialData) * materialDataIndex,
                                  scene.materialBuffer))
    {
        return false;
    }

    ArenaPopToMarker(scratch, marker);
    return true;
}

static bool ProcessTexture(RHI::Device& device, cgltf_data* data,
                           cgltf_texture* texture, RHI::Texture& hlsTexture)
{
    HLS_ASSERT(data);
    HLS_ASSERT(texture);

    if (!texture->image || !texture->image->uri)
    {
        return false;
    }

    Hls::Image image;
    if (!Hls::LoadImageFromFile(texture->image->uri, image))
    {
        HLS_ERROR("Failed to load image %s", texture->image->uri);
        return false;
    }

    if (!RHI::CreateTexture(device, image.data, image.width, image.height,
                            image.channelCount, VK_FORMAT_R8G8B8A8_SRGB,
                            RHI::Sampler::FilterMode::Trilinear,
                            RHI::Sampler::WrapMode::Repeat, 8, hlsTexture))
    {
        HLS_ERROR("Failed to create texture %s", texture->image->uri);
        return false;
    }
    Hls::FreeImage(image);

    return true;
}

static bool ProcessIndices(RHI::Device& device, cgltf_data* data, Scene& scene)
{
    HLS_ASSERT(data);

    Arena& scratch = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(scratch);

    u32 indexCount = 0;
    for (u64 i = 0; i < data->meshes_count; i++)
    {
        cgltf_mesh& mesh = data->meshes[i];
        for (u64 j = 0; j < mesh.primitives_count; j++)
        {
            cgltf_primitive& primitive = mesh.primitives[j];
            if (primitive.indices)
            {
                indexCount += static_cast<u32>(primitive.indices->count);
            }
        }
    }

    if (indexCount == 0)
    {
        return false;
    }

    u32* indices = HLS_ALLOC(scratch, u32, indexCount);
    u32 offset = 0;
    for (u64 i = 0; i < data->meshes_count; i++)
    {
        cgltf_mesh& mesh = data->meshes[i];
        for (u64 j = 0; j < mesh.primitives_count; j++)
        {
            cgltf_primitive& primitive = mesh.primitives[j];
            if (primitive.indices)
            {
                cgltf_accessor_unpack_indices(primitive.indices,
                                              indices + offset, sizeof(u32),
                                              primitive.indices->count);
                scene.meshes[i].submeshes[j].indexCount =
                    static_cast<u32>(primitive.indices->count);
                scene.meshes[i].submeshes[j].indexOffset = offset;
                offset += static_cast<u32>(primitive.indices->count);
            }
        }
    }

    if (!RHI::CreateIndexBuffer(device, indices, indexCount * sizeof(u32),
                                scene.indexBuffer))
    {
        return false;
    }

    ArenaPopToMarker(scratch, marker);
    return true;
}

static bool ProcessScene(Arena& arena, RHI::Device& device, cgltf_data* data,
                         cgltf_scene* scene, Scene& hlsScene)
{
    HLS_ASSERT(data);
    HLS_ASSERT(scene);

    if (data->meshes_count == 0)
    {
        return true;
    }

    Arena& scratch = GetScratchArena(&arena);
    ArenaMarker marker = ArenaGetMarker(arena);

    hlsScene.meshCount = static_cast<u32>(data->meshes_count);
    hlsScene.meshes = HLS_ALLOC(arena, Mesh, hlsScene.meshCount);
    hlsScene.materialCount = static_cast<u32>(data->materials_count);
    hlsScene.materials = HLS_ALLOC(arena, Material, hlsScene.materialCount);
    hlsScene.textureCount = static_cast<u32>(data->textures_count);
    hlsScene.textures = HLS_ALLOC(arena, RHI::Texture, hlsScene.textureCount);

    for (u64 i = 0; i < data->textures_count; i++)
    {
        ProcessTexture(device, data, &data->textures[i], hlsScene.textures[i]);
    }

    // Process vertices
    for (u64 i = 0; i < data->meshes_count; i++)
    {
        cgltf_mesh* mesh = &data->meshes[i];
        hlsScene.meshes[i].submeshes =
            HLS_ALLOC(arena, Submesh, mesh->primitives_count);
        hlsScene.meshes[i].submeshCount =
            static_cast<u32>(mesh->primitives_count);
        for (u64 j = 0; j < mesh->primitives_count; j++)
        {
            if (!ProcessPrimitive(device, data, &mesh->primitives[j],
                                  hlsScene.meshes[i].submeshes[j]))
            {
                return false;
            }
        }
    }

    if (!ProcessIndices(device, data, hlsScene))
    {
        return false;
    }

    if (!ProcessMaterials(device, data, hlsScene))
    {
        return false;
    }

    ArenaPopToMarker(scratch, marker);
    return true;
}

bool LoadShaderFromSpv(RHI::Device& device, const char* path,
                       RHI::Shader& shader)
{
    HLS_ASSERT(path);

    Arena& scratch = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(scratch);

    String8 spvSource = Hls::LoadSpvFromFile(scratch, path);
    if (!spvSource)
    {
        return false;
    }
    if (!RHI::CreateShader(device, spvSource.Data(), spvSource.Size(), shader))
    {
        ArenaPopToMarker(scratch, marker);
        return false;
    }

    ArenaPopToMarker(scratch, marker);
    return true;
}

bool LoadTextureFromFile(RHI::Device& device, const char* path, VkFormat format,
                         RHI::Sampler::FilterMode filterMode,
                         RHI::Sampler::WrapMode wrapMode, u32 anisotropy,
                         RHI::Texture& texture)
{
    HLS_ASSERT(path);

    Hls::Image image;
    if (!Hls::LoadImageFromFile(path, image))
    {
        return false;
    }
    if (!RHI::CreateTexture(device, image.data, image.width, image.height,
                            image.channelCount, format, filterMode, wrapMode,
                            anisotropy, texture))
    {
        return false;
    }
    Hls::FreeImage(image);
    return true;
}

bool LoadSceneFromGLTF(Arena& arena, RHI::Device& device, const char* path,
                       Scene& scene)
{
    cgltf_options options{};
    cgltf_data* data = nullptr;
    if (!LoadGltfFromFile(path, options, &data))
    {
        return false;
    }

    bool res = false;
    if (data && data->scene)
    {
        res = ProcessScene(arena, device, data, data->scene, scene);
    }
    FreeGltf(data);

    return res;
}

void UnloadScene(RHI::Device& device, Scene& scene)
{
    for (u64 i = 0; i < scene.meshCount; i++)
    {
        RHI::DestroyBuffer(device, scene.materialBuffer);
        RHI::DestroyBuffer(device, scene.indexBuffer);

        for (u64 j = 0; j < scene.meshes[i].submeshCount; j++)
        {
            RHI::DestroyBuffer(device,
                               scene.meshes[i].submeshes[j].vertexBuffer);
        }

        for (u64 j = 0; j < scene.textureCount; j++)
        {
            RHI::DestroyTexture(device, scene.textures[j]);
        }
    }
}

} // namespace Hls
