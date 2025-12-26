#include "core/filesystem.h"
#include "core/memory.h"
#include <stdio.h>

#include "math/vec.h"

#include "rhi/buffer.h"

#include "mesh.h"

namespace Fly
{

bool ImportMeshes(String8 path, RHI::Device& device, Mesh** meshes,
                  u32& meshCount, RHI::Buffer& vertexBuffer,
                  RHI::Buffer& indexBuffer)
{
    String8 extension = String8::FindLast(path, '.');
    if (!extension.StartsWith(FLY_STRING8_LITERAL(".fmesh")))
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

    MeshFileHeader* fileHeader = reinterpret_cast<MeshFileHeader*>(data);
    offset += sizeof(MeshFileHeader);

    MeshHeader* meshHeaderStart = reinterpret_cast<MeshHeader*>(data + offset);
    offset += sizeof(MeshHeader) * fileHeader->meshCount;

    LOD* lodStart = reinterpret_cast<LOD*>(data + offset);
    offset += sizeof(LOD) * fileHeader->totalLodCount;

    QVertex* vertexStart = reinterpret_cast<QVertex*>(data + offset);
    offset += sizeof(QVertex) * fileHeader->totalVertexCount;

    u32* indexStart = reinterpret_cast<u32*>(data + offset);

    *meshes = static_cast<Mesh*>(Alloc(sizeof(Mesh) * fileHeader->meshCount));

    u32 lodOffset = 0;
    i32 vertexOffset = 0;
    meshCount = fileHeader->meshCount;
    for (u32 i = 0; i < fileHeader->meshCount; i++)
    {
        MeshHeader& meshHeader = *(meshHeaderStart + i);
        Mesh& mesh = (*meshes)[i];

        mesh.sphereCenter = meshHeader.sphereCenter;
        mesh.submeshCount = meshHeader.submeshCount;
        mesh.vertexCount = meshHeader.vertexCount;
        mesh.indexCount = meshHeader.indexCount;
        mesh.sphereRadius = meshHeader.sphereRadius;
        mesh.lodCount = meshHeader.lodCount;
        mesh.vertexOffset = vertexOffset;

        mesh.submeshes = static_cast<Submesh*>(
            Alloc(sizeof(Submesh) * meshHeader.submeshCount));

        for (u32 j = 0; j < meshHeader.submeshCount; j++)
        {
            for (u32 k = 0; k < FLY_MAX_LOD_COUNT; k++)
            {
                mesh.submeshes[j].lods[k] = {};
            }
            memcpy(mesh.submeshes[j].lods, lodStart + lodOffset,
                   meshHeader.lodCount * sizeof(LOD));
            lodOffset += meshHeader.lodCount;
        }

        vertexOffset += mesh.vertexCount;
    }

    for (u32 i = 0; i < fileHeader->totalLodCount; i++)
    {
        LOD& lod = *(lodStart + i);
        printf("first index: %u, index count: %u\n", lod.firstIndex,
               lod.indexCount);
    }

    if (!RHI::CreateBuffer(device, false,
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                           vertexStart,
                           sizeof(QVertex) * fileHeader->totalVertexCount,
                           vertexBuffer))
    {
        Free(data);
        return false;
    }

    if (!RHI::CreateBuffer(
            device, false,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            indexStart, sizeof(u32) * fileHeader->totalIndexCount, indexBuffer))
    {
        Free(data);
        return false;
    }

    Free(data);
    return true;
}

void DestroyMesh(RHI::Device& device, Mesh& mesh)
{
    if (mesh.submeshes && mesh.submeshCount)
    {
        Free(mesh.submeshes);
    }

    mesh = {};
}

} // namespace Fly
