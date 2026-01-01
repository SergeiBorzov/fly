#include <cgltf.h>

#include "core/assert.h"
#include "core/filesystem.h"
#include "core/memory.h"
#include "core/thread_context.h"

#include "math/mat.h"

#include "assets/image/image.h"
#include "assets/image/import_image.h"
#include "assets/image/transform_image.h"

#include "assets/geometry/geometry.h"
#include "assets/geometry/vertex_layout.h"

#include "export_scene.h"
#include "scene_data.h"
#include "scene_serialization_types.h"

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
                           const SceneExportOptions& options,
                           SceneData& sceneData)
{
    if (data->textures_count == 0 || !options.exportMaterials)
    {
        return true;
    }

    sceneData.imageCount = data->textures_count;
    sceneData.images =
        static_cast<Image*>(Alloc(sizeof(Image) * data->textures_count));

    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    for (u64 i = 0; i < data->textures_count; i++)
    {
        bool isLinear = false;
        cgltf_texture& texture = data->textures[i];
        if (!texture.image)
        {
            continue;
        }

        ImageStorageType storageType = ImageStorageType::BC1;
        u32 desiredChannelCount = 4;
        for (cgltf_size i = 0; i < data->materials_count; i++)
        {
            cgltf_material* mat = &data->materials[i];

            if (mat->pbr_metallic_roughness.base_color_texture.texture ==
                &texture)
            {
                desiredChannelCount = 4;
                storageType = (mat->alpha_mode == cgltf_alpha_mode_opaque)
                                  ? ImageStorageType::BC1
                                  : ImageStorageType::BC3;
                break;
            }

            // TODO:
            // Combine Roughness, metallic, occlusion into single texture Use
            // BC1 Put roughness in green channel (highest precision)

            // TODO:
            // Emissive texture

            if (mat->normal_texture.texture == &texture)
            {
                desiredChannelCount = 2;
                storageType = ImageStorageType::BC5;
                isLinear = true;
                break;
            }
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
            if (!LoadImageFromFile(relativeImagePath, sceneData.images[i],
                                   desiredChannelCount))
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
        ArenaPopToMarker(arena, marker);

        GenerateMips(sceneData.images[i], isLinear);
        CompressImage(storageType, sceneData.images[i]);
    }

    ArenaPopToMarker(arena, marker);

    return true;
}

static void CookMaterialsGltf(const cgltf_data* data,
                              const SceneExportOptions& options,
                              SceneData& sceneData)
{
    if (!data->materials_count || !options.exportMaterials)
    {
        sceneData.materials = nullptr;
        sceneData.materialCount = 0;
        for (u32 i = 0; i < sceneData.geometryCount; i++)
        {
            for (u32 j = 0; j < sceneData.geometries[i].subgeometryCount; j++)
            {
                sceneData.geometries[i].subgeometries[j].materialIndex = -1;
            }
        }
        return;
    }

    u64 materialCount = 0;
    sceneData.materials = static_cast<SerializedPBRMaterial*>(
        Alloc(sizeof(SerializedPBRMaterial) * data->materials_count));

    for (u32 i = 0; i < data->materials_count; i++)
    {
        const cgltf_material& material = data->materials[i];
        SerializedPBRMaterial& serializedMaterial = sceneData.materials[i];
        serializedMaterial = {};

        if (!material.has_pbr_metallic_roughness)
        {
            continue;
        }

        const cgltf_pbr_metallic_roughness& pbr =
            material.pbr_metallic_roughness;
        memcpy(serializedMaterial.baseColor.data, pbr.base_color_factor,
               sizeof(Math::Vec4));
        serializedMaterial.metallic = pbr.metallic_factor;
        serializedMaterial.roughness = pbr.roughness_factor;
        if (pbr.base_color_texture.texture)
        {
            serializedMaterial.baseColorTextureIndex = static_cast<i32>(
                pbr.base_color_texture.texture - data->textures);
        }

        if (material.normal_texture.texture)
        {
            serializedMaterial.normalTextureIndex = static_cast<i32>(
                material.normal_texture.texture - data->textures);
        }

        materialCount++;
    }

    sceneData.materialCount = materialCount;
}

static void TransformSerializedNode(f32 scale, CoordSystem coordSystem,
                                    bool flipForward, SerializedSceneNode& node)
{
    switch (coordSystem)
    {
        case CoordSystem::XZY:
        {
            node.localPosition =
                Math::Vec3(node.localPosition.x, node.localPosition.z,
                           node.localPosition.y);
            node.localRotation =
                Math::Quat(node.localRotation.x, node.localRotation.z,
                           node.localRotation.y, node.localRotation.w);
            node.localScale = Math::Vec3(node.localScale.x, node.localScale.z,
                                         node.localScale.y);
            break;
        }
        case CoordSystem::YXZ:
        {
            node.localPosition =
                Math::Vec3(node.localPosition.y, node.localPosition.x,
                           node.localPosition.z);
            node.localRotation =
                Math::Quat(node.localRotation.y, node.localRotation.x,
                           node.localRotation.z, node.localRotation.w);
            node.localScale = Math::Vec3(node.localScale.y, node.localScale.x,
                                         node.localScale.z);
            break;
        }
        case CoordSystem::YZX:
        {
            node.localPosition =
                Math::Vec3(node.localPosition.y, node.localPosition.z,
                           node.localPosition.x);
            node.localRotation =
                Math::Quat(node.localRotation.y, node.localRotation.z,
                           node.localRotation.x, node.localRotation.w);
            node.localScale = Math::Vec3(node.localScale.y, node.localScale.z,
                                         node.localScale.x);
            break;
        }
        case CoordSystem::ZXY:
        {
            node.localPosition =
                Math::Vec3(node.localPosition.z, node.localPosition.x,
                           node.localPosition.y);
            node.localRotation =
                Math::Quat(node.localRotation.z, node.localRotation.x,
                           node.localRotation.y, node.localRotation.w);
            node.localScale = Math::Vec3(node.localScale.z, node.localScale.x,
                                         node.localScale.y);
            break;
        }
        case CoordSystem::ZYX:
        {
            node.localPosition =
                Math::Vec3(node.localPosition.z, node.localPosition.y,
                           node.localPosition.x);
            node.localRotation =
                Math::Quat(node.localRotation.z, node.localRotation.y,
                           node.localRotation.x, node.localRotation.w);
            node.localScale = Math::Vec3(node.localScale.z, node.localScale.y,
                                         node.localScale.x);
            break;
        }
        default:
        {
            break;
        }
    }

    node.localPosition *= scale;
    if (flipForward)
    {
        node.localPosition.z *= -1.0f;
        node.localRotation.z *= -1.0f;
    }
}

static void CookNodesGltf(const cgltf_data* data,
                          const SceneExportOptions& options,
                          SceneData& sceneData)
{
    if (!data->nodes_count || !options.exportNodes)
    {
        sceneData.nodes = nullptr;
        sceneData.nodeCount = 0;
        return;
    }

    DynamicArray<NodeTraverseData> stack;
    DynamicArray<SerializedSceneNode> sceneNodes;

    for (u32 i = 0; i < data->nodes_count; i++)
    {
        stack.Add({&data->nodes[i], -1});
    }

    while (stack.Count() != 0)
    {
        SerializedSceneNode sceneNode{};

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
            sceneNode.meshIndex = static_cast<i64>(node->mesh - data->meshes);
        }

        i64 nodeIndex = sceneNodes.Count();
        sceneNodes.Add(sceneNode);

        for (u32 i = 0; i < node->children_count; i++)
        {
            stack.Add({node->children[i], nodeIndex});
        }
    }

    sceneData.nodeCount = sceneNodes.Count();
    sceneData.nodes = static_cast<SerializedSceneNode*>(
        Alloc(sizeof(SerializedSceneNode) * sceneNodes.Count()));
    memcpy(sceneData.nodes, sceneNodes.Data(),
           sizeof(SerializedSceneNode) * sceneNodes.Count());

    if (options.scale != 1.0f || options.coordSystem != CoordSystem::XYZ ||
        options.flipForward)
    {
        for (u32 i = 0; i < sceneData.nodeCount; i++)
        {
            TransformSerializedNode(options.scale, options.coordSystem,
                                    options.flipForward, sceneData.nodes[i]);
        }
    }
}

