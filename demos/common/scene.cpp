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

static bool ProcessIndices(RHI::Device& device, cgltf_data* data, Scene& scene,
                           BoundingSphereDraw* boundingSphereDraws)
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
    u32 submeshOffset = 0;
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
                scene.directDrawData.meshes[i].submeshes[j].indexCount =
                    static_cast<u32>(primitive.indices->count);
                scene.directDrawData.meshes[i].submeshes[j].indexOffset =
                    offset;
                boundingSphereDraws[submeshOffset + j].indexCount =
                    static_cast<u32>(primitive.indices->count);
                boundingSphereDraws[submeshOffset + j].indexOffset = offset;
                offset += static_cast<u32>(primitive.indices->count);
            }
            submeshOffset += static_cast<u32>(mesh.primitives_count);
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

static bool ProcessTextures(Arena& arena, RHI::Device& device, cgltf_data* data,
                            Scene& scene)
{
    HLS_ASSERT(data);

    ArenaMarker marker = ArenaGetMarker(arena);
    scene.textureCount = static_cast<u32>(data->textures_count);
    scene.textures = HLS_ALLOC(arena, RHI::Texture, scene.textureCount);

    for (u64 i = 0; i < data->textures_count; i++)
    {
        cgltf_texture& texture = data->textures[i];

        if (!texture.image)
        {
            ArenaPopToMarker(arena, marker);
            return false;
        }

        if (!texture.image->uri)
        {
            HLS_ERROR("TODO: Load embedded textures");
            ArenaPopToMarker(arena, marker);
            return false;
        }

        if (!LoadTextureFromFile(
                device, texture.image->uri, VK_FORMAT_R8G8B8A8_SRGB,
                RHI::Sampler::FilterMode::Anisotropy8x,
                RHI::Sampler::WrapMode::Repeat, scene.textures[i]))
        {
            HLS_ERROR("Failed to load texture %s", texture.image->uri);
            ArenaPopToMarker(arena, marker);
            return false;
        }
    }
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
    PBRMaterialData* materialDataBuffer =
        HLS_ALLOC(scratch, PBRMaterialData, data->materials_count);

    for (u64 i = 0; i < data->materials_count; i++)
    {
        cgltf_material& material = data->materials[i];
        PBRMaterialData& materialData = materialDataBuffer[i];

        if (!material.has_pbr_metallic_roughness)
        {
            HLS_ERROR("TODO: Support not only mettalic roughness workflow");
            ArenaPopToMarker(scratch, marker);
            return false;
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

        if (albedoTextureView.texture)
        {
            u32 textureIndex = static_cast<u32>(
                cgltf_texture_index(data, albedoTextureView.texture));
            materialData.albedoTexture.textureHandle = textureIndex;
        }
    }

    if (!RHI::CreateStorageBuffer(device, false, materialDataBuffer,
                                  sizeof(PBRMaterialData) *
                                      data->materials_count,
                                  scene.materialBuffer))
    {
        ArenaPopToMarker(scratch, marker);
        return false;
    }

    ArenaPopToMarker(scratch, marker);
    return true;
}

static bool ProcessSubmesh(RHI::Device& device, cgltf_data* data,
                           cgltf_primitive* primitive, Submesh& submesh,
                           BoundingSphereDraw& boundingSphereDraw,
                           RHI::Buffer& vertexBuffer)
{
    HLS_ASSERT(data);
    HLS_ASSERT(primitive);

    if (!primitive->material ||
        !primitive->material->has_pbr_metallic_roughness)
    {
        HLS_ERROR("Unsupported material type");
        return false;
    }

    submesh.materialIndex =
        static_cast<u32>(cgltf_material_index(data, primitive->material));

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
                HLS_ERROR("Position has not exactly 3 components");
                return false;
            }
        }
        else if (attribute.type == cgltf_attribute_type_normal)
        {
            isNormalPresent = true;
            if (accessor->component_type != cgltf_component_type_r_32f &&
                accessor->type != cgltf_type_vec3)
            {
                HLS_ERROR("Normals has not exactly 3 components");
                return false;
            }
        }
        else if (attribute.type == cgltf_attribute_type_texcoord)
        {
            isTexCoordPresent = true;
            if (accessor->component_type != cgltf_component_type_r_32f &&
                accessor->type != cgltf_type_vec2)
            {
                HLS_ERROR("Texture coord has not exactly 2 components");
                return false;
            }
        }
    }

    if (!isPosPresent || !isNormalPresent || !isTexCoordPresent)
    {
        HLS_ERROR("Mesh misses one of position/normal/texCoords");
        return false;
    }

    if (vertexCount == 0)
    {
        return true;
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
                // flip x, since we use left handed system
                vertices[j].position.x *= -1.0f;
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
                // flip x, since we use left handed system
                vertices[j].normal.x *= -1.0f;
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

    // Simple bounding sphere through AABB
    Math::Vec3 min = vertices[0].position;
    Math::Vec3 max = vertices[0].position;
    for (u64 i = 0; i < vertexCount; i++)
    {
        min = Math::Min(min, vertices[i].position);
        max = Math::Max(max, vertices[i].position);
    }
    boundingSphereDraw.center = (min + max) * 0.5f;
    boundingSphereDraw.radius = Math::Length(max - boundingSphereDraw.center);

    if (!RHI::CreateStorageBuffer(device, false, vertices,
                                  sizeof(Vertex) * vertexCount, vertexBuffer))
    {
        ArenaPopToMarker(scratch, marker);
        return false;
    }

    ArenaPopToMarker(scratch, marker);
    return true;
}

