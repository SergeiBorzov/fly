#include "core/assert.h"
#include "core/hash_trie.h"
#include "core/log.h"
#include "core/memory.h"
#include "core/thread_context.h"

#include "math/vec.h"

#include "import_gltf.h"

#define CGLTF_MALLOC(size) (Hls::Alloc(size))
#define CGLTF_FREE(size) (Hls::Free(size))
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

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
                                    cgltf_accessor* accessor,
                                    Geometry& geometry)
{
    HLS_ASSERT(data);
    HLS_ASSERT(accessor);

    Arena& scratch = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(scratch);

    u32* indices = HLS_ALLOC(scratch, u32, accessor->count);
    cgltf_accessor_unpack_indices(accessor, indices, sizeof(u32),
                                  accessor->count);

    if (!CreateIndexBuffer(device, indices, static_cast<u32>(accessor->count),
                           geometry.indexBuffer))
    {
        return false;
    }
    geometry.indexCount = static_cast<u32>(accessor->count);

    ArenaPopToMarker(scratch, marker);
    return true;
}

static bool ProcessPrimitiveVertices(Device& device, cgltf_data* data,
                                     cgltf_primitive* primitive,
                                     Geometry& geometry)
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
                             geometry.vertexBuffer))
    {
        return false;
    }

    ArenaPopToMarker(scratch, marker);
    return true;
}

static bool ProcessPrimitive(Device& device, cgltf_data* data,
                             cgltf_primitive* primitive, Geometry& geometry)
{
    HLS_ASSERT(data);
    HLS_ASSERT(primitive);

    if (primitive->indices)
    {
        if (!ProcessPrimitiveIndices(device, data, primitive->indices,
                                     geometry))
        {
            return false;
        }
    }

    return ProcessPrimitiveVertices(device, data, primitive, geometry);
}

static bool ProcessScene(Arena& arena, Device& device, cgltf_data* data,
                         cgltf_scene* scene, SceneData& sceneData)
{
    HLS_ASSERT(data);
    HLS_ASSERT(scene);

    if (data->meshes_count == 0)
    {
        return true;
    }

    sceneData.meshes = HLS_ALLOC(arena, Mesh, data->meshes_count);
    sceneData.meshCount = static_cast<u32>(data->meshes_count);

    for (u64 i = 0; i < data->meshes_count; i++)
    {
        cgltf_mesh* mesh = &data->meshes[i];
        if (mesh)
        {
            sceneData.meshes[i].geometries =
                HLS_ALLOC(arena, Geometry, mesh->primitives_count);
            sceneData.meshes[i].geometryCount =
                static_cast<u32>(mesh->primitives_count);
            for (u64 j = 0; j < mesh->primitives_count; j++)
            {
                if (!ProcessPrimitive(device, data, &mesh->primitives[j],
                                      sceneData.meshes[i].geometries[j]))
                {
                    return false;
                }
            }
            i++;
        }
    }

    return true;
}

bool LoadGltf(const char* path, const cgltf_options& options, cgltf_data** data)
{
    HLS_ASSERT(path);
    if (cgltf_parse_file(&options, path, data) != cgltf_result_success)
    {
        return false;
    }

    if (cgltf_load_buffers(&options, *data, path) != cgltf_result_success)
    {
        return false;
    }

    return true;
}

bool CopyGltfToDevice(Arena& arena, Device& device, cgltf_data* data,
                      SceneData& sceneData)
{
    if (data && data->scene)
    {
        if (!ProcessScene(arena, device, data, data->scene, sceneData))
        {
            return false;
        }
    }

    return true;
}

void FreeGltf(cgltf_data* data) { cgltf_free(data); }

bool LoadGltfToDevice(Arena& arena, Device& device, const char* path,
                      const cgltf_options& options, SceneData& sceneData)
{
    HLS_ASSERT(path);

    cgltf_data* data = nullptr;
    if (!LoadGltf(path, options, &data))
    {
        return false;
    }

    bool res = CopyGltfToDevice(arena, device, data, sceneData);
    FreeGltf(data);

    return res;
}

void FreeDeviceSceneData(Device& device, SceneData& sceneData)
{
    for (u64 i = 0; i < sceneData.meshCount; i++)
    {
        for (u64 j = 0; j < sceneData.meshes[i].geometryCount; j++)
        {
            Hls::DestroyStorageBuffer(
                device, sceneData.meshes[i].geometries[j].vertexBuffer);
            Hls::DestroyIndexBuffer(
                device, sceneData.meshes[i].geometries[j].indexBuffer);
        }
    }
}

} // namespace Hls
