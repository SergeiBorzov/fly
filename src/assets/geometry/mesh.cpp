#include "mesh.h"
#include "core/filesystem.h"
#include "core/memory.h"

namespace Fly
{

struct MeshHeader
{
    u64 vertexCount;
    u64 vertexOffset;
    u64 indexCount;
    u64 indexOffset;
    u8 indexSize;
    u8 vertexSize;
    u8 vertexMask;
};

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
    mesh.vertexCount = header->vertexCount;
    mesh.indexCount = header->indexCount;
    mesh.vertexSize = header->vertexSize;
    mesh.indexSize = header->indexSize;
    mesh.vertexMask = header->vertexMask;

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
