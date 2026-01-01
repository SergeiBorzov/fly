#ifndef FLY_ASSETS_SCENE_SCENE_H
#define FLY_ASSETS_SCENE_SCENE_H

#include "core/types.h"
#include "core/string8.h"

#include "math/transform.h"

#include "rhi/buffer.h"
#include "rhi/texture.h"

#include "assets/scene/scene_common.h"

namespace Fly
{

struct Submesh
{
    LOD lods[FLY_MAX_LOD_COUNT];
    i32 materialIndex = -1;
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

struct SceneNode
{
    Math::Transform transform{};
    Mesh* mesh = nullptr;
};

struct PBRMaterial
{
    Math::Vec4 baseColor = Math::Vec4(1.0f);
    u32 baseColorTextureBindlessHandle = FLY_MAX_U32;
    u32 normalTextureBindlessHandle = FLY_MAX_U32;
    u32 ormTextureBindlessHandle = FLY_MAX_U32;
    u32 pad = 0;
};

struct Scene
{
    RHI::Buffer vertexBuffer{};
    RHI::Buffer indexBuffer{};
    RHI::Buffer pbrMaterialBuffer{};
    RHI::Texture whiteTexture{};
    RHI::Texture flatNormalTexture{};
    RHI::Texture* textures = nullptr;
    Mesh* meshes = nullptr;
    SceneNode* nodes = nullptr;
    u32 nodeCount = 0;
    u32 textureCount = 0;
    u32 meshCount = 0;
    u32 materialCount = 0;
};

bool ImportScene(String8 path, RHI::Device& device, Scene& scene);
void DestroyScene(RHI::Device& device, Scene& scene);

} // namespace Fly

#endif /* FLY_ASSETS_SCENE_SCENE_H */