static bool ProcessMeshes(RHI::Device& device, cgltf_data* data, Scene& scene,
                          BoundingSphereDraw* boundingSphereDraws,
                          MeshData* meshData)
{
    HLS_ASSERT(data);
    HLS_ASSERT(boundingSphereDraws);
    HLS_ASSERT(meshData);

    u32 offset = 0;
    for (u32 i = 0; i < data->meshes_count; i++)
    {
        u32 submeshCount = static_cast<u32>(data->meshes[i].primitives_count);
        scene.directDrawData.meshes[i].submeshes =
            scene.directDrawData.submeshes + offset;
        scene.directDrawData.meshes[i].submeshCount = submeshCount;

        for (u32 j = 0; j < submeshCount; j++)
        {
            if (!ProcessSubmesh(device, data, &data->meshes[i].primitives[j],
                                scene.directDrawData.submeshes[offset + j],
                                boundingSphereDraws[offset + j],
                                scene.vertexBuffers[offset + j]))
            {
                return false;
            }
            HLS_ASSERT(scene.vertexBuffers[offset + j].handle !=
                       VK_NULL_HANDLE);
            u32 vbBindlessHandle =
                scene.vertexBuffers[offset + j].bindlessHandle;
            scene.directDrawData.submeshes[offset + j].vertexBufferIndex =
                vbBindlessHandle;

            meshData[offset + j].vertexBufferIndex = vbBindlessHandle;
            meshData[offset + j].boundingSphereDrawIndex = offset + j;
            meshData[offset + j].materialIndex =
                scene.directDrawData.submeshes[offset + j].materialIndex;
        }
        offset += submeshCount;
    }
    return true;
}

static u32 MeshNodeCount(cgltf_node* node)
{
    HLS_ASSERT(node);

    u32 meshNodeCount = static_cast<bool>(node->mesh);

    for (u64 i = 0; i < node->children_count; i++)
    {
        meshNodeCount += MeshNodeCount(node->children[i]);
    }

    return meshNodeCount;
}

