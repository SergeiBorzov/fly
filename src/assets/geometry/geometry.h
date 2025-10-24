#ifndef FLY_GEOMETRY_H
#define FLY_GEOMETRY_H

#include "core/string8.h"
#include "math/vec.h"

namespace Fly
{

enum VertexFlags
{
    FLY_VERTEX_NONE_BIT = 0,
    FLY_VERTEX_POSITION_BIT = 1 << 1,
    FLY_VERTEX_NORMAL_BIT = 1 << 2,
    FLY_VERTEX_TANGENT_BIT = 1 << 3,
    FLY_VERTEX_TEXCOORD_BIT = 1 << 4
};

struct Geometry
{
    u8* vertices = nullptr;
    u32* indices = nullptr;
    u32 vertexSize = 0;
    u32 vertexCount = 0;
    u32 indexCount = 0;
    u8 vertexMask = FLY_VERTEX_NONE_BIT;
};

bool ImportGeometry(String8 path, Geometry& geometry);
bool ExportGeometry(String8 path, Geometry& geometry);
void DestroyGeometry(Geometry& geometry);

void TriangulateGeometry(Geometry& geometry);
void SetGeometryWindingOrder(Geometry& geometry, bool ccw = 1);
void GenerateGeometryIndices(Geometry& geometry);
void GenerateGeometryNormals(Geometry& geometry);
void GenerateGeometryTangents(Geometry& geometry);
void ApplyTransformToGeometry(Geometry& geometry);
void OptimizeGeometryVertexCache(Geometry& geometry);
void OptimizeGeometryOverdraw(Geometry& geometry);
void OptimizeGeometryVertexFetch(Geometry& geometry);
void QuantizeGeometry(Geometry& geometry);

} // namespace Fly

#endif /* FLY_GEOMETRY_H */
