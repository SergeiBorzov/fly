#ifndef FLY_ASSETS_MESH_H
#define FLY_ASSETS_MESH_H

#include "core/string8.h"
#include "math/vec.h"

#include "rhi/buffer.h"
#include "vertex_layout.h"

#define FLY_MAX_LOD_COUNT 8

namespace Fly
{

struct Submesh
{
    LOD lods[FLY_MAX_LOD_COUNT];
};

struct Mesh
{
    RHI::Buffer vertexBuffer;
    RHI::Buffer indexBuffer;
    Math::Vec3 sphereCenter;
    Submesh* submeshes;
    u32 submeshCount;
    u32 vertexCount;
    u32 indexCount;
    f32 sphereRadius;
    u8 lodCount;
};

bool ImportMeshes(String8 path, RHI::Device& device, Mesh** meshes,
                  u32& meshCount);
void DestroyMesh(RHI::Device& device, Mesh& mesh);

} // namespace Fly

#endif /* FLY_ASSETS_MESH_H */
