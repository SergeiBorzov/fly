#ifndef FLY_GEOMETRY_H
#define FLY_GEOMETRY_H

#include "core/string8.h"
#include "math/vec.h"

#define FLY_MAX_LOD_COUNT 8

namespace Fly
{

enum VertexFlags
{
    FLY_VERTEX_NONE_BIT = 0,
    FLY_VERTEX_POSITION_BIT = 1 << 0,
    FLY_VERTEX_NORMAL_BIT = 1 << 1,
    FLY_VERTEX_TANGENT_BIT = 1 << 2,
    FLY_VERTEX_TEXCOORD_BIT = 1 << 3
};

enum class CoordSystem : u8
{
    XYZ = 0,
    XZY = 1,
    YXZ = 2,
    YZX = 3,
    ZXY = 4,
    ZYX = 5
};

struct GeometryLOD
{
    u32 firstIndex;
    u32 indexCount;
};

struct Geometry
{
    GeometryLOD lods[FLY_MAX_LOD_COUNT];
    u8* vertices = nullptr;
    unsigned int* indices = nullptr;
    u32 vertexCount = 0;
    u32 indexCount = 0;
    u16 vertexSize = 0;
    u8 lodCount = 0;
    u8 vertexMask = FLY_VERTEX_NONE_BIT;
};

bool ImportGeometry(String8 path, Geometry& geometry);
bool ExportGeometry(String8 path, Geometry& geometry);
void DestroyGeometry(Geometry& geometry);
void TransformGeometry(f32 scale, CoordSystem coordSystem, bool flipForward,
                       Geometry& geometry);
void FlipGeometryWindingOrder(Geometry& geometry);
void ReindexGeometry(Geometry& geometry);
void OptimizeGeometryVertexCache(Geometry& geometry);
void OptimizeGeometryOverdraw(Geometry& geometry, f32 threshold);
void OptimizeGeometryVertexFetch(Geometry& geometry);
void QuantizeGeometry(Geometry& geometry);
void GenerateGeometryLODs(Geometry& geometry);
void CookGeometry(Geometry& geometry);

void TriangulateGeometry(Geometry& geometry);
void GenerateGeometryNormals(Geometry& geometry);
void GenerateGeometryTangents(Geometry& geometry);

} // namespace Fly

#endif /* FLY_GEOMETRY_H */
