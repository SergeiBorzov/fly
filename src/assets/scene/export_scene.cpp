#include <cgltf.h>

#include "core/assert.h"
#include "core/filesystem.h"
#include "core/memory.h"
#include "core/thread_context.h"

#include "assets/image/compress_image.h"
#include "assets/image/image.h"
#include "assets/image/import_image.h"
#include "assets/image/transform_image.h"

#include "assets/geometry/geometry.h"

#include "export_scene.h"
#include "scene_storage.h"

#include <limits.h>
#include <unistd.h>

namespace Fly
{

struct NodeTraverseData
{
    const cgltf_node* node = nullptr;
    i64 parentIndex = -1;
};

template <typename T>
struct DynamicArray
{
public:
    inline u64 Count() const { return count; }
    inline u64 Capacity() const { return capacity; }

    DynamicArray()
    {
        count = 0;
        capacity = 0;
        data = nullptr;
    }

    void Add(const T& value)
    {
        if (count + 1 > capacity)
        {
            if (capacity == 0)
            {
                capacity = 1;
            }
            else
            {
                capacity *= 2;
            }
            data = static_cast<T*>(Realloc(data, sizeof(T) * capacity));
        }
        data[count++] = value;
    }

    void Clear() { count = 0; }

    T* Last()
    {
        if (!data || count == 0)
        {
            return nullptr;
        }
        return &data[count - 1];
    }

    const T* Last() const
    {
        if (!data || count == 0)
        {
            return nullptr;
        }
        return &data[count - 1];
    }

    T* Pop()
    {
        if (!data || count == 0)
        {
            return nullptr;
        }
        return &data[--count];
    }

    inline T* Data() { return data; }
    inline const T* Data() const { return data; }
    inline T& operator[](u64 index) { return data[index]; }
    inline const T& operator[](u64 index) const { return data[index]; }