static u32 ProcessMeshNode(cgltf_data* data, cgltf_node* node,
                           Math::Mat4 parentModel, Scene& scene,
                           u32 meshNodeIndex)
{
    HLS_ASSERT(node);
    HLS_ASSERT(data);

    Math::Mat4 model;
    if (node->has_matrix)
    {
        model = Math::Mat4(node->matrix, 16);
    }
    else
    {
        if (node->has_scale)
        {
            model = Math::ScaleMatrix(node->scale[0], node->scale[1],
                                      node->scale[2]);
        }
        if (node->has_rotation)
        {
            // TODO: Quaternion to rotation matrix
        }
        if (node->has_translation)
        {
            model *= Math::TranslationMatrix(-node->translation[0],
                                             node->translation[1],
                                             node->translation[2]);
        }
    }
    model = parentModel * model;

    if (node->mesh)
    {
        scene.meshNodes[meshNodeIndex].model = model;
        u32 meshIndex = static_cast<u32>(cgltf_mesh_index(data, node->mesh));
        scene.meshNodes[meshNodeIndex].mesh =
            scene.directDrawData.meshes + meshIndex;
    }

    meshNodeIndex += static_cast<bool>(node->mesh);

    for (u64 i = 0; i < node->children_count; i++)
    {
        meshNodeIndex = ProcessMeshNode(data, node->children[i], model, scene,
                                        meshNodeIndex);
    }

    return meshNodeIndex;
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

    if (!ProcessTextures(arena, device, data, hlsScene))
    {
        return false;
    }

    if (!ProcessMaterials(device, data, hlsScene))
    {
        return false;
    }

    ArenaMarker marker = ArenaGetMarker(arena);
    hlsScene.directDrawData.meshes = HLS_ALLOC(arena, Mesh, data->meshes_count);
    hlsScene.directDrawData.meshCount = static_cast<u32>(data->meshes_count);

    u32 totalSubmeshCount = 0;
    for (u32 i = 0; i < data->meshes_count; i++)
    {
        u32 submeshCount = static_cast<u32>(data->meshes[i].primitives_count);
        totalSubmeshCount += static_cast<u32>(data->meshes[i].primitives_count);
    }

    if (totalSubmeshCount == 0)
    {
        return true;
    }

    hlsScene.vertexBuffers = HLS_ALLOC(arena, RHI::Buffer, totalSubmeshCount);
    hlsScene.vertexBufferCount = totalSubmeshCount;
    hlsScene.directDrawData.submeshes =
        HLS_ALLOC(arena, Submesh, totalSubmeshCount);
    hlsScene.directDrawData.submeshCount = totalSubmeshCount;

    Arena& scratch = GetScratchArena(&arena);
    ArenaMarker scratchMarker = ArenaGetMarker(scratch);

    BoundingSphereDraw* boundingSphereDraws =
        HLS_ALLOC(scratch, BoundingSphereDraw, totalSubmeshCount);
    MeshData* meshData = HLS_ALLOC(scratch, MeshData, totalSubmeshCount);

    if (!ProcessMeshes(device, data, hlsScene, boundingSphereDraws, meshData))
    {
        ArenaPopToMarker(arena, marker);
        ArenaPopToMarker(scratch, scratchMarker);
        return false;
    }

    if (!ProcessIndices(device, data, hlsScene, boundingSphereDraws))
    {
        ArenaPopToMarker(arena, marker);
        ArenaPopToMarker(scratch, scratchMarker);
        return false;
    }

    if (!RHI::CreateStorageBuffer(
            device, false, boundingSphereDraws,
            sizeof(BoundingSphereDraw) * totalSubmeshCount,
            hlsScene.indirectDrawData.boundingSphereDrawBuffer))
    {
        ArenaPopToMarker(arena, marker);
        ArenaPopToMarker(scratch, scratchMarker);
        return false;
    }

    if (!RHI::CreateStorageBuffer(device, false, meshData,
                                  sizeof(MeshData) * totalSubmeshCount,
                                  hlsScene.indirectDrawData.meshDataBuffer))
    {
        ArenaPopToMarker(arena, marker);
        ArenaPopToMarker(scratch, scratchMarker);
        return false;
    }

    u32 meshNodeCount = 0;
    for (u32 i = 0; i < scene->nodes_count; i++)
    {
        meshNodeCount += MeshNodeCount(scene->nodes[i]);
    }
    hlsScene.meshNodes = HLS_ALLOC(arena, MeshNode, meshNodeCount);
    hlsScene.meshNodeCount = meshNodeCount;

    u32 meshNodeIndex = 0;
    for (u32 i = 0; i < scene->nodes_count; i++)
    {
        meshNodeIndex = ProcessMeshNode(data, scene->nodes[i], Math::Mat4(),
                                        hlsScene, meshNodeIndex);
    }

    u32 instanceDataCount = 0;
    for (u32 i = 0; i < hlsScene.meshNodeCount; i++)
    {
        instanceDataCount += hlsScene.meshNodes[i].mesh->submeshCount;
    }

    InstanceData* instanceData =
        HLS_ALLOC(scratch, InstanceData, instanceDataCount);
    u32 index = 0;
    for (u32 i = 0; i < hlsScene.meshNodeCount; i++)
    {
        const MeshNode& meshNode = hlsScene.meshNodes[i];
        for (u32 j = 0; j < meshNode.mesh->submeshCount; j++)
        {
            instanceData[index].model = meshNode.model;
            instanceData[index].meshDataIndex = static_cast<u32>(
                (hlsScene.directDrawData.submeshes - meshNode.mesh->submeshes) +
                j);
        }
    }

    if (!RHI::CreateStorageBuffer(device, false, instanceData,
                                  sizeof(InstanceData) * instanceDataCount,
                                  hlsScene.indirectDrawData.instanceDataBuffer))
    {
        ArenaPopToMarker(arena, marker);
        ArenaPopToMarker(scratch, scratchMarker);
        return false;
    }

    ArenaPopToMarker(scratch, scratchMarker);
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
    RHI::DestroyBuffer(device, scene.indirectDrawData.instanceDataBuffer);
    RHI::DestroyBuffer(device, scene.indirectDrawData.boundingSphereDrawBuffer);
    RHI::DestroyBuffer(device, scene.indirectDrawData.meshDataBuffer);

    RHI::DestroyBuffer(device, scene.indexBuffer);
    RHI::DestroyBuffer(device, scene.materialBuffer);

    for (u64 i = 0; i < scene.vertexBufferCount; i++)
    {
        RHI::DestroyBuffer(device, scene.vertexBuffers[i]);
    }

    for (u64 i = 0; i < scene.textureCount; i++)
    {
        RHI::DestroyTexture(device, scene.textures[i]);
    }
}

bool LoadTextureFromFile(RHI::Device& device, const char* path, VkFormat format,
                         RHI::Sampler::FilterMode filterMode,
                         RHI::Sampler::WrapMode wrapMode, RHI::Texture& texture)
{
    HLS_ASSERT(path);

    Hls::Image image;
    if (!Hls::LoadImageFromFile(path, image))
    {
        return false;
    }
    if (!RHI::CreateTexture(device, image.data, image.width, image.height,
                            image.channelCount, format, filterMode, wrapMode,
                            texture))
    {
        return false;
    }
    Hls::FreeImage(image);
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

} // namespace Hls
