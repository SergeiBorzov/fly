#include <stdio.h>

#include "core/filesystem.h"
#include "core/memory.h"
#include "core/thread_context.h"

#define FAST_OBJ_REALLOC Fly::Realloc
#define FAST_OBJ_FREE Fly::Free
#define FAST_OBJ_IMPLEMENTATION
#include <fast_obj.h>

#define CGLTF_MALLOC(size) (Fly::Alloc(size))
#define CGLTF_FREE(size) (Fly::Free(size))
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include "geometry.h"

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

static void TransformVertex(Vertex& vertex, f32 scale, CoordSystem coordSystem,
                            bool flipForward)
{
    FLY_ASSERT(userData);

    switch (coordSystem)
    {
        case CoordSystem::XZY:
        {
            vertex.position = Math::Vec3(vertex.position.x, vertex.position.z,
                                         vertex.position.y);
            vertex.normal =
                Math::Vec3(vertex.normal.x, vertex.normal.z, vertex.normal.y);
            vertex.tangent = Math::Vec4(vertex.tangent.x, vertex.tangent.z,
                                        vertex.tangent.y, vertex.tangent.w);
            break;
        }
        case CoordSystem::YXZ:
        {
            vertex.position = Math::Vec3(vertex.position.y, vertex.position.x,
                                         vertex.position.z);
            vertex.normal =
                Math::Vec3(vertex.normal.y, vertex.normal.x, vertex.normal.z);
            vertex.tangent = Math::Vec4(vertex.tangent.y, vertex.tangent.x,
                                        vertex.tangent.z, vertex.tangent.w);
            break;
        }
        case CoordSystem::YZX:
        {
            vertex.position = Math::Vec3(vertex.position.y, vertex.position.z,
                                         vertex.position.x);
            vertex.normal =
                Math::Vec3(vertex.normal.y, vertex.normal.z, vertex.normal.x);
            vertex.tangent = Math::Vec4(vertex.tangent.y, vertex.tangent.z,
                                        vertex.tangent.x, vertex.tangent.w);
            break;
        }
        case CoordSystem::ZXY:
        {
            vertex.position = Math::Vec3(vertex.position.z, vertex.position.x,
                                         vertex.position.y);
            vertex.normal =
                Math::Vec3(vertex.normal.z, vertex.normal.x, vertex.normal.y);
            vertex.tangent = Math::Vec4(vertex.tangent.z, vertex.tangent.x,
                                        vertex.tangent.y, vertex.tangent.w);
            break;
        }
        case CoordSystem::ZYX:
        {
            vertex.position = Math::Vec3(vertex.position.z, vertex.position.y,
                                         vertex.position.x);
            vertex.normal =
                Math::Vec3(vertex.normal.z, vertex.normal.y, vertex.normal.x);
            vertex.tangent = Math::Vec4(vertex.tangent.z, vertex.tangent.y,
                                        vertex.tangent.x, vertex.tangent.w);
            break;
        }
        default:
        {
            break;
        }
    }

    vertex.position *= scale;
    if (flipForward)
    {
        vertex.position.z *= -1.0f;
        vertex.normal.z *= -1.0f;
        vertex.tangent.z *= -1.0f;
        vertex.tangent.w *= -1.0f;
    }
}

void TransformGeometry(f32 scale, CoordSystem coordSystem, bool flipForward,
                       Geometry& geometry)
{
    for (u32 i = 0; i < geometry.vertexCount; i++)
    {
        TransformVertex(geometry.vertices[i], scale, coordSystem, flipForward);
    }
}

