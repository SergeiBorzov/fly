#include "core/filesystem.h"
#include "core/memory.h"
#include "core/string8.h"
#include "core/thread_context.h"

#include "rhi/device.h"
#include "rhi/texture.h"

#include "assets/image/image.h"

#include "scene.h"

namespace Fly
{

static VkFormat CompressedStorageToVkFormat(ImageStorageType storageType)
{
    switch (storageType)
    {
        case ImageStorageType::BC1:
        {
            return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
        }
        case ImageStorageType::BC3:
        {
            return VK_FORMAT_BC3_SRGB_BLOCK;
        }
        case ImageStorageType::BC4:
        {
            return VK_FORMAT_BC4_UNORM_BLOCK;
        }
        case ImageStorageType::BC5:
        {
            return VK_FORMAT_BC5_UNORM_BLOCK;
        }
        default:
        {
            break;
        }
    }
    return VK_FORMAT_UNDEFINED;
}

static bool ImportTextures(RHI::Device& device,
                           const SceneFileHeader* fileHeader,
                           const ImageHeader* imageHeaderStart,
                           const u8* imageDataStart, Scene& scene)
{
    if (!fileHeader->textureCount)
    {
        return true;
    }

    scene.textureCount = fileHeader->textureCount;
    scene.textures = static_cast<RHI::Texture*>(
        Alloc(sizeof(RHI::Texture) * scene.textureCount));
    MemZero(scene.textures, sizeof(RHI::Texture) * scene.textureCount);

    for (u32 i = 0; i < scene.textureCount; i++)
    {
        const ImageHeader& imageHeader = *(imageHeaderStart + i);
        VkFormat imageFormat =
            CompressedStorageToVkFormat(imageHeader.storageType);

        if (!RHI::CreateTexture2D(device,
                                  VK_IMAGE_USAGE_SAMPLED_BIT |
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  imageDataStart + imageHeader.offset,
                                  imageHeader.width, imageHeader.height,
                                  imageFormat,
                                  RHI::Sampler::FilterMode::Anisotropy8x,
                                  RHI::Sampler::WrapMode::Repeat,
                                  imageHeader.mipCount, scene.textures[i]))
        {
            DestroyScene(device, scene);
            return false;
        }
    }

    return true;
}

static bool ImportBuffers(RHI::Device& device,
                          const SceneFileHeader* fileHeader,
                          const QVertex* vertexStart, const u32* indexStart,
                          Scene& scene)
{
    if (fileHeader->totalVertexCount)
    {
        if (!RHI::CreateBuffer(device, false,
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                               vertexStart,
                               sizeof(QVertex) * fileHeader->totalVertexCount,
                               scene.vertexBuffer))
        {
            DestroyScene(device, scene);
            return false;
        }
    }

    if (fileHeader->totalIndexCount)
    {

        if (!RHI::CreateBuffer(device, false,
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                               indexStart,
                               sizeof(u32) * fileHeader->totalIndexCount,
                               scene.indexBuffer))
        {
            DestroyScene(device, scene);
            return false;
        }
    }

    return true;
}

static void ImportNodes(const SceneFileHeader* fileHeader,
                        const SerializedSceneNode* sceneNodeStart, Scene& scene)
{
    if (!fileHeader->nodeCount)
    {
        return;
    }

    scene.nodeCount = fileHeader->nodeCount;
    scene.nodes =
        static_cast<SceneNode*>(Alloc(sizeof(SceneNode) * scene.nodeCount));

    for (u32 i = 0; i < fileHeader->nodeCount; i++)
    {
        SceneNode& node = scene.nodes[i];
        node = {};
    }

    for (u32 i = 0; i < fileHeader->nodeCount; i++)
    {
        const SerializedSceneNode& serializedNode = *(sceneNodeStart + i);
        SceneNode& node = scene.nodes[i];
        node.transform = {};

        if (serializedNode.parentIndex != -1)
        {
            node.transform.SetParent(
                &scene.nodes[serializedNode.parentIndex].transform);
        }
        if (serializedNode.meshIndex != -1)
        {
            node.mesh = &scene.meshes[serializedNode.meshIndex];
        }
    }

    for (u32 i = 0; i < fileHeader->nodeCount; i++)
    {
        const SerializedSceneNode& serializedNode = *(sceneNodeStart + i);
        SceneNode& node = scene.nodes[i];
        node.transform.SetLocalTransform(serializedNode.localPosition,
                                         serializedNode.localRotation,
                                         serializedNode.localScale);
    }
}

static bool ImportMaterials(RHI::Device& device,
                            const SceneFileHeader* fileHeader,
                            const SerializedPBRMaterial* pbrMaterialStart,
                            Scene& scene)
{
    scene.materialCount = fileHeader->materialCount + 1;

    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);
    PBRMaterial* pbrMaterials =
        FLY_PUSH_ARENA(arena, PBRMaterial, scene.materialCount);

