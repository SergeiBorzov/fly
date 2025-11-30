#ifndef FLY_ASSETS_VERTEX_LAYOUT_H

#define FLY_MAX_LOD_COUNT 8u

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

struct Vertex
{
    Math::Vec3 position;
    f32 u;
    Math::Vec3 normal;
    f32 v;
    Math::Vec3 tangent;
    f32 pad;
};

struct QVertex
{
    f16 positionX;
    f16 positionY;
    f16 positionZ;
    f16 u;
    f16 v;
    u32 normal;
    u32 tangent;
};

struct LOD
{
    u32 indexOffset = 0;
    u32 indexCount = 0;
};

struct MeshFileHeader
{
    struct
    {
        u32 major;
        u32 minor;
        u32 patch;
    } version;
    u32 meshCount;
    u32 totalLodCount;
    u32 totalVertexCount;
    u32 totalIndexCount;
};

struct MeshHeader
{
    Math::Vec3 sphereCenter;
    u64 lodsOffset;
    u32 submeshCount;
    u32 vertexCount;
    u32 vertexOffset;
    u32 indexCount;
    u32 indexOffset;
    f32 sphereRadius;
    u8 lodCount;
};

} // namespace Fly

#endif /* FLY_ASSETS_VERTEX_LAYOUT_H */
