#ifndef FLY_ASSETS_MESH_H
#define FLY_ASSETS_MESH_H

#include "core/string8.h"
#include "math/vec.h"

#include "vertex_layout.h"

namespace Fly
{

namespace RHI
{
struct Buffer;
}

struct Submesh
{
    LOD lods[FLY_MAX_LOD_COUNT];
};

struct Mesh
{
    Math::Vec3 sphereCenter;
    Submesh* submeshes;
    u32 submeshCount;
    u32 vertexCount;
    u32 indexCount;
    f32 sphereRadius;
    i32 vertexOffset;
    u8 lodCount;
};

bool ImportMeshes(String8 path, RHI::Device& device, Mesh** meshes,
                  u32& meshCount, RHI::Buffer& vertexBuffer,
                  RHI::Buffer& indexBuffer);
void DestroyMesh(RHI::Device& device, Mesh& mesh);

} // namespace Fly

#endif /* FLY_ASSETS_MESH_H */
