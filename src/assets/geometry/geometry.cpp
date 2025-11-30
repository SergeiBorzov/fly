#include <stdio.h>

#include "core/filesystem.h"
#include "core/memory.h"
#include "geometry.h"

#define FAST_OBJ_REALLOC Fly::Realloc
#define FAST_OBJ_FREE Fly::Free
#define FAST_OBJ_IMPLEMENTATION
#include <fast_obj.h>

#include <meshoptimizer.h>

namespace Fly
{

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

typedef void (*VertexTransformFunc)(Vertex& vertex, void* userData);

struct TransformData
{
    f32 scale;
    CoordSystem coordSystem;
    bool flipForward;
};

static void TransformVertex(Vertex& vertex, void* userData)
{
    FLY_ASSERT(userData);

    const TransformData& transformData =
        *static_cast<const TransformData*>(userData);

    switch (transformData.coordSystem)
    {
        case CoordSystem::XZY:
        {
            vertex.position = Math::Vec3(vertex.position.x, vertex.position.z,
                                         vertex.position.y);
            vertex.normal =
                Math::Vec3(vertex.normal.x, vertex.normal.z, vertex.normal.y);
            vertex.tangent = Math::Vec3(vertex.tangent.x, vertex.tangent.z,
                                        vertex.tangent.y);
            break;
        }
        case CoordSystem::YXZ:
        {
            vertex.position = Math::Vec3(vertex.position.y, vertex.position.x,
                                         vertex.position.z);
            vertex.normal =
                Math::Vec3(vertex.normal.y, vertex.normal.x, vertex.normal.z);
            vertex.tangent = Math::Vec3(vertex.tangent.y, vertex.tangent.x,
                                        vertex.tangent.z);
            break;
        }
        case CoordSystem::YZX:
        {
            vertex.position = Math::Vec3(vertex.position.y, vertex.position.z,
                                         vertex.position.x);
            vertex.normal =
                Math::Vec3(vertex.normal.y, vertex.normal.z, vertex.normal.x);
            vertex.tangent = Math::Vec3(vertex.tangent.y, vertex.tangent.z,
                                        vertex.tangent.x);
            break;
        }
        case CoordSystem::ZXY:
        {
            vertex.position = Math::Vec3(vertex.position.z, vertex.position.x,
                                         vertex.position.y);
            vertex.normal =
                Math::Vec3(vertex.normal.z, vertex.normal.x, vertex.normal.y);
            vertex.tangent = Math::Vec3(vertex.tangent.z, vertex.tangent.x,
                                        vertex.tangent.y);
            break;
        }
        case CoordSystem::ZYX:
        {
            vertex.position = Math::Vec3(vertex.position.z, vertex.position.y,
                                         vertex.position.x);
            vertex.normal =
                Math::Vec3(vertex.normal.z, vertex.normal.y, vertex.normal.x);
            vertex.tangent = Math::Vec3(vertex.tangent.z, vertex.tangent.y,
                                        vertex.tangent.x);
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
        vertex.tangent.z *= -1.0f;
    }
}

static void ApplyVertexTransformToGeometry(
    Geometry& geometry, VertexTransformFunc vertexTransformFunc, void* userData)
{
    FLY_ASSERT(vertexTransformFunc);
    for (u32 i = 0; i < geometry.vertexCount; i++)
    {
        vertexTransformFunc(geometry.vertices[i], userData);
    }
}

void TransformGeometry(f32 scale, CoordSystem coordSystem, bool flipForward,
                       Geometry& geometry)
{
    TransformData transformData;
    transformData.scale = scale;
    transformData.coordSystem = coordSystem;
    transformData.flipForward = flipForward;

    ApplyVertexTransformToGeometry(geometry, TransformVertex, &transformData);
    printf("Transformed geometry!\n");
}

static void ExtractGeometryDataFromObj(const fastObjMesh& mesh,
                                       const fastObjGroup& object,
                                       Geometry& geometry, u8 vertexMask)
{
    geometry = {};
    geometry.lodCount = 1;
    geometry.vertexMask = vertexMask;

    DynamicArray<Subgeometry> subgeometries;
    DynamicArray<Vertex> vertices;
    DynamicArray<u32> indices;

    i32 currMaterialIndex = -1;
    u32 subgeometryIndexCount = 0;
    for (u32 i = 0; i < object.face_count; i++)
    {
        i32 materialIndex =
            static_cast<i32>(mesh.face_materials[object.face_offset + i]);
        if (materialIndex != currMaterialIndex)
        {
            if (i != 0 && subgeometryIndexCount > 0)
            {
                Subgeometry& last = *(subgeometries.Last());
                last.lods[0].indexCount = subgeometryIndexCount;
                subgeometryIndexCount = 0;
            }

            Subgeometry subgeometry{};
            subgeometry.lods[0].indexOffset = 3 * i;
            subgeometries.Add(subgeometry);
            currMaterialIndex = materialIndex;
        }

        for (u32 j = 0; j < 3; j++)
        {
            fastObjIndex index = mesh.indices[object.index_offset + 3 * i + j];
            Vertex vertex{};
            vertex.position =
                *reinterpret_cast<Math::Vec3*>(&(mesh.positions[3 * index.p]));
            if (geometry.vertexMask & FLY_VERTEX_NORMAL_BIT)
            {
                vertex.normal = *reinterpret_cast<Math::Vec3*>(
                    &(mesh.normals[3 * index.n]));
            }
            if (geometry.vertexMask & FLY_VERTEX_TEXCOORD_BIT)
            {
                vertex.u = mesh.texcoords[2 * index.t];
                vertex.v = mesh.texcoords[2 * index.t + 1];
            }
            vertices.Add(vertex);
            indices.Add(3 * i + j);
            subgeometryIndexCount++;
        }
    }

    if (subgeometryIndexCount > 0)
    {
        Subgeometry& last = *(subgeometries.Last());
        last.lods[0].indexCount = subgeometryIndexCount;
    }

    {
        geometry.subgeometries = static_cast<Subgeometry*>(
            Alloc(sizeof(Subgeometry) * subgeometries.Count()));
        geometry.subgeometryCount = subgeometries.Count();
        memcpy(geometry.subgeometries, subgeometries.Data(),
               sizeof(Subgeometry) * subgeometries.Count());
    }
    {
        geometry.vertices =
            static_cast<Vertex*>(Alloc(sizeof(Vertex) * vertices.Count()));
        memcpy(geometry.vertices, vertices.Data(),
               sizeof(Vertex) * vertices.Count());
        geometry.vertexCount = vertices.Count();
    }
    {
        geometry.indices =
            static_cast<u32*>(Alloc(sizeof(u32) * indices.Count()));
        memcpy(geometry.indices, indices.Data(), sizeof(u32) * indices.Count());
        geometry.indexCount = indices.Count();
    }
}

static bool ImportGeometriesObj(String8 path, Geometry** ppGeometries,
                                u32& geometryCount)
{
    FLY_ASSERT(path);
    FLY_ASSERT(ppGeometries);
    geometryCount = 0;

    fastObjMesh* mesh = fast_obj_read(path.Data());
    if (!mesh || mesh->object_count == 0)
    {
        return false;
    }

    if (mesh->position_count < 3)
    {
        return false;
    }
    u8 vertexMask = FLY_VERTEX_POSITION_BIT;

    if (mesh->normal_count > 0)
    {
        vertexMask |= FLY_VERTEX_NORMAL_BIT;
    }

    if (mesh->texcoord_count > 0)
    {
        vertexMask |= FLY_VERTEX_TEXCOORD_BIT;
    }

    // Check triangulation
    for (u32 i = 0; i < mesh->face_count; i++)
    {
        // TODO: triangulation
        if (mesh->face_vertices[i] != 3)
        {
            FLY_ASSERT(false);
            return false;
        }
    }

    geometryCount = mesh->object_count;
    *ppGeometries =
        static_cast<Geometry*>(Alloc(sizeof(Geometry) * geometryCount));
    Geometry* pGeometries = *ppGeometries;

    for (u32 i = 0; i < mesh->object_count; i++)
    {
        ExtractGeometryDataFromObj(*mesh, mesh->objects[i], pGeometries[i],
                                   vertexMask);
        printf("Geometry %u has %u vertices and %u subgeometries\n", i,
               pGeometries[i].vertexCount, pGeometries[i].subgeometryCount);
        for (u32 j = 0; j < pGeometries[i].subgeometryCount; j++)
        {
            printf("Subgeometry %u has %u indices\n", j,
                   pGeometries[i].subgeometries[j].lods[0].indexCount);
        }
    }

    return true;
}

bool ImportGeometries(String8 path, Geometry** geometries, u32& geometryCount)
{
    String8 extension = String8::FindLast(path, '.');
    if (extension.StartsWith(FLY_STRING8_LITERAL(".obj")) ||
        extension.StartsWith(FLY_STRING8_LITERAL(".OBJ")))
    {
        return ImportGeometriesObj(path, geometries, geometryCount);
    }
    return false;
}

void FlipGeometryWindingOrder(Geometry& geometry)
{
    for (u32 i = 0; i < geometry.vertexCount / 3; i++)
    {
        Vertex& v0 = geometry.vertices[3 * i];
        Vertex& v1 = geometry.vertices[3 * i + 1];
        Vertex tmp = v0;
        v0 = v1;
        v1 = tmp;
    }
}

static void VertexDeduplication(Geometry& geometry)
{
    u32* remap = static_cast<u32*>(Alloc(sizeof(u32) * geometry.indexCount));

    size_t newVertexCount = meshopt_generateVertexRemap<u32>(
        remap, geometry.indices, geometry.indexCount, geometry.vertices,
        geometry.vertexCount, sizeof(Vertex));

    {
        Vertex* newVertices =
            static_cast<Vertex*>(Alloc(sizeof(Vertex) * newVertexCount));
        meshopt_remapVertexBuffer(newVertices, geometry.vertices,
                                  geometry.vertexCount, sizeof(Vertex), remap);
        Fly::Free(geometry.vertices);
        geometry.vertices = newVertices;
        geometry.vertexCount = newVertexCount;
    }

    {
        u32* newIndices =
            static_cast<u32*>(Alloc(sizeof(u32) * geometry.indexCount));
        meshopt_remapIndexBuffer<u32>(newIndices, geometry.indices,
                                      geometry.indexCount, remap);
        Fly::Free(geometry.indices);
        geometry.indices = newIndices;
    }

    Fly::Free(remap);

    printf("After vertex deduplication geometry has %u vertex count and %u "
           "subgeometries\n",
           geometry.vertexCount, geometry.subgeometryCount);
}

static void OptimizeGeometryVertexCache(Geometry& geometry)
{
    for (u32 i = 0; i < geometry.subgeometryCount; i++)
    {
        u32* newIndices = static_cast<u32*>(
            Alloc(sizeof(u32) * geometry.subgeometries[i].lods[0].indexCount));

        meshopt_optimizeVertexCache<u32>(
            newIndices,
            geometry.indices + geometry.subgeometries[i].lods[0].indexOffset,
            geometry.subgeometries[i].lods[0].indexCount, geometry.vertexCount);

        memcpy(geometry.indices + geometry.subgeometries[i].lods[0].indexOffset,
               newIndices,
               sizeof(u32) * geometry.subgeometries[i].lods[0].indexCount);
        Fly::Free(newIndices);
        printf("Vertex cache optimized for %u subgeometry\n", i);
    }
}

void OptimizeGeometryOverdraw(Geometry& geometry, f32 threshold)
{
    for (u32 i = 0; i < geometry.subgeometryCount; i++)
    {
        u32* newIndices =
            static_cast<u32*>(Alloc(sizeof(u32) * geometry.indexCount));
        meshopt_optimizeOverdraw<u32>(
            newIndices,
            geometry.indices + geometry.subgeometries[i].lods[0].indexOffset,
            geometry.subgeometries[i].lods[0].indexCount,
            reinterpret_cast<const f32*>(geometry.vertices),
            geometry.vertexCount, sizeof(Vertex), threshold);
        memcpy(geometry.indices + geometry.subgeometries[i].lods[0].indexOffset,
               newIndices,
               sizeof(u32) * geometry.subgeometries[i].lods[0].indexCount);
        Fly::Free(newIndices);
        printf("Vertex overdraw optimized for %u subgeometry\n", i);
    }
}

void DestroyGeometry(Geometry& geometry)
{
    if (geometry.vertices)
    {
        Fly::Free(geometry.vertices);
    }
    if (geometry.indices)
    {
        Fly::Free(geometry.indices);
    }
    if (geometry.subgeometries)
    {
        Fly::Free(geometry.subgeometries);
    }
    geometry = {};
}

static u32 HeuristicDetermineLODCount(Geometry& geometry)
{
    return Math::Clamp(static_cast<u32>(Math::Ceil(
                           Math::Log2((geometry.indexCount / 3) / 4000.0f))),
                       0u, FLY_MAX_LOD_COUNT - 1) +
           1;
}

void GenerateGeometryLODs(Geometry& geometry)
{
    u32 lodCount = HeuristicDetermineLODCount(geometry);
    geometry.lodCount = lodCount;
    printf("Geometry will have %u lods\n", geometry.lodCount);
    if (lodCount == 1)
    {
        return;
    }

    u32 totalIndexCount = geometry.indexCount;

    const f32 baseTargetError = 0.0001f;
    for (u32 i = 1; i < geometry.lodCount; i++)
    {
        geometry.indices = static_cast<u32*>(
            Realloc(geometry.indices,
                    sizeof(u32) * (totalIndexCount + geometry.indexCount)));
        for (u32 j = 0; j < geometry.subgeometryCount; j++)
        {
            Subgeometry& sg = geometry.subgeometries[j];
            const u32 minSgIndexCount =
                Math::Min(sg.lods[i - 1].indexCount, 256u * 3u);

            if (sg.lods[i - 1].indexCount == minSgIndexCount)
            {
                memcpy(geometry.indices + totalIndexCount,
                       geometry.indices + sg.lods[i - 1].indexOffset,
                       sizeof(u32) * sg.lods[i - 1].indexCount);
                sg.lods[i] = {totalIndexCount, sg.lods[i - 1].indexCount};
            }
            else
            {
                u32 targetIndexCount = Math::Max(
                    static_cast<u32>(Math::Ceil((sg.lods[0].indexCount / 3) *
                                                Math::Pow(0.5f, i))) *
                        3,
                    minSgIndexCount);
                f32 targetError = baseTargetError * Math::Pow(2.0f, i);

                u32 lodIndexCount = 0;
                for (u32 k = 0; k < 5; k++)
                {
                    f32 resultError = 0.0f;
                    lodIndexCount = meshopt_simplify<u32>(
                        geometry.indices + totalIndexCount,
                        geometry.indices + sg.lods[0].indexOffset,
                        sg.lods[0].indexCount,
                        reinterpret_cast<const float*>(geometry.vertices),
                        geometry.vertexCount, sizeof(Vertex), targetIndexCount,
                        targetError, 0, &resultError);
                    if (lodIndexCount <= targetIndexCount)
                    {
                        break;
                    }
                    targetError *= 1.5f;
                    if (k == 4)
                    {
                        printf(
                            "Warning: simplifier failed to reach target index "
                            "count, subgeometry index %u\n",
                            i);
                    }
                }
                sg.lods[i] = {totalIndexCount, lodIndexCount};
            }
            totalIndexCount += sg.lods[i].indexCount;
        }
    }
    geometry.indices = static_cast<u32*>(
        Realloc(geometry.indices, sizeof(u32) * totalIndexCount));
    geometry.indexCount = totalIndexCount;
}

void QuantizeGeometry(Geometry& geometry)
{
    QVertex* qvertices =
        static_cast<QVertex*>(Alloc(sizeof(QVertex) * geometry.vertexCount));
    for (u32 i = 0; i < geometry.vertexCount; i++)
    {
        const Vertex& vertex = geometry.vertices[i];
        QVertex& qvertex = qvertices[i];
        qvertex = {};
        qvertex.positionX = vertex.position.x;
        qvertex.positionY = vertex.position.y;
        qvertex.positionZ = vertex.position.z;
        qvertex.normal =
            (meshopt_quantizeUnorm((vertex.normal.x + 1.0f) / 2.0f, 10) << 20) |
            (meshopt_quantizeUnorm((vertex.normal.y + 1.0f) / 2.0f, 10) << 10) |
            meshopt_quantizeUnorm((vertex.normal.z + 1.0f) / 2.0f, 10);
        qvertex.tangent =
            (meshopt_quantizeUnorm((vertex.tangent.x + 1.0f) / 2.0f, 10)
             << 20) |
            (meshopt_quantizeUnorm((vertex.tangent.y + 1.0f) / 2.0f, 10)
             << 10) |
            meshopt_quantizeUnorm((vertex.tangent.z + 1.0f) / 2.0f, 10);
        qvertex.u = vertex.u;
        qvertex.v = vertex.v;
    }
    Free(geometry.vertices);
    geometry.qvertices = qvertices;
}

static void CalculateBoundingSphere(Geometry& geometry)
{
    Math::Vec3 min = Math::Vec3(MaxF32());
    Math::Vec3 max = Math::Vec3(MinF32());

    for (u32 i = 0; i < geometry.vertexCount; i++)
    {
        const Math::Vec3& pos = geometry.vertices[i].position;

        if (pos.x < min.x)
        {
            min.x = pos.x;
        }
        if (pos.y < min.y)
        {
            min.y = pos.y;
        }
        if (pos.z < min.z)
        {
            min.z = pos.z;
        }

        if (pos.x > max.x)
        {
            max.x = pos.x;
        }
        if (pos.y > max.y)
        {
            max.y = pos.y;
        }
        if (pos.z > max.z)
        {
            max.z = pos.z;
        }
    }

    geometry.sphereCenter = 0.5f * (max + min);
    geometry.sphereRadius = Math::Length(max - min) * 0.5f;

    printf("Geometry's bounding sphere: (%f, %f %f), radius - %f\n",
           geometry.sphereCenter.x, geometry.sphereCenter.y,
           geometry.sphereCenter.z, geometry.sphereRadius);
}

void CookGeometry(Geometry& geometry)
{
    VertexDeduplication(geometry);
    OptimizeGeometryVertexCache(geometry);
    OptimizeGeometryOverdraw(geometry, 1.05f);
    CalculateBoundingSphere(geometry);
    GenerateGeometryLODs(geometry);
    QuantizeGeometry(geometry);
}

bool ExportGeometries(String8 path, const Geometry* geometries,
                      u32 geometryCount)
{
    if (!geometries || geometryCount == 0)
    {
        return false;
    }

    u32 totalIndexCount = 0;
    u32 totalVertexCount = 0;
    u32 totalLodCount = 0;
    for (u32 i = 0; i < geometryCount; i++)
    {
        totalVertexCount += geometries[i].vertexCount;
        totalIndexCount += geometries[i].indexCount;
        totalLodCount +=
            geometries[i].lodCount * geometries[i].subgeometryCount;
    }

    u64 totalSize =
        sizeof(MeshFileHeader) + sizeof(MeshHeader) * geometryCount +
        sizeof(LOD) * totalLodCount + sizeof(QVertex) * totalVertexCount +
        sizeof(u32) * totalIndexCount;

    u64 offset = 0;
    u8* data = static_cast<u8*>(Alloc(totalSize));

    MeshFileHeader* fileHeader = reinterpret_cast<MeshFileHeader*>(data);
    offset += sizeof(MeshFileHeader);

    MeshHeader* meshHeaderStart = reinterpret_cast<MeshHeader*>(data + offset);
    offset += sizeof(MeshHeader) * geometryCount;

    LOD* lodStart = reinterpret_cast<LOD*>(data + offset);
    offset += sizeof(LOD) * totalLodCount;

    QVertex* vertexStart = reinterpret_cast<QVertex*>(data + offset);
    offset += sizeof(QVertex) * totalVertexCount;

    u32* indexStart = reinterpret_cast<u32*>(data + offset);

    fileHeader->version = {1, 0, 0};
    fileHeader->meshCount = geometryCount;
    fileHeader->totalLodCount = totalLodCount;
    fileHeader->totalVertexCount = totalVertexCount;
    fileHeader->totalIndexCount = totalIndexCount;

    u64 vertexOffset = 0;
    u64 indexOffset = 0;
    u32 lodsOffset = 0;
    for (u32 i = 0; i < geometryCount; i++)
    {
        const Geometry& geometry = geometries[i];

        MeshHeader& meshHeader = *(meshHeaderStart + i);
        meshHeader.sphereCenter = geometry.sphereCenter;
        meshHeader.submeshCount = geometry.subgeometryCount;
        meshHeader.vertexCount = geometry.vertexCount;
        meshHeader.indexCount = geometry.indexCount;
        meshHeader.lodCount = geometry.lodCount;
        meshHeader.sphereRadius = geometry.sphereRadius;
        meshHeader.lodsOffset = lodsOffset;
        meshHeader.vertexOffset = vertexOffset;
        meshHeader.indexOffset = indexOffset;

        LOD* lodData = lodStart + lodsOffset;
        QVertex* vertexData = vertexStart + vertexOffset;
        u32* indexData = indexStart + indexOffset;

        for (u32 j = 0; j < geometry.subgeometryCount; j++)
        {
            const Subgeometry& sg = geometry.subgeometries[j];
            memcpy(lodData + j * geometry.lodCount, sg.lods,
                   sizeof(LOD) * geometry.lodCount);
            for (u32 k = 0; k < geometry.lodCount; k++)
            {
                LOD* lod = lodData + j * geometry.lodCount + k;
                lod->indexOffset += indexOffset;
            }
        }

        memcpy(vertexData, geometry.qvertices,
               sizeof(QVertex) * geometry.vertexCount);
        memcpy(indexData, geometry.indices, sizeof(u32) * geometry.indexCount);

        lodsOffset += geometry.subgeometryCount * geometry.lodCount;
        vertexOffset += geometry.vertexCount;
        indexOffset += geometry.indexCount;
    }

    String8 str(reinterpret_cast<char*>(data), totalSize);
    if (!WriteStringToFile(str, path))
    {
        Free(data);
        return false;
    }

    Free(data);
    return true;
}

} // namespace Fly
