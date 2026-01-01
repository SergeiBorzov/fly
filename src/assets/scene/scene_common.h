#ifndef FLY_ASSETS_SCENE_SCENE_COMMON_H
#define FLY_ASSETS_SCENE_SCENE_COMMON_H

#define FLY_MAX_LOD_COUNT 8u

#include "math/quat.h"
#include "math/vec.h"

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
    Math::Vec4 tangent;
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
    u32 firstIndex = 0;
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
    u64 firstLod;
    u32 submeshCount;
    u32 vertexCount;
    u32 firstVertex;
    u32 indexCount;
    u32 firstIndex;
    f32 sphereRadius;
    u8 lodCount;
};

struct SerializedSceneNode
{
    Math::Quat localRotation = Math::Quat();
    Math::Vec3 localPosition = Math::Vec3(0.0f);
    Math::Vec3 localScale = Math::Vec3(1.0f);
    i64 meshIndex = -1;
    i64 parentIndex = -1;
};

struct SerializedPBRMaterial
{
    Math::Vec4 baseColor = Math::Vec4(1.0f);
    i32 baseColorTextureIndex = -1;
    i32 normalTextureIndex = -1;
    f32 roughness = 0.5f;
    f32 metallic = 0.0f;
    // i32 ormTextureIndex = -1;
};

struct SceneFileHeader
{
    struct
    {
        u32 major;
        u32 minor;
        u32 patch;
    } version;
    u64 totalVertexCount = 0;
    u64 totalIndexCount = 0;
    u32 totalSubmeshCount = 0;
    u32 totalLodCount = 0;
    u32 textureCount = 0;
    u32 meshCount = 0;
    u32 materialCount = 0;
    u32 nodeCount = 0;
};

} // namespace Fly

#endif /* FLY_ASSETS_SCENE_SCENE_COMMON_H */
