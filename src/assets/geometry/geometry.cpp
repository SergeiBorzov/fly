#include "geometry.h"
#include "core/filesystem.h"
#include "core/memory.h"

#define FAST_OBJ_REALLOC Fly::Realloc
#define FAST_OBJ_FREE Fly::Free
#define FAST_OBJ_IMPLEMENTATION
#include <fast_obj.h>

#include <meshoptimizer.h>

namespace Fly
{

typedef void (*VertexTransformFunc)(void* vertex, void* userData);

struct Vertex
{
    Math::Vec3 position;
    f32 pad0;
    Math::Vec3 normal;
    f32 pad1;
};

struct QuantizedVertex
{
    u16 positionX;
    u16 positionY;
    u16 positionZ;
    u16 pad0;
    u32 normal;
    u32 pad1;
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
    GeometryLOD lods[FLY_MAX_LOD_COUNT];
    u64 vertexCount;
    u64 vertexOffset;
    u64 indexCount;
    u64 indexOffset;
    u8 indexSize;
    u8 vertexSize;
    u8 vertexMask;
    u8 lodCount;
};

// struct QuantizedVertex
// {
//     f16 posX;
//     f16 posY;
//     f16 posZ;
//     u8 normalX;
//     u8 normalY;
//     u8 normalZ;
//     u8 tangentX;
//     u8 tangentY;
//     u8 tangentZ;
//     u16 u;
//     u16 v;
// };

struct TransformData
{
    f32 scale;
    CoordSystem coordSystem;
    bool flipForward;
};

static void CopyVertices(const fastObjMesh* mesh, Geometry& geometry)
{

    geometry.vertexCount = mesh->index_count;
    geometry.indexCount = mesh->index_count;
    geometry.lodCount = 1;

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

            Vertex vertex{};
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
            VertexTexCoord vertex{};
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

static void ApplyVertexTransformToGeometry(
    Geometry& geometry, VertexTransformFunc vertexTransformFunc, void* userData)
{
    FLY_ASSERT(vertexTransformFunc);
    for (u32 i = 0; i < geometry.vertexCount; i++)
    {
        vertexTransformFunc(geometry.vertices + i * geometry.vertexSize,
                            userData);
    }
}

static void TransformVertexTexCoord(void* pVertex, void* userData)
{
    FLY_ASSERT(pVertex);
    FLY_ASSERT(userData);

    const TransformData& transformData =
        *static_cast<const TransformData*>(userData);

    VertexTexCoord& vertex = *static_cast<VertexTexCoord*>(pVertex);
    switch (transformData.coordSystem)
    {
        case CoordSystem::XZY:
        {
            vertex.position = Math::Vec3(vertex.position.x, vertex.position.z,
                                         vertex.position.y);
            vertex.normal =
                Math::Vec3(vertex.normal.x, vertex.normal.z, vertex.normal.y);
            break;
        }
        case CoordSystem::YXZ:
        {
            vertex.position = Math::Vec3(vertex.position.y, vertex.position.x,
                                         vertex.position.z);
            vertex.normal =
                Math::Vec3(vertex.normal.y, vertex.normal.x, vertex.normal.z);
            break;
        }
        case CoordSystem::YZX:
        {
            vertex.position = Math::Vec3(vertex.position.y, vertex.position.z,
                                         vertex.position.x);
            vertex.normal =
                Math::Vec3(vertex.normal.y, vertex.normal.z, vertex.normal.x);
            break;
        }
        case CoordSystem::ZXY:
        {
            vertex.position = Math::Vec3(vertex.position.z, vertex.position.x,
                                         vertex.position.y);
            vertex.normal =
                Math::Vec3(vertex.normal.z, vertex.normal.x, vertex.normal.y);
            break;
        }
        case CoordSystem::ZYX:
        {
            vertex.position = Math::Vec3(vertex.position.z, vertex.position.y,
                                         vertex.position.x);
            vertex.normal =
                Math::Vec3(vertex.normal.z, vertex.normal.y, vertex.normal.x);
            break;
        }
        default:
        {
            break;
        }
    }

    vertex.position *= transformData.scale;
    if (transformData.flipForward)
    {
        vertex.position.z *= -1.0f;
        vertex.normal.z *= -1.0f;
    }
}

static void TransformVertex(void* pVertex, void* userData)
{
    FLY_ASSERT(vertex);
    FLY_ASSERT(userData);

    const TransformData& transformData =
        *static_cast<const TransformData*>(userData);

    Vertex& vertex = *static_cast<Vertex*>(pVertex);
    switch (transformData.coordSystem)
    {
        case CoordSystem::XZY:
        {
            vertex.position = Math::Vec3(vertex.position.x, vertex.position.z,
                                         vertex.position.y);
            vertex.normal =
                Math::Vec3(vertex.normal.x, vertex.normal.z, vertex.normal.y);
            break;
        }
        case CoordSystem::YXZ:
        {
            vertex.position = Math::Vec3(vertex.position.y, vertex.position.x,
                                         vertex.position.z);
            vertex.normal =
                Math::Vec3(vertex.normal.y, vertex.normal.x, vertex.normal.z);
            break;
        }
        case CoordSystem::YZX:
        {
            vertex.position = Math::Vec3(vertex.position.y, vertex.position.z,
                                         vertex.position.x);
            vertex.normal =
                Math::Vec3(vertex.normal.y, vertex.normal.z, vertex.normal.x);
            break;
        }
        case CoordSystem::ZXY:
        {
            vertex.position = Math::Vec3(vertex.position.z, vertex.position.x,
                                         vertex.position.y);
            vertex.normal =
                Math::Vec3(vertex.normal.z, vertex.normal.x, vertex.normal.y);
            break;
        }
        case CoordSystem::ZYX:
        {
            vertex.position = Math::Vec3(vertex.position.z, vertex.position.y,
                                         vertex.position.x);
            vertex.normal =
                Math::Vec3(vertex.normal.z, vertex.normal.y, vertex.normal.x);
            break;
        }
        default:
        {
            break;
        }
    }

    vertex.position *= transformData.scale;
    if (transformData.flipForward)
    {
        vertex.position.z *= -1.0f;
        vertex.normal.z *= -1.0f;
    }
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

void TransformGeometry(f32 scale, CoordSystem coordSystem, bool flipForward,
                       Geometry& geometry)
{
    TransformData transformData;
    transformData.scale = scale;
    transformData.coordSystem = coordSystem;
    transformData.flipForward = flipForward;

    if (geometry.vertexMask & FLY_VERTEX_TEXCOORD_BIT)
    {
        ApplyVertexTransformToGeometry(geometry, TransformVertexTexCoord,
                                       &transformData);
    }
    else
    {
        ApplyVertexTransformToGeometry(geometry, TransformVertex,
                                       &transformData);
    }
}

void FlipGeometryWindingOrder(Geometry& geometry)
{
    for (u32 i = 0; i < geometry.vertexCount; i += 3)
    {
        if (geometry.vertexMask & FLY_VERTEX_TEXCOORD_BIT)
        {
            VertexTexCoord& v0 = *reinterpret_cast<VertexTexCoord*>(
                geometry.vertices + 3 * i * geometry.vertexSize);
            VertexTexCoord& v1 = *reinterpret_cast<VertexTexCoord*>(
                geometry.vertices + (3 * i + 1) * geometry.vertexSize);

            VertexTexCoord tmp = v0;
            v0 = v1;
            v1 = tmp;
        }
        else
        {
            Vertex& v0 = *reinterpret_cast<Vertex*>(geometry.vertices +
                                                    i * geometry.vertexSize);
            Vertex& v1 = *reinterpret_cast<Vertex*>(
                geometry.vertices + (i + 1) * geometry.vertexSize);

            Vertex tmp = v0;
            v0 = v1;
            v1 = tmp;
        }
    }
}

void ReindexGeometry(Geometry& geometry)
{
    unsigned int* remap = static_cast<unsigned int*>(
        Fly::Alloc(sizeof(unsigned int) * geometry.vertexCount));
    size_t newVertexCount = meshopt_generateVertexRemap(
        remap, geometry.indices, geometry.indexCount, geometry.vertices,
        geometry.vertexCount, geometry.vertexSize);

    void* newVertices = Fly::Alloc(geometry.vertexSize * newVertexCount);
    unsigned int* newIndices = static_cast<unsigned int*>(
        Fly::Alloc(sizeof(unsigned int) * geometry.indexCount));

    meshopt_remapVertexBuffer(newVertices, geometry.vertices,
                              geometry.vertexCount, geometry.vertexSize, remap);
    meshopt_remapIndexBuffer(newIndices, geometry.indices, geometry.indexCount,
                             remap);

    Fly::Free(geometry.vertices);
    Fly::Free(geometry.indices);
    Fly::Free(remap);
    geometry.vertices = static_cast<u8*>(newVertices);
    geometry.indices = newIndices;
    geometry.vertexCount = newVertexCount;
}

void OptimizeGeometryVertexCache(Geometry& geometry)
{
    // Note: If index buffer contains multiple ranges for multiple draw calls,
    // this function needs to be called on each range individually
    unsigned int* newIndices = static_cast<unsigned int*>(
        Fly::Alloc(sizeof(unsigned int) * geometry.indexCount));

    meshopt_optimizeVertexCache(newIndices, geometry.indices,
                                geometry.indexCount, geometry.vertexCount);
    Fly::Free(geometry.indices);
    geometry.indices = newIndices;
}

void OptimizeGeometryOverdraw(Geometry& geometry, f32 threshold)
{
    unsigned int* newIndices = static_cast<unsigned int*>(
        Fly::Alloc(sizeof(unsigned int) * geometry.indexCount));
    meshopt_optimizeOverdraw(newIndices, geometry.indices, geometry.indexCount,
                             reinterpret_cast<const f32*>(geometry.vertices),
                             geometry.vertexCount, geometry.vertexSize,
                             threshold);
    Fly::Free(geometry.indices);
    geometry.indices = newIndices;
}

void OptimizeGeometryVertexFetch(Geometry& geometry)
{
    void* newVertices = Fly::Alloc(geometry.vertexSize * geometry.vertexCount);
    size_t newVertexCount = meshopt_optimizeVertexFetch(
        newVertices, geometry.indices, geometry.indexCount, geometry.vertices,
        geometry.vertexCount, geometry.vertexSize);
    Fly::Free(geometry.vertices);
    geometry.vertices = static_cast<u8*>(newVertices);
    geometry.vertexCount = newVertexCount;
}

void QuantizeGeometry(Geometry& geometry)
{
    if (geometry.vertexMask & FLY_VERTEX_TEXCOORD_BIT)
    {
        return;
    }

    QuantizedVertex* newVertices = static_cast<QuantizedVertex*>(
        Fly::Alloc(sizeof(QuantizedVertex) * geometry.vertexCount));
    for (u32 i = 0; i < geometry.vertexCount; i++)
    {
        QuantizedVertex& quantized = newVertices[i];
        Vertex& vertex = reinterpret_cast<Vertex*>(geometry.vertices)[i];
        quantized.positionX = meshopt_quantizeHalf(vertex.position.x);
        quantized.positionY = meshopt_quantizeHalf(vertex.position.y);
        quantized.positionZ = meshopt_quantizeHalf(vertex.position.z);
        quantized.normal =
            (meshopt_quantizeUnorm((vertex.normal.x + 1.0f) / 2.0f, 10) << 20) |
            (meshopt_quantizeUnorm((vertex.normal.y + 1.0f) / 2.0f, 10) << 10) |
            meshopt_quantizeUnorm((vertex.normal.z + 1.0f) / 2.0f, 10);
    }

    Fly::Free(geometry.vertices);
    geometry.vertices = reinterpret_cast<u8*>(newVertices);
    geometry.vertexSize = sizeof(QuantizedVertex);
}

void GenerateGeometryLODs(Geometry& geometry)
{
    f32 targetErrors[3] = {0.0005f, 0.003f, 0.01f};
    unsigned int* lodIndices[3] = {nullptr, nullptr, nullptr};
    size_t lodIndexCount[3] = {0};
    for (u32 i = 0; i < 3; i++)
    {
        lodIndices[i] = static_cast<unsigned int*>(
            Fly::Alloc(sizeof(unsigned int) * geometry.indexCount));
        lodIndexCount[i] = meshopt_simplify(
            lodIndices[i], geometry.indices, geometry.indexCount,
            reinterpret_cast<const float*>(geometry.vertices),
            geometry.vertexCount, geometry.vertexSize, 0, targetErrors[i], 0,
            nullptr);
    }

    unsigned int* optimizedLodIndices[3] = {nullptr, nullptr, nullptr};
    for (u32 i = 0; i < 3; i++)
    {
        optimizedLodIndices[i] = static_cast<unsigned int*>(
            Fly::Alloc(sizeof(unsigned int) * lodIndexCount[i]));
        meshopt_optimizeVertexCache(optimizedLodIndices[i], lodIndices[i],
                                    lodIndexCount[i], geometry.vertexCount);
        Fly::Free(lodIndices[i]);
    }

    u64 totalCount = geometry.indexCount;
    for (u32 i = 0; i < 3; i++)
    {
        totalCount += lodIndexCount[i];
    }

    unsigned int* newIndices = static_cast<unsigned int*>(
        Fly::Alloc(sizeof(unsigned int) * totalCount));

    memcpy(newIndices, geometry.indices,
           sizeof(unsigned int) * geometry.indexCount);
    geometry.lods[0].indexCount = geometry.indexCount;
    geometry.lods[0].firstIndex = 0;

    u64 offset = geometry.indexCount;
    for (u32 i = 0; i < 3; i++)
    {
        geometry.lods[i + 1].indexCount = lodIndexCount[i];
        geometry.lods[i + 1].firstIndex = offset;
        memcpy(newIndices + offset, optimizedLodIndices[i],
               lodIndexCount[i] * sizeof(unsigned int));
        offset += lodIndexCount[i];
    }

    Fly::Free(geometry.indices);
    geometry.indices = newIndices;
    geometry.indexCount = totalCount;
    geometry.lodCount = 4;

    for (u32 i = 0; i < 3; i++)
    {
        Fly::Free(optimizedLodIndices[i]);
    }
}

void CookGeometry(Geometry& geometry)
{
    ReindexGeometry(geometry);
    OptimizeGeometryVertexCache(geometry);
    OptimizeGeometryOverdraw(geometry, 1.05f);
    GenerateGeometryLODs(geometry);

    QuantizeGeometry(geometry);
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
    header->indexSize = sizeof(unsigned int);

    header->lodCount = geometry.lodCount;
    memset(header->lods, 0, sizeof(GeometryLOD) * FLY_MAX_LOD_COUNT);
    memcpy(header->lods, geometry.lods,
           sizeof(GeometryLOD) * geometry.lodCount);

    memcpy(data + header->vertexOffset, geometry.vertices,
           geometry.vertexSize * geometry.vertexCount);
    memcpy(data + header->indexOffset, geometry.indices,
           sizeof(unsigned int) * geometry.indexCount);

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