    pbrMaterials[0] = {};
    pbrMaterials[0].baseColor = Math::Vec4(1.0f, 0.0f, 1.0f, 1.0f);
    pbrMaterials[0].baseColorTextureBindlessHandle =
        scene.whiteTexture.bindlessHandle;
    pbrMaterials[0].normalTextureBindlessHandle =
        scene.flatNormalTexture.bindlessHandle;

    for (u32 i = 0; i < fileHeader->materialCount; i++)
    {
        const SerializedPBRMaterial& serializedMaterial = pbrMaterialStart[i];
        PBRMaterial& material = pbrMaterials[i + 1];
        material = {};

        material.baseColorTextureBindlessHandle =
            scene.whiteTexture.bindlessHandle;
        material.normalTextureBindlessHandle =
            scene.flatNormalTexture.bindlessHandle;

        if (serializedMaterial.baseColorTextureIndex != -1)
        {
            material.baseColorTextureBindlessHandle =
                scene.textures[serializedMaterial.baseColorTextureIndex]
                    .bindlessHandle;
        }

        if (serializedMaterial.normalTextureIndex != -1)
        {
            material.normalTextureBindlessHandle =
                scene.textures[serializedMaterial.normalTextureIndex]
                    .bindlessHandle;
        }
    }

    bool res = RHI::CreateBuffer(
        device, false,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        pbrMaterials, sizeof(PBRMaterial) * scene.materialCount,
        scene.pbrMaterialBuffer);

    ArenaPopToMarker(arena, marker);
    if (!res)
    {
        DestroyScene(device, scene);
    }

    return res;
}

static void ImportMeshes(const SceneFileHeader* fileHeader,
                         const MeshHeader* meshHeaderStart, const LOD* lodStart,
                         const i32* submeshMaterialIndexStart, Scene& scene)
{
    if (!fileHeader->meshCount)
    {
        return;
    }

    scene.meshCount = fileHeader->meshCount;
    scene.meshes = static_cast<Mesh*>(Alloc(sizeof(Mesh) * scene.meshCount));
    Submesh* submeshes = static_cast<Submesh*>(
        Alloc(sizeof(Submesh) * fileHeader->totalSubmeshCount));

    u32 lodOffset = 0;
    u32 submeshOffset = 0;
    i32 vertexOffset = 0;
    for (u32 i = 0; i < fileHeader->meshCount; i++)
    {
        const MeshHeader& meshHeader = *(meshHeaderStart + i);
        Mesh& mesh = *(scene.meshes + i);

        mesh.sphereCenter = meshHeader.sphereCenter;
        mesh.sphereRadius = meshHeader.sphereRadius;
        mesh.vertexCount = meshHeader.vertexCount;
        mesh.indexCount = meshHeader.indexCount;
        mesh.submeshCount = meshHeader.submeshCount;
        mesh.lodCount = meshHeader.lodCount;
        mesh.vertexOffset = vertexOffset;
        mesh.submeshes = submeshes + submeshOffset;

        for (u32 j = 0; j < meshHeader.submeshCount; j++)
        {
            i32 materialIndex =
                *(submeshMaterialIndexStart + submeshOffset + j);
            if (materialIndex != -1)
            {
                mesh.submeshes[j].materialIndex = materialIndex + 1;
            }
            else
            {
                mesh.submeshes[j].materialIndex = 0;
            }

            MemZero(mesh.submeshes[j].lods, sizeof(LOD) * FLY_MAX_LOD_COUNT);
            memcpy(mesh.submeshes[j].lods, lodStart + lodOffset,
                   meshHeader.lodCount * sizeof(LOD));
            lodOffset += meshHeader.lodCount;
        }

        vertexOffset += mesh.vertexCount;
        submeshOffset += mesh.submeshCount;
    }
}

static bool CreateFallbackTextures(RHI::Device& device, Scene& scene)
{
    const u8 white[4] = {255, 255, 255, 255};
    if (!RHI::CreateTexture2D(
            device,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, white,
            1, 1, VK_FORMAT_R8G8B8A8_UNORM, RHI::Sampler::FilterMode::Nearest,
            RHI::Sampler::WrapMode::Repeat, 1, scene.whiteTexture))
    {
        DestroyScene(device, scene);
        return false;
    }

    const u8 bc5FlatNormalBlock[16] = {128, 128, 0, 0, 0, 0, 0, 0,
                                       128, 128, 0, 0, 0, 0, 0, 0};

    if (!RHI::CreateTexture2D(
            device,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            bc5FlatNormalBlock, 4, 4, VK_FORMAT_BC5_UNORM_BLOCK,
            RHI::Sampler::FilterMode::Nearest, RHI::Sampler::WrapMode::Repeat,
            1, scene.flatNormalTexture))
    {
        DestroyScene(device, scene);
        return false;
    }

    return true;
}