static void CalculateTangents(Geometry& geometry)
{
    Math::Vec3* tangents = static_cast<Math::Vec3*>(
        Alloc(sizeof(Math::Vec3) * geometry.vertexCount * 2));
    Math::Vec3* bitangents = tangents + geometry.vertexCount;

    for (u32 i = 0; i < geometry.indexCount / 3; i++)
    {
        u32 i0 = geometry.indices[3 * i];
        u32 i1 = geometry.indices[3 * i + 1];
        u32 i2 = geometry.indices[3 * i + 2];

        Vertex& v0 = geometry.vertices[i0];
        Vertex& v1 = geometry.vertices[i1];
        Vertex& v2 = geometry.vertices[i2];

        float x1 = v1.u - v0.u;
        float x2 = v2.u - v0.u;
        float y1 = v1.v - v0.v;
        float y2 = v2.v - v0.v;

        Math::Vec3 e1 = v1.position - v0.position;
        Math::Vec3 e2 = v2.position - v0.position;

        float invDet = 1.0f / (x1 * y2 - x2 * y1);
        Math::Vec3 t = (e1 * y2 - e2 * y1) * invDet;
        Math::Vec3 b = (e2 * x1 - e1 * x2) * invDet;

        tangents[i0] += t;
        tangents[i1] += t;
        tangents[i2] += t;
        bitangents[i0] += b;
        bitangents[i1] += b;
        bitangents[i2] += b;
    }

    for (u32 i = 0; i < geometry.vertexCount; i++)
    {
        const Math::Vec3& t = tangents[i];
        const Math::Vec3& b = bitangents[i];
        const Math::Vec3& n = geometry.vertices[i].normal;

        f32 w = Math::Dot(Math::Cross(t, b), n) > 0.0f ? 1.0f : -1.0f;
        geometry.vertices[i].tangent =
            Math::Vec4(Math::Normalize(t - Math::Dot(t, n) * n), w);
    }
    Free(tangents);
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
            subgeometry.lods[0].firstIndex = 3 * i;
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

    if ((geometry.vertexMask & FLY_VERTEX_TEXCOORD_BIT) &&
        (geometry.vertexMask & FLY_VERTEX_NORMAL_BIT))
    {
        CalculateTangents(geometry);
        geometry.vertexMask |= FLY_VERTEX_TANGENT_BIT;
    }
}

static bool ImportGeometriesObj(const void* pMesh, Geometry** ppGeometries,
                                u32& geometryCount)
{
    FLY_ASSERT(ppGeometries);
    geometryCount = 0;

    const fastObjMesh* mesh = static_cast<const fastObjMesh*>(pMesh);

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
    }

    return true;
}

