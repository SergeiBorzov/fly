#include "core/filesystem.h"
#include "core/memory.h"
#include "core/string8.h"

#include "rhi/device.h"
#include "rhi/texture.h"

#include "assets/geometry/mesh.h"
#include "assets/geometry/vertex_layout.h"
#include "assets/image/image.h"

#include "scene.h"
#include "scene_serialization_types.h"

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

bool ImportScene(String8 path, RHI::Device& device, Scene& scene)
{
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

    const ImageHeader* imageHeaderStart = nullptr;
    if (fileHeader->textureCount)
    {
        imageHeaderStart = reinterpret_cast<const ImageHeader*>(data + offset);
        offset += sizeof(ImageHeader) * fileHeader->textureCount;
    }

    const MeshHeader* meshHeaderStart = nullptr;
    if (fileHeader->meshCount)
    {
        meshHeaderStart = reinterpret_cast<const MeshHeader*>(data + offset);
        offset += sizeof(MeshHeader) * fileHeader->meshCount;
    }

    const SerializedSceneNode* sceneNodeStart = nullptr;
    if (fileHeader->nodeCount)
    {
        sceneNodeStart =
            reinterpret_cast<const SerializedSceneNode*>(data + offset);
        offset += sizeof(SerializedSceneNode) * fileHeader->nodeCount;
    }

    const LOD* lodStart = nullptr;
    if (fileHeader->totalLodCount)
    {
        lodStart = reinterpret_cast<const LOD*>(data + offset);
        offset += sizeof(LOD) * fileHeader->totalLodCount;
    }

    const QVertex* vertexStart = nullptr;
    if (fileHeader->totalVertexCount)
    {
        vertexStart = reinterpret_cast<const QVertex*>(data + offset);
        offset += sizeof(QVertex) * fileHeader->totalVertexCount;
    }

    const u32* indexStart = nullptr;
    if (fileHeader->totalIndexCount)
    {
        indexStart = reinterpret_cast<const u32*>(data + offset);
        offset += sizeof(u32) * fileHeader->totalIndexCount;
    }

    const u8* imageDataStart = nullptr;
    if (fileHeader->textureCount)
    {
        imageDataStart = reinterpret_cast<const u8*>(data + offset);
    }

    // Textures
    if (imageHeaderStart)
    {
        scene.textureCount = fileHeader->textureCount;
        scene.textures = static_cast<RHI::Texture*>(
            Alloc(sizeof(RHI::Texture) * scene.textureCount));
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
                Free(data);
                return false;
            }
        }
    }

    // Buffers
    if (vertexStart &&
        !RHI::CreateBuffer(device, false,
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                           vertexStart,
                           sizeof(QVertex) * fileHeader->totalVertexCount,
                           scene.vertexBuffer))
    {
        Free(data);
        return false;
    }

    if (vertexStart && indexStart &&
        !RHI::CreateBuffer(
            device, false,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            indexStart, sizeof(u32) * fileHeader->totalIndexCount,
            scene.indexBuffer))
    {
        RHI::DestroyBuffer(device, scene.vertexBuffer);
        Free(data);
        return false;
    }

    // Meshes
    if (meshHeaderStart)
    {
        scene.meshCount = fileHeader->meshCount;
        scene.meshes =
            static_cast<Mesh*>(Alloc(sizeof(Mesh) * scene.meshCount));
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
                MemZero(mesh.submeshes[j].lods,
                        sizeof(LOD) * FLY_MAX_LOD_COUNT);
                memcpy(mesh.submeshes[j].lods, lodStart + lodOffset,
                       meshHeader.lodCount * sizeof(LOD));
                lodOffset += meshHeader.lodCount;
            }

            vertexOffset += mesh.vertexCount;
            submeshOffset += mesh.submeshCount;
        }
    }

    // Nodes
    if (sceneNodeStart)
    {
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

    if (scene.nodes && scene.nodeCount)
    {
        Free(scene.nodes);
        scene.nodes = nullptr;
        scene.nodeCount = 0;
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