bool ImportScene(String8 path, RHI::Device& device, Scene& scene)
{
    DestroyScene(device, scene);

    if (!CreateFallbackTextures(device, scene))
    {
        return false;
    }

    String8 extension = String8::FindLast(path, '.');
    if (!extension.StartsWith(FLY_STRING8_LITERAL(".fscene")))
    {
        return false;
    }

    u64 totalSize = 0;
    u64 offset = 0;
    u8* data = ReadFileToByteArray(path, totalSize);
    if (!data)
    {
        return false;
    }

    const SceneFileHeader* fileHeader =
        reinterpret_cast<const SceneFileHeader*>(data);
    offset += sizeof(SceneFileHeader);

    const ImageHeader* imageHeaderStart =
        reinterpret_cast<const ImageHeader*>(data + offset);
    offset += sizeof(ImageHeader) * fileHeader->textureCount;

    const SerializedPBRMaterial* pbrMaterialStart =
        reinterpret_cast<const SerializedPBRMaterial*>(data + offset);
    offset += sizeof(SerializedPBRMaterial) * fileHeader->materialCount;

    const MeshHeader* meshHeaderStart =
        reinterpret_cast<const MeshHeader*>(data + offset);
    offset += sizeof(MeshHeader) * fileHeader->meshCount;

    const SerializedSceneNode* sceneNodeStart =
        reinterpret_cast<const SerializedSceneNode*>(data + offset);
    offset += sizeof(SerializedSceneNode) * fileHeader->nodeCount;

    const LOD* lodStart = reinterpret_cast<const LOD*>(data + offset);
    offset += sizeof(LOD) * fileHeader->totalLodCount;

    const i32* submeshMaterialIndexStart =
        reinterpret_cast<i32*>(data + offset);
    offset += sizeof(i32) * fileHeader->totalSubmeshCount;

    const QVertex* vertexStart =
        reinterpret_cast<const QVertex*>(data + offset);
    offset += sizeof(QVertex) * fileHeader->totalVertexCount;

    const u32* indexStart = reinterpret_cast<const u32*>(data + offset);
    offset += sizeof(u32) * fileHeader->totalIndexCount;

    const u8* imageDataStart = data + offset;

    if (!ImportTextures(device, fileHeader, imageHeaderStart, imageDataStart,
                        scene))
    {
        Free(data);
        return false;
    }

    if (!ImportBuffers(device, fileHeader, vertexStart, indexStart, scene))
    {
        Free(data);
        return false;
    }

    if (!ImportMaterials(device, fileHeader, pbrMaterialStart, scene))
    {
        Free(data);
        return false;
    }

    ImportMeshes(fileHeader, meshHeaderStart, lodStart,
                 submeshMaterialIndexStart, scene);
    ImportNodes(fileHeader, sceneNodeStart, scene);

    Free(data);
    return true;
}

void DestroyScene(RHI::Device& device, Scene& scene)
{
    if (scene.meshes && scene.meshCount)
    {
        Free(scene.meshes[0].submeshes);
        Free(scene.meshes);
        scene.meshes = nullptr;
        scene.meshCount = 0;
    }

    if (scene.textures && scene.textureCount)
    {
        for (u32 i = 0; i < scene.textureCount; i++)
        {
            RHI::DestroyTexture(device, scene.textures[i]);
        }
        Free(scene.textures);
        scene.textures = nullptr;
        scene.textureCount = 0;
    }

    if (scene.whiteTexture.image != VK_NULL_HANDLE)
    {
        RHI::DestroyTexture(device, scene.whiteTexture);
    }

    if (scene.flatNormalTexture.image != VK_NULL_HANDLE)
    {
        RHI::DestroyTexture(device, scene.flatNormalTexture);
    }

    if (scene.nodes && scene.nodeCount)
    {
        Free(scene.nodes);
        scene.nodes = nullptr;
        scene.nodeCount = 0;
    }

    if (scene.pbrMaterialBuffer.handle != VK_NULL_HANDLE)
    {
        RHI::DestroyBuffer(device, scene.pbrMaterialBuffer);
    }
    if (scene.indexBuffer.handle != VK_NULL_HANDLE)
    {
        RHI::DestroyBuffer(device, scene.indexBuffer);
    }
    if (scene.vertexBuffer.handle != VK_NULL_HANDLE)
    {
        RHI::DestroyBuffer(device, scene.vertexBuffer);
    }
}
} // namespace Fly