bool ImportGeometriesGltf(const cgltf_data* data, Geometry** ppGeometries,
                          u32& geometryCount)
{
    FLY_ASSERT(data);
    FLY_ASSERT(ppGeometries);

    if (data->meshes_count == 0)
    {
        return true;
    }

    Arena& arena = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(arena);

    u32* vertexCountArray = FLY_PUSH_ARENA(arena, u32, data->meshes_count);
    MemZero(vertexCountArray, sizeof(u32) * data->meshes_count);
    u32* indexCountArray = FLY_PUSH_ARENA(arena, u32, data->meshes_count);
    MemZero(indexCountArray, sizeof(u32) * data->meshes_count);

    for (u32 i = 0; i < data->meshes_count; i++)
    {
        const cgltf_mesh& mesh = data->meshes[i];

        for (u32 j = 0; j < mesh.primitives_count; j++)
        {
            const cgltf_primitive& primitive = mesh.primitives[j];

            bool isPosPresent = false;
            bool isNormalPresent = false;

            for (u32 k = 0; k < primitive.attributes_count; k++)
            {
                const cgltf_attribute& attribute = primitive.attributes[k];
                const cgltf_accessor* accessor = attribute.data;
                if (attribute.type == cgltf_attribute_type_position)
                {
                    if (accessor->component_type !=
                            cgltf_component_type_r_32f &&
                        accessor->type != cgltf_type_vec3)
                    {
                        fprintf(
                            stderr,
                            "Primitive position has non float vec3 format!\n");
                        ArenaPopToMarker(arena, marker);
                        return false;
                    }
                    isPosPresent = true;
                }
                else if (attribute.type == cgltf_attribute_type_normal)
                {
                    if (accessor->component_type !=
                            cgltf_component_type_r_32f &&
                        accessor->type != cgltf_type_vec3)
                    {
                        fprintf(
                            stderr,
                            "Primitive normal has non float vec3 format!\n");
                        ArenaPopToMarker(arena, marker);
                        return false;
                    }
                    isNormalPresent = true;
                }
                else if (attribute.type == cgltf_attribute_type_texcoord)
                {
                    if (accessor->component_type !=
                            cgltf_component_type_r_32f &&
                        accessor->type != cgltf_type_vec2)
                    {
                        fprintf(
                            stderr,
                            "Primitive tex coord has non float vec2 format!\n");
                        ArenaPopToMarker(arena, marker);
                        return false;
                    }
                }
            }

            if (!isPosPresent || !isNormalPresent)
            {
                fprintf(stderr,
                        "Primitive vertex without position + normal!\n");
                ArenaPopToMarker(arena, marker);
                return false;
            }
            vertexCountArray[i] += primitive.attributes[0].data->count;

            if (!primitive.indices)
            {
                fprintf(stderr, "Primitive without index buffer!\n");
                ArenaPopToMarker(arena, marker);
                return false;
            }
            indexCountArray[i] += primitive.indices->count;
        }
    }

    geometryCount = data->meshes_count;
    *ppGeometries =
        static_cast<Geometry*>(Alloc(sizeof(Geometry) * data->meshes_count));

    for (u32 i = 0; i < data->meshes_count; i++)
    {
        Geometry& geometry = (*ppGeometries)[i];
        const cgltf_mesh& mesh = data->meshes[i];

        geometry.subgeometryCount = mesh.primitives_count;
        geometry.subgeometries = static_cast<Subgeometry*>(
            Alloc(sizeof(Subgeometry) * mesh.primitives_count));
        MemZero(geometry.subgeometries,
                sizeof(Subgeometry) * mesh.primitives_count);

        geometry.vertexCount = vertexCountArray[i];
        geometry.vertices =
            static_cast<Vertex*>(Alloc(sizeof(Vertex) * vertexCountArray[i]));
        MemZero(geometry.vertices, sizeof(Vertex) * vertexCountArray[i]);

        geometry.indexCount = indexCountArray[i];
        geometry.indices =
            static_cast<u32*>(Alloc(sizeof(u32) * indexCountArray[i]));
        MemZero(geometry.indices, sizeof(u32) * indexCountArray[i]);

        u32 vertexOffset = 0;
        u32 indexOffset = 0;

        for (u32 j = 0; j < mesh.primitives_count; j++)
        {
            const cgltf_primitive& primitive = mesh.primitives[j];

            geometry.subgeometries[j].materialIndex = -1;
            if (primitive.material)
            {
                geometry.subgeometries[j].materialIndex =
                    static_cast<i32>(primitive.material - data->materials);
            }

            // Vertices
            ArenaMarker loopMarker = ArenaGetMarker(arena);
            for (u32 k = 0; k < primitive.attributes_count; k++)
            {
                const cgltf_attribute& attribute = primitive.attributes[k];
                const cgltf_accessor* accessor = attribute.data;

                u32 vertexCount = accessor->count;
                if (attribute.type == cgltf_attribute_type_position)
                {
                    f32* unpacked = FLY_PUSH_ARENA(arena, f32, vertexCount * 3);
                    cgltf_accessor_unpack_floats(accessor, unpacked,
                                                 vertexCount * 3);
                    for (u64 l = 0; l < vertexCount; l++)
                    {
                        memcpy(
                            geometry.vertices[vertexOffset + l].position.data,
                            unpacked + 3 * l, sizeof(f32) * 3);
                    }
                }
                else if (attribute.type == cgltf_attribute_type_normal)
                {
                    f32* unpacked = FLY_PUSH_ARENA(arena, f32, vertexCount * 3);
                    cgltf_accessor_unpack_floats(accessor, unpacked,
                                                 vertexCount * 3);
                    for (u64 l = 0; l < vertexCount; l++)
                    {
                        memcpy(geometry.vertices[vertexOffset + l].normal.data,
                               unpacked + 3 * l, sizeof(f32) * 3);
                    }
                }
                else if (attribute.type == cgltf_attribute_type_texcoord)
                {
                    f32* unpacked = FLY_PUSH_ARENA(arena, f32, vertexCount * 2);
                    cgltf_accessor_unpack_floats(accessor, unpacked,
                                                 vertexCount * 2);
                    for (u64 l = 0; l < vertexCount; l++)
                    {
                        geometry.vertices[vertexOffset + l].u =
                            *(unpacked + 2 * l);
                        geometry.vertices[vertexOffset + l].v =
                            *(unpacked + 2 * l + 1);
                    }
                }
                ArenaPopToMarker(arena, loopMarker);
            }

            // Indices
            u32 indexCount = primitive.indices->count;
            geometry.subgeometries[j].lods[0] = {indexOffset, indexCount};

            u32* indices = FLY_PUSH_ARENA(arena, u32, indexCount);
            cgltf_accessor_unpack_indices(primitive.indices, indices,
                                          sizeof(u32), indexCount);

            for (u32 l = 0; l < indexCount; l++)
            {
                indices[l] += vertexOffset;
            }
            memcpy(geometry.indices + indexOffset, indices,
                   sizeof(u32) * indexCount);
            indexOffset += indexCount;
            vertexOffset += primitive.attributes[0].data->count;
            ArenaPopToMarker(arena, loopMarker);
        }
    }

    ArenaPopToMarker(arena, marker);

    return true;
}

