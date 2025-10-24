#ifndef FLY_MESH_H
#define FLY_MESH_H

#include "core/string8.h"
#include "rhi/buffer.h"

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

struct Mesh
{
    u64 vertexCount;
    u64 indexCount;
    u8 vertexSize;
    u8 indexSize;
    u8 vertexMask;
    RHI::Buffer vertexBuffer;
    RHI::Buffer indexBuffer;
};

bool ImportMesh(String8 path, RHI::Device& device, Mesh& mesh);
void DestroyMesh(RHI::Device& device, Mesh& mesh);

} // namespace Fly

#endif /* FLY_MESH_H */