static bool CookSceneGltf(String8 path, const cgltf_data* data,
                          const SceneExportOptions& options,
                          SceneData& sceneData)
{
    FLY_ASSERT(path);
    FLY_ASSERT(data);

    if (!CookImagesGltf(path, data, options, sceneData))
    {
        return false;
    }

    if (!ImportGeometriesGltf(data, &sceneData.geometries,
                              sceneData.geometryCount))
    {
        return false;
    }

    for (u32 i = 0; i < sceneData.geometryCount; i++)
    {
        if (options.scale != 1.0f || options.coordSystem != CoordSystem::XYZ ||
            options.flipForward)
        {
            TransformGeometry(options.scale, options.coordSystem,
                              options.flipForward, sceneData.geometries[i]);
        }
        if (options.flipWindingOrder)
        {
            FlipGeometryWindingOrder(sceneData.geometries[i]);
        }
        CookGeometry(sceneData.geometries[i]);
    }

    CookNodesGltf(data, options, sceneData);
    CookMaterialsGltf(data, options, sceneData);

    return true;
}

bool CookScene(String8 path, const SceneExportOptions& cookOptions,
               SceneData& sceneData)
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

        bool res = CookSceneGltf(path, data, cookOptions, sceneData);
        cgltf_free(data);
        return res;
    }
    return false;
}