bool ImportGeometries(String8 path, Geometry** geometries, u32& geometryCount)
{
    String8 extension = String8::FindLast(path, '.');
    if (extension.StartsWith(FLY_STRING8_LITERAL(".obj")) ||
        extension.StartsWith(FLY_STRING8_LITERAL(".OBJ")))
    {
        fastObjMesh* mesh = fast_obj_read(path.Data());
        bool res = ImportGeometriesObj(mesh, geometries, geometryCount);
        fast_obj_destroy(mesh);

        return res;
    }
    if (extension.StartsWith(FLY_STRING8_LITERAL(".gltf")) ||
        extension.StartsWith(FLY_STRING8_LITERAL(".GLTF")) ||
        extension.StartsWith(FLY_STRING8_LITERAL(".glb")) ||
        extension.StartsWith(FLY_STRING8_LITERAL(".GLB")))
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
        bool res = ImportGeometriesGltf(data, geometries, geometryCount);
        cgltf_free(data);

        return res;
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
}

static void OptimizeGeometryVertexCache(Geometry& geometry)
{
    for (u32 i = 0; i < geometry.subgeometryCount; i++)
    {
        u32* newIndices = static_cast<u32*>(
            Alloc(sizeof(u32) * geometry.subgeometries[i].lods[0].indexCount));

        meshopt_optimizeVertexCache<u32>(
            newIndices,
            geometry.indices + geometry.subgeometries[i].lods[0].firstIndex,
            geometry.subgeometries[i].lods[0].indexCount, geometry.vertexCount);

        memcpy(geometry.indices + geometry.subgeometries[i].lods[0].firstIndex,
               newIndices,
               sizeof(u32) * geometry.subgeometries[i].lods[0].indexCount);
        Fly::Free(newIndices);
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
            geometry.indices + geometry.subgeometries[i].lods[0].firstIndex,
            geometry.subgeometries[i].lods[0].indexCount,
            reinterpret_cast<const f32*>(geometry.vertices),
            geometry.vertexCount, sizeof(Vertex), threshold);
        memcpy(geometry.indices + geometry.subgeometries[i].lods[0].firstIndex,
               newIndices,
               sizeof(u32) * geometry.subgeometries[i].lods[0].indexCount);
        Fly::Free(newIndices);
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
                       geometry.indices + sg.lods[i - 1].firstIndex,
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
                        geometry.indices + sg.lods[0].firstIndex,
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
                        fprintf(stderr,
                                "Warning: mesh simplifier failed to reach "
                                "target index "
                                "count %u (current index count is %u), "
                                "subgeometry index %u\n",
                                targetIndexCount, lodIndexCount, i);
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
        u32 handness = (vertex.tangent.w > 0.0f) ? 1 : 0;
        qvertex.tangent =
            (handness << 21) |
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

    u64 totalIndexCount = 0;
    u64 totalVertexCount = 0;
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

    u64 firstVertex = 0;
    u64 firstIndex = 0;
    u32 firstLod = 0;
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
