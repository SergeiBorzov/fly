#include "core/filesystem.h"
#include "core/memory.h"

#include "math/vec.h"

#include "mesh.h"

namespace Fly
{

bool ImportMeshes(String8 path, RHI::Device& device, Mesh** meshes,
                  u32& meshCount)
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

        if (!RHI::CreateBuffer(device, false,
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                               vertexStart + meshHeader.vertexOffset,
                               sizeof(QVertex) * meshHeader.vertexCount,
                               mesh.vertexBuffer))
        {
            Free(data);
            return false;
        }

        if (!RHI::CreateBuffer(device, false,
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                               indexStart + meshHeader.indexOffset,
                               sizeof(u32) * meshHeader.indexCount,
                               mesh.indexBuffer))
        {
            Free(data);
            return false;
        }

        mesh.submeshes =
            static_cast<Submesh*>(sizeof(Submesh) * meshHeader.submeshCount);

        for (u32 j = 0; j < meshHeader.submeshCount; j++)
        {
            memset(mesh.submeshes.lods, 0, sizeof(LOD) * FLY_MAX_LOD_COUNT);
            memcpy(mesh.submeshes.lods, lodStart + lodOffset,
                   meshHeader.lodCount * sizeof(LOD));
            lodOffset += meshHeader.lodCount;
        }
    }

    Free(data);
    return true;
}

bool ImportMesh(String8 path, RHI::Device& device, Mesh& mesh)
{
    String8 extension = String8::FindLast(path, '.');
    if (!extension.StartsWith(FLY_STRING8_LITERAL(".fmesh")))
    {
        return false;
    }

    u64 size = 0;
    u8* data = ReadFileToByteArray(path, size);
    if (!data)
    {
        return false;
    }

    const MeshHeader* header = reinterpret_cast<const MeshHeader*>(data);
    mesh.sphereCenter = header->sphereCenter;
    mesh.sphereRadius = header->sphereRadius;
    mesh.vertexCount = header->vertexCount;
    mesh.indexCount = header->indexCount;
    mesh.vertexSize = header->vertexSize;
    mesh.indexSize = header->indexSize;
    mesh.vertexMask = header->vertexMask;
    mesh.lodCount = header->lodCount;
    memcpy(mesh.lods, header->lods, sizeof(GeometryLOD) * header->lodCount);

    if (!RHI::CreateBuffer(device, false,
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                           data + header->vertexOffset,
                           header->vertexSize * header->vertexCount,
                           mesh.vertexBuffer))
    {
        Fly::Free(data);
        return false;
    }

    if (!RHI::CreateBuffer(
            device, false,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            data + header->indexOffset, header->indexSize * header->indexCount,
            mesh.indexBuffer))
    {
        Fly::Free(data);
        return false;
    }

    Fly::Free(data);
    return true;
}

void DestroyMesh(RHI::Device& device, Mesh& mesh)
{
    if (mesh.vertexBuffer.handle != VK_NULL_HANDLE)
    {
        RHI::DestroyBuffer(device, mesh.vertexBuffer);
    }

    if (mesh.indexBuffer.handle != VK_NULL_HANDLE)
    {
        RHI::DestroyBuffer(device, mesh.indexBuffer);
    }

    mesh.vertexCount = 0;
    mesh.indexCount = 0;
    mesh.vertexSize = 0;
    mesh.indexSize = 0;
    mesh.vertexMask = FLY_VERTEX_NONE_BIT;
}

} // namespace Fly