static void SerializeNodes(const SceneData& sceneData,
                           SerializedSceneNode* sceneNodeStart)
{
    if (sceneData.nodes && sceneData.nodeCount)
    {
        memcpy(sceneNodeStart, sceneData.nodes,
               sizeof(SerializedSceneNode) * sceneData.nodeCount);
    }
}

static void SerializeImages(const SceneData& sceneData,
                            ImageHeader* imageHeaderStart, u8* imageDataStart)
{
    u64 imageOffset = 0;
    for (u32 i = 0; i < sceneData.imageCount; i++)
    {
        const Image& image = sceneData.images[i];

        ImageHeader& header = *(imageHeaderStart + i);
        header.size = GetImageSize(image);
        header.offset = imageOffset;
        header.width = image.width;
        header.height = image.height;
        header.channelCount = image.channelCount;
        header.layerCount = image.layerCount;
        header.mipCount = image.mipCount;
        header.storageType = image.storageType;

        memcpy(imageDataStart + imageOffset, image.data, header.size);
        imageOffset += header.size;
    }
}

static void SerializeMaterials(const SceneData& sceneData,
                               SerializedPBRMaterial* pbrMaterialStart)
{
    if (sceneData.materials && sceneData.materialCount)
    {
        memcpy(pbrMaterialStart, sceneData.materials,
               sizeof(SerializedPBRMaterial) * sceneData.materialCount);
    }
}

static void SerializeMeshes(const SceneData& sceneData,
                            MeshHeader* meshHeaderStart, LOD* lodStart,
                            i32* submeshMaterialIndexStart,
                            QVertex* vertexStart, u32* indexStart)
{
    u64 firstVertex = 0;
    u64 firstIndex = 0;
    u32 firstLod = 0;

    u32 sgOffset = 0;
    for (u32 i = 0; i < sceneData.geometryCount; i++)
    {
        const Geometry& geometry = sceneData.geometries[i];

        MeshHeader& meshHeader = *(meshHeaderStart + i);
        meshHeader.sphereCenter = geometry.sphereCenter;
        meshHeader.submeshCount = geometry.subgeometryCount;
        meshHeader.vertexCount = geometry.vertexCount;
        meshHeader.indexCount = geometry.indexCount;
        meshHeader.lodCount = geometry.lodCount;
        meshHeader.sphereRadius = geometry.sphereRadius;
        meshHeader.firstLod = firstLod;
        meshHeader.firstVertex = firstVertex;
        meshHeader.firstIndex = firstIndex;

        LOD* lodData = lodStart + firstLod;
        QVertex* vertexData = vertexStart + firstVertex;
        u32* indexData = indexStart + firstIndex;

        for (u32 j = 0; j < geometry.subgeometryCount; j++)
        {
            const Subgeometry& sg = geometry.subgeometries[j];
            memcpy(lodData + j * geometry.lodCount, sg.lods,
                   sizeof(LOD) * geometry.lodCount);
            *(submeshMaterialIndexStart + sgOffset) = sg.materialIndex;
            sgOffset++;

            for (u32 k = 0; k < geometry.lodCount; k++)
            {
                LOD* lod = lodData + j * geometry.lodCount + k;
                lod->firstIndex += firstIndex;
            }
        }

        memcpy(vertexData, geometry.qvertices,
               sizeof(QVertex) * geometry.vertexCount);
        memcpy(indexData, geometry.indices, sizeof(u32) * geometry.indexCount);

        firstLod += geometry.subgeometryCount * geometry.lodCount;
        firstVertex += geometry.vertexCount;
        firstIndex += geometry.indexCount;
    }
}