    ~DynamicArray()
    {
        if (data)
        {
            Free(data);
            count = 0;
            capacity = 0;
            data = nullptr;
        }
    }

private:
    u64 count;
    u64 capacity;
    T* data;
};

static bool CookImagesGltf(String8 path, const cgltf_data* data,
                           SceneStorage& sceneStorage)
{
    if (data->textures_count == 0)
    {
        return true;
    }

    sceneStorage.imageCount = data->textures_count;
    sceneStorage.images =
        static_cast<Image*>(Alloc(sizeof(Image) * data->textures_count));

    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    for (u64 i = 0; i < data->textures_count; i++)
    {
        ArenaMarker loopMarker = ArenaGetMarker(arena);
        cgltf_texture& texture = data->textures[i];
        if (!texture.image)
        {
            continue;
        }

        if (texture.image->uri)
        {
            String8 gltfDirPath = ParentDirectory(path);
            String8 imagePath =
                String8(texture.image->uri, strlen(texture.image->uri));

            u64 bufferSize = gltfDirPath.Size() + imagePath.Size() + 1;
            char* buffer = FLY_PUSH_ARENA(
                arena, char, gltfDirPath.Size() + imagePath.Size() + 1);
            MemZero(buffer, bufferSize);
            memcpy(buffer, gltfDirPath.Data(), gltfDirPath.Size());
            memcpy(buffer + gltfDirPath.Size(), imagePath.Data(),
                   imagePath.Size());

            String8 relativeImagePath = String8(buffer, bufferSize - 1);
            if (!LoadImageFromFile(relativeImagePath, sceneStorage.images[i]))
            {
                return false;
            };
        }
        else
        {
            FLY_ENSURE(false, "Not supported, image mime type is %s",
                       texture.image->mime_type);
            return false;
        }
        ArenaPopToMarker(arena, loopMarker);

        // Generate mips
        {
            Image transformedImage;
            GenerateMips(sceneStorage.images[i], transformedImage);
            FreeImage(sceneStorage.images[i]);
            sceneStorage.images[i] = transformedImage;
        }

        Fly::CodecType codecType = CodecType::Invalid;
        for (cgltf_size i = 0; i < data->materials_count; i++)
        {
            cgltf_material* mat = &data->materials[i];

            if (mat->pbr_metallic_roughness.base_color_texture.texture ==
                &texture)
            {
                codecType = CodecType::BC1;
                break;
            }

            // Metallic-Roughness
            if (mat->pbr_metallic_roughness.metallic_roughness_texture
                        .texture == &texture &&
                mat->occlusion_texture.texture != &texture)
            {
                codecType = CodecType::BC5;
                break;
            }

            if (mat->pbr_metallic_roughness.metallic_roughness_texture
                        .texture != &texture &&
                mat->occlusion_texture.texture == &texture)
            {
                codecType = CodecType::BC4;
                break;
            }

            if (mat->pbr_metallic_roughness.metallic_roughness_texture
                        .texture != &texture &&
                mat->occlusion_texture.texture == &texture)
            {
                codecType = CodecType::BC1;
                break;
            }

            if (mat->normal_texture.texture == &texture)
            {
                codecType = CodecType::BC5;
                break;
            }

            if (mat->emissive_texture.texture == &texture)
            {
                codecType = CodecType::BC1;
                break;
            }
        }

        // TODO: Store compressed image in scene storage
        u64 size = 0;
        u8* data = CompressImage(sceneStorage.images[i], codecType, size);
        Free(data);
    }

    ArenaPopToMarker(arena, marker);

    return true;
}

static bool CookSceneGltf(String8 path, const cgltf_data* data,
                          SceneStorage& sceneStorage)
{
    FLY_ASSERT(path);
    FLY_ASSERT(data);

    if (!CookImagesGltf(path, data, sceneStorage))
    {
        return false;
    }

    if (!ImportGeometriesGltf(data, &sceneStorage.geometries,
                              sceneStorage.geometryCount))
    {
        return false;
    }

    for (u32 i = 0; i < sceneStorage.geometryCount; i++)
    {
        CookGeometry(sceneStorage.geometries[i]);
    }

    DynamicArray<NodeTraverseData> stack;
    DynamicArray<SceneNode> sceneNodes;

    for (u32 i = 0; i < data->nodes_count; i++)
    {
        stack.Add({&data->nodes[i], -1});
    }

    while (stack.Count() != 0)
    {
        SceneNode sceneNode{};

        const NodeTraverseData* pTraverseData = stack.Pop();
        const cgltf_node* node = pTraverseData->node;
        sceneNode.parentIndex = pTraverseData->parentIndex;

        if (node->has_matrix)
        {
            Math::Mat4 localMatrix = Math::Mat4(node->matrix, 16);
            Math::Vec3 x = Math::Vec3(localMatrix[0]);
            Math::Vec3 y = Math::Vec3(localMatrix[1]);
            Math::Vec3 z = Math::Vec3(localMatrix[2]);

            sceneNode.localScale =
                Math::Vec3(Math::Length(x), Math::Length(y), Math::Length(z));

            Math::Mat4 rotMat(1.0f);
            rotMat[0] = Math::Vec4(x / sceneNode.localScale.x, 0.0f);
            rotMat[1] = Math::Vec4(y / sceneNode.localScale.y, 0.0f);
            rotMat[2] = Math::Vec4(z / sceneNode.localScale.z, 0.0f);
            sceneNode.localRotation = Math::Quat(rotMat);

            sceneNode.localPosition = Math::Vec3(localMatrix[3]);
        }
        else
        {
            if (node->has_scale)
            {
                memcpy(sceneNode.localScale.data, node->scale, sizeof(f32) * 3);
            }
            if (node->has_rotation)
            {
                memcpy(sceneNode.localRotation.data, node->rotation,
                       sizeof(f32) * 4);
            }
            if (node->has_translation)
            {
                memcpy(sceneNode.localPosition.data, node->translation,
                       sizeof(f32) * 3);
            }
        }

        if (node->mesh)
        {
            sceneNode.geometryIndex =
                static_cast<i64>(node->mesh - data->meshes);
        }

        i64 nodeIndex = sceneNodes.Count();
        sceneNodes.Add(sceneNode);

        for (u32 i = 0; i < node->children_count; i++)
        {
            stack.Add({node->children[i], nodeIndex});
        }
    }

    sceneStorage.nodeCount = sceneNodes.Count();
    sceneStorage.nodes =
        static_cast<SceneNode*>(Alloc(sizeof(SceneNode) * sceneNodes.Count()));
    memcpy(sceneStorage.nodes, sceneNodes.Data(),
           sizeof(SceneNode) * sceneNodes.Count());

    return true;
}

bool CookScene(String8 path, SceneStorage& sceneStorage)
{

    if (path.EndsWith(FLY_STRING8_LITERAL(".gltf")) ||
        path.EndsWith(FLY_STRING8_LITERAL(".GLTF")) ||
        path.EndsWith(FLY_STRING8_LITERAL(".glb")) ||
        path.EndsWith(FLY_STRING8_LITERAL(".GLB")))
    {
        const cgltf_options options{};
        cgltf_data* data = nullptr;

        if (cgltf_parse_file(&options, path.Data(), &data) !=
            cgltf_result_success)
        {
            return false;
        }

        if (cgltf_load_buffers(&options, data, path.Data()) !=
            cgltf_result_success)
        {
            cgltf_free(data);
            return false;
        }

        bool res = CookSceneGltf(path, data, sceneStorage);
        cgltf_free(data);
        return res;
    }
    return false;
}

bool ExportScene(String8 path, SceneStorage& sceneStorage)
{
    String8 dummy = FLY_STRING8_LITERAL("dummy");
    if (!WriteStringToFile(dummy, path))
    {
        return false;
    }
    return true;
}

} // namespace Fly
