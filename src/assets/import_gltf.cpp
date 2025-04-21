#include <xxhash.h>

#include "core/assert.h"
#include "core/log.h"
#include "core/memory.h"
#include "core/thread_context.h"

#include "import_gltf.h"

#define CGLTF_MALLOC(size) (Hls::Alloc(size))
#define CGLTF_FREE(size) (Hls::Free(size))
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#define HASH_SEED 2025

namespace Hls
{

struct BufferKey
{
    u32* bvIndices = nullptr;
    u32 count = 0;

    bool operator==(BufferKey& rhs) const
    {
        if (count != rhs.count)
        {
            return false;
        }

        for (u32 i = 0; i < count; i++)
        {
            if (bvIndices[i] != rhs.bvIndices[i])
            {
                return false;
            }
        }

        return true;
    }
};

BufferKey PushBufferKeyToArena(Arena& arena, u32 bvIndexCount)
{
    BufferKey key;
    key.bvIndices = HLS_ALLOC(arena, u32, bvIndexCount);
    key.count = bvIndexCount;
    return key;
}

u64 HashBufferKey(BufferKey key)
{
    HLS_ASSERT(key.bvIndices);
    return XXH64(key.bvIndices, sizeof(u32) * key.count, HASH_SEED);
}

struct BufferHashMap
{
    BufferHashMap* children[4];
    BufferKey key;
    Hls::Buffer value;
};

Hls::Buffer* Upsert(BufferHashMap** map, Arena* arena, BufferKey key)
{
    for (u64 h = HashBufferKey(key); h != 0; h <<= 2)
    {
        if (!*map)
        {
            if (!arena)
            {
                return nullptr;
            }

            *map = HLS_ALLOC(*arena, BufferHashMap, 1);
            (*map)->key = key;
            (*map)->value = {};
            Hls::MemZero((*map)->children, sizeof(BufferHashMap*) * 4);
            return &(*map)->value;
        }

        if (key == (*map)->key)
        {
            return &(*map)->value;
        }

        map = &(*map)->children[h >> 62];
    }

    // Full hash collision case
    // TODO: fallback to linked list, if encountered ever
    HLS_ASSERT(false);
    return nullptr;
}

static void ProcessPrimitive(Arena& arena, BufferHashMap** map,
                             cgltf_data* data, cgltf_primitive* primitive)
{
    HLS_ASSERT(data);
    HLS_ASSERT(primitive);

    if (primitive->indices)
    {
        cgltf_buffer_view* indexBufferView = primitive->indices->buffer_view;
        HLS_ASSERT(indexBufferView);

        BufferKey key = PushBufferKeyToArena(arena, 1);
        key.bvIndices[0] =
            static_cast<u32>(cgltf_buffer_view_index(data, indexBufferView));

        Hls::Buffer* buffer = Upsert(map, &arena, key);

        if (buffer->handle == VK_NULL_HANDLE)
        {
            HLS_LOG("Index buffer not filled with data");
        }
    }

    BufferKey key = PushBufferKeyToArena(
        arena, static_cast<u32>(primitive->attributes_count));
    for (u64 i = 0; i < primitive->attributes_count; i++)
    {
        u32 bvIndex = static_cast<u32>(cgltf_buffer_view_index(
            data, primitive->attributes[i].data->buffer_view));
        key.bvIndices[i] = bvIndex;
    }
    Hls::Buffer* buffer = Upsert(map, &arena, key);
    if (buffer->handle == VK_NULL_HANDLE)
    {
        HLS_LOG("Vertex buffer not filled with data");
    }
}

static void ProcessScene(cgltf_data* data, cgltf_scene* scene)
{
    HLS_ASSERT(data);
    HLS_ASSERT(scene);

    Arena& scratch = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(scratch);
    BufferHashMap* map = nullptr;

    for (u64 i = 0; i < scene->nodes_count; i++)
    {
        cgltf_mesh* mesh = scene->nodes[i]->mesh;
        if (mesh)
        {
            for (u64 i = 0; i < mesh->primitives_count; i++)
            {
                ProcessPrimitive(scratch, &map, data, &mesh->primitives[i]);
            }
        }
    }

    ArenaPopToMarker(scratch, marker);
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

bool CopyGltfToDevice(Device& device, cgltf_data* data, SceneData& sceneData)
{
    if (data && data->scene)
    {
        HLS_LOG("Buffer count: %llu", data->buffers_count);
        HLS_LOG("Buffer view count: %llu", data->buffer_views_count);
        ProcessScene(data, data->scene);
    }

    return true;
}

void FreeGltf(cgltf_data* data) { cgltf_free(data); }

bool LoadGltfToDevice(Device& device, const char* path,
                      const cgltf_options& options, SceneData& sceneData)
{
    HLS_ASSERT(path);

    cgltf_data* data = nullptr;
    if (!LoadGltf(path, options, &data))
    {
        return false;
    }

    bool res = CopyGltfToDevice(device, data, sceneData);
    FreeGltf(data);

    return res;
}

} // namespace Hls
