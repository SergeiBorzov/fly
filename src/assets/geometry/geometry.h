#ifndef FLY_ASSETS_GEOMETRY_H
#define FLY_ASSETS_GEOMETRY_H

#include "core/string8.h"
#include "math/vec.h"

#include "vertex_layout.h"

struct cgltf_data;

namespace Fly
{

enum class CoordSystem : u8
{
    XYZ = 0,
    XZY = 1,
    YXZ = 2,
    YZX = 3,
    ZXY = 4,
    ZYX = 5
};

struct Subgeometry
{
    LOD lods[FLY_MAX_LOD_COUNT];
    i32 materialIndex = -1;
};

struct Geometry
{
    Math::Vec3 sphereCenter;
    Subgeometry* subgeometries = nullptr;
    union
    {
        Vertex* vertices = nullptr;
        QVertex* qvertices;
    };
    u32* indices = nullptr;
    
    f32 sphereRadius = 0.0f;
    u32 indexCount = 0;
    u32 vertexCount = 0;
    u32 subgeometryCount = 0;
    u8 vertexMask = FLY_VERTEX_NONE_BIT;
    u8 lodCount = 0;
};

bool ImportGeometiresObj(const void* mesh, Geometry** ppGeometries,
                         u32& geometryCount);
bool ImportGeometriesGltf(const cgltf_data* data, Geometry** ppGeometries,
                          u32& geometryCount);
bool ImportGeometries(String8 path, Geometry** geometries, u32& geometryCount);
bool ExportGeometries(String8 path, const Geometry* geometries,
                      u32 geometryCount);
void TransformGeometry(f32 scale, CoordSystem coordSystem, bool flipForward,
                       Geometry& geometry);
void FlipGeometryWindingOrder(Geometry& geometry);
void CookGeometry(Geometry& geometry);
void DestroyGeometry(Geometry& geometry);

} // namespace Fly

#endif /* FLY_ASSETS_GEOMETRY_H */
