#ifndef FLY_MESH_H
#define FLY_MESH_H

#include "core/string8.h"
#include "math/vec.h"

#include "rhi/buffer.h"

#define FLY_MAX_LOD_COUNT 8

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

struct GeometryLOD
{
    u32 firstIndex;
    u32 indexCount;
};

struct Mesh
{
    GeometryLOD lods[FLY_MAX_LOD_COUNT];
    RHI::Buffer vertexBuffer;
    RHI::Buffer indexBuffer;
    Math::Vec3 sphereCenter;
    u64 vertexCount;
    u64 indexCount;
    f32 sphereRadius;
    u8 vertexSize;
    u8 indexSize;
    u8 vertexMask;
    u8 lodCount;
};

bool ImportMesh(String8 path, RHI::Device& device, Mesh& mesh);
void DestroyMesh(RHI::Device& device, Mesh& mesh);

} // namespace Fly

#endif /* FLY_MESH_H */
