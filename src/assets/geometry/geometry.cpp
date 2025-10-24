#include "geometry.h"
#include "core/filesystem.h"
#include "core/memory.h"

#define FAST_OBJ_REALLOC Fly::Realloc
#define FAST_OBJ_FREE Fly::Free
#define FAST_OBJ_IMPLEMENTATION
#include <fast_obj.h>

namespace Fly
{

struct Vertex
{
    Math::Vec3 position;
    f32 pad0;
    Math::Vec3 normal;
    f32 pad1;
};

struct VertexTexCoord
{
    Math::Vec3 position;
    f32 u;
    Math::Vec3 normal;
    f32 v;
};

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

static void CopyVertices(const fastObjMesh* mesh, Geometry& geometry)
{

    geometry.vertexCount = mesh->index_count;
    geometry.indexCount = mesh->index_count;

    if (!(geometry.vertexMask & FLY_VERTEX_TEXCOORD_BIT))
    {
        geometry.vertices =
            static_cast<u8*>(Fly::Alloc(sizeof(Vertex) * mesh->index_count));
        geometry.indices =
            static_cast<u32*>(Fly::Alloc(sizeof(u32) * mesh->index_count));

        for (u32 i = 0; i < mesh->index_count; i++)
        {
            geometry.indices[i] = i;
            fastObjIndex index = mesh->indices[i];

            Vertex vertex;
            vertex.position =
                *reinterpret_cast<Math::Vec3*>(&(mesh->positions[3 * index.p]));
            vertex.normal =
                *reinterpret_cast<Math::Vec3*>(&(mesh->normals[3 * index.n]));
            memcpy(geometry.vertices + sizeof(Vertex) * i, &vertex,
                   sizeof(Vertex));
        }
    }
    else
    {
        geometry.vertices = static_cast<u8*>(
            Fly::Alloc(sizeof(VertexTexCoord) * mesh->index_count));
        geometry.indices =
            static_cast<u32*>(Fly::Alloc(sizeof(u32) * mesh->index_count));

        for (u32 i = 0; i < mesh->index_count; i++)
        {
            geometry.indices[i] = i;
            fastObjIndex index = mesh->indices[i];
            VertexTexCoord vertex;
            vertex.position =
                *reinterpret_cast<Math::Vec3*>(&(mesh->positions[3 * index.p]));
            vertex.normal =
                *reinterpret_cast<Math::Vec3*>(&(mesh->normals[3 * index.n]));
            vertex.u = mesh->texcoords[2 * index.t];
            vertex.v = mesh->texcoords[2 * index.t + 1];
            memcpy(geometry.vertices + sizeof(VertexTexCoord) * i, &vertex,
                   sizeof(VertexTexCoord));
        }
    }
}

static bool ImportGeometryObj(String8 path, Geometry& geometry)
{
    FLY_ASSERT(path);

    fastObjMesh* mesh = fast_obj_read(path.Data());
    if (!mesh)
    {
        return false;
    }
    FLY_ASSERT(mesh.object_count == 1);

    geometry.vertexSize = 0;
    geometry.vertexMask = FLY_VERTEX_NONE_BIT;

    if (mesh->position_count <= 1)
    {
        return false;
    }

    geometry.vertexMask = FLY_VERTEX_POSITION_BIT | FLY_VERTEX_NORMAL_BIT;
    geometry.vertexSize = sizeof(Vertex);

    // TODO: Normal generation

    if (mesh->texcoord_count > 1)
    {
        geometry.vertexMask |= FLY_VERTEX_TEXCOORD_BIT;
        geometry.vertexSize = sizeof(VertexTexCoord);
    }

    for (u32 i = 0; i < mesh->face_count; i++)
    {
        // TODO: triangulation
        if (mesh->face_vertices[i] != 3)
        {
            return false;
        }
    }

    CopyVertices(mesh, geometry);

    fast_obj_destroy(mesh);

    return true;
}

bool ImportGeometry(String8 path, Geometry& geometry)
{
    String8 extension = String8::FindLast(path, '.');
    if (extension.StartsWith(FLY_STRING8_LITERAL(".obj")) ||
        extension.StartsWith(FLY_STRING8_LITERAL(".OBJ")))
    {
        return ImportGeometryObj(path, geometry);
    }
    return false;
}

void DestroyGeometry(Geometry& geometry)
{
    if (geometry.vertices)
    {
        Fly::Free(geometry.vertices);
        geometry.vertices = nullptr;
    }
    if (geometry.indices)
    {
        Fly::Free(geometry.indices);
        geometry.indices = nullptr;
    }
}

bool ExportGeometry(String8 path, Geometry& geometry)
{
    u64 totalSize = sizeof(MeshHeader) +
                    geometry.vertexSize * geometry.vertexCount +
                    sizeof(u32) * geometry.indexCount;
    u8* data = static_cast<u8*>(Fly::Alloc(totalSize));

    MeshHeader* header = reinterpret_cast<MeshHeader*>(data);
    header->vertexCount = geometry.vertexCount;
    header->vertexOffset = sizeof(MeshHeader);
    header->vertexMask = geometry.vertexMask;
    header->vertexSize = geometry.vertexSize;
    header->indexCount = geometry.indexCount;
    header->indexOffset =
        sizeof(MeshHeader) + geometry.vertexSize * geometry.vertexCount;
    header->indexSize = sizeof(u32);

    memcpy(data + header->vertexOffset, geometry.vertices,
           geometry.vertexSize * geometry.vertexCount);
    memcpy(data + header->indexOffset, geometry.indices,
           sizeof(u32) * geometry.indexCount);

    String8 str(reinterpret_cast<char*>(data), totalSize);
    if (!WriteStringToFile(str, path))
    {
        Fly::Free(data);
        return false;
    }

    Fly::Free(data);
    return true;
}

} // namespace Fly
