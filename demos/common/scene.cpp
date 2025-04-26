#include "core/assert.h"
#include "core/log.h"
#include "core/thread_context.h"

#include "assets/import_gltf.h"
#include "assets/import_image.h"

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

static bool ProcessPrimitiveIndices(Device& device, cgltf_data* data,
                                    cgltf_accessor* accessor, Submesh& submesh)
{
    HLS_ASSERT(data);
    HLS_ASSERT(accessor);

    Arena& scratch = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(scratch);

    u32* indices = HLS_ALLOC(scratch, u32, accessor->count);
    cgltf_accessor_unpack_indices(accessor, indices, sizeof(u32),
                                  accessor->count);

    if (!CreateIndexBuffer(device, indices, static_cast<u32>(accessor->count),
                           submesh.indexBuffer))
    {
        return false;
    }
    submesh.indexCount = static_cast<u32>(accessor->count);

    ArenaPopToMarker(scratch, marker);
    return true;
}

static bool ProcessPrimitiveVertices(Device& device, cgltf_data* data,
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

    if (!CreateStorageBuffer(device, vertices, sizeof(Vertex) * vertexCount,
                             submesh.vertexBuffer))
    {
        return false;
    }

    ArenaPopToMarker(scratch, marker);
    return true;
}

static bool ProcessPrimitiveMaterial(Device& device, cgltf_data* data,
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

static bool ProcessPrimitive(Device& device, cgltf_data* data,
                             cgltf_primitive* primitive, Submesh& submesh)
{
    HLS_ASSERT(data);
    HLS_ASSERT(primitive);

    if (primitive->indices)
    {
        if (!ProcessPrimitiveIndices(device, data, primitive->indices, submesh))
        {
            return false;
        }
    }

    if (!ProcessPrimitiveVertices(device, data, primitive, submesh))
    {
        return false;
    }

    ProcessPrimitiveMaterial(device, data, primitive->material, submesh);
    return true;
}

static bool ProcessMaterial(Device& device, cgltf_data* data,
                            cgltf_material* material, Scene& scene,
                            Material& hlsMaterial)
{
    HLS_ASSERT(data);
    HLS_ASSERT(material);

    if (!material->has_pbr_metallic_roughness)
    {
        return false;
    }

    cgltf_texture_view albedoTextureView =
        material->pbr_metallic_roughness.base_color_texture;
    if (albedoTextureView.has_transform)
    {
        hlsMaterial.albedoTexture.offset.x =
            albedoTextureView.transform.offset[0];
        hlsMaterial.albedoTexture.offset.y =
            albedoTextureView.transform.offset[1];
        hlsMaterial.albedoTexture.scale.x =
            albedoTextureView.transform.scale[0];
        hlsMaterial.albedoTexture.scale.y =
            albedoTextureView.transform.scale[1];
    }

    u32 textureIndex =
        static_cast<u32>(cgltf_texture_index(data, albedoTextureView.texture));
    hlsMaterial.albedoTexture.texture = scene.textures[textureIndex];

    return true;
}

static bool ProcessTexture(Device& device, cgltf_data* data,
                           cgltf_texture* texture, Hls::Texture& hlsTexture)
{
    HLS_ASSERT(data);
    HLS_ASSERT(texture);

    if (!texture->image || !texture->image->uri)
    {
        return false;
    }

    Hls::Image image;
    if (!Hls::ImportImageFromFile(texture->image->uri, image))
    {
        HLS_ERROR("Failed to load image %s", texture->image->uri);
        return false;
    }

    if (!Hls::CreateTexture(device, image.width, image.height,
                            VK_FORMAT_R8G8B8A8_SRGB, hlsTexture))
    {
        HLS_ERROR("Failed to create texture %s", texture->image->uri);
        return false;
    }
    if (!Hls::TransferImageDataToTexture(device, image, hlsTexture))
    {
        HLS_ERROR("Failed to transfer %s image data to texture",
                  texture->image->uri);
        return false;
    }
    Hls::FreeImage(image);

    return true;
}

static bool ProcessScene(Arena& arena, Device& device, cgltf_data* data,
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
    hlsScene.textures = HLS_ALLOC(arena, Texture, hlsScene.textureCount);

    for (u64 i = 0; i < data->textures_count; i++)
    {
        ProcessTexture(device, data, &data->textures[i], hlsScene.textures[i]);
    }

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

    for (u64 i = 0; i < data->materials_count; i++)
    {
        cgltf_material* material = &data->materials[i];
        ProcessMaterial(device, data, material, hlsScene,
                        hlsScene.materials[i]);
    }

    ArenaPopToMarker(scratch, marker);
    return true;
}

bool LoadSceneFromGLTF(Arena& arena, Device& device, const char* path,
                       Scene& scene)
{
    cgltf_options options{};
    cgltf_data* data = nullptr;
    if (!LoadGltf(path, options, &data))
    {
        return false;
    }

    bool res = false;
    if (data && data->scene)
    {
        res = ProcessScene(arena, device, data, data->scene, scene);
    }
    FreeGltf(data);

    return true;
}

void UnloadScene(Device& device, Scene& scene)
{
    for (u64 i = 0; i < scene.meshCount; i++)
    {
        for (u64 j = 0; j < scene.meshes[i].submeshCount; j++)
        {
            Hls::DestroyStorageBuffer(
                device, scene.meshes[i].submeshes[j].vertexBuffer);
            Hls::DestroyIndexBuffer(device,
                                    scene.meshes[i].submeshes[j].indexBuffer);
        }

        for (u64 j = 0; j < scene.textureCount; j++)
        {
            Hls::DestroyTexture(device, scene.textures[j]);
        }
    }
}

} // namespace Hls