bool ExportScene(String8 path, const SceneData& sceneData)
{
    u64 totalIndexCount = 0;
    u64 totalVertexCount = 0;
    u64 totalSubmeshCount = 0;
    u32 totalLodCount = 0;

    for (u32 i = 0; i < sceneData.geometryCount; i++)
    {
        const Geometry& geometry = sceneData.geometries[i];
        totalVertexCount += geometry.vertexCount;
        totalIndexCount += geometry.indexCount;
        totalSubmeshCount += geometry.subgeometryCount;
        totalLodCount += geometry.lodCount * geometry.subgeometryCount;
    }

    u64 totalImageSize = 0;
    for (u32 i = 0; i < sceneData.imageCount; i++)
    {
        totalImageSize += GetImageSize(sceneData.images[i]);
    }

    u64 totalSize =
        sizeof(SceneFileHeader) + sizeof(ImageHeader) * sceneData.imageCount +
        sizeof(MeshHeader) * sceneData.geometryCount +
        sizeof(SerializedSceneNode) * sceneData.nodeCount +
        sizeof(LOD) * totalLodCount + sizeof(QVertex) * totalVertexCount +
        sizeof(u32) * totalIndexCount + totalImageSize;

    u64 offset = 0;
    u8* data = static_cast<u8*>(Alloc(totalSize));
    SceneFileHeader* sceneHeader = reinterpret_cast<SceneFileHeader*>(data);
    offset += sizeof(SceneFileHeader);

    ImageHeader* imageHeaderStart =
        reinterpret_cast<ImageHeader*>(data + offset);
    offset += sizeof(ImageHeader) * sceneData.imageCount;

    SerializedPBRMaterial* pbrMaterialStart =
        reinterpret_cast<SerializedPBRMaterial*>(data + offset);
    offset += sizeof(SerializedPBRMaterial) * sceneData.materialCount;

    MeshHeader* meshHeaderStart = reinterpret_cast<MeshHeader*>(data + offset);
    offset += sizeof(MeshHeader) * sceneData.geometryCount;

    SerializedSceneNode* sceneNodeStart =
        reinterpret_cast<SerializedSceneNode*>(data + offset);
    offset += sizeof(SerializedSceneNode) * sceneData.nodeCount;

    LOD* lodStart = reinterpret_cast<LOD*>(data + offset);
    offset += sizeof(LOD) * totalLodCount;

    i32* submeshMaterialIndexStart = reinterpret_cast<i32*>(data + offset);
    offset += sizeof(i32) * totalSubmeshCount;

    QVertex* vertexStart = reinterpret_cast<QVertex*>(data + offset);
    offset += sizeof(QVertex) * totalVertexCount;

    u32* indexStart = reinterpret_cast<u32*>(data + offset);
    offset += sizeof(u32) * totalIndexCount;

    u8* imageDataStart = reinterpret_cast<u8*>(data + offset);
    offset += sizeof(u8) * totalImageSize;

    sceneHeader->version = {1, 0, 0};
    sceneHeader->totalVertexCount = totalVertexCount;
    sceneHeader->totalIndexCount = totalIndexCount;
    sceneHeader->totalSubmeshCount = totalSubmeshCount;
    sceneHeader->totalLodCount = totalLodCount;
    sceneHeader->textureCount = sceneData.imageCount;
    sceneHeader->meshCount = sceneData.geometryCount;
    sceneHeader->nodeCount = sceneData.nodeCount;
    sceneHeader->materialCount = sceneData.materialCount;

    SerializeImages(sceneData, imageHeaderStart, imageDataStart);
    SerializeMaterials(sceneData, pbrMaterialStart);
    SerializeNodes(sceneData, sceneNodeStart);
    SerializeMeshes(sceneData, meshHeaderStart, lodStart,
                    submeshMaterialIndexStart, vertexStart, indexStart);

    String8 str(reinterpret_cast<char*>(data), totalSize);
    bool res = WriteStringToFile(str, path);
    Free(data);

    return res;
}

} // namespace Fly
